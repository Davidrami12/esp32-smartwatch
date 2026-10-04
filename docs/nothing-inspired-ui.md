# Monochrome AMOLED visual system

An independent technical/industrial design, not a Nothing OS reproduction. No Nothing
logos, trademarks, font files or proprietary icon assets are embedded.

`components/watch-ui/watch_design.h` centralizes grayscale palette, state colors, spacing,
surface/border/radius, touch height, typography roles, icons, animation and major layouts.
Black backgrounds and nearly black outlined surfaces avoid large luminous fills. A muted
red is reserved for battery <=20%; connected/available is white, inactive is gray.
Press feedback is grayscale, navigation is directional and 160 ms.
Category accents: Bluetooth blue (`#67A9FF`), Activity/WALK orange (`#FFAA55`),
Wi-Fi mint (`#74DCC8`), Settings gray. Active connectivity statuses use their category
color; disabled/disconnected states remain gray. Surfaces and weather stay monochrome.

## Typography and assets

Normal labels reuse LVGL's existing Montserrat at 14/18/20/26 px; geometric large counters
reuse 48 px. Primary Montserrat is SIL OFL 1.1, permitting embedding/redistribution:
https://github.com/JulietaUla/Montserrat/blob/master/OFL.txt (license checked during implementation).
No new font binary or extra glyph ranges are bundled. Technical labels use uppercase and
tracked headings. The clock uses original 5x7 numeric dot patterns rendered with LVGL
rounded rectangles (digits, colon, unavailable dash); this is procedural code, not a
third-party font. Its pattern table is only 70 bytes. No bitmap atlas, seconds animation,
or independent timer is required. Only a changed HH:MM invalidates the clock.

Existing LVGL symbols cover Wi-Fi, Bluetooth, battery, settings, back and menu; these
ship with the existing LVGL font assets. Activity uses procedural outlined footprints;
weather uses the existing compact geometric drawing/mapping in monochrome. No asset pipeline.

## Layouts (410 x 502)

- Home: 18px Wi-Fi/BT/battery top row; dominant 310 x 84 dotted clock; uppercase
  weekday/date; compact orange WALK/count row; full-width MADRID weather panel with
  temperature and icon; APPS action. No separate weather-category footer.
  Top Wi-Fi ON/LINK/OFF means connected/connecting/disconnected; Bluetooth LINK/READY/OFF
  means connected/available/disabled. Detail screens retain explicit status words.
- Apps: uniform 2-column outlined tiles for Activity, Timer and Stopwatch. Home also has
  a separate Settings action with Wi-Fi/Bluetooth/Brightness/Timeout tiles.
  ChatGPT Usage was removed by the user before this feature and is not restored.
- Activity: TODAY large grouped step count, then all seven existing local-calendar records
  (today included) in compact weekday/count rows. No extra history or charts invented.
- Settings details: DISPLAY/Brightness and SCREEN/Timeout sections; existing callbacks,
  persistence, slider range and dropdown options remain unchanged.
- Wi-Fi/Bluetooth: monochrome state, network/device name, signal and outlined action/back.
  Network name remains connected-only. BLE behavior is unchanged.

Simulator mocks: 5,421 steps, seven calendar rows including missing-day zeroes, Madrid 24°C,
sunny, 82% battery; existing Wi-Fi/BT controls exercise states. No removed usage mock is added.
Manual visual checks: clock at 00:00/11:11/23:59/unavailable, longer counts, weather categories,
charging/low battery, inactive/connecting/connected states, rounded-edge clearance, touch
targets, dropdown list, history rows, navigation and unchanged wake/timeout behavior.
