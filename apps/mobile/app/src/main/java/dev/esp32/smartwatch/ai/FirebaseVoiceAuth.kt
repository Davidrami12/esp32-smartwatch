package dev.esp32.smartwatch.ai

import java.net.URI
import java.net.HttpURLConnection
import java.util.Base64
import java.util.concurrent.atomic.AtomicLong
import org.json.JSONObject

object VoiceAiConfig {
    const val PROJECT = "esp32-smartwatch-511417"
    const val BACKEND = "https://watch-ai-backend-z5stegdulq-no.a.run.app"
}

class VoiceAuthException(val reason: String, val fatal: Boolean = true) : Exception(reason)

/** Firebase REST session: tokens stay in memory, passwords are never stored. */
class FirebaseVoiceAuth(
    private val post: (String, String, ByteArray) -> JSONObject = ::firebasePost,
    private val clock: () -> Long = System::currentTimeMillis,
) {
    private data class Session(val token: String, val refresh: String, val key: String, val uid: String, val expires: Long)
    @Volatile private var session: Session? = null
    private val generation = AtomicLong()
    fun available() = session != null
    @Synchronized fun logout() { generation.incrementAndGet(); session = null }
    @Synchronized fun prepareLogin(): Long { logout(); return generation.get() }
    fun current(epoch: Long) = generation.get() == epoch
    fun login(key: String, email: String, password: String, epoch: Long = prepareLogin()) {
        if (!current(epoch)) throw VoiceAuthException("Sign-in cancelled")
        if (!key.matches(Regex("AIza[A-Za-z0-9_-]{35}"))) throw VoiceAuthException("Enter the Firebase Web API key, not an OpenAI key")
        val body = JSONObject().put("email",email).put("password",password).put("returnSecureToken",true).toString().toByteArray()
        val json = try { post("https://identitytoolkit.googleapis.com/v1/accounts:signInWithPassword?key=$key","application/json",body) }
        finally { body.fill(0) }
        val candidate = validated(json.getString("idToken"),json.getString("refreshToken"),key,json.getString("localId"))
        synchronized(this) {
            if (epoch != generation.get()) throw VoiceAuthException("Sign-in cancelled")
            session = candidate
        }
    }
    fun token(): String {
        val current = session ?: throw VoiceAuthException("Sign in to AI on your phone")
        if (clock() < current.expires - 60000) return current.token
        val epoch = generation.get()
        val body = ("grant_type=refresh_token&refresh_token=" + java.net.URLEncoder.encode(current.refresh,"UTF-8")).toByteArray()
        try {
            val json = post("https://securetoken.googleapis.com/v1/token?key=${current.key}","application/x-www-form-urlencoded",body)
            val candidate = validated(json.getString("id_token"),json.getString("refresh_token"),current.key,json.getString("user_id"))
            if (candidate.uid != current.uid) throw VoiceAuthException("Firebase account changed. Sign in again")
            synchronized(this) {
                if (epoch != generation.get() || session !== current) throw VoiceAuthException("Session ended. Sign in again")
                session = candidate
            }
            return candidate.token
        } catch (error: VoiceAuthException) {
            if (error.fatal) synchronized(this) { if (session === current) logout() }
            throw error
        } catch (_: Exception) {
            synchronized(this) { if (session === current) logout() }
            throw VoiceAuthException("Invalid Firebase response. Sign in again")
        } finally { body.fill(0) }
    }
    private fun validated(token: String, refresh: String, key: String, uid: String): Session {
        try {
            require(token.length <= 8192 && refresh.isNotBlank() && uid.isNotBlank())
            val parts = token.split('.'); require(parts.size == 3)
            val claims = JSONObject(String(Base64.getUrlDecoder().decode(parts[1]),Charsets.UTF_8))
            require(claims.getString("aud") == VoiceAiConfig.PROJECT)
            require(claims.getString("iss") == "https://securetoken.google.com/${VoiceAiConfig.PROJECT}")
            require(claims.getString("sub") == uid)
            val expires = claims.getLong("exp") * 1000; require(expires > clock())
            // Local project consistency only; backend Firebase Admin verifies the signature/authorization.
            return Session(token,refresh,key,uid,expires)
        } catch (_: Exception) { throw VoiceAuthException("Firebase project/token mismatch. Use ${VoiceAiConfig.PROJECT}") }
    }
}

private fun firebasePost(url: String, type: String, body: ByteArray): JSONObject {
    val connection = URI(url).toURL().openConnection() as HttpURLConnection
    try {
        connection.requestMethod = "POST"; connection.connectTimeout = 10000; connection.readTimeout = 15000
        connection.instanceFollowRedirects = false; connection.doOutput = true
        connection.setRequestProperty("Content-Type",type); connection.setFixedLengthStreamingMode(body.size)
        connection.outputStream.use { it.write(body) }
        val success = connection.responseCode == 200
        val input = if (success) connection.inputStream else connection.errorStream
        val bytes = input?.use { it.readNBytes(16001) } ?: ByteArray(0)
        try {
            if (bytes.size > 16000) throw VoiceAuthException("Invalid Firebase response")
            val json = JSONObject(bytes.toString(Charsets.UTF_8))
            if (!success) {
                val code = json.optJSONObject("error")?.optString("message")?.substringBefore(" :") ?: ""
                throw when (code) {
                    "INVALID_LOGIN_CREDENTIALS", "INVALID_PASSWORD", "EMAIL_NOT_FOUND" -> VoiceAuthException("Incorrect email or password")
                    "USER_DISABLED", "USER_NOT_FOUND", "INVALID_REFRESH_TOKEN", "TOKEN_EXPIRED" -> VoiceAuthException("Account disabled or session expired. Sign in again")
                    "TOO_MANY_ATTEMPTS_TRY_LATER" -> VoiceAuthException("Too many attempts. Try later",false)
                    "API_KEY_INVALID", "INVALID_API_KEY", "PROJECT_NUMBER_MISMATCH" -> VoiceAuthException("Check Firebase project and Web API key")
                    else -> VoiceAuthException("Firebase sign-in unavailable. Check configuration/network",false)
                }
            }
            return json
        } finally { bytes.fill(0) }
    } catch (error: VoiceAuthException) { throw error }
    catch (_: Exception) { throw VoiceAuthException("Firebase network error. Try again",false) }
    finally { connection.disconnect() }
}
