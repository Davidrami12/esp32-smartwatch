package dev.esp32.smartwatch.health

import dev.esp32.smartwatch.activity.*
import kotlinx.coroutines.runBlocking
import org.junit.Assert.*
import org.junit.Test
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.time.*

class StepSyncTest {
    private val zone = ZoneId.of("Europe/Madrid")
    private val today = LocalDate.of(2026, 10, 12)
    private val now = today.atTime(16, 0).atZone(zone).toInstant()
    private fun day(date: LocalDate = today, count: Long = 100, complete: Boolean = false, zone: ZoneId = this.zone): WatchActivityDay {
        val start = date.atStartOfDay(zone)
        val end = if (complete) date.plusDays(1).atStartOfDay(zone) else date.atTime(12, 0).atZone(zone)
        return WatchActivityDay(date, count, start.toInstant(), end.toInstant(), start.offset, end.offset)
    }
    private fun snapshot(vararg days: WatchActivityDay) = WatchActivitySnapshot("AA:BB:CC:DD:EE:FF", days.toList())
    private fun encoded(vararg days: WatchActivityDay): ByteArray {
        val data = ByteBuffer.allocate(2 + 24*days.size).order(ByteOrder.LITTLE_ENDIAN)
        data.put(1).put(days.size.toByte())
        days.forEach { d -> data.putInt(d.date.year*10000 + d.date.monthValue*100 + d.date.dayOfMonth)
            .putInt(d.count.toInt()).putInt(d.start.epochSecond.toInt()).putInt(d.end.epochSecond.toInt())
            .putInt(d.startOffset.totalSeconds).putInt(d.endOffset.totalSeconds) }
        return data.array()
    }
    private class Journal : StepSyncJournal {
        val rows = mutableMapOf<String, StepSyncRevision>()
        var failAck = false
        override suspend fun read(id: String) = rows[id]
        override suspend fun write(id: String, revision: StepSyncRevision) {
            if (failAck && revision.acknowledged) throw IllegalStateException("disk failed")
            rows[id] = revision
        }
    }
    private class Writer : HealthStepsWriter {
        val records = mutableMapOf<String, Pair<StepSyncItem, StepSyncRevision>>()
        var granted = true
        var calls = 0
        var failure = false
        var afterWrite: (() -> Unit)? = null
        override suspend fun permitted() = granted
        override suspend fun insert(item: StepSyncItem, revision: StepSyncRevision) {
            ++calls
            if (!granted) throw SecurityException()
            if (failure) throw IllegalStateException("provider failed")
            val prior = records[item.id]
            if (item.day.count == 0L) records.remove(item.id)
            else if (prior == null || revision.version > prior.second.version) records[item.id] = item to revision
            afterWrite?.invoke()
        }
    }
    @Test fun repeatedManualAutomaticAndIncreasingTotalsAreIdempotent() = runBlocking {
        val journal = Journal(); val writer = Writer(); val engine = StepSyncEngine(writer, journal)
        val input = snapshot(day())
        assertEquals(1, engine.synchronize(input, true, now) { true })
        val version = writer.records.values.single().second.version
        assertEquals(0, engine.synchronize(input, false, now.plusSeconds(300)) { true })
        assertEquals(1, engine.synchronize(input, true, now.plusSeconds(600)) { true })
        assertEquals(1, writer.records.size)
        assertEquals(version, writer.records.values.single().second.version)
        val restart = StepSyncEngine(writer, journal)
        restart.synchronize(snapshot(day(count = 150)), false, now.minusSeconds(10)) { true }
        assertEquals(1, writer.records.size)
        assertEquals(150L, writer.records.values.single().first.day.count)
        assertTrue(writer.records.values.single().second.version > version) // Also tolerates phone clock going back.
        restart.synchronize(snapshot(day(count = 80)), true, now.plusSeconds(900)) { true }
        assertEquals(80L, writer.records.values.single().first.day.count) // Watch-owned correction, not max(Android, watch).
    }
    @Test fun journalFailureAfterRemoteSuccessRecoversWithoutDuplicate() = runBlocking {
        val journal = Journal(); val writer = Writer(); val engine = StepSyncEngine(writer, journal)
        journal.failAck = true
        try { engine.synchronize(snapshot(day()), true, now) { true }; fail() } catch (_: IllegalStateException) { }
        val version = writer.records.values.single().second.version
        assertFalse(journal.rows.values.single().acknowledged)
        journal.failAck = false
        StepSyncEngine(writer, journal).synchronize(snapshot(day()), false, now.plusSeconds(500)) { true }
        assertEquals(1, writer.records.size); assertEquals(version, writer.records.values.single().second.version)
        assertTrue(journal.rows.values.single().acknowledged)
    }
    @Test fun permissionDenialRevocationAndDisconnectStopWrites() = runBlocking {
        val journal = Journal(); val writer = Writer(); val engine = StepSyncEngine(writer, journal)
        writer.granted = false
        try { engine.synchronize(snapshot(day()), true, now) { true }; fail() } catch (_: SecurityException) { }
        assertEquals(0, writer.calls); assertTrue(journal.rows.isEmpty())
        writer.granted = true; writer.afterWrite = { writer.granted = false }
        try { engine.synchronize(snapshot(day(), day(today.minusDays(1), complete = true)), true, now) { true }; fail() }
        catch (_: SecurityException) { }
        assertEquals(1, writer.records.size)
        writer.granted = true; writer.afterWrite = null
        try { engine.synchronize(snapshot(day()), true, now) { false }; fail() } catch (_: IllegalStateException) { }
        assertEquals(1, writer.calls)
        engine.synchronize(snapshot(day()), true, now.plusSeconds(300)) { true }
        assertEquals(1, writer.records.size) // Reconnect replay.
    }
    @Test fun remoteFailureRetainsPendingVersion() = runBlocking {
        val journal = Journal(); val writer = Writer(); val engine = StepSyncEngine(writer, journal)
        writer.failure = true
        try { engine.synchronize(snapshot(day()), true, now) { true }; fail() } catch (_: IllegalStateException) { }
        val pending = journal.rows.values.single().version
        writer.failure = false
        engine.synchronize(snapshot(day()), true, now.plusSeconds(300)) { true }
        assertEquals(pending, writer.records.values.single().second.version)
    }
    @Test fun rolloverFinalizationMissingDatesAndOtherSources() = runBlocking {
        val journal = Journal(); val writer = Writer(); val engine = StepSyncEngine(writer, journal)
        engine.synchronize(snapshot(day()), true, now) { true }
        val later = today.plusDays(1).atTime(16,0).atZone(zone).toInstant()
        engine.synchronize(snapshot(day(today.plusDays(1), count=10), day(complete=true), day(today.minusDays(3), count=200, complete=true)), true, later) { true }
        assertEquals(3, writer.records.size) // No entries for missing days.
        assertTrue(writer.records.values.first { it.first.day.date == today }.first.day.end.atOffset(ZoneOffset.ofHours(2)).toLocalDate() > today)
        val another = snapshot(day()).copy(source = "another-watch")
        engine.synchronize(another, true, now) { true }; assertEquals(4, writer.records.size)
    }
    @Test fun phoneTimezoneDoesNotChangeRecordIdentityOrInterval() {
        val input = snapshot(day()); val first = StepSyncPlan.items(input, now).single()
        val previous = java.util.TimeZone.getDefault()
        try {
            java.util.TimeZone.setDefault(java.util.TimeZone.getTimeZone("Pacific/Honolulu"))
            val other = StepSyncPlan.items(input, now).single()
            assertEquals(first, other)
            assertEquals(today.minusDays(1), input.days.single().start.atZone(ZoneId.of("Pacific/Honolulu")).toLocalDate())
        } finally { java.util.TimeZone.setDefault(previous) }
    }
    @Test fun dstShortLongDaysAndZeroCountsDecode() {
        for ((date, hours) in listOf(LocalDate.of(2026,3,29) to 23L, LocalDate.of(2026,10,25) to 25L)) {
            val d = day(date, count=0, complete=true)
            val parsed = WatchActivityProtocol.decode("watch", encoded(d))
            assertEquals(d, parsed.days.single())
            assertEquals(hours, Duration.between(d.start,d.end).toHours())
        }
        assertTrue(WatchActivityProtocol.decode("watch", byteArrayOf(1,0)).days.isEmpty())
    }
    @Test fun malformedAndOutOfRangeDataAreRejected() {
        for (bytes in listOf(byteArrayOf(), byteArrayOf(2,0), byteArrayOf(1,8), encoded(day(),day()), encoded(day(today.minusDays(1)),day()))) {
            try { WatchActivityProtocol.decode("watch",bytes); fail("Malformed accepted") } catch (_: IllegalArgumentException) { }
        }
        try { StepSyncPlan.items(snapshot(day(count=1_000_001)),now); fail() } catch (_: IllegalArgumentException) { }
        try { StepSyncPlan.items(snapshot(day()), now.minusSeconds(8*3600)); fail() } catch (_: IllegalArgumentException) { }
    }
    @Test fun timestampOnlyRefreshDoesNotIncrementVersionButFinalizationDoes() {
        val first = StepSyncPlan.items(snapshot(day()), now).single()
        val refreshed = StepSyncPlan.items(snapshot(day().copy(end=now)), now).single()
        assertEquals(first.fingerprint, refreshed.fingerprint)
        val completed = StepSyncPlan.items(snapshot(day(complete=true)), now.plusSeconds(24*3600)).single()
        assertNotEquals(first.fingerprint, completed.fingerprint); assertEquals(first.id, completed.id)
    }
    @Test fun actualSdkRecordHasWriteOnlyPermissionAndWatchMetadata() {
        val item = StepSyncPlan.items(snapshot(day(count=5)), now).single()
        val revision = StepSyncPlan.revision(item, null, now.toEpochMilli())
        val record = HealthConnectRepository.record(item, revision)
        assertEquals(setOf("android.permission.health.WRITE_STEPS"), HealthConnectRepository.PERMISSIONS)
        assertEquals(5L, record.count)
        assertEquals(item.day.start, record.startTime); assertEquals(item.day.end, record.endTime)
        assertEquals(item.day.startOffset, record.startZoneOffset); assertEquals(item.day.endOffset, record.endZoneOffset)
        assertEquals(item.id, record.metadata.clientRecordId)
        assertEquals(revision.version, record.metadata.clientRecordVersion)
        assertEquals(androidx.health.connect.client.records.metadata.Device.TYPE_WATCH, record.metadata.device?.type)
        assertEquals("Waveshare", record.metadata.device?.manufacturer)
        assertEquals("ESP32-S3 Touch AMOLED 2.06", record.metadata.device?.model)
        assertEquals(androidx.health.connect.client.records.metadata.Metadata.RECORDING_METHOD_AUTOMATICALLY_RECORDED, record.metadata.recordingMethod)
    }
    @Test fun zeroDaysDoNotInventStepsAndZeroCorrectionsRemoveOnlyOwnExactId() = runBlocking {
        val journal = Journal(); val writer = Writer(); val engine = StepSyncEngine(writer, journal)
        engine.synchronize(snapshot(day(count=0)), true, now) { true }
        assertTrue(writer.records.isEmpty())
        engine.synchronize(snapshot(day(count=20)), true, now.plusSeconds(1)) { true }
        val other = snapshot(day(count=50)).copy(source="other-source")
        engine.synchronize(other, true, now.plusSeconds(2)) { true }
        assertEquals(2, writer.records.size)
        engine.synchronize(snapshot(day(count=0)), true, now.plusSeconds(3)) { true }
        assertEquals(1, writer.records.size); assertEquals(50L,writer.records.values.single().first.day.count)
        engine.synchronize(snapshot(day(count=10)), true, now.plusSeconds(4)) { true }
        assertEquals(2, writer.records.size)
    }
}
