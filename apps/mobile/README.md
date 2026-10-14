# Android companion — secure background BLE, controls, notifications and media

## AI Assistant — deployed Firebase/Cloud Run configuration

- Firebase project: `esp32-smartwatch-511417`.
- Backend: `https://watch-ai-backend-z5stegdulq-no.a.run.app` (fixed deployed URL).
- Open **AI Assistant · configure / sign in**. Enter the Firebase public **Web API
  key** for that project and your Firebase Email/Password account. Never enter an
  OpenAI key. Only the backend's allowlisted UID is authorized; email alone is not
  authorization. The phone checks returned token audience/issuer/UID consistency;
  the backend remains responsible for signature, expiry, revocation and UID checks.
- Optional build-local setup: copy `ai.local.properties.example` to
  `ai.local.properties` and supply `firebase.webApiKey` privately. That file is
  ignored by Git; never add passwords, bearer/refresh tokens or an OpenAI key.
  When supplied, the public project-identifying key is included in BuildConfig/APK:
  it is **not a secret** and is not trusted as backend authorization. Otherwise,
  enter it on the phone; only this public key is saved in private app preferences.
- Passwords, ID tokens and rotating refresh tokens are memory-only, never logged
  or persisted. Near expiry (last 60 seconds), the next authenticated operation
  uses the official Secure Token REST endpoint to refresh. Re-login after process
  death/restart. Disabled/revoked/invalid refresh sessions clear authentication;
  transient network errors allow a retry. Logout cancels active AI HTTP calls and
  prevents a queued/in-flight login or refresh from restoring a signed-out session.
- **Sign in & enable paid AI** POSTs an empty `audio/wav` body with the ID token.
  The known backend authenticates/allowlists first, then returns its specific 400
  invalid-WAV response before Firestore quota reservation or OpenAI. 403 means
  non-allowlisted UID; 401 means rejected authentication; 503 is service/configuration
  failure. HTTP redirects are disabled. No audio is submitted by login verification.
- **Allow paid AI requests** is OFF initially, after logout and after process
  restart. Successful sign-in AND backend UID verification automatically enable
  it for new hold-to-talk questions (except transfer-test builds). The sign-in UI
  explains this authorization and potential charges. Manually switching it off
  keeps it off until explicitly enabled or another successful sign-in. For unpaid
  transfer tests, sign out or switch it off before recording. Firebase/Cloud Run networking itself can
  still have ordinary service charges, but the login probe cannot call OpenAI.
- Consent is captured when the phone claims a recording. If OFF then, enabling it
  later cannot authorize that old clip: record a new question. Any consent change
  revokes queued tickets and cancels active HTTP calls. Cancellation cannot undo a
  request already accepted remotely or guarantee a refund.
- Optional v2 lossless PCM notification windows are negotiated via protected
  capabilities/CCCD; missing/unsupported peers retain legacy pull. For offline A/B
  APK builds use `-PvoiceNotify=false` (pull) or `-PvoiceNotify=true` (default).
  Physical validation/approval is still required; no measured speedup yet. See
  `../../docs/voice-ble-window-optimization.md` for the protocol and test plan.

### Firebase registration/key prerequisites

This integration uses native HTTPS **Firebase REST**, not the Firebase Android SDK
or Google Services Gradle plugin. Android app registration and `google-services.json`
are **not required for this Email/Password REST flow**. Firebase Auth must be enabled
in the correct project; a Web app registration is useful to obtain its public Web
API key/configuration. The backend requires its authorized UID and existing IAM;
this Android change does not create users, alter IAM, Firestore or backend settings.

Use a Web API key with API restrictions compatible with both Identity Toolkit
sign-in and Secure Token refresh. Browser-referrer or Android application-restricted
keys may reject these REST calls. This client does not send Android package/cert
restriction headers. Do not silently weaken an existing key: use the appropriate
approved Web key. If adopting Android-restricted credentials/native Firebase SDK
later, register package `dev.esp32.smartwatch` and its actual debug/release signing
certificate fingerprints, then implement matching headers/SDK configuration.

