# watch-ai-voice-assistant

## Current MVP implementation

The voice-input/text-response path is implemented: explicit bounded ES7210 recording
→ existing secure pull BLE transport → Android CRC validation/WAV → authenticated
HTTPS backend → official OpenAI transcription/Responses → bounded session-scoped
Spanish text frames → scrolling watch UI. No TTS. The current backend has not been
deployed and no live OpenAI call has been validated. No billable resources were
provisioned. The latest transfer-performance fixes remain physically unverified;
further physical tests were explicitly skipped by the user. Do not call the entire
physical end-to-end feature PASS from build/mock results.

Runtime setup is through Android **AI Assistant · configure / sign in**. Endpoint
and Firebase public Web API key are nonsecret configuration; password and expiring
Firebase ID token stay memory-only on the phone. The OpenAI key exists only in
backend environment/Secret Manager. An existing companion service owns BLE; no
second FGS or continuous microphone was added. Sign-in is intentionally not durable:
re-login after expiry or process restart. Missing configuration produces a watch
error with a phone-configuration instruction, not a fabricated AI answer.

Backend deployment/IAM/env/cost/privacy instructions: `apps/voice-backend/README.md`.
Wire contract: `docs/ble-protocol.md`, additive `000f`–`0012` extension. Default models
are `gpt-4o-mini-transcribe` (Spanish) and `gpt-4.1-mini` (Spanish concise answer).
Output is NFC UTF-8, bounded to 512/3072 bytes. Watch fixed-buffer assembly validates
session, type, offset, length and complete UTF-8 before display. Processing/error/
retry/Complete states and multiline vertical scrolling share the existing visual
language. Clear, Back, IDLE, disconnect and notification/pairing interruption discard
local buffers and reject late results. HTTP disconnect cannot reverse upstream work
or charges already incurred. No private audio/transcript/credential is logged.

Mutable PCM/WAV and transport buffers are cleared; SDK/string/HTTP immutable copies
are garbage-collected, not guaranteed immediately zeroized. Result text is volatile
RAM, not persisted as history. Responses use `store=False`, which does not override
all provider retention policies. No weather/tools integration or conversational
history is implied by the model response.

Automated validation includes layered mocked capture/CRC→WAV→Spanish result framing,
the actual backend route with mocked official SDK calls, auth/quota/audio bounds/
sanitized provider errors, and host UI text assembly/Unicode/scrolling checks. This
is not a physical GATT/HTTP/UI end-to-end test. Remaining manual tests: deployed
Firebase login/allowlist/revocation, real transcription/generation, complete clip
CRC/throughput after priority fix, scroll/readability, cancellation/timeouts and
media/notification coexistence. No commit or push.

Final MVP validation: firmware and simulator builds PASS; three simulator/host suites
PASS; Android build/lint and 39 unit tests PASS; eight backend mock tests and Ruff lint PASS;
COM9 flash, bounded boot validation and Pixel APK installation PASS. Boot shows no
crash or voice-task initialization failure. `git diff --check` PASS. COM9 monitor
stopped cleanly. Live cloud deployment/OpenAI/device result/transfer/regression tests
SKIPPED, not PASS. Remaining advisories: existing 32 MB detected versus 16 MB flash
configuration, BLE prevents light sleep, Wi-Fi auth-threshold notice, and backend
Starlette TestClient/httpx deprecation warning. No cloud resource was provisioned.

The following sections retain the historical Phase 1/2 investigation evidence;
their local-only/discard behavior describes those earlier builds, not this MVP.

### Feature files

- Firmware: `apps/watch/main/watch_voice.{h,cpp}`, `watch_ble.cpp`, `main.cpp`,
  `CMakeLists.txt`.
- Shared UI: `components/watch-ui/watch_ui.{h,cpp}`, `watch_voice_result.h`;
  integration tests: `tests/time_tools_ui_host.cpp`.
- Android: `apps/mobile/app/src/main/AndroidManifest.xml`,
  `src/main/java/dev/esp32/smartwatch/MainActivity.kt`,
  `ble/WatchRepository.kt`, `ble/VoiceProtocol.kt`,
  `ai/VoiceAiClient.kt`, `ai/VoiceAiSettingsActivity.kt`; tests under
  `src/test/java/dev/esp32/smartwatch/ble/VoiceProtocolTest.kt` and
  `VoiceAiPipelineTest.kt` (Android paths relative to `apps/mobile/app`).
