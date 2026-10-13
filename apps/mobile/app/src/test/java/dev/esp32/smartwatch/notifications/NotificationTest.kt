package dev.esp32.smartwatch.notifications

import org.junit.Assert.*
import org.junit.Test

class NotificationTest {
    @Test fun filtersNoiseAndNormalizes() {
        fun capture(pkg: String = "demo.app", title: String? = "David", body: String? = "Message", ongoing: Boolean = false,
                    summary: Boolean = false, service: Boolean = false) = NotificationPolicy.normalize(pkg, "companion", null, title, body, 123,
            ongoing, summary, service)
        assertNull(capture(pkg = "companion")); assertNull(capture(pkg = "android"))
        assertNull(capture(pkg = "com.android.systemui")); assertNull(capture(ongoing = true))
        assertNull(capture(summary = true)); assertNull(capture(service = true))
        assertNull(capture(title = null, body = " \n\t"))
        assertNull(capture(title = "\uD83D\uDE00", body = null))
        val item = capture(title = "  David\n Pérez ", body = "Hello\u0000\uD83D\uDE00  there")!!
        assertEquals("David Pérez", item.title); assertEquals("Hello there", item.body)
        assertEquals("demo.app", item.appLabel); assertEquals(123L, item.timestamp)
        assertNotNull(capture(title = null))
    }
    @Test fun boundedUtf8AndFrames() {
        val item = NotificationPolicy.normalize("p".repeat(200), "self", "é".repeat(100), "x".repeat(200), "é".repeat(300), 0)!!
        assertEquals(96, item.packageName.toByteArray().size)
        assertEquals(48, item.appLabel.toByteArray().size)
        assertEquals(96, item.title.toByteArray().size)
        assertEquals(240, item.body.toByteArray().size)
        val frames = NotificationTransport.frames(item, 65537)
        assertEquals(42, frames.size)
        val bytes = frames.flatMap { it.drop(8) }.toByteArray()
        assertEquals(496, bytes.size)
        frames.forEachIndexed { index, frame ->
            assertTrue(frame.size in 9..20)
            assertEquals(1, frame[0].toInt()); assertEquals(1, frame[1].toInt())
            assertEquals(1, frame[2].toInt()); assertEquals(0, frame[3].toInt())
            assertEquals(index * 12, (frame[4].toInt() and 255) or ((frame[5].toInt() and 255) shl 8))
        }
        assertThrows(IllegalArgumentException::class.java) { NotificationTransport.frames(item.copy(body = "x".repeat(241)), 1) }
        assertThrows(IllegalArgumentException::class.java) { NotificationTransport.frames(item.copy(timestamp = -1), 1) }
    }
}
