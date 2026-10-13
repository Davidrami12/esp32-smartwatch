package dev.esp32.smartwatch.media

import dev.esp32.smartwatch.notifications.NotificationPolicy

data class PhoneMedia(val state: Int = 0, val actions: Int = 0, val session: Long = 0, val title: String = "", val artist: String = "")
data class MediaCommand(val action: Int, val session: Long, val sequence: Int)
object MediaProtocol {
    fun normalized(state: Int, actions: Int, session: Long, title: String?, artist: String?): PhoneMedia =
        if (state == 0) PhoneMedia() else PhoneMedia(state, actions, session,
            NotificationPolicy.sanitize(title, 144), NotificationPolicy.sanitize(artist, 96))
    fun frames(media: PhoneMedia, id: Int, volumeSupported: Boolean = false): List<ByteArray> {
        val title = media.title.toByteArray(Charsets.UTF_8); val artist = media.artist.toByteArray(Charsets.UTF_8)
        require(media.state in 0..2 && media.actions in 0..31 && title.size <= 144 && artist.size <= 96)
        require(if (media.state == 0) media == PhoneMedia() else media.session in 1..0xffffffffL)
        val body = ByteArray(12 + title.size + artist.size)
        body[0] = if (volumeSupported) 2 else 1; body[1] = media.state.toByte(); body[2] = (media.actions and if (volumeSupported) 31 else 7).toByte()
        for (i in 0..3) body[4+i] = (media.session shr (8*i)).toByte()
        put16(body, 8, title.size); put16(body, 10, artist.size)
        title.copyInto(body, 12); artist.copyInto(body, 12+title.size)
        return (body.indices step 12).map { offset -> ByteArray(8 + minOf(12, body.size-offset)).also {
            it[0] = 1; it[1] = 2; put16(it, 2, id and 65535); put16(it, 4, offset); put16(it, 6, body.size)
            body.copyInto(it, 8, offset, minOf(body.size, offset+12))
        } }
    }
    fun command(bytes: ByteArray): MediaCommand {
        require(bytes.size == 8 && bytes[0].toInt() == 1 && bytes[1].toInt() in 0..5)
        val action = bytes[1].toInt()
        var session = 0L
        for (i in 0..3) session = session or ((bytes[2+i].toLong() and 255) shl (8*i))
        val sequence = (bytes[6].toInt() and 255) or ((bytes[7].toInt() and 255) shl 8)
        require(if (action == 0) session == 0L && sequence == 0 else session != 0L)
        return MediaCommand(action, session, sequence)
    }
    fun isNewSequence(previous: Int?, next: Int): Boolean = previous == null || ((next-previous) and 65535) in 1..32767
    fun permitted(command: MediaCommand, media: PhoneMedia): Boolean = media.state in 1..2 &&
        command.session == media.session && command.action in 1..5 && media.actions and (1 shl (command.action-1)) != 0
    private fun put16(bytes: ByteArray, offset: Int, value: Int) { bytes[offset] = value.toByte(); bytes[offset+1] = (value shr 8).toByte() }
}
