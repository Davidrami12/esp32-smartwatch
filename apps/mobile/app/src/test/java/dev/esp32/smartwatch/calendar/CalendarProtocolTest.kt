package dev.esp32.smartwatch.calendar

import org.junit.Assert.*
import org.junit.Test
import java.time.Instant
import java.time.LocalDate
import java.util.zip.CRC32

class CalendarProtocolTest {
    @Test fun holidayCalendarsUseMetadataNotPersonalEventTitles() {
        for (name in listOf("Festivos de España","Festivos en Madrid","Fiestas de la Comunidad de Madrid","Holidays in Spain","Calendario laboral Madrid"))
            assertTrue(CalendarProtocol.suggestedHolidayCalendar(name))
        assertTrue(CalendarProtocol.suggestedHolidayCalendar("Imported","es.spain#holiday@group.v.calendar.google.com"))
        for (name in listOf("Personal Madrid","Spain travel","Cumpleaños","Holidays in France","Work","Festivos"))
            assertFalse(CalendarProtocol.suggestedHolidayCalendar(name))
    }
    @Test fun holidayMaskIsNegotiatedAndBounded() {
        val request=CalendarProtocol.Request(1,LocalDate.of(2026,10,19),3)
        val week=CalendarProtocol.Week(holidayDays=0x45)
        val modern=CalendarProtocol.frames(request,week,23).flatMap { it.drop(10) }.toByteArray()
        assertEquals(2,modern[3].toInt()); assertEquals(0x45,modern[8].toInt())
        val old=CalendarProtocol.frames(request.copy(version=2),week,23).flatMap { it.drop(10) }.toByteArray()
        assertEquals(1,old[3].toInt()); assertEquals(old.size+1,modern.size)
        assertThrows(IllegalArgumentException::class.java) { CalendarProtocol.frames(request,week.copy(holidayDays=128),23) }
        assertThrows(IllegalArgumentException::class.java) { CalendarProtocol.frames(request,week.copy(status=2),23) }
    }
    @Test fun colorsAreNegotiatedWithoutBreakingLegacyFrames() {
        val request=CalendarProtocol.Request(1,LocalDate.of(2026,10,19),2)
        val week=CalendarProtocol.Week(events=listOf(CalendarProtocol.Event(1,false,"Event","","09:00",0xff12ab34.toInt())))
        val modern=CalendarProtocol.frames(request,week,517).flatMap { it.drop(10) }.toByteArray()
        assertEquals(1,modern[3].toInt())
        assertArrayEquals(byteArrayOf(0x34,0xab.toByte(),0x12),modern.copyOfRange(13,16))
        val legacy=CalendarProtocol.frames(request.copy(version=1),week,517).flatMap { it.drop(10) }.toByteArray()
        assertEquals(0,legacy[3].toInt()); assertEquals(legacy.size+3,modern.size)
    }
    private val request = CalendarProtocol.Request(0x12345678, LocalDate.of(2026,10,19))
    @Test fun framesAreBoundedAndCrcProtected() {
        val event = CalendarProtocol.Event(127, false, "Reunión ñ".repeat(20), "Madrid".repeat(20), "09:00 - 10:00")
        for (mtu in listOf(23, 247, 517)) for (version in 1..4) {
            val frames = CalendarProtocol.frames(request.copy(version=version), CalendarProtocol.Week(events = List(32) { event }, fetchedAt = 100), mtu)
            assertTrue(frames.all { it.size <= minOf(244, mtu-3) })
            val payload = frames.flatMap { it.drop(10) }.toByteArray()
            val crc = (0..3).fold(0L) { n,i -> n or ((payload[payload.size-4+i].toLong() and 255) shl (8*i)) }
            assertEquals(CRC32().apply { update(payload,0,payload.size-4) }.value,crc)
            assertTrue(payload.size <= 7000)
            assertEquals(32,payload[1].toInt())
        }
    }
    @Test fun utf8TruncationIsAPrefixAndReplacesControls() {
        assertEquals("á", CalendarProtocol.text("á💥z",3).toString(Charsets.UTF_8))
        assertEquals("a b c", CalendarProtocol.text("a\nb\tc",80).toString(Charsets.UTF_8))
    }
    @Test fun allDayAndMidnightEndAreDateCorrect() {
        fun mask(begin: String,end: String,allDay: Boolean=false) = CalendarProtocol.dayMask(request.monday,
            Instant.parse(begin).toEpochMilli(),Instant.parse(end).toEpochMilli(),allDay)
        assertEquals(1,mask("2026-10-19T00:00:00Z","2026-10-20T00:00:00Z",true))
        assertEquals(3,mask("2026-10-19T21:00:00Z","2026-10-20T01:00:00Z"))
        assertEquals(1,mask("2026-10-19T20:00:00Z","2026-10-19T22:00:00Z")) // Ends at Madrid midnight.
        assertEquals(64,mask("2026-10-25T00:30:00Z","2026-10-25T02:30:00Z")) // Fall-back DST.
        assertEquals(0,mask("2026-10-26T00:00:00Z","2026-10-27T00:00:00Z",true))
        assertEquals(64,CalendarProtocol.dayMask(LocalDate.of(2026,3,23),
            Instant.parse("2026-03-29T00:30:00Z").toEpochMilli(),Instant.parse("2026-03-29T02:30:00Z").toEpochMilli(),false))
    }
    @Test fun requestsRejectWrongWeekAndVersion() {
        val bytes=byteArrayOf(1,0,1,0,0,0,0,0,0,0)
        val key=20261019
        repeat(4) { bytes[6+it]=(key ushr (8*it)).toByte() }
        assertEquals(request.monday,CalendarProtocol.request(bytes)!!.monday)
        bytes[0]=2; assertEquals(2,CalendarProtocol.request(bytes)!!.version)
        bytes[0]=3; assertEquals(3,CalendarProtocol.request(bytes)!!.version)
        bytes[0]=4; assertEquals(4,CalendarProtocol.request(bytes)!!.version)
        bytes[0]=5
        assertThrows(IllegalArgumentException::class.java) { CalendarProtocol.request(bytes) }
        bytes[0]=1; bytes[6]=(key+1).toByte()
        assertThrows(IllegalArgumentException::class.java) { CalendarProtocol.request(bytes) }
    }
    @Test fun googleAuthorizationStatusIsCompatibleWithOlderWatches() {
        val week=CalendarProtocol.Week(status=5)
        for (version in 1..4) {
            val first=CalendarProtocol.frames(request.copy(version=version),week,517).first()
            assertEquals(if (version==4) 5 else 2,first[10].toInt())
        }
    }
}
