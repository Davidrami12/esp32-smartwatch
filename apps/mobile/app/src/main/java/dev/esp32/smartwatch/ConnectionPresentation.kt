package dev.esp32.smartwatch

/** Presentation uses verified readiness, not merely an Android GATT connection. */
internal fun connectionTitle(state: WatchState): String = when (state.connectionState) {
    ConnectionState.CONNECTED -> if (state.secureLink) "Conectado · reloj listo" else "Bluetooth conectado · preparando reloj"
    ConnectionState.DISCOVERING_SERVICES -> "Bluetooth conectado · preparando reloj"
    ConnectionState.CONNECTING -> "Conectando"
    ConnectionState.RECONNECTING -> "Reconectando"
    ConnectionState.PAIRING -> "Conectando · confirma el código"
    ConnectionState.SCANNING -> "Buscando reloj"
    ConnectionState.DISCONNECTED, ConnectionState.ERROR -> "Sin conexión"
}
