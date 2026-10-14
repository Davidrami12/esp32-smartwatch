# Offline BLE voice window optimization

Scope: `feature/mobile-android-foundation`; no hardware tests, COM9 flash, Pixel
install, OpenAI/Firebase networking, deployment, infrastructure/secret/quota/model
changes, commit or push. Production backend/Spanish policy and watch UI unchanged.

## Confirmed versus suspected bottlenecks

Confirmed baseline: 82560-byte / 2.58s clip, MTU 517, 165 pull chunks, whole-clip
CRC PASS, 30087ms Android transfer, 2744 bytes/s; ~39662ms capture-complete to
final answer assembly, not release-to-panel-scanout. Pull uses one acknowledged
selection write and one read per chunk: **330 serialized GATT operations**. The
single Handler-owned queue prevents overlapping operations and permits media/
phone-notification turns. Metadata polling adds up to two seconds of discovery
delay (1.492s in the recorded sample), not the dominant ~30 seconds of transfer.

Observed average is ~182ms/chunk including queue, write/read and callback time.
It is **not** a measured connection interval or read/write split. Android HIGH
priority / watch 15–30ms parameter requests are advisory; connection update
diagnostics now report negotiated interval, latency and supervision units without
device/user identifiers. Default idle watch request is 100–200ms with latency 4.
Android 14+ requests MTU 517 on the first request; actual callback MTU is still
used. The new code uses API33 byte-array notification callbacks, not mutable
characteristic value getters. Radio fragmentation, actual callback scheduling,
media queue cost and connection-parameter effects remain unisolated physically.
Inspection also confirmed a fairness corner case: continuous media changes could
make the old pump repeatedly yield the same lone voice operation. The pump now
allows one normal-traffic turn before executing that voice operation, then resets
the yield allowance for the next operation/window. Its contribution to the
historical 30-second run is unknown; a policy regression test guards starvation.

## Alternatives evaluated

| Option | Theoretical benefit | CPU / memory / battery considerations | Risk |
| --- | --- | --- | --- |
| Four-credit lossless PCM notifications (chosen) | Amortizes application ACK over four chunks; removes per-chunk read + select cycle | One 512-byte watch scratch buffer; existing PCM retained; Android same <=256kB clip plus <=16 callback copies (8KiB). Copy/CRC unchanged; fewer application round trips may reduce awake time, but higher packet rate/fast connection may increase instantaneous power | New flow/retry protocol and real stack behavior need hardware validation |
| Versioned IMA ADPCM, decode to PCM WAV on Android | ~4:1 audio payload reduction; could reduce legacy pull chunks roughly fourfold | Linear encode/decode CPU and predictor/index state, encoded storage or block scratch; potentially shorter radio awake time. CPU/battery have NOT been benchmarked | Lossy speech quality, independent block/index bounds, encoded vs decoded CRC, exact sample count/padding and malformed-stream validation; needs codec capability plus listening/transcription tests |
| Pipeline simultaneous GATT reads/writes | Uncertain | Android generally expects serialized outstanding operations | Unsafe callback attribution/reliability; rejected |
| Faster metadata polling | Reduces only discovery delay | More idle radio work/battery | Not the measured dominant transfer bottleneck; unchanged |
| Silence trimming | Payload reduction depends on speech | VAD CPU/buffers | Can cut consonants/context; rejected without accuracy evidence |

IMA ADPCM is **evaluated, not implemented**. The chosen transport is lossless and
keeps the deployed backend's PCM WAV contract bit-identical; it does not require
decoder changes or a cloud rollout. Neither speed nor speech quality is traded
for an assumed codec gain.

## Exact implementation

Authoritative byte layouts: `ble-protocol.md`, optional voice transport v2.
Protected capabilities `0013` and notification data `0014`; existing metadata,
pull, process/cancel and result characteristics remain compatible. Four-frame
grants use a separate version-2 opcode-4 layout on existing secured `0011`.
Each grant carries session, monotonically increasing epoch, cumulative PCM prefix
and credits. Frames carry epoch, offset, sequence and final-window flag. Sender
never advances after failed enqueue. Replayed grants cannot refill credits.

At most one frame/10ms host event, four enqueues/window; callback copy queue <=16.
Receiver never stores out-of-order payloads. It ignores duplicates/stale epochs,
requests a new epoch from the contiguous prefix after five seconds of inactivity,
allows three no-progress retries, then resumes legacy pull without recapture.
Absolute deadlines remain 175s Android / 180s claimed watch / 30s unclaimed READY.
Final CRC mismatch cannot produce `takeAudio()` even before cleanup; cancelled
receivers cannot be reused. Existing Clear/Back/disconnect, authentication and
encrypted trusted-peer checks remain mandatory. Final process ACK and result
delivery remain the production path.

Builds can select legacy pull with `-PvoiceNotify=false` for controlled A/B. New
firmware defaults to supporting both; no audio pushes until the verified peer
subscribes AND grants credits. Old peers continue to pull. Capability/CCCD failure,
unknown schema/codec, rejected grant or retry exhaustion fall back safely. Loss,
duplicates and late callbacks remain possible and are handled at application
level even though BLE provides link-layer retransmission.

### Offline counts, not a speed measurement

For the baseline 82560 bytes at MTU 517:

