# Local speaker volume and microphone sensitivity

- **Status:** Ready for approval — retrospective; implemented.
- **Owner:** Project maintainer; historical formal approval unavailable.
- **Last updated:** 2026-10-23.
- **Sources:** `c2f966f`; `watch_settings.*`, `watch_voice.*`, shared volume UI, `docs/volume.md`.
- **Related ADR:** [0013](../../adr/0013-local-settings-and-power-ownership.md).

## 1. Goal and context
Offer meaningful watch audio controls, correcting an earlier phone-volume interpretation
while keeping microphone sensitivity on the same page.

## 2. Scope
Output volume, local preview, microphone gain and independent persistence.
Exclude phone absolute-volume commands, recording files and TTS playback.

## 3. Requirements (EARS)
| ID | Pattern | Requirement |
| --- | --- | --- |
| AUDIO-01 | Event-driven | When output volume is released, the firmware shall save a 0–100% local setting with default 60%, preserving the old value on save failure. |
| AUDIO-02 | Event-driven | When a nonzero preview is requested and audio is idle, the worker shall play a local 180 ms, 880 Hz preview and mute/close the speaker afterward. |
| AUDIO-03 | State-driven | While volume is zero or a voice capture/transfer/process session is busy, the firmware shall suppress preview playback. |
| AUDIO-04 | Event-driven | When microphone sensitivity changes, the firmware shall persist 0–42 dB in 6 dB steps, default 24 dB, and apply it to subsequent recordings. |

## 4. Acceptance scenarios
- **AC-01 (AUDIO-01/02):** Given no phone and idle audio, when editing/releasing volume, then the setting persists and a local preview plays at the selected level.
- **AC-02 (AUDIO-03/04):** Given zero/busy audio and a gain edit, when requesting previews/new capture, then preview remains silent and the next capture uses the saved gain.

## 5. Constraints and decisions
NVS keys `output_volume` and `mic_gain`; codec work is off UI thread. Relative
phone media volume stays owned by Media. No extra BLE extension is needed.

## 6. Implementation record
Settings callbacks → firmware persistence → audio worker; corrected erroneous absolute
phone-volume behavior while preserving the gain control.

## 7. Test plan
FW/UI and manual independent phone/watch volume, mute, busy session, reboot/gain checks.
Preview is user-triggered only; do not start recordings/paid AI automatically.

## 8. Documentation impact
Volume feature note and Settings/Media distinction in README.

## 9. Risks and approval
Codec contention and unexpected loud preview; ratification pending.

## 10. Validation record
Build/flash and audio-setting regressions were reported; subjective loudness and
microphone calibration require physical interaction. No preview or recording here.

## 11. History
2026-10-23: retrospective local audio ownership and preview implementation.
