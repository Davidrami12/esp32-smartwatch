package dev.esp32.smartwatch.calendar

import android.accounts.Account
import android.content.Context
import com.google.android.gms.auth.api.identity.AuthorizationRequest
import com.google.android.gms.auth.api.identity.AuthorizationResult
import com.google.android.gms.auth.api.identity.ClearTokenRequest
import com.google.android.gms.auth.api.identity.Identity
import com.google.android.gms.auth.api.identity.RevokeAccessRequest
import com.google.android.gms.common.api.Scope
import com.google.android.gms.common.api.ApiException
import com.google.android.gms.tasks.Tasks
import java.util.concurrent.TimeUnit

class GoogleAgendaConsentRequired : Exception("Connect Google in the phone calendar settings")

/** Google Play services owns token refresh. No OAuth secret, refresh token or
 * access token is stored by this app, shipped in the APK, or sent to our backend. */
object GoogleAgendaAuth {
    val scopes = listOf("https://www.googleapis.com/auth/calendar.readonly", "https://www.googleapis.com/auth/tasks.readonly")
    fun request(account: String) = AuthorizationRequest.builder()
        .setAccount(Account(account,"com.google"))
        .setRequestedScopes(scopes.map(::Scope)).build()

    fun token(result: AuthorizationResult): String {
        if (result.hasResolution() || !result.grantedScopes.containsAll(scopes) || result.accessToken.isNullOrBlank())
            throw GoogleAgendaConsentRequired()
        return result.accessToken!!
    }
    // Worker thread only. Never launches a consent screen from BLE/background work.
    fun token(context: Context, account: String): String {
        if (account.isBlank()) throw GoogleAgendaConsentRequired()
        try {
            return token(Tasks.await(Identity.getAuthorizationClient(context).authorize(request(account)),15,TimeUnit.SECONDS))
        } catch (e: GoogleAgendaConsentRequired) { throw e }
        catch (e: java.util.concurrent.ExecutionException) {
            val status=(e.cause as? ApiException)?.statusCode
            if (status in listOf(4,6,10,12500,12501)) throw GoogleAgendaConsentRequired()
            throw e // Network/timeouts are sync errors, not proof of revoked consent.
        }
    }
    fun clearToken(context: Context, token: String) {
        Identity.getAuthorizationClient(context).clearToken(ClearTokenRequest.builder().setToken(token).build())
    }
    fun disconnect(context: Context) {
        val prefs=CalendarBridge.preferences(context)
        val account=prefs.getString("google_account",null)
        // Local privacy removal is immediate even if Google's revocation is offline.
        prefs.edit().remove("google_account").putBoolean("google_birthdays",false)
            .remove("google_calendars").remove("google_tasklists").apply()
        if (!account.isNullOrBlank()) Identity.getAuthorizationClient(context).revokeAccess(
            RevokeAccessRequest.builder().setAccount(Account(account,"com.google")).setScopes(scopes.map(::Scope)).build())
    }
}
