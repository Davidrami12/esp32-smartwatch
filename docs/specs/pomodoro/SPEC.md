# Manually controlled Pomodoro cycle

- **Status:** Ready for approval — retrospective; implemented.
- **Owner:** Project maintainer; historical formal approval unavailable.
- **Last updated:** 2026-10-23.
- **Sources:** `47f2ec3`; shared time/UI helpers and `docs/pomodoro.md`.
- **Related ADR:** [0002](../../adr/0002-wall-clock-and-monotonic-time.md).

## 1. Goal and context
Replace the adjustable Timer launcher entry with a simple focus/break workflow
that does not automatically consume unattended phases.

## 2. Scope
25-minute focus, 5-minute break, 15-minute long break every fourth focus, pause,
skip/reset, cycle dots and static tomato icon. Exclude alarms, persistence across reboot and automatic next phase.

## 3. Requirements (EARS)
| ID | Pattern | Requirement |
| --- | --- | --- |
| POMO-01 | Event-driven | When a phase starts/resumes, Pomodoro shall measure remaining duration from monotonic elapsed time using the defined 25/5/15-minute cycle. |
| POMO-02 | Event-driven | When a phase completes, the app shall stop at zero and require START NEXT to start its successor. |
| POMO-03 | Event-driven | When SKIP PHASE or RESET is selected, the app shall respectively advance without starting or reset to the first focus. |
| POMO-04 | State-driven | While offscreen/display inactive, phase time shall continue without visible per-second redraws or automatic wake/audio. |

## 4. Acceptance scenarios
- **AC-01 (POMO-01/02):** Given a started focus, when time reaches zero including offscreen duration, then DONE remains until explicit next start.
- **AC-02 (POMO-03/04):** Given completed/skipped focuses, when the fourth advances, then the long-break duration is selected; reset returns to initial focus without sound/wake.

## 5. Constraints and decisions
RAM-only state; red focus/green break ring, once-per-second visible update. A skipped
focus counts toward cycle progression as documented. Screen sleep is not a phase pause.

## 6. Implementation record
Existing monotonic helpers and navigation support the replacement app; no new scheduler or NVS.

## 7. Test plan
UI/time host tests and FW/SIM; manual pause/resume, fourth phase, skip, offscreen expiry.

## 8. Documentation impact
Pomodoro note supersedes original Timer launcher instructions, not all historical tool code.

## 9. Risks and approval
Confusing completion with automatic progression; ratification pending.

## 10. Validation record
Existing tests/build evidence do not establish every real 25-minute cycle or battery effect.
No new long-running session performed in this task.

## 11. History
2026-10-23: retrospective replacement of Timer with manual phase progression.
