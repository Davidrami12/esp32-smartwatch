# mobile-activity-dashboard

## Screen and source

Companion **Activity dashboard** opens a dedicated Compose screen, with explicit
Companion/Android Back and Refresh. Black background, large step typography,
monochrome cards and a restrained mint accent; original UI, no proprietary assets.

- Today: full count, watch-owned goal, uncapped numeric percentage, remaining steps,
  capped 0–100% animated visual progress.
- Seven-day chart: oldest through today, English weekday labels/counts, today mint,
  selected-day date and full count, total and average over all seven calendar days.
- Seven lightweight Compose Canvas bars, individual accessible/clickable columns,
  zero baseline markers, short finite height/progress transitions. Each hit target
  is at least 48dp; narrow screens can scroll horizontally. No chart dependency.
- No-data state; LIVE vs CACHED/NOT LIVE badge and explicit last update timestamp.

Compose → WatchViewModel → ActivityDashboardRepository → existing WatchRepository
StateFlow/secure BLE. Only `000d` dated activity and optional `000e` goal are consumed.
No Health Connect reads, new permissions, alternate step source, duplicated sync
logic, calories/distance/GPS/heart rate or goal editing.

## Calendar and goal

The seven dates use the current CET/CEST calendar (`Europe/Madrid`), matching watch
firmware, regardless of phone timezone. Counts come only from dated watch records.
Missing dates display zero, with a no-record annotation; original snapshots are not
filled with zeros or modified for Health Connect. Today's missing value is clearly
identified as no watch record, not a live zero-step measurement. Daily intervals
retain their original aggregate meaning; no walking timestamps are generated.

Average = seven-day sum / 7, including missing-day display zeros. After midnight the
window advances; yesterday is never relabeled today. Until the watch supplies a
current-day record, dashboard is not live. This uses the phone's instant converted
to watch calendar, so an incorrectly set watch/phone clock can leave it cached.

Read-only protected `000e` is the smallest additive goal extension: uint32, exact
watch persistent goal range/units. Boot and successful watch-side persistence
publish it; Android refreshes along with activity. No independent 8,000-step default
is stored in Android. Missing goal means no percentage/remaining calculation; cached
goals retain their own read timestamp. Goal changes can take up to one minute unless
Refresh is tapped. Existing `000d` schema and Health Connect behavior are unchanged.

## Cache and freshness

Private no-backup SharedPreferences `activity_dashboard/snapshot` contains one bounded,
versioned binary cache encoded as Base64: source, validated original dated snapshot,
activity receipt time, optional watch goal and goal read time. It survives Activity/
process restart; no cloud or sensitive data logging. Corrupt/unreadable cache is
reported and safely replaced by the next trusted snapshot. A different watch does
not inherit the old watch's goal. This is a presentation cache, not an editable source
of truth or a history database.

LIVE requires a secure CONNECTED session, matching source, a current-day record,
successful latest read and receipt less than 90 seconds old. Disconnect/read failure/
expiry retains last known data but marks it cached. A 15-second local freshness tick
updates badges/calendar without extra BLE polling. The existing service reads each
minute and on connect/Refresh; firmware's 10-second read-fragment cache still applies.

## Reconnect regression caught during validation

Initial physical validation found repeated reconnects. Logs showed the existing bond
could verify securely, then GATT was dropped immediately after ready. `nextOperation`
could be re-entered by CONNECTED StateFlow observers starting the new activity/goal
read, overwriting a pending operation and its timeout. A pending-operation guard now
preserves serialization, and retry UI derives the real bond state instead of default
NOT_BONDED. Generic disconnect reasons are logged for diagnosis. No bond erasure,
automatic re-pairing, weaker encryption or security-status bypass was added.

Final Pixel logs show secure verification, `000d`/`000e` successful reads on repeated
refreshes, and successful media transmission without the immediate disconnect.
User confirmed everything works after the fixed APK installation. This does not
establish the root cause of all older intermittent secure-read status-7 failures or
prove indefinite background stability.

## Validation

- Android assembleDebug / 32 unit tests (8 new dashboard tests) / lintDebug: PASS.
- Firmware build / simulator build / three simulator regression suites: PASS.
- COM9 flash, bounded runtime boot and final Pixel installation: PASS.
- Physical real BLE dashboard/reconnect workflow: PASS, user confirmation after fix.
  Requested workflow covered watch steps/goal, chart selection/history/totals,
  disconnected cache and reconnect; user reported everything works.
- Unit coverage: seven calendar dates, missing/real-zero values, correct totals and
  average, remaining/percentage/over-goal cap, unknown/invalid goal, cache round-trip/
  corruption/restart representation, disconnection/source mismatch/read error/expiry,
  midnight, phone timezone and DST boundaries.
- Real midnight/DST crossing, long-duration background reconnection, TalkBack/large
  font/accessibility and explicit process-restart offline cache inspection remain
  manual follow-ups, not claimed as physically exercised by unit tests.
- Existing flash-size mismatch, BLE light-sleep limitation, LVGL advisories and
  Android dependency/KTX lint warnings remain. No commit or push; branch retained.
