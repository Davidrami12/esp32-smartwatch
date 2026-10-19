# Watch volume settings

Settings → Volume is the coral speaker tile. Its percentage is the watch's
locally saved output-volume preference, not the phone's media volume.

- The slider works without Bluetooth, a phone or an active media session.
- Range is 0–100%, default 60%. A release saves to NVS (`output_volume`);
  failed saves restore the previous value. Missing/invalid stored values use 60%.
- Releasing the slider saves the setting and requests a short 180 ms, 880 Hz
  local speaker preview at that codec volume. At 0% no speaker is enabled.
  Previews run on the audio worker, not the UI thread, and are skipped while
  recording, transferring or processing voice audio. The speaker is muted and
  closed afterwards. A future playback path must also apply the saved setting.
- Microphone sensitivity remains on the same page, with its existing independent
  persistence and next-recording behavior.
- The separate Media app keeps its original relative phone-volume controls.
  The erroneous absolute phone-volume protocol extension has been removed.
- No companion update is required for this watch-local setting. No recordings,
  paid AI calls are started automatically; previews require slider interaction.
