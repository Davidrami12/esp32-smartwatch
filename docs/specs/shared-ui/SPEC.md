# Shared LVGL interface, navigation and desktop simulator

- **Status:** Ready for approval — retrospective; implemented.
- **Owner:** Project maintainer; historical formal approval unavailable.
- **Last updated:** 2026-10-23.
- **Sources:** `210467b`, `c3bdcb3`, `2b4519e`, `634347f`; `components/watch-ui/`, simulator CMake and host tests.
- **Related ADRs:** [0001](../../adr/0001-platform-and-shared-ui.md), [0015](../../adr/0015-display-lifecycle-and-bounded-rendering.md).

## 1. Goal and context
Keep a single watch UI implementation testable on desktop, with original
minimalist assets, legible Spanish content and stable navigation.

## 2. Scope
Home, Apps/Settings separation, touch targets, transitions, fonts, mock desktop
adapters and final launcher order. Exclude emulation of real BLE/audio/power.

## 3. Requirements (EARS)
| ID | Pattern | Requirement |
| --- | --- | --- |
| UI-01 | Ubiquitous | Firmware and simulator shall compile the same shared watch UI source. |
| UI-02 | Event-driven | When Back or the applicable right-swipe is used, navigation shall return through the owning app/menu route, except explicitly consumed game gestures. |
| UI-03 | Ubiquitous | The launcher shall place Weather at 1, Media at 4 and Calendar at 5, read left-to-right/top-to-bottom. |
| UI-04 | Ubiquitous | The UI shall preserve the dotted Home clock, English controls and glyph support for Spanish response text. |

## 4. Acceptance scenarios
- **AC-01 (UI-01/02):** Given the host build, when navigating Home/Apps/Settings/details, then real shared screens and matching Back routes are exercised.
- **AC-02 (UI-03/04):** Given the final launcher and Spanish content, when rendered, then tile positions match the catalogue and accented text remains readable without changing the Home clock typography.

## 5. Constraints and decisions
410 × 502 layout; common button helper enforces 56 px minimum dimensions, with
documented compact calendar exceptions. Keep font/third-party notices. Desktop
FreeRTOS mode is unsupported; use SDL2 and mock platform values.

## 6. Implementation record
Shared `watch_ui.cpp`, design/fonts helpers, SDL2 adapter and `time_tools_ui_host.cpp`.
Removed broad-reading mobile toggle is documented in the Android companion SPEC.

## 7. Test plan
SIM/UI and FW. Inspect launcher positions, glyphs, clipping, scrolling and touch
targets physically; simulator success cannot establish touch-controller behavior.

## 8. Documentation impact
README final app grid and `apps/watch-simulator/README.md`; earlier UI notes may
describe superseded monochrome/icon placements, not current launcher requirements.

## 9. Risks and approval
Large text, small screen and interruption routing; retrospective ratification pending.

## 10. Validation record
Final launcher host regression and build/boot checks were reported PASS in the
project conversation. No current binary was produced by this documentation task.

## 11. History
2026-10-23: grouped shared UI extraction, visual system and final layout revisions.
