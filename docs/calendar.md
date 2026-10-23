# Read-only phone calendar / weekly watch agenda

## Setup and use

1. Install the updated Android companion and watch firmware. Keep the existing
   bonded, authenticated, encrypted BLE connection. Local sources need no Google
   login; optional Google calendars/birthdays/Tasks require explicit OAuth consent.
2. In the companion, open **Calendar · choose calendars**, explicitly grant
   **read-only calendar access**, select the calendars to share, and enable sharing.
   Nothing is selected or shared by default. The source must be synced into
   Android's Calendar Provider (Google, Exchange, etc.). Work profiles can restrict
   access. Google Tasks is not a Calendar Provider event source: connect it through
   the separate Google source selector described in [google-agenda.md](google-agenda.md).
   The selector now includes local calendars whose synchronization is disabled,
   labelled as cached-only, without changing account sync settings.
3. Open **Apps**, then **CALENDAR** in the first tile (replacing the Activity launcher shortcut, not activity data). The app grid
   scrolls independently of its fixed heading and Back button.
4. Calendar opens a **month grid** titled **Calendar**, without `/ TODAY`.
   Arrows change months; tap **Last refresh** to retry sync. Dates show up to three event bars
   in Android Calendar Provider `DISPLAY_COLOR` (event override, otherwise calendar
   colour). A `+` marks additional events or a truncated week. Date cells contain
   only day numbers (no `?` suffix). Neighbouring-month dates are dimmed; Today is
   highlighted by a neutral outline. In-month weekdays have black cells; weekends
   and imported holiday dates use a very subtle dark grey background (`#101418`), with pale blue-grey digits.
   Neighbouring-month cells are near-black with much darker, smaller day numbers
   labels and event bars at 20% opacity. Event bars retain their source colours.
   The month/year heading is enlarged to 22 px within its existing button.
   The small 12 px footer shows **Last refresh** and the oldest visible cached
   week's age, with a short offline/partial/refreshing/error qualifier when needed.
   No successful timestamp or invalid watch clock is shown as `--`, never a fake
   refresh time. Unsynced dates cannot be assumed empty; the weekly agenda retains
   explicit not-synced states. Tap a date to open its **weekly agenda**, initially
   scrolled to that day. Tap the month/year heading to return to the current month,
   without opening another view. Back returns to the grid.
5. The weekly agenda is one vertically scrollable list containing Monday–Sunday,
   with each day's events grouped under its date (including empty-day labels).
   There is no daily selector. Previous/next arrows change weeks; tap the week range
   to retry synchronization without changing weeks. No `/ TODAY` text is shown.
   The heading includes the year (both years across New Year); all times use Madrid.
6. Scroll the week's events and tap a row for title, date/time and location.
   Back/right swipe from details returns to the agenda, then month grid, then Apps.

The watch distinguishes pending, unavailable, denied/off/no-calendar selection,
empty days, failed sync, and offline cached data. Cached data displays its age.
Failed refreshes retain a usable cache; they do not pretend it is fresh. Each week
contains at most 32 occurrences across all seven days. Overflow is explicitly
marked, including empty days in a truncated snapshot. Long UTF-8 title/location
fields are bounded to 80 bytes each, time labels to 40 bytes. The watch's existing
font coverage applies; this is a text agenda, not rich Google Calendar rendering.

## Privacy and offline behavior

- Calendar sharing is opt-in, read-only, independent of AI and notification access.
  No WRITE_CALENDAR permission, Google password, watch-side OAuth token, AI/backend
  request, calendar mutation, SD access or flash persistence is introduced. Optional
  Google sources use phone-only read-only OAuth and direct HTTPS to Google, not AI.
- Calendar choice IDs and the sharing toggle are saved in private phone preferences.
  Event details are not logged or saved by the companion. Only title, time,
  location, effective display colour and a seven-bit imported-holiday date mask
  are transferred over protected GATT.
- Six most recently received weeks are cached in watch RAM (bounded, about 40 KB).
  Firmware allocates the calendar cache, assembler and handoff buffers in PSRAM to
  preserve internal memory for display DMA and existing BLE/voice functionality.
  Reboot discards them. Browsing an uncached week offline says **Not synced**, not
  **No events**. Calendar data never wakes the screen.
- Sharing/selection changes and detected permission loss cancel in-flight query
  results, discard queued calendar frames, and request clearing all watch caches.
  Cached details are also cleared when forgetting the paired phone on the watch.
  If disconnected or Android has killed the companion, remote clearing cannot
  happen until the updated companion runs and reconnects. The watch can still show
  its old RAM cache until then (or until reboot).
- Revocation uses one short high-priority clear write; event frames are background
  work, one at a time, behind voice, media, notifications and controls. Partial or
  corrupt snapshots never become a visible week. Existing voice credits, PCM/CRC,
  fallback, consent, bonding and transport security remain unchanged.

## Data flow and limits

