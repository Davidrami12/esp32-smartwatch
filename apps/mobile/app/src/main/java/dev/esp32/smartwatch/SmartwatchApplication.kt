package dev.esp32.smartwatch

import android.app.Application
import dev.esp32.smartwatch.companion.CompanionConnection
import dev.esp32.smartwatch.notifications.PhoneNotification

// Process-local boundary; no GATT object exposed to the listener or Compose.
class SmartwatchApplication : Application() {
    val companion: CompanionConnection by lazy { CompanionConnection(this) }
    fun forwardNotification(item: PhoneNotification) = companion.forwardNotification(item)
}
