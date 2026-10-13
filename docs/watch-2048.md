# watch-2048-game

## Shared game and controls

`components/watch-ui/watch_2048.{h,cpp}` is the production game for firmware and
simulator. It has no LVGL, ESP-IDF, storage, external game library or heap allocation:
16 fixed uint32 tiles plus score/best, RNG and lifecycle flags. Each move compacts a
line toward the chosen edge and consumes equal original pairs once. Merged values
increase score; successful moves spawn one tile (approximately 90% 2 / 10% 4).
Invalid moves do not advance RNG, score or tile spawning. Fixed-seed xorshift makes
tests deterministic; firmware seeds from `esp_random`, simulator from system time.

Win is acknowledged once at 2048, with Continue/New Game. Full boards without
empty cells or adjacent mergeable pairs are game over. New Game keeps best and
resets only the RAM game; progress requires confirmation. Scores saturate at uint32
max; tiles cannot overflow beyond 2^31. This safety bound is well beyond normal play.

Apps has a sixth 2048 tile with an original procedural grid icon. UI is a 306 px
board of 72 px tiles with 6 px gutters, score/best above and 56 px New Game/Back
targets below. Tile value colors use subdued surfaces and colored borders/text,
with smaller typography for larger values. Dialogs shield underlying controls.

The existing gesture handler explicitly routes all four directions to the game
while its screen is active, **before** normal navigation. Board/tiles do not scroll;
gesture bubbling reaches that one handler and `lv_indev_wait_release` prevents
multiple moves/clicks per swipe. Right swipe does not navigate away. Use explicit
Back to Apps, then Home. Dialog gestures do not move the board or navigate.

Simulator: mouse press/drag/release provides the same LVGL swipes. No keyboard hook
or firmware input behavior was introduced. Desktop best stays process-local; only
the firmware platform provides NVS persistence.

## Power, notifications and persistence

Board/current score remain in RAM across navigation, IDLE/wake and notification
interruption. The existing notification return-screen mechanism restores the game
including any confirmation/win/game-over dialog. No game timer/redraw occurs during
IDLE; existing touch activity, power timeout, RTC, steps and BLE are unchanged.
Reboot resets current game; it does not reset best.

`apps/watch/main/watch_2048_store.{h,cpp}` uses NVS namespace **watch_2048**, key
**best_score**, type uint32. Missing/unreadable/type-mismatched values default to zero;
non-multiple-of-four values are invalid, except the saturated uint32 max score.
No partition/key erase or automatic recovery erase is performed by this feature.

Best improves only when score exceeds it. Changes queue in RAM, with at most one
periodic save attempt per 15 seconds, and immediate flush on leaving the game,
New Game, display IDLE, win or game over. Unchanged/lower values do not write. Failed
open/set/commit retains pending best for retry, logs a warning and does not crash the
game. Failed lifecycle saves mark BEST with `*` until a successful callback. Abrupt
power loss before a flush can lose recent queued improvements; leave the game or
wait for the batch before power-cycling. Storage writes run on the existing UI task.

## Automated validation

- `watch_2048_tests`: all directions, 2+2, four-tile pairs, no double merge, score,
  valid/no-op spawn, win/continue, game over, best/reset, RNG and overflow.
- `watch_2048_store_tests`: actual firmware adapter with NVS mocks; missing/invalid
  data, batching, unchanged/lower saves, reload and open/set/commit failure retries.
- Existing headless UI test includes production game input routing, new-game cancel/
  confirmation, win/over dialogs, explicit Back/Home, notification restoration and IDLE.

From `apps/watch-simulator`:

```powershell
cmake --build build --config Debug --target main watch_ui_tools_tests watch_2048_tests watch_2048_store_tests
.\bin\Debug\watch_2048_tests.exe
.\bin\Debug\watch_2048_store_tests.exe
.\bin\Debug\watch_ui_tools_tests.exe
```

## Physical checklist

Pending physical confirmation: all four touch swipes and no normal right-swipe
navigation, merges/score, New Game cancel/confirm and best retention, Back/Home,
timeout/double-tap wake restoring board, incoming notification/dismiss restoring board,
and saved BEST after reboot. Win/continue and game over have automated fixture coverage;
real touchscreen reachability/legibility are still manual checks.

## Validation results

- Current branch retained: `feature/mobile-android-foundation`; no new branch,
  commit or push.
- Firmware build, simulator build, production game unit tests, storage-adapter mock
  tests and shared UI regression tests: PASS.
- `git diff --check`, COM9 flash and final bounded runtime boot: PASS. Best-score
  namespace safely defaulted to zero on first startup. Serial monitor stopped;
  COM9 released.
- User confirmed full initial gameplay: board/UI, four swipe directions, merges,
  scoring, New Game and Back/no unintended navigation: PASS (manual confirmation).
- Timeout/wake, notification restoration and best surviving physical reboot:
  SKIPPED at user's request. Automated fixtures cover the first two and mocked
  storage reload; they are not substitutes for those real-device interactions.
- Existing flash-size mismatch (32 MB detected / 16 MB configured), BLE light-sleep
  limitation, LVGL deprecation/sanity and simulator narrowing warnings remain.

### Startup blocker discovered during physical validation

Initial firmware flashed but repeatedly hit a TLSF heap assertion during display
startup, before creating the game. Callback-registration locking alone did not
resolve it. The bundled BSP calls the RGB display-registration path for a QSPI
SH8601 AMOLED; RGB callback setup interprets the SPI panel as an RGB panel and can
overwrite adjacent heap data. `apps/watch/main/watch_display_start.{h,cpp}` provides
an app-side linker adapter to use `lvgl_port_add_disp` (SPI/IO completion path), and
holds the recursive port mutex until BSP display/input startup is finished.
Dependency-managed sources are untouched. The corrected firmware booted without
heap assertions, and physical gameplay passed. Wider existing-screen/display and
long-duration regression checks remain manual; this is not a claim of exhaustive
display-driver validation.
