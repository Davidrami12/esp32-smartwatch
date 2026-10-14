package dev.esp32.smartwatch.ble

import org.junit.Assert.*
import org.junit.Test
import java.util.UUID

class VoicePollTest {
    @Test fun busyNonVoiceQueueDoesNotSuppressCaptureDiscovery() {
        val traffic = UUID.fromString("7e57000a-8e7a-4b6d-9f2a-6c4d3e2f1000")
        assertTrue(VoiceProtocol.shouldPollMetadata(false,traffic,listOf(traffic,traffic)))
        assertTrue(VoiceProtocol.shouldPollMetadata(false,null,emptyList()))
    }
    @Test fun metadataPollsAreNotDuplicated() {
        assertFalse(VoiceProtocol.shouldPollMetadata(false,VoiceProtocol.META,emptyList()))
        assertFalse(VoiceProtocol.shouldPollMetadata(false,null,listOf(VoiceProtocol.META)))
    }
    @Test fun activeAudioTransferIsNotPolled() {
        assertFalse(VoiceProtocol.shouldPollMetadata(true,VoiceProtocol.DATA,emptyList()))
    }
}
