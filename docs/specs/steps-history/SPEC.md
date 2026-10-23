# Motion sampling, step history and watch Activity

- **Status:** Ready for approval — retrospective; implemented.
- **Owner:** Project maintainer; historical formal approval unavailable.
- **Last updated:** 2026-10-23.
- **Sources:** `0176d9a`, `d0d336f`, `a1fb9ab`; `watch_motion.*`, `watch_steps.*`, `watch_activity.*`, `docs/watch-activity.md`.
- **Related ADR:** [0006](../../adr/0006-watch-owned-dated-activity.md).

## 1. Goal and context
Turn accelerometer samples into dated daily counts and a useful activity display,
without presenting estimated steps as medical or calibrated sports measurements.

## 2. Scope
QMI8658 samples, filtered step detection, day rollover, seven records, persistent
goal and ring/chart UI. Exclude distance, calories, GPS and heart-rate measurements.

## 3. Requirements (EARS)
| ID | Pattern | Requirement |
| --- | --- | --- |
| STEP-01 | Event-driven | When a qualifying filtered motion peak is confirmed, the detector shall increment the counter subject to its minimum step interval. |
| STEP-02 | Event-driven | When a valid local date rolls over, activity storage shall retain dated history and start the new day's count. |
| STEP-03 | Event-driven | When the goal changes, the firmware shall persist a value from 1,000 to 30,000 in 1,000-step increments, defaulting to 8,000. |
| STEP-04 | Ubiquitous | Activity shall display live today's steps, a saturated goal ring and selectable seven-day counts without truncating the actual count to the goal. |

## 4. Acceptance scenarios
- **AC-01 (STEP-01/02):** Given real samples and a valid clock, when walking and crossing midnight, then counts advance and records retain their correct dates.
- **AC-02 (STEP-03/04):** Given a goal edit or count above goal, when saving/rendering, then bounds apply, failure preserves the previous goal and the numeric count remains complete.

## 5. Constraints and decisions
Detector has a 300 ms minimum interval; tuning requires on-wrist validation.
History owns dates, not the phone. Chart goal line represents the current goal.
Removing the Activity launcher shortcut does not remove activity data/features.

## 6. Implementation record
Sensor adapter → detector → activity storage → UI/BLE snapshots; final ring and
selectable weekly graph reuse existing counts and storage.

## 7. Test plan
FW/UI; `tests/activity_history_host.py` and activity cases in UI host tests.
Manual walking, orientation/shaking rejection, reboot, midnight, goal and sleep/wake.

## 8. Documentation impact
`docs/watch-activity.md`, `docs/home-context.md`, dated BLE history documentation.

## 9. Risks and approval
False steps and invalid clocks; no accuracy certification. Ratification pending.

## 10. Validation record
History/UI tests exist and physical startup is observed; detector accuracy and
long-term rollover reliability are not established by compilation. No reruns here.

## 11. History
2026-10-23: covers initial motion/step integration through final activity UI.
