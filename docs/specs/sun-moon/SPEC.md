# Sun & Moon offline estimates

- **Status:** Ready for approval — retrospective; implemented.
- **Owner:** Project maintainer; historical formal approval unavailable.
- **Last updated:** 2026-10-23.
- **Sources:** `9d9b549`; shared astronomy helpers, `docs/sun-moon.md`.
- **Related ADR:** [0012](../../adr/0012-offline-schematic-astronomy.md).

## 1. Goal and context
Provide educational solar/lunar information without network, location permission
or paid astronomy services.

## 2. Scope
Madrid Sun/Moon tabs, today plus six preview days, solar events and approximate
moon phase. Exclude precise ephemerides, configurable location and navigation use.

## 3. Requirements (EARS)
| ID | Pattern | Requirement |
| --- | --- | --- |
| SUN-01 | Ubiquitous | Astronomy shall calculate locally for Madrid (40.4168 N, 3.7038 W) using existing UTC time and Madrid local-time rules. |
| SUN-02 | Event-driven | When the user selects a tab/date, the UI shall show the corresponding solar estimates or northern-hemisphere lunar phase information for today through six days ahead. |
| SUN-03 | Unwanted behavior | If clock/date is invalid, the app shall show unavailable information instead of plausible fabricated events. |
| SUN-04 | State-driven | While visible and awake, updates shall follow entry/interaction/minute changes rather than continuous astronomy animation. |

## 4. Acceptance scenarios
- **AC-01 (SUN-01/02):** Given valid UTC and a phone/PC in another timezone, when previewing Sun/Moon dates, then Madrid dates and plausible seasonal events remain consistent.
- **AC-02 (SUN-03/04):** Given invalid clock or inactive display, when app state updates, then invalid values remain unavailable and inactive rendering is suppressed.

## 5. Constraints and decisions
Supported clock range 2000–2099; NOAA fractional-year solar equations and mean
synodic lunar estimates. Lunar events may differ by about a day. Dome is schematic.
Keep app-pixels adaptation MIT notice at `components/watch-ui/licenses/sun-moon-MIT.txt`.

## 6. Implementation record
Portable calculation helpers feed shared LVGL screens; no new worker/network dependency.

## 7. Test plan
`tests/watch_astronomy_test.cpp`, UI astronomy cases; SIM/UI/FW and manual preview/Back.
Seasonal plausibility tests do not certify a two-minute event-error bound.

## 8. Documentation impact
Sun & Moon feature note and third-party notice; no precision claims beyond estimates.

## 9. Risks and approval
Timezone law changes and approximate lunar timing; ratification pending.

## 10. Validation record
Calculation/UI regression targets exist; physical scientific accuracy not certified.
No tests or external ephemeris queries run in this task.

## 11. History
2026-10-23: retrospective local-astronomy feature specification.
