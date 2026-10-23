package dev.esp32.smartwatch.calendar

import android.Manifest
import android.content.Intent
import android.net.Uri
import android.os.Bundle
import android.provider.Settings
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleEventObserver
import androidx.lifecycle.compose.LocalLifecycleOwner
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext

class CalendarSettingsActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContent {
            MaterialTheme(colorScheme = darkColorScheme()) {
                val prefs = remember { CalendarBridge.preferences(this) }
                var permitted by remember { mutableStateOf(CalendarBridge.permission(this)) }
                var enabled by remember { mutableStateOf(prefs.getBoolean("enabled", false)) }
                var selected by remember { mutableStateOf(prefs.getStringSet("selected", emptySet()).orEmpty().toSet()) }
                var calendars by remember { mutableStateOf<List<CalendarBridge.Companion.Choice>>(emptyList()) }
                var loading by remember { mutableStateOf(false) }
                var error by remember { mutableStateOf<String?>(null) }
                var revision by remember { mutableIntStateOf(0) }
                val permission = rememberLauncherForActivityResult(ActivityResultContracts.RequestPermission()) { granted -> permitted = granted; revision++ }
                val owner = LocalLifecycleOwner.current
                DisposableEffect(owner) {
                    val observer = LifecycleEventObserver { _, event -> if (event == Lifecycle.Event.ON_RESUME) {
                        permitted = CalendarBridge.permission(this@CalendarSettingsActivity); revision++
                    } }
                    owner.lifecycle.addObserver(observer)
                    onDispose { owner.lifecycle.removeObserver(observer) }
                }
                LaunchedEffect(permitted, revision) {
                    if (!permitted) { calendars = emptyList(); return@LaunchedEffect }
                    loading = true; error = null
                    calendars = withContext(Dispatchers.IO) { runCatching { CalendarBridge.calendars(this@CalendarSettingsActivity) }.getOrElse { emptyList() } }
                    loading = false
                    if (calendars.isEmpty()) error = "No local calendars found. Connect Google above for cloud calendars, birthdays and task lists. Work-profile restrictions may prevent local access."
                }
                Surface(Modifier.fillMaxSize()) {
                    LazyColumn(contentPadding = PaddingValues(24.dp), verticalArrangement = Arrangement.spacedBy(12.dp), modifier = Modifier.systemBarsPadding()) {
                        item { Text("Watch calendar", style = MaterialTheme.typography.headlineMedium) }
                        item { Text("Read-only access to selected local and authorized Google sources. Titles, dates and locations are sent only to your securely paired watch. No Google password, AI calls or calendar edits.") }
                        item { Text("The watch opens a month grid with your calendar colours. Tap a date for its agenda or the month heading for the weekly view. Cells stay black; weekend and imported holiday numbers are pale blue-grey. Neighbouring-month dates are dimmed. Spain/Madrid holiday calendars are suggested automatically; you can override the holiday setting below. Times use Madrid. Up to six weeks are kept in RAM; reboot clears them. Turning sharing off clears cached events when the watch reconnects.") }
                        item {
                            Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
                                Text("Share calendar with watch", modifier = Modifier.weight(1f))
                                Switch(checked = enabled, onCheckedChange = {
                                    enabled = it; prefs.edit().putBoolean("enabled", it).apply()
                                })
                            }
                        }
                        item { GoogleAgendaSettings() }
                        if (!permitted) item {
                            Button(onClick = { permission.launch(Manifest.permission.READ_CALENDAR) }) { Text("Allow read-only calendar access") }
                            TextButton(onClick = { startActivity(Intent(Settings.ACTION_APPLICATION_DETAILS_SETTINGS, Uri.parse("package:$packageName"))) }) { Text("Open Android app permissions") }
                        }
                        item { Text("Local Android calendars (${selected.size}/32)", style = MaterialTheme.typography.titleMedium) }
                        if (loading) item { CircularProgressIndicator() }
                        error?.let { message -> item { Text(message) } }
                        items(calendars, key = { it.id }) { calendar ->
                            val id = calendar.id.toString()
                            Row(Modifier.fillMaxWidth()) {
                                Checkbox(checked = id in selected, enabled = id in selected || selected.size < 32, onCheckedChange = { checked ->
                                    selected = if (checked) selected + id else selected - id
                                    prefs.edit().putStringSet("selected", selected).apply()
                                })
                                Text(calendar.label, Modifier.weight(1f).padding(top = 12.dp))
                            }
                            var holiday by remember(calendar.id,calendar.holidaySuggested) { mutableStateOf(prefs.getBoolean("holiday_$id",calendar.holidaySuggested)) }
                            if (id in selected) Row(Modifier.fillMaxWidth().padding(start = 24.dp)) {
                                Checkbox(checked = holiday,onCheckedChange = { checked ->
                                    holiday = checked; prefs.edit().putBoolean("holiday_$id",checked).apply()
                                })
                                Text("Holiday calendar · weekend colour",Modifier.weight(1f).padding(top = 12.dp),style = MaterialTheme.typography.bodySmall)
                            }
                        }
                        if (permitted) item { OutlinedButton(onClick = { revision++ }) { Text("Refresh calendar list") } }
                        item { Text("Local calendars with sync disabled are listed too, but may only contain cached events. This app never enables account synchronization automatically. Up to 256 local calendars are listed.",style=MaterialTheme.typography.bodySmall) }
                        item { Text("Up to 32 events per week. Overflow is indicated on the watch; full details remain available in Google Calendar.", style = MaterialTheme.typography.bodySmall) }
                        item { TextButton(onClick = { finish() }) { Text("Back") } }
                    }
                }
            }
        }
    }
}
