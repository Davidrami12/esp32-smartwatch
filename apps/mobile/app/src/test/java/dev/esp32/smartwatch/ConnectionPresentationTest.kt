package dev.esp32.smartwatch

import org.junit.Assert.*
import org.junit.Test

class ConnectionPresentationTest {
    @Test fun connectedDoesNotMeanReadyUntilSecurityIsVerified() {
        assertEquals("Bluetooth conectado · preparando reloj",connectionTitle(WatchState(connectionState = ConnectionState.CONNECTED)))
        assertEquals("Conectado · reloj listo",connectionTitle(WatchState(connectionState = ConnectionState.CONNECTED,secureLink = true)))
        assertEquals("Bluetooth conectado · preparando reloj",connectionTitle(WatchState(connectionState = ConnectionState.DISCOVERING_SERVICES)))
    }
    @Test fun reconnectionAndErrorsAreExplicit() {
        assertEquals("Reconectando",connectionTitle(WatchState(connectionState = ConnectionState.RECONNECTING)))
        assertEquals("Sin conexión",connectionTitle(WatchState(connectionState = ConnectionState.ERROR)))
        assertEquals("Sin conexión",connectionTitle(WatchState()))
    }
}
