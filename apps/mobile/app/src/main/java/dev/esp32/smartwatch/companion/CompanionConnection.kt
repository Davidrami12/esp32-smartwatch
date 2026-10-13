package dev.esp32.smartwatch.companion

import android.content.Context
import android.content.Intent
import dev.esp32.smartwatch.ble.WatchRepository
import dev.esp32.smartwatch.notifications.PhoneNotification

/** Process-local service API. The single StateFlow is the repository's, not a copied UI state. */
class CompanionConnection(context: Context) {
    private val context = context.applicationContext
    internal val repository = WatchRepository(this.context)
    val state = repository.state
    private var visible = false
    internal var service: CompanionService? = null
    internal var requested = false
        private set

    fun hasPermissions() = repository.hasPermissions()
    fun permissionResult(granted: Boolean) = repository.permissionResult(granted)
    fun refreshNotificationAccess() = repository.refreshNotificationAccess()
    fun setVisible(value: Boolean) { visible = value; if (value) refreshNotificationAccess() }
    fun scan() = start(CompanionService.SCAN)
    fun resume() = start(CompanionService.RESUME)
    fun connect(id: String) = start(CompanionService.CONNECT, id)
    fun stopScan() = send(CompanionService.STOP_SCAN)
    fun setBrightness(value: Int) = send(CompanionService.BRIGHTNESS, value.toLong())
    fun setScreenTimeout(value: Long) = send(CompanionService.TIMEOUT, value)
    fun setWifiEnabled(value: Boolean) = send(CompanionService.WIFI, if (value) 1L else 0L)
    fun disconnect() {
        requested = false
        service?.let { it.stopByUser(); return }
        // stopService doesn't start a service from the background and onDestroy revokes all BLE work.
        if (!context.stopService(Intent(context, CompanionService::class.java))) repository.serviceStopped(user = true)
    }
    internal fun stopped() { requested = false }
    fun forwardNotification(item: PhoneNotification) {
        // Listener never starts/restarts a foreground service or obtains a GATT object.
        if (state.value.companionEnabled) repository.forwardNotification(item)
    }
    private fun start(action: String, device: String? = null) {
        if (!visible || !hasPermissions()) {
            repository.serviceStartFailed("Open the app and grant Nearby devices permission to enable the companion.")
            return
        }
        requested = true
        try {
            context.startForegroundService(Intent(context, CompanionService::class.java).setAction(action)
                .putExtra(CompanionService.DEVICE, device))
        } catch (_: IllegalStateException) {
            requested = false
            repository.serviceStartFailed("Android could not start the foreground companion. Keep the app visible and retry.")
        } catch (_: SecurityException) {
            requested = false
            repository.serviceStartFailed("Nearby devices permission is required for the companion service.")
        }
    }
    private fun send(action: String, value: Long = 0) {
        if (!visible || !requested || !state.value.companionEnabled) return
        try {
            context.startService(Intent(context, CompanionService::class.java).setAction(action).putExtra(CompanionService.VALUE, value))
        } catch (_: IllegalStateException) { repository.serviceStartFailed("Companion service unavailable; enable it again from the app.") }
        catch (_: SecurityException) { repository.serviceStartFailed("Bluetooth permission was revoked.") }
    }
}
