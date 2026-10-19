package dev.esp32.smartwatch.ble

import org.junit.Assert.*
import org.junit.Test
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.util.zip.CRC32

class VoiceProtocolTest {
    @Test fun assistantErrorsAreDistinctBoundedTagsWithoutUpstreamDetails() {
        val cases = mapOf(
            "Enable paid AI before recording a new question" to "AI_DISABLED",
            "Paid AI consent revoked. Hold to retry" to "AI_DISABLED",
            "Sign in to AI on your phone" to "AI_SIGN_IN",
            "Configure AI and sign in on phone" to "AI_SIGN_IN",
            "AI request timed out. Hold to retry" to "AI_TIMEOUT",
            "AI request limit reached" to "AI_LIMIT",
            "AI service unavailable" to "AI_SERVICE",
            "AI request cancelled" to "AI_CANCELLED",
            "Audio transfer verified. AI paused for testing" to "AI_TEST_MODE",
            "private upstream error / token=must-not-be-forwarded" to "AI_CONNECTION"
        )
        cases.forEach { (message,tag) -> assertEquals(tag,VoiceProtocol.watchError(message)) }
        assertEquals("AI_CONNECTION",VoiceProtocol.watchError(null))
        assertEquals("AI_DISABLED",VoiceProtocol.watchError("ENABLE PAID AI"))
    }
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
