package dev.esp32.smartwatch.ble

import org.junit.Assert.*
import org.junit.Test

class WatchSecurityTest {
    @Test fun rejectsMissingWeakOrMalformedSecurity() {
        assertTrue(WatchBleProtocol.secureStatus(byteArrayOf(1, 15, 16)))
        assertFalse(WatchBleProtocol.secureStatus(byteArrayOf()))
        assertFalse(WatchBleProtocol.secureStatus(byteArrayOf(1, 15)))
        assertFalse(WatchBleProtocol.secureStatus(byteArrayOf(1, 15, 16, 0)))
        assertFalse(WatchBleProtocol.secureStatus(byteArrayOf(2, 15, 16)))
        assertFalse(WatchBleProtocol.secureStatus(byteArrayOf(1, 15, 7)))
        for (bit in listOf(1, 2, 4, 8)) assertFalse(WatchBleProtocol.secureStatus(byteArrayOf(1, (15 xor bit).toByte(), 16)))
    }
}
