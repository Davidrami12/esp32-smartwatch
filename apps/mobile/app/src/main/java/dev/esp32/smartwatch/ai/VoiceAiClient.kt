package dev.esp32.smartwatch.ai

import java.net.URI
import java.net.HttpURLConnection
import java.util.concurrent.Executors
import org.json.JSONObject
import java.io.ByteArrayOutputStream
import java.nio.ByteBuffer
import java.nio.ByteOrder

/** Credentials are supplied at runtime, memory-only, never logged or embedded. */
object VoiceAiClient {
    val endpoint: String = VoiceAiConfig.BACKEND
    private val auth = FirebaseVoiceAuth()
    private val calls = java.util.concurrent.ConcurrentHashMap.newKeySet<Call>()
    private val consent = PaidVoiceConsent()
    val paidRequestsEnabled get() = consent.enabled()
    internal fun consentTicket() = consent.ticket()
    internal fun consentValid(ticket: PaidVoiceConsent.Ticket?) = consent.permits(ticket)
    private val executor = Executors.newSingleThreadExecutor()
    class Call { @Volatile var cancelled = false; @Volatile var connection: HttpURLConnection? = null
        @Volatile var consentRevoked = false
        fun cancel() { cancelled = true; connection?.disconnect() }
        fun revokeConsent() { consentRevoked = true; cancel() }
    }
    data class Result(val transcription: String, val answer: String)
    fun configured() = auth.available()
    internal fun paidAfterVerifiedSignIn(verified: Boolean, transferOnly: Boolean) = verified && !transferOnly
    fun enablePaidRequests(enabled: Boolean) {
        consent.set(enabled && configured())
        // Revocation affects already queued calls as well as a connected request.
        calls.forEach { it.revokeConsent() }
    }
    internal fun accessOutcome(status: Int, detail: String, active: Boolean): Pair<Boolean, String> = when (status) {
        400 -> if (detail == "Invalid bounded 16 kHz mono PCM WAV" && active)
            true to "Signed in; backend UID access verified."
            else false to "Unexpected backend validation response"
        403 -> false to "Firebase account is not authorized by this backend"
        401 -> false to "Backend rejected Firebase authentication"
        503 -> false to "Backend configuration/service unavailable"
        else -> false to "Backend access verification failed"
    }
    fun https(value: String): String {
        val uri = URI(value)
        require(uri.scheme == "https" && uri.host != null && uri.userInfo == null && uri.fragment == null && uri.query == null)
        return value.trimEnd('/')
    }
    fun signIn(url: String, apiKey: String, email: String, password: String, callback: (Boolean, String) -> Unit) {
        signOut()
        val epoch = auth.prepareLogin()
        executor.execute {
            var ok = false; var message = "Sign-in failed"
            try {
                val target = https(url)
                require(target == VoiceAiConfig.BACKEND)
                auth.login(apiKey,email,password,epoch)
                // Empty body verifies backend UID authorization but fails WAV validation
                // before quota reservation/OpenAI. No speech or paid AI request is sent.
                val connection = URI("$endpoint/v1/voice").toURL().openConnection() as HttpURLConnection
                try {
                    connection.requestMethod = "POST"; connection.connectTimeout = 10000; connection.readTimeout = 15000
                    connection.instanceFollowRedirects = false; connection.doOutput = true
                    connection.setRequestProperty("Authorization","Bearer ${auth.token()}")
                    connection.setRequestProperty("Content-Type","audio/wav")
                    connection.setFixedLengthStreamingMode(0)
                    connection.outputStream.use { }
                    val status = connection.responseCode
                    var detail = ""
                    if (status == 400) {
                        val bytes = connection.errorStream?.use { it.readNBytes(16001) } ?: ByteArray(0)
                        try { if (bytes.size <= 16000) detail = JSONObject(bytes.toString(Charsets.UTF_8)).optString("detail") }
                        finally { bytes.fill(0) }
                    }
                    val outcome = accessOutcome(status,detail,auth.available())
                    ok = outcome.first; message = outcome.second
                } finally { connection.disconnect() }
            } catch (error: VoiceAuthException) { message = error.reason }
            catch (_: Exception) { message = "Sign-in or backend network error. Try again" }
            synchronized(auth) { if (auth.current(epoch)) {
                if (!ok) auth.logout()
                enablePaidRequests(paidAfterVerifiedSignIn(ok, dev.esp32.smartwatch.BuildConfig.VOICE_TRANSFER_ONLY))
                if (ok) message += if (paidRequestsEnabled) " Paid AI enabled for new hold-to-talk questions." else " Paid AI blocked in transfer test mode."
                callback(ok,message)
            } else callback(false,"Sign-in cancelled") }
        }
    }
    fun signOut() = synchronized(auth) { enablePaidRequests(false); auth.logout() }
    internal fun ask(audio: ByteArray, session: Int = 0, diagnostic: (String) -> Unit = {},
                     ticket: PaidVoiceConsent.Ticket? = consent.ticket(), callback: (Result?, String?) -> Unit): Call {
        val queued = System.nanoTime()
        fun trace(stage: String, start: Long) {
            diagnostic("VoiceLatency trace=${session.toUInt().toString(16).padStart(8,'0')} stage=$stage duration_us=${(System.nanoTime()-start)/1000}")
        }
        val call = Call()
        calls.add(call)
        executor.execute {
            trace("executor_queue",queued)
            var result: Result? = null; var error: String? = null
            var wav: ByteArray? = null
            try {
                if (dev.esp32.smartwatch.BuildConfig.VOICE_TRANSFER_ONLY) throw VoiceAuthException("Audio transfer test mode. Backend requests blocked",false)
                require(configured())
                if (!consent.permits(ticket)) throw VoiceAuthException("Enable paid AI before recording a new question",false)
                if (call.cancelled) throw VoiceAuthException("AI request cancelled",false)
                val preparation = System.nanoTime()
                val token = auth.token()
                if (!consent.permits(ticket)) throw VoiceAuthException("Paid AI consent revoked. Record again after enabling",false)
                if (call.cancelled) throw VoiceAuthException("AI request cancelled",false)
                wav = wav(audio)
                val connection = URI("${https(endpoint)}/v1/voice").toURL().openConnection() as HttpURLConnection
                call.connection = connection
                if (call.cancelled) throw VoiceAuthException("AI request cancelled",false)
                connection.requestMethod = "POST"; connection.instanceFollowRedirects = false
                connection.connectTimeout = 10000; connection.readTimeout = 65000; connection.doOutput = true
                connection.setRequestProperty("Authorization", "Bearer $token")
                connection.setRequestProperty("Content-Type", "audio/wav")
                connection.setRequestProperty("X-Voice-Trace",session.toUInt().toString(16).padStart(8,'0'))
                connection.setFixedLengthStreamingMode(wav.size)
                trace("http_preparation",preparation)
                val network = System.nanoTime()
                // Linearization point: a revoked/old ticket can never commit a new request.
                if (!consent.commit(ticket) { require(!call.cancelled) })
                    throw VoiceAuthException("Paid AI consent revoked. Record again after enabling",false)
                if (call.cancelled) throw VoiceAuthException("AI request cancelled",false)
                connection.outputStream.use { it.write(wav) }
                trace("http_upload_including_connect",network)
                val waiting = System.nanoTime()
                val httpStatus = connection.responseCode
                trace("http_wait_headers",waiting)
                if (httpStatus != 200) {
                    error = when (connection.responseCode) { 401,403 -> "Sign in to AI on your phone"; 429 -> "AI request limit reached"; 503 -> "AI service unavailable"; else -> "AI request failed. Hold to retry" }
                    if (connection.responseCode == 401 || connection.responseCode == 403) { auth.logout(); enablePaidRequests(false) }
                } else {
                    val download = System.nanoTime()
                    val bytes = readBounded(connection, 16000)
                    trace("http_download",download)
                    val parsing = System.nanoTime()
                    val json = JSONObject(bytes.toString(Charsets.UTF_8)); bytes.fill(0)
                    result = Result(bounded(json.getString("transcription"),512),bounded(json.getString("answer"),3072))
                    require(result.transcription.isNotBlank() && result.answer.isNotBlank())
                    trace("result_parse",parsing)
                }
                trace("http_round_trip",network)
            } catch (failure: VoiceAuthException) { error = failure.reason; if (failure.fatal) enablePaidRequests(false) }
            catch (_: java.net.SocketTimeoutException) { error = "AI request timed out. Hold to retry" }
            catch (_: Exception) { error = if (!configured()) "Configure AI and sign in on phone" else "AI request failed. Hold to retry" }
            finally { audio.fill(0); wav?.fill(0); call.connection?.disconnect(); call.connection = null; calls.remove(call) }
            if (call.consentRevoked || (result != null && !consent.permits(ticket))) callback(null,"Paid AI consent revoked. Hold to retry")
            else if (!call.cancelled) callback(result,error)
        }
        return call
    }
    fun bounded(value: String, maximum: Int): String {
        val normalized = java.text.Normalizer.normalize(value,java.text.Normalizer.Form.NFC)
            // Latin-1 watch fonts: preserve Spanish accents; simplify typography.
            .replace('\u2018','\'').replace('\u2019','\'').replace('\u201c','"').replace('\u201d','"')
            .replace('\u2013','-').replace('\u2014','-').replace("\u2026","...").replace('\u00a0',' ')
            .replace("\r\n","\n").replace('\r','\n').filter { it >= ' ' || it == '\n' || it == '\t' }
        var count = 0; val out = StringBuilder()
        normalized.codePoints().toArray().forEach { cp ->
            val text = String(Character.toChars(cp)); val size = text.toByteArray(Charsets.UTF_8).size
            if (count + size <= maximum) { out.append(text); count += size } else return out.toString()
        }
        return out.toString()
    }
    fun wav(audio: ByteArray): ByteArray {
        require(audio.size in 8000..256000 && audio.size % 2 == 0)
        return ByteBuffer.allocate(44+audio.size).order(ByteOrder.LITTLE_ENDIAN)
            .put("RIFF".toByteArray()).putInt(36+audio.size).put("WAVEfmt ".toByteArray()).putInt(16)
            .putShort(1).putShort(1).putInt(16000).putInt(32000).putShort(2).putShort(16)
            .put("data".toByteArray()).putInt(audio.size).put(audio).array()
    }
    private fun readBounded(connection: HttpURLConnection, maximum: Int): ByteArray {
        val output = ByteArrayOutputStream()
        connection.inputStream.use { input ->
            val chunk = ByteArray(1024)
            try { while (true) { val count = input.read(chunk); if (count < 0) break; require(output.size()+count <= maximum); output.write(chunk,0,count) } }
            finally { chunk.fill(0) }
        }
        return output.toByteArray()
    }
}
