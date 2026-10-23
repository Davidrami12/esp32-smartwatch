package dev.esp32.smartwatch.calendar

import android.accounts.AccountManager
import android.app.Activity
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.IntentSenderRequest
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.layout.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.unit.dp
import com.google.android.gms.auth.api.identity.AuthorizationResult
import com.google.android.gms.auth.api.identity.Identity
import com.google.android.gms.common.api.ApiException
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext

/** Consent/account choice are exclusively user-driven. No access-token state in Compose. */
@Composable
fun GoogleAgendaSettings() {
    val context=LocalContext.current
    val prefs=remember { CalendarBridge.preferences(context) }
    var account by remember { mutableStateOf(prefs.getString("google_account",null)) }
    var chosenAccount by rememberSaveable { mutableStateOf<String?>(null) }
    var calendars by remember { mutableStateOf(prefs.getStringSet("google_calendars",emptySet()).orEmpty().toSet()) }
    var tasklists by remember { mutableStateOf(prefs.getStringSet("google_tasklists",emptySet()).orEmpty().toSet()) }
    var birthdays by remember { mutableStateOf(prefs.getBoolean("google_birthdays",false)) }
    var catalog by remember { mutableStateOf<GoogleAgendaClient.Catalog?>(null) }
    var revision by remember { mutableIntStateOf(0) }
    var busy by remember { mutableStateOf(false) }
    var message by remember { mutableStateOf<String?>(null) }
    fun failure(e: Exception) {
        busy=false
        message=when (e) {
            is GoogleAgendaConsentRequired -> "Google permission is required. Connect Google again and grant both read-only permissions."
            is ApiException -> if (e.statusCode==10) "Google setup required (10): register this app's package and signing SHA-1 as an Android OAuth client. See docs/google-agenda.md." else "Google authorization failed (${e.statusCode}). You can retry; no data was changed."
            is GoogleAgendaHttpFailure -> "Google API unavailable (${e.code}). Check connection and that Calendar API / Tasks API are enabled."
            else -> "Google could not be reached. Check the connection and Google API/OAuth setup, then retry."
        }
    }
    fun accept(result: AuthorizationResult) {
        try {
            GoogleAgendaAuth.token(result) // Validate consent, discard token; Play services caches it.
            val selected=chosenAccount ?: throw GoogleAgendaConsentRequired()
            val edit=prefs.edit()
            if (account!=selected) {
                calendars=emptySet(); tasklists=emptySet(); birthdays=false; catalog=null
                edit.remove("google_calendars").remove("google_tasklists").putBoolean("google_birthdays",false)
            }
            account=selected; edit.putString("google_account",selected).apply()
            busy=false; message="Google connected. Select the sources below; nothing is shared automatically."; revision++
        } catch (e: Exception) { failure(e) }
    }
    val authorization=rememberLauncherForActivityResult(ActivityResultContracts.StartIntentSenderForResult()) { response ->
        if (response.resultCode!=Activity.RESULT_OK) { busy=false; message="Google authorization cancelled." }
        else try { accept(Identity.getAuthorizationClient(context).getAuthorizationResultFromIntent(response.data)) }
        catch (e: Exception) { failure(e) }
    }
    val chooseAccount=rememberLauncherForActivityResult(ActivityResultContracts.StartActivityForResult()) { response ->
        val email=response.data?.getStringExtra(AccountManager.KEY_ACCOUNT_NAME)
        if (response.resultCode!=Activity.RESULT_OK || email.isNullOrBlank()) { busy=false; message="Account selection cancelled." }
        else {
            chosenAccount=email; busy=true
            Identity.getAuthorizationClient(context).authorize(GoogleAgendaAuth.request(email))
                .addOnSuccessListener { result ->
                    if (result.hasResolution()) {
                        val intent=result.pendingIntent
                        if (intent==null) failure(GoogleAgendaConsentRequired())
                        else authorization.launch(IntentSenderRequest.Builder(intent.intentSender).build())
                    } else accept(result)
                }.addOnFailureListener { failure(it) }
        }
    }
    LaunchedEffect(account,revision) {
        val selected=account ?: return@LaunchedEffect
        busy=true
        try {
            catalog=withContext(Dispatchers.IO) { GoogleAgendaClient(context.applicationContext,selected) {
                prefs.getString("google_account",null)==selected
            }.catalog() }
        } catch (e: Exception) { failure(e) }
        finally { busy=false }
    }
    val count=calendars.size+tasklists.size+(if (birthdays) 1 else 0)
    Column(verticalArrangement=Arrangement.spacedBy(8.dp)) {
        HorizontalDivider()
        Text("Google calendars, birthdays and tasks",style=MaterialTheme.typography.titleMedium)
        Text("Optional, read-only Google access. This is separate from Mochi sign-in. Only selected sources go to the watch; Google tokens never leave the phone.",style=MaterialTheme.typography.bodySmall)
        Button(enabled=!busy,onClick={
            message=null; busy=true
            try { chooseAccount.launch(AccountManager.newChooseAccountIntent(null,null,arrayOf("com.google"),null,null,null,null)) }
            catch (e: Exception) { failure(e) }
        }) { Text(if (account==null) "Connect Google · Calendar + Tasks" else "Reconnect / change Google account") }
        if (busy) LinearProgressIndicator(Modifier.fillMaxWidth())
        message?.let { Text(it,style=MaterialTheme.typography.bodySmall) }
        if (account!=null) {
            Text(account!!,style=MaterialTheme.typography.bodySmall)
            Text("Google sources ($count/8)",style=MaterialTheme.typography.titleSmall)
            Row {
                Checkbox(checked=birthdays,enabled=birthdays || count<8,onCheckedChange={
                    birthdays=it; prefs.edit().putBoolean("google_birthdays",it).apply()
                })
                Text("Birthdays · Google account",Modifier.padding(top=12.dp))
            }
            catalog?.let { data ->
                if (data.limited) Text("Source list is limited to the first 128 entries per type. Additional sources are not listed.",style=MaterialTheme.typography.bodySmall)
                Text("Google calendars",style=MaterialTheme.typography.titleSmall)
                data.calendars.forEach { source ->
                    Row {
                        Checkbox(checked=source.id in calendars,enabled=source.id in calendars || count<8,onCheckedChange={ checked ->
                            calendars=if (checked) calendars+source.id else calendars-source.id
                            prefs.edit().putStringSet("google_calendars",calendars).apply()
                        })
                        Text(source.label,Modifier.weight(1f).padding(top=12.dp))
                    }
                    var holiday by remember(source.id,source.holiday) { mutableStateOf(prefs.getBoolean("google_holiday_${source.id}",source.holiday)) }
                    if (source.id in calendars) Row(Modifier.padding(start=24.dp)) {
                        Checkbox(checked=holiday,onCheckedChange={ holiday=it; prefs.edit().putBoolean("google_holiday_${source.id}",it).apply() })
                        Text("Holiday calendar · weekend colour",Modifier.padding(top=12.dp),style=MaterialTheme.typography.bodySmall)
                    }
                }
                Text("Google Tasks lists",style=MaterialTheme.typography.titleSmall)
                data.tasks.forEach { source ->
                    Row {
                        Checkbox(checked=source.id in tasklists,enabled=source.id in tasklists || count<8,onCheckedChange={ checked ->
                            tasklists=if (checked) tasklists+source.id else tasklists-source.id
                            prefs.edit().putStringSet("google_tasklists",tasklists).apply()
                        })
                        Text(source.label,Modifier.weight(1f).padding(top=12.dp))
                    }
                }
            }
            Text("Only pending tasks with a date appear in the watch calendar. Google Tasks API does not expose the scheduled hour. Undated tasks are not placed on an invented date. Select a calendar here OR its local copy below; matching provider calendar IDs are skipped to avoid duplicates.",style=MaterialTheme.typography.bodySmall)
            OutlinedButton(enabled=!busy,onClick={ revision++ }) { Text("Refresh Google source lists") }
            TextButton(enabled=!busy,onClick={
                GoogleAgendaAuth.disconnect(context.applicationContext)
                account=null; calendars=emptySet(); tasklists=emptySet(); birthdays=false; catalog=null
                message="Google sharing removed. Watch caches clear when connected. If offline, revoke the app in your Google Account to ensure server-side revocation."
            }) { Text("Disconnect Google and clear its selections") }
        }
        HorizontalDivider()
    }
}
