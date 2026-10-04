# Android companion — BLE foundation and settings controls

Kotlin / Compose / native Android BLE, designed for the Google Pixel 10 Pro.
No firmware changes. `../../docs/ble-protocol.md` remains the protocol authority.

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
Only Nearby devices (`BLUETOOTH_SCAN`, `BLUETOOTH_CONNECT`) permissions are requested.
Scanning is declared `neverForLocation`; no location permission or tracking.
No Internet permission, storage/database, cloud, DI framework or background service.

## Structure

`app/src/main/java/dev/esp32/smartwatch/`:

- `MainActivity.kt`: dark Compose discovery/dashboard, permission launcher,
  Android app-settings link and lifecycle observation.
- `WatchViewModel.kt`: lifecycle-facing actions and StateFlow, owns repository.
- `WatchState.kt`: application state/nearby device records; no raw GATT objects.
- `ble/WatchRepository.kt`: single main-looper BLE session, scan/connection,
  serial operation queue, timeouts, reads, subscriptions and write/readback.
- `ble/WatchBleProtocol.kt`: centralized UUIDs and strict LE snapshot decoders.
- Unit tests under `app/src/test/`: documented values, unsigned uint32 maximum,
  zero/decreased steps, invalid lengths/ranges and unknown Wi-Fi states.

## Connection contract

DISCONNECTED → SCANNING (15-second timeout) → manual selection → CONNECTING
(20-second timeout) → DISCOVERING_SERVICES (15-second discovery timeout) →
verify services/read/notify/CCCD properties → read version first and require v1 →
read telemetry and optional controls → subscribe to discovered values → CONNECTED.
Each queued operation has a 10-second timeout; only one is outstanding.
Errors release GATT and show actionable UI text. Cancel/disconnect releases GATT,
clears displayed measurements and suppresses automatic reconnect.
Stale callbacks from prior GATT or scan sessions are ignored.

Battery ATT 0x0E is the documented temporary PMIC-unavailable case, not a fatal
protocol error. Other initial read/subscription failures stop setup. Malformed
notifications are ignored with a visible warning; unknown Wi-Fi values display
Unknown, never Connected. Steps replace the complete count, including decreases.

The optional additive v1 settings group has Brightness, Screen Timeout and Wi-Fi
Enabled (intent) UUIDs, centralized alongside telemetry in `WatchBleProtocol.kt`.
Older firmware remains telemetry-compatible, but shows Controls unavailable.
All controls are read/write-with-response/notify and expose actual initial values.
Brightness writes once on slider release, never on each drag tick. The preview is
explicitly labeled and the confirmed percentage remains shown separately.
Timeout chips and Wi-Fi switch do not optimistically change the confirmed state.
All controls are disabled until the current write callback and readback finish.
Rejected writes trigger actual-value readback and a small error, not a fatal
telemetry disconnect. Failed readback disables controls until reconnect. Lost
callbacks/timeouts close the ambiguous GATT session and require fresh reads.
Phone settings are not persisted. See `../../docs/ble-protocol.md` for wire values,
validation/ATT errors, firmware persistence semantics and security limitations.
The standard Service Changed indication is subscribed/handled to invalidate old
Android GATT caches after firmware updates; no hidden refresh APIs are used.

Native scans filter by advertised Watch Service UUID, not name. Results are
deduplicated, signal sorted and selectable; address is an opaque session key,
not authentication. No bonding/encryption is initiated, as firmware v1 requires
neither. **Any nearby central can read or change these development controls.**

Notifications replace values immediately via StateFlow; there is no polling.
The watch itself limits steps notifications; small changes can take 15 seconds.
An unexpected transport disconnect schedules up to three reconnect attempts
(2/4/6 seconds) only while the Activity is started. Each attempt rediscoveres,
reads and subscribes. Setup errors/timeouts require a manual retry. Scanning and
pending retries stop when the Activity stops. An existing connection may remain
while the process lives, but has no service/process survival guarantee. Returning
to the Activity can resume a pending disconnected session; process death/reboot
does not persist identity or reconnect. ViewModel clear closes the session.

Development logs include statuses and operation UUIDs, not device names,
addresses, payloads, credentials or account data. Permissions are checked before
operations and revocation is caught. Enable Bluetooth manually if disabled.

## Physical validation checklist

1. Enable USB debugging and authorize the workstation; install on Pixel.
2. First scan requests Nearby devices; deny and verify explicit messaging and
   app-settings recovery. Grant and rescan. Test permission revocation in settings.
3. Enable watch Bluetooth; close nRF Connect/other central sessions (watch supports
   one central). Verify only compatible watches appear, timeout and stop scan.
   If you previously paired ESP32 Smartwatch in Android Bluetooth settings, forget
   that entry and connect from this app without OS pairing. The current firmware
   does not retain bond keys; a stale phone key can cause repeated 30-second drops.
4. Connect: wait through connection/discovery. Compare version v1, battery,
   today's steps and Wi-Fi with the watch. Battery unavailable must not crash.
5. Walk >=5 steps; wait for notification. Verify a trailing small delta within
   15 seconds; Wi-Fi toggles update and battery changes update without polling.
6. Manual Disconnect: watch resumes advertising; connect again with fresh reads.
7. Disable watch BLE/power-cycle/out-of-range: no crash or stale connected UI;
   foreground bounded reconnect works when advertising returns. Manual scan
   recovers after exhausted attempts. No retries while Activity is backgrounded.
8. Screen rotation / app background / return: no duplicate sessions or scans.
9. Watch IDLE/double-tap and Wi-Fi/weather/NVS saves remain functional while connected
   with this unbonded GATT connection. Pairing/security is not validated by this.

No phone notification forwarding, provisioning, history sync, OTA, accounts,
Health Connect, weather sync or permanent background operation is implemented.
Settings controls are the only remote commands added. The simulator has no BLE
emulation but still builds the shared UI unchanged.

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
- Security remains development-only unpaired read/write access, not production
  authorization. Existing flash-size, BLE light-sleep and LVGL warnings remain;
  Android lint only suggests dependency upgrades / a KTX convenience method.
