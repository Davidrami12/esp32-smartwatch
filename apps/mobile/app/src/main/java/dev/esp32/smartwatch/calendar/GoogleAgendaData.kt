package dev.esp32.smartwatch.calendar

import org.json.JSONObject
import java.time.Instant
import java.time.LocalDate
import java.time.ZoneId
import java.time.ZoneOffset
import java.time.format.DateTimeFormatter
import java.util.Locale

/** Pure parsers shared by network integration and host unit tests. */
object GoogleAgendaData {
    private val zone=ZoneId.of("Europe/Madrid")
    data class Choice(val id: String,val label: String,val color: Int=0x78B8FF,val holiday: Boolean=false)
    data class Occurrence(val key: String,val event: CalendarProtocol.Event)
    fun color(value: String, fallback: Int=0x78B8FF): Int =
        if (Regex("#[0-9a-fA-F]{6}").matches(value)) value.substring(1).toInt(16) else fallback

    fun calendarEvent(item: JSONObject,monday: LocalDate,calendar: Choice,palette: Map<String,Int>,birthdaysOnly: Boolean): Occurrence? {
        if (item.optString("status")=="cancelled" || (item.optString("eventType")=="birthday") != birthdaysOnly) return null
        val attendees=item.optJSONArray("attendees")
        if (attendees!=null) for (i in 0 until attendees.length()) {
            val a=attendees.getJSONObject(i)
            if (a.optBoolean("self") && a.optString("responseStatus")=="declined") return null
        }
        val start=item.optJSONObject("start") ?: return null
        val end=item.optJSONObject("end") ?: return null
        val allDay=start.has("date")
        val begin=if (allDay) LocalDate.parse(start.getString("date")).atStartOfDay(ZoneOffset.UTC).toInstant()
            else Instant.parse(start.getString("dateTime"))
        val finish=if (allDay) LocalDate.parse(end.getString("date")).atStartOfDay(ZoneOffset.UTC).toInstant()
            else Instant.parse(end.getString("dateTime"))
        if (finish<begin) return null
        val days=CalendarProtocol.dayMask(monday,begin.toEpochMilli(),finish.toEpochMilli(),allDay)
        if (days==0) return null
        val first=begin.atZone(zone); val last=finish.atZone(zone)
        val time=DateTimeFormatter.ofPattern("HH:mm",Locale.UK)
        val dateTime=DateTimeFormatter.ofPattern("dd/MM HH:mm",Locale.UK)
        val label=when {
            birthdaysOnly -> "Birthday / All day"
            allDay && LocalDate.parse(end.getString("date"))>LocalDate.parse(start.getString("date")).plusDays(1) ->
                "All day / ${start.getString("date").substring(5)} - ${LocalDate.parse(end.getString("date")).minusDays(1).toString().substring(5)}"
            allDay -> "All day"
            first.toLocalDate()==last.toLocalDate() -> "${first.format(time)} - ${last.format(time)}"
            else -> "${first.format(dateTime)} - ${last.format(dateTime)}"
        }
        val rgb=palette[item.optString("colorId")] ?: calendar.color
        val title=item.optString("summary").ifBlank { if (birthdaysOnly) "Birthday" else "Untitled event" }
        return Occurrence("${calendar.id}/${item.getString("id")}/${begin}",CalendarProtocol.Event(days,allDay,title,item.optString("location"),label,rgb,begin.toEpochMilli()))
    }
    fun task(item: JSONObject,monday: LocalDate,listId: String): Occurrence? {
        if (item.optBoolean("deleted") || item.optBoolean("hidden") || item.optString("status")!="needsAction") return null
        val due=item.optString("due")
        if (due.isBlank()) return null // Never invent a date or an unavailable scheduled hour.
        val date=LocalDate.parse(due.take(10))
        val offset=java.time.temporal.ChronoUnit.DAYS.between(monday,date)
        if (offset !in 0..6) return null
        return Occurrence("task/$listId/${item.getString("id")}",CalendarProtocol.Event(1 shl offset.toInt(),true,
            "Task: ${item.optString("title").ifBlank { "Untitled task" }}","","Task / Date only",0x8AB4F8,date.atStartOfDay(zone).toInstant().toEpochMilli()))
    }
    fun merge(weeks: List<CalendarProtocol.Week>): CalendarProtocol.Week {
        val failure=weeks.firstOrNull { it.status!=0 }
        if (failure!=null) return CalendarProtocol.Week(status=failure.status)
        val events=weeks.flatMap { it.events }.sortedWith(compareBy<CalendarProtocol.Event>(
            { Integer.numberOfTrailingZeros(it.days) },{ !it.allDay },{ it.startMillis },{ it.title }))
        return CalendarProtocol.Week(events=events.take(CalendarProtocol.MAX_EVENTS),
            truncated=events.size>CalendarProtocol.MAX_EVENTS || weeks.any { it.truncated },
            holidayDays=weeks.fold(0) { mask,w -> mask or w.holidayDays })
    }
}
