# Persistent settings, battery and hardware information

- **Status:** Ready for approval — retrospective; implemented.
- **Owner:** Project maintainer; historical formal approval unavailable.
- **Last updated:** 2026-10-23.
- **Sources:** `1709a0d`, `5c6aecd`, `e24feaa`, `500bad4`, `b628621`; `watch_settings.*`, `watch_power.*`, shared UI.
- **Related ADR:** [0013](../../adr/0013-local-settings-and-power-ownership.md).

## 1. Goal and context
Expose understandable local controls and real battery/hardware state, with
firmware-owned persistence rather than storage logic inside LVGL.

## 2. Scope
Brightness, timeout, settings navigation, PMU state, Hardware Info and common
persistence. Audio and Battery Saver details have dedicated SPECs.

## 3. Requirements (EARS)
| ID | Pattern | Requirement |
| --- | --- | --- |
| SET-01 | Event-driven | When brightness is changed, the system shall clamp it to the supported 10–100% range and apply effective changes through platform callbacks. |
| SET-02 | Event-driven | When valid settings are saved, the firmware shall restore them after restart. |
| SET-03 | Unwanted behavior | If a saved value is absent/invalid or a write fails, the system shall use a defined default or report failure rather than falsely acknowledge persistence. |
| SET-04 | Ubiquitous | The UI shall present available battery/charging state and grouped hardware information without inventing measurements. |

## 4. Acceptance scenarios
- **AC-01 (SET-01/02):** Given adjustable brightness/timeout, when changed and restarted, then effective values are bounded and restored.
- **AC-02 (SET-03/04):** Given invalid storage or unavailable battery data, when Settings opens, then defaults/unavailable or save-failure states are explicit.

## 5. Constraints and decisions
Staggered square settings tiles, central minimalist information action and white
Hardware Info typography are current visual requirements. No flash erase recovery.

## 6. Implementation record
`watch_settings.*` owns NVS; PMU/startup adapters provide state; shared UI owns layout.

## 7. Test plan
FW/SIM/UI; manual bounds, save/reboot, charging/unplugging, Hardware Info scrolling.
Inject save failures through existing test callbacks rather than corrupting live flash.

## 8. Documentation impact
README, `docs/home-context.md`, `docs/battery-saver.md`, `docs/volume.md`.

## 9. Risks and approval
PMU readings are not calibrated remaining-hours estimates. Ratification pending.

## 10. Validation record
Existing UI tests cover callbacks/bounds and failed saves; hardware persistence
checks remain distinct from compilation. No new execution in this task.

## 11. History
2026-10-23: reconstructed settings/battery evolution; local audio documented separately.
