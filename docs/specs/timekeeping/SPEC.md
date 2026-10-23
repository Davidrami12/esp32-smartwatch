# Clock, network synchronization and RTC

- **Status:** Ready for approval — retrospective; implemented.
- **Owner:** Project maintainer; formal historical approval unavailable.
- **Last updated:** 2026-10-23.
- **Sources:** `4006fc8`, `9611056`, `efb73ac`; `apps/watch/main/watch_rtc.cpp`, `main.cpp`.
- **Related ADR:** [0002](../../adr/0002-wall-clock-and-monotonic-time.md).

## 1. Goal and context
Provide usable time/date across network availability and distinguish calendar time
from elapsed durations. The initial clock evolved into RTC-backed timekeeping.

## 2. Scope
Include Home clock, NTP, RTC validity and Madrid date consumers. Exclude GPS time,
user-selectable world time zones and precise scientific timing.

## 3. Requirements (EARS)
| ID | Pattern | Requirement |
| --- | --- | --- |
| TIME-01 | Event-driven | When valid network time is acquired, the firmware shall update system time and synchronize the RTC through its adapter. |
| TIME-02 | Unwanted behavior | If RTC date fields or oscillator state are invalid, the firmware shall reject that value as a trusted clock. |
| TIME-03 | State-driven | While the clock is valid, calendar consumers shall derive local dates consistently with the watch's Madrid CET/CEST rules. |
| TIME-04 | Ubiquitous | Elapsed timer state shall not depend on NTP or wall-clock corrections. |

## 4. Acceptance scenarios
- **AC-01 (TIME-01/02):** Given network synchronization and later loss of Wi-Fi, when the watch restarts with a valid RTC, then time remains usable; invalid BCD/calendar values are rejected.
- **AC-02 (TIME-03/04):** Given a running timer near a DST/date transition, when the clock is corrected, then dates follow the watch zone and elapsed duration is not reset.

## 5. Constraints and decisions
RTC adapter uses PCF85063, validates BCD and round-tripped dates in 2000–2099.
Astronomy apps have their own documented supported ranges.

## 6. Implementation record
`watch_rtc.*` owns RTC conversion; `main.cpp` connects time acquisition and UI;
shared time/date helpers serve UI and tests.

## 7. Test plan
FW, UI and BOOT; inspect `tests/time_tools_host.cpp`, `tests/watch_astronomy_test.cpp`
and calendar/date tests. Manual: cold boot, unavailable Wi-Fi, midnight and corrected time.

## 8. Documentation impact
README, `docs/sun-moon.md`, `docs/calendar.md` and `docs/dashboard-connectivity.md`.

## 9. Risks and approval
RTC hardware state and future timezone-rule changes; formal ratification pending.

## 10. Validation record
Source and host checks exist; exhaustive RTC fault injection and long-duration drift
measurement are not established by this documentation. No tests rerun here.

## 11. History
2026-10-23: retrospective reconstruction, not a new time-service design.
