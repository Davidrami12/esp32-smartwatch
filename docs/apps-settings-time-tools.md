# Apps / Settings and local time tools

Home has separate APPS and SETTINGS actions. Apps contains Activity, Timer and Stopwatch;
no speculative extra app is added. Settings contains Wi-Fi, Bluetooth, Brightness, and
Screen timeout. Each settings detail returns to Settings; app details return to Apps;
both menus return Home. Right-swipe mirrors Back. Home left-swipe still opens Apps.

Brightness -/+ are 48 x 56px buttons with consistent pressed feedback. Each click changes
by 5 percentage points, clamped to the existing 10–100% range. Existing hardware-update
and commit callbacks are called once per effective change; reaching a bound does not write
again. Slider drag/release and timeout persistence keep their existing callbacks.

Timer: 1–120 minutes, default 5; adjust in one-minute steps only before starting or after
Reset. Start/Pause/Resume, Reset, and Restart after completion. Countdown rounds remaining
seconds upward. Completion shows TIME'S UP at zero; visual only, no audio, vibration or
automatic wake. An off-screen/IDLE expiry remains visible when returning to the timer.
Stopwatch: hours/minutes/seconds/tenths, Start/Pause/Resume and Reset. Both can run at once.
Both are RAM-only: reboot clears them; no NVS writes or wall-clock/SNTP dependency.

Portable monotonic arithmetic in `watch_time_tools.h` accumulates unsigned LVGL tick deltas
into uint64 milliseconds. A lightweight 100ms UI timer advances state while IDLE without
redrawing; firmware supplies display activity through `watch_ui_set_display_active`.
Timer drawing refreshes at second granularity, stopwatch at tenths only while visible and
active. No new sleep, radio, BLE, motion, battery or network behavior. LVGL ticks must keep
advancing (as they do in current display-only IDLE); true deep sleep would need platform time.

Validation: `tests/time_tools_host.cpp` tests monotonic start/pause/resume/reset and tick wrap.
`tests/time_tools_ui_host.cpp` tests the actual shared LVGL UI headlessly: brightness
callbacks/bounds, menu routes, one-minute expiry, concurrent stopwatch, and IDLE/wake.
Build explicitly using simulator CMake target `watch_ui_tools_tests`, then run the Debug
executable. It is excluded from the normal build and never linked into firmware.
Manual: all navigation routes, +/- bounds and reboot restore, slider save, timeout save,
one-minute expiry, timer/stopwatch across navigation and IDLE/wake, simultaneous runs.
Simulator supports the same controls; its screen remains active by default.
The extra screens increase UI heap usage. Firmware reserves 64KiB (previously 32KiB)
of internal RAM for DMA/internal-only allocations and prefers PSRAM for ordinary malloc
allocations (threshold now zero; originally 16KiB), preserving larger contiguous DMA blocks
for the LCD SPI bounce buffer. Increasing the reserve alone was insufficient in runtime
testing. No dependency-managed source is modified.