Mock session tests cover login, wrong project, refresh/rotation, transient/fatal
refresh failures, logout races and disabled paid-request gate. They do not prove
real Firebase login/UID access or token refresh. Real setup is unverified without
the project's actual Web key and authorized user's privately entered credentials.

## Activity dashboard (TFM)

Open **Activity dashboard** for today's watch steps, watch-owned goal/progress,
interactive seven-day Canvas chart and total/average. A private persistent cache
keeps the latest snapshot offline, clearly marked CACHED/NOT LIVE with update time.
Dates use watch CET/CEST semantics; missing chart dates display zero but are never
manufactured into Health Connect records. Optional protected read-only `000e`
provides the existing persistent goal; older firmware shows unavailable progress.
No extra permissions or Health Connect readback. Full implementation/validation:
`../../docs/mobile-activity-dashboard.md`.

## Health Connect steps (TFM)

Official Health Connect SDK, write-steps-only permission, explicit Sync now and
optional batched automatic sync in the existing companion service. Grant access
from the new Health Connect card; inspect records in Health Connect, not a new
health-reading API in this app. Today and existing seven-day recorded history need
the secured dated-activity `000d` firmware extension; older undated telemetry is
never assigned a guessed phone date. Daily aggregates retain watch offsets and do
not represent precise walking timestamps. Stable watch/date IDs and durable versions
upsert the watch's records without adding the entire total again on each refresh.
Privacy rationale is accessible both in the app and from Health Connect permissions.
Full strategy, limitations and validation: `../../docs/mobile-health-connect.md`.

Kotlin / Compose / native Android BLE, designed for the Google Pixel 10 Pro.
`../../docs/ble-protocol.md` remains the protocol authority. Media requires the
additive media-enabled firmware; older secured v1 firmware retains existing features.

## Native media companion

The existing CompanionService owns `media/MediaSessionBridge.kt`: supported Android
`MediaSessionManager`, `MediaController.Callback` and `TransportControls`, not a
Spotify/app-specific API. Grant notification-listener access, enable the companion
from the visible app, start audio in a normal Android media app, and open **Apps →
Media** on the watch. No extra foreground service or media logic in Compose/listener.
Event-driven title/artist/playback snapshots use secured UUID `000a`; authenticated
watch previous/play-pause/next commands use `000b`. Session generations reject stale
commands; supported-action flags disable unavailable buttons. Multiple sessions prefer
PLAYING, otherwise PAUSED, in manager order (up to eight observed controllers).
Stopped/transitional-only sessions show unavailable. No polling, artwork,
seek, playlists, streaming or music storage. Text uses the watch's bounded printable
Latin-1 repertoire. Activity closure leaves the existing service observing sessions.

Relative volume −/+ was subsequently added on user request using native
`MediaController.adjustVolume`, gated by protected firmware capability `000c` and
non-fixed session PlaybackInfo. It adjusts the selected route; no volume percentage
or slider. Schema 1 fallback preserves older media-enabled firmware.

### Media validation (2026-10-12)

- Firmware/simulator builds, headless UI/parser tests, Android assemble/unit tests/lint,
  diff check, COM9 flash, bounded runtime boot and Pixel APK installation: PASS.
- After the initial unavailable report, standard Service Changed discovery was deferred
  750 ms; logs confirmed both media characteristics, command subscription, secure ready
  and snapshot delivery. The existing bond was retained.
- Real title/artist presentation and playback controls: **SKIPPED** after the user
  requested no further physical tests. Transport delivery is not media-control PASS.
- Pending: pause/resume/next/previous, phone-side track changes, locked-Pixel/closed-Activity
  controls, session disappearance, access revocation and extended background stability.
- APK replacement requires explicit companion re-enable. No commit/push. Existing
  firmware/LVGL/dependency advisory warnings remain; no observed boot crash.
- Final diagnostic/busy-state APK builds/tests/lint PASS after restoring missing
  temporary SDK files. Its replacement installation is SKIPPED: Pixel serial
  `56201FDCH000N5` now reports ADB unauthorized. The preceding media APK installation
  passed; do not interpret that as installation of these final refinements.

