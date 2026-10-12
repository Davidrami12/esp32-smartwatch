package dev.esp32.smartwatch.activity

import android.content.Context
import dev.esp32.smartwatch.ConnectionState
import dev.esp32.smartwatch.WatchState
import kotlinx.coroutines.*
import kotlinx.coroutines.flow.*
import java.time.Instant
import java.util.Base64

/** Presentation/cache boundary: consumes watch StateFlow, never Health Connect data. */
class ActivityDashboardRepository(context: Context, private val watch: StateFlow<WatchState>) {
    private val preferences = context.getSharedPreferences("activity_dashboard", Context.MODE_PRIVATE)
    private val scope = CoroutineScope(SupervisorJob() + Dispatchers.Main.immediate)
    private val mutableState = MutableStateFlow(ActivityDashboardState())
    val state = mutableState.asStateFlow()
    private var cached: ActivityCache? = null
    private var cacheError: String? = null
    init {
        scope.launch {
            cached = withContext(Dispatchers.IO) {
                try {
                    preferences.getString("snapshot",null)?.let { ActivityCacheCodec.decode(Base64.getDecoder().decode(it)) }
                } catch (_: Exception) { cacheError = "Saved activity cache is unavailable. Connect to refresh."; null }
            }
            render()
            watch.collect { value ->
                val snapshot = value.activity
                val updated = value.activityUpdatedAt
                if (value.secureLink && value.connectionState == ConnectionState.CONNECTED &&
                    snapshot != null && snapshot.days.isNotEmpty() && updated != null) {
                    val sameWatch = cached?.snapshot?.source == snapshot.source
                    val next = ActivityCache(snapshot,updated,value.stepGoal ?: cached?.goal?.takeIf { sameWatch },
                        value.stepGoalUpdatedAt ?: cached?.goalUpdatedAt?.takeIf { sameWatch })
                    if (next != cached) {
                        cached = next; cacheError = null
                        try {
                            withContext(Dispatchers.IO) {
                                check(preferences.edit().putString("snapshot", Base64.getEncoder().encodeToString(ActivityCacheCodec.encode(next))).commit())
                            }
                        } catch (cancel: CancellationException) { throw cancel }
                        catch (_: Exception) { cacheError = "Activity received, but saving the local cache failed." }
                    }
                }
                render()
            }
        }
        scope.launch { while (isActive) { delay(15_000); render() } } // Freshness and CET/CEST midnight, no BLE polling.
    }
    private fun render() {
        val current = watch.value
        val now = Instant.now()
        val secure = current.secureLink && current.connectionState == ConnectionState.CONNECTED
        mutableState.value = ActivityDashboardState(cached,
            cached?.let { ActivityDashboardModel.isLive(it,secure,current.activity,now,current.activityError) } ?: false,
            cached?.let { ActivityDashboardModel.summary(it,now) },
            cacheError ?: current.activityError ?: current.stepGoalError ?: if (secure && !current.activityAvailable) "Dated activity is unavailable on this firmware." else null)
    }
}