Android queries `CalendarContract.Instances` off the UI thread, so recurring
occurrences and recurrence exceptions are supplied by the OS provider. Selected
calendar IDs only; deleted/cancelled/declined events are excluded. Each query scans
at most 2,048 rows, retains at most 32 events, and selects at most 32 calendars.
Settings lists at most 256 local calendars. No event descriptions, guests or conferencing
credentials are requested. Provider changes are observed with a short debounce.

All-day dates use Android's UTC date boundaries with exclusive ends. Timed events
use Europe/Madrid with DST; events ending at midnight do not spill into the next
day. Multi-day events carry a seven-bit day-overlap mask. Weeks are requested on
entry/navigation/reconnect; the phone checks the request every two seconds while
connected, yielding to audio. Unchanged requests do not cause repeated provider
queries. Connection loss or rejected transfers stops the transaction; selecting
the week range or month refresh footer retries. The watch times out pending sync after 130 seconds.

Simulator data is explicitly labelled **DEMO** and requires no calendar permission.
It is never injected into firmware or sent to a real calendar.

The month grid serializes six week requests through the existing bounded channel;
it never transfers a whole unbounded calendar. Six weekly caches cover the 42
Monday-first cells (including leading/trailing neighbouring-month dates). Changing
month, tapping a date or switching to the weekly agenda replaces the active request,
so old fragments cannot overwrite the newly selected request. Dates are compact
48×36 px targets; the day agenda retains larger controls for reading and scrolling.
Each date shows at most three bars, with `+` for overflow; this is not a guarantee
that all events were transferred when a week hit its existing 32-event limit.
The phone uses Calendar Provider `DISPLAY_COLOR`; Google account colours must be
synced to that provider to match. Local-only mode adds no Google API credentials.
Optional direct Google source authorization and its bounds are documented separately
in [google-agenda.md](google-agenda.md); OAuth tokens never enter firmware/BLE.

Holiday classification uses **selected** source-calendar metadata, not event-title
guessing or hardcoded holiday dates. Names such as "Festivos de España", "Festivos
de Madrid", "Fiestas de la Comunidad de Madrid", "Holidays in Spain" and Google's
Spain holiday subscription identifier are suggested automatically (accent/case
insensitive). Every selected calendar has an explicit **Holiday calendar · weekend
colour** checkbox in the companion, which overrides the suggestion and supports
arbitrarily named .ics imports. Only the imported provider occurrences define
holiday dates; no claim is made that a subscription contains only legal public
holidays rather than observances. A change cancels stale queries and clears caches
through the existing privacy/configuration path. Sharing/permission gates remain.

The weekly holiday mask is computed across the bounded 2,048-row scan, even after
32 agenda slots are full. Thus a busy personal agenda does not hide a later holiday
within that scan. Hitting the scan limit still marks the snapshot truncated; absent
holiday highlights in incomplete/offline data are not proof of a working day.

Month-grid tests additionally cover default entry, month/year navigation, February
29, adjacent-month cells, serialized six-week demo responses, RGB decoding/display,
overflow markers and grid bounds. Colour wire negotiation keeps version-1 firmware
requests compatible with updated companions; version-4 watches require the updated
companion. Manual checks must compare actual Google calendar colours/event overrides
and touch readability on the physical display.

## Validation

Host tests cover date validity, Monday/year transitions, CRC, fragmentation at
minimum MTU, stale request IDs, gaps, malformed UTF-8 and oversize messages; UI day
selection, pagination, event details, empty/offline/error states, cache eviction,
permission clearing and layout bounds. Android unit tests cover MTU bounds, CRC,
UTF-8 prefix limits, all-day/exclusive-end semantics, both DST transitions, and
request validation. Existing UI/voice/media/notification tests remain enabled.
Additional tests cover v1/v2/v3 negotiation, holiday-name suggestions (and negative
personal-calendar cases), holiday-mask bounds/CRC decoding, matching weekend and
weekday-holiday colours, the small Last refresh footer and removal of `?`/WEEK VIEW.

Manual physical tests: grant permission and choose calendars yourself; compare
today and adjacent weeks against Google Calendar, including recurring/all-day/
multi-day events; update/delete an event on the phone; check offline cached and
uncached weeks; disable sharing or deselect calendars and confirm all cached
details disappear; verify sleep/wake, touch scrolling and Back; check existing
media/notification behavior and perform a manual voice test if desired (ordinary
AI consent/possible charges still apply). Do not use adb to grant calendar access
or insert synthetic events into the user's real calendars.

Implementation validation: firmware and simulator builds, host UI/protocol tests,
Android debug build/unit tests and whitespace checks passed. The companion was
installed on the Pixel and the firmware flashed on COM9. An initial internal-RAM
allocation caused SPI display DMA starvation/watchdog warnings; moving the bounded
calendar buffers to PSRAM resolved that failure. The final 35-second boot window
showed successful display/touch initialization, BLE advertising, RTC/SNTP and live
weather without DMA failures or watchdog warnings. Actual calendar synchronization
still requires user permission, selection and physical interaction; it was not
validated by reading the user's calendars automatically.
