package dev.esp32.smartwatch.media

import android.app.NotificationManager
import android.content.ComponentName
import android.content.Context
import android.media.MediaMetadata
import android.media.AudioManager
import android.media.VolumeProvider
import android.media.session.*
import android.os.Handler
import android.os.Looper
import android.util.Log
import dev.esp32.smartwatch.notifications.WatchNotificationListener
import kotlin.random.Random

/** Native media APIs live only here, owned by the existing foreground companion. */
class MediaSessionBridge(context: Context, private val publish: (PhoneMedia) -> Unit) {
    private val context = context.applicationContext
    private val manager = this.context.getSystemService(MediaSessionManager::class.java)
    private val component = ComponentName(this.context, WatchNotificationListener::class.java)
    private val handler = Handler(Looper.getMainLooper())
    private val controllers = LinkedHashMap<MediaSession.Token, Pair<MediaController, MediaController.Callback>>()
    private var observing = false
    private var selected: MediaController? = null
    private var generation = Random.nextLong(1, 0x100000000L)
    private var current = PhoneMedia()
    private val sessions = MediaSessionManager.OnActiveSessionsChangedListener { if (observing) replace(it.orEmpty()) }
    fun refreshAccess() {
        val granted = context.getSystemService(NotificationManager::class.java).isNotificationListenerAccessGranted(component)
        if (!granted) { stop(); return }
        if (observing) return
        try {
            manager.addOnActiveSessionsChangedListener(sessions, component, handler)
            observing = true
            replace(manager.getActiveSessions(component))
            Log.i(TAG, "Native media session observation enabled")
        } catch (_: SecurityException) { stop(); Log.w(TAG, "Media session access unavailable") }
    }
    private fun replace(list: List<MediaController>) {
        controllers.values.forEach { it.first.unregisterCallback(it.second) }; controllers.clear()
        // Bounded observation, manager order = most recently active; no per-app logic.
        list.take(8).forEach { controller ->
            val callback = object : MediaController.Callback() {
                override fun onMetadataChanged(metadata: MediaMetadata?) = update()
                override fun onPlaybackStateChanged(state: PlaybackState?) = update()
                override fun onAudioInfoChanged(info: MediaController.PlaybackInfo) = update()
                override fun onSessionDestroyed() {
                    controllers.remove(controller.sessionToken)?.let { it.first.unregisterCallback(it.second) }
                    update()
                }
            }
            controllers[controller.sessionToken] = controller to callback
            controller.registerCallback(callback, handler)
        }
        update()
    }
    private fun update() {
        if (!observing) return
        try {
            val active = controllers.values.map { it.first }
            val next = active.firstOrNull { it.playbackState?.state == PlaybackState.STATE_PLAYING }
                ?: active.firstOrNull { it.playbackState?.state == PlaybackState.STATE_PAUSED }
            if (next?.sessionToken != selected?.sessionToken) generation = (generation % 0xffffffffL) + 1
            selected = next
            val playback = next?.playbackState
            if (next == null || playback == null) { emit(PhoneMedia()); return }
            val playing = playback.state == PlaybackState.STATE_PLAYING
            val available = playback.actions
            var actions = 0
            if (available and PlaybackState.ACTION_SKIP_TO_PREVIOUS != 0L) actions = actions or 1
            if (available and (PlaybackState.ACTION_PLAY_PAUSE or if (playing) PlaybackState.ACTION_PAUSE else PlaybackState.ACTION_PLAY) != 0L) actions = actions or 2
            if (available and PlaybackState.ACTION_SKIP_TO_NEXT != 0L) actions = actions or 4
            if (next.playbackInfo?.volumeControl?.let { it != VolumeProvider.VOLUME_CONTROL_FIXED } == true)
                actions = actions or 24
            val metadata = next.metadata
            emit(MediaProtocol.normalized(if (playing) 1 else 2, actions, generation,
                (metadata?.getText(MediaMetadata.METADATA_KEY_TITLE) ?: metadata?.getText(MediaMetadata.METADATA_KEY_DISPLAY_TITLE))?.toString(),
                (metadata?.getText(MediaMetadata.METADATA_KEY_ARTIST) ?: metadata?.getText(MediaMetadata.METADATA_KEY_ALBUM_ARTIST))?.toString()))
        } catch (_: SecurityException) { stop(); Log.w(TAG, "Media access revoked") }
    }
    private fun emit(value: PhoneMedia) {
        if (value == current) return
        current = value; publish(value)
        Log.i(TAG, "Media changed: state=${value.state}, actions=${value.actions}")
    }
    fun execute(command: MediaCommand) {
        // Refresh from supported APIs on a command/event, never periodic polling.
        update()
        val controller = selected ?: return
        if (!MediaProtocol.permitted(command, current)) {
            publish(current); Log.w(TAG, "Stale/unsupported media command ignored"); return
        }
        try {
            when (command.action) {
                1 -> controller.transportControls.skipToPrevious()
                2 -> if (current.state == 1) controller.transportControls.pause() else controller.transportControls.play()
                3 -> controller.transportControls.skipToNext()
                4 -> controller.adjustVolume(AudioManager.ADJUST_LOWER, 0)
                5 -> controller.adjustVolume(AudioManager.ADJUST_RAISE, 0)
            }
            Log.i(TAG, "Media command dispatched: ${command.action}")
            // Playback/metadata callbacks provide actual state, never optimistic success.
        } catch (_: SecurityException) { stop(); Log.w(TAG, "Media command denied") }
    }
    fun stop() {
        if (observing) try { manager.removeOnActiveSessionsChangedListener(sessions) } catch (_: SecurityException) { Log.w(TAG, "Media observer cleanup after access revocation") }
        observing = false
        controllers.values.forEach { it.first.unregisterCallback(it.second) }; controllers.clear(); selected = null
        emit(PhoneMedia())
    }
    companion object { private const val TAG = "WatchMedia" }
}
