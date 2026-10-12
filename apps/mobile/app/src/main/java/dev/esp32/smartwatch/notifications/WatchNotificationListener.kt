package dev.esp32.smartwatch.notifications

import android.app.Notification
import android.service.notification.NotificationListenerService
import android.service.notification.StatusBarNotification
import android.util.Log
import dev.esp32.smartwatch.SmartwatchApplication
import java.util.LinkedHashMap

class WatchNotificationListener : NotificationListenerService() {
    private val recent = LinkedHashMap<String, PhoneNotification>()
    override fun onListenerConnected() {
        Log.i(TAG, "Notification listener connected")
        (application as SmartwatchApplication).companion.refreshNotificationAccess()
        // Never replay active notifications on grant or reconnect.
    }
    override fun onListenerDisconnected() {
        Log.i(TAG, "Notification listener disconnected")
        recent.clear()
        (application as SmartwatchApplication).companion.refreshNotificationAccess()
    }
    override fun onNotificationPosted(sbn: StatusBarNotification) {
        try {
            val n = sbn.notification
            val label = try { packageManager.getApplicationLabel(packageManager.getApplicationInfo(sbn.packageName, 0)).toString() }
                catch (_: android.content.pm.PackageManager.NameNotFoundException) { null }
            val item = NotificationPolicy.normalize(sbn.packageName, packageName, label,
                n.extras.getCharSequence(Notification.EXTRA_TITLE)?.toString(),
                (n.extras.getCharSequence(Notification.EXTRA_BIG_TEXT) ?: n.extras.getCharSequence(Notification.EXTRA_TEXT))?.toString(),
                sbn.postTime, n.flags and Notification.FLAG_ONGOING_EVENT != 0,
                n.flags and Notification.FLAG_GROUP_SUMMARY != 0,
                n.flags and Notification.FLAG_FOREGROUND_SERVICE != 0 || n.category == Notification.CATEGORY_SERVICE)
            if (item == null) { Log.d(TAG, "Notification filtered (noise/empty/unsupported content)"); return }
            val previous = recent[sbn.key]
            if (previous != null && previous.copy(timestamp = item.timestamp) == item) {
                Log.d(TAG, "Unchanged notification update filtered"); return
            }
            recent.remove(sbn.key); recent[sbn.key] = item
            if (recent.size > 32) recent.remove(recent.keys.first())
            Log.i(TAG, "Eligible notification captured")
            (application as SmartwatchApplication).forwardNotification(item)
        } catch (_: RuntimeException) { Log.w(TAG, "Unreadable notification filtered") }
    }
    override fun onNotificationRemoved(sbn: StatusBarNotification) { recent.remove(sbn.key) }
    companion object { private const val TAG = "WatchNotifications" }
}
