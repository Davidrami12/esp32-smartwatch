package dev.esp32.smartwatch.ai

import dev.esp32.smartwatch.BuildConfig
import org.junit.Assert.*
import org.junit.Test
import java.util.concurrent.CountDownLatch
import java.util.concurrent.TimeUnit

class VoiceTransferOnlyTest {
    @Test fun productionBuildRequiresLoginAndConsentAndDiscardsRejectedAudio() {
        assertFalse(BuildConfig.VOICE_TRANSFER_ONLY)
        VoiceAiClient.signOut()
        VoiceAiClient.enablePaidRequests(true)
        assertFalse(VoiceAiClient.paidRequestsEnabled)
        val audio = ByteArray(8000) { 42 }
        val done = CountDownLatch(1)
        var result: VoiceAiClient.Result? = null
        var error: String? = null
        val diagnostics = java.util.concurrent.CopyOnWriteArrayList<String>()
        VoiceAiClient.ask(audio,0x1234abcd,{ diagnostics.add(it) }) { value, failure -> result = value; error = failure; done.countDown() }
        assertTrue(done.await(5,TimeUnit.SECONDS))
        assertNull(result)
        assertEquals("Configure AI and sign in on phone",error)
        assertTrue(audio.all { it == 0.toByte() })
        assertTrue(diagnostics.isNotEmpty())
        assertTrue(diagnostics.all { it.matches(Regex("VoiceLatency trace=1234abcd stage=executor_queue duration_us=[0-9]+")) })
    }
}
