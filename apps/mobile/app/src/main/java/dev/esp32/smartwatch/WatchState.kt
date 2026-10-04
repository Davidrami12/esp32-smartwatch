package dev.esp32.smartwatch

import dev.esp32.smartwatch.ble.WifiState

enum class ConnectionState { DISCONNECTED, SCANNING, CONNECTING, DISCOVERING_SERVICES, CONNECTED, ERROR }
data class NearbyWatch(val id: String, val name: String, val rssi: Int)
data class WatchState(
    val connectionState: ConnectionState = ConnectionState.DISCONNECTED,
    val devices: List<NearbyWatch> = emptyList(),
    val battery: Int? = null,
    val steps: Long? = null,
    val wifiState: WifiState? = null,
    val protocolVersion: Int? = null,
    val brightness: Int? = null,
    val screenTimeout: Long? = null,
    val wifiEnabled: Boolean? = null,
    val controlsAvailable: Boolean = false,
    val writingControl: Boolean = false,
    val controlError: String? = null,
    val message: String? = null,
)
