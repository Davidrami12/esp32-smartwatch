# Motion Lab cube, level and acceleration traces

- **Status:** Ready for approval — retrospective; implemented.
- **Owner:** Project maintainer; historical formal approval unavailable.
- **Last updated:** 2026-10-23.
- **Sources:** `b59db73`; `watch_motion.*`, shared Motion Lab UI, `docs/motion-lab.md`.
- **Related ADR:** [0015](../../adr/0015-display-lifecycle-and-bounded-rendering.md).

## 1. Goal and context
Replace the Stopwatch launcher entry with simple interactive motion visualizations
without adding a second sensor stream or disturbing step detection.

## 2. Scope
Gravity-derived Cube, recenterable Level and Accel history. Exclude gyroscope enablement,
compass heading, persistent recordings and full orientation tracking.

## 3. Requirements (EARS)
| ID | Pattern | Requirement |
| --- | --- | --- |
| MOTION-01 | Ubiquitous | Motion Lab shall consume the existing accelerometer stream without reconfiguring the sensor or changing steps. |
| MOTION-02 | Event-driven | When Set level/recenter is selected, the app shall set a session-only reference and visibly confirm it. |
| MOTION-03 | State-driven | While visible and awake, rendering shall be bounded to 25 Hz with an 80-sample acceleration ring. |
| MOTION-04 | Unwanted behavior | If readings are stale/missing, hardware UI shall show sensor unavailable rather than substitute synthetic motion. |

## 4. Acceptance scenarios
- **AC-01 (MOTION-01/02):** Given a steady/tilted watch, when switching modes and recentering, then the level reference changes without resetting steps or persisting calibration.
- **AC-02 (MOTION-03/04):** Given inactive display or missing samples, when updating, then rendering pauses/unavailable appears; only the desktop demo may explicitly simulate motion.

## 5. Constraints and decisions
Accel ±4 g plot includes gravity; 80 samples at 25 Hz represent about 3.2 seconds.
Level green zone is approximately two degrees; shaking affects acceleration-derived tilt.

## 6. Implementation record
Existing firmware sensor snapshots feed bounded shared UI state. Simple visuals and
separate tabs/status/actions preserve Back and small-display usability.

## 7. Test plan
FW/SIM/UI; manual axis tilt, upside-down transition, recenter, gentle shake, unavailable
sensor, sleep/wake and steps/notification coexistence.

## 8. Documentation impact
Motion Lab note supersedes Stopwatch launcher entry; no calibration/heading claims.

## 9. Risks and approval
Sensor noise, stale state and redraw workload; ratification pending.

## 10. Validation record
Build/UI coverage does not prove physical tilt accuracy. Real tilt/shake scenarios
remain manual; no new sensor interaction performed here.

## 11. History
2026-10-23: retrospective replacement feature and shared sensor constraints.
