package dev.esp32.smartwatch.ai

import javax.crypto.Cipher
import javax.crypto.SecretKey
import javax.crypto.spec.GCMParameterSpec
import org.json.JSONObject

/** Versioned authenticated format; ciphertext cannot be moved to a different app key. */
internal object VoiceSessionCipher {
    private val aad = "dev.esp32.smartwatch.voice-session.v1".toByteArray()
    fun seal(session: SavedVoiceSession, key: SecretKey): ByteArray {
        val plain = JSONObject().put("refresh",session.refresh).put("key",session.key).put("uid",session.uid).toString().toByteArray()
        try {
            require(plain.size <= 32000)
            val cipher = Cipher.getInstance("AES/GCM/NoPadding")
            cipher.init(Cipher.ENCRYPT_MODE,key); cipher.updateAAD(aad)
            require(cipher.iv.size == 12)
            return byteArrayOf(1) + cipher.iv + cipher.doFinal(plain)
        } finally { plain.fill(0) }
    }
    fun open(bytes: ByteArray, key: SecretKey): SavedVoiceSession {
        require(bytes.size in 30..32768 && bytes[0].toInt() == 1)
        val cipher = Cipher.getInstance("AES/GCM/NoPadding")
        cipher.init(Cipher.DECRYPT_MODE,key,GCMParameterSpec(128,bytes.copyOfRange(1,13)))
        cipher.updateAAD(aad)
        val plain = cipher.doFinal(bytes,13,bytes.size-13)
        try {
            val json = JSONObject(plain.toString(Charsets.UTF_8))
            return SavedVoiceSession(json.getString("refresh"),json.getString("key"),json.getString("uid")).also {
                require(it.refresh.isNotBlank() && it.uid.isNotBlank())
            }
        } finally { plain.fill(0) }
    }
}
