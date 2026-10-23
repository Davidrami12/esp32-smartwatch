# Madrid weather, forecasts and daily dashboard

- **Status:** Ready for approval — retrospective; implemented.
- **Owner:** Project maintainer; historical formal approval unavailable.
- **Last updated:** 2026-10-23.
- **Sources:** `ec624ec`, `42f1ea4`, `abb5253`; `watch_weather.*`, `docs/weather-forecast.md`.
- **Related ADR:** [0005](../../adr/0005-platform-owned-network-workers.md).

## 1. Goal and context
Evolve the Home weather summary into a useful portrait dashboard without HTTP in
LVGL callbacks, misleading hourly/daily labels or invalid cache replacement.

## 2. Scope
Madrid current conditions, hourly source data, seven daily forecasts, three-day
MAX/MIN dashboard and seven-row detail. Exclude location selection, API secrets and phone proxying.

## 3. Requirements (EARS)
| ID | Pattern | Requirement |
| --- | --- | --- |
| WEATHER-01 | Event-driven | When due and connected, the watch worker shall fetch Open-Meteo over verified HTTPS for Madrid. |
| WEATHER-02 | Unwanted behavior | If HTTP/JSON/date/numeric/array validation fails, the worker shall retain the last successful bounded RAM cache. |
| WEATHER-03 | Ubiquitous | The dashboard shall display current metrics and daily MAX/MIN values, not hourly temperatures mislabeled as daily summaries. |
| WEATHER-04 | Event-driven | When forecast detail is selected or connectivity changes, the UI shall expose the seven-day view and freshness/offline state without waking the display automatically. |

## 4. Acceptance scenarios
- **AC-01 (WEATHER-01/02):** Given valid then malformed responses, when fetching, then verified valid data updates the cache and invalid data cannot overwrite it.
- **AC-02 (WEATHER-03/04):** Given daily highs/lows and missing/zero rain, when rendering dashboard/detail, then MAX/MIN match dates, zero stays zero, unavailable stays unavailable and Back restores the dashboard.

## 5. Constraints and decisions
Today plus six days; 30-minute success refresh, five-minute retry/reconnect limit;
16,384-byte response bound, at most 200 hourly entries/nine display slots. RAIN 1h
is the forecast ending at the next full hour, not a rolling observation. Home uses date-matched highs/lows.

## 6. Implementation record
Initial current weather → forecast/cache → app-pixels-inspired native dashboard;
worker and critical-section snapshots remain platform-owned. Weather is launcher tile 1.

## 7. Test plan
FW/SIM/UI; parser and layout regression checks, offline/malformed response fixtures;
manual network loss, stale age, daily summaries and detail/Back. Respect service terms.

## 8. Documentation impact
Weather note, Open-Meteo attribution/terms and `docs/third-party/app-pixels-weather-LICENSE.txt`.

## 9. Risks and approval
External API availability/terms and invalid dates; no forecast accuracy guarantee.
Ratification pending.

## 10. Validation record
Recent physical startup logged populated current/daily caches. This is not a
certification of all forecasts or extended offline behavior. No new requests in this task.

## 11. History
2026-10-23: records current weather through final daily MAX/MIN dashboard.
