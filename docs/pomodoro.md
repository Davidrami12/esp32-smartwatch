# Pomodoro

Pomodoro replaces the adjustable Timer app. Its static tomato icon appears in
the launcher and on the app screen.

A non-interactive circular ring fills clockwise as the phase completes, red
for focus and green for breaks. The remaining MM:SS countdown sits inside it.
Updates are limited to once per second while the app is visible and awake.

- Focus: 25 minutes.
- Short break: 5 minutes.
- Every fourth completed/skipped focus leads to a 15-minute long break.
- START/PAUSE/RESUME controls the current phase. At completion, START NEXT advances
  to and starts the next phase. SKIP PHASE advances without starting.
- RESET clears the cycle and returns to the first focus session.
- Completion is a visual DONE status only; no sound, vibration or automatic wake.
- Time continues while navigating away or while the screen is off. Each phase
  stops at zero, so an unattended session never consumes a break automatically.
- State is held in RAM and resets when the device restarts.

The stopwatch is unchanged.

The main action is a full-width colored button, with secondary Reset/Skip
controls separated from Back. Four dots mark cycle progress: filled dots are
completed/skipped focus phases and an outlined red dot is the current focus.
The ring separates the phase title, countdown and Ready/Paused/running status.
