# Madrid seven-day weather

Apps → WEATHER and a tap on Home's Madrid card both open the same seven-row forecast.
Rows show TODAY or weekday with DD/MM, the existing condition icon, and high / low °C.
Back/right-swipe returns to Apps. Weather data attribution appears beneath the list.

Firmware networking remains solely in `watch_weather.cpp`. Open-Meteo's documented
`/v1/forecast` endpoint now requests current weather plus daily `weather_code`,
`temperature_2m_max`, `temperature_2m_min`, and `wind_speed_10m_max`, with
`timezone=Europe/Madrid` and `forecast_days=7`. This means **today plus six days**.
No API key or secret is required for the current free noncommercial endpoint.
Docs: https://open-meteo.com/en/docs ; data attribution: https://open-meteo.com/ .
Review licensing/commercial service terms before distributing commercially.

The existing background worker, verified HTTPS certificate bundle, 30-minute success
refresh, and five-minute retry/reconnect rate limit are retained. No request runs on a
UI callback and forecast updates never wake the display. Response is bounded to 4096 bytes;
overflow, HTTP errors, malformed JSON, invalid dates, numbers/nulls or array lengths reject
the new forecast and retain the last successful RAM-only cache. Worker stack is 12KiB
to accommodate the larger response. Cache snapshots use the existing critical section.

Each daily snapshot contains Madrid-local YYYYMMDD, low/high Celsius and existing condition
enum. Thunderstorm precedes precipitation (snow shares rain icon), then WIND at daily max
10m wind >=30km/h, then sunny/cloudy. These are daily forecast summaries, not current weather.
Firmware matches calendar dates to today's seven local days at noon with mktime, including
DST/month/year boundaries. Old dates drop out after midnight; unavailable future slots
show -- instead of being relabeled as fresh days. No 86400-second date arithmetic.

Shared UI receives only seven presentation rows, online state and monotonic cache age.
Offline data is marked OFFLINE / CACHED; >=35-minute-old online data is STALE / CACHED.
No data shows loading/unavailable. Cache age is not affected by SNTP corrections. Reboot
clears the cache. Simulator has seven varied conditions/high-low mocks and offline labels
when disconnecting Wi-Fi; they are not forecasts for a real account/device.
With the extra forecast screen/task, ordinary malloc now prefers PSRAM at all sizes
(`SPIRAM_MALLOC_ALWAYSINTERNAL=0`); the existing 64KiB internal reserve remains for
DMA/internal-only allocations. A 256-byte preference threshold still starved the LCD
bounce buffer during the first hardware test. This can modestly increase PSRAM access
latency; timing and peripheral buffers retain their explicit internal-capability policies.

Physical checks: both entry points, seven-row fit, date/temperature legibility, icon clarity,
Back/swipe, disconnected cached state, network recovery, midnight slot reconciliation,
and display timeout/wake preservation. Existing local time tools/steps must remain working.
