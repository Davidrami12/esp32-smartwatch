package dev.esp32.smartwatch.ble

import android.Manifest
import android.app.NotificationManager
import android.content.ComponentName
import android.content.BroadcastReceiver
import android.content.Intent
import android.content.IntentFilter
import android.annotation.SuppressLint
import android.bluetooth.*
import android.bluetooth.le.*
import android.content.Context
import android.content.pm.PackageManager
import android.os.Handler
import android.os.Looper
import android.os.ParcelUuid
import android.util.Log
import dev.esp32.smartwatch.*
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.asStateFlow
import java.util.UUID
import dev.esp32.smartwatch.notifications.PhoneNotification
import dev.esp32.smartwatch.notifications.NotificationTransport
import dev.esp32.smartwatch.notifications.WatchNotificationListener
import dev.esp32.smartwatch.companion.ReconnectPolicy
import dev.esp32.smartwatch.media.*

/** All mutable BLE/session state is confined to the main looper, including callbacks. */
@SuppressLint("MissingPermission") // Every entry point/callback checks permissions; revocation is caught.
class WatchRepository(context: Context) {
    private val context = context.applicationContext
    private val adapter = this.context.getSystemService(BluetoothManager::class.java)?.adapter
    private val handler = Handler(Looper.getMainLooper())
    private val preferences = this.context.getSharedPreferences("companion", Context.MODE_PRIVATE)
    private val mutableState = MutableStateFlow(WatchState(savedWatchAvailable = preferences.contains("watch_address")))
    val state = mutableState.asStateFlow()
    private val devices = mutableMapOf<String, BluetoothDevice>()
    private var scanner: BluetoothLeScanner? = null
    private var scanCallback: ScanCallback? = null
    private var gatt: BluetoothGatt? = null
    private var lastDevice: BluetoothDevice? = null
    private var serviceEnabled = false
    private var reconnectAllowed = false
    private var retryCount = 0
    private var reconnectTask: Runnable? = null
    private var timeout: Runnable? = null
    private var databaseSubscriptionDone = false
    private var discoveryDelay: Runnable? = null
    private val characteristics = mutableMapOf<UUID, BluetoothGattCharacteristic>()
    private data class Operation(val uuid: UUID, val subscribe: Boolean = false, val writeValue: ByteArray? = null, val readback: Boolean = false,
                                 val notificationChunk: Boolean = false, val mediaChunk: Boolean = false)
    private val operations = ArrayDeque<Operation>()
    private var pending: Operation? = null
    private val phoneNotifications = ArrayDeque<PhoneNotification>()
    private var notificationId = 0
    private var latestMedia = PhoneMedia()
    private var sentMedia: PhoneMedia? = null
    private var mediaInFlight: PhoneMedia? = null
    private var mediaCommandHandler: ((MediaCommand) -> Unit)? = null
    private var mediaSequence: Int? = null
    private var mediaVolumeSupported = false
    private var activitySnapshotHandler: (() -> Unit)? = null
    private var voiceReceiver: VoiceProtocol.Receiver? = null
    private var voiceDeadline = 0L
    private var voiceStarted = 0L
    private var voiceChunks = 0
    private var voiceReadUs = 0L
    private var voiceWriteUs = 0L
    private var operationStartedNs = 0L
    private var resultDeliveryStartedNs = 0L
    private var aiSession = 0
    private var aiAudio: ByteArray? = null
    private var aiCall: dev.esp32.smartwatch.ai.VoiceAiClient.Call? = null
    private var voiceConsent: dev.esp32.smartwatch.ai.PaidVoiceConsent.Ticket? = null
    private var aiConsent: dev.esp32.smartwatch.ai.PaidVoiceConsent.Ticket? = null
    private var voiceStream: VoiceProtocol.StreamReceiver? = null
    private var streamCapable = false
    private var streamSubscribed = false
    private var voiceLastProgress = 0L
    private var streamNeedsGrant = false
    private var voiceYielded = false
    private val streamCallbacks = java.util.concurrent.atomic.AtomicInteger()
    private fun queueGrant(retry: Boolean = false) {
        val stream = voiceStream ?: return
        if (pending?.uuid == VoiceProtocol.CONTROL || operations.any { it.uuid == VoiceProtocol.CONTROL }) { if (!retry) streamNeedsGrant = true; return }
        streamNeedsGrant = false
        operations.add(Operation(VoiceProtocol.CONTROL,writeValue=stream.nextGrant(retry)))
        if (retry) Log.w(TAG,"Voice window retry: epoch=${stream.epoch} prefix_bytes=${stream.receiver.offset}")
        voiceLastProgress = android.os.SystemClock.elapsedRealtime()
        nextOperation()
    }
    private fun fallbackVoice() {
        val receiver = voiceReceiver ?: return
        voiceStream = null; streamNeedsGrant = false; handler.removeCallbacks(voiceWatchdog)
        operations.removeAll { it.uuid == VoiceProtocol.CONTROL || it.uuid == VoiceProtocol.DATA }
        Log.w(TAG,"Voice notifications unavailable; resuming secure pull at offset=${receiver.offset}")
        operations.add(Operation(VoiceProtocol.CONTROL,writeValue=VoiceProtocol.control(receiver.metadata.session,receiver.offset)))
        nextOperation()
    }
    private fun completeVoice(receiver: VoiceProtocol.Receiver, frameBytes: Int) {
        if (VoiceProtocol.RESULT in characteristics) {
            aiSession = receiver.metadata.session; aiAudio = receiver.takeAudio(); aiConsent = voiceConsent
            operations.add(Operation(VoiceProtocol.CONTROL,writeValue=VoiceProtocol.process(aiSession,receiver.offset)))
        } else operations.add(Operation(VoiceProtocol.CONTROL,writeValue=VoiceProtocol.control(receiver.metadata.session,receiver.offset,true)))
        val elapsed = (android.os.SystemClock.elapsedRealtime()-voiceStarted).coerceAtLeast(1)
        Log.i(TAG,"Voice transfer verified: bytes=${receiver.offset} chunks=$voiceChunks duration_ms=$elapsed bytes_per_second=${receiver.offset*1000L/elapsed} session_offsets_crc=PASS")
        Log.i(TAG,"VoiceLatency trace=${receiver.metadata.session.toUInt().toString(16).padStart(8,'0')} stage=ble_audio mode=${if (voiceStream == null) "pull" else "notify"} duration_ms=$elapsed read_us=$voiceReadUs select_write_us=$voiceWriteUs mtu=$negotiatedMtu last_frame_bytes=$frameBytes")
        clearVoice()
    }
    private fun abortVoice() {
        voiceReceiver?.let { receiver ->
            operations.removeAll { it.uuid == VoiceProtocol.CONTROL || it.uuid == VoiceProtocol.DATA }
            operations.add(Operation(VoiceProtocol.CONTROL,writeValue=VoiceProtocol.control(receiver.metadata.session,receiver.offset,true)))
        }
        clearVoice(); clearAi()
        mutableState.value = state.value.copy(message="Audio transfer failed or expired. Hold to retry.")
        nextOperation()
    }
    private val voiceWatchdog = object : Runnable {
        override fun run() {
            if (voiceStream == null || voiceReceiver == null) return
            val now = android.os.SystemClock.elapsedRealtime()
            try {
                if (now >= voiceDeadline) { abortVoice(); return }
                if (now-voiceLastProgress >= 5000) queueGrant(true)
            } catch (_: IllegalArgumentException) { fallbackVoice() }
            if (voiceStream != null) handler.postDelayed(this,500)
        }
    }
    private fun clearAi() {
        aiCall?.cancel(); aiCall = null; aiAudio?.fill(0); aiAudio = null; aiSession = 0
        operations.removeAll { if (it.uuid == VoiceProtocol.RESULT) { it.writeValue?.fill(0); true } else false }
        aiConsent = null
    }
    private fun deliverAi(session: Int, result: dev.esp32.smartwatch.ai.VoiceAiClient.Result?, error: String?) {
        if (session != aiSession || !state.value.secureLink || VoiceProtocol.RESULT !in characteristics) return
        val accepted = if (dev.esp32.smartwatch.ai.VoiceAiClient.consentValid(aiConsent)) result else null
        val message = if (result != null && accepted == null) "Paid AI consent revoked. Hold to retry" else error
        val frames = if (accepted != null) VoiceProtocol.resultFrames(session,1,accepted.transcription,negotiatedMtu) +
            VoiceProtocol.resultFrames(session,2,accepted.answer,negotiatedMtu)
        else VoiceProtocol.resultFrames(session,3,message ?: "AI service unavailable",negotiatedMtu)
        frames.forEach { operations.add(Operation(VoiceProtocol.RESULT,writeValue=it)) }
        resultDeliveryStartedNs = System.nanoTime()
        Log.i(TAG,"VoiceLatency trace=${session.toUInt().toString(16).padStart(8,'0')} stage=result_queued frames=${frames.size}")
        nextOperation()
    }
    private var lastVoiceAvailability = -1
    private val voicePoll = object : Runnable {
        override fun run() {
            if (serviceEnabled && state.value.secureLink && state.value.connectionState == ConnectionState.CONNECTED &&
                VoiceProtocol.shouldPollMetadata(voiceReceiver != null,pending?.uuid,operations.map { it.uuid }) &&
                VoiceProtocol.META in characteristics) {
                // Queue behind the current operation instead of requiring an idle queue.
                // Media/notification traffic must not hide a capture until its expiry.
                operations.addFirst(Operation(VoiceProtocol.META))
                Log.d(TAG, "Voice metadata poll queued: pending=${pending != null} queued=${operations.size}")
                safely { nextOperation() }
            }
            if (serviceEnabled && gatt != null) handler.postDelayed(this, 2000)
        }
    }
    private fun clearVoice() {
        if (voiceReceiver != null) gatt?.requestConnectionPriority(BluetoothGatt.CONNECTION_PRIORITY_BALANCED)
        voiceReceiver?.clear(); voiceReceiver = null; voiceDeadline = 0; voiceChunks = 0
        voiceStream = null; voiceConsent = null; streamNeedsGrant = false; voiceYielded = false; handler.removeCallbacks(voiceWatchdog)
    }
    fun setActivitySnapshotHandler(handler: (() -> Unit)?) { activitySnapshotHandler = handler }
    fun setMediaCommandHandler(handler: ((MediaCommand) -> Unit)?) { mediaCommandHandler = handler }
    fun updateMedia(value: PhoneMedia) {
        latestMedia = value
        if (serviceEnabled && state.value.connectionState == ConnectionState.CONNECTED && state.value.mediaAvailable && pending == null && operations.isEmpty())
            safely { nextOperation() }
    }
    private var bondReceiverRegistered = false
    private var transportConnected = false
    private var discoveryStarted = false
    private var waitingForMtu = false
    private var negotiatedMtu = 23
    private var automaticConnection = false
    private var adapterReceiverRegistered = false
    private val adapterReceiver = object : BroadcastReceiver() {
        override fun onReceive(ctx: Context, intent: Intent) = safely {
            if (intent.action != BluetoothAdapter.ACTION_STATE_CHANGED || !serviceEnabled || !permitted()) return@safely
            when (adapter?.state) {
                BluetoothAdapter.STATE_OFF, BluetoothAdapter.STATE_TURNING_OFF -> {
                    stopScan(); cancelReconnect(); releaseGatt()
                    mutableState.value = freshState(ConnectionState.RECONNECTING, "Bluetooth is off. Waiting for it to be enabled.")
                        .copy(disconnectReason = DisconnectReason.UNEXPECTED_DISCONNECT)
                }
                BluetoothAdapter.STATE_ON -> if (reconnectAllowed && lastDevice != null && gatt == null) {
                    retryCount = 0; scheduleReconnect()
                }
                BluetoothAdapter.STATE_TURNING_ON -> Unit
            }
        }
    }
    private val bondReceiver = object : BroadcastReceiver() {
        override fun onReceive(ctx: Context, intent: Intent) { safely {
            if (!hasPermissions()) return@safely
            val device = intent.getParcelableExtra(BluetoothDevice.EXTRA_DEVICE, BluetoothDevice::class.java) ?: return@safely
            if (gatt == null || lastDevice?.address != device.address) return@safely
            when (intent.action) {
                BluetoothDevice.ACTION_PAIRING_REQUEST -> {
                    if (state.value.secureLink) return@safely
                    if (automaticConnection) {
                        securityFailure("Unexpected pairing request during bonded recovery. Repair stale keys explicitly on BOTH devices from the app."); return@safely
                    }
                    // Android owns the numeric-comparison dialog. Never auto-confirm or abort its broadcast.
                    mutableState.value = state.value.copy(pairingState = PairingState.PAIRING,
                        message = "Compare the six-digit code on the watch and Android pairing dialog. Confirm on BOTH devices only if it matches.")
                    Log.i(TAG, "System pairing confirmation requested")
                }
                BluetoothDevice.ACTION_BOND_STATE_CHANGED -> when (device.bondState) {
                    BluetoothDevice.BOND_BONDING -> {
                        if (automaticConnection) {
                            securityFailure("Bond repair requires explicit user pairing. Automatic recovery stopped."); return@safely
                        }
                        mutableState.value = state.value.copy(connectionState = ConnectionState.PAIRING, pairingState = PairingState.PAIRING)
                        armTimeout(60_000) { securityFailure("Pairing timed out. Retry; if keys are stale, forget on both devices.") }
                    }
                    BluetoothDevice.BOND_BONDED -> {
                        if (state.value.secureLink) return@safely
                        Log.i(TAG, "Android bond established; protected GATT verification still required")
                        mutableState.value = state.value.copy(pairingState = PairingState.BONDED)
                        discoverSecureServices()
                    }
                    BluetoothDevice.BOND_NONE -> securityFailure("Pairing was rejected/failed or the bond was removed. If keys are stale, use Forget paired phone on the watch and Forget ESP32 Smartwatch in Android Bluetooth settings, then reconnect.")
                }
            }
        } }
    }
    private fun registerBondReceiver() {
        if (bondReceiverRegistered) return
        context.registerReceiver(bondReceiver, IntentFilter().apply {
            addAction(BluetoothDevice.ACTION_BOND_STATE_CHANGED); addAction(BluetoothDevice.ACTION_PAIRING_REQUEST)
        }, Context.RECEIVER_EXPORTED) // Bluetooth broadcasts originate in the privileged Bluetooth process.
        bondReceiverRegistered = true
    }
    private fun securityFailure(message: String) {
        fail(message)
        mutableState.value = state.value.copy(pairingState = PairingState.REJECTED_OR_FAILED, disconnectReason = DisconnectReason.SECURITY_FAILURE)
    }
    private fun discoverSecureServices() {
        val source = gatt ?: return
        if (!transportConnected || discoveryStarted || source.device.bondState != BluetoothDevice.BOND_BONDED) return
        discoveryStarted = true
        mutableState.value = state.value.copy(connectionState = ConnectionState.DISCOVERING_SERVICES, pairingState = PairingState.VERIFYING,
            message = "Verifying authenticated, encrypted watch communication…")
        waitingForMtu = source.requestMtu(517)
        if (waitingForMtu) armTimeout(10_000) { unexpectedDisconnect("MTU negotiation timed out; reconnecting securely.") }
        else startServiceDiscovery(source)
    }
    private fun startServiceDiscovery(source: BluetoothGatt) {
        if (servicesInFlight || servicesConfigured) return
        servicesInFlight = true
        if (!source.discoverServices()) unexpectedDisconnect("Secure service discovery could not start.")
        else armTimeout(20_000) { unexpectedDisconnect("Secure communication timed out. Retrying the existing bond; no pairing downgrade.") }
    }
    private var servicesInFlight = false
    private var servicesConfigured = false

