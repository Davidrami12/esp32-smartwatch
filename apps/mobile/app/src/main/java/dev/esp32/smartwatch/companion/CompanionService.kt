package dev.esp32.smartwatch.companion

import android.app.*
import android.content.Intent
import android.content.pm.ServiceInfo
import android.os.IBinder
import android.util.Log
import dev.esp32.smartwatch.*
import kotlinx.coroutines.*
import kotlinx.coroutines.flow.distinctUntilChanged
import kotlinx.coroutines.flow.map
import dev.esp32.smartwatch.media.MediaSessionBridge

/** Started foreground service owns every BLE operation; never bound to Activity lifetime. */
class CompanionService : Service() {
    private val api get() = (application as SmartwatchApplication).companion
    private val repository get() = api.repository
    private val scope = CoroutineScope(SupervisorJob() + Dispatchers.Main.immediate)
    private var promoted = false
    private var media: MediaSessionBridge? = null

    override fun onCreate() {
        super.onCreate()
        val manager = getSystemService(NotificationManager::class.java)
        manager.createNotificationChannel(NotificationChannel(CHANNEL, "Smartwatch connection", NotificationManager.IMPORTANCE_LOW).apply {
            description = "Ongoing secure watch connection and reconnection status"
            setShowBadge(false)
        })
        try {
            startForeground(NOTIFICATION_ID, notification("Starting companion…"), ServiceInfo.FOREGROUND_SERVICE_TYPE_CONNECTED_DEVICE)
            promoted = true
            api.service = this
            repository.serviceStarted()
            val health = (application as SmartwatchApplication).health
            repository.setActivitySnapshotHandler(health::onSnapshotReceived)
            scope.launch { repository.state.collect { health.onWatchState(it) } }
            scope.launch {
                // Refresh on secure connect and each minute, including midnight with zero new steps.
                repository.state.map { it.connectionState == ConnectionState.CONNECTED && it.secureLink }
                    .distinctUntilChanged().collect { if (it) repository.requestActivitySnapshot() }
            }
            scope.launch {
                while (isActive) { delay(60_000); repository.requestActivitySnapshot() }
            }
            media = MediaSessionBridge(this, repository::updateMedia)
            repository.setMediaCommandHandler { media?.execute(it) }
            scope.launch {
                repository.state.map { it.notificationAccess }.distinctUntilChanged().collect { media?.refreshAccess() }
            }
            scope.launch {
                repository.state.map { notificationText(it) }.distinctUntilChanged().collect { text ->
                    manager.notify(NOTIFICATION_ID, notification(text))
                }
            }
            Log.i(TAG, "connectedDevice foreground service started")
        } catch (_: SecurityException) {
            repository.serviceStartFailed("Foreground companion needs Nearby devices permission. Grant it and retry.")
            stopSelf()
        } catch (_: IllegalStateException) {
            repository.serviceStartFailed("Android rejected the foreground service start. Open the app and retry.")
            stopSelf()
        }
    }
    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        if (!promoted || !api.requested) { stopSelf(); return START_NOT_STICKY }
        when (intent?.action) {
            SCAN -> repository.startScan()
            STOP_SCAN -> repository.stopScan()
            CONNECT -> intent.getStringExtra(DEVICE)?.let(repository::connect)
            RESUME -> repository.resumeBonded()
            BRIGHTNESS -> repository.setBrightness(intent.getLongExtra(VALUE, 0).toInt())
            TIMEOUT -> repository.setScreenTimeout(intent.getLongExtra(VALUE, 0))
            WIFI -> repository.setWifiEnabled(intent.getLongExtra(VALUE, 0) != 0L)
        }
        // Process kill / force stop / reboot require a new visible user action; no silent resurrection.
        return START_NOT_STICKY
    }
    override fun onBind(intent: Intent?): IBinder? = null
    override fun onTaskRemoved(rootIntent: Intent?) {
        Log.i(TAG, "Activity task removed; foreground companion intentionally remains active")
        super.onTaskRemoved(rootIntent)
    }
    override fun onDestroy() {
        scope.cancel()
        repository.setActivitySnapshotHandler(null)
        (application as SmartwatchApplication).health.serviceStopped()
        repository.setMediaCommandHandler(null)
        media?.stop(); media = null
        repository.serviceStopped(user = !api.requested)
        api.stopped()
        if (api.service === this) api.service = null
        stopForeground(STOP_FOREGROUND_REMOVE)
        Log.i(TAG, "Foreground companion stopped; GATT and recovery released")
        super.onDestroy()
    }
    internal fun stopByUser() {
        repository.setActivitySnapshotHandler(null)
        (application as SmartwatchApplication).health.serviceStopped()
        repository.setMediaCommandHandler(null)
        media?.stop()
        repository.serviceStopped(user = true) // Cancel pending retry/GATT immediately, before asynchronous destruction.
        stopForeground(STOP_FOREGROUND_REMOVE)
        stopSelf()
    }
    private fun notification(text: String): Notification {
        val open = PendingIntent.getActivity(this, 0, Intent(this, MainActivity::class.java)
            .addFlags(Intent.FLAG_ACTIVITY_SINGLE_TOP or Intent.FLAG_ACTIVITY_CLEAR_TOP), PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE)
        return Notification.Builder(this, CHANNEL).setSmallIcon(R.drawable.ic_companion)
            .setContentTitle("Smartwatch").setContentText(text).setContentIntent(open)
            .setOngoing(true).setOnlyAlertOnce(true).setCategory(Notification.CATEGORY_SERVICE)
            .setVisibility(Notification.VISIBILITY_PUBLIC).build()
    }
    companion object {
        const val CHANNEL = "watch_connection"
        const val NOTIFICATION_ID = 1001
        const val SCAN = "companion.SCAN"
        const val STOP_SCAN = "companion.STOP_SCAN"
        const val CONNECT = "companion.CONNECT"
        const val RESUME = "companion.RESUME"
        const val BRIGHTNESS = "companion.BRIGHTNESS"
        const val TIMEOUT = "companion.TIMEOUT"
        const val WIFI = "companion.WIFI"
        const val DEVICE = "device"
        const val VALUE = "value"
        private const val TAG = "WatchCompanion"
        fun notificationText(state: WatchState): String = when (state.connectionState) {
            ConnectionState.CONNECTED -> "Connected"
            ConnectionState.RECONNECTING -> "Reconnecting…"
            ConnectionState.CONNECTING, ConnectionState.DISCOVERING_SERVICES -> "Connecting securely…"
            ConnectionState.PAIRING -> "Confirm matching codes on both devices"
            ConnectionState.SCANNING -> "Scanning for watch…"
            ConnectionState.ERROR -> "Attention required · open app"
            ConnectionState.DISCONNECTED -> "Choose a watch in the app"
        }
    }
}