- Backend: `apps/voice-backend/app.py`, `Dockerfile`, `requirements.txt`,
  `requirements-dev.txt`, `tests/test_app.py`, `README.md`, `.gitignore`,
  `.dockerignore`, `.gcloudignore`.
- Protocol/evidence docs: `docs/ble-protocol.md`, this document.

## Hardware review

Official Waveshare board wiki and ESP-IDF `05_Spec_Analyzer` reviewed at repository
revision `b099739ad0e33b34e5fbaae77f02bd84805d79a3`:

- https://www.waveshare.com/wiki/ESP32-S3-Touch-AMOLED-2.06
- https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-2.06/tree/b099739ad0e33b34e5fbaae77f02bd84805d79a3/examples/esp-idf/05_Spec_Analyzer

The official analyzer opens microphone/speaker codecs through the BSP, captures
16 kHz stereo int16 PCM and combines left/right samples. This firmware already
depends on `esp_codec_dev ~1.5` and the board BSP, whose microphone path uses ES7210
and output path uses ES8311. I2S mapping: MCLK16, BCLK41, WS45, DOUT40, DIN42.
No managed component changes, FFT library, external AI demo or embedded API key.

## Implemented local capture test

Apps → **AI Assistant** explicitly labels itself a Phase 1 microphone test with no
upload or AI request. Hold the large audio button to start; release stops. Dragging
out, navigation, IDLE and notification/pairing interruption cancel and discard.
The normal seven-app launcher is compacted to fit without overlapping Back.

Firmware `watch_voice.{h,cpp}` owns codec operations on a dedicated worker; UI
commands never block LVGL on audio I/O. Initialization is lazy after the first
explicit hold. ES7210 open/gain/read/close use the existing official BSP/codec API.
Stereo 16 kHz/16-bit input is averaged into mono int16 PCM. Capture is bounded by
128,000 samples/eight seconds and wall-clock deadline. PCM buffer is at most 256 KB
in PSRAM; scratch input is 1,280 bytes. Reads/close can add driver-timeout cleanup
latency; this is not a hard-real-time stop guarantee. Failed codec/gain/read/close
operations become UI errors; a new hold retries. Minimum accepted capture is 0.25s.

Recording has a visible status, duration and peak-level meter. No audio or transcript
is logged, persisted, sent to BLE, written to SD, or uploaded. Codec closes after
capture. Explicit interaction temporarily refreshes display activity; the ordinary
timeout resumes afterwards. Microphone is not opened by app navigation, boot, BLE
traffic or notifications.

**PLAY TEST** is explicit ES8311 local playback at conservative configured volume;
it never starts the microphone. This provides a way to verify recognizable recorded
speech and investigates the future playback path without implementing TTS. PCM is
zeroed through volatile writes and freed after playback, cancellation/error, new
recording or 30 seconds without use. Scratch PCM is also zeroed. Codec/I2S control
objects remain initialized after first interaction; the BSP has no public audio
deinit API. Playback cannot prove speech quality until physically heard.

Simulator uses the same screen/input routing, but has no microphone backend and
explicitly reports unavailable rather than manufacturing captured audio.

## Validation and phase gate

- Firmware build: PASS; simulator build and all three host suites: PASS.
- Shared UI tests exercise hold/release, preview enable/action, lost-touch cancel,
  IDLE, notification interruption and Back privacy routing, alongside existing
  media, 2048, timer/stopwatch, font and protocol regressions.
- Unchanged Android build/unit tests/lint: PASS. No Android files were modified;
  no replacement APK was required for this phase.
- COM9 flash: PASS. Bounded runtime/boot validation: inspected separately; monitor
  stops cleanly and releases COM9. Runtime does not exercise lazy microphone init.
- Actual microphone capture/local playback tests: **PASS (subsequent user confirmation)**.
  Initially skipped; the user later reported that the tests work correctly. This
  clears the capture gate, but is not an independent measurement of clipping,
  channel calibration, latency or signal quality.
- Long-duration power/step/RTC and BLE/media/notification coexistence under audio
  load, plus detailed cancellation/expiry edge cases, remain follow-up checks.

**Capture gate cleared by user confirmation.** Phase 2 now has an initial secure
pull receiver, described below; physical throughput/end-to-end tests are pending.
Authenticated cloud backend, OpenAI transcription/Responses and remote result UI
are not implemented or validated. No PC dependency or key handling was introduced.
The whole voice-assistant feature is not complete.

## Initial Phase 2 transport

Three additive characteristics retain the existing trusted-bond, authenticated,
encrypted GATT access gate. Firmware suffixes `000f` (metadata read), `0010` (chunk
read), `0011` (control write-with-response) share the existing service UUID tail.
All integers are little-endian. This is independent of the text transport.

