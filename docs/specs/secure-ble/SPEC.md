# Secure BLE telemetry and control contracts

- **Status:** Ready for approval — retrospective; implemented.
- **Owner:** Project maintainer; historical formal approval unavailable.
- **Last updated:** 2026-10-23.
- **Sources:** `7447c20`, `2d6cb9e`, `7740474`; `watch_ble.*`, `watch_ble_security.*`, `docs/ble-protocol.md`.
- **Related ADR:** [0003](../../adr/0003-native-ble-security-and-additive-contracts.md).

## 1. Goal and context
Expose watch state and controls to Android without admitting unprotected
application traffic or breaking peers lacking newer optional features.

## 2. Scope
Discovery, pairing, bonding, protected telemetry/controls, disconnect/forget and
additive optional characteristics. Exclude DFU and secrets sent over app protocols.

## 3. Requirements (EARS)
| ID | Pattern | Requirement |
| --- | --- | --- |
| BLE-01 | State-driven | While a link lacks required native security, protected GATT values shall reject application access. |
| BLE-02 | Ubiquitous | Strong-link policy shall require encryption, authentication, bonding, Secure Connections and a 16-byte key. |
| BLE-03 | Event-driven | When a peer lacks an optional characteristic/version, the companion shall retain compatible base functionality. |
| BLE-04 | Event-driven | When forgetting/disconnecting invalidates private transfers, the system shall discard pending session data and reject stale results. |

## 4. Acceptance scenarios
- **AC-01 (BLE-01/02):** Given an unpaired/weak link, when protected reads/writes are attempted, then they are refused; a properly bonded encrypted connection can proceed.
- **AC-02 (BLE-03/04):** Given an older peer or disconnected transfer, when optional support is absent or stale fragments arrive, then base controls work and stale data is not published.

## 5. Constraints and decisions
Native security replaces previous custom phone-admission gates. Pure policy helper
`watch_ble_security_policy.h` and exact byte layouts in `docs/ble-protocol.md` remain authoritative.

## 6. Implementation record
NimBLE service registration/security callbacks and Android GATT discovery evolved
through controls, notifications, media, activity, voice and calendar extensions.

## 7. Test plan
FW/ANDROID/UI; `tests/watch_ble_untrusted.py` is an optional hardware security probe,
not a self-contained certificate. Manual pairing, reboot reconnect and forget on both ends.

## 8. Documentation impact
BLE protocol and setup sections; no custom admission gate should be reintroduced by documentation.

## 9. Risks and approval
Bond storage failures, changed credentials and stale peers; ratification pending.

## 10. Validation record
Secure physical reconnects occurred; a prior security status 7 was followed by
successful recovery. Exhaustive adversarial testing is not claimed. Not rerun here.

## 11. History
2026-10-23: retrospective native-security contract and extension compatibility.
