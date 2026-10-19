# Sun & Moon

Open **APPS → SUN & MOON**. SUN and MOON select the view; arrows preview
today through six days ahead. BACK returns to APPS.

- Fixed location: Madrid, 40.4168° N, 3.7038° W.
- Uses platform UTC, with Madrid local time calculated using current EU DST
  rules, regardless of the simulator computer's timezone.
- All astronomy runs locally; no location permissions, network requests or
  paid services. Clock synchronization uses the watch's existing facilities.
- SUN shows estimated rise/set, daylight duration, solar noon and the next
  rise/set countdown. Its dome is a schematic daytime progress indicator,
  not a calibrated altitude/azimuth sky map. Preview dates have no live marker.
- MOON shows a northern-hemisphere phase diagram, illumination, rounded-up
  days until full moon, and next new-moon date. Mean synodic-month estimates
  can differ from actual lunar events by about a day; not for precise planning.
- Solar calculations use NOAA fractional-year equations with event refinement.
  Tests check seasonal plausibility, not a certified ±2-minute error bound.
- Refreshes on entry, interaction and clock-minute changes while visible and
  awake. No continuous animation or separate astronomy timer.
- Location is not configurable yet. EU timezone-rule changes would require an
  update. The supported clock range is 2000–2099.

Inspired by and adapted from app-pixels/sun-moon v1.0.0:
https://github.com/app-pixels/sun-moon/tree/v1.0.0
MIT attribution: `components/watch-ui/licenses/sun-moon-MIT.txt`.

Regression targets: `watch_astronomy_tests` and `watch_ui_tools_tests` in the
desktop simulator CMake project (Debug configuration).
