# ADR-0003: Native bonded BLE security and additive versioned contracts

- **Status:** Proposed for retrospective ratification; implemented approach.
- **Recorded:** 2026-10-23; historical formal approval unavailable.
- **Decision owner:** Project maintainer.
- **Related specification:** [Secure BLE](../specs/secure-ble/SPEC.md).
- **Sources:** `watch_ble_security_policy.h`, `watch_ble_security.*`, `docs/ble-protocol.md`.
- **Related ADRs:** [0004](0004-single-companion-and-serialized-gatt.md), [0008](0008-lossless-pcm-and-credit-window-transport.md), [0010](0010-bounded-calendar-snapshots-in-psram.md).
- **Supersedes / superseded by:** None formally recorded; custom admission gates were removed in product evolution.

## Context and drivers
Private notifications, audio and calendar data require authenticated/encrypted
access. New services must coexist with older firmware and companion versions.

## Options considered retrospectively
| Option | Benefit | Cost / risk |
| --- | --- | --- |
| Unprotected telemetry and app-only checks | Easy initial integration | Does not enforce access at the device boundary |
| Native strong bonding plus protected characteristics | Stack-backed security and durable pairing | Bond recovery and failure handling required |
| Native security plus custom phone-admission gates | Extra local policy | Previous extra gates added failure/recovery complexity |

## Implemented decision and rationale
Require encryption, authentication, bonding, Secure Connections and a 16-byte key
for protected values. Retain native pairing/forget behavior without custom phone
admission filters. Introduce optional characteristics and explicit schema/version
negotiation rather than silently redefining old values.

## Consequences
- Optional features can be absent without disabling base telemetry/controls.
- CRC protects transfer integrity but is not a replacement for link authentication.
- Revocation/forget must invalidate pending private application data as well as bonds.
- Corrupt bond storage and weak security must fail closed rather than bypass checks.

## Adoption and compatibility
Exact UUIDs, byte layouts and legacy behavior remain in BLE protocol documentation.
Native-policy helper supports host tests; physical stacks still require pairing tests.

## Validation
Security policy/protocol tests and optional untrusted-client hardware probe; real
pair/reconnect/forget checks. Historical secure connections observed; not a penetration
test or exhaustive adversarial assessment. Nothing rerun for this record.

## Open questions and approval history
No new security policy proposed. 2026-10-23: reconstructed final policy; formal
ratification pending rather than inventing historical accepted/superseded ADR dates.
