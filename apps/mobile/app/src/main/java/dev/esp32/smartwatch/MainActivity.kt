package dev.esp32.smartwatch

import android.Manifest
import android.content.Intent
import android.content.pm.PackageManager
import android.net.Uri
import android.os.Bundle
import android.provider.Settings
import androidx.activity.ComponentActivity
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleEventObserver
import androidx.lifecycle.compose.LocalLifecycleOwner
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import androidx.lifecycle.viewmodel.compose.viewModel
import dev.esp32.smartwatch.ble.WifiState
import dev.esp32.smartwatch.ble.WatchBleProtocol
import java.text.NumberFormat
import kotlin.math.roundToInt

class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()
        setContent {
            MaterialTheme(colorScheme = darkColorScheme(
                primary = Color.White, onPrimary = Color.Black,
                background = Color.Black, surface = Color(0xFF141414),
                onSurface = Color(0xFFF2F2F2), onSurfaceVariant = Color(0xFFAAAAAA),
            )) {
                val model: WatchViewModel = viewModel()
                val state by model.state.collectAsStateWithLifecycle()
                var permitted by remember { mutableStateOf(model.hasPermissions()) }
                var notificationAllowed by remember { mutableStateOf(checkSelfPermission(Manifest.permission.POST_NOTIFICATIONS) == PackageManager.PERMISSION_GRANTED) }
                val notificationPermission = rememberLauncherForActivityResult(ActivityResultContracts.RequestPermission()) {
                    notificationAllowed = it
                }
                val permissions = rememberLauncherForActivityResult(ActivityResultContracts.RequestMultiplePermissions()) {
                    permitted = model.hasPermissions()
                    model.permissionResult(permitted)
                    if (permitted) model.scan()
                }
                val owner = LocalLifecycleOwner.current
                DisposableEffect(owner, model) {
                    val observer = LifecycleEventObserver { _, event ->
                        if (event == Lifecycle.Event.ON_START) {
                            permitted = model.hasPermissions()
                            model.refreshNotificationAccess()
                            notificationAllowed = checkSelfPermission(Manifest.permission.POST_NOTIFICATIONS) == PackageManager.PERMISSION_GRANTED
                            model.setForeground(true)
                        } else if (event == Lifecycle.Event.ON_STOP) model.setForeground(false)
                    }
                    owner.lifecycle.addObserver(observer)
                    model.setForeground(owner.lifecycle.currentState.isAtLeast(Lifecycle.State.STARTED))
                    onDispose { owner.lifecycle.removeObserver(observer); model.setForeground(false) }
                }
                val requestScan = {
                    if (model.hasPermissions()) model.scan()
                    else permissions.launch(arrayOf(Manifest.permission.BLUETOOTH_SCAN, Manifest.permission.BLUETOOTH_CONNECT))
                }
                SmartwatchScreen(state, permitted, requestScan, model::stopScan, model::connect, model::disconnect,
                    model::setBrightness, model::setScreenTimeout, model::setWifiEnabled,
                    model::resumeCompanion, notificationAllowed,
                    { notificationPermission.launch(Manifest.permission.POST_NOTIFICATIONS) },
                    { startActivity(Intent(Settings.ACTION_NOTIFICATION_LISTENER_SETTINGS)) }) {
                    startActivity(Intent(Settings.ACTION_APPLICATION_DETAILS_SETTINGS, Uri.parse("package:$packageName")))
                }
            }
        }
    }
}

