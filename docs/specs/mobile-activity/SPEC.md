# Android activity dashboard

- **Status:** Ready for approval — retrospective; implemented.
- **Owner:** Project maintainer; historical formal approval unavailable.
- **Last updated:** 2026-10-23.
- **Sources:** `066a34c`; Android `activity/`, `docs/mobile-activity-dashboard.md`.
- **Related ADR:** [0006](../../adr/0006-watch-owned-dated-activity.md).

## 1. Goal and context
Present watch activity on the phone without introducing a second step source or
claiming that cached/missing records are live measurements.

## 2. Scope
Today, watch-owned goal, seven-day chart, selection, totals/average, freshness and
refresh/Back. Exclude goal editing, Health Connect reads and extra health permissions.

## 3. Requirements (EARS)
| ID | Pattern | Requirement |
| --- | --- | --- |
| DASH-01 | Ubiquitous | The dashboard shall derive counts and goal from existing secured watch snapshots. |
| DASH-02 | State-driven | While data is cached or unavailable, the dashboard shall distinguish it from live data and show freshness/no-record state. |
| DASH-03 | Event-driven | When a day is selected, the dashboard shall show its date and full count using Madrid calendar dates. |
| DASH-04 | Ubiquitous | Seven-day summaries shall use a seven-date display window, marking missing-date zeros as display values rather than exportable observations. |

## 4. Acceptance scenarios
- **AC-01 (DASH-01/02):** Given connected then disconnected watch data, when opening/refreshing the panel, then counts retain provenance and the LIVE/CACHED state changes correctly.
- **AC-02 (DASH-03/04):** Given missing dates and a different phone timezone, when selecting bars, then watch-zone dates and no-record annotations remain correct and average equals the displayed seven-day sum divided by seven.

## 5. Constraints and decisions
Compose Canvas bars, at least 48 dp hit areas, horizontal recovery on narrow screens;
no chart library. Numeric percentage may exceed 100%; visual progress saturates.

## 6. Implementation record
Compose → WatchViewModel → ActivityDashboardRepository → existing WatchRepository.

## 7. Test plan
ANDROID, dashboard model/date tests; manual large Android font scale, narrow screen,
cached display, bar selection and refresh. No custom reading multiplier remains.

## 8. Documentation impact
Mobile dashboard note and Android navigation guidance.

## 9. Risks and approval
Misleading missing-data display or timezone relabeling; ratification pending.

## 10. Validation record
Host/unit models and Android builds exist; visual/accessibility checks remain
separate from unit evidence. No tests rerun here.

## 11. History
2026-10-23: retrospective phone visualization, not a new activity-data service.