Follow-up: Pixel ADB authorization recovered; final diagnostic APK installation
passed and the user confirmed Media works after reconnecting with Spotify playing.
The requested UI refinement centers title/`Artist: name`, uses circular playback
buttons, and animates a record ring only when PLAYING, visible and awake. Volume
support and animation have automated protocol/UI coverage; real volume response and
physical appearance still require manual confirmation.
User subsequently confirmed the layout, rotating/static record and volume worked.
Final follow-up enlarges title to 30 px and places artist 8 px below the actual wrapped
title, keeping the bounded group centered. Firmware/simulator tests and COM9 flash
passed; final typography requires visual inspection. Android build/tests/lint and
Pixel installation for the volume-capable APK passed.

## Build

Open this directory in Android Studio, or use JDK 17 and an Android SDK containing
API 36, platform-tools and build-tools 35.0.0. Set `ANDROID_HOME` or a local,
ignored `local.properties` (`sdk.dir=...`).

```powershell
.\gradlew.bat assembleDebug testDebugUnitTest lintDebug
adb devices -l
adb -s <PIXEL_SERIAL> install -r app/build/outputs/apk/debug/app-debug.apk
adb -s <PIXEL_SERIAL> shell am start -n dev.esp32.smartwatch/.MainActivity
adb -s <PIXEL_SERIAL> logcat -s WatchBle
```

Versions are pinned: AGP 8.13.2, Gradle 8.13, Kotlin/Compose compiler 2.2.21,
Compose BOM 2025.12.01 / Material3. minSdk 33, target/compileSdk 36.
Android 13+ is intentional: it covers the Pixel and permits memory-safe native
GATT value callbacks/descriptor writes without maintaining legacy API paths.
Nearby devices (`BLUETOOTH_SCAN`, `BLUETOOTH_CONNECT`) are required. Optional
`POST_NOTIFICATIONS` is requested explicitly for the ongoing connection notification.
Manifest also declares FOREGROUND_SERVICE and FOREGROUND_SERVICE_CONNECTED_DEVICE.
Scanning is declared `neverForLocation`; no location permission or tracking.
No Internet/location permission, content database, cloud or DI framework. A user-started
connectedDevice foreground service maintains BLE; preferences store only the last
successfully verified watch address, not keys, telemetry or private notification content.

## Structure

`app/src/main/java/dev/esp32/smartwatch/`:

- `MainActivity.kt`: dark Compose discovery/dashboard, permission launcher,
  Android app-settings link and lifecycle observation.
- `WatchViewModel.kt`: UI actions through the companion service API; observes the
  repository StateFlow and never owns connection lifetime.
- `WatchState.kt`: application state/nearby device records; no raw GATT objects.
- `SmartwatchApplication.kt`: single process-local companion API and normalized content
  boundary; ViewModel and listener use the same session.
- `companion/CompanionConnection.kt`: visible-user-action service launch/command API,
  one repository StateFlow, listener forwarding without background service launches.
- `companion/CompanionService.kt`: started connectedDevice foreground service, sole
  owner of BLE operations/lifetime, minimal persistent notification and task-removal policy.
- `companion/ReconnectPolicy.kt`: pure capped backoff and disconnect-intent policy.
- `notifications/WatchNotificationListener.kt`: OS-bound listener, extraction and
  volatile deduplication, no Bluetooth APIs.
- `notifications/PhoneNotification.kt`: five-field model, centralized filtering/
  font-safe sanitization and 20-byte frame encoding.
- `ble/WatchRepository.kt`: single main-looper BLE session, scan/connection,
  serial operation queue, timeouts, reads, subscriptions and write/readback.
- `ble/WatchBleProtocol.kt`: centralized UUIDs and strict LE snapshot decoders.
- Unit tests under `app/src/test/`: documented values, unsigned uint32 maximum,
  zero/decreased steps, invalid lengths/ranges and unknown Wi-Fi states.

## Connection contract

