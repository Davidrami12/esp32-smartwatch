package dev.esp32.smartwatch.ble

import org.junit.Assert.*
import org.junit.Test
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.util.zip.CRC32

class VoiceProtocolTest {
    @Test fun completeAndClear() {
        val audio = ByteArray(8000) { it.toByte() }
        val receiver = VoiceProtocol.Receiver(VoiceProtocol.Metadata(123, audio.size, CRC32().apply { update(audio) }.value))
        while (receiver.offset < audio.size) {
            val offset = receiver.offset
            val count = minOf(234, audio.size - offset)
            val frame = ByteBuffer.allocate(9 + count).order(ByteOrder.LITTLE_ENDIAN)
                .put(1.toByte()).putInt(123).putInt(offset).put(audio, offset, count).array()
            assertEquals(offset + count == audio.size, receiver.accept(frame))
        }
        receiver.clear(); assertEquals(0, receiver.offset)
    }
    @Test fun rejectWrongOffsetAndSession() {
        val receiver = VoiceProtocol.Receiver(VoiceProtocol.Metadata(123, 8000, 0))
        for ((session, offset) in listOf(124 to 0, 123 to 2)) {
            val frame = ByteBuffer.allocate(11).order(ByteOrder.LITTLE_ENDIAN).put(1.toByte()).putInt(session).putInt(offset).putShort(0).array()
            assertThrows(IllegalArgumentException::class.java) { receiver.accept(frame) }
        }
    }
    @Test fun metadataBounds() {
        val bytes = ByteArray(18); bytes[0] = 1
        assertNull(VoiceProtocol.metadata(bytes))
        bytes[1] = 1
        assertThrows(IllegalArgumentException::class.java) { VoiceProtocol.metadata(bytes) }
    }
}
