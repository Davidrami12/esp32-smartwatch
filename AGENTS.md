# Repository notes

- Two separate builds live here: `apps/watch` is ESP-IDF firmware for the Waveshare ESP32-S3 Touch AMOLED 2.06; `apps/watch-simulator` is a desktop CMake/SDL2 LVGL build. Shared UI implementation is `components/watch-ui/` and is compiled into both.
- Firmware commands must run from `apps/watch` in an ESP-IDF environment: `idf.py build`; flash/monitor with `idf.py -p <PORT> flash monitor`. `apps/watch/CMakeLists.txt` reads optional `apps/watch/.env` and injects `WIFI_SSID`/`WIFI_PASSWORD` compile definitions; don't commit credentials.
- Simulator requires SDL2 and CMake. On Windows, use the Visual Studio 2019 generator and vcpkg toolchain (see `apps/watch-simulator/README.md`); configure with CMake, then build Debug. `USE_FREERTOS` defaults OFF; FreeRTOS simulation is currently unsupported because its source tree is not present.
- Simulator CMake expects LVGL at `apps/watch-simulator/lvgl` via `add_subdirectory(lvgl)`; it also builds shared UI from `../../components/watch-ui/watch_ui.cpp`. `run` is a CMake target for launching the built `main` executable.
- Firmware registers `main/main.cpp` and shared UI in `apps/watch/main/CMakeLists.txt`; LVGL demos are recursively globbed from the managed LVGL component. Treat `apps/watch/managed_components/` as dependency-managed, not the place for app changes.

## Feature completion workflow

When finishing an implementation feature, do not stop after compilation. Unless the task explicitly says otherwise, use this workflow:

1. Build the firmware from `apps/watch` in an ESP-IDF environment:
   ```sh
   idf.py build
   ```
2. Build the desktop simulator when the change can affect shared UI or simulator compatibility.
3. Run from the repository root:
   ```sh
   git diff --check
   ```
4. If the ESP32-S3 watch is connected, flash it automatically from `apps/watch`. The default development port is **COM9**:
   ```sh
   idf.py -p COM9 flash
   ```
5. After flashing, start the serial monitor from `apps/watch` when runtime validation is useful:
   ```sh
   idf.py -p COM9 monitor
   ```
6. Inspect startup/runtime logs for crashes, initialization failures, warnings related to the implemented feature, and regressions in existing functionality.
7. Do not treat successful compilation as sufficient validation when the physical device is available.
8. If COM9 or the device is unavailable, do not fail the implementation solely because flashing could not be performed. Report physical validation as **SKIPPED** and explain why.
9. Never erase flash automatically unless explicitly required.
10. Never commit or push unless explicitly requested.
11. After flashing, clearly report manual physical interaction still required from the user, such as touchscreen interaction, double tap, walking steps, power cycling, or visual UI inspection.

At the end of each feature, report:

- Firmware build: **PASS / FAIL**
- Simulator build: **PASS / FAIL / N/A**
- `git diff --check`: **PASS / FAIL**
- Device flash: **PASS / FAIL / SKIPPED**
- Runtime boot validation: **PASS / FAIL / SKIPPED**
- Manual physical tests still required
- Files changed
- Remaining warnings or issues