DISCONNECTED → user enables service/SCANNING (15-second timeout) → manual selection → CONNECTING
(20-second timeout) → PAIRING if explicitly selected and not already bonded →
DISCOVERING_SERVICES → verify services/read/notify/CCCD properties →
read protected Security Status and require bonded/authenticated SC → read version and require v1 →
read telemetry and optional controls → subscribe to discovered values → CONNECTED.
Each queued operation has a 10-second timeout; only one is outstanding.
Transport failures release GATT and retry the existing bond with backoff; security,
permission and protocol failures require explicit action. Cancel/disconnect stops the service, releases GATT,
clears displayed measurements and suppresses automatic reconnect.
Stale callbacks from prior GATT or scan sessions are ignored.

Battery ATT 0x0E is the documented temporary PMIC-unavailable case, not a fatal
protocol error. Other initial read/subscription failures stop setup. Malformed
notifications are ignored with a visible warning; unknown Wi-Fi values display
Unknown, never Connected. Steps replace the complete count, including decreases.

The optional additive v1 settings group has Brightness, Screen Timeout and Wi-Fi
Enabled (intent) UUIDs, centralized alongside telemetry in `WatchBleProtocol.kt`.
Security-enabled telemetry-only firmware can show Controls unavailable; old insecure
firmware without protected Security Status is rejected.
All controls are read/write-with-response/notify and expose actual initial values.
Brightness writes once on slider release, never on each drag tick. The preview is
explicitly labeled and the confirmed percentage remains shown separately.
Timeout chips and Wi-Fi switch do not optimistically change the confirmed state.
All controls are disabled until the current write callback and readback finish.
Rejected writes trigger actual-value readback and a small error, not a fatal
telemetry disconnect. Failed readback disables controls until reconnect. Lost
callbacks/timeouts close the ambiguous GATT session and require fresh reads.
Watch settings are not persisted on the phone (only the secure watch selection is). See `../../docs/ble-protocol.md` for wire values,
validation/ATT errors, firmware persistence semantics and security limitations.
The standard Service Changed indication is subscribed/handled to invalidate old
Android GATT caches after firmware updates; no hidden refresh APIs are used.

Native scans filter by advertised Watch Service UUID, not name. Results are
deduplicated, signal sorted and selectable; address is an opaque session key,
not authentication. Bonding, authenticated SC encryption and trusted identity checks
are mandatory; no weaker recovery fallback or automatic background re-pairing.

Notifications replace values immediately via StateFlow; there is no polling.
The watch itself limits steps notifications; small changes can take 15 seconds.
The foreground service keeps the connection/recovery alive across Activity close,
Home, screen lock and recents removal. Unexpected transport loss retries after
0/5/15/30/60/120 seconds, then every 120 seconds; one 20-second connection attempt at
a time, no recovery scans or polling. Each successful attempt re-verifies protected
status, reads and subscribes. Manual Disconnect cancels retries and stops the service.
Process death/force-stop/reboot does not auto-start; enable explicitly from the app.
ViewModel clear and Activity stop do not close GATT. See background companion below.

Development logs include statuses and operation UUIDs, not device names,
addresses, payloads, credentials or account data. Permissions are checked before
operations and revocation is caught. Enable Bluetooth manually if disabled.

## Physical validation checklist

1. Enable USB debugging and authorize the workstation; install on Pixel.
2. First scan requests Nearby devices; deny and verify explicit messaging and
   app-settings recovery. Grant and rescan. Test permission revocation in settings.
3. Enable watch Bluetooth; close nRF Connect/other central sessions (watch supports
   one central). Verify only compatible watches appear, timeout and stop scan.
    If keys are stale, explicitly forget on BOTH watch and Android (see security below).
4. Connect: first pairing compares a six-digit code on both devices; confirm both.
   Wait through bond/encrypted verification/discovery. Compare version v1, battery,
   today's steps and Wi-Fi with the watch. Battery unavailable must not crash.
5. Walk >=5 steps; wait for notification. Verify a trailing small delta within
   15 seconds; Wi-Fi toggles update and battery changes update without polling.
6. Manual Disconnect: watch resumes advertising; connect again with fresh reads.
7. Disable watch BLE/power-cycle/out-of-range: no crash or stale connected UI;
   service-owned backoff recovers when advertising returns, including in background.
   Explicit authentication/bond failure stops recovery; manual security repair is required.
