package dev.esp32.smartwatch.ble

import java.util.UUID

// Source of truth: docs/ble-protocol.md. Values are snapshots, never deltas.
object WatchBleProtocol {
    const val SUPPORTED_VERSION = 1
    val WATCH_SERVICE: UUID = UUID.fromString("7e570001-8e7a-4b6d-9f2a-6c4d3e2f1000")
    val BATTERY_SERVICE: UUID = UUID.fromString("0000180f-0000-1000-8000-00805f9b34fb")
    val GATT_SERVICE: UUID = UUID.fromString("00001801-0000-1000-8000-00805f9b34fb")
    val SERVICE_CHANGED: UUID = UUID.fromString("00002a05-0000-1000-8000-00805f9b34fb")
    val VERSION: UUID = UUID.fromString("7e570002-8e7a-4b6d-9f2a-6c4d3e2f1000")
    val STEPS: UUID = UUID.fromString("7e570003-8e7a-4b6d-9f2a-6c4d3e2f1000")
    val WIFI: UUID = UUID.fromString("7e570004-8e7a-4b6d-9f2a-6c4d3e2f1000")
    val BRIGHTNESS: UUID = UUID.fromString("7e570005-8e7a-4b6d-9f2a-6c4d3e2f1000")
    val TIMEOUT: UUID = UUID.fromString("7e570006-8e7a-4b6d-9f2a-6c4d3e2f1000")
    val WIFI_ENABLED: UUID = UUID.fromString("7e570007-8e7a-4b6d-9f2a-6c4d3e2f1000")
    val PHONE_NOTIFICATION: UUID = UUID.fromString("7e570008-8e7a-4b6d-9f2a-6c4d3e2f1000")
    val SECURITY: UUID = UUID.fromString("7e570009-8e7a-4b6d-9f2a-6c4d3e2f1000")
    val MEDIA: UUID = UUID.fromString("7e57000a-8e7a-4b6d-9f2a-6c4d3e2f1000")
    val MEDIA_COMMAND: UUID = UUID.fromString("7e57000b-8e7a-4b6d-9f2a-6c4d3e2f1000")
    val MEDIA_CAPABILITIES: UUID = UUID.fromString("7e57000c-8e7a-4b6d-9f2a-6c4d3e2f1000")
    val ACTIVITY: UUID = UUID.fromString("7e57000d-8e7a-4b6d-9f2a-6c4d3e2f1000")
    val STEP_GOAL: UUID = UUID.fromString("7e57000e-8e7a-4b6d-9f2a-6c4d3e2f1000")
    fun stepGoal(value: ByteArray): Int {
        require(value.size == 4)
        return unsigned(value).also { require(it in 1000L..30000L && it % 1000L == 0L) }.toInt()
    }
    fun secureStatus(value: ByteArray): Boolean = value.contentEquals(byteArrayOf(1, 15, 16))
    val BATTERY: UUID = UUID.fromString("00002a19-0000-1000-8000-00805f9b34fb")
    val CCCD: UUID = UUID.fromString("00002902-0000-1000-8000-00805f9b34fb")
    val TELEMETRY = listOf(BATTERY, STEPS, WIFI)
    val CONTROLS = listOf(BRIGHTNESS, TIMEOUT, WIFI_ENABLED)
    val NOTIFY = TELEMETRY + CONTROLS
    val TIMEOUT_VALUES = listOf(5000L, 10000L, 30000L, 60000L, 0L)
    fun brightness(value: ByteArray): Int {
        require(value.size == 1)
        return unsigned(value).toInt().also { require(it in 10..100) }
    }
    fun timeout(value: ByteArray): Long {
        require(value.size == 4)
        return unsigned(value).also { require(it in TIMEOUT_VALUES) }
    }
    fun wifiEnabled(value: ByteArray): Boolean {
        require(value.size == 1 && unsigned(value) <= 1)
        return value[0].toInt() == 1
    }
    fun encodeBrightness(value: Int): ByteArray { require(value in 10..100); return byteArrayOf(value.toByte()) }
    fun encodeTimeout(value: Long): ByteArray {
        require(value in TIMEOUT_VALUES)
        return ByteArray(4) { (value shr (8 * it)).toByte() }
    }
    fun encodeWifiEnabled(value: Boolean): ByteArray = byteArrayOf(if (value) 1 else 0)
    fun version(value: ByteArray): Int { require(value.size == 2); return unsigned(value).toInt() }
    fun battery(value: ByteArray): Int {
        require(value.size == 1)
        return unsigned(value).toInt().also { require(it <= 100) }
    }
    fun steps(value: ByteArray): Long { require(value.size == 4); return unsigned(value) }
    fun wifi(value: ByteArray): WifiState {
        require(value.size == 1)
        return when (unsigned(value).toInt()) {
            0 -> WifiState.DISCONNECTED
            1 -> WifiState.CONNECTING
            2 -> WifiState.CONNECTED
            3 -> WifiState.DISABLED
            else -> WifiState.UNKNOWN
        }
    }
    private fun unsigned(value: ByteArray): Long = value.foldIndexed(0L) { i, result, byte ->
        result or ((byte.toLong() and 0xff) shl (8 * i))
    }
}

enum class WifiState { DISCONNECTED, CONNECTING, CONNECTED, DISABLED, UNKNOWN }
