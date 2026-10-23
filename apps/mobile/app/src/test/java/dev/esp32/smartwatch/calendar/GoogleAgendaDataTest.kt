package dev.esp32.smartwatch.calendar

import org.json.JSONObject
import org.junit.Assert.*
import org.junit.Test
import java.time.LocalDate

class GoogleAgendaDataTest {
    private val monday=LocalDate.of(2026,10,19)
    private val calendar=GoogleAgendaData.Choice("primary","Personal",0x123456)
    private fun event(type: String="default")=JSONObject("""{"id":"event1","status":"confirmed","eventType":"$type","summary":"Meeting","start":{"date":"2026-10-19"},"end":{"date":"2026-10-20"}}""")
    @Test fun birthdaysAreSeparateAndUseExclusiveAllDayDates() {
        val b=event("birthday")
        assertNull(GoogleAgendaData.calendarEvent(b,monday,calendar,emptyMap(),false))
        val parsed=GoogleAgendaData.calendarEvent(b,monday,calendar,emptyMap(),true)!!.event
        assertEquals(1,parsed.days); assertEquals("Birthday / All day",parsed.time); assertTrue(parsed.allDay)
        assertNull(GoogleAgendaData.calendarEvent(event(),monday,calendar,emptyMap(),true))
    }
    @Test fun cancelledAndDeclinedAreNotShownAndColorsAreBounded() {
        val e=event().put("colorId","7")
        assertEquals(0xabcd12,GoogleAgendaData.calendarEvent(e,monday,calendar,mapOf("7" to 0xabcd12),false)!!.event.color)
        assertEquals(0x123456,GoogleAgendaData.calendarEvent(e,monday,calendar,emptyMap(),false)!!.event.color)
        e.put("status","cancelled"); assertNull(GoogleAgendaData.calendarEvent(e,monday,calendar,emptyMap(),false))
        e.put("status","confirmed").put("attendees",org.json.JSONArray("""[{"self":true,"responseStatus":"declined"}]"""))
        assertNull(GoogleAgendaData.calendarEvent(e,monday,calendar,emptyMap(),false))
        assertEquals(0x123456,GoogleAgendaData.color("#123456"))
        assertEquals(0x78B8FF,GoogleAgendaData.color("javascript:bad"))
    }
    @Test fun timedEventsRespectMadridDstAndMidnight() {
        val e=event().put("start",JSONObject("""{"dateTime":"2026-10-25T00:30:00Z"}"""))
            .put("end",JSONObject("""{"dateTime":"2026-10-25T02:30:00Z"}"""))
        assertEquals(64,GoogleAgendaData.calendarEvent(e,monday,calendar,emptyMap(),false)!!.event.days)
        e.put("start",JSONObject("""{"dateTime":"2026-10-19T23:00:00+02:00"}"""))
            .put("end",JSONObject("""{"dateTime":"2026-10-20T00:00:00+02:00"}"""))
        assertEquals(1,GoogleAgendaData.calendarEvent(e,monday,calendar,emptyMap(),false)!!.event.days)
    }
    @Test fun tasksNeverInventHoursDatesOrIncludeCompletedWork() {
        val t=JSONObject("""{"id":"task1","title":"Call","status":"needsAction","due":"2026-10-21T00:00:00.000Z"}""")
        val parsed=GoogleAgendaData.task(t,monday,"list")!!.event
        assertEquals(4,parsed.days); assertEquals("Task / Date only",parsed.time); assertTrue(parsed.allDay)
        t.put("status","completed"); assertNull(GoogleAgendaData.task(t,monday,"list"))
        t.put("status","needsAction").put("hidden",true); assertNull(GoogleAgendaData.task(t,monday,"list"))
        t.put("hidden",false).put("due","2026-10-26T00:00:00Z"); assertNull(GoogleAgendaData.task(t,monday,"list"))
        t.remove("due"); assertNull(GoogleAgendaData.task(t,monday,"list"))
    }
    @Test fun mergedSourcesAreOrderedBoundedAndNeverClaimPartialFailureIsEmpty() {
        val earlier=CalendarProtocol.Event(1,true,"First","","All day")
        val later=CalendarProtocol.Event(64,false,"Last","","10:00")
        val merged=GoogleAgendaData.merge(listOf(CalendarProtocol.Week(events=listOf(later),holidayDays=64),
            CalendarProtocol.Week(events=List(32) { earlier },holidayDays=1)))
        assertEquals(32,merged.events.size); assertEquals("First",merged.events.first().title)
        assertEquals(65,merged.holidayDays); assertTrue(merged.truncated)
        val failed=GoogleAgendaData.merge(listOf(CalendarProtocol.Week(events=listOf(earlier)),CalendarProtocol.Week(status=5)))
        assertEquals(5,failed.status); assertTrue(failed.events.isEmpty())
    }
}
