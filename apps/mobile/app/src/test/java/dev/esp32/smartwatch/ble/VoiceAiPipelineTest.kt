package dev.esp32.smartwatch.ble

import dev.esp32.smartwatch.ai.VoiceAiClient
import org.junit.Assert.*
import org.junit.Test
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.util.zip.CRC32

class VoiceAiPipelineTest {
    @Test fun mockEndToEndAudioToSpanishResultFrames() {
        val audio = ByteArray(16000) { (it % 127).toByte() }
        val receiver = VoiceProtocol.Receiver(VoiceProtocol.Metadata(77,audio.size,CRC32().apply { update(audio) }.value))
        for (offset in audio.indices step 234) {
            val size = minOf(234,audio.size-offset)
            receiver.accept(ByteBuffer.allocate(9+size).order(ByteOrder.LITTLE_ENDIAN).put(1.toByte()).putInt(77).putInt(offset).put(audio,offset,size).array())
        }
        val captured = receiver.takeAudio(); receiver.clear()
        val wav = VoiceAiClient.wav(captured)
        assertEquals(16044,wav.size)
        assertEquals(16000,ByteBuffer.wrap(wav).order(ByteOrder.LITTLE_ENDIAN).getInt(24))
        val mock = VoiceAiClient.Result("¿Cómo estás?","Estoy bien. ¿En qué puedo ayudarte?")
        for ((kind,text) in listOf(1 to mock.transcription,2 to mock.answer)) {
            val output = java.io.ByteArrayOutputStream()
            VoiceProtocol.resultFrames(77,kind,text,23).forEach { frame ->
                assertTrue(frame.size <= 20)
                val header = ByteBuffer.wrap(frame).order(ByteOrder.LITTLE_ENDIAN)
                assertEquals(77,header.getInt(2)); assertEquals(output.size(),header.getShort(6).toInt())
                output.write(frame,10,frame.size-10)
            }
            assertEquals(text,output.toByteArray().toString(Charsets.UTF_8))
        }
        captured.fill(0); wav.fill(0)
    }
    @Test fun truncationPreservesSpanishUnicode() {
        assertEquals("á",VoiceAiClient.bounded("a\u0301á",3))
        assertEquals(3072,VoiceAiClient.bounded("á".repeat(3000),3072).toByteArray().size)
    }
    @Test fun invalidResultBoundsRejected() {
        assertThrows(IllegalArgumentException::class.java) { VoiceProtocol.resultFrames(77,2,"a".repeat(3073),256) }
    }
    @Test fun cancellationMarksInFlightCall() {
        val call = VoiceAiClient.Call(); call.cancel(); assertTrue(call.cancelled)
    }
}
