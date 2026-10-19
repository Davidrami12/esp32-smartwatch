package dev.esp32.smartwatch.ai

/** Revocable capability: a later opt-in never authorizes older queued recordings. */
internal class PaidVoiceConsent {
    data class Ticket internal constructor(val generation: Long)
    private var generation = 0L
    private var enabled = false
    @Synchronized fun set(value: Boolean) { generation++; enabled = value }
    @Synchronized fun ticket(): Ticket? = if (enabled) Ticket(generation) else null
    @Synchronized fun permits(ticket: Ticket?) = enabled && ticket != null && ticket.generation == generation
    @Synchronized fun enabled() = enabled
    @Synchronized fun commit(ticket: Ticket?, action: () -> Unit): Boolean {
        if (!permits(ticket)) return false
        action() // Registration only: never perform blocking I/O while holding the lock.
        return true
    }
}