8. Screen rotation / app background / return: no duplicate sessions or scans.
9. Watch IDLE/double-tap and Wi-Fi/weather/NVS saves remain functional while connected
    with the authenticated bonded GATT connection. Test reboot persistence separately.

No notification replies/actions, phone dismissal, provisioning, history sync, OTA,
accounts, Health Connect, weather sync or phone-reboot autostart.
Settings controls and text-notification transport are the only remote content/
commands. The simulator has no BLE emulation but builds the hardware-neutral UI.

## Notification forwarding

Open Smartwatch → **Grant notification access** → enable **Smartwatch notification
forwarding** in Android listener settings. The app displays NOT_GRANTED / GRANTED
and explains that forwarding requires verified authenticated Secure Connections.
POST_NOTIFICATIONS is separate and optional, solely for the connection-status notification;
it is not notification-listener access. No location permission is requested.

Connect to the watch. Newly posted eligible notifications are normalized to
package, app label, title, body and timestamp. Ongoing/service/system/own-app,
group summary, empty and identical repost content is filtered. No historical replay,
images, extras, actions or emoji. Basic Latin / Latin-1 only for current fonts; long
content is byte-bounded. Listener never contains GATT logic: it forwards through
the application boundary to the same repository used by the ViewModel.

The optional `0008` characteristic receives 9–20-byte writes with response, with
framing v1 and sequenced offset/ID/total. See `../../docs/ble-protocol.md` for exact
bytes. One message in flight, five waiting; overflow discards oldest waiting.
Disconnected notifications and pending content on disconnect are dropped, not
persisted. The foreground companion owns the background BLE session and recovery;
the OS-bound listener only forwards normalized content through the application API.
It never starts a foreground service or owns GATT. Process death means no BLE
connection, even if Android restarts the listener alone.

The watch holds one current screen plus five waiting; DISMISS/right-swipe advances
then returns to the prior screen. Title/body wrap and scroll vertically. A complete
notification wakes IDLE and resets normal timeout, with no vibration. Timeout does
not consume the current message. Diagnostics contain counts/statuses, never bodies.
Android can redact sensitive notifications (including OTP) or suppress work-profile
content; this app does not bypass those OS protections.

Physical checks: grant/deny/revoke access, genuine notification app/title/body,
IDLE wake and normal timeout, dismiss/scroll, >5 messages/overflow, disconnect mid
message, disconnected drops, and previous settings/telemetry stability.

### Notification feature validation

- Firmware build, simulator build, headless UI/parser tests, Android build/unit
  tests/lint, `git diff --check`, COM9 flash and runtime boot: PASS.
- Pixel installation of the initial notification build: PASS. Notification access
  grant and connection confirmed by user.
- BLE chunk delivery: PASS in Pixel/write-response logs and firmware receive/display
  logs; no crashes or DMA errors in the captured runtime window.
- User confirmed readable app/title/body, DISMISS/return, IDLE wake, and bounded
  numbered-message queue advancement: PASS.
- On 2026-10-06 user confirmed disconnect stability, dropping disconnected
  notifications and no replay after reconnect: PASS (manual confirmation).
- Final APK (revoked-access diagnostic refinement) builds/tests successfully but
  its replacement install is pending: latest adb check lists no connected Pixel.
- Remaining manual tests: deliberately malformed/oversized ATT writes on-device,
  disconnect mid-message, access revocation during transmission, long-term stability
  and notifications from multiple real apps. Parser bounds/rejection tests are not
   a substitute for hostile on-device ATT tests. These historical notification results
   preceded watch-ble-security below. No commit or push performed.

## Foundation validation (2026-10-04)

- `assembleDebug`: PASS; APK at `app/build/outputs/apk/debug/app-debug.apk`.
- `testDebugUnitTest`: PASS (2 tests, zero failures).
- `lintDebug`: PASS (no errors). Remaining warnings suggest newer dependency/
  Gradle versions and a KTX URI convenience method; stable pinned versions are
  intentional for this foundation, not a claim to be the latest release.
