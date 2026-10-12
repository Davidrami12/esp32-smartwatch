package dev.esp32.smartwatch.companion

import dev.esp32.smartwatch.DisconnectReason

/** One bounded connection attempt at a time; no recovery scan or tight retry loop. */
object ReconnectPolicy {
    fun delayMillis(attempt: Int): Long = when (attempt.coerceAtLeast(0)) {
        0 -> 0L
        1 -> 5_000L
        2 -> 15_000L
        3 -> 30_000L
        4 -> 60_000L
        else -> 120_000L
    }
    fun mayRecover(serviceEnabled: Boolean, reason: DisconnectReason, bonded: Boolean): Boolean =
        serviceEnabled && reason == DisconnectReason.UNEXPECTED_DISCONNECT && bonded
}
