package dev.esp32.smartwatch.health

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp

class HealthPrivacyActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContent { MaterialTheme { Surface { Column(Modifier.fillMaxSize().systemBarsPadding().padding(24.dp)
            .verticalScroll(rememberScrollState()), verticalArrangement = Arrangement.spacedBy(16.dp)) {
            Text("Health Connect · privacy & permissions", style = MaterialTheme.typography.headlineSmall)
            Text("Smartwatch requests only permission to write steps. It does not read Health Connect data or change records from other apps.")
            Text("With your consent, dated daily step totals received from your bonded watch are written to Health Connect on this phone. Records identify the companion as the writing app and the Waveshare watch as the recording device.")
            Text("These are daily aggregates: midnight to the latest snapshot, or midnight to midnight for completed days, using the watch's date and UTC offsets. They are not precise walking times. Missing dates are not filled in.")
            Text("Health Connect does not accept zero-step records. A real zero total leaves no entry, or removes only Smartwatch's matching watch/date entry if the watch corrects an earlier positive total to zero.")
            Text("Sync now performs an explicit sync. Automatic sync is off by default; when enabled it batches updates in the existing companion service. No cloud upload, advertising, health-data logging or extra foreground service is used.")
            Text("A private local journal stores hashed source/date record IDs, content fingerprints, versions, automatic-sync preference and the last sync time. Android backup is disabled. Uninstalling clears this journal; it does not provide a reliable Health Connect deletion workflow.")
            Text("Disable automatic sync here or revoke Write steps in Health Connect at any time. Existing Health Connect records remain under Health Connect's controls; manage/delete them there. Other apps you authorize in Health Connect may access the steps you share.")
            Button(onClick = { finish() }) { Text("Back") }
        } } } }
    }
}
