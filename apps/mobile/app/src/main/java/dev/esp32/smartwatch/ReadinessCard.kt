package dev.esp32.smartwatch

import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothManager
import android.content.Intent
import android.os.PowerManager
import android.provider.Settings
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.layout.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.unit.dp
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleEventObserver
import androidx.lifecycle.compose.LocalLifecycleOwner
import dev.esp32.smartwatch.ai.VoiceAiClient
import kotlinx.coroutines.delay

@Composable
internal fun ReadinessCard(state: WatchState, permitted: Boolean, requestPermissions: () -> Unit,
                           appSettings: () -> Unit, notificationSettings: () -> Unit,
                           notificationAllowed: Boolean, notificationPermission: () -> Unit,
                           reconnect: () -> Unit, openAi: () -> Unit) {
    val context = LocalContext.current
    val owner = LocalLifecycleOwner.current
    var bluetooth by remember { mutableStateOf(false) }
    var unrestricted by remember { mutableStateOf(false) }
    var aiEnabled by remember { mutableStateOf(VoiceAiClient.paidRequestsEnabled) }
    var aiRequested by remember { mutableStateOf(VoiceAiClient.paidRequestsRequested) }
    var aiSession by remember { mutableStateOf(VoiceAiClient.sessionReady) }
    fun refresh() {
        bluetooth = permitted && runCatching { context.getSystemService(BluetoothManager::class.java).adapter?.isEnabled == true }.getOrDefault(false)
        unrestricted = context.getSystemService(PowerManager::class.java).isIgnoringBatteryOptimizations(context.packageName)
        aiEnabled = VoiceAiClient.paidRequestsEnabled
        aiRequested = VoiceAiClient.paidRequestsRequested
        aiSession = VoiceAiClient.sessionReady
    }
    val enableBluetooth = rememberLauncherForActivityResult(ActivityResultContracts.StartActivityForResult()) { refresh() }
    DisposableEffect(owner, permitted) {
        val observer = LifecycleEventObserver { _, event -> if (event == Lifecycle.Event.ON_RESUME) refresh() }
        owner.lifecycle.addObserver(observer); refresh()
        onDispose { owner.lifecycle.removeObserver(observer) }
    }
    // Only poll while visible. No sign-in or billable requests are triggered here.
    LaunchedEffect(owner, permitted) {
        while (true) {
            if (owner.lifecycle.currentState.isAtLeast(Lifecycle.State.RESUMED)) refresh()
            delay(1000)
        }
    }
    fun openBattery() {
        runCatching { context.startActivity(Intent(Settings.ACTION_IGNORE_BATTERY_OPTIMIZATION_SETTINGS)) }
            .onFailure { appSettings() }
    }
    Column(Modifier.fillMaxWidth(), verticalArrangement = Arrangement.spacedBy(16.dp)) {
    Card(Modifier.fillMaxWidth()) {
        Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
            Text(connectionTitle(state), style = MaterialTheme.typography.titleLarge)
            Text("Bluetooth: ${if (!permitted) "permiso pendiente" else if (bluetooth) "activado" else "desactivado"}")
            Text("Reloj: ${if (state.connectionState == ConnectionState.CONNECTED && state.secureLink) "listo para usar" else "todavía no está listo"}")
            when {
                !permitted -> {
                    Button(onClick = requestPermissions) { Text("Revisar permisos") }
                    TextButton(onClick = appSettings) { Text("Abrir ajustes de permisos") }
                }
                !bluetooth -> Button(onClick = { enableBluetooth.launch(Intent(BluetoothAdapter.ACTION_REQUEST_ENABLE)) }) { Text("Activar Bluetooth") }
                state.connectionState in listOf(ConnectionState.DISCONNECTED, ConnectionState.ERROR) ->
                    Button(onClick = reconnect) { Text(if (state.savedWatchAvailable) "Reconectar" else "Buscar reloj") }
            }
            state.retryDelayMillis?.let { Text("Nuevo intento en ${(it + 999) / 1000} s", style = MaterialTheme.typography.bodySmall) }
        }
    }
    Card(Modifier.fillMaxWidth()) {
        Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
            Text("Preparación del móvil", style = MaterialTheme.typography.titleLarge)
            Text(if (permitted && bluetooth && notificationAllowed && state.notificationAccess == NotificationAccess.GRANTED && unrestricted)
                "Todo preparado en el móvil" else "Revisa los elementos pendientes")
            Text("Dispositivos cercanos: ${if (permitted) "permitido" else "pendiente"}")
            Text("Aviso de conexión: ${if (notificationAllowed) "permitido" else "pendiente"}")
            if (!notificationAllowed) TextButton(onClick = notificationPermission) { Text("Permitir aviso de conexión") }
            val access = state.notificationAccess == NotificationAccess.GRANTED
            Text("Notificaciones: ${if (access) "acceso concedido" else "sin acceso"}")
            Text("Controles multimedia: ${if (access) "acceso concedido" else "necesitan acceso a notificaciones"}")
            Text("El acceso permite detectar sesiones multimedia; las funciones disponibles dependen del reproductor y del firmware.", style = MaterialTheme.typography.bodySmall)
            TextButton(onClick = notificationSettings) { Text(if (access) "Gestionar acceso" else "Permitir notificaciones y multimedia") }
            Text("Batería: ${if (unrestricted) "exenta de optimización" else "optimización activa"}")
            Text("Opcional: revisa las restricciones si Android interrumpe la conexión en segundo plano.", style = MaterialTheme.typography.bodySmall)
            TextButton(onClick = ::openBattery) { Text("Revisar ajustes de batería") }
        }
    }
    Card(Modifier.fillMaxWidth()) {
        Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
            Text("AI Assistant", style = MaterialTheme.typography.titleLarge)
            Text("Sesión: ${if (aiSession) "verificada" else "inicia sesión o espera a su recuperación"}")
            Text("Conexión: ${if (state.connectionState == ConnectionState.CONNECTED && state.secureLink) "reloj listo" else "reloj no disponible"}")
            Text("Consultas de pago: ${if (BuildConfig.VOICE_TRANSFER_ONLY) "bloqueadas en modo prueba" else if (aiEnabled) "activadas" else if (aiRequested) "pendientes de sesión verificada" else "desactivadas"}")
            Text("Mantener pulsado y soltar envía la consulta si AI está disponible y puede generar costes. CLEAR/BACK cancela; cancelar no es un error.", style = MaterialTheme.typography.bodySmall)
            OutlinedButton(onClick = openAi) { Text("Gestionar sesión y AI de pago") }
        }
    }
    }
}
