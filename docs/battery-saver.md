# Battery Saver settings

Settings → Battery is a sixth tile in a staggered six-tile layout: two centered
at the top, two spread to the sides in the middle, and two centered at the bottom.
All six tiles are 108 × 108 pixel squares.
It opens a scrollable set of suggestions based on current settings, not a
power-consumption profiler or an automatic low-power mode.

- Battery percentage and charging state come from the existing battery API.
  Unknown values stay unknown; 20% or less gets a low-battery warning.
- Brightness: suggests 30% or lower indoors when currently above 30%.
- Screen timeout: suggests 5/10 seconds when long or Never is selected.
- Wi-Fi: suggests disabling it when unused and explains clock/weather impact.
- Bluetooth: explains this build's light-sleep limitation and the loss of phone,
  media and assistant connections when Bluetooth is disabled.

Tap a suggestion card to open the existing control, then Back returns to the
suggestions. All changes are explicit and use the existing setting callbacks;
there is no automatic disconnection, brightness override or new persistence.
Updates use existing events, with no new polling timer or asleep UI redraws.

No remaining-hours estimates, per-app drain percentages or measured savings are
claimed. Notifications and voice navigation retain their existing behavior.
