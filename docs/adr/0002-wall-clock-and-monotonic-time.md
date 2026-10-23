# ADR-0002: Separate wall-clock dates from monotonic durations

- **Status:** Proposed for retrospective ratification; implemented approach.
- **Recorded:** 2026-10-23; historical approval unavailable.
- **Decision owner:** Project maintainer.
- **Related specifications:** [Timekeeping](../specs/timekeeping/SPEC.md), [time tools](../specs/time-tools/SPEC.md), [Pomodoro](../specs/pomodoro/SPEC.md).
- **Sources:** `efb73ac`, `c3bdcb3`, `47f2ec3`; RTC and shared time helpers.
- **Supersedes / superseded by:** None formally recorded.

## Context and drivers
Network time can jump and DST changes calendar intervals. Timer/Pomodoro elapsed
time must remain stable, while calendar, activity and astronomy need actual dates.

## Options considered retrospectively
| Option | Benefit | Cost / risk |
| --- | --- | --- |
| Wall-clock subtraction for everything | One clock | NTP corrections distort durations |
| Monotonic elapsed state plus UTC/date helpers | Stable timers and explicit dates | Two time concepts must be maintained |
| Persist every duration tick | Survives restart | Unnecessary flash writes and changed product scope |

## Implemented decision and rationale
Use system UTC/RTC/NTP for real time and Madrid local-date conversion. Use
monotonic unsigned tick deltas accumulated into wider elapsed values for duration
tools. Keep tool state RAM-only. RTC validates actual Gregorian fields rather than
accepting arbitrary BCD as usable time.

## Consequences
- NTP adjustment does not restart or shorten an active duration.
- Display IDLE does not stop phases while platform ticks advance.
- Reboot resets RAM timers; true deep sleep would require additional restoration design.
- Date/zone rules and all-day conventions must stay explicit across watch and phone.

## Adoption and compatibility
Original Timer/Stopwatch timing logic is reused by later tools. Pomodoro replaces
the launch entry, not the need for monotonic arithmetic. No persistent timer migration.

## Validation
`tests/time_tools_host.cpp`, astronomy/date/UI cases; manual NTP correction, midnight,
RTC invalidity and offscreen expiry. Existing coverage is not new execution evidence.

## Open questions and approval history
Future timezone law changes or deep sleep would require review. No new change is
authorized here. 2026-10-23: retrospective record, formal ratification pending.
