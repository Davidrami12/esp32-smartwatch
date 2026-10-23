# Display timeout, touch gating and wake shortcuts

- **Status:** Ready for approval — retrospective; implemented.
- **Owner:** Project maintainer; historical formal approval unavailable.
- **Last updated:** 2026-10-23.
- **Sources:** `e94ebbd`, `3e1a9e3`, `4ea7fbe`; `main.cpp`, `watch_power.cpp`, `components/watch-ui/watch_touch_gate.h`.
- **Related ADR:** [0015](../../adr/0015-display-lifecycle-and-bounded-rendering.md).

## 1. Goal and context
Prevent accidental navigation when the screen is off while retaining deliberate
wake and assistant access. Display inactivity is not deep sleep.

## 2. Scope
Timeout, brightness-based IDLE, double-tap and offscreen assistant hold. Exclude
automatic recording, automatic Home flashes during the shortcut and deep-sleep claims.

## 3. Requirements (EARS)
| ID | Pattern | Requirement |
| --- | --- | --- |
| WAKE-01 | Event-driven | When the configured inactivity timeout expires, the firmware shall deactivate the display through the power adapter. |
| WAKE-02 | State-driven | While the display is off, ordinary touches shall not activate underlying UI navigation. |
| WAKE-03 | Event-driven | When an offscreen double tap is recognized, the firmware shall wake the display. |
| WAKE-04 | Event-driven | When an offscreen hold reaches one second, the firmware shall open Mochi without starting a recording or first showing Home. |

## 4. Acceptance scenarios
- **AC-01 (WAKE-01/02):** Given a visible button before timeout, when it is touched after the screen goes off, then its action is not executed.
- **AC-02 (WAKE-03/04):** Given an offscreen watch, when double-tapped or held for one second, then the corresponding wake/assistant action occurs without audio capture.

## 5. Constraints and decisions
Current power adapter sets display brightness to zero; panel-controller sleep is
not exposed by the BSP. BLE/light-sleep limitations remain documented.

## 6. Implementation record
Firmware touch gate precedes LVGL dispatch; shared UI receives display-active state.
Subsequent revisions corrected accidental button presses and assistant wake routing.

## 7. Test plan
FW/SIM/UI/BOOT plus physical single touch, double tap, hold/release, timeout and
notification interruption. Host tests cannot prove the electrical touch behavior.

## 8. Documentation impact
README and `docs/apps-settings-time-tools.md`; do not describe IDLE as measured deep sleep.

## 9. Risks and approval
Gesture timing and press-release propagation require hardware checks; ratification pending.

## 10. Validation record
Regression tests and historical physical work exist; complete gesture acceptance
for the final hardware configuration is not certified here. No tests rerun.

## 11. History
2026-10-23: grouped initial timeout and later safe-wake revisions.