- `git diff --check`: PASS.
- Pixel installation / real BLE interaction: SKIPPED; `adb devices -l` lists no
  connected/authorized Android device. The physical checklist above is unverified.
- Firmware/simulator builds and watch flash: N/A; only `apps/mobile/` changed.

## Settings controls validation (2026-10-04)

- Firmware build / COM9 flash / runtime boot: PASS; no crashes or LCD DMA failures.
- Simulator build / headless UI + portable firmware parser tests: PASS. Shared UI
  remains hardware independent; simulator does not emulate BLE.
- Android assemble / protocol tests / lint: PASS (3 unit tests, no lint errors).
- Pixel 10 Pro install / real connection: PASS.
- Brightness, timeout, Wi-Fi write-with-response + notification + readback: PASS,
  verified in Pixel logs; user also confirmed controls worked physically.
- Reboot persistence: PASS for brightness 79% and timeout 60000ms, loaded from NVS
  in the watch startup log; reconnected phone read current settings successfully.
- Initial stale Android GATT cache: fixed with standard Service Changed indication,
  fresh discovery confirmed all three new characteristics/properties/CCCDs.
- Repeated ~30-second drops: Pixel logs identified stale OS pairing/key-loss and
  SMP repair timeout. User forgot the watch entry and reconnected; 90-second
  foreground observation had no disconnect callbacks, UI stayed Connected.
- Restored captured settings: brightness 88%, timeout 30000ms, Wi-Fi enabled.
  Temporary Pixel stay-awake-while-charging value restored from 3 to original 0.
- Remaining physical tests: extended stability/background transitions; exact screen
  timeout behavior at each option; disabled Wi-Fi across >60s and reboot; malicious
  ATT payload rejection on-device; deliberate disconnect during a write. Parser unit
  tests cover bad lengths/ranges but do not substitute for these physical tests.
- At that time security was development-only unpaired read/write access; superseded
  by watch-ble-security below. Existing flash-size, BLE light-sleep and LVGL warnings remain;
  Android lint only suggests dependency upgrades / a KTX convenience method.

## watch-ble-security

Native `BluetoothDevice.createBond()` plus bond/pairing broadcasts; Android owns the
pairing confirmation dialog. Compare its code to the watch's six-digit code and
ACCEPT on both devices. Never confirm a mismatched code. No auto-confirm, privileged
permission or hidden bond-removal API. Reject/failed states share a truthful error
because public Android bond callbacks do not reliably distinguish every cause.

The watch requires LE Secure Connections Numeric Comparison, MITM and 16-byte keys,
with persistent NVS LTK/IRK and one trusted identity. Just Works/legacy methods are
rejected. The watch wakes for code comparison; display timeout resumes afterward.
Existing UUIDs and payloads are unchanged. Mandatory protected status `0009` reads
`01 0F 10` before other reads, subscriptions or CONNECTED. Android BOND_BONDED alone
does not imply a verified encrypted session. Old insecure firmware is not a fallback.
Controls and forwarding are additionally gated on verified secure state.

For stale keys or a different phone: watch **Settings → Bluetooth → Forget paired
phone → FORGET**, then Android **Bluetooth → ESP32 Smartwatch → Forget**. Retry from
Smartwatch and confirm a new code. Forget is local/confirmed, does not erase unrelated
settings, and never silently happens on a remote repeat-pairing request. Interrupted
first pairing after watch acceptance reserves the slot until explicit Forget.
Normal reconnect/reboot does not need pairing again. Watch bond capacity is internally
2 to work around IDF 6.1 NVS migration compilation; logical admission is still one
phone and automatic bond eviction is disabled. Details in `../../docs/ble-protocol.md`.

Build/test coverage: protected-status decoding/rejection, firmware strong-key/admission
policy, pairing code formatting, BACK-as-reject, and forget confirmation navigation;
plus existing telemetry/control/content/UI tests. `../../tests/watch_ble_untrusted.py`
uses a separate unpaired PC central for admission testing (correlate firmware logs).

