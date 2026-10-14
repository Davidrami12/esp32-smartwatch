package dev.esp32.smartwatch.ble

import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.util.UUID
import java.util.zip.CRC32

/** Independent bounded binary transport, not the notification text framing. */
object VoiceProtocol {
    val META: UUID = UUID.fromString("7e57000f-8e7a-4b6d-9f2a-6c4d3e2f1000")
    val DATA: UUID = UUID.fromString("7e570010-8e7a-4b6d-9f2a-6c4d3e2f1000")
    val CONTROL: UUID = UUID.fromString("7e570011-8e7a-4b6d-9f2a-6c4d3e2f1000")
    val RESULT: UUID = UUID.fromString("7e570012-8e7a-4b6d-9f2a-6c4d3e2f1000")
    val CAPS: UUID = UUID.fromString("7e570013-8e7a-4b6d-9f2a-6c4d3e2f1000")
    val STREAM: UUID = UUID.fromString("7e570014-8e7a-4b6d-9f2a-6c4d3e2f1000")
    fun supportsStream(bytes: ByteArray) = bytes.contentEquals(byteArrayOf(2,1,4,15,0,2))
    fun shouldYieldVoice(active: Boolean, alreadyYielded: Boolean, queueSize: Int, next: UUID?) =
        active && !alreadyYielded && queueSize == 1 && (next == CONTROL || next == DATA)
    fun grant(session: Int, epoch: Int, offset: Int): ByteArray = ByteBuffer.allocate(15)
        .order(ByteOrder.LITTLE_ENDIAN).put(2.toByte()).put(4.toByte()).putInt(session)
        .putInt(epoch).putInt(offset).put(4.toByte()).array()
    fun shouldPollMetadata(transferActive: Boolean, pending: UUID?, queued: Iterable<UUID>) =
        !transferActive && pending != META && queued.none { it == META }
    data class Metadata(val session: Int, val size: Int, val crc: Long)
    fun metadata(bytes: ByteArray): Metadata? {
        require(bytes.size == 18 && bytes[0].toInt() == 1)
        if (bytes[1].toInt() == 0) return null
        require(bytes[1].toInt() in 1..2)
        val b = ByteBuffer.wrap(bytes).order(ByteOrder.LITTLE_ENDIAN)
        val session = b.getInt(2); val size = b.getInt(6)
        require(session != 0 && size in 8000..256000 && size % 2 == 0)
        require(b.getShort(14).toInt() == 16000 && bytes[16].toInt() == 16 && bytes[17].toInt() == 1)
        return Metadata(session, size, b.getInt(10).toLong() and 0xffffffffL)
    }
    fun control(session: Int, offset: Int, finish: Boolean = false): ByteArray =
        ByteBuffer.allocate(10).order(ByteOrder.LITTLE_ENDIAN).put(1.toByte())
            .put(if (finish) 2.toByte() else 1.toByte()).putInt(session).putInt(offset).array()
    fun process(session: Int, size: Int): ByteArray = control(session,size,true).apply { this[1] = 3 }
    fun resultFrames(session: Int, kind: Int, text: String, mtu: Int): List<ByteArray> {
        val bytes = text.toByteArray(Charsets.UTF_8)
        require(kind in 1..3 && bytes.isNotEmpty() && bytes.size <= if (kind == 1) 512 else if (kind == 2) 3072 else 160)
        val chunk = minOf(244,mtu-3)-10; require(chunk > 0)
        return (bytes.indices step chunk).map { offset ->
            val count = minOf(chunk,bytes.size-offset)
            ByteBuffer.allocate(10+count).order(ByteOrder.LITTLE_ENDIAN).put(1.toByte()).put(kind.toByte())
                .putInt(session).putShort(offset.toShort()).putShort(bytes.size.toShort()).put(bytes,offset,count).array()
        }
    }
    class Receiver(val metadata: Metadata) {
        private val audio = ByteArray(metadata.size)
        var active = true; private set
        private var verified = false
        var offset = 0; private set
        fun accept(frame: ByteArray): Boolean {
            require(frame.size in 11..512 && (frame.size - 9) % 2 == 0 && frame[0].toInt() == 1)
            val b = ByteBuffer.wrap(frame).order(ByteOrder.LITTLE_ENDIAN)
            return acceptPayload(frame,9,b.getInt(1),b.getInt(5))
        }
        internal fun acceptPayload(frame: ByteArray, header: Int, session: Int, at: Int): Boolean {
            require(active) { "Cancelled voice receiver" }
            require(session == metadata.session && at == offset)
            require(offset + frame.size - header <= audio.size)
            frame.copyInto(audio, offset, header); offset += frame.size - header
            if (offset == audio.size) {
                verified = CRC32().apply { update(audio) }.value == metadata.crc
                require(verified)
            }
            return offset == audio.size
        }
        fun clear() { audio.fill(0); offset = 0; active = false; verified = false }
        fun takeAudio(): ByteArray { require(active && verified && offset == audio.size); return audio.copyOf() }
    }
    /** No out-of-order audio buffer: acknowledge only the verified contiguous prefix. */
    class StreamReceiver(val receiver: Receiver, mtu: Int) {
        enum class Event { IGNORE, GAP, PROGRESS, WINDOW, COMPLETE }
        private val payload = (minOf(512,mtu-3)-15) and -2
        var epoch = 0; private set
        private var sequence = 0
        private var retries = 0
        private var retryOffset = -1
        init { require(mtu in 64..517 && payload > 0) }
        fun nextGrant(retry: Boolean = false): ByteArray {
            require(receiver.active && receiver.offset < receiver.metadata.size)
            if (retry) {
                if (retryOffset != receiver.offset) { retryOffset = receiver.offset; retries = 0 }
                require(++retries <= 3) { "Voice notification retries exhausted" }
            } else { retries = 0; retryOffset = receiver.offset }
            require(epoch < Int.MAX_VALUE)
            epoch++; sequence = 0
            return grant(receiver.metadata.session,epoch,receiver.offset)
        }
        fun accept(frame: ByteArray): Event {
            require(frame.size in 17..(payload+15) && frame[0].toInt() == 2 && (frame.size-15)%2 == 0)
            val b = ByteBuffer.wrap(frame).order(ByteOrder.LITTLE_ENDIAN)
            if (b.getInt(1) != receiver.metadata.session || b.getInt(5) != epoch) return Event.IGNORE
            val at = b.getInt(9); val seq = frame[13].toInt() and 255
            val flags = frame[14].toInt() and 255
            require(seq < 4 && flags in 0..1)
            if (at < receiver.offset && seq < sequence) return Event.IGNORE
            if (at != receiver.offset || seq != sequence) return Event.GAP
            require(frame.size-15 == minOf(payload,receiver.metadata.size-at))
            val last = seq == 3 || at+frame.size-15 == receiver.metadata.size
            require((flags == 1) == last)
            val complete = receiver.acceptPayload(frame,15,b.getInt(1),at)
            sequence++
            return if (complete) Event.COMPLETE else if (last) Event.WINDOW else Event.PROGRESS
        }
    }
}