- Metadata: 18 bytes: version 1, state (0 absent/1 ready/2 claimed), session u32,
  PCM length u32, IEEE CRC32 u32, sample rate u16, bit depth u8, channels u8.
- Control: 10 bytes: version 1, opcode (1 select/claim, 2 finish/cancel), session
  u32, byte offset u32. Offsets must be even and bounded; stale sessions fail.
- Chunk: version 1, session u32, byte offset u32, then even-length PCM payload.
  Total frame is capped at min(244, ATT_MTU-3). Selecting offset zero claims ready
  audio. Reads do not advance the selection, so an explicit offset can be retried
  without ambiguous cursor state. Android requires exactly its expected offset
  and session; complete clips must match CRC before acceptance.
- Maximum 256,000 bytes/eight seconds. Ready expiry is 30 seconds; claimed transfer
  has an absolute 120-second watchdog, with Android stopping at 115 seconds.
  Disconnect, user Clear/navigation or malformed response discards RAM audio.

Android negotiates MTU before service discovery; callbacks are serialized with the
existing lifecycle. Android 14 may request 517 even when 247 is requested; the
firmware cap remains 244. Below MTU 64, Android leaves audio local rather than
attempting a predictably expensive tiny-chunk transfer. It polls metadata every
two seconds only when the secure companion is connected and its GATT queue is idle.
Each select/write response and subsequent read uses that same queue; queued media,
notifications and settings can interleave. Each GATT operation has the existing
ten-second timeout/reconnect policy. No second foreground service is introduced.

This first receiver verifies then **discards** audio; no backend callback or transcript
is connected. Logs contain status/byte count only, never PCM. Unit tests exercise
bounded metadata, offset/session rejection and complete CRC-checked assembly.
Firmware/simulator/Android builds, tests and lint passed; COM9 flash and Pixel APK
replacement passed. Actual transfer, throughput, recovery and coexistence remain
pending physical tests. Request retries are protocol-safe but automatic chunk
retry is not yet implemented: GATT failure aborts/reconnects rather than loops.

## Later work, not yet implemented

Existing text frames are only 9–20 bytes; 16 kHz int16 mono is 32 KB/s and an
eight-second clip is 256 KB. BLE throughput must be measured, not assumed. Future
transfer needs its own negotiated chunk/sequence/credit/ACK/retry protocol, total
size/duration bounds, timeout/cancellation, stale-session rejection and secure
reconnection behavior. Health/media/notification GATT serialization must remain
intact. Do not send this PCM through existing text notification/media framing.

Before Phase 3, select/provision an authenticated cloud endpoint with server-side
API-key secret storage, per-user rate/size/model/output limits and execution/spend
bounds. No key belongs in firmware, APK, repository, BLE, logs or chat. Actual
OpenAI transcription and Responses cannot be validated without a configured backend
and explicit test speech. ES8311 playback is a foundation for later explicit TTS,
not a completed voice-output integration.

No branch creation, commit, push, bond reset or flash erase.

## Physical transfer investigation

Fresh baseline logs confirmed authenticated/encrypted SC/MITM trusted bonding and
MTU 256. ES7210 capture completed: 128,000 mono samples, 256,000 bytes, 8,000 ms.
Android read audio chunks successfully, but the transfer hit its approximately
115-second deadline before full assembly/CRC verification. Complete delivery and
CRC therefore **FAIL for this baseline**, not PASS. Notifications were captured
during transfer but delivered only after abort. A media snapshot was delivered;
media button actions were not independently evidenced by the logs.

Root causes addressed: the ordinary watch connection requests 100–200 ms intervals
with peripheral latency 4, inappropriate for two acknowledged GATT operations per
234-byte PCM chunk; and the continuously populated audio queue prevented the
empty-queue media/notification pump from running. A claimed transfer now requests
15–30 ms intervals with zero latency on firmware and HIGH connection priority on
Android. Android restores BALANCED priority on termination. Connection parameter
requests are advisory; successful negotiation/throughput still needs hardware proof.
The queue pump now offers pending media/notifications a turn between voice operations.

Added privacy-safe diagnostics: watch capture byte/sample/duration counts, Android
validated chunk count, elapsed monotonic transfer time and PCM bytes/second, plus
partial byte/chunk count on abort. No PCM, transcript, session value or CRC value
is logged. Duration starts at claim enqueue and ends at validated final chunk,
including control round trips and interleaved traffic, excluding recording.

