package dev.esp32.smartwatch.ai

/** Only refresh credentials are persisted; never a password or an ID token. */
internal data class SavedVoiceSession(val refresh: String, val key: String, val uid: String)
internal interface VoiceSessionStore {
    fun load(): SavedVoiceSession?
    fun save(session: SavedVoiceSession)
    fun clear()
}
