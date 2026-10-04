# Home context and daily step goal

Home's weather card uses the date-matched **today** forecast slot to show H / L in
Celsius below the current temperature. Unavailable daily data shows H -- / L --;
offline cached forecasts use the existing Weather screen's age/offline indicator.
The weather icon is inset so it stays inside its card.

The steps card adds a three-pixel orange progress bar and Goal 8,000 by default.
Progress is actual steps / goal, saturated at 100%; the count is never truncated
to the goal. Daily count rollover also resets the displayed progress.

Activity has - / + goal controls (1,000 increments; bounds 1,000–30,000) above the
seven-day history. Bounds disable the corresponding button. Firmware saves each
effective change under step_goal in the existing settings NVS namespace. Invalid
or missing saved values fall back to 8,000. A failed save leaves the prior goal
unchanged and shows Save failed / retry. Shared UI has no NVS dependency; simulator
goal edits are RAM-only. Goal changes do not alter step history or BLE counts.

Home's boxed Settings/Apps buttons retain equal 160x56 touch areas, the same
positions and directional transitions, matching 16px white icon/text labels,
and gray fill / bright border press feedback. No chevrons are shown.
Weather icons share condition-based colors across Home, forecast and the Apps
tile: sunny yellow, rain light blue, cloudy light gray, storm lavender and wind
pale mint. The Apps tile remains sunny yellow because its icon depicts a sun.
Temperature remains white and daily high/low muted.

Headless UI tests cover progress saturation/zero/large counts, bounds, +/- callback
results, save failure, invalid goals and available/unavailable daily high/low.
Physical checks: card legibility, Activity/history fit, touch controls, saved goal
after reboot, progress after walking, weather/offline display and existing wake.
