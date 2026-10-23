# ADR-0001: ESP-IDF platform adapters and a shared LVGL UI

- **Status:** Proposed for retrospective ratification; implemented approach.
- **Recorded:** 2026-10-23; original approval date/reference unavailable.
- **Decision owner:** Project maintainer.
- **Related specifications:** [Platform](../specs/platform/SPEC.md), [shared UI](../specs/shared-ui/SPEC.md).
- **Sources:** `f1c2478`, `210467b`; firmware/simulator CMake and `components/watch-ui/`.
- **Supersedes / superseded by:** None formally recorded.

## Context and drivers
The ESP32 requires board drivers, FreeRTOS and ESP-IDF services, while desktop
iteration requires SDL2. Duplicating screens would cause different product behavior
and make host tests less representative.

## Options considered retrospectively
| Option | Benefit | Cost / risk |
| --- | --- | --- |
| Separate desktop mock UI | Few firmware abstractions | Divergent layout/navigation and low-value tests |
| Shared LVGL UI with platform callbacks | Same screen code, testable host logic | Explicit data/callback boundary required |
| Full hardware emulator | Potentially wider simulation | Out of project scope and not provided by SDL2 |

## Implemented decision and rationale
Firmware owns hardware/network/storage adapters under `apps/watch/main`; shared
`components/watch-ui` owns LVGL presentation, portable helpers and callbacks.
Simulator compiles the same UI and supplies mock platform values. Native BSP/managed
drivers remain dependencies rather than application edit targets.

## Consequences
- UI/navigation tests exercise production screen code without flashing.
- Platform failures cannot be certified by desktop rendering.
- Adapters must preserve ownership and avoid introducing ESP-IDF dependencies into portable logic.
- Unsupported FreeRTOS simulation is not a second supported execution mode.

## Adoption and compatibility
The extraction milestone is `210467b`. Existing CMake projects compile the shared
source. No new migration is proposed; keep both builds compiling when shared code changes.

## Validation
SIM/UI and FW commands are in the [verification catalogue](../specs/README.md#shared-verification-commands).
Historical builds passed; no builds rerun for this record. Desktop tests do not
emulate physical BLE, sensor, audio or power correctness.

## Open questions and approval history
No new architecture question is introduced. Formal historical approval is unavailable.
2026-10-23: reconstructed implemented decision; ratification pending. Alternatives
above are comparisons, not claims that every option had a historical prototype.
