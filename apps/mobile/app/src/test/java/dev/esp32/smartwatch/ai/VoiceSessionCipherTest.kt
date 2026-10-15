package dev.esp32.smartwatch.ai

import javax.crypto.KeyGenerator
import org.junit.Assert.*
import org.junit.Test

class VoiceSessionCipherTest {
    private fun key() = KeyGenerator.getInstance("AES").apply { init(256) }.generateKey()
    private val session = SavedVoiceSession("test-refresh-credential","test-public-key","test-uid")
    @Test fun encryptedSessionRoundTripUsesFreshIvAndContainsNoPlaintextCredential() {
        val key = key()
        val first = VoiceSessionCipher.seal(session,key)
        val second = VoiceSessionCipher.seal(session,key)
        assertFalse(first.contentEquals(second))
        assertFalse(first.toString(Charsets.ISO_8859_1).contains(session.refresh))
        assertEquals(session,VoiceSessionCipher.open(first,key))
    }
    @Test fun corruptionWrongKeyUnsupportedVersionAndTruncationFailClosed() {
        val key = key(); val sealed = VoiceSessionCipher.seal(session,key)
        assertThrows(Exception::class.java) { VoiceSessionCipher.open(sealed,key()) }
        for (index in listOf(1,12,13,sealed.lastIndex)) {
            val corrupt = sealed.copyOf().apply { this[index] = (this[index].toInt() xor 1).toByte() }
            assertThrows(Exception::class.java) { VoiceSessionCipher.open(corrupt,key) }
        }
        assertThrows(Exception::class.java) { VoiceSessionCipher.open(sealed.copyOf().apply { this[0] = 2 },key) }
        assertThrows(Exception::class.java) { VoiceSessionCipher.open(sealed.copyOf(20),key) }
    }
}
