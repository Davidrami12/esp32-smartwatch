package dev.esp32.smartwatch

import dev.esp32.smartwatch.ble.WifiState

enum class ConnectionState { DISCONNECTED, SCANNING, CONNECTING, PAIRING, DISCOVERING_SERVICES, CONNECTED, RECONNECTING, ERROR }
enum class DisconnectReason { NONE, USER_DISCONNECTED, UNEXPECTED_DISCONNECT, SERVICE_STOPPED, SECURITY_FAILURE }
enum class PairingState { NOT_BONDED, PAIRING, BONDED, VERIFYING, SECURE, REJECTED_OR_FAILED }
enum class NotificationAccess { NOT_GRANTED, GRANTED }
data class NearbyWatch(val id: String, val name: String, val rssi: Int)
data class WatchState(
    val connectionState: ConnectionState = ConnectionState.DISCONNECTED,
    val companionEnabled: Boolean = false,
    val savedWatchAvailable: Boolean = false,
    val disconnectReason: DisconnectReason = DisconnectReason.NONE,
    val retryDelayMillis: Long? = null,
    val pairingState: PairingState = PairingState.NOT_BONDED,
    val secureLink: Boolean = false,
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
    val notificationAccess: NotificationAccess = NotificationAccess.NOT_GRANTED,
    val notificationTransportAvailable: Boolean = false,
    val sendingNotification: Boolean = false,
    val notificationError: String? = null,
    val mediaAvailable: Boolean = false,
    val sendingMedia: Boolean = false,
    val mediaError: String? = null,
    val message: String? = null,
)
