# Bounded watch microphone capture

- **Status:** Ready for approval — retrospective; implemented.
- **Owner:** Project maintainer; historical formal approval unavailable.
- **Last updated:** 2026-10-23.
- **Sources:** `a3f6af4`; `apps/watch/main/watch_voice.*`, shared voice UI.
- **Related ADR:** [0008](../../adr/0008-lossless-pcm-and-credit-window-transport.md).

## 1. Goal and context
Capture intentional speech on the watch without continuous recording, file storage
or blocking LVGL during codec work.

## 2. Scope
Push-to-talk, microphone codec, PCM buffer, duration, cancellation and handoff.
Exclude microSD recorder, background listening, ADPCM and speaker response synthesis.

## 3. Requirements (EARS)
| ID | Pattern | Requirement |
| --- | --- | --- |
| CAP-01 | Event-driven | When an authorized recording interaction begins, the watch shall capture mono PCM16 at 16 kHz for at most eight seconds. |
| CAP-02 | Event-driven | When recording is released/completed, the watch shall finalize bounded audio and its integrity metadata for the current session. |
| CAP-03 | Event-driven | When capture is canceled or its session invalidated, the firmware shall release/clear owned buffers and prevent stale submission. |
| CAP-04 | Ubiquitous | Codec capture shall run outside UI event handling and apply the persisted microphone gain on subsequent recordings. |

## 4. Acceptance scenarios
- **AC-01 (CAP-01/02):** Given a held control, when released before/at the maximum duration, then sample rate, duration, byte count and CRC match the finalized clip.
- **AC-02 (CAP-03/04):** Given a canceled recording and gain edit, when a new session starts, then old PCM is not reused and the new gain applies without UI-thread audio work.

## 5. Constraints and decisions
At most 256,000 PCM bytes for eight seconds; backend accepts a 44-byte WAV header
in addition. ES7210 codec setup is platform-specific; transfer remains lossless.

## 6. Implementation record
Audio worker and PSRAM buffer in `watch_voice.*`; firmware/session callbacks hand
state to UI/BLE. No audio files are introduced.

## 7. Test plan
FW/BOOT plus unpaid TRANSFER build; manual short/maximum clip, cancel, busy preview,
gain and integrity checks. A spoken live AI test is optional, explicit and billable.

## 8. Documentation impact
Voice assistant and local audio notes; temporary capture is distinct from a recorder app.

## 9. Risks and approval
Microphone quality, buffer ownership and codec concurrency; ratification pending.

## 10. Validation record
Physical mic-to-phone PCM/CRC was reported successful during development. Identical
audio quality A/B and broad microphone calibration remain unestablished. No capture here.

## 11. History
2026-10-23: retrospective hardware capture and cancellation contract.
