# Google calendars, birthdays and Tasks (read only)

## User setup

In the Android companion open **Calendar · choose calendars**. Under **Google
calendars, birthdays and tasks**, press **Connect Google · Calendar + Tasks**,
choose the account and approve the two read-only permissions. This is unrelated
to Mochi/Firebase sign-in and does not enable AI or paid requests.

After authorization, explicitly select **Birthdays · Google account**, Google
calendars, and/or Tasks lists. No sources are automatically selected. Enable
**Share calendar with watch**, then open the watch calendar or tap TODAY to refresh.
The existing layout, colours, holidays, six-week PSRAM cache and protected BLE
channel are retained. Changing selections/account or disconnecting Google clears
cached weeks on the connected watch through the existing privacy-clear operation.
An offline watch can retain its RAM cache until reconnect/reboot.

Local calendars are listed separately, including ones with synchronization off.
Such calendars can only supply whatever events Android has cached; this app never
changes account sync settings. Up to 256 local calendars are listed, 32 selectable.

Google source lists include hidden calendars and up to 128 calendars / 128 Tasks
lists, explicitly indicating list truncation. At most eight Google sources may
be selected across calendars, birthdays and task lists. Where a selected local
calendar's provider NAME equals a selected Google calendar ID, the duplicate cloud
copy is skipped. Providers with unrelated identifiers cannot be reliably deduped:
select the local OR cloud copy, not both. The separate Birthday option reads
`eventTypes=birthday` from the account's primary calendar. Generic cloud calendar
reads exclude birthday occurrences, avoiding duplication within the Google source
selection. Local providers may represent birthdays differently; do not select a
local birthday copy as well as the Google Birthdays source.

Only pending, non-hidden, non-deleted **dated** tasks appear on the watch, including
assigned tasks when available. Google Tasks API exposes a date but **not its
scheduled hour**. Entries are labelled `Task / Date only`; no hour is invented.
Undated tasks cannot be placed on a calendar date and are not shown on the watch.
The integration does not complete, create, reschedule or delete tasks/events.

## Required Google developer configuration

Code alone cannot grant account access or register an Android OAuth identity.
An Android OAuth client must match the installed APK's package AND signing SHA-1.
Google Play services handles consent and token refresh; this app has no client
secret, Web client ID, authorization-code exchange, or OAuth redirect server.

For the existing development project `esp32-smartwatch-511417`, a project owner can:

1. Enable **Google Calendar API** (`calendar-json.googleapis.com`) and **Google
   Tasks API** (`tasks.googleapis.com`) in APIs & Services. No billing/backend,
   Cloud Run, IAM-role or secret changes are required by this implementation.
2. Review Google Auth Platform branding/audience. If in testing, add your own
   Google account as a test user; account/organization policies may also restrict
   consent. Do not overwrite unrelated Firebase/AI clients or configuration.
3. Register a new **Android OAuth client** for package `dev.esp32.smartwatch`,
   using the installed APK's signing certificate SHA-1. The development certificate
   used for this installation is:
   `7E:AD:83:F9:05:3B:D8:E5:99:A0:15:32:AF:FB:F6:44:D0:9E:81:05`.
   Other machines/release builds must use their own actual certificate. This is a
   public certificate fingerprint, not a password or token.
4. Configure/request scopes `https://www.googleapis.com/auth/calendar.readonly`
   and `https://www.googleapis.com/auth/tasks.readonly`. Public distribution may
   require Google's verification; testing consent does not imply verified status.
5. Return to the companion and connect Google. The user must choose the account
   and approve the permissions themselves. A developer-error status 10 normally
   needs the package/certificate/client configuration checked. HTTP 403 can also
   mean an API is disabled or the account/app is restricted; it is not proof of
   successful authorization or of empty calendars.

The implementation agent does not automatically change these Google project
settings. Until registered and consented, cloud synchronization is **not validated**.
Existing local calendars continue to work if no Google sources are selected.

## Data flow / security / bounds

- Account choice uses Android's system account chooser, pinned in subsequent
  Google Identity Services AuthorizationClient requests. No READ_CONTACTS,
  GET_ACCOUNTS, write-calendar or write-task permission is requested.
- Account identifier, selections and holiday overrides reside in private phone
  preferences (backup disabled). Tokens are only short-lived in-memory values
  obtained from Google Play services, never preferences/files/APK/BLE/logs or the
  watch AI backend. Google owns refresh-token management; none is requested/stored
  by this app. Account switching clears the previous cloud selections.
- HTTP is GET-only to fixed `https://www.googleapis.com` Calendar/Tasks endpoints,
  with platform certificate validation, no redirects, bearer token only in the
  Authorization header, 6 s connect/read timeouts and a 1 MiB response cap. Responses
  are not logged. Request fields exclude notes, descriptions, attendee emails,
  conference credentials and other unused data.
- One query has a 70-second HTTP budget and at most 32 HTTP calls, plus at most
  15 seconds for silent token acquisition. Each event/task source is at most three
  100-item pages; source catalogues at most two pages / 128 entries per type.
  Extra pages/events flag truncation, not an empty successful calendar.
- Google expands recurring events (`singleEvents=true`). All-day dates/exclusive
  ends, Madrid DST, birthdays and task dates are parsed before the existing
  seven-bit overlap-mask protocol. Source colours and event overrides are read
  from CalendarList/Colors. Selected holiday sources use the existing holiday mask.
- Local/cloud entries merge chronologically and publish one bounded 32-event week.
  Failure of a selected source fails the snapshot, rather than silently declaring
  missing birthdays/tasks absent. Transient failures preserve a usable watch cache.
  Missing Google consent is status 5 (v4 request), with explicit phone-connect UI
  and privacy clearing; v1-v3 watches receive legacy permission status 2 instead.
- Background work never launches account/consent screens. A stale consent/config/
  BLE generation discards results and prevents further network calls; an already
  running socket can take up to its bounded timeout to return. Calendar BLE frames
  still yield to voice/media/notifications. No AI, IAM, microphone or authentication
  pipeline is changed.
- Google data is refreshed on requested weeks/TODAY, not by a permanent HTTP poll.
  Local provider changes still use the existing observer. Disconnect removes local
  Google sharing immediately and requests Google grant revocation. If offline or
  revocation fails, revoke this app in Google Account permissions as well.

## Validation

Unit tests cover all-day/exclusive ends, Madrid DST, recurring/birthday occurrence
parsing, cancelled/declined filtering, task dates and exclusion of undated/completed
tasks, source colour overrides, ordering/overflow/failure propagation and v1-v4
negotiation. Host UI/protocol tests cover Google's auth-required status and clearing.
Tests use synthetic JSON only; no user calendars/tasks are altered or fabricated.

Manual tests after registration/consent: select an account, birthdays and a task
list; compare dates to Google Calendar/Tasks; verify a dated pending task, recurring
birthday, account switching, denied/partial consent, disabled APIs/offline recovery,
disconnect/cache clearing and existing voice/media/notifications. Do not use adb
to grant permissions or accept Google's consent screens automatically.

References:
- https://developers.google.com/identity/sign-in/android/authorize-access
- https://developers.google.com/workspace/calendar/api/guides/event-types
- https://developers.google.com/workspace/tasks/auth
- https://developers.google.com/workspace/tasks/reference/rest/v1/tasks
