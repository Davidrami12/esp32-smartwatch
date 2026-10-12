package dev.esp32.smartwatch.health

import dev.esp32.smartwatch.activity.WatchActivityDay
import dev.esp32.smartwatch.activity.WatchActivitySnapshot
import java.security.MessageDigest
import java.time.Instant

data class StepSyncItem(val id: String, val fingerprint: String, val day: WatchActivityDay)
data class StepSyncRevision(val fingerprint: String, val version: Long, val acknowledged: Boolean)

/** Pure policy; a record's ID is independent of counts, offsets and phone timezone. */
object StepSyncPlan {
    private fun hash(value: String) = MessageDigest.getInstance("SHA-256").digest(value.toByteArray(Charsets.UTF_8))
        .joinToString("") { "%02x".format(it) }
    fun items(snapshot: WatchActivitySnapshot, now: Instant): List<StepSyncItem> {
        require(snapshot.days.isNotEmpty()) { "Watch calendar is not ready." }
        return snapshot.days.map { day ->
            require(day.count in 0..1_000_000) { "Watch step total is outside Health Connect's supported range." }
            require(day.end <= now.plusSeconds(300)) { "Watch clock is ahead of the phone. Correct the watch clock first." }
            val id = "watch-steps-v1:${hash(snapshot.source)}:${day.date}"
            val closed = day.end.atOffset(day.endOffset).toLocalDate() == day.date.plusDays(1)
            // Advancing fetch timestamps alone must not generate writes. Finalizing a day must.
            val fingerprint = hash("${day.count}|${day.start}|${day.startOffset}|${day.endOffset}|$closed")
            StepSyncItem(id, fingerprint, day)
        }
    }
    fun revision(item: StepSyncItem, previous: StepSyncRevision?, nowMillis: Long): StepSyncRevision {
        if (previous?.fingerprint == item.fingerprint) return previous
        val last = previous?.version ?: 0L
        require(last < Long.MAX_VALUE)
        return StepSyncRevision(item.fingerprint, maxOf(1, nowMillis, last + 1), false)
    }
}
