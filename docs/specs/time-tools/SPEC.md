# Original Timer and Stopwatch

- **Status:** Ready for approval — retrospective; implemented historically, launcher entries superseded.
- **Owner:** Project maintainer; historical formal approval unavailable.
- **Last updated:** 2026-10-23.
- **Sources:** `c3bdcb3`, `47f2ec3`, `b59db73`; `watch_time_tools.h`, `docs/apps-settings-time-tools.md`.
- **Related ADR:** [0002](../../adr/0002-wall-clock-and-monotonic-time.md).

## 1. Goal and context
Record the original standalone duration tools, including behavior retained in
helpers/tests after Pomodoro and Motion Lab replaced their launcher shortcuts.

## 2. Scope
Historical adjustable Timer, Stopwatch and monotonic arithmetic. Do not imply
these original entries are present in the final nine-app launcher.

## 3. Requirements (EARS)
| ID | Pattern | Historical requirement |
| --- | --- | --- |
| TOOL-01 | Event-driven | When the original Timer is adjusted before start/reset, it shall accept 1–120 minutes in one-minute increments, default 5. |
| TOOL-02 | Event-driven | When Timer/Stopwatch is paused or resumed, elapsed time shall retain completed intervals without wall-clock dependence. |
| TOOL-03 | State-driven | While a tool runs offscreen or in display IDLE, elapsed duration shall continue without unnecessary redraws. |
| TOOL-04 | Event-driven | When the timer reaches zero, it shall stop with a visual completion state and no automatic audio/vibration/wake. |

## 4. Acceptance scenarios
- **AC-01 (TOOL-01/02):** Given the historical tool route or host fixture, when bounds/pause/resume/reset are exercised, then documented duration and accumulation behavior hold.
- **AC-02 (TOOL-03/04):** Given offscreen operation and tick wrap, when time advances beyond expiry, then zero/completion is retained without advancing into another phase.

## 5. Constraints and decisions
RAM-only state resets at reboot; uint64 accumulated milliseconds consume unsigned
tick deltas. Continuous platform ticking is assumed; deep-sleep restoration is excluded.

## 6. Implementation record
Pure time helpers and shared screens were added first. Pomodoro replaced Timer
(`47f2ec3`); Motion Lab replaced Stopwatch (`b59db73`). No removal of unrelated tests is implied.

## 7. Test plan
`tests/time_tools_host.cpp` and UI host cases; SIM/UI. Final launcher acceptance
uses Pomodoro/Motion Lab specifications rather than historical entry names.

## 8. Documentation impact
Historical tool notes remain useful but must not override the final README launcher.

## 9. Risks and approval
Historical versus current reachability must remain explicit; ratification pending.

## 10. Validation record
Existing arithmetic/UI tests are available. No fresh hardware certification or
claim of current launcher access is made by this retrospective document.

## 11. History
2026-10-23: records original feature and subsequent product-scope replacements.
