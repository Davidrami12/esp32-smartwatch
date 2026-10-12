package dev.esp32.smartwatch.health

import dev.esp32.smartwatch.activity.WatchActivitySnapshot
import java.time.Instant

interface StepSyncJournal {
    suspend fun read(id: String): StepSyncRevision?
    suspend fun write(id: String, revision: StepSyncRevision)
}
interface HealthStepsWriter {
    suspend fun permitted(): Boolean
    suspend fun insert(item: StepSyncItem, revision: StepSyncRevision)
}

/** Actual upsert/journal ordering, portable for failure/restart/replay tests. */
class StepSyncEngine(private val writer: HealthStepsWriter, private val journal: StepSyncJournal) {
    suspend fun synchronize(snapshot: WatchActivitySnapshot, manual: Boolean, now: Instant,
                            stillAllowed: () -> Boolean): Int {
        if (!writer.permitted()) throw SecurityException("Write steps not granted")
        val items = StepSyncPlan.items(snapshot, now) // Validate all dates before writing any.
        var written = 0
        for (item in items) {
            check(stillAllowed()) { "Synchronization disabled or watch disconnected" }
            val revision = StepSyncPlan.revision(item, journal.read(item.id), now.toEpochMilli())
            if (!manual && revision.acknowledged) continue
            if (!writer.permitted()) throw SecurityException("Write steps revoked")
            // Durable intent before remote mutation: remote success/local failure retries same version.
            journal.write(item.id, revision)
            writer.insert(item, revision)
            journal.write(item.id, revision.copy(acknowledged = true))
            ++written
        }
        return written
    }
}
