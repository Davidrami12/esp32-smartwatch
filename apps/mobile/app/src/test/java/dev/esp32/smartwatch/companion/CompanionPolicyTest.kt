package dev.esp32.smartwatch.companion

import dev.esp32.smartwatch.*
import org.junit.Assert.*
import org.junit.Test

class CompanionPolicyTest {
    @Test fun backoffIsProgressiveAndCapped() {
        assertEquals(listOf(0L, 5_000L, 15_000L, 30_000L, 60_000L, 120_000L), (0..5).map(ReconnectPolicy::delayMillis))
        for (attempt in listOf(6, 100, Int.MAX_VALUE)) assertEquals(120_000L, ReconnectPolicy.delayMillis(attempt))
        assertEquals(0L, ReconnectPolicy.delayMillis(-1))
    }
    @Test fun manualIntentAndSecurityNeverRecover() {
        assertTrue(ReconnectPolicy.mayRecover(true, DisconnectReason.UNEXPECTED_DISCONNECT, true))
        for (reason in listOf(DisconnectReason.USER_DISCONNECTED, DisconnectReason.SERVICE_STOPPED, DisconnectReason.SECURITY_FAILURE, DisconnectReason.NONE)) {
            assertFalse(ReconnectPolicy.mayRecover(true, reason, true))
        }
        assertFalse(ReconnectPolicy.mayRecover(false, DisconnectReason.UNEXPECTED_DISCONNECT, true))
        assertFalse(ReconnectPolicy.mayRecover(true, DisconnectReason.UNEXPECTED_DISCONNECT, false))
    }
    @Test fun minimalForegroundStatusDoesNotIncludePrivateTelemetry() {
        assertEquals("Connected", CompanionService.notificationText(WatchState(connectionState = ConnectionState.CONNECTED, battery = 42, steps = 1234)))
        assertEquals("Reconnecting…", CompanionService.notificationText(WatchState(connectionState = ConnectionState.RECONNECTING)))
        assertEquals("Attention required · open app", CompanionService.notificationText(WatchState(connectionState = ConnectionState.ERROR, message = "Private details")))
    }
}
