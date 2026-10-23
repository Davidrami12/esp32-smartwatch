# ADR-0005: Platform-owned Wi-Fi/weather workers with bounded caches

- **Status:** Proposed for retrospective ratification; implemented approach.
- **Recorded:** 2026-10-23; historical formal approval unavailable.
- **Decision owner:** Project maintainer.
- **Related specifications:** [Wi-Fi](../specs/wifi/SPEC.md), [Weather](../specs/weather/SPEC.md).
- **Sources:** `watch_wifi.*`, `watch_weather.*`, dashboard/weather feature notes.
- **Supersedes / superseded by:** None formally recorded.

## Context and drivers
Slow association, HTTP and JSON parsing must not block touch rendering. Weather
should remain useful offline without replacing good data with malformed responses.

## Options considered retrospectively
| Option | Benefit | Cost / risk |
| --- | --- | --- |
| HTTP in UI callbacks | Minimal plumbing | Frozen navigation and display timing risk |
| Watch worker plus validated RAM snapshot | Direct Wi-Fi weather and responsive UI | Bounds, synchronization and retry ownership needed |
| Phone-proxied weather | Centralizes networking | Adds phone dependency not used by the delivered weather path |

## Implemented decision and rationale
Firmware workers own connection intent, retry policy and verified HTTPS fetches.
Open-Meteo Madrid data is parsed within fixed bounds and published as complete
thread-safe snapshots. LVGL draws values only. Failures retain the last successful
cache and expose freshness; user Wi-Fi OFF prevents reconnect.

## Consequences
- Weather works without the phone but needs watch Wi-Fi for updates.
- Invalid arrays/dates/numbers cannot silently replace a valid cache.
- RAM data does not survive reboot; offline age is not a current-observation guarantee.
- Public service terms, attribution and noncommercial usage restrictions matter.

## Adoption and compatibility
Current-weather source grew into hourly/daily forecasts without moving networking
into UI. Latest response bound is 16,384 bytes; early 2,048-byte documentation is historical.

## Validation
Parser/UI cases, malformed/offline fixtures, FW/BOOT and manual AP recovery.
Recent cache startup logs observed; no service availability or forecast-accuracy guarantee.

## Open questions and approval history
Configurable location/commercial service use is outside scope.
2026-10-23: retrospective record; ratification pending; no external API calls performed here.
