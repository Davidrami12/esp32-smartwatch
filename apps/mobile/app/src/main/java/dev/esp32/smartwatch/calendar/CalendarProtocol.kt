package dev.esp32.smartwatch.calendar

import java.io.ByteArrayOutputStream
import java.time.LocalDate
import java.util.UUID
import java.util.zip.CRC32

object CalendarProtocol {
    val UUID: UUID = java.util.UUID.fromString("7e570015-8e7a-4b6d-9f2a-6c4d3e2f1000")
    const val MAX_EVENTS = 32
    data class Request(val id: Int, val monday: LocalDate, val version: Int = 1)
    data class Event(val days: Int, val allDay: Boolean, val title: String, val location: String, val time: String, val color: Int = 0x78B8FF, val startMillis: Long = 0)
    data class Week(val status: Int = 0, val events: List<Event> = emptyList(), val truncated: Boolean = false,
                    val fetchedAt: Long = System.currentTimeMillis() / 1000, val holidayDays: Int = 0)
    fun request(value: ByteArray): Request? {
        require(value.size == 10 && value[0].toInt() in 1..4 && value[1].toInt() == 0)
        fun le(at: Int) = (0..3).fold(0) { n, i -> n or ((value[at+i].toInt() and 255) shl (8*i)) }
        val id = le(2); if (id == 0) return null
        val date = le(6)
        val monday = LocalDate.of(date / 10000, date / 100 % 100, date % 100)
        require(monday.year in 2020..2099 && monday.dayOfWeek.value == 1)
        return Request(id, monday, value[0].toInt())
    }
    fun text(value: String, maxBytes: Int): ByteArray {
        val out = ByteArrayOutputStream()
        val points = value.codePoints().iterator()
        while (points.hasNext()) {
            val cp = points.nextInt()
            val clean = if (Character.isISOControl(cp)) ' '.code else cp
            val bytes = String(Character.toChars(clean)).toByteArray(Charsets.UTF_8)
            if (out.size() + bytes.size > maxBytes) break
            out.write(bytes)
        }
        return out.toByteArray()
    }
    fun dayMask(monday: LocalDate, beginMillis: Long, endMillis: Long, allDay: Boolean): Int {
        require(endMillis >= beginMillis)
        val zone = if (allDay) java.time.ZoneOffset.UTC else java.time.ZoneId.of("Europe/Madrid")
        val first = java.time.Instant.ofEpochMilli(beginMillis).atZone(zone).toLocalDate()
        val last = java.time.Instant.ofEpochMilli(if (endMillis > beginMillis) endMillis - 1 else beginMillis).atZone(zone).toLocalDate()
        var mask = 0
        repeat(7) { d -> val day = monday.plusDays(d.toLong()); if (!day.isBefore(first) && !day.isAfter(last)) mask = mask or (1 shl d) }
        return mask
    }
    fun frames(request: Request, week: Week, mtu: Int): List<ByteArray> {
        require(week.status in 0..5 && week.events.size <= MAX_EVENTS && (week.status == 0 || week.events.isEmpty()))
        require(week.holidayDays in 0..127 && (week.status == 0 || week.holidayDays == 0))
        val out = ByteArrayOutputStream()
        fun le(value: Long, bytes: Int) { repeat(bytes) { out.write((value shr (8*it)).toInt() and 255) } }
        require(request.version in 1..4)
        out.write(if (week.status==5 && request.version<4) 2 else week.status)
        out.write(week.events.size); out.write(if (week.truncated) 1 else 0); out.write(minOf(request.version-1,2))
        le(week.fetchedAt, 4)
        if (request.version >= 3) out.write(week.holidayDays)
        week.events.forEach { e ->
            require(e.days in 1..127)
            val title = text(e.title.ifBlank { "Untitled event" }, 80)
            val location = text(e.location, 80); val time = text(e.time, 40)
            require(title.isNotEmpty() && time.isNotEmpty())
            out.write(e.days); out.write(if (e.allDay) 1 else 0)
            out.write(title.size); out.write(location.size); out.write(time.size)
            if (request.version >= 2) le((e.color and 0xffffff).toLong(), 3)
            out.write(title); out.write(location); out.write(time)
        }
        le(CRC32().apply { update(out.toByteArray()) }.value, 4)
        val payload = out.toByteArray(); require(payload.size <= 7000)
        val size = (mtu - 3).coerceIn(20, 244) - 10
        return payload.indices.step(size).map { offset ->
            ByteArray(10 + minOf(size, payload.size - offset)).also { frame ->
                frame[0] = 1; frame[1] = 1
                repeat(4) { frame[2+it] = (request.id ushr (8*it)).toByte() }
                repeat(2) { frame[6+it] = (offset ushr (8*it)).toByte(); frame[8+it] = (payload.size ushr (8*it)).toByte() }
                payload.copyInto(frame, 10, offset, offset + frame.size - 10)
            }
        }
    }
    // Calendar metadata only, never event-title guessing or invented holiday dates.
    fun suggestedHolidayCalendar(displayName: String, name: String = ""): Boolean {
        fun normalize(s: String) = java.text.Normalizer.normalize(s,java.text.Normalizer.Form.NFD)
            .replace(Regex("\\p{M}+"), "").lowercase(java.util.Locale.ROOT)
        val text = normalize("$displayName $name")
        val region = Regex("\\b(espana|spain|spanish|madrid)\\b").containsMatchIn(text)
        val holidays = Regex("\\b(festivos?|festividades|fiestas?|holidays?|laboral)\\b").containsMatchIn(text)
        return region && holidays
    }
}
