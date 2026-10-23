package dev.esp32.smartwatch.calendar

import android.content.Context
import android.content.SharedPreferences
import org.json.JSONObject
import java.io.ByteArrayOutputStream
import java.net.URL
import java.net.URLEncoder
import java.time.LocalDate
import java.time.ZoneId
import java.time.ZoneOffset
import java.util.concurrent.CancellationException
import javax.net.ssl.HttpsURLConnection

class GoogleAgendaHttpFailure(val code: Int) : Exception("Google agenda request failed ($code)")

/** Fixed HTTPS origin, GET-only API, no redirects, bounded work and no response logs. */
class GoogleAgendaClient(private val context: Context,private val account: String,private val valid: () -> Boolean) {
    private val token=GoogleAgendaAuth.token(context,account)
    private val deadline=System.nanoTime()+70_000_000_000L
    private var calls=0
    private fun check() {
        if (!valid()) throw CancellationException()
        check(System.nanoTime()<deadline && calls<32) { "Google agenda request budget exceeded" }
    }
    private fun enc(s: String)=URLEncoder.encode(s,"UTF-8").replace("+","%20")
    private fun get(path: String,params: Map<String,String> = emptyMap()): JSONObject {
        check(); calls++
        require(path.startsWith("/calendar/v3/") || path.startsWith("/tasks/v1/"))
        val query=params.entries.joinToString("&") { "${enc(it.key)}=${enc(it.value)}" }
        val connection=URL("https://www.googleapis.com$path${if (query.isEmpty()) "" else "?$query"}").openConnection() as HttpsURLConnection
        try {
            connection.requestMethod="GET"; connection.instanceFollowRedirects=false
            connection.connectTimeout=6000; connection.readTimeout=6000
            connection.setRequestProperty("Authorization","Bearer $token")
            connection.setRequestProperty("Accept","application/json")
            val status=connection.responseCode
            if (status!=200) {
                if (status==401) { GoogleAgendaAuth.clearToken(context,token); throw GoogleAgendaConsentRequired() }
                throw GoogleAgendaHttpFailure(status)
            }
            val out=ByteArrayOutputStream()
            connection.inputStream.use { input ->
                val buffer=ByteArray(8192)
                while (true) {
                    if (!valid() || System.nanoTime()>=deadline) throw CancellationException()
                    val count=input.read(buffer); if (count<0) break
                    require(out.size()+count<=1_048_576) { "Google agenda response too large" }
                    out.write(buffer,0,count)
                }
            }
            if (!valid()) throw CancellationException()
            return JSONObject(out.toString("UTF-8"))
        } finally { connection.disconnect() }
    }
    private data class Page(val items: List<JSONObject>,val limited: Boolean)
    private fun pages(path: String,params: Map<String,String>,maxPages: Int=3,maxItems: Int=300): Page {
        val items=mutableListOf<JSONObject>(); var page=""
        repeat(maxPages) {
            val result=get(path,params + if (page.isEmpty()) emptyMap() else mapOf("pageToken" to page))
            val values=result.optJSONArray("items")
            if (values!=null) for (i in 0 until values.length()) {
                if (items.size==maxItems) return Page(items,true)
                items.add(values.getJSONObject(i))
            }
            page=result.optString("nextPageToken")
            if (page.isEmpty()) return Page(items,false)
        }
        return Page(items,true)
    }
    data class Catalog(val calendars: List<GoogleAgendaData.Choice>,val tasks: List<GoogleAgendaData.Choice>,val limited: Boolean)
    fun catalog(): Catalog {
        val calendars=pages("/calendar/v3/users/me/calendarList",mapOf("maxResults" to "100","showHidden" to "true",
            "fields" to "items(id,summary,summaryOverride,backgroundColor,deleted),nextPageToken"),2,128)
        val tasks=pages("/tasks/v1/users/@me/lists",mapOf("maxResults" to "100","fields" to "items(id,title),nextPageToken"),2,128)
        return Catalog(calendars.items.filter { !it.optBoolean("deleted") }.map { item ->
            val name=item.optString("summaryOverride").ifBlank { item.optString("summary") }
            GoogleAgendaData.Choice(item.getString("id"),name.ifBlank { "Google calendar" },GoogleAgendaData.color(item.optString("backgroundColor")),
                CalendarProtocol.suggestedHolidayCalendar(name,item.getString("id")))
        },tasks.items.map { GoogleAgendaData.Choice(it.getString("id"),it.optString("title").ifBlank { "Task list" }) },calendars.limited || tasks.limited)
    }
    fun week(monday: LocalDate,prefs: SharedPreferences,localCalendarNames: Set<String>): CalendarProtocol.Week {
        val selected=prefs.getStringSet("google_calendars",emptySet()).orEmpty().toSet()
        val lists=prefs.getStringSet("google_tasklists",emptySet()).orEmpty().toSet()
        val birthdays=prefs.getBoolean("google_birthdays",false)
        require(selected.size+lists.size+(if (birthdays) 1 else 0)<=8) { "Select at most eight Google sources" }
        val catalog=if (selected.isNotEmpty()) catalog() else Catalog(emptyList(),emptyList(),false)
        if (selected.any { id -> catalog.calendars.none { it.id==id } }) throw IllegalStateException("Selected Google calendar unavailable")
        val palette=mutableMapOf<String,Int>()
        if (selected.isNotEmpty() || birthdays) {
            val colors=get("/calendar/v3/colors").getJSONObject("event")
            colors.keys().forEach { id -> palette[id]=GoogleAgendaData.color(colors.getJSONObject(id).getString("background")) }
        }
        val entries=linkedMapOf<String,CalendarProtocol.Event>(); var holidayDays=0; var limited=false
        val zone=ZoneId.of("Europe/Madrid")
        val params=mapOf("singleEvents" to "true","orderBy" to "startTime","maxResults" to "100",
            "fields" to "items(id,status,eventType,summary,location,start,end,colorId,attendees(self,responseStatus)),nextPageToken",
            "timeMin" to monday.atStartOfDay(zone).toInstant().toString(),
            "timeMax" to monday.plusDays(7).atStartOfDay(zone).toInstant().toString())
        fun events(calendar: GoogleAgendaData.Choice,birthdayOnly: Boolean) {
            val result=pages("/calendar/v3/calendars/${enc(calendar.id)}/events",params + if (birthdayOnly) mapOf("eventTypes" to "birthday") else emptyMap())
            limited=limited || result.limited
            val holiday=!birthdayOnly && prefs.getBoolean("google_holiday_${calendar.id}",calendar.holiday)
            result.items.forEach { item ->
                GoogleAgendaData.calendarEvent(item,monday,calendar,palette,birthdayOnly)?.let { entry ->
                    entries[entry.key]=entry.event
                    if (holiday) holidayDays=holidayDays or entry.event.days
                }
            }
        }
        catalog.calendars.filter { it.id in selected && it.id !in localCalendarNames }.forEach { events(it,false) }
        if (birthdays) events(GoogleAgendaData.Choice("primary","Birthdays",0xBA91EB),true)
        lists.sorted().forEach { id ->
            val result=pages("/tasks/v1/lists/${enc(id)}/tasks",mapOf("maxResults" to "100","showCompleted" to "false",
                "fields" to "items(id,title,status,deleted,hidden,due),nextPageToken",
                "showHidden" to "false","showDeleted" to "false","showAssigned" to "true",
                "dueMin" to monday.atStartOfDay(ZoneOffset.UTC).toInstant().toString(),
                "dueMax" to monday.plusDays(7).atStartOfDay(ZoneOffset.UTC).toInstant().toString()))
            limited=limited || result.limited
            result.items.forEach { item -> GoogleAgendaData.task(item,monday,id)?.let { entries[it.key]=it.event } }
        }
        return GoogleAgendaData.merge(listOf(CalendarProtocol.Week(events=entries.values.toList(),truncated=limited,holidayDays=holidayDays)))
    }
    companion object {
        fun selected(prefs: SharedPreferences)=prefs.getBoolean("google_birthdays",false) ||
            !prefs.getStringSet("google_calendars",emptySet()).isNullOrEmpty() || !prefs.getStringSet("google_tasklists",emptySet()).isNullOrEmpty()
    }
}
