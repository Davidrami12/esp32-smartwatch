# Battery Saver advisory page

- **Status:** Ready for approval — retrospective; implemented.
- **Owner:** Project maintainer; historical formal approval unavailable.
- **Last updated:** 2026-10-23.
- **Sources:** delivered shared Settings UI; `docs/battery-saver.md`.
- **Related ADR:** [0013](../../adr/0013-local-settings-and-power-ownership.md).

## 1. Goal and context
Help users choose lower-consumption settings without hiding automatic radio or
brightness changes behind an unmeasured low-power-mode label.

## 2. Scope
Suggestions using real battery/settings state and links to existing controls.
Exclude power profiling, per-app drain, remaining-hour estimates and autonomous settings edits.

## 3. Requirements (EARS)
| ID | Pattern | Requirement |
| --- | --- | --- |
| SAVE-01 | Ubiquitous | Battery Saver shall show available battery/charging state and preserve unknown readings as unknown. |
| SAVE-02 | Event-driven | When a suggestion is tapped, the UI shall open the existing relevant control and return to suggestions on Back. |
| SAVE-03 | Ubiquitous | The page shall explain Wi-Fi clock/weather and Bluetooth companion/light-sleep tradeoffs without automatically changing settings. |
| SAVE-04 | Ubiquitous | The page shall not claim measured battery savings or remaining runtime from settings alone. |

## 4. Acceptance scenarios
- **AC-01 (SAVE-01/02):** Given low/unknown battery and high brightness, when opening suggestions and selecting brightness, then state is honest and navigation returns correctly.
- **AC-02 (SAVE-03/04):** Given Wi-Fi/Bluetooth enabled, when viewing the page, then controls remain unchanged until an explicit edit and no invented savings are shown.

## 5. Constraints and decisions
Low battery threshold 20%; brightness suggestion at values above 30%; no new
polling timer or inactive-screen redraw policy. Existing callbacks own changes.

## 6. Implementation record
Sixth Settings tile and scrollable advice consume existing state and control routes.

## 7. Test plan
UI/FW/SIM; manual suggestion routes, unavailable battery and no unintended settings changes.

## 8. Documentation impact
Battery Saver note must describe advice, not automatic power management.

## 9. Risks and approval
Misleading power-saving claims and radio interruption; ratification pending.

## 10. Validation record
No power-consumption study or remaining-hours validation exists in this record.
No tests or setting changes performed during documentation.

## 11. History
2026-10-23: retrospective advisory feature, grouped with existing settings ownership.
