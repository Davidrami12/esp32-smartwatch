# Repository notes

- Two separate builds live here: `apps/watch` is ESP-IDF firmware for the Waveshare ESP32-S3 Touch AMOLED 2.06; `apps/watch-simulator` is a desktop CMake/SDL2 LVGL build. Shared UI implementation is `components/watch-ui/` and is compiled into both.
- Firmware commands must run from `apps/watch` in an ESP-IDF environment: `idf.py build`; flash/monitor with `idf.py -p <PORT> flash monitor`. `apps/watch/CMakeLists.txt` reads optional `apps/watch/.env` and injects `WIFI_SSID`/`WIFI_PASSWORD` compile definitions; don't commit credentials.
- Simulator requires SDL2 and CMake. On Windows, use the Visual Studio 2019 generator and vcpkg toolchain (see `apps/watch-simulator/README.md`); configure with CMake, then build Debug. `USE_FREERTOS` defaults OFF; FreeRTOS simulation is currently unsupported because its source tree is not present.
- Simulator CMake expects LVGL at `apps/watch-simulator/lvgl` via `add_subdirectory(lvgl)`; it also builds shared UI from `../../components/watch-ui/watch_ui.cpp`. `run` is a CMake target for launching the built `main` executable.
- Firmware registers `main/main.cpp` and shared UI in `apps/watch/main/CMakeLists.txt`; LVGL demos are recursively globbed from the managed LVGL component. Treat `apps/watch/managed_components/` as dependency-managed, not the place for app changes.
