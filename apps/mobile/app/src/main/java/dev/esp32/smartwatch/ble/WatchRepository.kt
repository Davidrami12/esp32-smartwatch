package dev.esp32.smartwatch.ble

import android.Manifest
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

/** All mutable BLE/session state is confined to the main looper, including callbacks. */
@SuppressLint("MissingPermission") // Every entry point/callback checks permissions; revocation is caught.
class WatchRepository(context: Context) {
    private val context = context.applicationContext
    private val adapter = this.context.getSystemService(BluetoothManager::class.java)?.adapter
    private val handler = Handler(Looper.getMainLooper())
    private val mutableState = MutableStateFlow(WatchState())
    val state = mutableState.asStateFlow()
    private val devices = mutableMapOf<String, BluetoothDevice>()
    private var scanner: BluetoothLeScanner? = null
    private var scanCallback: ScanCallback? = null
    private var gatt: BluetoothGatt? = null
    private var lastDevice: BluetoothDevice? = null
    private var foreground = false
    private var reconnectAllowed = false
    private var retryCount = 0
    private var reconnectTask: Runnable? = null
    private var timeout: Runnable? = null
    private var databaseSubscriptionDone = false
    private var discoveryDelay: Runnable? = null
    private val characteristics = mutableMapOf<UUID, BluetoothGattCharacteristic>()
    private data class Operation(val uuid: UUID, val subscribe: Boolean = false, val writeValue: ByteArray? = null, val readback: Boolean = false)
    private val operations = ArrayDeque<Operation>()
    private var pending: Operation? = null

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

    fun setForeground(value: Boolean) {
        foreground = value
        if (value && (gatt != null || scanCallback != null) && !permitted()) return
        if (!value) {
            stopScan()
            cancelReconnect()
        } else if (reconnectAllowed && gatt == null && lastDevice != null) scheduleReconnect()
    }

    fun startScan() = safely {
        if (!permitted()) return@safely
        if (adapter?.isEnabled != true) { fail("Enable Bluetooth, then scan again."); return@safely }
        if (gatt != null) return@safely
        stopScan()
        cancelReconnect()
        reconnectAllowed = false
        devices.clear()
        mutableState.value = WatchState(connectionState = ConnectionState.SCANNING)
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
        if (!permitted()) return@safely
        val device = devices[id] ?: return@safely
        stopScan()
        cancelReconnect()
        releaseGatt()
        retryCount = 0
        reconnectAllowed = true
        lastDevice = device
        connectDevice(device)
    }

    private fun connectDevice(device: BluetoothDevice) {
        if (!permitted()) return
        if (adapter?.isEnabled != true) { fail("Enable Bluetooth and reconnect."); return }
        mutableState.value = WatchState(connectionState = ConnectionState.CONNECTING, devices = state.value.devices)
        Log.i(TAG, "Connecting GATT")
        gatt = device.connectGatt(context, false, callback, BluetoothDevice.TRANSPORT_LE)
        if (gatt == null) { fail("Could not start GATT connection."); return }
        armTimeout(20_000) { fail("Connection timed out. Scan and retry.") }
    }

