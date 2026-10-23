package dev.esp32.smartwatch.calendar

import android.Manifest
import android.content.ContentUris
import android.content.Context
import android.content.SharedPreferences
import android.content.pm.PackageManager
import android.database.ContentObserver
import android.os.Handler
import android.os.Looper
import android.provider.CalendarContract
import java.time.Instant
import java.time.LocalDate
import java.time.ZoneId
import java.time.ZoneOffset
import java.time.format.DateTimeFormatter
import java.util.Locale
import java.util.concurrent.Executors

/** Local provider + explicitly authorized Google sources. Never sends agenda data to AI/backend. */
class CalendarBridge(private val context: Context, private val changed: () -> Unit) {
    private val main = Handler(Looper.getMainLooper())
    private val worker = Executors.newSingleThreadExecutor()
    private val prefs = preferences(context)
    @Volatile private var generation = 0
    private var observing = false
    private val refresh = Runnable { changed() }
    private fun schedule() { main.removeCallbacks(refresh); main.postDelayed(refresh, 500) }
    private val observer = object : ContentObserver(main) { override fun onChange(selfChange: Boolean) { schedule() } }
    private val listener = SharedPreferences.OnSharedPreferenceChangeListener { _, _ ->
        generation++; main.removeCallbacks(refresh); changed() // Privacy changes are not debounced.
    }
    init { prefs.registerOnSharedPreferenceChangeListener(listener) }
    fun invalidate() { generation++; main.removeCallbacks(refresh) }
    fun allowed() = prefs.getBoolean("enabled", false) &&
        (if (!prefs.getStringSet("selected",emptySet()).isNullOrEmpty()) permission(context) else GoogleAgendaClient.selected(prefs))
    fun query(request: CalendarProtocol.Request, done: (CalendarProtocol.Week) -> Unit) {
        val ticket = ++generation
        val enabled = prefs.getBoolean("enabled", false)
        val permitted = permission(context)
        if (enabled && permitted && !observing) {
            runCatching { context.contentResolver.registerContentObserver(CalendarContract.CONTENT_URI, true, observer); observing = true }
        } else if ((!enabled || !permitted) && observing) {
            context.contentResolver.unregisterContentObserver(observer); observing = false
        }
        val selected = prefs.getStringSet("selected", emptySet()).orEmpty().mapNotNull { it.toLongOrNull() }.take(32)
        val google=GoogleAgendaClient.selected(prefs)
        worker.execute {
            if (ticket != generation) return@execute
            val result = when {
                !enabled -> CalendarProtocol.Week(status = 1)
                !permitted && selected.isNotEmpty() -> CalendarProtocol.Week(status = 2)
                selected.isEmpty() && !google -> CalendarProtocol.Week(status = 3)
                else -> try {
                    val localNames=mutableSetOf<String>()
                    val local=if (selected.isEmpty()) CalendarProtocol.Week() else load(request.monday,selected,localNames)
                    if (google) {
                        val account=prefs.getString("google_account",null) ?: throw GoogleAgendaConsentRequired()
                        val remote=GoogleAgendaClient(context,account) { ticket==generation && prefs.getBoolean("enabled",false) }
                            .week(request.monday,prefs,localNames)
                        GoogleAgendaData.merge(listOf(local,remote))
                    } else local
                }
                    catch (_: GoogleAgendaConsentRequired) { CalendarProtocol.Week(status = 5) }
                    catch (_: SecurityException) { CalendarProtocol.Week(status = 2) }
                    catch (_: Exception) { CalendarProtocol.Week(status = 4) }
            }
            main.post { if (ticket == generation) done(result) }
        }
    }
    private fun load(monday: LocalDate, selected: List<Long>,localNames: MutableSet<String>): CalendarProtocol.Week {
        val holidayCalendars = mutableSetOf<Long>()
        context.contentResolver.query(CalendarContract.Calendars.CONTENT_URI,
            arrayOf(CalendarContract.Calendars._ID,CalendarContract.Calendars.CALENDAR_DISPLAY_NAME,CalendarContract.Calendars.NAME),
            "_id IN (${selected.joinToString(",") { "?" }})",selected.map { it.toString() }.toTypedArray(),null)?.use { c ->
            while (c.moveToNext()) {
                val id=c.getLong(0)
                c.getString(2)?.takeIf { it.isNotBlank() }?.let(localNames::add)
                val suggested=CalendarProtocol.suggestedHolidayCalendar(c.getString(1).orEmpty(),c.getString(2).orEmpty())
                if (prefs.getBoolean("holiday_$id",suggested)) holidayCalendars.add(id)
            }
        } ?: return CalendarProtocol.Week(status = 4)
        val zone = ZoneId.of("Europe/Madrid")
        val next = monday.plusDays(7)
        val uri = CalendarContract.Instances.CONTENT_URI.buildUpon().also {
            ContentUris.appendId(it, monday.atStartOfDay(zone).toInstant().minusSeconds(86400).toEpochMilli())
            ContentUris.appendId(it, next.atStartOfDay(zone).toInstant().plusSeconds(86400).toEpochMilli())
        }.build()
        val projection = arrayOf(CalendarContract.Instances.BEGIN, CalendarContract.Instances.END,
            CalendarContract.Instances.ALL_DAY, CalendarContract.Instances.TITLE, CalendarContract.Instances.EVENT_LOCATION,
            CalendarContract.Instances.DISPLAY_COLOR,CalendarContract.Instances.CALENDAR_ID)
        val selection = "calendar_id IN (${selected.joinToString(",") { "?" }}) AND deleted=0 AND (eventStatus IS NULL OR eventStatus!=2) AND (selfAttendeeStatus IS NULL OR selfAttendeeStatus!=2)"
        val events = mutableListOf<CalendarProtocol.Event>()
        var truncated = false
        var holidayDays = 0
        val time = DateTimeFormatter.ofPattern("HH:mm", Locale.UK)
        val dateTime = DateTimeFormatter.ofPattern("dd/MM HH:mm", Locale.UK)
        val date = DateTimeFormatter.ofPattern("dd/MM", Locale.UK)
        context.contentResolver.query(uri, projection, selection, selected.map { it.toString() }.toTypedArray(),
            "begin ASC, allDay DESC, event_id ASC")?.use { c ->
            var rows = 0
            while (c.moveToNext()) {
                if (++rows > 2048) { truncated = true; break }
                val begin = Instant.ofEpochMilli(c.getLong(0)); val end = Instant.ofEpochMilli(c.getLong(1))
                if (end < begin) continue
                val allDay = c.getInt(2) != 0
                // Android stores all-day date boundaries in UTC: do not shift them into Madrid.
                val first = begin.atZone(if (allDay) ZoneOffset.UTC else zone).toLocalDate()
                val last = (if (end > begin) end.minusMillis(1) else begin).atZone(if (allDay) ZoneOffset.UTC else zone).toLocalDate()
                val mask = CalendarProtocol.dayMask(monday, begin.toEpochMilli(), end.toEpochMilli(), allDay)
                if (mask == 0) continue
                if (c.getLong(6) in holidayCalendars) holidayDays = holidayDays or mask
                // Continue the bounded scan so a busy personal calendar cannot
                // hide holiday dates merely by filling the 32 agenda slots.
                if (events.size == CalendarProtocol.MAX_EVENTS) { truncated = true; continue }
                val startLocal = begin.atZone(zone); val endLocal = end.atZone(zone)
                val label = when {
                    allDay && first == last -> "All day"
                    allDay -> "All day / ${first.format(date)} - ${last.format(date)}"
                    startLocal.toLocalDate() == endLocal.toLocalDate() -> "${startLocal.format(time)} - ${endLocal.format(time)}"
                    else -> "${startLocal.format(dateTime)} - ${endLocal.format(dateTime)}"
                }
                val color = if (c.isNull(5)) 0x78B8FF else c.getInt(5) and 0xffffff
                events.add(CalendarProtocol.Event(mask, allDay, c.getString(3).orEmpty(), c.getString(4).orEmpty(), label, color,begin.toEpochMilli()))
            }
        } ?: return CalendarProtocol.Week(status = 4)
        return CalendarProtocol.Week(events = events, truncated = truncated, holidayDays = holidayDays)
    }
    companion object {
        data class Choice(val id: Long, val label: String, val holidaySuggested: Boolean = false)
        fun preferences(context: Context): SharedPreferences = context.getSharedPreferences("calendar", Context.MODE_PRIVATE)
        fun permission(context: Context) = context.checkSelfPermission(Manifest.permission.READ_CALENDAR) == PackageManager.PERMISSION_GRANTED
        fun calendars(context: Context): List<Choice> {
            if (!permission(context)) return emptyList()
            val result = mutableListOf<Choice>()
            context.contentResolver.query(CalendarContract.Calendars.CONTENT_URI,
                arrayOf(CalendarContract.Calendars._ID, CalendarContract.Calendars.CALENDAR_DISPLAY_NAME, CalendarContract.Calendars.ACCOUNT_NAME,CalendarContract.Calendars.NAME,CalendarContract.Calendars.SYNC_EVENTS),
                null, null, "calendar_displayName ASC")?.use { c ->
                while (c.moveToNext() && result.size < 256) result.add(Choice(c.getLong(0), "${c.getString(1).orEmpty()} · ${c.getString(2).orEmpty()}${if (c.getInt(4)==0) " · sync off / cached events only" else ""}",
                    CalendarProtocol.suggestedHolidayCalendar(c.getString(1).orEmpty(),c.getString(3).orEmpty())))
            }
            return result
        }
    }
}
