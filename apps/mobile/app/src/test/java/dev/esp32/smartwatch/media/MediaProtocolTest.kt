package dev.esp32.smartwatch.media
import org.junit.Assert.*
import org.junit.Test

class MediaProtocolTest {
    @Test fun boundedFramesAndUnavailable() {
        val value = MediaProtocol.normalized(1, 7, 0xffffffffL, "é".repeat(200), "a".repeat(200))
        assertEquals(144, value.title.toByteArray().size); assertEquals(96, value.artist.toByteArray().size)
        val frames = MediaProtocol.frames(value, 65537)
        assertEquals(21, frames.size)
        frames.forEachIndexed { index, frame ->
            assertTrue(frame.size in 9..20); assertEquals(1, frame[0].toInt()); assertEquals(2, frame[1].toInt())
            assertEquals(1, frame[2].toInt()); assertEquals(index*12, (frame[4].toInt() and 255) or ((frame[5].toInt() and 255) shl 8))
        }
        assertEquals(252, frames.sumOf { it.size-8 })
        assertEquals(1, MediaProtocol.frames(PhoneMedia(), 0).size)
        assertEquals(PhoneMedia(), MediaProtocol.normalized(0, 7, 12, "secret", "artist"))
        assertThrows(IllegalArgumentException::class.java) { MediaProtocol.frames(PhoneMedia(1, 7, 0), 0) }
        assertThrows(IllegalArgumentException::class.java) { MediaProtocol.frames(PhoneMedia(0, 1, 1), 0) }
        assertThrows(IllegalArgumentException::class.java) { MediaProtocol.frames(value.copy(title="x".repeat(145)), 0) }
    }
    @Test fun commandsAndReplayProtection() {
        assertEquals(MediaCommand(2, 0xffffffffL, 65535), MediaProtocol.command(byteArrayOf(1, 2, -1, -1, -1, -1, -1, -1)))
        assertEquals(0, MediaProtocol.command(byteArrayOf(1, 0, 0, 0, 0, 0, 0, 0)).action)
        assertThrows(IllegalArgumentException::class.java) { MediaProtocol.command(byteArrayOf(1, 2, 0, 0, 0, 0, 1, 0)) }
        assertThrows(IllegalArgumentException::class.java) { MediaProtocol.command(byteArrayOf(1, 6, 1, 0, 0, 0, 1, 0)) }
        assertThrows(IllegalArgumentException::class.java) { MediaProtocol.command(ByteArray(9)) }
        assertTrue(MediaProtocol.isNewSequence(null, 0)); assertTrue(MediaProtocol.isNewSequence(65535, 0))
        assertFalse(MediaProtocol.isNewSequence(1, 1)); assertFalse(MediaProtocol.isNewSequence(2, 1))
        val value = PhoneMedia(1, 2, 99, "Track", "Artist")
        assertTrue(MediaProtocol.permitted(MediaCommand(2, 99, 1), value))
        assertFalse(MediaProtocol.permitted(MediaCommand(2, 100, 1), value))
        assertFalse(MediaProtocol.permitted(MediaCommand(3, 99, 1), value))
        assertFalse(MediaProtocol.permitted(MediaCommand(2, 99, 1), PhoneMedia()))
        assertTrue(MediaProtocol.permitted(MediaCommand(4, 99, 1), value.copy(actions=31)))
        assertTrue(MediaProtocol.permitted(MediaCommand(5, 99, 1), value.copy(actions=31)))
        assertFalse(MediaProtocol.permitted(MediaCommand(4, 99, 1), value))
    }
    @Test fun volumeExtensionPreservesLegacySnapshot() {
        val media = PhoneMedia(1, 31, 99)
        val legacy = MediaProtocol.frames(media, 1).first()
        assertEquals(1, legacy[8].toInt()); assertEquals(7, legacy[10].toInt())
        val volume = MediaProtocol.frames(media, 1, volumeSupported=true).first()
        assertEquals(2, volume[8].toInt()); assertEquals(31, volume[10].toInt())
        assertEquals(5, MediaProtocol.command(byteArrayOf(1,5,99,0,0,0,1,0)).action)
    }
}