    fun refreshNotificationAccess() {
        val granted = context.getSystemService(NotificationManager::class.java)
            .isNotificationListenerAccessGranted(ComponentName(context, WatchNotificationListener::class.java))
        mutableState.value = state.value.copy(notificationAccess = if (granted) NotificationAccess.GRANTED else NotificationAccess.NOT_GRANTED)
        if (!granted) {
            phoneNotifications.clear()
            operations.removeAll { it.notificationChunk }
            if (state.value.sendingNotification) mutableState.value = state.value.copy(notificationError = "Notification access revoked; pending content dropped.")
            if (pending?.notificationChunk != true) mutableState.value = state.value.copy(sendingNotification = false)
        }
    }
    private fun freshState(connection: ConnectionState = ConnectionState.DISCONNECTED, message: String? = null,
                           devices: List<NearbyWatch> = state.value.devices) = WatchState(connectionState = connection,
        devices = devices, message = message, notificationAccess = state.value.notificationAccess,
        companionEnabled = serviceEnabled, savedWatchAvailable = preferences.contains("watch_address"),
        pairingState = try { when (lastDevice?.bondState) {
            BluetoothDevice.BOND_BONDED -> PairingState.BONDED
            BluetoothDevice.BOND_BONDING -> PairingState.PAIRING
            else -> PairingState.NOT_BONDED
        } } catch (_: SecurityException) { PairingState.NOT_BONDED },
        disconnectReason = state.value.disconnectReason)

