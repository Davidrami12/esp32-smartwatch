package dev.esp32.smartwatch.ble

import org.junit.Assert.*
import org.junit.Test
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.util.zip.CRC32

class VoiceLargeReadTest {
    @Test fun spanishAccentsAndPunctuationSurviveTypographyNormalization() {
        assertEquals("¿Qué tal, España? ¡Sí! \"áéíóúüñ\"...",
            dev.esp32.smartwatch.ai.VoiceAiClient.bounded("¿Qué tal, España? ¡Sí! “a\u0301éíóúüñ”…",512))
    }
    @Test fun largerAndLegacyReadsPreserveWholeClipCrc() {
        for (payload in listOf(234,502)) {
            val audio = ByteArray(8000) { (it % 127).toByte() }
            val receiver = VoiceProtocol.Receiver(VoiceProtocol.Metadata(7,audio.size,CRC32().apply { update(audio) }.value))
            for (offset in audio.indices step payload) {
                val count = minOf(payload,audio.size-offset)
                val frame = ByteBuffer.allocate(count+9).order(ByteOrder.LITTLE_ENDIAN)
                    .put(1.toByte()).putInt(7).putInt(offset).put(audio,offset,count).array()
                assertEquals(offset+count == audio.size,receiver.accept(frame))
            }
            assertArrayEquals(audio,receiver.takeAudio()); receiver.clear()
        }
    }
}
