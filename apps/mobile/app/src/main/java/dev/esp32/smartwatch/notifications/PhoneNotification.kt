package dev.esp32.smartwatch.notifications

import java.text.Normalizer

data class PhoneNotification(val packageName: String, val appLabel: String, val title: String, val body: String, val timestamp: Long)

object NotificationPolicy {
    fun normalize(packageName: String, ownPackage: String, label: String?, title: String?, body: String?, timestamp: Long,
                  ongoing: Boolean = false, groupSummary: Boolean = false, service: Boolean = false): PhoneNotification? {
        if (packageName == ownPackage || packageName == "android" || packageName == "com.android.systemui" || ongoing || groupSummary || service) return null
        val pkg = sanitize(packageName, 96)
        val heading = sanitize(title, 96)
        val text = sanitize(body, 240)
        if (pkg.isEmpty() || (heading.isEmpty() && text.isEmpty())) return null
        return PhoneNotification(pkg, sanitize(label, 48).ifEmpty { sanitize(packageName, 48) }, heading, text, timestamp.coerceAtLeast(0))
    }
    fun sanitize(value: String?, maxBytes: Int): String {
        val clean = Normalizer.normalize(value.orEmpty().take(4096), Normalizer.Form.NFC).codePoints().toArray().asSequence().mapNotNull { cp ->
            when {
                Character.isWhitespace(cp) -> ' '
                cp in 32..126 || cp in 160..255 -> cp.toChar()
                else -> null // First-version watch fonts: no emoji or unsupported scripts.
            }
        }.joinToString("").trim().replace(Regex(" +"), " ")
        val out = StringBuilder()
        var count = 0
        for (c in clean) {
            val size = if (c.code < 128) 1 else 2
            if (count + size > maxBytes) break
            out.append(c); count += size
        }
        return out.toString()
    }
}

object NotificationTransport {
    const val MAX_MESSAGE = 512
    const val FRAME_VERSION = 1
    fun frames(item: PhoneNotification, id: Int): List<ByteArray> {
        val fields = listOf(item.packageName, item.appLabel, item.title, item.body).map { it.toByteArray(Charsets.UTF_8) }
        val limits = listOf(96, 48, 96, 240)
        require(fields.indices.all { fields[it].size <= limits[it] })
        require(fields[0].isNotEmpty() && (fields[2].isNotEmpty() || fields[3].isNotEmpty()))
        val total = 16 + fields.sumOf { it.size }
        require(total <= MAX_MESSAGE && item.timestamp >= 0)
        val message = ByteArray(total)
        for (i in 0..7) message[i] = (item.timestamp shr (8 * i)).toByte()
        var at = 16
        fields.forEachIndexed { i, value ->
            put16(message, 8 + 2 * i, value.size)
            value.copyInto(message, at); at += value.size
        }
        return (message.indices step 12).map { offset ->
            val count = minOf(12, total - offset)
            ByteArray(8 + count).also { frame ->
                frame[0] = FRAME_VERSION.toByte(); frame[1] = 1
                put16(frame, 2, id and 0xffff); put16(frame, 4, offset); put16(frame, 6, total)
                message.copyInto(frame, 8, offset, offset + count)
            }
        }
    }
    private fun put16(out: ByteArray, offset: Int, value: Int) {
        out[offset] = value.toByte(); out[offset + 1] = (value shr 8).toByte()
    }
}