    fun forwardNotification(item: PhoneNotification) { handler.post {
        refreshNotificationAccess()
        if (!serviceEnabled || state.value.notificationAccess != NotificationAccess.GRANTED || !state.value.secureLink || state.value.connectionState != ConnectionState.CONNECTED ||
            !state.value.notificationTransportAvailable || !hasPermissions()) {
            Log.d(TAG, "Phone notification dropped (access/disconnected/unsupported)"); return@post
        }
        if (phoneNotifications.size == 5) { phoneNotifications.removeFirst(); Log.d(TAG, "Notification send queue full; oldest dropped") }
        phoneNotifications.add(item)
        if (pending == null && operations.isEmpty()) safely { nextOperation() }
    } }

    fun hasPermissions(): Boolean = listOf(Manifest.permission.BLUETOOTH_SCAN, Manifest.permission.BLUETOOTH_CONNECT)
        .all { context.checkSelfPermission(it) == PackageManager.PERMISSION_GRANTED }

    private fun permitted(): Boolean {
        if (hasPermissions()) return true
        Log.i(TAG, "BLE permissions missing")
        fail("Nearby devices permission is required. Grant it to scan and connect.")
        return false
    }

    private inline fun safely(block: () -> Unit) {
        try { block() } catch (_: SecurityException) {
            fail("Bluetooth permission was revoked. Grant Nearby devices permission again.")
        } catch (_: IllegalStateException) { fail("Bluetooth is unavailable. Enable Bluetooth and retry.") }
    }

    fun permissionResult(granted: Boolean) {
        Log.i(TAG, "BLE permission result: $granted")
        if (!granted) fail("Nearby devices permission denied. You can grant it in Android app settings.")
        else mutableState.value = state.value.copy(message = null)
    }

    fun serviceStarted() {
        serviceEnabled = true
        mutableState.value = state.value.copy(companionEnabled = true, disconnectReason = DisconnectReason.NONE)
        refreshNotificationAccess()
        if (!adapterReceiverRegistered) {
            context.registerReceiver(adapterReceiver, IntentFilter(BluetoothAdapter.ACTION_STATE_CHANGED), Context.RECEIVER_EXPORTED)
            adapterReceiverRegistered = true
        }
    }
    fun serviceStopped(user: Boolean) {
        serviceEnabled = false; reconnectAllowed = false; lastDevice = null
        cancelReconnect(); stopScan(); releaseGatt()
        if (adapterReceiverRegistered) { context.unregisterReceiver(adapterReceiver); adapterReceiverRegistered = false }
        val error = !user && state.value.connectionState == ConnectionState.ERROR
        mutableState.value = freshState(if (error) ConnectionState.ERROR else ConnectionState.DISCONNECTED,
            if (error) state.value.message else null).copy(disconnectReason = if (user) DisconnectReason.USER_DISCONNECTED else DisconnectReason.SERVICE_STOPPED)
        Log.i(TAG, "Service stopped: ${state.value.disconnectReason}")
    }
    fun serviceStartFailed(message: String) { fail(message) }
    fun resumeBonded() = safely {
        if (!serviceEnabled || !permitted()) return@safely
        val address = preferences.getString("watch_address", null)
        if (address == null || !BluetoothAdapter.checkBluetoothAddress(address)) {
            fail("No saved secure watch. Scan and connect first."); return@safely
        }
        stopScan(); cancelReconnect(); releaseGatt()
        lastDevice = adapter?.getRemoteDevice(address)
        reconnectAllowed = true; retryCount = 0
        mutableState.value = freshState(ConnectionState.CONNECTING).copy(disconnectReason = DisconnectReason.NONE)
        lastDevice?.let { connectDevice(it, recovery = true) } ?: fail("Bluetooth adapter unavailable.")
    }