@Composable
private fun SmartwatchScreen(
    state: WatchState, permitted: Boolean, scan: () -> Unit, stopScan: () -> Unit,
    connect: (String) -> Unit, disconnect: () -> Unit,
    brightnessChanged: (Int) -> Unit, timeoutChanged: (Long) -> Unit, wifiChanged: (Boolean) -> Unit,
    resumeCompanion: () -> Unit, notificationAllowed: Boolean, notificationPermission: () -> Unit,
    notificationSettings: () -> Unit,
    appSettings: () -> Unit,
) {
    val busy = state.connectionState in listOf(ConnectionState.CONNECTING, ConnectionState.PAIRING, ConnectionState.DISCOVERING_SERVICES, ConnectionState.RECONNECTING)
    Scaffold(containerColor = Color.Black) { padding ->
        LazyColumn(modifier = Modifier.fillMaxSize().padding(padding), contentPadding = PaddingValues(24.dp),
            verticalArrangement = Arrangement.spacedBy(16.dp)) {
            item { Text("Smartwatch", fontSize = 34.sp, fontWeight = FontWeight.Bold) }
            item {
                Text(when (state.connectionState) {
                    ConnectionState.DISCONNECTED -> "Disconnected"
                    ConnectionState.SCANNING -> "Scanning for compatible watches…"
                    ConnectionState.CONNECTING -> "Connecting…"
                    ConnectionState.PAIRING -> "Pairing · compare codes on both devices"
                    ConnectionState.DISCOVERING_SERVICES -> "Discovering services and preparing data…"
                    ConnectionState.CONNECTED -> "Connected · encrypted + MITM protected"
                    ConnectionState.RECONNECTING -> "Reconnecting to bonded watch…"
                    ConnectionState.ERROR -> "Connection error"
                }, color = MaterialTheme.colorScheme.onSurfaceVariant)
            }
            if (!permitted) item {
                Text("Nearby devices permission is required to find and connect to your watch. Location is not used.")
                TextButton(onClick = appSettings) { Text("Open app settings") }
            }
            state.message?.let { message -> item { Text(message, color = MaterialTheme.colorScheme.onSurfaceVariant) } }
            item { Text("Pairing · ${state.pairingState.name}", style = MaterialTheme.typography.bodySmall) }
            item {
                Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
                    Text(if (state.companionEnabled) "Background companion enabled" else "Background companion stopped",
                        style = MaterialTheme.typography.titleMedium)
                    Text("Home, screen lock and removing this app from recents keep the enabled service running. Disconnect stops it. Phone reboot or Android force-stop requires enabling it again.",
                        style = MaterialTheme.typography.bodySmall)
                    if (!notificationAllowed) {
                        Text("Allow notifications to see connection status in the notification drawer. The foreground service can run without this permission, but Android may show it only in Active apps.",
                            style = MaterialTheme.typography.bodySmall)
                        TextButton(onClick = notificationPermission) { Text("Allow connection notification") }
                    }
                    if (!state.companionEnabled && state.savedWatchAvailable) {
                        OutlinedButton(onClick = resumeCompanion, enabled = permitted, modifier = Modifier.fillMaxWidth()) { Text("Enable companion · saved watch") }
                    }
                    if (state.companionEnabled && state.connectionState != ConnectionState.CONNECTED && !busy) {
                        TextButton(onClick = disconnect) { Text("Disconnect · stop companion") }
                    }
                }
            }
            item {
                Card(Modifier.fillMaxWidth()) {
                    Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                        Text("Notification access · ${state.notificationAccess.name}", style = MaterialTheme.typography.titleMedium)
                        Text("Selected text notifications are sent only after bonded, authenticated LE Secure Connections communication is verified. Confirm matching pairing codes on both devices. This is not a full production security audit.",
                            style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
                        if (state.notificationAccess == NotificationAccess.NOT_GRANTED) {
                            OutlinedButton(onClick = notificationSettings) { Text("Grant notification access") }
                        } else TextButton(onClick = notificationSettings) { Text("Manage notification access") }
                        if (state.connectionState == ConnectionState.CONNECTED && !state.notificationTransportAvailable)
                            Text("Notification transport unavailable on this firmware.")
                        if (state.sendingNotification) Text("Sending notification…", style = MaterialTheme.typography.bodySmall)
                        state.notificationError?.let { Text(it, color = MaterialTheme.colorScheme.error) }
                        state.mediaError?.let { Text(it, color = MaterialTheme.colorScheme.error) }
                        if (state.connectionState == ConnectionState.CONNECTED && !state.mediaAvailable)
                            Text("Media transport unavailable on this firmware.", style = MaterialTheme.typography.bodySmall)
                    }
                }
            }
            if (state.connectionState == ConnectionState.CONNECTED) {
                item { ValueCard("Battery", state.battery?.let { "$it%" } ?: "Unavailable") }
                item { ValueCard("Steps today", state.steps?.let { NumberFormat.getIntegerInstance().format(it) } ?: "—") }
                item { ValueCard("Wi-Fi", when (state.wifiState) {
                    WifiState.DISCONNECTED -> "Disconnected"
                    WifiState.CONNECTING -> "Connecting"
                    WifiState.CONNECTED -> "Connected"
                    WifiState.DISABLED -> "Disabled"
                    WifiState.UNKNOWN, null -> "Unknown"
                }) }
                item { ValueCard("Protocol", state.protocolVersion?.let { "v$it" } ?: "—") }
                item { WatchControls(state, brightnessChanged, timeoutChanged, wifiChanged) }
                item { OutlinedButton(onClick = disconnect, modifier = Modifier.fillMaxWidth()) { Text("Disconnect") } }
            } else {
                item {
                    if (busy) {
                        LinearProgressIndicator(modifier = Modifier.fillMaxWidth())
                        TextButton(onClick = disconnect) { Text("Disconnect · stop companion") }
                    } else if (state.connectionState == ConnectionState.SCANNING) {
                        LinearProgressIndicator(modifier = Modifier.fillMaxWidth())
                        OutlinedButton(onClick = stopScan) { Text("Stop scan") }
                    } else Button(onClick = scan, modifier = Modifier.fillMaxWidth()) { Text("Scan for watch") }
                }
                if (!busy) {
                    item { Text("Nearby compatible watches", style = MaterialTheme.typography.titleMedium) }
                    items(state.devices, key = { it.id }) { watch ->
                        Card(modifier = Modifier.fillMaxWidth()) {
                            Column(Modifier.padding(16.dp)) {
                                Text(watch.name, fontWeight = FontWeight.SemiBold)
                                Text("${watch.id.takeLast(5)} · Signal ${watch.rssi} dBm", style = MaterialTheme.typography.bodySmall)
                                TextButton(onClick = { connect(watch.id) }, enabled = permitted) { Text("Connect") }
                            }
                        }
                    }
                    item { Text("Enable Bluetooth on the watch. It supports one phone connection at a time.", style = MaterialTheme.typography.bodySmall,
                        color = MaterialTheme.colorScheme.onSurfaceVariant) }
                }
            }
        }
    }
}

