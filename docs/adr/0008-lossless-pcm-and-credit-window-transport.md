# ADR-0008: Lossless PCM, negotiated notification credits and legacy fallback

- **Status:** Proposed for retrospective ratification; implemented approach.
- **Recorded:** 2026-10-23; historical formal approval unavailable.
- **Decision owner:** Project maintainer.
- **Related specifications:** [Capture](../specs/voice-capture/SPEC.md), [voice transport](../specs/voice-transport/SPEC.md).
- **Sources:** BLE protocol, `docs/voice-ble-window-optimization.md`, voice window/queue tests.
- **Supersedes / superseded by:** None formally recorded; four-credit prototype evolved to 16 credits.

## Context and drivers
Legacy serialized select/read operations dominated a historical short-clip transfer.
Optimization must not trade speech quality for an unmeasured compression benefit
or break notifications/media/security.

## Options considered
The original optimization note explicitly evaluates lossless notification windows,
IMA ADPCM, parallel GATT, faster polling and silence trimming. Final pacing/window
size was refined after that note; its four-credit figures are historical.

| Option | Benefit | Cost / risk |
| --- | --- | --- |
| Keep pull only | Existing compatibility | Per-chunk acknowledged round trips |
| Lossless notification windows | Amortized control overhead, identical PCM | Credit/session/recovery state machine |
| IMA ADPCM or silence trimming | Smaller payload | Quality/context risk not validated; not implemented |

## Implemented decision and rationale
Retain PCM16 mono 16 kHz and existing backend WAV. Negotiate optional windows up to
16 credits, send one packet per 10 ms callout, validate epoch/session/offset/sequence
and full CRC, and bound no-progress retries. Keep compatible legacy pull fallback.
Generation/cancel/deadline state invalidates late traffic and queue scheduling preserves other features.

## Consequences
- Optimization leaves decoded speech bit-identical; no decoder/cloud migration.
- Fewer application round trips do not prove a measured speedup or battery saving.
- Buffers, callback copying and credit replay handling become security/reliability constraints.
- Development builds need `-PvoiceNotify=true`; source Gradle defaults false.

## Adoption and compatibility
Use protected capabilities/CCCD and existing secured control/result characteristics.
Older peers keep pull behavior; transfer-only APKs block voice HTTP for unpaid tests.

## Validation
Window/legacy CRC, stale grant, drop/reorder/cancel and fairness tests; historical
physical checks reported. Controlled identical-audio A/B and long-term battery data
remain absent. No fresh throughput evidence is manufactured here.

## Open questions and approval history
Compression would require a separate approved quality/contract change.
2026-10-23: retrospective final transport record; ratification pending.
