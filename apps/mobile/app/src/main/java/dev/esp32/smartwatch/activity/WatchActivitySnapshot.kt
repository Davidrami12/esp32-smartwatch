package dev.esp32.smartwatch.activity

import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.time.Instant
import java.time.LocalDate
import java.time.ZoneOffset

/** Watch-owned daily aggregate boundaries, not individual walking timestamps. */
data class WatchActivityDay(val date: LocalDate, val count: Long, val start: Instant, val end: Instant,
                            val startOffset: ZoneOffset, val endOffset: ZoneOffset)
data class WatchActivitySnapshot(val source: String, val days: List<WatchActivityDay>)

object WatchActivityProtocol {
    fun encode(snapshot: WatchActivitySnapshot): ByteArray {
        require(snapshot.days.size <= 7)
        val bytes = ByteBuffer.allocate(2 + snapshot.days.size * 24).order(ByteOrder.LITTLE_ENDIAN)
        bytes.put(1).put(snapshot.days.size.toByte())
        snapshot.days.forEach { day ->
            bytes.putInt(day.date.year*10000 + day.date.monthValue*100 + day.date.dayOfMonth)
                .putInt(day.count.toInt()).putInt(day.start.epochSecond.toInt()).putInt(day.end.epochSecond.toInt())
                .putInt(day.startOffset.totalSeconds).putInt(day.endOffset.totalSeconds)
        }
        return bytes.array()
    }
    fun decode(source: String, bytes: ByteArray): WatchActivitySnapshot {
        require(source.isNotBlank() && bytes.size >= 2 && bytes[0].toInt() == 1)
        val count = bytes[1].toInt() and 255
        require(count <= 7 && bytes.size == 2 + count * 24)
        val data = ByteBuffer.wrap(bytes, 2, bytes.size - 2).order(ByteOrder.LITTLE_ENDIAN)
        val days = List(count) {
            val encoded = data.int.toLong() and 0xffffffffL
            val date = LocalDate.of((encoded / 10000).toInt(), ((encoded / 100) % 100).toInt(), (encoded % 100).toInt())
            require(date.year in 2000..2099)
            val steps = data.int.toLong() and 0xffffffffL
            val start = Instant.ofEpochSecond(data.int.toLong() and 0xffffffffL)
            val end = Instant.ofEpochSecond(data.int.toLong() and 0xffffffffL)
            val startOffset = ZoneOffset.ofTotalSeconds(data.int)
            val endOffset = ZoneOffset.ofTotalSeconds(data.int)
            require(start < end && end.epochSecond-start.epochSecond <= 26*3600)
            require(start.atOffset(startOffset).toLocalDate() == date && start.atOffset(startOffset).toLocalTime() == java.time.LocalTime.MIDNIGHT)
            val endLocal = end.atOffset(endOffset)
            require(endLocal.toLocalDate() == date || (endLocal.toLocalDate() == date.plusDays(1) && endLocal.toLocalTime() == java.time.LocalTime.MIDNIGHT))
            WatchActivityDay(date, steps, start, end, startOffset, endOffset)
        }
        require(days.zipWithNext().all { (a,b) -> a.date > b.date && b.end <= a.start })
        require(days.isEmpty() || days.last().date >= days.first().date.minusDays(6))
        return WatchActivitySnapshot(source, days)
    }
}
