package dev.esp32.smartwatch.activity

import dev.esp32.smartwatch.ble.WatchBleProtocol
import org.junit.Assert.*
import org.junit.Test
import java.time.*

class ActivityDashboardTest {
    private val today = LocalDate.of(2026,10,12)
    private val zone = ActivityDashboardModel.WATCH_ZONE
    private val now = today.atTime(14,0).atZone(zone).toInstant()
    private fun day(date: LocalDate, count: Long): WatchActivityDay {
        val start = date.atStartOfDay(zone)
        val end = if (date == today) now.atZone(zone) else date.plusDays(1).atStartOfDay(zone)
        return WatchActivityDay(date,count,start.toInstant(),end.toInstant(),start.offset,end.offset)
    }
    private fun cache(count: Long=4000, goal: Int?=8000) = ActivityCache(
        WatchActivitySnapshot("watch",listOf(day(today,count),day(today.minusDays(2),2100),day(today.minusDays(6),900))),
        now.toEpochMilli(),goal,now.toEpochMilli())
    @Test fun sevenDaysIncludeTodayAndMissingZeros() {
        val summary = ActivityDashboardModel.summary(cache(),now)
        assertEquals(7,summary.bars.size); assertEquals(today,summary.bars.last().date)
        assertEquals(today.minusDays(6),summary.bars.first().date)
        assertEquals(0L,summary.bars[1].steps); assertFalse(summary.bars[1].recorded)
        assertEquals(7000L,summary.total); assertEquals(1000.0,summary.average,0.001)
        assertEquals(50.0,summary.percentage!!,0.001); assertEquals(4000L,summary.remaining)
        assertEquals(0.5f,summary.visualProgress,0.001f)
    }
    @Test fun countsAndPercentageAreNotTruncatedButVisualProgressIsCapped() {
        val summary = ActivityDashboardModel.summary(cache(12000),now)
        assertEquals(12000L,summary.todaySteps); assertEquals(150.0,summary.percentage!!,0.001)
        assertEquals(1f,summary.visualProgress,0f); assertEquals(0L,summary.remaining)
    }
    @Test fun zeroAndUnknownGoalAreSafe() {
        val zero = ActivityDashboardModel.summary(cache(0),now)
        assertTrue(zero.todayRecorded); assertEquals(0f,zero.visualProgress,0f); assertEquals(8000L,zero.remaining)
        val unknown = ActivityDashboardModel.summary(cache(goal=null),now)
        assertNull(unknown.percentage); assertNull(unknown.remaining); assertNull(unknown.goal)
    }
    @Test fun calendarRolloverDoesNotRelabelYesterdayAsToday() {
        val next = today.plusDays(1).atTime(12,0).atZone(zone).toInstant()
        val summary = ActivityDashboardModel.summary(cache(),next)
        assertEquals(today.plusDays(1),summary.today); assertEquals(0L,summary.todaySteps); assertFalse(summary.todayRecorded)
        assertEquals(4000L,summary.bars[5].steps)
        assertFalse(ActivityDashboardModel.isLive(cache(),true,cache().snapshot,next,null))
    }
    @Test fun disconnectedWrongSourceExpiredAndFailedReadsAreNotLive() {
        val cache = cache()
        assertTrue(ActivityDashboardModel.isLive(cache,true,cache.snapshot,now,null))
        assertFalse(ActivityDashboardModel.isLive(cache,false,cache.snapshot,now,null))
        assertFalse(ActivityDashboardModel.isLive(cache,true,cache.snapshot.copy(source="other"),now,null))
        assertFalse(ActivityDashboardModel.isLive(cache,true,cache.snapshot,now.plusSeconds(91),null))
        assertFalse(ActivityDashboardModel.isLive(cache,true,cache.snapshot,now,"read failed"))
        assertFalse(ActivityDashboardModel.isLive(cache,true,cache.snapshot,now.minusSeconds(1),null))
    }
    @Test fun cacheRoundTripRetainsGoalDatesOffsetsCountsAndTimestamp() {
        val cached = cache()
        assertEquals(cached,ActivityCacheCodec.decode(ActivityCacheCodec.encode(cached)))
        assertEquals(cached.snapshot,WatchActivityProtocol.decode("watch",WatchActivityProtocol.encode(cached.snapshot)))
        val unknown = cache(goal=null).copy(goalUpdatedAt=null)
        assertEquals(unknown,ActivityCacheCodec.decode(ActivityCacheCodec.encode(unknown)))
        for (bytes in listOf(byteArrayOf(),byteArrayOf(2),ByteArray(513),ActivityCacheCodec.encode(cached).dropLast(3).toByteArray())) {
            try { ActivityCacheCodec.decode(bytes); fail("Malformed cache accepted") } catch (_: Exception) { }
        }
    }
    @Test fun phoneTimezoneAndDstDoNotShiftWatchCalendar() {
        val original = java.util.TimeZone.getDefault()
        try {
            java.util.TimeZone.setDefault(java.util.TimeZone.getTimeZone("Pacific/Honolulu"))
            assertEquals(today,ActivityDashboardModel.summary(cache(),now).today)
            assertEquals(today,ActivityDashboardModel.summary(cache(),today.atStartOfDay(zone).toInstant()).today)
        } finally { java.util.TimeZone.setDefault(original) }
        assertEquals(23,Duration.between(LocalDate.of(2026,3,29).atStartOfDay(zone),LocalDate.of(2026,3,30).atStartOfDay(zone)).toHours().toInt())
        assertEquals(25,Duration.between(LocalDate.of(2026,10,25).atStartOfDay(zone),LocalDate.of(2026,10,26).atStartOfDay(zone)).toHours().toInt())
    }
    @Test fun goalReadIsStrictAndBackwardCompatible() {
        assertEquals(8000,WatchBleProtocol.stepGoal(byteArrayOf(0x40,0x1f,0,0)))
        for (bytes in listOf(byteArrayOf(),byteArrayOf(0,0,0,0),byteArrayOf(1,0,0,0),byteArrayOf(-1,-1,-1,-1))) {
            try { WatchBleProtocol.stepGoal(bytes); fail("Invalid goal accepted") } catch (_: IllegalArgumentException) { }
        }
    }
}
