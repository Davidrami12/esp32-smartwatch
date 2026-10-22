# Motion Lab

Replaces Stopwatch in Apps. No phone, SD card, microphone, cloud or AI required.

- **Cube:** colored wireframe follows smoothed gravity-derived roll and pitch.
- **Level:** a bubble and crosshair; green within roughly 2 degrees of the
  user-set reference. Place the watch still, then use **Set level / recenter**.
- **Accel:** rolling X/Y/Z history, roughly 3.2 seconds at 25 Hz, in g. The
  display range is ±4 g and includes gravity (about 1 g total at rest).

Uses the existing QMI8658 accelerometer stream without changing its configuration,
enabling the gyroscope or interfering with step detection. Accelerometer tilt
is not compass heading or full orientation tracking; shaking disturbs tilt.
Recenter is session-only and does not calibrate the sensor or change steps.

Stale/missing readings show sensor unavailable; no synthetic data on hardware.
Desktop explicitly enables a labeled simulated-motion demo. Rendering is bounded
to 25 Hz while the app is visible and the display active, paused elsewhere.
History is an 80-sample RAM ring, not a persistent recording.

Layout reserves separate bands for tabs, status, visualization, readings and
actions, accounting for the shared 56 px minimum button height. All modes leave
Back unobstructed. Accel hides the inapplicable recenter action and shows its
range/gravity note instead. Recenter briefly confirms "Reference set".

Manual checks: slowly tilt each axis, test upside-down transition, recenter on
a flat surface, inspect bubble movement, shake gently to inspect traces; verify
Back, screen sleep/wake, step counting and notification interruption/recovery.