    private val callback = object : BluetoothGattCallback() {
        override fun onConnectionStateChange(source: BluetoothGatt, status: Int, newState: Int) = onSession(source) {
            Log.i(TAG, "GATT connection callback: status=$status state=$newState")
            if (status != BluetoothGatt.GATT_SUCCESS || newState == BluetoothProfile.STATE_DISCONNECTED) {
                releaseGatt()
                mutableState.value = WatchState(connectionState = ConnectionState.DISCONNECTED, devices = state.value.devices,
                    message = "Watch disconnected (status $status).")
                scheduleReconnect()
            } else if (newState == BluetoothProfile.STATE_CONNECTED) {
                mutableState.value = state.value.copy(connectionState = ConnectionState.DISCOVERING_SERVICES)
                Log.i(TAG, "Discovering services")
                if (!source.discoverServices()) fail("Service discovery could not start.")
                else armTimeout(15_000) { fail("Service discovery timed out.") }
            }
        }

        override fun onServicesDiscovered(source: BluetoothGatt, status: Int) = onSession(source) {
            if (status != BluetoothGatt.GATT_SUCCESS) { fail("Service discovery failed ($status)."); return@onSession }
            Log.i(TAG, "Services discovered; verifying protocol characteristics")
            val watch = source.getService(WatchBleProtocol.WATCH_SERVICE)
            val battery = source.getService(WatchBleProtocol.BATTERY_SERVICE)
            for (uuid in listOf(WatchBleProtocol.VERSION) + WatchBleProtocol.TELEMETRY) {
                val c = (if (uuid == WatchBleProtocol.BATTERY) battery else watch)?.getCharacteristic(uuid)
                if (c == null || c.properties and BluetoothGattCharacteristic.PROPERTY_READ == 0 ||
                    (uuid in WatchBleProtocol.NOTIFY && (c.properties and BluetoothGattCharacteristic.PROPERTY_NOTIFY == 0 || c.getDescriptor(WatchBleProtocol.CCCD) == null))) {
                    fail("Incompatible watch: required services, reads or notifications missing."); return@onSession
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
            // One outstanding GATT operation. Version must succeed before custom values are decoded.
            operations.add(Operation(WatchBleProtocol.VERSION))
            WatchBleProtocol.NOTIFY.filter { it in characteristics }.forEach { operations.add(Operation(it)) }
            WatchBleProtocol.NOTIFY.filter { it in characteristics }.forEach { operations.add(Operation(it, subscribe = true)) }
            val changed = source.getService(WatchBleProtocol.GATT_SERVICE)?.getCharacteristic(WatchBleProtocol.SERVICE_CHANGED)
            if (!databaseSubscriptionDone && changed != null && changed.properties and BluetoothGattCharacteristic.PROPERTY_INDICATE != 0 &&
                changed.getDescriptor(WatchBleProtocol.CCCD) != null) {
                characteristics[changed.uuid] = changed
                operations.addFirst(Operation(changed.uuid, subscribe = true))
            }
            nextOperation()
        }

        override fun onCharacteristicRead(source: BluetoothGatt, c: BluetoothGattCharacteristic, value: ByteArray, status: Int) {
            val snapshot = value.copyOf()
            onSession(source) {
                val op = pending ?: return@onSession
                if (op.uuid != c.uuid || op.subscribe || op.writeValue != null) return@onSession
                Log.d(TAG, "Characteristic read: ${c.uuid}, status=$status")
                if (op.readback) {
                    val valid = status == BluetoothGatt.GATT_SUCCESS && decode(c.uuid, snapshot, strict = false)
                    mutableState.value = state.value.copy(writingControl = false,
                        controlsAvailable = state.value.controlsAvailable && valid,
                        controlError = if (valid) state.value.controlError else "Could not confirm settings. Reconnect before changing them again.")
                    pending = null
                    nextOperation()
                    return@onSession
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
            Log.i(TAG, "Control write callback: ${c.uuid}, status=$status")
            if (status != BluetoothGatt.GATT_SUCCESS) mutableState.value = state.value.copy(
                controlError = "Watch rejected the change ($status). Reading its actual value.")
            pending = null
            operations.addFirst(Operation(c.uuid, readback = true))
            nextOperation()
        }

        override fun onDescriptorWrite(source: BluetoothGatt, descriptor: BluetoothGattDescriptor, status: Int) = onSession(source) {
            if (descriptor.uuid != WatchBleProtocol.CCCD || pending != Operation(descriptor.characteristic.uuid, true)) return@onSession
            Log.d(TAG, "Notification subscription: ${descriptor.characteristic.uuid}, status=$status")
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
            operations.clear(); pending = null; characteristics.clear()
            databaseSubscriptionDone = true
            mutableState.value = WatchState(connectionState = ConnectionState.DISCOVERING_SERVICES, devices = state.value.devices)
            if (!source.discoverServices()) fail("Changed-service discovery could not start. Reconnect.")
            else armTimeout(15_000) { fail("Changed-service discovery timed out.") }
        }

        override fun onCharacteristicChanged(source: BluetoothGatt, c: BluetoothGattCharacteristic, value: ByteArray) {
            val snapshot = value.copyOf()
            onSession(source) {
                if (state.value.protocolVersion == WatchBleProtocol.SUPPORTED_VERSION && c.uuid in WatchBleProtocol.NOTIFY && c.uuid in characteristics) {
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
        clearTimeout()
        val source = gatt ?: return
        val op = operations.removeFirstOrNull()
        if (op == null) {
            if (state.value.connectionState != ConnectionState.CONNECTED) {
                retryCount = 0
                Log.i(TAG, "Watch ready: initial reads and all subscriptions complete")
                mutableState.value = state.value.copy(connectionState = ConnectionState.CONNECTED)
            }
            return
        }
        pending = op
        val c = characteristics.getValue(op.uuid)
        val started = if (op.writeValue != null) {
            source.writeCharacteristic(c, op.writeValue, BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT) == BluetoothStatusCodes.SUCCESS
        } else if (op.subscribe) {
            source.setCharacteristicNotification(c, true) && source.writeDescriptor(c.getDescriptor(WatchBleProtocol.CCCD),
                if (c.uuid == WatchBleProtocol.SERVICE_CHANGED) BluetoothGattDescriptor.ENABLE_INDICATION_VALUE
                else BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE) == BluetoothStatusCodes.SUCCESS
        } else source.readCharacteristic(c)
        if (!started && op.writeValue != null) {
            pending = null
            mutableState.value = state.value.copy(writingControl = false, controlError = "Could not send the change. Previous confirmed value retained; retry.")
        } else if (!started && op.readback) {
            pending = null
            mutableState.value = state.value.copy(writingControl = false, controlsAvailable = false,
                controlError = "Readback could not start. Reconnect to confirm settings.")
        } else if (!started) fail("GATT operation could not start. Reconnect and retry.")
        else armTimeout(10_000) { fail(if (state.value.writingControl)
            "Settings confirmation timed out. Outcome unknown; reconnect to read actual values."
            else "GATT operation timed out. Reconnect and retry.") }
    }

    private fun decode(uuid: UUID, value: ByteArray, strict: Boolean): Boolean {
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
    fun setScreenTimeout(value: Long) = writeControl(WatchBleProtocol.TIMEOUT) { WatchBleProtocol.encodeTimeout(value) }
    fun setWifiEnabled(value: Boolean) = writeControl(WatchBleProtocol.WIFI_ENABLED) { WatchBleProtocol.encodeWifiEnabled(value) }

    private fun writeControl(uuid: UUID, encode: () -> ByteArray) = safely {
        if (!permitted()) return@safely
        if (state.value.connectionState != ConnectionState.CONNECTED || !state.value.controlsAvailable || state.value.writingControl || pending != null) return@safely
        val bytes = try { encode() } catch (_: IllegalArgumentException) {
            mutableState.value = state.value.copy(controlError = "Unsupported setting value."); return@safely
        }
        mutableState.value = state.value.copy(writingControl = true, controlError = null)
        Log.i(TAG, "Sending control: $uuid")
        operations.add(Operation(uuid, writeValue = bytes))
        nextOperation()
    }

    private fun scheduleReconnect() {
        if (!foreground || !reconnectAllowed || reconnectTask != null || retryCount >= 3) return
        val device = lastDevice ?: return
        retryCount++
        Log.i(TAG, "Scheduling foreground reconnect attempt $retryCount/3")
        mutableState.value = state.value.copy(message = "Watch disconnected. Reconnecting ($retryCount/3)…")
        val task = Runnable {
            reconnectTask = null
            if (foreground && reconnectAllowed && gatt == null) safely { connectDevice(device) }
        }
        reconnectTask = task
        handler.postDelayed(task, 2_000L * retryCount)
    }

    fun disconnect() {
        Log.i(TAG, "Manual disconnect")
        reconnectAllowed = false
        lastDevice = null
        cancelReconnect()
        stopScan()
        releaseGatt()
        mutableState.value = WatchState(devices = state.value.devices)
    }

    private fun fail(message: String) {
        Log.w(TAG, message)
        reconnectAllowed = false
        cancelReconnect()
        stopScan()
        releaseGatt()
        mutableState.value = WatchState(connectionState = ConnectionState.ERROR, devices = state.value.devices, message = message)
    }

    private fun releaseGatt() {
        clearTimeout()
        discoveryDelay?.let(handler::removeCallbacks); discoveryDelay = null
        databaseSubscriptionDone = false
        val old = gatt
        gatt = null
        pending = null
        operations.clear()
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
    fun close() { disconnect(); handler.removeCallbacksAndMessages(null) }
    companion object { private const val TAG = "WatchBle" }
}