Firmware build, Android build/unit tests/lint, COM9 flash and Pixel replacement
passed after fixes. Bounded watch runtime capture found no crash. The final retest
window, despite user completion confirmation, contains no watch connection/touch/
capture activity or Pixel voice events. Do not infer successful retest, CRC or a
throughput figure from that confirmation. Fixes remain **physically unverified**;
repeat with companion actually connected and a new completed recording. COM9
monitor was stopped cleanly. No OpenAI integration was added.

Evidence files under `C:\Users\david\AppData\Local\Temp\opencode\`:
`voice-validation-watch.log`, `voice-validation-baseline-pixel.log`,
`voice-validation-fixed-watch.log`, `voice-validation-fixed-pixel.log`.
# Production pipeline restored — 2026-10-13

## Language/UI and larger-read improvement

User physically confirmed real OpenAI end-to-end delivery (photo), but the
transcription/answer was Galician and caption accents were missing. Voice
transcription now uses the existing Latin-1-capable 18px font, as does the answer.
Question/answer headings, wrapping, a larger vertical scroll area and hidden
recording instructions improve the response screen. Firmware performance overlay
is disabled in defaults and the current local sdkconfig. Long foreign scripts,
emoji and all-Unicode coverage are not promised by the Latin-1 font.

Local backend source adds Spain-Spanish transcription context and explicit
Spain-Spanish response defaults with an exception for intentional other-language
requests. **Not deployed**: the existing Cloud Run language behavior is unchanged
until a separately approved rollout. No cloud/auth/IAM/secrets were modified.

Baseline successful PCM throughput was 2180–2212 bytes/s with 234-byte chunks and
one acknowledged selection write plus one read per chunk (~106 ms/chunk).
The application previously capped reads at 244 bytes despite MTU 256. Preferred
MTU is now 517 on both ends; audio frames cap at 512 bytes (502 PCM bytes, aligned
even), below MTU-1 to avoid Read Blob continuation. Results remain capped at 244.
The receiver accepts both legacy and larger reads with unchanged offsets/session
checks and whole-clip CRC. Secure serialized operation fairness remains unchanged.
At equivalent callback latency, chunk count can approximately halve; **this is
an estimate, not a physical speedup measurement**. Actual radio/scheduler/ATT
latency contributions have not been isolated. IMA ADPCM could reduce payload ~4x
but needs a versioned codec contract, decode/CRC definitions and quality testing;
lossless larger reads were chosen as the safer first improvement.

After installation, test throughput with paid AI OFF, then separately approve
any live AI question. Verify accents, question/answer layout, no FPS overlay,
long-result scrolling and cancellation/media/notification responsiveness.

Post-install user confirmed transfer followed by the paid-consent error and
physical FPS overlay removal. The bounded capture produced no ESP/Android log
lines and the subsequent filtered logcat dump was empty, so **after-throughput
and negotiated MTU are UNVERIFIED**, not a measured speedup. Baseline remains
2180–2212 bytes/s. Long Spanish response rendering/scrolling and real backend
language behavior remain pending. Android normalizes NFC accents and maps smart
quotes, dashes, ellipsis and NBSP to supported typography without stripping
Spanish characters. Other scripts/emoji may still need additional fonts.

`VOICE_TRANSFER_ONLY=false` restores the existing authenticated Cloud Run pipeline.
Paid-request consent remains memory-only and OFF after login/process restart;
installation/testing does not submit audio or invoke OpenAI. Android transfer
deadline is now 175 seconds and firmware claimed-transfer deadline 180 seconds.
At the measured slowest successful 2180 bytes/s, 256000 bytes requires about
117.4 seconds, leaving about 57 seconds of Android margin. This is a bounded
deadline, not a guarantee for poorer links; eight-second transfer remains untested.
Unclaimed READY still expires after 30 seconds; processing retains its separate
90-second watch deadline. Existing bounded result/error frames and visible
STOPPED / SEE MESSAGE UI are preserved.

Manual live test: enable the saved-watch companion, wait for secure CONNECTED,
sign in/verify UID access, then explicitly enable paid AI. Record ONE 1–2-second
Spanish question. Expect WAITING → TRANSFERRING → PROCESSING → AI RESPONSE.
Verify transcription, answer, and any specific error text; disable paid AI after
the test. This test is billable and must be initiated/approved by the user, not
automatically by build/installation. Previous diagnostic evidence below is
historical and does not establish live transcription/GPT success.

# Physical CAPTURED/READY investigation — 2026-10-13

The original incident has no retained synchronized pre-fix trace. READY means
capture completed, but the phone has not claimed it; Firebase login/paid-request
settings do not enable or prove the BLE companion connection. Do not attribute
the original incident conclusively to Firebase, Cloud Run or queue starvation.
Inspection found a real discovery starvation path: metadata polling required both
an empty queue and no pending GATT operation. Polling now queues one deduplicated
metadata read behind the current operation and ahead of queued traffic, without
overlapping GATT operations or weakening secure-link checks. A 30-second unclaimed
capture now reports an explicit companion/claim timeout instead of silently clearing.
READY/STOPPED labels explain waiting and direct the user to the actual message.

**Temporary diagnostic build:** `BuildConfig.VOICE_TRANSFER_ONLY=true` is deliberately
hardcoded in `apps/mobile/app/build.gradle.kts`. Both the BLE orchestration and
`VoiceAiClient.ask` block voice backend requests. Complete CRC-verified PCM is
discarded and a bounded result frame says `Audio transfer verified. AI paused for
testing`. The result uses the existing stopped/error wire kind, not a GPT response.
Paid-AI checkbox is disabled in this build. Removing this gate requires explicit
approval to resume live AI testing; normal sign-in's empty-body access probe remains
unchanged. No backend, IAM, Firestore or Firebase configuration was modified.

Physical evidence (Pixel `56201FDCH000N5`, COM9, ATT MTU 256):

| Clip bytes | Chunks | Transfer duration | Bytes/s | Whole-clip CRC |
| --- | --- | --- | --- | --- |
| 104320 | 446 | 47832 ms | 2180 | PASS (Android trace) |
| 68480 | 293 | 30954 ms | 2212 | PASS (synchronized ESP/Android) |
| 55680 | 238 | 25412 ms | 2191 | PASS (synchronized ESP/Android) |

For the 68480-byte clip, ESP captured 2140 ms, claimed audio about 1.6 seconds
later, acknowledged the complete recording and assembled a 46-byte kind-3 return
message. The 30-second deadline applies to **unclaimed READY**, not a claimed
transfer: firmware extends to 120 seconds at claim; Android enforces 115 seconds.
All completed diagnostic clips logged `backend request BLOCKED`; there was no
voice HTTP/OpenAI request from these clips. Historical Cloud Run request logs were
not fetched, so no claim is made about requests before the diagnostic installation.
One intervening 64000-byte recording was interrupted; Android saw control status
19 and a subsequent new capture. It is not a successful transfer/CRC result.

User confirmed the returned diagnostic text on the physical display. Short text
fits the result area and need not scroll; long real GPT response/scrolling remains
untested. Throughput remains only ~2.2 kB/s: an 8-second 256000-byte recording is
near the 115-second Android deadline and must not be called validated. Original
READY root cause is localized to discovery/claim, but its exact historical trigger
(companion inactive/disconnected, polling starvation or another pre-claim failure)
is not conclusively established without the original trace.

Local evidence lives outside Git under
`C:\Users\david\AppData\Local\Temp\opencode`: `ai-ready-pixel-after-test.log`,
`ai-ready-pixel-runtime.log`, `ai-ready-esp-serial-runtime.log`. Host UTC timestamps
align the latter two; phone clock differs by about one second. `idf.py monitor`
could not run with redirected stdin (TTY required); bounded pyserial fallback
captured runtime, closed COM9 cleanly, and logged no audio/transcripts/credentials.
Full startup capture, real unclaimed expiry interaction, full-length transfer and
authorized live AI end-to-end validation remain pending.
# Current automatic AI session policy (2026-10-15)

Supersedes historical memory-only refresh-token / paid-OFF-after-restart notes
below. One initial manual Firebase login is required; no password or bearer token
goes in a global `.env` or the APK. Refresh credentials (refresh token, UID and
public Firebase project key) are encrypted with AES-256-GCM and an Android
Keystore key, saved atomically in the phone's non-backup directory. ID tokens
remain in memory. On app startup and watch-ready reconnect, the app attempts
refresh and backend authorization verification without sending speech to OpenAI.
Only then are new recordings eligible for paid AI, enabled by default. A manual
OFF preference survives restarts, reconnects and new logins. Signing out clears
the saved session/key, revokes queued consent tickets and cancels active calls.
Old recordings cannot become eligible after automatic restoration. Transient
network failures retain saved credentials; invalid/revoked credentials or lost
Keystore access require a new manual sign-in. Transfer-only builds never restore
automatically or permit voice HTTP requests.
