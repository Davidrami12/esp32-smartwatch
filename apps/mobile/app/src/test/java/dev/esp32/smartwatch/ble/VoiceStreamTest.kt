package dev.esp32.smartwatch.ble

import org.junit.Assert.*
import org.junit.Test
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.util.zip.CRC32
import dev.esp32.smartwatch.ai.VoiceAiClient

class VoiceStreamTest {
    private val audio = ByteArray(82560) { (it % 127).toByte() }
    private fun receiver(bytes: ByteArray = audio) = VoiceProtocol.Receiver(VoiceProtocol.Metadata(77,bytes.size,CRC32().apply { update(bytes) }.value))
    private fun frame(stream: VoiceProtocol.StreamReceiver, offset: Int, sequence: Int, mtu: Int = 517, bytes: ByteArray = audio): ByteArray {
        val payload = (minOf(512,mtu-3)-15) and -2
        val count = minOf(payload,bytes.size-offset)
        return ByteBuffer.allocate(15+count).order(ByteOrder.LITTLE_ENDIAN)
            .put(2.toByte()).putInt(77).putInt(stream.epoch).putInt(offset).put(sequence.toByte())
            .put(if (sequence == 3 || offset+count == bytes.size) 1.toByte() else 0.toByte())
            .put(bytes,offset,count).array()
    }
    @Test fun capabilityIsExplicitAndUnknownSchemasFallBack() {
        assertTrue(VoiceProtocol.supportsStream(byteArrayOf(2,1,4,15,0,2)))
        assertFalse(VoiceProtocol.supportsStream(byteArrayOf(3,1,4,15,0,2)))
        assertFalse(VoiceProtocol.supportsStream(byteArrayOf(2,2,4,15,0,2)))
        assertFalse(VoiceProtocol.supportsStream(byteArrayOf()))
    }
    @Test fun continuousMediaChurnCannotRepeatedlyPushBackTheSameGrant() {
        assertTrue(VoiceProtocol.shouldYieldVoice(true,false,1,VoiceProtocol.CONTROL))
        // After one media/notification turn, the same grant must execute next.
        assertFalse(VoiceProtocol.shouldYieldVoice(true,true,1,VoiceProtocol.CONTROL))
        assertTrue(VoiceProtocol.shouldYieldVoice(true,false,1,VoiceProtocol.DATA))
        assertFalse(VoiceProtocol.shouldYieldVoice(true,false,2,VoiceProtocol.CONTROL))
        assertFalse(VoiceProtocol.shouldYieldVoice(false,false,1,VoiceProtocol.CONTROL))
    }
    @Test fun grantsHaveIndependentVersionSessionEpochAndCredits() {
        val grant = VoiceProtocol.grant(77,2,496)
        assertEquals(15,grant.size)
        val b = ByteBuffer.wrap(grant).order(ByteOrder.LITTLE_ENDIAN)
        assertEquals(2,b.get(0).toInt()); assertEquals(4,b.get(1).toInt())
        assertEquals(77,b.getInt(2)); assertEquals(2,b.getInt(6)); assertEquals(496,b.getInt(10))
        assertEquals(4,b.get(14).toInt())
    }
    @Test fun losslessTransferAtMultipleMtusPreservesCrcAndBackendWav() {
        for (mtu in listOf(64,247,256,517)) {
            val receiver = receiver(); val stream = VoiceProtocol.StreamReceiver(receiver,mtu)
            var frames = 0; var grants = 0
            while (receiver.offset < audio.size) {
                stream.nextGrant(); grants++
                for (sequence in 0..3) {
                    val event = stream.accept(frame(stream,receiver.offset,sequence,mtu))
                    frames++
                    if (event == VoiceProtocol.StreamReceiver.Event.COMPLETE) break
                }
            }
            val pcm = receiver.takeAudio()
            assertArrayEquals(audio,pcm)
            val wav = VoiceAiClient.wav(pcm)
            assertEquals(audio.size+44,wav.size)
            assertArrayEquals(audio,wav.copyOfRange(44,wav.size))
            if (mtu == 517) { assertEquals(167,frames); assertEquals(42,grants) }
            receiver.clear()
        }
    }
    @Test fun droppedFrameRecoversContiguousPrefixWithNewEpoch() {
        val r = receiver(); val s = VoiceProtocol.StreamReceiver(r,517); s.nextGrant()
        val first = frame(s,0,0); val dropped = frame(s,496,1); val later = frame(s,992,2)
        assertEquals(VoiceProtocol.StreamReceiver.Event.PROGRESS,s.accept(first))
        assertEquals(VoiceProtocol.StreamReceiver.Event.IGNORE,s.accept(first))
        assertEquals(VoiceProtocol.StreamReceiver.Event.GAP,s.accept(later))
        assertEquals(496,r.offset)
        s.nextGrant(true)
        assertEquals(VoiceProtocol.StreamReceiver.Event.IGNORE,s.accept(dropped))
        assertEquals(VoiceProtocol.StreamReceiver.Event.PROGRESS,s.accept(frame(s,496,0)))
    }
    @Test fun lostLastFrameAndRetryExhaustionAreBounded() {
        val r = receiver(); val s = VoiceProtocol.StreamReceiver(r,517); s.nextGrant()
        for (seq in 0..2) s.accept(frame(s,r.offset,seq))
        val at = r.offset
        repeat(3) { s.nextGrant(true) }
        assertThrows(IllegalArgumentException::class.java) { s.nextGrant(true) }
        assertEquals(at,r.offset)
        // Legacy pull can resume from exactly the verified prefix, without recapture.
        val count = 502
        val legacy = ByteBuffer.allocate(9+count).order(ByteOrder.LITTLE_ENDIAN)
            .put(1.toByte()).putInt(77).putInt(at).put(audio,at,count).array()
        assertFalse(r.accept(legacy)); assertEquals(at+count,r.offset)
    }
    @Test fun staleSessionMalformedFlagsAndCorruptAudioAreRejected() {
        val r = receiver(); val s = VoiceProtocol.StreamReceiver(r,517); s.nextGrant()
        val stale = frame(s,0,0).apply { this[1] = 1 }
        assertEquals(VoiceProtocol.StreamReceiver.Event.IGNORE,s.accept(stale)); assertEquals(0,r.offset)
        assertThrows(IllegalArgumentException::class.java) { s.accept(frame(s,0,0).apply { this[14] = 1 }) }
        assertThrows(IllegalArgumentException::class.java) { s.accept(ByteArray(513)) }
        val corrupted = audio.copyOf().apply { this[80000] = (this[80000].toInt() xor 1).toByte() }
        assertThrows(IllegalArgumentException::class.java) {
            while (r.offset < audio.size) {
                s.nextGrant()
                for (seq in 0..3) {
                    s.accept(frame(s,r.offset,seq,bytes=corrupted))
                    if (r.offset == audio.size) break
                }
            }
        }
        assertThrows(IllegalArgumentException::class.java) { r.takeAudio() }
        r.clear(); assertEquals(0,r.offset)
    }
    @Test fun cancelledReceiverCannotGrantOrResume() {
        val r = receiver(); val s = VoiceProtocol.StreamReceiver(r,517); s.nextGrant()
        val pending = frame(s,0,0)
        r.clear()
        assertThrows(IllegalArgumentException::class.java) { s.nextGrant(true) }
        assertThrows(IllegalArgumentException::class.java) { s.accept(pending) }
        assertThrows(IllegalArgumentException::class.java) { r.takeAudio() }
    }
    @Test fun repeatedWindowLossDuplicatesAndLatePacketsRecoverWithoutExtraAudioBuffers() {
        val r = receiver(); val s = VoiceProtocol.StreamReceiver(r,517)
        var rounds = 0
        while (r.offset < audio.size) {
            s.nextGrant()
            val stale = frame(s,r.offset,0)
            // Deliver a prefix, drop the second frame, and see a gap from a later one.
            if (audio.size-r.offset > 496*3 && rounds++ % 3 == 0) {
                assertEquals(VoiceProtocol.StreamReceiver.Event.PROGRESS,s.accept(stale))
                assertEquals(VoiceProtocol.StreamReceiver.Event.GAP,s.accept(frame(s,r.offset+496,2)))
                s.nextGrant(true)
                assertEquals(VoiceProtocol.StreamReceiver.Event.IGNORE,s.accept(stale))
            }
            for (seq in 0..3) {
                val packet = frame(s,r.offset,seq)
                val event = s.accept(packet)
                assertEquals(VoiceProtocol.StreamReceiver.Event.IGNORE,s.accept(packet))
                if (event == VoiceProtocol.StreamReceiver.Event.COMPLETE) break
            }
        }
        assertArrayEquals(audio,r.takeAudio())
    }
}
