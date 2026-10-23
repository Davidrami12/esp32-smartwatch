# ADR-0013: Firmware-owned settings, local audio and advisory power guidance

- **Status:** Proposed for retrospective ratification; implemented approach.
- **Recorded:** 2026-10-23; historical formal approval unavailable.
- **Decision owner:** Project maintainer.
- **Related specifications:** [Settings](../specs/settings-power/SPEC.md), [audio](../specs/audio-settings/SPEC.md), [Battery Saver](../specs/battery-saver/SPEC.md).
- **Sources:** `watch_settings.*`, `watch_power.*`, `watch_voice.*`, volume/battery notes.
- **Supersedes / superseded by:** None formally recorded; erroneous absolute phone-volume behavior was removed.

## Context and drivers
Settings must persist on the watch without UI storage dependencies. Watch speaker
volume is not phone media volume, and power advice must not silently disconnect
services or imply unmeasured savings.

## Options considered retrospectively
| Option | Benefit | Cost / risk |
| --- | --- | --- |
| UI directly writes NVS/codec | Short path | Platform coupling and blocking work |
| Firmware callbacks/adapters own storage/audio | Testable UI and clear ownership | Save-failure and worker feedback required |
| Automatic Battery Saver overrides | Potential aggressive reduction | Unexpected setting changes and unmeasured claims outside scope |

## Implemented decision and rationale
Shared UI requests bounded changes; firmware validates/persists settings and owns
hardware effects. Speaker volume controls ES8311 locally with a short user-triggered
preview on the audio worker; microphone gain is independently persisted. Media
retains relative phone actions. Battery Saver links to existing controls and explains
tradeoffs without automatically changing them.

## Consequences
- Local volume works without a phone and needs no new GATT extension.
- Preview must be silent at zero, suppressed during busy voice work and muted/closed afterward.
- Invalid/missing values use defined defaults; save failure must not falsely claim success.
- Display IDLE/brightness changes are not evidence of deep sleep or remaining battery hours.

## Adoption and compatibility
Keep existing NVS namespaces/keys and callback semantics; no automatic erase.
Removed phone absolute-volume protocol is not reintroduced. New playback features
would also need to apply local output-volume preference explicitly.

## Validation
UI bounds/save-failure tests and manual reboot, independent phone/watch volume,
preview/busy/mute checks. No power/loudness calibration or new tests in this task.

## Open questions and approval history
Future autonomous power policy or speaker TTS needs a separate scope/decision.
2026-10-23: retrospective record; formal ratification pending.
