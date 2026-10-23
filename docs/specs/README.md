# Retrospective implementation specifications

This catalogue reconstructs the smartwatch project from the initial firmware to
the delivered implementation. Documents are in English, matching the root README.
Each SPEC defines observable requirements and acceptance examples; ADRs explain
durable design decisions. Cosmetic revisions are recorded within their owning
feature rather than assigned artificial architectural decisions.

## Provenance and interpretation

- Reconstruction date: **2026-10-23**. Source baseline: **`6362892`**, plus the
  current working-tree documentation. The repository is not a clean release tag.
- History begins at `5e9ae08` (2026-09-27); initial firmware is `f1c2478`.
- Commit references identify implementation milestones, not formal approval or proof
  that a feature first existed on that date. Merge dates are not acceptance dates.
- **Status: Ready for approval** refers to these newly reconstructed specifications,
  not an assertion that delivered code is unimplemented. Historical approval dates,
  owners' signatures, and independent review verdicts are not reconstructed.
- Implementation status and validation limitations are stated separately. Existing
  tests are evidence of available checks, not proof those checks passed in this task.
- Earlier technical notes sometimes describe superseded states: temporary-only AI
  login, four-credit voice windows, Timer/Stopwatch launcher entries, first-tile
  Calendar, and daily-only calendar navigation. Current code and the final README
  resolve these differences; original notes remain historical evidence.
- Deferred recorder, TTS, ADPCM and iOS support are exclusions, not completed features.
  Google synchronization is implemented but real-account validation is conditional.
- This documentation task does not build, flash, deploy, change cloud resources,
  authorize accounts, consume paid AI, commit, or modify application behavior.

## Catalogue and implementation milestones

| SPEC | Delivered area and historical evolution | Representative commits | ADR |
| --- | --- | --- | --- |
| [01 Platform](platform/SPEC.md) | Board bring-up and peripheral ownership | `f1c2478`, `364ace2`, `5c6aecd` | [0001](../adr/0001-platform-and-shared-ui.md) |
| [02 Timekeeping](timekeeping/SPEC.md) | Clock, NTP, RTC, local dates | `4006fc8`, `9611056`, `efb73ac` | [0002](../adr/0002-wall-clock-and-monotonic-time.md) |
| [03 Display and wake](display-wake/SPEC.md) | Timeout, wake gating, assistant shortcut | `e94ebbd`, `3e1a9e3`, `4ea7fbe` | [0015](../adr/0015-display-lifecycle-and-bounded-rendering.md) |
| [04 Settings and power](settings-power/SPEC.md) | Brightness, persistence, battery, Hardware Info | `1709a0d`, `500bad4`, `b628621` | [0013](../adr/0013-local-settings-and-power-ownership.md) |
| [05 Wi-Fi](wifi/SPEC.md) | Connection intent and recovery | `9611056`, `ec624ec` | [0005](../adr/0005-platform-owned-network-workers.md) |
| [06 Shared UI](shared-ui/SPEC.md) | Simulator, navigation, fonts and final launcher | `210467b`, `c3bdcb3`, `2b4519e`, `634347f` | [0001](../adr/0001-platform-and-shared-ui.md), [0015](../adr/0015-display-lifecycle-and-bounded-rendering.md) |
| [07 Steps/history](steps-history/SPEC.md) | Sensor, detection, rollover, goal and activity UI | `0176d9a`, `d0d336f`, `a1fb9ab` | [0006](../adr/0006-watch-owned-dated-activity.md) |
| [08 Secure BLE](secure-ble/SPEC.md) | Telemetry, pairing, encryption, controls | `7447c20`, `2d6cb9e`, `7740474` | [0003](../adr/0003-native-ble-security-and-additive-contracts.md) |
| [09 Android companion](android-companion/SPEC.md) | Connection service, readiness and recovery | `2d6cb9e`, `2c8dbd7`, `e01b6f6` | [0004](../adr/0004-single-companion-and-serialized-gatt.md) |
| [10 Notifications](notifications/SPEC.md) | Permissioned text forwarding | `2c8dbd7` | [0004](../adr/0004-single-companion-and-serialized-gatt.md) |
| [11 Media](media/SPEC.md) | Metadata, playback and phone controls | `2c8dbd7` | [0004](../adr/0004-single-companion-and-serialized-gatt.md) |
| [12 Original time tools](time-tools/SPEC.md) | Adjustable Timer/Stopwatch, later launcher replacements | `c3bdcb3`, `47f2ec3`, `b59db73` | [0002](../adr/0002-wall-clock-and-monotonic-time.md) |
| [13 2048](game-2048/SPEC.md) | Deterministic engine, gestures and saved best | `e6e5b91` | [0014](../adr/0014-pure-game-engine-and-platform-storage.md) |
| [14 Health Connect](health-connect/SPEC.md) | Optional, dated step export | `5c17bbf` | [0006](../adr/0006-watch-owned-dated-activity.md) |
| [15 Mobile dashboard](mobile-activity/SPEC.md) | Watch-owned activity visualization | `066a34c` | [0006](../adr/0006-watch-owned-dated-activity.md) |
| [16 Voice backend](voice-backend/SPEC.md) | Authenticated API, quotas, Spanish models | `2ed4821` | [0007](../adr/0007-authenticated-cloud-voice-and-quotas.md) |
| [17 Voice capture](voice-capture/SPEC.md) | Bounded microphone/audio pipeline | `a3f6af4` | [0008](../adr/0008-lossless-pcm-and-credit-window-transport.md) |
| [18 Voice transport](voice-transport/SPEC.md) | Pull, notification windows, CRC and profiling | `a3f6af4`, `7740474` | [0008](../adr/0008-lossless-pcm-and-credit-window-transport.md) |
| [19 AI session/consent](ai-session/SPEC.md) | Encrypted restoration and paid-request gates | `e01b6f6` | [0009](../adr/0009-encrypted-session-and-generation-consent.md) |
| [20 Mochi UI](mochi-ui/SPEC.md) | Assistant states, Spanish text, fixed face geometry | `548c9ae`, `8aead05`, `b60da6e` | [0015](../adr/0015-display-lifecycle-and-bounded-rendering.md) |
| [21 Sun & Moon](sun-moon/SPEC.md) | Offline Madrid estimates | `9d9b549` | [0012](../adr/0012-offline-schematic-astronomy.md) |
| [22 Planets](planets/SPEC.md) | Bounded approximate heliocentric display | `28cba95`, `a611f89` | [0012](../adr/0012-offline-schematic-astronomy.md) |
| [23 Pomodoro](pomodoro/SPEC.md) | Manual focus/break cycle | `47f2ec3` | [0002](../adr/0002-wall-clock-and-monotonic-time.md) |
| [24 Local audio settings](audio-settings/SPEC.md) | Microphone gain and local speaker preview | `c2f966f` | [0013](../adr/0013-local-settings-and-power-ownership.md) |
| [25 Battery Saver](battery-saver/SPEC.md) | Advisory settings, not automatic power control | `c2f966f` and subsequent delivered UI | [0013](../adr/0013-local-settings-and-power-ownership.md) |
| [26 Motion Lab](motion-lab/SPEC.md) | Cube, level and acceleration history | `b59db73` | [0015](../adr/0015-display-lifecycle-and-bounded-rendering.md) |
| [27 Weather](weather/SPEC.md) | Current weather, hourly data, daily MAX/MIN dashboard | `ec624ec`, `42f1ea4`, `abb5253` | [0005](../adr/0005-platform-owned-network-workers.md) |
| [28 Calendar](calendar/SPEC.md) | Local provider, PSRAM protocol/cache, month and weekly UI | `634347f` | [0010](../adr/0010-bounded-calendar-snapshots-in-psram.md) |
| [29 Google agenda](google-agenda/SPEC.md) | OAuth, birthdays and dated Tasks | `634347f`; existing Google integration sources | [0011](../adr/0011-optional-read-only-google-agenda.md) |
| [30 Validation workflow](validation-workflow/SPEC.md) | Host tests, physical checks and delivery evidence | `0987b90`, `6199767`, `1558a18` | No separate product ADR |
| [31 Performance overlay](performance-overlay/SPEC.md) | Shared FPS, UI busy-time and render/flush diagnostics | Delivered `watch_perf_monitor.h` | [0015](../adr/0015-display-lifecycle-and-bounded-rendering.md) |