- Legacy: 165 chunks, 330 acknowledged GATT operations.
- New model/production receiver: 167 PCM notifications (496 PCM bytes/full frame),
  **42 acknowledged grants**, then the same final process command.
- This is ~87% fewer central audio GATT request operations, NOT an 87% latency
  improvement. Header overhead is slightly higher; radio/L2CAP fragmentation,
  acknowledgements, grants, scheduling and fairness still consume airtime.
- No after milliseconds, physical throughput or battery/CPU speedup is reported.

## Paid-consent audit

Historical exact cause is **not recoverable**: prior logs never recorded checkbox
state, consent epoch or HTTP request commitment. Real result frames prove a live
request occurred; they do not prove the checkbox was disabled. Previously enabled
consent persisted within that app process unless logout/login/restart reset it.
The operator may not have disabled it, or one of the following code gaps may have
applied; do not label either explanation as the confirmed historical root cause.

Confirmed old-code gaps:

1. Disabling only changed a boolean; queued/active calls were not cancelled.
2. Consent was checked once before token refresh/WAV/HTTP work, not at send.
3. Queue execution read the current boolean: disable then re-enable could allow
   an older recording. BLE did not bind eligibility when claiming the clip.

Fix: revocable generation tickets, captured at BLE claim (null if OFF), propagated
unchanged through receipt/process/queue. Every consent change advances generation
and cancels queued/active calls. Work checks its ticket before refresh, after
refresh and immediately before committing the network stage. Re-enabling never
resurrects an older ticket. Results are rechecked before returning/queueing, and
revocation produces a sanitized status instead of silently waiting 90 seconds.
No automatic HTTP retry is added. Login's empty-audio UID probe remains unchanged
and cannot reach quota reservation/OpenAI.

The request-commit check is synchronized and nonblocking; no network operation
runs while holding the consent lock. Requests committed BEFORE revocation are
cancelled/disconnected best-effort. A request already submitted/accepted remotely
may still incur a charge; revocation is not a remote transaction rollback or a
billing refund guarantee. A request queued/claimed while OFF or carrying a revoked
ticket cannot newly commit, including after refresh, fallback/retry or re-enable.
The tiny distinction between authorization commitment and actual socket delivery
is inherent in cancelling in-flight HTTP; no claim of remote recall is made.

## Offline validation

Results: firmware build **PASS**; default notification-mode Android build,
**67 unit tests**, lint **PASS**; legacy `-PvoiceNotify=false` build and the same
67 tests **PASS**; C++ sender host test **PASS**; `git diff --check` **PASS**.
Simulator build **N/A** (no simulator/UI runtime code changed). Device flash,
Pixel installation, boot logs and physical A/B tests **SKIPPED by instruction**.
Final local APK output is the legacy A/B build; rebuild with
`-PvoiceNotify=true` before an approved notification-mode installation.
No backend code/configuration changed in this task. Security/consent integration
with actual HTTP cancellation and real notification callback timing is not proven
by these isolated policy/transport tests; hardware tests remain mandatory.

- Firmware build without flashing; Android build/unit tests/lint without install.
- Kotlin receiver tests at MTU 64/247/256/517: bit-identical PCM and WAV/CRC,
  credit/grant wire bytes, capability fallback, duplicate/gap/stale-epoch handling,
  loss/retry exhaustion/legacy-prefix resume, malformed flags/frame bounds,
  corrupted CRC, cancelled receiver and repeated-window loss.
- Pure C++ sender host test: credits cannot overrun, replay/foreign-session grants
  rejected, even payload/frame caps, prefix retry, 167-frame/42-grant fixture.
- Consent tests: OFF capture then opt-in, disable/re-enable queued ticket, simulated
  delayed token refresh, and generation-safe commit. No external network.
- Integration of real NimBLE/Android callbacks, actual credit pacing, MTU/connection
  updates, media responsiveness, loss recovery and battery remain hardware-pending.

## Physical A/B plan — REQUIRES explicit approval

1. Obtain approval separately for COM9 flash, Pixel install and unpaid physical
   tests. Nothing is run automatically from this document.
2. Build/save A with `-PvoiceNotify=false`, B with `-PvoiceNotify=true`; same new
   firmware, consent fix, sample rate and audio data. Prefer an approved deterministic
   synthetic PCM transport fixture for identical bytes, or control clip durations
   carefully; human speech recordings are not bit-identical A/B inputs.
3. Ensure paid AI OFF and sign out before each run; confirm consent eligibility
   false in logs. Final login/consent errors are expected; no cloud request needed.
4. Run at least three ~2.58s and three ~7–8s clips per mode, alternate A/B order,
   same watch/phone placement, battery conditions and normal companion traffic.
5. Compare medians/range: discovery, transfer, PCM bytes/s, MTU, payload count,
   grant/retry counts, CRC, actual negotiated connection parameters and callback
   queue. Compare counts separately from timing; do not infer throughput from MTU.
6. Test Clear/Back mid-window, disconnect/reconnect, dropped-window/retry/fallback,
   legacy peer compatibility, media controls and notification forwarding during
   transfers. Confirm ports/monitors close cleanly. No new OpenAI calls.
7. Only after this passes, propose one separately approved short live Spanish test
   with fresh consent and a new recording, then turn consent off. Backend remains
   untouched; models/limits/secrets/quotas stay unchanged.