@Composable
private fun ValueCard(title: String, value: String) {
    Card(modifier = Modifier.fillMaxWidth()) {
        Column(Modifier.padding(20.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
            Text(title, color = MaterialTheme.colorScheme.onSurfaceVariant)
            Text(value, fontSize = 28.sp, fontWeight = FontWeight.SemiBold)
        }
    }
}

@Composable
private fun WatchControls(state: WatchState, brightnessChanged: (Int) -> Unit,
                          timeoutChanged: (Long) -> Unit, wifiChanged: (Boolean) -> Unit) {
    var draft by remember { mutableFloatStateOf((state.brightness ?: 60).toFloat()) }
    var dragging by remember { mutableStateOf(false) }
    LaunchedEffect(state.brightness, state.writingControl, dragging) {
        if (!dragging && !state.writingControl) draft = (state.brightness ?: 60).toFloat()
    }
    val enabled = state.controlsAvailable && !state.writingControl && !state.sendingNotification && !state.sendingMedia
    Card(modifier = Modifier.fillMaxWidth()) {
        Column(Modifier.padding(20.dp), verticalArrangement = Arrangement.spacedBy(12.dp)) {
            Text("Settings / Controls", style = MaterialTheme.typography.titleLarge)
            state.controlError?.let { Text(it, color = MaterialTheme.colorScheme.error, style = MaterialTheme.typography.bodySmall) }
            if (!state.controlsAvailable) {
                Text("Controls unavailable. Install compatible watch firmware and reconnect.")
            } else {
                Text("Brightness · ${state.brightness ?: "—"}%")
                if (dragging) Text("Preview ${draft.roundToInt()}% · applied on release", style = MaterialTheme.typography.bodySmall)
                Slider(value = draft, onValueChange = { dragging = true; draft = it },
                    onValueChangeFinished = { dragging = false; brightnessChanged(draft.roundToInt()) },
                    enabled = enabled && state.brightness != null, valueRange = 10f..100f)
                Text("Screen timeout")
                Row(Modifier.horizontalScroll(rememberScrollState()), horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                    WatchBleProtocol.TIMEOUT_VALUES.forEach { value ->
                        FilterChip(selected = state.screenTimeout == value, onClick = { timeoutChanged(value) },
                            enabled = enabled && state.screenTimeout != null,
                            label = { Text(if (value == 0L) "Never" else "${value / 1000}s") })
                    }
                }
                Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
                    Text("Wi-Fi enabled", modifier = Modifier.padding(top = 12.dp))
                    Switch(checked = state.wifiEnabled == true, onCheckedChange = wifiChanged,
                        enabled = enabled && state.wifiEnabled != null)
                }
                if (state.writingControl) {
                    LinearProgressIndicator(Modifier.fillMaxWidth())
                    Text("Waiting for watch confirmation…", style = MaterialTheme.typography.bodySmall)
                }
                Text("Settings are saved on the watch.", style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant)
            }
        }
    }
}
