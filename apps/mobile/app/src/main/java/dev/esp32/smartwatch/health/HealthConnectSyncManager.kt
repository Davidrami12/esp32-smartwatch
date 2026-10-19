package dev.esp32.smartwatch.health

import android.content.Context
import android.os.SystemClock
import androidx.health.connect.client.HealthConnectClient
import dev.esp32.smartwatch.ConnectionState
import dev.esp32.smartwatch.WatchState
import kotlinx.coroutines.*
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.sync.Mutex
import kotlinx.coroutines.sync.withLock
import java.time.Instant

data class HealthConnectState(
    val availability: Int = HealthConnectClient.SDK_UNAVAILABLE,
    val permissionGranted: Boolean = false,
    val automatic: Boolean = false,
    val syncing: Boolean = false,
    val lastSyncMillis: Long? = null,
    val error: String? = null,
    val message: String? = null,
)

/** Existing service supplies secure snapshots. No FGS, permission prompt, cloud or health reads here. */
class HealthConnectSyncManager(context: Context, private val watch: () -> WatchState,
                               private val refreshWatch: () -> Unit) {
    private val repository = HealthConnectRepository(context)
    private val prefs = context.getSharedPreferences("health_steps_sync", Context.MODE_PRIVATE)
    private val scope = CoroutineScope(SupervisorJob() + Dispatchers.Main.immediate)
    private val mutex = Mutex()
    private val engine = StepSyncEngine(repository, object : StepSyncJournal {
        override suspend fun read(id: String) = withContext(Dispatchers.IO) { readRevision(id) }
        override suspend fun write(id: String, revision: StepSyncRevision) = withContext(Dispatchers.IO) { writeRevision(id, revision) }
    })
    private val mutableState = MutableStateFlow(HealthConnectState(automatic = prefs.getBoolean("automatic", false),
        lastSyncMillis = prefs.getLong("last_sync", 0).takeIf { it > 0 }))
    val state = mutableState.asStateFlow()
    private var automaticJob: Job? = null
    private var lastAttemptElapsed = -BATCH_MILLIS
    private var manualRequested = false
    private var manualTimeout: Job? = null

    fun refreshPermissions() { scope.launch {
        try {
            val availability = repository.availability()
            val granted = repository.permitted()
            mutableState.value = state.value.copy(availability = availability, permissionGranted = granted,
                error = if (!granted && state.value.permissionGranted) "Steps permission was revoked. Sync is paused." else state.value.error)
        } catch (cancel: CancellationException) { throw cancel }
        catch (_: Exception) {
            mutableState.value = state.value.copy(permissionGranted = false, error = "Could not check Health Connect. Open its settings and retry.")
        }
    } }
    fun permissionResult(granted: Boolean) {
        mutableState.value = state.value.copy(permissionGranted = granted,
            error = if (granted) null else "Steps permission not granted. No data was written.")
        refreshPermissions()
    }
    fun setAutomatic(enabled: Boolean) { scope.launch {
        if (!withContext(Dispatchers.IO) { prefs.edit().putBoolean("automatic", enabled).commit() }) {
            mutableState.value = state.value.copy(error = "Could not save automatic sync preference."); return@launch
        }
        mutableState.value = state.value.copy(automatic = enabled)
        if (!enabled) { automaticJob?.cancel(); automaticJob = null }
        else onWatchState(watch())
    } }
    fun syncNow() {
        val current = watch()
        if (!current.secureLink || current.connectionState != ConnectionState.CONNECTED || !current.activityAvailable) {
            mutableState.value = state.value.copy(error = "Connect securely to activity-enabled firmware first."); return
        }
        manualRequested = true
        mutableState.value = state.value.copy(error = null, message = "Fetching dated totals from watch…")
        refreshWatch()
        manualTimeout?.cancel()
        manualTimeout = scope.launch {
            delay(15_000)
            if (manualRequested) { manualRequested = false; mutableState.value = state.value.copy(error = "Watch activity read timed out. Retry.", message = null) }
        }
    }
    // Invoked for every fresh successful read (including byte-identical reads).
    fun onSnapshotReceived() {
        if (manualRequested) {
            manualRequested = false; manualTimeout?.cancel()
            scope.launch { sync(manual = true) }
        } else onWatchState(watch())
    }
    fun onWatchState(value: WatchState) {
        if (!value.secureLink || value.connectionState != ConnectionState.CONNECTED) {
            automaticJob?.cancel(); automaticJob = null; return
        }
        if (value.activityError != null) mutableState.value = state.value.copy(error = value.activityError)
        if (!state.value.automatic || value.activity == null || automaticJob?.isActive == true) return
        automaticJob = scope.launch {
            delay(maxOf(1000, lastAttemptElapsed + BATCH_MILLIS - SystemClock.elapsedRealtime()))
            sync(manual = false)
        }
    }
    fun serviceStopped() {
        automaticJob?.cancel(); automaticJob = null
        manualTimeout?.cancel(); manualRequested = false
    }
    private fun readRevision(id: String): StepSyncRevision? {
        val text = prefs.getString(id, null) ?: return null
        val parts = text.split('|')
        check(parts.size == 3 && parts[0].length == 64 && parts[1].toLongOrNull() != null) { "Invalid synchronization journal." }
        return StepSyncRevision(parts[0], parts[1].toLong(), parts[2] == "1")
    }
    private fun writeRevision(id: String, revision: StepSyncRevision) {
        check(prefs.edit().putString(id, "${revision.fingerprint}|${revision.version}|${if (revision.acknowledged) 1 else 0}").commit())
    }
    private suspend fun sync(manual: Boolean) = mutex.withLock {
        val current = watch()
        if (!current.secureLink || current.connectionState != ConnectionState.CONNECTED) return@withLock
        if (!manual && !state.value.automatic) return@withLock
        val snapshot = current.activity ?: return@withLock
        lastAttemptElapsed = SystemClock.elapsedRealtime()
        mutableState.value = state.value.copy(syncing = true, error = null, message = null)
        try {
            val available = repository.availability()
            val granted = repository.permitted()
            mutableState.value = state.value.copy(availability = available, permissionGranted = granted)
            if (!granted) {
                mutableState.value = state.value.copy(error = "Allow Write steps in Health Connect to sync. No permission prompts run in the background.")
                return@withLock
            }
            val written = engine.synchronize(snapshot, manual, Instant.now()) {
                (manual || state.value.automatic) && watch().secureLink &&
                    watch().connectionState == ConnectionState.CONNECTED && watch().activity?.source == snapshot.source
            }
            if (written > 0) {
                val now = System.currentTimeMillis()
                withContext(Dispatchers.IO) { check(prefs.edit().putLong("last_sync", now).commit()) }
                mutableState.value = state.value.copy(lastSyncMillis = now)
            }
            mutableState.value = state.value.copy(message = if (written == 0) "Already up to date." else "Synced $written dated daily total(s).")
        } catch (cancel: CancellationException) { throw cancel }
        catch (_: SecurityException) {
            mutableState.value = state.value.copy(permissionGranted = false, error = "Steps permission denied or revoked. Grant it from the app to retry.")
        } catch (error: IllegalArgumentException) {
            mutableState.value = state.value.copy(error = error.message?.takeIf { it.startsWith("Watch ") } ?: "Invalid watch daily aggregate. No invented timestamps were used.")
        } catch (_: Exception) {
            mutableState.value = state.value.copy(error = "Health Connect write or local journal failed. Retry safely with Sync now.")
        } finally { mutableState.value = state.value.copy(syncing = false) }
    }
    companion object { const val BATCH_MILLIS = 300_000L }
}