No flash encryption/secure boot/rotating advertising identity, radio DoS protection
or production security certification is introduced. NVS keys remain vulnerable to
physical flash extraction. Endpoint compromise and OS notification privacy remain
outside the security feature. Background service support is described below.

### Security validation (2026-10-06)

- Firmware, simulator, Android assemble/unit tests/lint, headless UI/policy tests,
  diff check, COM9 flash, Pixel APK replacement and runtime boot: PASS.
- First Numeric Comparison: PASS, user confirmed matching six-digit codes and
  confirmation on both devices. Pixel log confirms Android bonding, protected
  `0009` success, all telemetry/control reads and subscriptions, then ready.
- Bonded reconnect without pairing: PASS by user confirmation.
- Bond persistence across reboot: PASS, boot loaded trust slot 2 and the user
  confirmed secure reconnection without re-pairing. However, an initial reboot
  reconnect failed (Pixel status 147, watch encryption status 7 / disconnect 520);
  retry subsequently verified protected GATT and telemetry without a new bond.
  Do not interpret persistence PASS as perfect automatic reconnect stability.
- Final firmware avoids simultaneous peripheral Security Request / Android
  protected-read initiation; build/flash/boot PASS. Physical verification of this
  reconnect refinement is SKIPPED after the user requested no further tests.
- Unauthorized independent central, Forget phone, stale-key recovery, explicit
  Reject, secured control writes/readbacks, secured phone-notification delivery
  and extended stability: SKIPPED (user requested skipping remaining tests).
  Successful secured reads/subscriptions do not prove those interaction paths.
- Bounded serial monitors stopped cleanly; COM9 released. No commit/push.

## mobile-background-companion

### Ownership and user intent

Compose → ViewModel → CompanionConnection API → CompanionService → the existing
WatchRepository/native GATT. The app process has one repository and one WatchState
StateFlow; UI reopening observes that state, not a new independent session. BLE
commands are handled by the service, with no GATT access from Compose, ViewModel or
NotificationListenerService. Activity stop/destroy/ViewModel clear only changes whether
a new user-visible service launch is allowed; it never cancels the enabled session.

**Scan for watch** starts the service from the visible Activity and opens the existing
bounded discovery flow. Connect selects a single watch and retains it only after all
secure setup succeeds. Later **Enable companion · saved watch** directly connects to
that verified Android-bonded watch, without discovery scanning or fresh pairing.
No auto-launch on Activity reopen: after Disconnect, the app stays disconnected until
an explicit Scan/Connect/Enable action. Service creation immediately promotes itself,
before BLE work. Background listener callbacks never launch/restart a service.

**Disconnect / Disconnect · stop companion** revokes recovery immediately, closes GATT,
stops scans/timers and removes the foreground service. State distinguishes
USER_DISCONNECTED from UNEXPECTED_DISCONNECT, SECURITY_FAILURE and SERVICE_STOPPED.
Manual stop does not delete either device's bond or the saved watch selection.

### Foreground notification and permissions

Non-exported service declares `foregroundServiceType="connectedDevice"`, base
FOREGROUND_SERVICE permission and FOREGROUND_SERVICE_CONNECTED_DEVICE; existing
Nearby devices permission satisfies the runtime prerequisites. Notification channel
`watch_connection` (**Smartwatch connection**) uses IMPORTANCE_LOW, no badge/sound,
ongoing/only-alert-once, no telemetry/private content: Connected / Reconnecting… /
Connecting securely… / Attention required. Notification tap opens MainActivity.
Only status-text changes update it, not every telemetry change or chunk callback.

Optional **Allow connection notification** requests POST_NOTIFICATIONS while visible.
Denial does not deny listener access or prevent legal FGS execution, but Android can
hide the notification from the drawer and show the service only in Active apps.
The UI explains that distinction. Notification channel/app settings can also suppress
visibility; the user controls that. Own-app/service notifications remain filtered from
watch forwarding, avoiding feedback loops. No location, exact alarm, wake-lock or
battery-optimization exemption permission is added.

### Recovery and lifecycle

