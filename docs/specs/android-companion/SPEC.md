# Android companion lifecycle, readiness and recovery

- **Status:** Ready for approval — retrospective; implemented.
- **Owner:** Project maintainer; historical formal approval unavailable.
- **Last updated:** 2026-10-23.
- **Sources:** Android `MainActivity.kt`, `WatchViewModel`, `ble/WatchRepository.kt`, `companion/`; commits `2d6cb9e`, `2c8dbd7`, `e01b6f6`.
- **Related ADR:** [0004](../../adr/0004-single-companion-and-serialized-gatt.md).

## 1. Goal and context
Provide one reliable phone bridge for controls, telemetry and optional services,
with clear permission/connection recovery instead of hidden background failures.

## 2. Scope
Android 13+, permission prompts, BLE foreground service, readiness cards, controls
and settings routes. Exclude iOS, a second voice/calendar connection and custom text scaling.

## 3. Requirements (EARS)
| ID | Pattern | Requirement |
| --- | --- | --- |
| AND-01 | Event-driven | When scanning/connecting requires permission, the app shall request the applicable Android nearby-device permissions through user-visible UI. |
| AND-02 | Ubiquitous | One repository/service path shall own the watch connection and serialize GATT operations. |
| AND-03 | Unwanted behavior | If a permission/service/link is unavailable, the app shall show its readiness state and an applicable recovery action. |
| AND-04 | Ubiquitous | The app shall respect Android's font scale without exposing the removed Lectura amplia toggle or additional multiplier. |

## 4. Acceptance scenarios
- **AC-01 (AND-01/03):** Given denied Bluetooth/notification access, when the app opens, then it explains the state and offers explicit recovery without silently granting permission.
- **AC-02 (AND-02/04):** Given a connected service and different system font scale, when navigating activity/calendar/AI, then connection ownership stays shared and text follows system scale.

## 5. Constraints and decisions
JDK 17, min SDK 33, target/compile SDK 36; background continuity depends on Android
battery policies. Authentication and Google access have separate consent paths.

## 6. Implementation record
Compose → ViewModel/state → existing WatchRepository/service. A temporary enlarged
reading option was implemented, then removed at the user's request in the delivered version.

## 7. Test plan
ANDROID; manual permission denial/regrant, foreground/background, process restart,
saved-device recovery and system font scaling. No adb auto-consent acceptance.

## 8. Documentation impact
`apps/mobile/README.md` and root setup; platform permissions remain in the manifest.

## 9. Risks and approval
OEM background limits and stale bonds; retrospective ratification pending.

## 10. Validation record
Recent Android builds/unit tests and installs were reported PASS. Removal of the
reading option was installed; final visual inspection remained manual. No tests rerun here.

## 11. History
2026-10-23: records companion foundation, readiness and removal of custom reading mode.
