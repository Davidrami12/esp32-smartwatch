package dev.esp32.smartwatch.ble

import org.junit.Assert.*
import org.junit.Test

class WatchBleProtocolTest {
    @Test fun documentedSnapshots() {
        assertEquals(1, WatchBleProtocol.version(byteArrayOf(1, 0)))
        assertEquals(65535, WatchBleProtocol.version(byteArrayOf(-1, -1)))
        assertEquals(0, WatchBleProtocol.battery(byteArrayOf(0)))
        assertEquals(100, WatchBleProtocol.battery(byteArrayOf(100)))
        assertEquals(77, WatchBleProtocol.battery(byteArrayOf(0x4d)))
        assertEquals(1234L, WatchBleProtocol.steps(byteArrayOf(0xd2.toByte(), 4, 0, 0)))
        assertEquals(4294967295L, WatchBleProtocol.steps(ByteArray(4) { 0xff.toByte() }))
        assertEquals(0L, WatchBleProtocol.steps(ByteArray(4)))
        assertTrue(WatchBleProtocol.steps(ByteArray(4)) < WatchBleProtocol.steps(byteArrayOf(5, 0, 0, 0)))
        assertEquals(WifiState.DISCONNECTED, WatchBleProtocol.wifi(byteArrayOf(0)))
        assertEquals(WifiState.CONNECTING, WatchBleProtocol.wifi(byteArrayOf(1)))
        assertEquals(WifiState.CONNECTED, WatchBleProtocol.wifi(byteArrayOf(2)))
        assertEquals(WifiState.DISABLED, WatchBleProtocol.wifi(byteArrayOf(3)))
        assertEquals(WifiState.UNKNOWN, WatchBleProtocol.wifi(byteArrayOf(99)))
    }
    @Test fun rejectMalformedValues() {
        for (size in listOf(0, 1, 3, 5)) assertThrows(IllegalArgumentException::class.java) { WatchBleProtocol.steps(ByteArray(size)) }
        assertThrows(IllegalArgumentException::class.java) { WatchBleProtocol.version(byteArrayOf(1)) }
        assertThrows(IllegalArgumentException::class.java) { WatchBleProtocol.battery(byteArrayOf(101)) }
        assertThrows(IllegalArgumentException::class.java) { WatchBleProtocol.battery(byteArrayOf(-1)) }
        assertThrows(IllegalArgumentException::class.java) { WatchBleProtocol.wifi(byteArrayOf(1, 2)) }
    }
    @Test fun controlRoundTripsAndRejection() {
        for (value in 10..100) assertEquals(value, WatchBleProtocol.brightness(WatchBleProtocol.encodeBrightness(value)))
        for (value in WatchBleProtocol.TIMEOUT_VALUES) assertEquals(value, WatchBleProtocol.timeout(WatchBleProtocol.encodeTimeout(value)))
        assertArrayEquals(byteArrayOf(0x30, 0x75, 0, 0), WatchBleProtocol.encodeTimeout(30000))
        for (value in listOf(false, true)) assertEquals(value, WatchBleProtocol.wifiEnabled(WatchBleProtocol.encodeWifiEnabled(value)))
        for (value in listOf(-1, 0, 9, 101, 255)) assertThrows(IllegalArgumentException::class.java) { WatchBleProtocol.encodeBrightness(value) }
        for (value in listOf(-1L, 1L, 9999L, 4294967295L)) assertThrows(IllegalArgumentException::class.java) { WatchBleProtocol.encodeTimeout(value) }
        for (value in listOf(0, 9, 101, 255)) assertThrows(IllegalArgumentException::class.java) { WatchBleProtocol.brightness(byteArrayOf(value.toByte())) }
        assertThrows(IllegalArgumentException::class.java) { WatchBleProtocol.timeout(byteArrayOf(-1, -1, -1, -1)) }
        for (value in listOf(2, 255)) assertThrows(IllegalArgumentException::class.java) { WatchBleProtocol.wifiEnabled(byteArrayOf(value.toByte())) }
        for (size in listOf(0, 2, 3, 5)) {
            assertThrows(IllegalArgumentException::class.java) { WatchBleProtocol.brightness(ByteArray(size)) }
            assertThrows(IllegalArgumentException::class.java) { WatchBleProtocol.wifiEnabled(ByteArray(size)) }
            assertThrows(IllegalArgumentException::class.java) { WatchBleProtocol.timeout(ByteArray(size)) }
        }
        assertThrows(IllegalArgumentException::class.java) { WatchBleProtocol.timeout(byteArrayOf(0)) }
    }
}
