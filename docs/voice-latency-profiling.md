# Voice latency profiling — 2026-10-13

Branch: `feature/mobile-android-foundation`. No assistant-initiated speech requests,
cloud builds/deployments, model changes, quota changes, billing changes or IAM/
secret modifications. User stopped physical tests after instrumentation installed.
This is a **profiling phase**, not a completed transport optimization or proven
speedup. Keep production PCM, secure read/select protocol and response framing.

## Physical baseline BEFORE timing instrumentation

Current larger-read firmware/APK measured in a synchronized five-minute capture:

| Stage / metric | Measurement | Evidence / limitation |
| --- | --- | --- |
| Recording length | 2580 ms / 82560 PCM bytes | Watch capture log |
| Release → finalization | Unmeasured | Original build had no release timing |
| Capture complete → claim | 1492 ms | Watch monotonic log timestamps |
| Android metadata receipt → CRC verified | 30087 ms | 165 chunks, CRC PASS |
| BLE PCM throughput | 2744 bytes/s | One short run; MTU 517 confirmed |
| Average PCM/chunk | 500.36 bytes | Last partial chunk included; full chunks up to 502 |
| Average transfer time/chunk | 182.35 ms | Includes select-write/read/queue overhead; not pure radio latency |
| CRC verified → processing acknowledged | ~50 ms | Android timestamps |
| Watch processing acknowledgement → final answer assembly | 8349 ms | Includes HTTP/backend/OpenAI/parsing/result delivery together |
| Capture complete → final answer assembly | 39662 ms | Not release-to-display or panel scanout |
| Backend auth, quota, transcription, GPT individually | Unmeasured | Deployed backend has no new instrumentation |
| UI rendering / physical display flush | Unmeasured | Original trace ends at frame assembly |

Earlier 244-byte-frame physical runs measured 2180–2212 bytes/s. The newer
517-MTU run is 2744 bytes/s, but clip length/conditions differ; do NOT describe
this as a controlled before/after speedup. Long audio and repeated trials remain
pending. No post-instrumentation recordings were completed after user stopped tests.

Pre-change evidence: `C:\Users\david\AppData\Local\Temp\opencode\`
`voice-latency-baseline-android.log` and `voice-latency-baseline-esp.log`.
The user-initiated baseline clip returned kind-1 transcription and kind-2 answer
despite instructions to keep paid AI off: treat it as an apparent real live call,
not a mock/consent-error measurement. No assistant tool initiated that recording
or HTTP call, and no additional paid run was requested after discovering it.

## Instrumentation added (not a wire-format change)

The watch's random 32-bit clip/session ID is rendered as eight lowercase hex
characters. It is not a UID, account identifier, token or audio hash. Android
passes it as `X-Voice-Trace`; local backend validates this header strictly and
generates a random fallback for invalid/missing values. No audio/transcripts/
answers/passwords/auth headers/provider errors are logged.

- Watch: release-to-finalized time, claim timestamp, result assembly timestamp,
  release-to-result duration, UI text-copy revision and label-update CPU duration.
  A revision joins UI copy and label update within one boot. **Label updates do
  not measure LVGL rasterization, DMA flush or physical panel scanout.**
- Android GATT: session, negotiated MTU, frame length, chunk count, transfer time,
  aggregate DATA-read callback time and CONTROL-select-write callback time. These
  include Handler scheduling; residual transfer time includes queue/fairness/CRC
  processing. No overlapping GATT operations are introduced.
- Android HTTP: executor wait, authentication/token/WAV/request preparation,
  connect+upload, header wait, body download, parsing, combined HTTP round trip.
  Upload includes connection/TLS; header wait includes backend work. Do not
  subtract these overlapping intervals as if they were independent stages.
- Android return BLE: result queue timestamp/frame count and last successful
  acknowledged frame time. Queue delay is intentionally included.
- Local backend: provider acquisition (including cold lazy initialization),
  Firebase verification, streamed body receive/validation, Firestore reservation,
  transcription, GPT generation, response-ready total, ASGI send duration and
  request total. Context-local traces isolate concurrent requests. ASGI send
  measures sending to the server transport, **not** delivery across the network.
  Cold Cloud Run scheduling before ASGI entry is outside these measurements.

`latency.py` is now a runtime dependency copied by the Dockerfile. Existing
Spanish ZIP is a historical, pre-instrumentation artifact; do not combine its
file manifest with new source. Any future package must include `latency.py` and
be revalidated. No backend deployment or upload was executed in this task.

## Root causes supported by evidence

1. Audio transfer dominates the observed post-capture interval (~30 seconds versus
   ~8 seconds for the combined processing/result leg).
2. MTU 517 is actually negotiated; the read-frame cap is no longer the old 244-byte
   cap. Raw PCM for only 2.58 seconds still requires 165 acknowledged read/select
   cycles. Larger packets alone do not eliminate the round trips.
3. Two-second metadata polling contributes measurable discovery delay (1.492s
   in this sample). It is not the dominant 30-second transfer bottleneck.
4. Exact read-versus-write, radio/connection interval, Handler/queue and backend
   contributions are **not yet isolated**; new instrumentation is intended to do so.

## Ranked next steps (require further evidence/approval)

1. Resume unpaid controlled short/long trials with instrumentation. Verify consent
   OFF/process restart first. Correlate aggregate read/write times and fairness
   residual, and observe connection parameters rather than assuming requests worked.
2. If read/select round trips dominate, negotiate a new notification-based audio
   capability with bounded windows, credits/backpressure, offsets, retry/resume,
   whole-clip CRC and existing secure-link checks. Preserve old pull fallback and
   serialize writes alongside media/notifications. Do not simply parallelize GATT
   operations or auto-advance retryable reads without a versioned contract.
3. Evaluate IMA ADPCM offline: roughly 4:1 payload reduction, but lossy audio;
   requires codec capability/version, predictor/index state, decoded size bounds,
   defined encoded/decoded CRC, malformed-stream tests, and speech-quality
   validation. Decode to the unchanged 16kHz mono PCM WAV on Android. Not implemented
   without quality/evidence; no silent PCM sample-rate or bit-depth reduction.
4. After separately approved backend rollout, measure auth/quota/OpenAI separately
   with at most one manually approved live test. Existing OpenAI client is reused;
   transcription and GPT cannot be parallelized because GPT needs the transcript.
5. Consider faster ready notification (negotiated) rather than higher idle polling
   frequency; measure battery/queue impact. Only tune Cloud Run cold starts/min
   instances with explicit cost approval; no infrastructure change here.

Silence trimming is deferred: no VAD/leading-consonant safety evidence. HTTP client
replacement/coroutine rewrites are deferred: the existing executor is off the BLE
Handler, and connection-pool gains are unmeasured. Backend synchronous lazy
initialization is visible via provider timing; do not change initialization/auth
semantics speculatively. Streaming partial answers is a future protocol change,
not achieved by displaying PROCESSING sooner.
