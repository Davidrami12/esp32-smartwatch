package dev.esp32.smartwatch.ai

import org.json.JSONObject
import org.junit.Assert.*
import org.junit.Test
import java.util.Base64

class FirebaseVoiceAuthTest {
    private val key = "AIza" + "A".repeat(35)
    private var now = 1700000000000L
    private fun jwt(project: String = VoiceAiConfig.PROJECT, uid: String = "user", duration: Long = 3600000): String {
        val claims = JSONObject().put("aud",project).put("iss","https://securetoken.google.com/$project")
            .put("sub",uid).put("exp",(now+duration)/1000).toString()
        return "header.${Base64.getUrlEncoder().withoutPadding().encodeToString(claims.toByteArray())}.mock-signature"
    }
    private fun loginResponse(project: String = VoiceAiConfig.PROJECT) = JSONObject()
        .put("idToken",jwt(project)).put("refreshToken","mock-refresh").put("localId","user")
    @Test fun loginCachedTokenAndLogout() {
        var posts = 0
        val auth = FirebaseVoiceAuth({ url, type, body ->
            posts++; assertTrue(url.startsWith("https://identitytoolkit.googleapis.com/"))
            assertEquals("application/json",type); assertTrue(body.isNotEmpty()); loginResponse()
        }, { now })
        auth.login(key,"mock@example.invalid","mock-password")
        assertTrue(auth.available()); assertEquals(jwt(),auth.token()); assertEquals(1,posts)
        auth.logout(); assertFalse(auth.available())
        assertThrows(VoiceAuthException::class.java) { auth.token() }
    }
    @Test fun refreshNearExpiryAndRotatedToken() {
        var posts = 0
        val auth = FirebaseVoiceAuth({ url, type, body ->
            posts++
            if (posts == 1) loginResponse() else {
                assertTrue(url.startsWith("https://securetoken.googleapis.com/"))
                assertEquals("application/x-www-form-urlencoded",type)
                assertTrue(body.toString(Charsets.UTF_8).contains("grant_type=refresh_token"))
                JSONObject().put("id_token",jwt()).put("refresh_token","rotated-refresh").put("user_id","user")
            }
        }, { now })
        auth.login(key,"mock@example.invalid","mock-password"); now += 3550000
        assertEquals(jwt(),auth.token()); assertEquals(2,posts)
        assertEquals(jwt(),auth.token()); assertEquals(2,posts)
    }
    @Test fun wrongProjectIsRejectedBeforeBackendUse() {
        val auth = FirebaseVoiceAuth({ _, _, _ -> loginResponse("other-project") },{ now })
        assertThrows(VoiceAuthException::class.java) { auth.login(key,"mock","mock") }
        assertFalse(auth.available())
    }
    @Test fun revokedRefreshClearsSession() {
        var posts = 0
        val auth = FirebaseVoiceAuth({ _, _, _ -> if (++posts == 1) loginResponse() else throw VoiceAuthException("Session expired") },{ now })
        auth.login(key,"mock","mock"); now += 3550000
        assertThrows(VoiceAuthException::class.java) { auth.token() }; assertFalse(auth.available())
    }
    @Test fun transientRefreshFailureKeepsSessionForRetry() {
        var posts = 0
        val auth = FirebaseVoiceAuth({ _, _, _ -> if (++posts == 1) loginResponse() else throw VoiceAuthException("Network unavailable",false) },{ now })
        auth.login(key,"mock","mock"); now += 3550000
        assertThrows(VoiceAuthException::class.java) { auth.token() }; assertTrue(auth.available())
    }
    @Test fun logoutDuringLoginCannotRestoreSession() {
        lateinit var auth: FirebaseVoiceAuth
        auth = FirebaseVoiceAuth({ _, _, _ -> auth.logout(); loginResponse() },{ now })
        assertThrows(VoiceAuthException::class.java) { auth.login(key,"mock","mock") }
        assertFalse(auth.available())
    }
    @Test fun cancelledQueuedLoginCannotSendCredentials() {
        val auth = FirebaseVoiceAuth({ _, _, _ -> fail("Must not send credentials"); loginResponse() },{ now })
        val epoch = auth.prepareLogin(); auth.logout()
        assertThrows(VoiceAuthException::class.java) { auth.login(key,"mock","mock",epoch) }
    }
    @Test fun openAiKeyIsNotAcceptedAndPaidRequestsStartDisabled() {
        val auth = FirebaseVoiceAuth({ _, _, _ -> fail("Must not send wrong API key"); loginResponse() },{ now })
        assertThrows(VoiceAuthException::class.java) { auth.login("not-a-firebase-key","mock","mock") }
        VoiceAiClient.signOut(); VoiceAiClient.enablePaidRequests(true)
        assertFalse(VoiceAiClient.paidRequestsEnabled)
        assertEquals("https://watch-ai-backend-z5stegdulq-no.a.run.app",VoiceAiClient.endpoint)
    }
    @Test fun accessProbeRejectsUnauthorizedAndUnexpectedResponses() {
        assertTrue(VoiceAiClient.accessOutcome(400,"Invalid bounded 16 kHz mono PCM WAV",true).first)
        assertFalse(VoiceAiClient.accessOutcome(400,"Invalid bounded 16 kHz mono PCM WAV",false).first)
        for (status in listOf(401,403,503,200,404)) {
            val outcome = VoiceAiClient.accessOutcome(status,"private upstream detail",true)
            assertFalse(outcome.first); assertFalse(outcome.second.contains("private"))
        }
        assertFalse(VoiceAiClient.accessOutcome(400,"unrelated error",true).first)
    }
    @Test fun paidAiRequiresVerifiedSignInAndNeverEnablesInTransferMode() {
        assertTrue(VoiceAiClient.paidAfterVerifiedSignIn(true, false))
        assertFalse(VoiceAiClient.paidAfterVerifiedSignIn(false, false))
        assertFalse(VoiceAiClient.paidAfterVerifiedSignIn(true, true))
        assertFalse(VoiceAiClient.paidAfterVerifiedSignIn(false, true))
        val consent = PaidVoiceConsent()
        val olderRecording = consent.ticket()
        consent.set(VoiceAiClient.paidAfterVerifiedSignIn(true, false))
        assertFalse(consent.permits(olderRecording))
        assertTrue(consent.permits(consent.ticket()))
    }
}