    fun startScan() = safely {
        if (!serviceEnabled || !permitted()) return@safely
        if (adapter?.isEnabled != true) { fail("Enable Bluetooth, then scan again."); return@safely }
        if (gatt != null) return@safely
        stopScan()
        cancelReconnect()
        reconnectAllowed = false
        lastDevice = null
        devices.clear()
        mutableState.value = freshState(ConnectionState.SCANNING, devices = emptyList()).copy(disconnectReason = DisconnectReason.NONE)
        val callback = object : ScanCallback() {
            override fun onScanResult(callbackType: Int, result: ScanResult) = deliver(result)
            override fun onBatchScanResults(results: MutableList<ScanResult>) = results.forEach(::deliver)
            private fun deliver(result: ScanResult) { handler.post {
                if (scanCallback !== this) return@post
                safely {
                    if (!permitted()) return@safely
                    val id = result.device.address
                    if (!devices.containsKey(id)) Log.d(TAG, "Compatible watch discovered")
                    devices[id] = result.device
                    val watch = NearbyWatch(id, result.scanRecord?.deviceName ?: "ESP32 Smartwatch", result.rssi)
                    mutableState.value = state.value.copy(devices = (state.value.devices.filterNot { it.id == id } + watch).sortedByDescending { it.rssi })
                }
            } }
            override fun onScanFailed(errorCode: Int) { handler.post {
                if (scanCallback === this) fail("Scan failed ($errorCode). Wait a moment and retry.")
            } }
        }
        scanner = adapter.bluetoothLeScanner
        if (scanner == null) { fail("Bluetooth scanner unavailable."); return@safely }
        scanCallback = callback
        Log.i(TAG, "Scan started (15s, Watch Service filter)")
        scanner?.startScan(listOf(ScanFilter.Builder().setServiceUuid(ParcelUuid(WatchBleProtocol.WATCH_SERVICE)).build()),
            ScanSettings.Builder().setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY).build(), callback)
        armTimeout(15_000) { stopScan() }
    }

    fun stopScan() {
        val callback = scanCallback ?: return
        scanCallback = null
        clearTimeout()
        try { scanner?.stopScan(callback) } catch (_: SecurityException) { Log.w(TAG, "Permission revoked during scan stop") }
        scanner = null
        Log.i(TAG, "Scan stopped")
        if (state.value.connectionState == ConnectionState.SCANNING) mutableState.value = state.value.copy(
            connectionState = ConnectionState.DISCONNECTED,
            message = if (devices.isEmpty()) "No compatible watch found. Check watch Bluetooth and scan again." else null)
    }

    fun connect(id: String) = safely {
        if (!serviceEnabled || !permitted()) return@safely
        val device = devices[id] ?: return@safely
        stopScan()
        cancelReconnect()
        releaseGatt()
        retryCount = 0
        reconnectAllowed = true
        lastDevice = device
        mutableState.value = state.value.copy(disconnectReason = DisconnectReason.NONE)
        connectDevice(device)
    }

    private fun connectDevice(device: BluetoothDevice, recovery: Boolean = false) {
        if (!serviceEnabled || !permitted()) return
        if (adapter?.isEnabled != true) { unexpectedDisconnect("Bluetooth is off. Waiting for it to be enabled."); return }
        if (recovery && device.bondState != BluetoothDevice.BOND_BONDED) {
            securityFailure("The saved watch bond is missing. Pair explicitly from the app; automatic recovery will not create a new bond."); return
        }
        mutableState.value = freshState(ConnectionState.CONNECTING)
        automaticConnection = recovery
        registerBondReceiver()
        Log.i(TAG, "Connecting GATT")
        gatt = device.connectGatt(context, false, callback, BluetoothDevice.TRANSPORT_LE)
        if (gatt == null) { unexpectedDisconnect("Could not start GATT connection."); return }
        armTimeout(20_000) { unexpectedDisconnect("Watch not reachable. Retrying with backoff.") }
    }

    private val callback = object : BluetoothGattCallback() {
        override fun onMtuChanged(source: BluetoothGatt, mtu: Int, status: Int) = onSession(source) {
            negotiatedMtu = if (status == BluetoothGatt.GATT_SUCCESS) mtu.coerceIn(23, 517) else 23
            Log.i(TAG, "Negotiated ATT MTU=$negotiatedMtu")
            if (waitingForMtu) { waitingForMtu = false; clearTimeout(); startServiceDiscovery(source) }
        }
        override fun onConnectionStateChange(source: BluetoothGatt, status: Int, newState: Int) = onSession(source) {
            Log.i(TAG, "GATT connection callback: status=$status state=$newState")
            if (status != BluetoothGatt.GATT_SUCCESS || newState == BluetoothProfile.STATE_DISCONNECTED) {
                // Authentication/key-loss errors require explicit recovery, not repeated re-pairing.
                if (status == BluetoothGatt.GATT_INSUFFICIENT_AUTHENTICATION || status == BluetoothGatt.GATT_INSUFFICIENT_ENCRYPTION) {
                    securityFailure("Secure connection failed ($status). For stale keys, forget on BOTH devices and pair explicitly in the app.")
                    return@onSession
                }
                unexpectedDisconnect("Watch disconnected unexpectedly (status $status).")
            } else if (newState == BluetoothProfile.STATE_CONNECTED) {
                transportConnected = true
                when (source.device.bondState) {
                    BluetoothDevice.BOND_BONDED -> discoverSecureServices()
                    else -> {
                        if (automaticConnection) {
                            securityFailure("Bond was removed while reconnecting. Pair explicitly from the app."); return@onSession
                        }
                        mutableState.value = state.value.copy(connectionState = ConnectionState.PAIRING, pairingState = PairingState.PAIRING,
                            message = "Compare the code and accept pairing on the watch and Android. No private data is available before secure verification.")
                        if (source.device.bondState == BluetoothDevice.BOND_NONE && !source.device.createBond()) {
                            securityFailure("Android pairing could not start. Retry from Bluetooth settings."); return@onSession
                        }
                        armTimeout(60_000) { securityFailure("Pairing timed out. Retry; stale keys require Forget on both devices.") }
                    }
                }
            }
        }

        override fun onServicesDiscovered(source: BluetoothGatt, status: Int) = onSession(source) {
            if (servicesConfigured) { Log.i(TAG,"Duplicate service-discovery callback ignored; initialization already queued"); return@onSession }
            servicesInFlight = false
            if (status != BluetoothGatt.GATT_SUCCESS) { unexpectedDisconnect("Service discovery failed ($status)."); return@onSession }
            servicesConfigured = true
            Log.i(TAG, "Services discovered; verifying protocol characteristics")
            val watch = source.getService(WatchBleProtocol.WATCH_SERVICE)
            val battery = source.getService(WatchBleProtocol.BATTERY_SERVICE)
            for (uuid in listOf(WatchBleProtocol.SECURITY, WatchBleProtocol.VERSION) + WatchBleProtocol.TELEMETRY) {
                val c = (if (uuid == WatchBleProtocol.BATTERY) battery else watch)?.getCharacteristic(uuid)
                if (c == null || c.properties and BluetoothGattCharacteristic.PROPERTY_READ == 0 ||
                    (uuid in WatchBleProtocol.NOTIFY && (c.properties and BluetoothGattCharacteristic.PROPERTY_NOTIFY == 0 || c.getDescriptor(WatchBleProtocol.CCCD) == null))) {
                    securityFailure("Incompatible watch: secure GATT service or required telemetry missing. Install security-enabled firmware."); return@onSession
                }
                characteristics[uuid] = c
            }
            // Additive v1 extension: telemetry-only firmware remains usable.
            val controls = WatchBleProtocol.CONTROLS.mapNotNull { watch?.getCharacteristic(it) }
            Log.i(TAG, "Control discovery: found=${controls.size}/${WatchBleProtocol.CONTROLS.size}")
            controls.forEach { Log.d(TAG, "Control ${it.uuid}: properties=${it.properties}, CCCD=${it.getDescriptor(WatchBleProtocol.CCCD) != null}") }
            val supportsControls = controls.size == WatchBleProtocol.CONTROLS.size && controls.all {
                it.properties and BluetoothGattCharacteristic.PROPERTY_READ != 0 &&
                it.properties and BluetoothGattCharacteristic.PROPERTY_WRITE != 0 &&
                it.properties and BluetoothGattCharacteristic.PROPERTY_NOTIFY != 0 && it.getDescriptor(WatchBleProtocol.CCCD) != null
            }
            if (supportsControls) controls.forEach { characteristics[it.uuid] = it }
            mutableState.value = state.value.copy(controlsAvailable = supportsControls)
            val notification = watch?.getCharacteristic(WatchBleProtocol.PHONE_NOTIFICATION)
            val supportsNotifications = notification != null && notification.properties and BluetoothGattCharacteristic.PROPERTY_WRITE != 0
            if (supportsNotifications) characteristics[WatchBleProtocol.PHONE_NOTIFICATION] = notification!!
            mutableState.value = state.value.copy(notificationTransportAvailable = supportsNotifications)
            val media = watch?.getCharacteristic(WatchBleProtocol.MEDIA)
            val command = watch?.getCharacteristic(WatchBleProtocol.MEDIA_COMMAND)
            val supportsMedia = media != null && command != null && media.properties and BluetoothGattCharacteristic.PROPERTY_WRITE != 0 &&
                command.properties and BluetoothGattCharacteristic.PROPERTY_NOTIFY != 0 && command.getDescriptor(WatchBleProtocol.CCCD) != null
            Log.i(TAG, "Media discovery: metadata=${media != null}, commands=${command != null}, supported=$supportsMedia")
            if (supportsMedia) { characteristics[media!!.uuid] = media; characteristics[command!!.uuid] = command }
            mutableState.value = state.value.copy(mediaAvailable = supportsMedia)
            val mediaCapabilities = watch?.getCharacteristic(WatchBleProtocol.MEDIA_CAPABILITIES)
            val activity = watch?.getCharacteristic(WatchBleProtocol.ACTIVITY)
            val activityAvailable = activity != null && activity.properties and BluetoothGattCharacteristic.PROPERTY_READ != 0
            if (activityAvailable) characteristics[WatchBleProtocol.ACTIVITY] = activity!!
            mutableState.value = state.value.copy(activityAvailable = activityAvailable)
            val goal = watch?.getCharacteristic(WatchBleProtocol.STEP_GOAL)
            if (goal != null && goal.properties and BluetoothGattCharacteristic.PROPERTY_READ != 0)
                characteristics[WatchBleProtocol.STEP_GOAL] = goal
            mediaVolumeSupported = false
            if (supportsMedia && mediaCapabilities != null && mediaCapabilities.properties and BluetoothGattCharacteristic.PROPERTY_READ != 0) {
                characteristics[mediaCapabilities.uuid] = mediaCapabilities
            }
            // One outstanding GATT operation. Version must succeed before custom values are decoded.
            listOf(VoiceProtocol.META, VoiceProtocol.DATA, VoiceProtocol.CONTROL, VoiceProtocol.RESULT).forEach { uuid ->
                watch?.getCharacteristic(uuid)?.let { characteristics[uuid] = it }
            }
            streamCapable = false; streamSubscribed = false
            val caps = watch?.getCharacteristic(VoiceProtocol.CAPS)
            val stream = watch?.getCharacteristic(VoiceProtocol.STREAM)
            if (dev.esp32.smartwatch.BuildConfig.VOICE_NOTIFY_ENABLED && caps != null && stream != null &&
                caps.properties and BluetoothGattCharacteristic.PROPERTY_READ != 0 &&
                stream.properties and BluetoothGattCharacteristic.PROPERTY_NOTIFY != 0 && stream.getDescriptor(WatchBleProtocol.CCCD) != null) {
                characteristics[caps.uuid] = caps; characteristics[stream.uuid] = stream
            }
            handler.removeCallbacks(voicePoll); handler.postDelayed(voicePoll, 2000)
            operations.add(Operation(WatchBleProtocol.SECURITY))
            operations.add(Operation(WatchBleProtocol.VERSION))
            if (VoiceProtocol.CAPS in characteristics) operations.add(Operation(VoiceProtocol.CAPS))
            if (WatchBleProtocol.MEDIA_CAPABILITIES in characteristics) operations.add(Operation(WatchBleProtocol.MEDIA_CAPABILITIES))
            WatchBleProtocol.NOTIFY.filter { it in characteristics }.forEach { operations.add(Operation(it)) }
            WatchBleProtocol.NOTIFY.filter { it in characteristics }.forEach { operations.add(Operation(it, subscribe = true)) }
            if (supportsMedia) operations.add(Operation(WatchBleProtocol.MEDIA_COMMAND, subscribe = true))
            if (VoiceProtocol.STREAM in characteristics) operations.add(Operation(VoiceProtocol.STREAM,subscribe=true))
            val changed = source.getService(WatchBleProtocol.GATT_SERVICE)?.getCharacteristic(WatchBleProtocol.SERVICE_CHANGED)
            if (!databaseSubscriptionDone && changed != null && changed.properties and BluetoothGattCharacteristic.PROPERTY_INDICATE != 0 &&
                changed.getDescriptor(WatchBleProtocol.CCCD) != null) {
                characteristics[changed.uuid] = changed
                operations.add(1, Operation(changed.uuid, subscribe = true))
            }
            nextOperation()
        }

        override fun onCharacteristicRead(source: BluetoothGatt, c: BluetoothGattCharacteristic, value: ByteArray, status: Int) {
            val snapshot = value.copyOf()
            onSession(source) {
                val op = pending ?: return@onSession
                if (op.uuid != c.uuid || op.subscribe || op.writeValue != null) return@onSession
                if (c.uuid != WatchBleProtocol.SECURITY && (status == BluetoothGatt.GATT_INSUFFICIENT_AUTHENTICATION || status == BluetoothGatt.GATT_INSUFFICIENT_ENCRYPTION)) {
                    snapshot.fill(0)
                    securityFailure("Watch rejected authenticated access ($status). Secure connection lost; companion stopped to prevent repeated failures.")
                    return@onSession
                }
                Log.d(TAG, "Characteristic read: ${c.uuid}, status=$status")
                if (c.uuid == VoiceProtocol.CAPS) {
                    streamCapable = status == BluetoothGatt.GATT_SUCCESS && VoiceProtocol.supportsStream(snapshot)
                    Log.i(TAG,"Voice notification capability supported=$streamCapable")
                    snapshot.fill(0); pending = null; nextOperation(); return@onSession
                }
                if (c.uuid == VoiceProtocol.META || c.uuid == VoiceProtocol.DATA) {
                    try {
                        require(status == BluetoothGatt.GATT_SUCCESS && state.value.secureLink)
                        if (c.uuid == VoiceProtocol.META) {
                            if (aiSession != 0) {
                                val session = if (snapshot.size == 18) java.nio.ByteBuffer.wrap(snapshot).order(java.nio.ByteOrder.LITTLE_ENDIAN).getInt(2) else 0
                                if (snapshot.size != 18 || snapshot[0].toInt() != 1 || snapshot[1].toInt() != 3 || session != aiSession) clearAi()
                                snapshot.fill(0); pending = null; nextOperation(); return@onSession
                            }
                            if (snapshot.size == 18 && snapshot[1].toInt() == 3) { snapshot.fill(0); pending = null; nextOperation(); return@onSession }
                            val metadata = VoiceProtocol.metadata(snapshot)
                            val available = if (metadata == null) 0 else 1
                            if (available != lastVoiceAvailability) {
                                lastVoiceAvailability = available
                                Log.i(TAG, "Voice capture available=$available")
                            }
                            if (metadata != null && negotiatedMtu >= 64 && VoiceProtocol.CONTROL in characteristics && VoiceProtocol.DATA in characteristics) {
                                clearVoice(); voiceReceiver = VoiceProtocol.Receiver(metadata)
                                voiceConsent = dev.esp32.smartwatch.ai.VoiceAiClient.consentTicket()
                                Log.i(TAG,"Voice claim consent eligible=${voiceConsent != null}; later opt-in cannot authorize this clip")
                                voiceStarted = android.os.SystemClock.elapsedRealtime()
                                voiceReadUs = 0; voiceWriteUs = 0
                                source.requestConnectionPriority(BluetoothGatt.CONNECTION_PRIORITY_HIGH)
                                // 256 kB / measured 2.18 kB/s ~= 118s; allow bounded margin.
                                voiceDeadline = voiceStarted + 175_000
                                Log.i(TAG, "Voice transfer started: bytes=${metadata.size} mtu=$negotiatedMtu")
                                if (streamCapable && streamSubscribed) {
                                    voiceStream = VoiceProtocol.StreamReceiver(requireNotNull(voiceReceiver),negotiatedMtu)
                                    // Poll callback owns the current GATT read; queue its grant after clearing pending.
                                    operations.add(Operation(VoiceProtocol.CONTROL,writeValue=requireNotNull(voiceStream).nextGrant()))
                                    voiceLastProgress = voiceStarted
                                    handler.postDelayed(voiceWatchdog,500)
                                } else operations.add(Operation(VoiceProtocol.CONTROL, writeValue = VoiceProtocol.control(metadata.session, 0)))
                            } else if (metadata != null) {
                                Log.w(TAG, "Voice capture cannot transfer: mtu=$negotiatedMtu control=${VoiceProtocol.CONTROL in characteristics} data=${VoiceProtocol.DATA in characteristics}")
                                mutableState.value = state.value.copy(message = "Audio transfer unavailable. Reconnect companion to negotiate MTU/services.")
                            }
                        } else {
                            voiceReadUs += (System.nanoTime()-operationStartedNs)/1000
                            val receiver = requireNotNull(voiceReceiver)
                            require(android.os.SystemClock.elapsedRealtime() < voiceDeadline)
                            val complete = receiver.accept(snapshot)
                            voiceChunks++
                            if (complete) {
                                completeVoice(receiver,snapshot.size)
                            } else operations.add(Operation(VoiceProtocol.CONTROL, writeValue = VoiceProtocol.control(receiver.metadata.session, receiver.offset)))
                        }
                    } catch (_: IllegalArgumentException) {
                        voiceReceiver?.let { Log.w(TAG, "Voice transfer aborted: received_bytes=${it.offset} chunks=$voiceChunks elapsed_ms=${android.os.SystemClock.elapsedRealtime() - voiceStarted}") }
                        voiceReceiver?.let { operations.add(Operation(VoiceProtocol.CONTROL,
                            writeValue = VoiceProtocol.control(it.metadata.session, it.offset, true))) }
                        clearVoice(); Log.w(TAG, "Voice transfer rejected or cancelled; audio discarded")
                    }
                    snapshot.fill(0); pending = null; nextOperation(); return@onSession
                }
                if (c.uuid == WatchBleProtocol.SECURITY) {
                    if (status != BluetoothGatt.GATT_SUCCESS && status != BluetoothGatt.GATT_INSUFFICIENT_AUTHENTICATION &&
                        status != BluetoothGatt.GATT_INSUFFICIENT_ENCRYPTION) {
                        unexpectedDisconnect("Protected GATT transport failed ($status); retrying the existing bond."); return@onSession
                    }
                    if (status != BluetoothGatt.GATT_SUCCESS || source.device.bondState != BluetoothDevice.BOND_BONDED || !WatchBleProtocol.secureStatus(snapshot)) {
                        securityFailure("Authenticated/encrypted watch verification failed ($status). For stale keys, forget on BOTH devices and pair again."); return@onSession
                    }
                    mutableState.value = state.value.copy(secureLink = true, pairingState = PairingState.SECURE, message = null)
                    Log.i(TAG, "Protected GATT verified: SC, MITM, encryption, 128-bit keys, trusted bond")
                    pending = null; nextOperation(); return@onSession
                }
                if (op.readback) {
                    val valid = status == BluetoothGatt.GATT_SUCCESS && decode(c.uuid, snapshot, strict = false)
                    mutableState.value = state.value.copy(writingControl = false,
                        controlsAvailable = state.value.controlsAvailable && valid,
                        controlError = if (valid) state.value.controlError else "Could not confirm settings. Reconnect before changing them again.")
                    pending = null
                    nextOperation()
                    return@onSession
                }
                if (c.uuid == WatchBleProtocol.ACTIVITY) {
                    try {
                        require(status == BluetoothGatt.GATT_SUCCESS)
                        val activity = dev.esp32.smartwatch.activity.WatchActivityProtocol.decode(source.device.address, snapshot)
                        mutableState.value = state.value.copy(activity = activity, activityUpdatedAt = System.currentTimeMillis(),
                            activityError = if (activity.days.isEmpty()) "Watch calendar is not ready." else null)
                        activitySnapshotHandler?.invoke()
                    } catch (_: Exception) {
                        mutableState.value = state.value.copy(activity = null, activityError = "Dated watch activity unavailable or malformed ($status).")
                    }
                    pending = null; nextOperation(); return@onSession
                }
                if (c.uuid == WatchBleProtocol.STEP_GOAL) {
                    try {
                        require(status == BluetoothGatt.GATT_SUCCESS)
                        mutableState.value = state.value.copy(stepGoal = WatchBleProtocol.stepGoal(snapshot),
                            stepGoalUpdatedAt = System.currentTimeMillis(), stepGoalError = null)
                    } catch (_: IllegalArgumentException) {
                        mutableState.value = state.value.copy(stepGoal = null, stepGoalError = "Watch goal read failed; refresh to retry.")
                    }
                    pending = null; nextOperation(); return@onSession
                }
                // Firmware intentionally returns ATT Unlikely Error when PMIC data is unavailable.
                if (c.uuid == WatchBleProtocol.BATTERY && status == 0x0e) {
                    mutableState.value = state.value.copy(battery = null, message = "Battery measurement unavailable.")
                } else if (status != BluetoothGatt.GATT_SUCCESS) { fail("Characteristic read failed ($status)."); return@onSession }
                else if (!decode(c.uuid, snapshot, strict = true)) return@onSession
                pending = null
                nextOperation()
            }
        }

        override fun onCharacteristicWrite(source: BluetoothGatt, c: BluetoothGattCharacteristic, status: Int) = onSession(source) {
            val op = pending ?: return@onSession
            if (op.uuid != c.uuid || op.writeValue == null) return@onSession
            if (status == BluetoothGatt.GATT_INSUFFICIENT_AUTHENTICATION || status == BluetoothGatt.GATT_INSUFFICIENT_ENCRYPTION) {
                op.writeValue.fill(0)
                securityFailure("Watch rejected authenticated access ($status). Secure connection lost; companion stopped to prevent repeated failures.")
                return@onSession
            }
            if (c.uuid == VoiceProtocol.RESULT) {
                op.writeValue.fill(0)
                if (status != BluetoothGatt.GATT_SUCCESS) { clearAi(); Log.w(TAG,"AI result delivery cancelled/rejected") }
                else if (operations.none { it.uuid == VoiceProtocol.RESULT }) {
                    Log.i(TAG,"VoiceLatency trace=${aiSession.toUInt().toString(16).padStart(8,'0')} stage=result_delivered duration_us=${(System.nanoTime()-resultDeliveryStartedNs)/1000}")
                    clearAi()
                }
                pending = null; nextOperation(); return@onSession
            }
            if (c.uuid == VoiceProtocol.CONTROL) {
                if (voiceReceiver != null) voiceWriteUs += (System.nanoTime()-operationStartedNs)/1000
                if (op.writeValue[0].toInt() == 2) {
                    pending = null
                    if (status != BluetoothGatt.GATT_SUCCESS && voiceReceiver != null) fallbackVoice()
                    else {
                        voiceLastProgress = android.os.SystemClock.elapsedRealtime()
                        if (streamNeedsGrant && voiceStream != null) queueGrant() else nextOperation()
                    }
                    return@onSession
                }
                if (op.writeValue[1].toInt() == 3) {
                    if (status == BluetoothGatt.GATT_SUCCESS && aiSession != 0 && aiAudio != null) {
                        val session = aiSession; val audio = requireNotNull(aiAudio); aiAudio = null
                        if (dev.esp32.smartwatch.BuildConfig.VOICE_TRANSFER_ONLY) {
                            audio.fill(0)
                            Log.i(TAG, "Voice transfer-only validation: CRC verified; backend request BLOCKED")
                            deliverAi(session,null,"Audio transfer verified. AI paused for testing")
                        } else {
                            Log.i(TAG, "Voice processing acknowledged; starting authenticated backend stage")
                            aiCall = dev.esp32.smartwatch.ai.VoiceAiClient.ask(audio,session,{ Log.i(TAG,it) },aiConsent) { result, error -> handler.post { deliverAi(session,result,error) } }
                        }
                    } else clearAi()
                }
                if (status == BluetoothGatt.GATT_SUCCESS && voiceReceiver != null) operations.add(Operation(VoiceProtocol.DATA))
                else if (status != BluetoothGatt.GATT_SUCCESS) { clearVoice(); Log.w(TAG, "Voice control rejected ($status)") }
                pending = null; nextOperation(); return@onSession
            }
            Log.i(TAG, if (op.mediaChunk) "Media frame write callback: status=$status" else if (op.notificationChunk) "Notification frame write callback: status=$status" else "Control write callback: ${c.uuid}, status=$status")
            if (op.mediaChunk) {
                if (status != BluetoothGatt.GATT_SUCCESS) {
                    operations.removeAll { it.mediaChunk }
                    mutableState.value = state.value.copy(mediaAvailable = false, sendingMedia = false, mediaError = "Media snapshot rejected ($status); reconnect to retry.")
                    mediaInFlight = null
                } else if (operations.none { it.mediaChunk }) {
                    sentMedia = mediaInFlight; mediaInFlight = null
                    mutableState.value = state.value.copy(sendingMedia = false)
                    Log.i(TAG, "Media snapshot delivered")
                }
                pending = null; nextOperation(); return@onSession
            }
            if (op.notificationChunk) {
                if (status != BluetoothGatt.GATT_SUCCESS) {
                    operations.removeAll { it.notificationChunk }
                    mutableState.value = state.value.copy(sendingNotification = false, notificationError = "Watch rejected notification content ($status).")
                    Log.w(TAG, "Notification frame rejected: status=$status")
                } else {
                    Log.d(TAG, "Notification frame acknowledged")
                    if (operations.none { it.notificationChunk }) {
                        mutableState.value = state.value.copy(sendingNotification = false)
                        Log.i(TAG, if (state.value.notificationAccess == NotificationAccess.GRANTED)
                            "Notification message delivered" else "Notification transmission cancelled (access revoked)")
                    }
                }
                pending = null
                nextOperation()
                return@onSession
            }
            if (status != BluetoothGatt.GATT_SUCCESS) mutableState.value = state.value.copy(
                controlError = "Watch rejected the change ($status). Reading its actual value.")
            pending = null
            operations.addFirst(Operation(c.uuid, readback = true))
            nextOperation()
        }

        override fun onDescriptorWrite(source: BluetoothGatt, descriptor: BluetoothGattDescriptor, status: Int) = onSession(source) {
            if (descriptor.uuid != WatchBleProtocol.CCCD || pending != Operation(descriptor.characteristic.uuid, true)) return@onSession
            Log.d(TAG, "Notification subscription: ${descriptor.characteristic.uuid}, status=$status")
            if (status == BluetoothGatt.GATT_INSUFFICIENT_AUTHENTICATION || status == BluetoothGatt.GATT_INSUFFICIENT_ENCRYPTION) {
                securityFailure("Watch rejected secure subscription ($status). Connection authentication lost."); return@onSession
            }
            if (descriptor.characteristic.uuid == VoiceProtocol.STREAM) {
                streamSubscribed = status == BluetoothGatt.GATT_SUCCESS
                if (!streamSubscribed) source.setCharacteristicNotification(descriptor.characteristic,false)
                pending = null; nextOperation(); return@onSession
            }
            if (status != BluetoothGatt.GATT_SUCCESS) { fail("Notification subscription failed ($status)."); return@onSession }
            pending = null
            if (descriptor.characteristic.uuid == WatchBleProtocol.SERVICE_CHANGED) {
                databaseSubscriptionDone = true
                clearTimeout()
                discoveryDelay = Runnable { discoveryDelay = null; nextOperation() }.also { handler.postDelayed(it, 750) }
                return@onSession
            }
            nextOperation()
        }

        override fun onServiceChanged(source: BluetoothGatt) = onSession(source) {
            Log.i(TAG, "Standard Service Changed received; rediscovering actual database")
            discoveryDelay?.let(handler::removeCallbacks); discoveryDelay = null
            clearTimeout()
            waitingForMtu = false; servicesInFlight = false; servicesConfigured = false
            clearAi(); clearVoice(); streamCapable = false; streamSubscribed = false
            operations.clear(); pending = null; characteristics.clear()
            phoneNotifications.clear()
            sentMedia = null; mediaInFlight = null; mediaSequence = null
            databaseSubscriptionDone = true
            mutableState.value = freshState(ConnectionState.DISCOVERING_SERVICES)
            // Bonded Android may invalidate its database asynchronously after this callback.
            discoveryDelay = Runnable {
                discoveryDelay = null
                if (gatt !== source) return@Runnable
                startServiceDiscovery(source)
            }.also { handler.postDelayed(it, 750) }
        }

        override fun onCharacteristicChanged(source: BluetoothGatt, c: BluetoothGattCharacteristic, value: ByteArray) {
            if (c.uuid == VoiceProtocol.STREAM) {
                // Bound audio callback copies even if Handler is busy. Missing frames are
                // recovered from the contiguous prefix with a fresh grant epoch.
                if (value.size !in 17..512) return
                if (streamCallbacks.incrementAndGet() > 16) { streamCallbacks.decrementAndGet(); return }
                val frame = value.copyOf()
                handler.post {
                    try {
                        if (gatt !== source || !state.value.secureLink || !permitted()) return@post
                        val stream = voiceStream ?: return@post
                        require(android.os.SystemClock.elapsedRealtime() < voiceDeadline)
                        when (stream.accept(frame)) {
                            VoiceProtocol.StreamReceiver.Event.PROGRESS -> { voiceChunks++; voiceLastProgress = android.os.SystemClock.elapsedRealtime() }
                            VoiceProtocol.StreamReceiver.Event.WINDOW -> { voiceChunks++; voiceLastProgress = android.os.SystemClock.elapsedRealtime(); queueGrant() }
                            VoiceProtocol.StreamReceiver.Event.COMPLETE -> { voiceChunks++; completeVoice(stream.receiver,frame.size); nextOperation() }
                            VoiceProtocol.StreamReceiver.Event.IGNORE, VoiceProtocol.StreamReceiver.Event.GAP -> Unit
                        }
                    } catch (_: IllegalArgumentException) {
                        Log.w(TAG,"Voice notification integrity/expiry rejected; discarding audio")
                        abortVoice()
                    } finally { frame.fill(0); streamCallbacks.decrementAndGet() }
                }
                return
            }
            val snapshot = value.copyOf()
            onSession(source) {
                if (c.uuid == WatchBleProtocol.MEDIA_COMMAND && state.value.connectionState == ConnectionState.CONNECTED && state.value.secureLink && state.value.mediaAvailable) {
                    val command = try { MediaProtocol.command(snapshot) } catch (_: IllegalArgumentException) { Log.w(TAG, "Invalid media command rejected"); return@onSession }
                    if (command.action != 0 && MediaProtocol.isNewSequence(mediaSequence, command.sequence)) {
                        mediaSequence = command.sequence
                        mediaCommandHandler?.invoke(command)
                    }
                    return@onSession
                }
                if (state.value.secureLink && state.value.protocolVersion == WatchBleProtocol.SUPPORTED_VERSION && c.uuid in WatchBleProtocol.NOTIFY && c.uuid in characteristics) {
                    Log.d(TAG, "Notification: ${c.uuid}")
                    decode(c.uuid, snapshot, strict = false)
                }
            }
        }
    }

    private fun onSession(source: BluetoothGatt, action: () -> Unit) { handler.post {
        if (gatt !== source) return@post // Ignore stale callbacks after close/reconnect.
        safely { if (permitted()) action() }
    } }

    private fun nextOperation() {
        // CONNECTED StateFlow observers can enqueue/start activity reads synchronously.
        // A re-entrant pump must not overwrite that pending read or cancel its timer.
        if (pending != null) return
        clearTimeout()
        val source = gatt ?: return
        // A pull transfer always has one next voice operation queued. Let normal
        // traffic run between chunks instead of requiring an empty queue forever.
        val voiceNext = if (VoiceProtocol.shouldYieldVoice(voiceReceiver != null,voiceYielded,operations.size,operations.firstOrNull()?.uuid)) {
            voiceYielded = true; operations.removeFirst()
        } else null
        if (operations.isEmpty() && state.value.connectionState == ConnectionState.CONNECTED && state.value.secureLink &&
            state.value.mediaAvailable && latestMedia != sentMedia) {
            mediaInFlight = latestMedia
            mutableState.value = state.value.copy(sendingMedia = true, mediaError = null)
            MediaProtocol.frames(latestMedia, notificationId++, mediaVolumeSupported).forEach {
                operations.add(Operation(WatchBleProtocol.MEDIA, writeValue = it, mediaChunk = true))
            }
            Log.i(TAG, "Media snapshot transmission started")
        }
        if (operations.isEmpty() && phoneNotifications.isNotEmpty() && state.value.connectionState == ConnectionState.CONNECTED) {
            val item = phoneNotifications.removeFirst()
            val frames = try { NotificationTransport.frames(item, notificationId++) } catch (_: IllegalArgumentException) {
                Log.w(TAG, "Invalid normalized notification dropped")
                mutableState.value = state.value.copy(notificationError = "Invalid notification dropped.")
                nextOperation(); return
            }
            frames.forEach { operations.add(Operation(WatchBleProtocol.PHONE_NOTIFICATION, writeValue = it, notificationChunk = true)) }
            mutableState.value = state.value.copy(sendingNotification = true, notificationError = null)
            Log.i(TAG, "Notification transmission started: chunks=${frames.size}")
        }
        if (voiceNext != null) operations.add(voiceNext)
        val op = operations.removeFirstOrNull()
        if (op == null) {
            if (!state.value.secureLink || source.device.bondState != BluetoothDevice.BOND_BONDED) {
                securityFailure("Secure communication is not ready."); return
            }
            if (state.value.connectionState != ConnectionState.CONNECTED) {
                retryCount = 0
                preferences.edit().putString("watch_address", source.device.address).apply()
                Log.i(TAG, "Watch ready: initial reads and all subscriptions complete")
                mutableState.value = state.value.copy(connectionState = ConnectionState.CONNECTED, savedWatchAvailable = true,
                    disconnectReason = DisconnectReason.NONE, retryDelayMillis = null)
                if (state.value.mediaAvailable) { nextOperation(); return }
            }
            return
        }
        pending = op
        if (op.uuid == VoiceProtocol.CONTROL || op.uuid == VoiceProtocol.DATA) voiceYielded = false
        operationStartedNs = System.nanoTime()
        val c = characteristics.getValue(op.uuid)
        val started = if (op.writeValue != null) {
            source.writeCharacteristic(c, op.writeValue, BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT) == BluetoothStatusCodes.SUCCESS
        } else if (op.subscribe) {
            source.setCharacteristicNotification(c, true) && source.writeDescriptor(c.getDescriptor(WatchBleProtocol.CCCD),
                if (c.uuid == WatchBleProtocol.SERVICE_CHANGED) BluetoothGattDescriptor.ENABLE_INDICATION_VALUE
                else BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE) == BluetoothStatusCodes.SUCCESS
        } else source.readCharacteristic(c)
        if (!started && (op.uuid == VoiceProtocol.CAPS || op.uuid == VoiceProtocol.STREAM)) {
            pending = null; streamCapable = false; streamSubscribed = false
            nextOperation()
        } else if (!started && op.uuid == VoiceProtocol.CONTROL) {
            pending = null
            if (op.writeValue?.firstOrNull()?.toInt() == 2) fallbackVoice() else abortVoice()
        } else if (!started && op.mediaChunk) {
            pending = null; operations.removeAll { it.mediaChunk }; mediaInFlight = null
            mutableState.value = state.value.copy(mediaAvailable = false, sendingMedia = false, mediaError = "Media transmission could not start; reconnect to retry.")
            nextOperation()
        } else if (!started && op.notificationChunk) {
            pending = null
            operations.removeAll { it.notificationChunk }
            phoneNotifications.clear()
            mutableState.value = state.value.copy(sendingNotification = false, notificationError = "Notification transmission could not start; message dropped.")
        } else if (!started && op.writeValue != null) {
            pending = null
            mutableState.value = state.value.copy(writingControl = false, controlError = "Could not send the change. Previous confirmed value retained; retry.")
        } else if (!started && op.readback) {
            pending = null
            mutableState.value = state.value.copy(writingControl = false, controlsAvailable = false,
                controlError = "Readback could not start. Reconnect to confirm settings.")
        } else if (!started) unexpectedDisconnect("GATT operation could not start. Reconnecting securely.")
        else armTimeout(10_000) { unexpectedDisconnect(if (state.value.writingControl)
            "Settings confirmation timed out. Outcome unknown; reconnect to read actual values."
            else "GATT operation timed out. Reconnect and retry.") }
    }

    private fun decode(uuid: UUID, value: ByteArray, strict: Boolean): Boolean {
        if (uuid == WatchBleProtocol.MEDIA_CAPABILITIES) {
            mediaVolumeSupported = value.size == 1 && value[0].toInt() == 1
            Log.i(TAG, "Relative media volume support=$mediaVolumeSupported")
            return true // Unknown capability falls back to the original media snapshot.
        }
        try {
            mutableState.value = when (uuid) {
                WatchBleProtocol.VERSION -> {
                    val version = WatchBleProtocol.version(value)
                    if (version != WatchBleProtocol.SUPPORTED_VERSION) { fail("Unsupported watch protocol v$version; this app supports v${WatchBleProtocol.SUPPORTED_VERSION}."); return false }
                    state.value.copy(protocolVersion = version)
                }
                WatchBleProtocol.BATTERY -> state.value.copy(battery = WatchBleProtocol.battery(value))
                WatchBleProtocol.STEPS -> state.value.copy(steps = WatchBleProtocol.steps(value))
                WatchBleProtocol.WIFI -> state.value.copy(wifiState = WatchBleProtocol.wifi(value))
                WatchBleProtocol.BRIGHTNESS -> state.value.copy(brightness = WatchBleProtocol.brightness(value))
                WatchBleProtocol.TIMEOUT -> state.value.copy(screenTimeout = WatchBleProtocol.timeout(value))
                WatchBleProtocol.WIFI_ENABLED -> state.value.copy(wifiEnabled = WatchBleProtocol.wifiEnabled(value))
                else -> state.value
            }
            return true
        } catch (_: IllegalArgumentException) {
            if (strict) fail("Malformed watch value. Check firmware compatibility.")
            else { Log.w(TAG, "Malformed notification ignored"); mutableState.value = state.value.copy(message = "Invalid notification ignored.") }
            return false
        }
    }

    fun setBrightness(value: Int) = writeControl(WatchBleProtocol.BRIGHTNESS) { WatchBleProtocol.encodeBrightness(value) }
    fun requestActivitySnapshot() = safely {
        if (!state.value.secureLink || state.value.connectionState != ConnectionState.CONNECTED || !state.value.activityAvailable) return@safely
        if (pending?.uuid == WatchBleProtocol.ACTIVITY || operations.any { it.uuid == WatchBleProtocol.ACTIVITY }) return@safely
        operations.add(Operation(WatchBleProtocol.ACTIVITY))
        if (WatchBleProtocol.STEP_GOAL in characteristics && pending?.uuid != WatchBleProtocol.STEP_GOAL && operations.none { it.uuid == WatchBleProtocol.STEP_GOAL })
            operations.add(Operation(WatchBleProtocol.STEP_GOAL))
        if (pending == null) nextOperation()
    }
    fun setScreenTimeout(value: Long) = writeControl(WatchBleProtocol.TIMEOUT) { WatchBleProtocol.encodeTimeout(value) }
    fun setWifiEnabled(value: Boolean) = writeControl(WatchBleProtocol.WIFI_ENABLED) { WatchBleProtocol.encodeWifiEnabled(value) }

    private fun writeControl(uuid: UUID, encode: () -> ByteArray) = safely {
        if (!permitted()) return@safely
        if (!state.value.secureLink || state.value.connectionState != ConnectionState.CONNECTED || !state.value.controlsAvailable || state.value.writingControl || pending != null) return@safely
        val bytes = try { encode() } catch (_: IllegalArgumentException) {
            mutableState.value = state.value.copy(controlError = "Unsupported setting value."); return@safely
        }
        mutableState.value = state.value.copy(writingControl = true, controlError = null)
        Log.i(TAG, "Sending control: $uuid")
        operations.add(Operation(uuid, writeValue = bytes))
        nextOperation()
    }

    private fun unexpectedDisconnect(message: String) {
        Log.w(TAG, message)
        cancelReconnect(); stopScan(); releaseGatt()
        mutableState.value = freshState(ConnectionState.RECONNECTING, message).copy(disconnectReason = DisconnectReason.UNEXPECTED_DISCONNECT)
        scheduleReconnect()
    }
    private fun scheduleReconnect() {
        if (!serviceEnabled || !reconnectAllowed || reconnectTask != null || gatt != null) return
        val device = lastDevice ?: return
        if (!permitted()) return
        if (adapter?.isEnabled != true) {
            mutableState.value = state.value.copy(connectionState = ConnectionState.RECONNECTING, retryDelayMillis = null,
                message = "Bluetooth is off. Waiting for it to be enabled.")
            return
        }
        if (!ReconnectPolicy.mayRecover(serviceEnabled, state.value.disconnectReason, device.bondState == BluetoothDevice.BOND_BONDED)) {
            securityFailure("Bond missing or removed. Automatic recovery stopped; pair explicitly from the app."); return
        }
        val delay = ReconnectPolicy.delayMillis(retryCount)
        retryCount = (retryCount + 1).coerceAtMost(6)
        Log.i(TAG, "Unexpected disconnect; retry in ${delay}ms (no recovery scan)")
        mutableState.value = state.value.copy(connectionState = ConnectionState.RECONNECTING, retryDelayMillis = delay,
            message = "Reconnecting to the bonded watch in ${delay / 1000}s…")
        val task = Runnable {
            reconnectTask = null
            if (serviceEnabled && reconnectAllowed && gatt == null) safely { connectDevice(device, recovery = true) }
        }
        reconnectTask = task
        handler.postDelayed(task, delay)
    }

    private fun fail(message: String) {
        Log.w(TAG, message)
        reconnectAllowed = false
        cancelReconnect()
        stopScan()
        releaseGatt()
        mutableState.value = freshState(ConnectionState.ERROR, message)
    }

    private fun releaseGatt() {
        servicesInFlight = false; servicesConfigured = false
        clearAi()
        waitingForMtu = false; negotiatedMtu = 23
        handler.removeCallbacks(voicePoll); clearVoice()
        if (bondReceiverRegistered) { context.unregisterReceiver(bondReceiver); bondReceiverRegistered = false }
        transportConnected = false
        discoveryStarted = false
        automaticConnection = false
        clearTimeout()
        discoveryDelay?.let(handler::removeCallbacks); discoveryDelay = null
        databaseSubscriptionDone = false
        val old = gatt
        gatt = null
        pending = null
        operations.clear()
        phoneNotifications.clear()
        sentMedia = null; mediaInFlight = null; mediaSequence = null
        mediaVolumeSupported = false
        characteristics.clear()
        try { old?.disconnect() } catch (_: SecurityException) { Log.w(TAG, "Permission revoked during disconnect") }
        try { old?.close() } catch (_: SecurityException) { Log.w(TAG, "Permission revoked during close") }
    }
    private fun cancelReconnect() { reconnectTask?.let(handler::removeCallbacks); reconnectTask = null }
    private fun clearTimeout() { timeout?.let(handler::removeCallbacks); timeout = null }
    private fun armTimeout(delay: Long, action: () -> Unit) {
        clearTimeout()
        timeout = Runnable { timeout = null; action() }.also { handler.postDelayed(it, delay) }
    }
    companion object { private const val TAG = "WatchBle" }
}
