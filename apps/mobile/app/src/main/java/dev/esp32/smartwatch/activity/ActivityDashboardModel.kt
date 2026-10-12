package dev.esp32.smartwatch.activity

import java.io.*
import java.time.*

data class ActivityCache(val snapshot: WatchActivitySnapshot, val updatedAt: Long,
                         val goal: Int?, val goalUpdatedAt: Long?)
data class ActivityBar(val date: LocalDate, val steps: Long, val recorded: Boolean)
data class ActivitySummary(val today: LocalDate, val todaySteps: Long, val todayRecorded: Boolean,
                           val goal: Int?, val percentage: Double?, val remaining: Long?,
                           val visualProgress: Float, val bars: List<ActivityBar>, val total: Long, val average: Double)
data class ActivityDashboardState(val cache: ActivityCache? = null, val live: Boolean = false,
                                  val summary: ActivitySummary? = null, val error: String? = null)

object ActivityDashboardModel {
    val WATCH_ZONE: ZoneId = ZoneId.of("Europe/Madrid")
    fun summary(cache: ActivityCache, now: Instant): ActivitySummary {
        val today = now.atZone(WATCH_ZONE).toLocalDate()
        val records = cache.snapshot.days.associateBy { it.date }
        val bars = (6L downTo 0L).map { days ->
            val date = today.minusDays(days)
            ActivityBar(date, records[date]?.count ?: 0, records.containsKey(date))
        }
        val count = bars.last().steps
        val goal = cache.goal
        val percentage = goal?.let { count.toDouble() / it * 100 }
        val total = bars.sumOf { it.steps }
        return ActivitySummary(today, count, bars.last().recorded, goal, percentage,
            goal?.let { maxOf(0L, it.toLong()-count) }, ((percentage ?: 0.0)/100).coerceIn(0.0,1.0).toFloat(),
            bars, total, total / 7.0)
    }
    fun isLive(cache: ActivityCache, secureConnected: Boolean, latest: WatchActivitySnapshot?,
               now: Instant, readError: String?): Boolean = secureConnected && readError == null &&
        latest?.source == cache.snapshot.source && cache.updatedAt <= now.toEpochMilli() &&
        now.toEpochMilli()-cache.updatedAt < 90_000 && summary(cache, now).todayRecorded
}

/** Bounded versioned display cache, using the same validated BLE snapshot codec. */
object ActivityCacheCodec {
    fun encode(cache: ActivityCache): ByteArray = ByteArrayOutputStream().use { buffer ->
        DataOutputStream(buffer).use { out ->
            out.writeByte(1); out.writeLong(cache.updatedAt); out.writeInt(cache.goal ?: -1)
            out.writeLong(cache.goalUpdatedAt ?: 0); out.writeUTF(cache.snapshot.source)
            val payload = WatchActivityProtocol.encode(cache.snapshot)
            out.writeShort(payload.size); out.write(payload)
        }
        buffer.toByteArray()
    }
    fun decode(bytes: ByteArray): ActivityCache {
        require(bytes.size <= 512)
        return DataInputStream(ByteArrayInputStream(bytes)).use { input ->
            require(input.readUnsignedByte() == 1)
            val updated = input.readLong().also { require(it > 0) }
            val goal = input.readInt().also { require(it == -1 || (it in 1000..30000 && it % 1000 == 0)) }.takeIf { it != -1 }
            val goalTime = input.readLong().also { require(it >= 0) }.takeIf { it > 0 }
            val source = input.readUTF().also { require(it.length in 1..64) }
            val size = input.readUnsignedShort().also { require(it in 2..170 && it == input.available()) }
            val payload = ByteArray(size); input.readFully(payload)
            val snapshot = WatchActivityProtocol.decode(source,payload)
            require(snapshot.days.isNotEmpty())
            ActivityCache(snapshot,updated,goal,goalTime)
        }
    }
}