## Shared verification commands

Commands are reference procedures, **not executed evidence for this reconstruction**.
Run only those appropriate to a future code change, with the required environment.

| Key | Directory | Command | Prerequisites / limits |
| --- | --- | --- | --- |
| FW | `apps/watch` | `idf.py build` | ESP-IDF 6.1 environment |
| FLASH | `apps/watch` | `idf.py -p COM9 flash` | Connected watch; never automatically erase flash |
| BOOT | `apps/watch` | `idf.py -p COM9 monitor` | Capture bounded logs; stop monitor cleanly |
| SIM | `apps/watch-simulator` | `cmake --build build --config Debug --target main watch_ui_tools_tests` | Configured MSVC/SDL2/LVGL build |
| UI | `apps/watch-simulator` | `.\bin\Debug\watch_ui_tools_tests.exe` | Debug host executable |
| ANDROID | `apps/mobile` | `.\gradlew.bat :app:assembleDebug :app:testDebugUnitTest -PvoiceNotify=true` | JDK 17 / SDK 36 |
| TRANSFER | `apps/mobile` | `.\gradlew.bat :app:assembleDebug -PvoiceNotify=true -PvoiceTransferOnly=true` | Blocks voice HTTP; not a benchmark by itself |
| API | `apps/voice-backend` | `python -m pytest` | Virtual environment with `requirements-dev.txt`; mocked services |
| DIFF | repository root | `git diff --check` | Formatting only |

Use the root [README](../../README.md) for setup and the linked feature notes for
manual procedures. Historical PASS statements in the project conversation do not
replace rerunning checks against a newly modified binary. No cloud or physical
checks are required merely to create these retrospective Markdown records.

## Reconstruction validation record

On 2026-10-23, a local read-only Python consistency check completed with exit 0:

- 31 feature SPECs and 15 numbered ADRs present.
- 126 requirement IDs mapped to 64 acceptance scenarios (slash shorthand such as
  `CAL-01/02` means both `CAL-01` and `CAL-02`).
- Every SPEC contains sections 1–11, separating requirements, implementation and evidence.
- Local Markdown links/heading anchors resolve; referenced Git commits exist.
- Code fences are balanced and newly documented files have no trailing whitespace.
- Repository-root `git diff --check` exited 0; existing LF/CRLF notices remain warnings.
- Existing `TEMPLATE.md` files were not edited. No implementation/configuration files
  were changed by this task; the root README only gained catalogue links.

These are **documentation checks**, not acceptance execution for the 31 product
areas. Firmware/simulator/mobile/backend builds, hardware tests and cloud checks
were not rerun for this documentation-only task. Independent review and formal
retrospective ratification have not been performed.