Unexpected transport loss/connection timeouts retry the **existing bond** with delays
0, 5, 15, 30, 60, then 120 seconds (120-second cap). One bounded 20-second attempt,
one pending retry, no overlapping GATT clients, no recovery BLE scan and no polling.
The capped cadence continues while service intent is enabled so a returning watch
can be found; it is not a tight infinite loop. Native GATT establishes each link to
the saved bonded device; successful secure setup resets backoff. Manual discovery
remains a 15-second UUID-filtered scan, explicitly user initiated.

Bluetooth OFF suspends attempts until adapter ON; permission revocation, missing bond,
explicit authentication/encryption rejection, incompatible/weak/malformed protected
status or protocol errors stop automatic recovery and show Attention required. Generic
transport loss (including status 147) may retry the existing keys, **not** replace them.
Every reconnect verifies BOND_BONDED and protected `0009`, then fresh telemetry reads/
subscriptions before CONNECTED or notification delivery. No downgrade, new background
pairing, private-content replay, or settings-write replay after an ambiguous loss.

Home, screen lock, closing the Activity and swiping its task out of recents intentionally
keep the foreground service running (`stopWithTask=false`). START_NOT_STICKY is deliberate:
OS process kill, Active apps Stop, force-stop, phone reboot, or APK replacement require
another visible Enable/Scan/Connect action. No BOOT_COMPLETED receiver, WorkManager,
alarms or background service resurrection. Android/OEM battery restrictions and Doze
can delay retry timers; indefinite availability is not guaranteed. No full production
battery/security certification, CompanionDeviceManager migration or multi-watch support.

### Physical validation checklist

1. Grant Nearby devices and connection-notification visibility; connect securely.
2. Home → lock Pixel → wait at least three minutes; service/notification must remain.
3. Receive a normal text notification while Activity is closed and screen off;
   verify app/title/body on watch and Android chunk acknowledgements.
4. Reopen the app: same connection and correct telemetry/controls, no new GATT.
5. Remove the app from recents: service remains intentionally; reopening restores UI.
6. Turn watch OFF: Reconnecting/backoff; turn it ON: secure automatic reconnect
   without new pairing, fresh reads/subscriptions and notification forwarding.
7. Disconnect: service/notification disappear; wait past the next retry interval and
   confirm no reconnect. Reopen app: still disconnected; Enable explicitly to resume.
8. Bluetooth OFF/ON, missing/stale bonds, permission revocation, notification denial
   and OS stop/force-stop: verify documented waiting/error/re-enable behavior.

### Background companion validation (2026-10-06)

- Android assembleDebug, testDebugUnitTest (9 tests, zero failures), lintDebug (no
  errors), and git diff --check: PASS. Remaining lint suggestions concern pinned
  dependency upgrades, KTX conveniences and pre-existing resource qualifiers.
- Pixel initial and final APK installs: PASS. User confirmed secure Connected and
  visible foreground-service notification. Android service dump confirmed
  isForeground=true, connectedDevice type 0x10, watch_connection / notification 1001.
- Home + Pixel screen off for three minutes: PASS on-device Android evidence.
  Process 21649 and the foreground service remained alive before/after the soak.
- Notification forwarding with Activity closed/screen off: PASS at BLE transport
  level. Notification posted after the three-minute wait was captured and all ten
  frames received status-0 write acknowledgements, including completed message.
  Evidence: background-screen-off-test.log in the approved OpenCode temporary directory.
  Visual watch presentation and UI reopening were not confirmed (user skipped).
- Watch power-off/on auto-reconnect, manual Disconnect/no retry, recents removal,
  visible state/controls after reopening and extended Doze/battery testing: SKIPPED
  at user request. Backoff/intent rules pass unit tests; that does not prove recovery
  on the physical BLE link.
- Final APK includes guarded automatic-recovery pairing rejection, saved-watch
  enabling and adapter transition handling refinements; build/install PASS, no new
  physical session after that replacement. Replacement intentionally stops the
  service; enable saved watch explicitly from the visible app to resume.
- Firmware/simulator build, COM9 flash/runtime: N/A for this Android-only feature;
  no firmware/shared UI edits or new protocol bytes. No commit or push.
