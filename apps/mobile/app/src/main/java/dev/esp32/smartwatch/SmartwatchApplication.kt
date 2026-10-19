package dev.esp32.smartwatch

import android.app.Application
import dev.esp32.smartwatch.companion.CompanionConnection
import dev.esp32.smartwatch.notifications.PhoneNotification

// Process-local boundary; no GATT object exposed to the listener or Compose.
class SmartwatchApplication : Application() {
    override fun onCreate() {
        super.onCreate()
        dev.esp32.smartwatch.ai.VoiceAiClient.initialize(this)
    }
    val companion: CompanionConnection by lazy { CompanionConnection(this) }
    val activityDashboard by lazy { dev.esp32.smartwatch.activity.ActivityDashboardRepository(this, companion.state) }
    val health: dev.esp32.smartwatch.health.HealthConnectSyncManager by lazy {
        dev.esp32.smartwatch.health.HealthConnectSyncManager(this, { companion.state.value }, { companion.repository.requestActivitySnapshot() })
    }
    fun forwardNotification(item: PhoneNotification) = companion.forwardNotification(item)
}
