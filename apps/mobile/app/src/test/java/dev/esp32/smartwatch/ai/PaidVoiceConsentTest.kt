package dev.esp32.smartwatch.ai

import org.junit.Assert.*
import org.junit.Test
import java.util.concurrent.CountDownLatch
import java.util.concurrent.TimeUnit

class PaidVoiceConsentTest {
    @Test fun disabledRecordingsCannotBecomePaidByLaterOptIn() {
        val gate = PaidVoiceConsent()
        val disabledClip = gate.ticket()
        gate.set(true)
        assertFalse(gate.permits(disabledClip))
        var sends = 0
        assertFalse(gate.commit(disabledClip) { sends++ })
        assertEquals(0,sends)
    }
    @Test fun queuedRefreshedAndRetriedTicketsRemainRevokedAfterReenable() {
        val gate = PaidVoiceConsent(); gate.set(true)
        val queued = gate.ticket()
        gate.set(false); assertFalse(gate.permits(queued))
        gate.set(true)
        assertFalse(gate.permits(queued))
        assertFalse(gate.commit(queued) { fail("Old recording must not send") })
        assertTrue(gate.commit(gate.ticket()) {})
    }
    @Test fun revokeDuringSimulatedTokenRefreshPreventsCommit() {
        val gate = PaidVoiceConsent(); gate.set(true)
        val ticket = gate.ticket()
        val refreshStarted = CountDownLatch(1); val resume = CountDownLatch(1); val done = CountDownLatch(1)
        var committed = true
        val worker = Thread {
            assertTrue(gate.permits(ticket)); refreshStarted.countDown()
            assertTrue(resume.await(3,TimeUnit.SECONDS))
            committed = gate.commit(ticket) { fail("No request may begin after revocation") }
            done.countDown()
        }
        worker.start(); assertTrue(refreshStarted.await(3,TimeUnit.SECONDS))
        gate.set(false); gate.set(true); resume.countDown()
        assertTrue(done.await(3,TimeUnit.SECONDS)); worker.join()
        assertFalse(committed)
    }
    @Test fun validTicketCommitsOnlyWhileItsGenerationIsEnabled() {
        val gate = PaidVoiceConsent(); gate.set(true)
        val ticket = gate.ticket(); var sends = 0
        assertTrue(gate.commit(ticket) { sends++ })
        gate.set(false)
        assertFalse(gate.commit(ticket) { sends++ })
        assertEquals(1,sends)
    }
}
