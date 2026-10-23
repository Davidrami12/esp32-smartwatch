# Read-only calendar transport, month and weekly agenda

- **Status:** Ready for approval — retrospective; implemented.
- **Owner:** Project maintainer; historical formal approval unavailable.
- **Last updated:** 2026-10-23.
- **Sources:** `634347f`; Android `calendar/CalendarBridge.kt`, `CalendarProtocol.kt`, `watch_calendar_transport.inc`, `watch_calendar.h`, `watch_calendar_ui.inc`.
- **Related ADR:** [0010](../../adr/0010-bounded-calendar-snapshots-in-psram.md).

## 1. Goal and context
Share selected phone calendars privately and show a month overview plus a useful
whole-week agenda. Later revisions replace the old single-day selector.

## 2. Scope
Calendar Provider selection, read-only sharing, holiday/color metadata, BLE
snapshots, PSRAM caches, month/week/detail UI and privacy clearing. Google has its own SPEC.

## 3. Requirements (EARS)
| ID | Pattern | Requirement |
| --- | --- | --- |
| CAL-01 | State-driven | While sharing is OFF or local permission/selection is missing, the system shall not publish private local event data. |
| CAL-02 | Event-driven | When requesting a month, the watch shall serialize six bounded week requests and show overflow/unavailable/freshness states explicitly. |
| CAL-03 | Event-driven | When a date is tapped, the UI shall open the Monday–Sunday agenda and initially scroll to that day; tapping month/year shall return to the current month. |
| CAL-04 | Ubiquitous | The month UI shall preserve source/event colors, subtly gray weekends/imported holidays and dim adjacent-month dates, without `/ TODAY`, `?` date suffixes or visible WEEK VIEW text. |
| CAL-05 | Event-driven | When permissions/selections/link identity invalidate data, the system shall clear pending frames/caches and reject stale responses. |

## 4. Acceptance scenarios
- **AC-01 (CAL-01/02):** Given sharing enabled and selected providers, when loading a month, then up to six bounded weeks render; OFF/revocation produces a private-data clear, not an empty-success claim.
- **AC-02 (CAL-03/04):** Given a different month and multi-day events, when selecting a date then returning/tapping the month, then all seven day groups are scrollable and the current-month shortcut works with final styling.
- **AC-03 (CAL-05):** Given in-flight fragments and a selection/reconnect change, when stale/CRC-invalid data arrives, then no partial or stale week is published.

## 5. Constraints and decisions
Six PSRAM week caches, 32 events/week, 2,048 provider-row scan; source selection up
to 32, listing up to 256 local sources including sync-OFF cached calendars. Monday-first,
Madrid times, provider-expanded recurrence, UTC all-day dates/exclusive ends. BLE UUID
`0015`: 7,000-byte total, contiguous offsets, request ID, CRC32 and 120-second assembly timeout.

## 6. Implementation record
Weekly provider bridge → versioned RGB/holiday mask → six-week month cache → final
whole-week scroll list. Internal allocation initially starved display DMA; PSRAM relocation
resolved observed stalls. Calendar is now tile 5, not the original first-tile placement.

## 7. Test plan
CalendarProtocol/Google parser unit tests, host receiver/layout/navigation cases;
FW/ANDROID/UI. Manual real colors/holidays, recurrence/DST, week scrolling, offline
refresh and permission clearing. No need to inspect personal event content automatically.

## 8. Documentation impact
`docs/calendar.md`, BLE protocol, README. Earlier first-tile/daily-selector/TODAY
instructions are superseded by current behavior. Last refresh footer retries sync.

## 9. Risks and approval
An offline watch may retain RAM cache until reconnect/reboot. Imported holiday
classification is overrideable, not guaranteed by an authoritative holiday service.
Ratification pending.

## 10. Validation record
Recent builds, host/calendar tests, flash and boot were reported PASS. Actual account
contents, physical contrast and full revocation interactions remain manual. No reruns here.

## 11. History
2026-10-23: records initial agenda, bounded transport, memory fix and final month/week revisions.
