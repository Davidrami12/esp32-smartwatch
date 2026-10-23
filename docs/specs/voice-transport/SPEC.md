# Lossless voice BLE transfer and latency diagnostics

- **Status:** Ready for approval — retrospective; implemented.
- **Owner:** Project maintainer; historical formal approval unavailable.
- **Last updated:** 2026-10-23.
- **Sources:** `a3f6af4`, `7740474`; BLE protocol, voice window tests, Android `VoiceProtocol` and `WatchRepository`.
- **Related ADR:** [0008](../../adr/0008-lossless-pcm-and-credit-window-transport.md).

## 1. Goal and context
Reduce serialized GATT overhead while preserving bit-identical speech, bounded
recovery, legacy compatibility and responsiveness of media/notifications.

## 2. Scope
Pull mode, negotiated notifications, result framing, cancellation, CRC and privacy-safe
timing. Exclude compression changes, parallel uncorrelated GATT and claimed benchmark gains.

## 3. Requirements (EARS)
| ID | Pattern | Requirement |
| --- | --- | --- |
| VOX-01 | Event-driven | When notification capability is negotiated/enabled, the transport shall send bounded lossless PCM windows of up to 16 credits at at most one packet per 10 ms callout. |
| VOX-02 | Unwanted behavior | If a peer/version is unsupported or bounded no-progress recovery fails, the transport shall retain/resume compatible legacy pull without inventing PCM. |
| VOX-03 | Event-driven | When a transfer completes, the receiver shall verify session, contiguous prefix, length and full CRC before admitting audio for processing. |
| VOX-04 | Event-driven | When a session expires/cancels/disconnects, the system shall reject late frames/results and release private data while preserving queue fairness. |

## 4. Acceptance scenarios
- **AC-01 (VOX-01/03):** Given a synthetic clip and supported peer, when transferring with windows, then bytes/CRC equal the original and replayed grants cannot create extra credit.
- **AC-02 (VOX-02/04):** Given drop/reorder/stale epochs or cancellation, when bounded retries/fallback occur alongside media updates, then no corrupt clip is submitted, no stale result is displayed and other traffic retains turns.

## 5. Constraints and decisions
Versioned capabilities/CCCD, epochs, offsets, sequences and bounded queues. Gradle
defaults notifications OFF; build with `-PvoiceNotify=true`. Earlier four-credit
design notes are historical. Detailed layouts/timeouts remain in BLE protocol/source.

## 6. Implementation record
Legacy larger reads → negotiated notification windows → final 16-credit pacing;
existing secured control/result channels remain compatible. Timing diagnostics omit content.

## 7. Test plan
ANDROID, `tests/watch_voice_window_test.cpp`, voice protocol/queue tests, TRANSFER
physical CRC/fallback/cancel runs. Compare identical clips and conditions before claiming speedup.

## 8. Documentation impact
`docs/ble-protocol.md`, latency and window-optimization notes; clearly label historical measurements.

## 9. Risks and approval
Android radio scheduling and MTU negotiation differ; requested connection settings
are advisory. CRC is not authentication. Ratification pending.

## 10. Validation record
Historical physical window/legacy checks were reported; baseline 82,560 bytes took
30.087 s in legacy transfer. No controlled final A/B or battery study is established.
No transfers/tests rerun for these records.

## 11. History
2026-10-23: reconstructs baseline, instrumentation and final compatible window contract.
