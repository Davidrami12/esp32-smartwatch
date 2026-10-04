package dev.esp32.smartwatch

import android.app.Application
import androidx.lifecycle.AndroidViewModel
import dev.esp32.smartwatch.ble.WatchRepository

class WatchViewModel(application: Application) : AndroidViewModel(application) {
    private val repository = WatchRepository(application)
    val state = repository.state
    fun hasPermissions() = repository.hasPermissions()
    fun permissionResult(granted: Boolean) = repository.permissionResult(granted)
    fun scan() = repository.startScan()
    fun stopScan() = repository.stopScan()
    fun connect(id: String) = repository.connect(id)
    fun disconnect() = repository.disconnect()
    fun setBrightness(value: Int) = repository.setBrightness(value)
    fun setScreenTimeout(value: Long) = repository.setScreenTimeout(value)
    fun setWifiEnabled(value: Boolean) = repository.setWifiEnabled(value)
    fun setForeground(value: Boolean) = repository.setForeground(value)
    override fun onCleared() { repository.close() }
}
