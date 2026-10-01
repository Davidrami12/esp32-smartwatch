# Dashboard and connectivity

## Responsibilities

- `watch_wifi`: serialized connection requests/events, explicit disconnected/connecting/connected state,
  user intent, retry policy, and NVS. UI requests are queued; no network work runs in LVGL callbacks.
- `watch_weather`: background HTTPS/JSON fetch and thread-safe RAM cache. No LVGL calls.
- `watch_activity`: local-calendar rollover, seven records, persistence and history snapshots.
- `watch_steps`: detection and atomic counter operations only.
- `watch_ui`: screen state, icons, labels and callbacks only. No HTTP, Wi-Fi or NVS dependencies.

## Wi-Fi

NVS namespace `watch_wifi`, key `enabled` (`u8`, 0/1). Missing/invalid intent defaults to enabled.
Credentials still come from the existing build configuration and are not edited or copied into a new store.
Manual disconnect clears intent before calling `esp_wifi_disconnect`; its event cannot reconnect.
Manual connect enables intent and immediately presents CONNECTING; duplicate requests are rejected.
Connected means an IP address was obtained, not merely association or an RSSI reading.
Unexpected loss retries every five seconds in bursts of five, then waits 60 seconds before continuing.
Association/DHCP stalls have a 30-second deadline. Failures show DISCONNECTED between retries.
NVS errors are logged; live control remains usable, but persistence cannot be guaranteed after an error.

## Madrid weather

Provider: [Open-Meteo](https://open-meteo.com/) (weather data attribution).
Use the public endpoint only within its [licence/usage terms](https://open-meteo.com/en/terms).
There is no API secret; the public API is intended for non-commercial use.

`https://api.open-meteo.com/v1/forecast?latitude=40.4168&longitude=-3.7038&current=temperature_2m,weather_code,wind_speed_10m&temperature_unit=celsius&wind_speed_unit=kmh&forecast_days=1`

HTTPS uses the ESP-IDF CA bundle and server verification. JSON parsing uses managed `espressif/cjson`.
Only a bounded 2048-byte response and three numeric current-condition fields are needed.
Fetch after connection (and trustworthy system time), refresh every 30 minutes after success,
retry no sooner than five minutes after failure, including across rapidly flapping connections.
No requests while disconnected; the last successful RAM value remains visible until reboot.
With no successful fetch, Home shows unavailable. Networking never wakes the display.

Simplified categories, in priority order:

1. WMO 95–99: thunderstorm.
2. WMO 51–67 and 71–86: precipitation/rain (snow folded into this limited icon set).
3. Sustained 10m wind >= **30 km/h**: windy (`kWindyKmh`, centralized in `watch_weather.cpp`).
4. WMO 0–1: sunny.
5. Remaining codes (including 2–3 and fog 45/48): cloudy.

Weather icons are tiny LVGL line/circle drawings (sun, cloud, rain strokes, lightning and wind strokes),
not emoji/fonts/assets. Temperature is Celsius. UI receives a simplified enum, never provider JSON/codes.

## Activity history

NVS namespace `watch_activity`, key `history`: one versioned **60-byte blob**:
`u32 version=1`, seven pairs of `u32 YYYYMMDD` and `u32 steps`. Fixed-size, bounded, newest-first on retrieval.
Legacy `day`/`steps` are imported only when `history` is absent; no partition erase.
Today remains live; rollover atomically swaps the detector count and finalizes the previous day.
Missing calendar dates return zero. Calendar iteration uses local noon and `tm_mday`/`mktime`,
not repeated 86400-second subtraction, so month/year/leap-day/DST boundaries follow local time.
Entries older than today minus six calendar days are discarded.
During a backward clock correction, future-dated entries are temporarily retained in the same seven slots
(not displayed as past days), so correcting back to that date can restore its count.
Invalid epoch/time defers restore/rollover/writes; matching-day restore adds pending RAM steps.
Dirty changes save approximately once per minute; day transitions save immediately. Errors retain dirty RAM state.
A sudden power loss may lose steps since the last successful batched save.

## Validation

Firmware/simulator builds and physical validation follow `AGENTS.md`.
Host tests execute the production activity module with RAM NVS and fake clock/task interfaces:

```text
# Visual Studio developer prompt, repository root
python tests/activity_history_host.py
```

These cover migration, restore, rollover, missing days, invalid-time recovery, date corrections,
calendar windows around European DST dates, leap/year boundaries, batching, storage failures and pruning.
On-watch DST behavior, midnight rollover, Wi-Fi intent after reboot and manual controls still require physical tests.
Simulator mocks a seven-day list, Madrid 24°C/sunny, and manual disconnect/connect with a three-second CONNECTING phase.
