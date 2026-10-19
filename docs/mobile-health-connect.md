# mobile-health-connect

## Scope and permissions

Official AndroidX `androidx.health.connect:connect-client:1.1.0` (stable). Only
`android.permission.health.WRITE_STEPS` is declared/requested. No READ_STEPS,
background-read or history-read permission, fitness account, cloud, location,
Health Connect firmware/UI dependency or extra foreground service.

The Health Connect card checks SDK/provider availability and current permission on
Activity start and before writes; it displays permission, last successful write,
errors, Sync now, automatic-sync toggle (default OFF), and settings/privacy links.
The foreground Activity launches the official permission contract. Rationale/privacy
Activity handles Android 13 rationale and Android 14+ protected permission-usage
alias. Denial/revocation pauses writes without launching background prompts.

## Source and history

`watch_steps` detects steps; `watch_activity` owns dated daily totals and its seven
slots. Existing BLE `0003` carries no date. UI history's zero-filled missing dates
cannot be exported as real observations. Additive secured read-only `000d` therefore
exports up to seven **actually stored** date/count/interval/offset records. Schema
is in `ble-protocol.md`. Initial sync writes today first, followed by available
recorded history. No record is created for an absent date. SDK StepsRecord requires
at least one step: true zero dates remain absent. A correction from positive to
zero uses idempotent deletion of only this app's matching clientRecordId, with the
same durable journal ordering; it does not manufacture a one-step record.

Calendar intervals are watch local midnight to snapshot for today, and midnight to
next midnight for completed days, with watch-derived offsets. They convey daily
aggregate coverage, **not precise walking times**. The phone's local date never
labels undated steps. DST is respected. Firmware currently uses CET/CEST; changing
the phone timezone does not change watch dates or IDs. Forward-clock drift greater
than five minutes and unsupported counts (>1,000,000) are rejected, not clamped.

`StepsRecord` metadata is auto-recorded, device type WATCH, manufacturer Waveshare,
model ESP32-S3 Touch AMOLED 2.06. Health Connect attributes the writing data origin
to `dev.esp32.smartwatch`; device metadata identifies the external watch. The app
does not attempt to impersonate a Wear OS health provider.

## Duplicate prevention and durability

One ID per watch/date: `watch-steps-v1:<SHA256 BLE identity>:<ISO local date>`.
Identity is the existing stable BLE address for this firmware (future rotating
identities need an explicit stable device ID). Counts, phone timezone and reconnects
do not alter the ID. `insertRecords` performs an upsert when the new
`clientRecordVersion` is higher, rather than adding another daily total.

Private durable journal stores content fingerprint/version/acknowledgement per ID.
New content uses `max(nowMillis, previousVersion+1, 1)`, so count corrections downward
and phone clock corrections still replace earlier content. Persist intended version
**before** insertion; acknowledge only after SDK success. Remote success followed by
local failure/process kill replays the same version and ID, with no second record.
Unchanged manual sync re-inserts the same version safely; automatic sync skips
acknowledged unchanged content. Snapshot-time changes alone do not cause writes;
count/offset changes and finalizing a completed day do.

Writes/deletions target only IDs created by this app. No cross-app reads, updates or deletion.
Uninstall/app-data clear loses the local journal. Stable IDs still prevent multiple
records with the same ID, but replacement ordering after journal loss/large clock
rollback is not guaranteed without reading existing versions; these are out of
scope for WRITE_STEPS-only PoC. Deleting own entries in Health Connect can be repaired
with explicit Sync now. Automatic sync may consider a deleted unchanged entry
acknowledged until changed/manual replay. There is no silently destructive reset.

## Lifecycle and automatic sync

Application health manager/Repository/StateFlow integrate with the existing
ViewModel and CompanionService. Transport has only a generic dated-activity
snapshot hook and no Health Connect dependencies. Secure connection triggers a
read; once/minute reads catch count changes and day rollover even with no steps.
Manual Sync now fetches a fresh bounded snapshot through the same GATT queue.
Firmware read-fragment cache can make a snapshot up to 10 seconds old.

Automatic writes coalesce to latest snapshot and are rate-limited to one batch per
five minutes, with an initial short delay. A mutex serializes manual/automatic
operations. Every batch checks current permission; every record checks session/
automatic consent and permission again. Turning automatic off or disconnecting
cancels queued work; an already submitted provider operation cannot be undone.
Partial batch failures retain journal state and retry safely. No new FGS or reboot/
force-stop resurrection: user must re-enable the existing companion.

## Validation checklist

Automated production engine tests: first/repeated sync, stable IDs, increasing and
decreasing counts, permission denial/revocation, failed remote write, failed local
ack after remote success, reconnect/restart replay, daily rollover/finalization,
missing history, different sources/phone timezones, zero totals, DST 23/25-hour
days, malformed payload, unsupported count and ahead-of-phone clock.

Physical Pixel checks still require explicit permission consent and Health Connect
entry inspection: denial then grant, first sync, repeated Sync now leaving one
record per date, walking/increased total replacing the same record, attribution,
reconnect and Activity/process restart, permission revocation, automatic batch,
midnight/history and phone timezone change. No fabricated test steps are inserted.

## Validation evidence

- Firmware and simulator builds: PASS. Simulator's three existing game/storage/UI
  regression suites: PASS. Shared UI and simulator sources were not changed.
- Android assembleDebug, testDebugUnitTest (24 tests, including 11 Health Connect
  policy/SDK record tests) and lintDebug: PASS (lint has advisory
  dependency/resource/KTX warnings, no errors).
- COM9 flash and bounded startup/runtime: PASS; activity loaded a trustworthy dated
  total and secure BLE initialized/reconnected without a crash. Monitor stopped cleanly.
- Pixel installation: PASS. User confirmed Health Connect available, Write steps
  grant and first successful Sync now: **PASS (physical user confirmation)**.
  Final APK, including SDK-tested zero-total handling, installed successfully;
  the first physical sync was on the preceding APK. Further physical checks of
  the final zero-total refinement are skipped. APK replacement stops the companion;
  enable it again through the visible app to resume.
- Real Health Connect entry/device attribution, repeated sync duplicate inspection,
  increased-total replacement, historical data inspection, denial/revocation,
  disconnect/reconnect/restart, automatic background batch, midnight and timezone
  interaction: **SKIPPED at user's request**. Production-policy tests are not proof
  that those provider/device interactions passed.

No synthetic steps, ADB permission grants/revocations, other-app record changes,
NVS erase, commit or push were performed.
