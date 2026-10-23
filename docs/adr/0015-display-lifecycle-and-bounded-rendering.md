# ADR-0015: Display-aware rendering and explicit input/interruption ownership

- **Status:** Proposed for retrospective ratification; implemented approach.
- **Recorded:** 2026-10-23; historical formal approval unavailable.
- **Decision owner:** Project maintainer.
- **Related specifications:** [Display/wake](../specs/display-wake/SPEC.md), [shared UI](../specs/shared-ui/SPEC.md), [Mochi](../specs/mochi-ui/SPEC.md), [Motion Lab](../specs/motion-lab/SPEC.md).
- **Sources:** shared screen state/callbacks, touch gate, Motion Lab and assistant tests.
- **Supersedes / superseded by:** None formally recorded.

## Context and drivers
Frequent rendering or delayed callbacks must not cause sleep navigation, stalled
display updates, overwritten notification screens or work on inactive apps.

## Options considered retrospectively
| Option | Benefit | Cost / risk |
| --- | --- | --- |
| Unconditional polling/redraw and propagated input | Simple local loops | Waste, accidental actions and stale callback navigation |
| Explicit display/screen/session ownership | Bounded rendering and safe interruption | Lifecycle state and regression tests required |
| Stop all background state during screen-off | Fewer updates | Breaks timer phases, connectivity and async processing semantics |

## Implemented decision and rationale
Distinguish state progression from visible drawing. Gate offscreen input before
normal navigation; wake/hold are explicit gestures. Draw feature-specific updates
only at required visible rates: minute clock changes, once-per-second Pomodoro and
bounded 25 Hz Motion Lab. Keep request/session IDs and defined Back/interruption
routes so delayed results cannot steal another screen. Mochi face/name geometry
and static halo remain stable across expressions.

## Consequences
- Timers continue without waking/redrawing the display unnecessarily.
- Sensor history and animation work remain bounded and do not require new persistent capture.
- Each feature must state whether interruption preserves state or invalidates private sessions.
- Game gestures intentionally consume navigation; calendar/list scrolling uses explicit containers.

## Adoption and compatibility
Use existing screen state and display-active callbacks, not one independent thread
or redraw timer per application. Maintain touch-target/layout exceptions consciously.
This is not a deep-sleep or frame-rate benchmark guarantee.

## Validation
Host UI navigation/sleep/stale-result tests and physical double-tap/hold, notification
interruption and scroll checks. Existing evidence does not certify every hardware
gesture or contrast choice. No new physical interaction in this task.

## Open questions and approval history
Future deep sleep or new animated apps must review time/input/resource assumptions.
2026-10-23: retrospective lifecycle record; formal ratification pending.
