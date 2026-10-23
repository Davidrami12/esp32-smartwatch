# Architectural decision records

These records document architecture already present in the delivered smartwatch.
They are **retrospective**, not evidence that ADRs were approved before development.
All are **Proposed for formal ratification**: the implemented approach is recorded
as a fact, while original formal approval references are unavailable. This status
does not ask for the implementation to be replaced or redesigned.

Reconstruction date: 2026-10-23; baseline `6362892` and current working tree.
Alternatives are retrospective comparisons unless a cited historical document
explicitly records their evaluation. No measurements or approvals are invented.
For feature chronology, requirements and verification procedures, see the
[SPEC catalogue](../specs/README.md). Templates are preserved unchanged.

| ADR | Architectural boundary / decision |
| --- | --- |
| [0001](0001-platform-and-shared-ui.md) | ESP-IDF platform adapters and shared LVGL UI/simulator |
| [0002](0002-wall-clock-and-monotonic-time.md) | UTC/RTC wall clock versus monotonic duration state |
| [0003](0003-native-ble-security-and-additive-contracts.md) | Native BLE security and additive versioned contracts |
| [0004](0004-single-companion-and-serialized-gatt.md) | Single companion connection owner and serialized GATT |
| [0005](0005-platform-owned-network-workers.md) | Background Wi-Fi/weather workers, bounded caches |
| [0006](0006-watch-owned-dated-activity.md) | Watch-owned dated history and optional write-only health export |
| [0007](0007-authenticated-cloud-voice-and-quotas.md) | Backend-held AI secret, Firebase authorization, durable quotas |
| [0008](0008-lossless-pcm-and-credit-window-transport.md) | Lossless PCM, negotiated credit windows, legacy fallback |
| [0009](0009-encrypted-session-and-generation-consent.md) | Keystore session restoration and generation-bound consent |
| [0010](0010-bounded-calendar-snapshots-in-psram.md) | Bounded weekly calendar transport and PSRAM ownership |
| [0011](0011-optional-read-only-google-agenda.md) | Separate optional read-only Google authorization |
| [0012](0012-offline-schematic-astronomy.md) | Offline approximate astronomy and bounded schematic rendering |
| [0013](0013-local-settings-and-power-ownership.md) | Firmware-owned persistence, local audio and advisory power UI |
| [0014](0014-pure-game-engine-and-platform-storage.md) | Deterministic 2048 engine separated from UI/storage |
| [0015](0015-display-lifecycle-and-bounded-rendering.md) | Display-aware rendering and explicit navigation/interruption rules |

Routine icon positions, captions and color adjustments belong in the related SPEC,
not one ADR per visual edit. Superseded product behavior is explained in records;
no fictitious historical Accepted/Superseded approval chain is created.
