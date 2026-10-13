package dev.esp32.smartwatch

import android.app.Application
import androidx.lifecycle.AndroidViewModel

class WatchViewModel(application: Application) : AndroidViewModel(application) {
    private val repository = (application as SmartwatchApplication).companion
    val state = repository.state
    fun hasPermissions() = repository.hasPermissions()
    fun permissionResult(granted: Boolean) = repository.permissionResult(granted)
    fun scan() = repository.scan()
    fun stopScan() = repository.stopScan()
    fun connect(id: String) = repository.connect(id)
    fun disconnect() = repository.disconnect()
    fun setBrightness(value: Int) = repository.setBrightness(value)
    fun setScreenTimeout(value: Long) = repository.setScreenTimeout(value)
    fun setWifiEnabled(value: Boolean) = repository.setWifiEnabled(value)
    fun resumeCompanion() = repository.resume()
    fun setForeground(value: Boolean) = repository.setVisible(value)
    fun refreshNotificationAccess() = repository.refreshNotificationAccess()
    // Activity/ViewModel destruction never disconnects or releases the service-owned GATT.
}
