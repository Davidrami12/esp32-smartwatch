package dev.esp32.smartwatch.ai

import android.content.Context
import android.security.keystore.KeyGenParameterSpec
import android.security.keystore.KeyProperties
import android.util.AtomicFile
import java.io.File
import java.security.KeyStore
import javax.crypto.KeyGenerator
import javax.crypto.SecretKey

/** Device-local authenticated ciphertext, excluded from backup; no plaintext credential file. */
internal class KeystoreVoiceSessionStore(context: Context) : VoiceSessionStore {
    private val file = AtomicFile(File(context.noBackupFilesDir,"voice-session.enc"))
    private val alias = "watch-voice-session-v1"
    private fun key(create: Boolean): SecretKey {
        val keys = KeyStore.getInstance("AndroidKeyStore").apply { load(null) }
        (keys.getKey(alias,null) as? SecretKey)?.let { return it }
        require(create) { "Session key unavailable" }
        return KeyGenerator.getInstance(KeyProperties.KEY_ALGORITHM_AES,"AndroidKeyStore").run {
            init(KeyGenParameterSpec.Builder(alias,KeyProperties.PURPOSE_ENCRYPT or KeyProperties.PURPOSE_DECRYPT)
                .setBlockModes(KeyProperties.BLOCK_MODE_GCM)
                .setEncryptionPaddings(KeyProperties.ENCRYPTION_PADDING_NONE)
                .setKeySize(256).setRandomizedEncryptionRequired(true).build())
            generateKey()
        }
    }
    @Synchronized override fun load(): SavedVoiceSession? {
        val bytes = try { file.openRead().use { it.readNBytes(32769) } }
                    catch (_: java.io.FileNotFoundException) { return null }
        try { return VoiceSessionCipher.open(bytes,key(false)) }
        finally { bytes.fill(0) }
    }
    @Synchronized override fun save(session: SavedVoiceSession) {
        val encrypted = VoiceSessionCipher.seal(session,key(true))
        try {
            val output = file.startWrite()
            try {
                output.write(encrypted)
                file.finishWrite(output)
            } catch (error: Exception) { file.failWrite(output); throw error }
        } finally { encrypted.fill(0) }
    }
    @Synchronized override fun clear() {
        file.delete()
        KeyStore.getInstance("AndroidKeyStore").apply { load(null); deleteEntry(alias) }
    }
}
