# ESP32 Smartwatch BLE protocol — version 1

This is the source of truth for the ESP32 firmware and future Android companion.
The Android companion consumes telemetry and the additive v1 controls extension below.
Text-only Android notification forwarding is an additional optional v1 extension.
Native Android media metadata/playback controls are another optional additive v1 extension.
Credentials, accounts, DFU and weather transfer remain excluded. Optional dated activity
history is exposed by the secured read-only extension below.
All application values are complete snapshots, not increments.

## AI voice extension (`000f`–`0012`)

All characteristics share the service UUID suffix and the existing trusted-phone,
authenticated/encrypted LE SC/MITM/128-bit key access gate. No API key, login token,
password or cloud credential travels over BLE. Values below are little-endian.

- `000f` read: 18 bytes `[version=1, state, session:u32, pcmBytes:u32, crc32:u32,
  sampleRate:u16, bits:u8, channels:u8]`. State 0 absent, 1 ready, 2 claimed,
  3 processing. Processing retains only session; audio size/format/CRC fields are
  zero after RAM audio cleanup. PCM format: 16000 Hz, 16 bits, mono; size 8000–256000.
- `0010` read: `[version=1, session:u32, byteOffset:u32, PCM…]`. Cap total frame
  at min(512, MTU−3); even PCM payload, contiguous receiver offset required.
  Legacy 244-byte reads remain supported. Preferred MTU is 517; result writes
  retain their independent 244-byte cap.
  Reads never advance offset. Re-selecting an offset is protocol-safe; automatic
  retry is not implemented. Receiver verifies IEEE CRC32 of the complete PCM.
- `0011` write-with-response: `[version=1, opcode:u8, session:u32, offset:u32]`.
  1 selects/claims, 2 finishes/cancels local transfer, 3 accepts full audio into AI
  processing (requires final offset equal to PCM size). Processing releases PCM,
  retains request session and starts a 90-second response deadline. Capture is
   eight seconds maximum; ready/transfer deadlines remain 30/180 seconds (Android
  ends transfer at 175 seconds). Disconnect/Clear/Back/notification interruption
  cancels the request. Late result frames are rejected, not displayed.
- `0012` write-with-response: `[version=1, kind:u8, session:u32, offset:u16,
  totalUtf8Bytes:u16, text…]`, frame cap min(244, MTU−3). Kind 1 transcription
  (1–512 bytes), 2 answer (1–3072), 3 sanitized error (1–160). Contiguous offsets,
  constant kind/total within a message, nonzero payload and matching active request
  are required; complete strings must be valid UTF-8 without NUL. UTF-8 codepoints
  can span chunk boundaries. First offset zero begins a message. Transcription is
  displayed when assembled; answer/error terminates the request. State 8 in the
  internal UI is Complete, not a metadata wire state.

The companion converts CRC-verified PCM to WAV and calls the HTTPS authenticated
backend using a runtime Firebase ID token. Result frames use the same serialized
GATT queue; pending media/notification traffic gets a turn between pull operations.
MTU is negotiated before discovery; below 64 audio remains local. Unsupported
older firmware remains local capture/preview only. See `apps/voice-backend/README.md`
for deployment/authentication and `docs/watch-ai-voice-assistant.md` for evidence.

## Optional voice notification transport v2 (`0013`–`0014`)

Base service protocol remains 1. Existing `000f`–`0012` PCM metadata, pull reads,
v1 controls and result frames are unchanged. New characteristics have the same
trusted encrypted/authenticated bond gate; no credentials travel over BLE.

- `0013` protected read: six-byte capability snapshot `[2,1,4,15,0,2]`: transport
  version 2, codec 1 (unchanged PCM16 mono 16kHz), maximum four credits, 15-byte
  notification header, maximum 512-byte frame little-endian. This Android version
  requires an exact match; missing/unknown capabilities fall back to pull.
- `0014` protected notify + read: read returns `[2]` (no audio replay). Subscribe
  before transferring; no notification is emitted by subscribing alone. After
  capability verification and CCCD acknowledgement, explicitly grant credits.
- `0011` additional **15-byte** write-with-response: `[2,4,session:u32,
  epoch:u32,offset:u32,credits:u8]`. Nonzero session/epoch, strictly increasing
  epoch within a clip, even offset less than PCM size, and 1–4 credits required.
  Android requests four credits. The first grant requires offset zero and claims
  READY audio with the same 180-second expiry. Subsequent grants acknowledge the
  contiguous received prefix and grant the next window, or retry from that prefix
  with a fresh epoch. Grants do not extend the transfer deadline.
- `0014` notification: `[2,session:u32,epoch:u32,offset:u32,sequence:u8,
  flags:u8,PCM...]`. `sequence` starts at zero for each grant and increments after
  successful NimBLE enqueue. Flag bit 0 marks the final credit or clip end; all
  other bits must be zero. Frame cap `min(512,MTU-3)`, even PCM payload: 496 bytes
  at MTU 517, 228 at MTU 247. Offsets, not sequence alone, identify audio bytes.

The firmware emits at most one notification per 10ms host callout and pauses
after four enqueues until the next grant. Enqueue failures do not advance the
cursor; at most 20 attempts/window, paced 25ms. NimBLE owns its mbuf on either
success or failure. No separate audio copy/window allocation is retained on the
watch. Security, cancellation and expiry are checked before every payload copy
and notification send. Unsubscribe/disable/reset stops push; disconnect cancels.

Android accepts only contiguous offset/sequence for the active session/epoch.
Old epochs/sessions and duplicate frames are ignored, gaps are not buffered.
After five seconds without progress, retransmit the contiguous prefix with a new
epoch; at most three retries without prefix progress, within the unchanged
175-second Android deadline. A failed grant or exhausted retry budget switches
to v1 select/read from the same verified prefix (which stops push on the watch).
CRC mismatch, invalid frame or expiry discards the clip; only whole-clip
CRC-verified PCM may enter WAV/backend processing. Final v1 opcode 3 is still
required and authenticated before invoking the existing AI path.

Android bounds pending audio callback snapshots to 16 (8KiB maximum); excess
frames are dropped and recovered by the prefix retry mechanism. Snapshots are
wiped after handling, including stale callbacks. GATT grant/result/media/control
writes remain serialized. Normal companion traffic gets a scheduling turn
between windows; the watch host queue yields between notifications. No burst of
unbounded application writes or unacknowledged audio is allowed.

Old Android + new watch: unchanged pull protocol. New Android + old watch:
capability absent → unchanged pull. Unknown capabilities or subscription failure:
pull. A/B build switch: `-PvoiceNotify=false` (default is true). No codec change,
sample-rate reduction, ADPCM, silence trimming or backend change in this extension.
See `voice-ble-window-optimization.md` for offline evidence and the approved-only
physical A/B plan. Notification speed/reliability is not physically validated yet.

## Dated watch activity extension (`000d`)

Optional `7e57000d-8e7a-4b6d-9f2a-6c4d3e2f1000` is **read-only**, with the same
trusted-phone, authenticated/encrypted SC, MITM and 16-byte-key checks as existing
telemetry. Existing `0003` steps and all other payloads remain unchanged. Older
firmware remains compatible but cannot export Health Connect records: undated
`0003` must never be assigned the phone's date.

Little-endian payload: byte 0 schema **1**, byte 1 record count **0–7**, followed by
that many fixed **24-byte** records, newest recorded date first:

| Record offset | Type | Meaning |
|---|---|---|
| 0 | uint32 | Watch local calendar date YYYYMMDD |
| 4 | uint32 | Full watch-owned daily step count, not a delta |
| 8 | uint32 | Local midnight start, expressed as UTC epoch seconds |
| 12 | uint32 | Today: snapshot time; completed date: next local midnight, UTC epoch seconds |
| 16 | int32 | UTC offset seconds at start |
| 20 | int32 | UTC offset seconds at end |

Length is exactly `2 + 24*count`, maximum 170 bytes. GATT Read/Read Blob supports
minimum MTU; firmware freezes the snapshot for 10 seconds across fragments and
resets the cache on connection. Reads can consequently be up to 10 seconds old.
No writes, CCCD, app credentials or Health Connect SDK exist in firmware.

`watch_activity` exports only records actually present in its bounded current-day/
six-prior-day window. This intentionally differs from zero-filled UI history.
Count 0 means unavailable/unreconciled calendar or no exportable records, **not**
seven zero-step days. Dates outside the window are omitted; zero totals with a real
stored date are legitimate. A reconciled local date must match the system date;
midnight transitions wait for the activity worker rather than relabel yesterday.
The source calendar is currently the firmware's CET/CEST configuration, not the
phone timezone. DST days use calendar midnight boundaries (23/25 hours as needed).

These are **daily aggregates**, not individual walking timestamps. Android strictly
validates schema, length, date order/uniqueness, calendar boundaries and offsets.
It reads after secure connection, once/minute while the existing companion runs,
and on explicit Sync now, always through the serialized GATT queue. Read failures
disable that snapshot only; they do not weaken BLE security or disconnect unrelated
media/notification features. No synthetic data is substituted.

## Read-only daily step goal (`000e`)

Optional `7e57000e-8e7a-4b6d-9f2a-6c4d3e2f1000`: exactly four little-endian
uint32 bytes, persistent daily goal (1,000–30,000, multiples of 1,000). Same SC/
MITM/trusted-phone encryption and 16-byte minimum key requirement as activity.
No writes, notifications or CCCD. Firmware publishes the loaded goal at boot and
updates it only after a watch-side goal change successfully persists. No Android
default goal or editing is introduced. Missing/failed reads display goal unavailable
(or explicitly dated cached goal), never an invented independent goal.

Android reads it immediately after `000d` in the serialized GATT queue, including
manual dashboard refresh and the existing once/minute activity refresh. All prior
UUIDs, activity payloads and protocol version 1 remain unchanged; older firmware is
usable without goal progress. The dashboard's missing-day zeros are presentation
only and never passed to Health Connect as recorded dates.

## Native Android media extension

Optional UUIDs `000a`/`000b` extend v1 without changing existing UUIDs/payloads.
The optional relative-volume extension advertises protected read-only `000c`
(`7e57000c-8e7a-4b6d-9f2a-6c4d3e2f1000`), exactly one byte `01`. Read it after
security verification; only that capability enables snapshot schema 2 and volume
commands. Without it Android sends schema 1 with the original three action bits.
New firmware accepts both schemas, preserving older media clients and firmware.
Android enables media only when both characteristics and the command CCCD exist.
Older secured v1 firmware remains usable for its existing features. Both media
characteristics require authenticated encryption, a 16-byte SC key and trusted-phone
admission. Android verifies protected security status before subscribing/sending.
Standard Service Changed triggers rediscovery, deferred 750 ms for cache invalidation.

### Phone → watch: complete snapshot (`000a`)

Write-with-response frames: schema `uint8=1`, type `uint8=2`, snapshot ID `uint16`,
offset `uint16`, total `uint16`, then 1–12 data bytes (9–20 bytes/frame, MTU 23).
Integers are little-endian. Payload is 12–252 bytes, at most 21 frames:

| Offset | Field |
|---|---|
| 0 | schema `uint8=1`, or `2` after volume-capability verification |
| 1 | state `uint8`: 0 UNAVAILABLE, 1 PLAYING, 2 PAUSED |
| 2 | actions mask: bit 0 previous, bit 1 play/pause, bit 2 next; schema 2 adds bit 3 volume down / bit 4 volume up |
| 3 | reserved `uint8=0` |
| 4 | session generation `uint32`, nonzero when available |
| 8 | title byte length `uint16`, 0–144 |
| 10 | artist byte length `uint16`, 0–96 |
| 12 | title followed by artist, no terminators |

Unavailable requires zero session/actions/text lengths. Text is printable ASCII/Latin-1
UTF-8, normalized like notifications and byte-safely truncated; unsupported glyphs
(including emoji) are removed/replaced. Missing available metadata shows Unknown
track/artist. The watch ellipsizes bounded multiline text and disables unsupported
controls. Metadata never wakes an IDLE watch.

Offset zero starts/replaces assembly; subsequent frames must match ID/total and exact
next offset. Partial assembly expires after five seconds. Invalid fields, lengths,
text/order are rejected; malformed input clears displayed availability. Atomic complete
snapshots, assembly and commands are RAM-only and clear on disconnect, host reset or
command unsubscribe. Android coalesces changes to the latest snapshot and sends current
state after secure reconnect, never a track history. Serialized GATT operations do not
interleave frames with content/control transactions. Text is not persisted/logged.

### Watch → phone: command (`000b`)

Eight bytes: schema `uint8=1`, command `uint8` (1 previous, 2 play/pause, 3 next,
4 volume down, 5 volume up; volume only with the verified extension),
displayed session generation `uint32`, sequence `uint16`. Read returns
`01 00 00 00 00 00 00 00` (no command), never the last tap. CCCD subscription is `01 00`.
The watch holds at most one pending command, sending at most once per 350 ms.
Commands are best-effort notifications, never retried/persisted across disconnect.
Android rejects malformed, duplicate/backward sequences (modulo 65536), wrong sessions
and unsupported actions. Sequence tracking resets on rediscovery/reconnect. The bridge
rechecks current session/playback/actions before native TransportControls dispatch;
toggle pauses while playing, otherwise plays. Stale commands cannot control a replacement
session. Actual playback callbacks confirm state; buttons do not update it optimistically.

### Ownership, availability and validation

The existing connectedDevice CompanionService owns MediaSessionBridge using native
`MediaSessionManager`, `MediaController.Callback` and `MediaController.TransportControls`.
Enabled notification-listener access permits active-session observation; the listener
contains no controller logic and starts no service. Compose contains no media APIs.
No extra FGS or app-specific API is added. Existing stop/recovery rules still apply.

Event callbacks observe up to eight controllers. Choose the first PLAYING session in
manager order, otherwise first PAUSED; stopped/none/error/transitional-only sessions
are unavailable. Metadata/playback/session changes and destruction drive updates,
without polling. Permission revocation/service stop removes observers and publishes
unavailable. Activity closure does not stop observation. Unsupported actions are disabled.
Artwork, seek, playlists, streaming and music storage are out of scope.
Relative volume is now included at the user's subsequent request: native
`MediaController.adjustVolume(ADJUST_LOWER/ADJUST_RAISE, 0)` targets the selected
session's route, not ringer/system volume. PlaybackInfo fixed-volume routes disable
both buttons. `onAudioInfoChanged` refreshes support without polling. No volume slider,
percentage, optimistic volume state or auto-repeat is implemented.

Media UI refinement: centered multiline title and `Artist: name`, circular skip and
play/pause targets, and a lightweight record-groove arc rotating once per four seconds
only for confirmed PLAYING on the visible, awake Media screen. Pause/unavailable,
navigation away and IDLE stop animation and retain a static ring.
Follow-up physical confirmation: user reported layout, playback animation and volume
all working with Spotify on Pixel. Title was subsequently enlarged to 30 px and
artist positioned directly beneath it with an adaptive 8 px gap; text-group height
is bounded to avoid control overlap. Simulator tests cover long-text bounds,
alignment and animation stop/resume. Final typography remains pending visual inspection.
Final boot showed no crash, but a secure reconnect attempt returned status 7;
subsequent Pixel logs confirmed successful protected setup, volume capability read,
all subscriptions and media delivery. This existing intermittent bonded-reconnect
warning remains unresolved; no keys/security policy were changed to bypass it.

Validation (2026-10-12): firmware/simulator builds, parser/UI tests, Android build/tests/lint,
diff check, COM9 flash, Pixel install and bounded boot passed. Initial physical report
was unavailable. After Service Changed discovery refinement, Android logs confirmed both
media UUIDs, command subscription, protected secure setup and media snapshot delivery.
This is transport evidence, not proof of correct visual metadata or playback execution.
User explicitly skipped further physical checks: title/artist, pause/resume/next/previous,
phone-side changes, locked/background controls and session disappearance. Real playback
validation is **SKIPPED**, not PASS. Existing flash-size/BLE light-sleep/LVGL/dependency
advisories remain. No commit/push, flash erase or bond reset performed.
Final diagnostic/busy-state APK build/tests/lint passed after temporary SDK restoration;
its replacement install is SKIPPED because the Pixel now reports ADB unauthorized.
The preceding media APK was installed successfully. Re-enable the companion explicitly
after a future authorized replacement install.

## Discovery and connection

- Stack: ESP-IDF NimBLE, BLE-only peripheral/GATT server, **one central connection**.
- GAP name: **ESP32 Smartwatch** (also readable via the standard GAP Device Name).
- Legacy, general-discoverable, undirected connectable advertising on default primary channels.
- Advertising payload: flags, custom Watch Service UUID and Battery Service UUID.
- Scan response: complete device name. Use active scanning to see the name reliably.
- Interval: **500–750 ms**. Public device identity is used; no rotating privacy address policy yet.
- Disconnected → advertising; connected → advertising stops; disconnection/failed connection → advertising resumes.
- Firmware status: DISABLED (not initialized/reset/error), ADVERTISING/READY (including securing),
  CONNECTED/ON **only after authenticated encryption and trusted identity verification**.
- Settings has a Bluetooth tile beside Wi-Fi. Its screen enables/disables advertising and connection intent.
  Disable stops advertising and terminates an existing connection; a remote disconnect cannot re-enable it.
  Enable resumes advertising. This runtime choice defaults to enabled each reboot (not persisted).
  The initialized host/controller remain allocated; disabled is not a full hardware power-down.
- Requested connection interval: 100–200 ms, peripheral latency 4, supervision timeout 6 seconds.
  A central can negotiate different valid parameters; clients must not depend on these values.

## Services and characteristics

All multibyte **values** use little-endian byte order. UUID strings below are canonical UUIDs;
the NimBLE source represents 128-bit UUIDs in its required least-significant-byte-first array form.
Attribute handles are assigned by the stack: clients must discover by UUID, never hardcode handles.

| Service | UUID |
|---|---|
| Standard Battery Service | `0000180f-0000-1000-8000-00805f9b34fb` (0x180F) |
| Custom Watch Service | `7e570001-8e7a-4b6d-9f2a-6c4d3e2f1000` |

| Characteristic | UUID | Format | Permissions |
|---|---|---|---|
| Battery Level (Battery Service) | `00002a19-0000-1000-8000-00805f9b34fb` (0x2A19) | `uint8`, 1 byte, 0–100 percent | Read, Notify |
| Protocol Version (Watch Service) | `7e570002-8e7a-4b6d-9f2a-6c4d3e2f1000` | `uint16` LE, 2 bytes, value **1** | Read only |
| Today's Steps (Watch Service) | `7e570003-8e7a-4b6d-9f2a-6c4d3e2f1000` | `uint32` LE, 4 bytes | Read, Notify |
| Wi-Fi State (Watch Service) | `7e570004-8e7a-4b6d-9f2a-6c4d3e2f1000` | `uint8`, 1 byte, enum below | Read, Notify |
| Brightness (Watch Service) | `7e570005-8e7a-4b6d-9f2a-6c4d3e2f1000` | `uint8`, exactly 1 byte, **10–100** percent | Read, Write with response, Notify |
| Screen Timeout (Watch Service) | `7e570006-8e7a-4b6d-9f2a-6c4d3e2f1000` | `uint32` LE, exactly 4 bytes, **5000 / 10000 / 30000 / 60000 / 0** ms; 0 = Never | Read, Write with response, Notify |
| Wi-Fi Enabled (Watch Service) | `7e570007-8e7a-4b6d-9f2a-6c4d3e2f1000` | `uint8`, exactly 1 byte, **0 disabled / 1 enabled** user intent | Read, Write with response, Notify |
| Phone Notification Transport (Watch Service) | `7e570008-8e7a-4b6d-9f2a-6c4d3e2f1000` | framed content, **9–20 bytes/write**, below | Write with response only |
| Security Status (Watch Service) | `7e570009-8e7a-4b6d-9f2a-6c4d3e2f1000` | exactly `01 0F 10`: schema 1, flags encrypted/authenticated/SC/trusted, key size 16 bytes | Authenticated encrypted Read only |
| Media Snapshot (Watch Service) | `7e57000a-8e7a-4b6d-9f2a-6c4d3e2f1000` | framed snapshot, 9–20 bytes/write | Authenticated encrypted Write with response |
| Media Command (Watch Service) | `7e57000b-8e7a-4b6d-9f2a-6c4d3e2f1000` | 8 bytes, below | Authenticated encrypted Read, Notify |
| Media Capabilities (Watch Service) | `7e57000c-8e7a-4b6d-9f2a-6c4d3e2f1000` | `uint8=1`: snapshot schema 2 + relative volume | Authenticated encrypted Read |

Standard GAP (0x1800) and GATT (0x1801) services are supplied by NimBLE.
Notification characteristics have stack-managed Client Characteristic Configuration Descriptors
(CCCD, 0x2902): write `01 00` to subscribe and `00 00` to unsubscribe.
Only the three controls, notification transport and media snapshot are writable. Write-without-response is not advertised.
Application values use notifications, not indications. Standard GATT Service Changed
(0x1801 / 0x2A05) uses indications for database cache invalidation (CCCD `02 00`).
All individual writes/values fit default ATT MTU; notification content uses framing.
**All application characteristics, including Battery, Version and Security Status, require
authenticated encryption with a 16-byte key and the single trusted identity.** Service discovery,
standard GAP/GATT metadata and advertising remain public; they are not private content.

### Value semantics

- Battery: supplied by the existing PMIC acquisition, not measured by BLE. If battery is unavailable,
  reading returns ATT Unlikely Error (0x0E); no invalid percentage/sentinel is sent as Battery Level.
  ACTIVE sampling remains five seconds; connected BLE peers allow sampling once per 30 seconds in IDLE.
  Otherwise IDLE retains the last measurement and does not add PMIC polls.
- Steps: the existing live daily activity counter. The day is the firmware's Spain-local calendar day,
  including DST. It can decrease at rollover, clock reconciliation or an explicit local reset;
  clients must replace the displayed count, not assume monotonicity. While clock validity is pending,
  the value is the live pending count; protocol v1 does not transmit a date/time-validity flag.
- Wi-Fi: user intent takes precedence over radio state:

| Byte | Meaning |
|---|---|
| 0 | Disconnected, user intent enabled (may be awaiting an automatic retry) |
| 1 | Connecting, user intent enabled |
| 2 | Connected, IP address obtained, user intent enabled |
| 3 | Disabled by user intent; automatic reconnect suppressed |

Examples: version 1 → `01 00`; 1234 steps → `D2 04 00 00`; battery 77% → `4D`; disabled Wi-Fi → `03`.

### Additive v1 settings controls

Version remains **1** because the existing UUIDs, encodings and semantics are unchanged.
The Android app discovers this optional group by UUID. Security-enabled telemetry-only
firmware can show Controls unavailable, but this app now requires `0009`: old insecure
firmware is rejected rather than silently forwarding private content. Older apps must
bond/authenticate to access otherwise unchanged existing characteristics.
All three controls provide initial reads and change/initial-subscription notifications.
Local watch edits and remote edits update the same cached values. Brightness notifications
reflect the committed local slider/button edit; a local in-progress slider gesture may
temporarily preview another value before its release. No settings polling is needed.

Examples: brightness 60% → `3C`; timeout 30000ms → `30 75 00 00`; Wi-Fi enabled → `01`.
Reject lengths with ATT 0x0D, invalid ranges/enums with ATT 0x13, and application/persistence
or busy failures with ATT 0x0E. Rejected writes are logged without dumping payloads.
Firmware validates before application; no wire parsing lives in shared `watch-ui`.

The synchronous Write Response means the platform callback completed persistence and
application (not simply enqueued the request). Brightness/timeout use existing settings
NVS and runtime callbacks under the BSP LVGL mutex (500ms acquisition limit). Hardware
brightness applies immediately when ACTIVE; in IDLE it becomes the next wake brightness,
without waking the screen. UI slider/dropdown and Settings tiles are synchronized.
Wi-Fi uses the existing worker/request path, with at most 2s waiting for persisted intent;
enabled acknowledges intent, **not** association/IP. Existing Wi-Fi State reports later
Connecting/Connected/Disconnected. Disabled intent suppresses automatic retries.
Wi-Fi persistence errors retain the prior intent rather than falsely confirming success.

Android uses a single GATT operation queue. One control change is allowed at a time;
brightness writes on slider release. On write callback (even rejection), read back the
actual characteristic before releasing controls. Notifications alone do not complete the
write transaction. Failures retain/show confirmed values and a small error. Timeout or
disconnect makes outcome ambiguous: close/reconnect and read the actual persisted state.
Readback failure disables controls until reconnect, while telemetry remains usable.
No multi-characteristic atomic transaction or cross-client ordering is promised.

All persistence remains watch-owned. **No Wi-Fi credentials are exposed.**

#### Firmware-update GATT cache handling

The Pixel cached the original telemetry-only database across an unbonded firmware
update; simple reconnect and Bluetooth toggling did not reveal the new controls.
Firmware now sends one full-range standard Service Changed indication per connection
when that characteristic is subscribed. Android verifies protected Security Status, then subscribes before other value
reads, handles `onServiceChanged`, drops stale operations and rediscovers, without
hidden `BluetoothGatt.refresh()` APIs, app-data resets or hardcoded handles. A short
750ms initial subscription grace period lets the indication invalidate the cache
before telemetry setup; old firmware without that indication still proceeds normally.
This development policy forces one rediscovery per connection, not continuous polling.

Historical nonbonding firmware caused Pixel key-missing recovery, SMP response timeout
and repeated status-147 disconnects. The security firmware deliberately does **not**
automatically repair/replace keys: stale-key recovery now requires **Forget paired phone**
on the watch and **Forget ESP32 Smartwatch** in Android Bluetooth settings, then fresh
numeric comparison through the app. Do not erase the watch or reset unrelated devices.

## Notification policy

### Android content transport (optional v1 extension)

Not to be confused with watch telemetry notifications. Android writes `0008` with
response, serialized through the same native GATT operation queue as settings.
The app discovers capability, never hardcodes handles or requires MTU negotiation.

Each frame has this 8-byte header followed by **1–12 content bytes**:

| Offset | Field | Encoding |
|---|---|---|
| 0 | Framing version | uint8 = 1 |
| 1 | Message type | uint8 = 1 (text notification) |
| 2 | Message ID | uint16 LE, wraps, session-local |
| 4 | Content offset | uint16 LE, first frame 0 |
| 6 | Total content length | uint16 LE, 16–512 |
| 8 | Content bytes | 1–12 bytes |

Maximum write is 20 bytes, fitting ATT MTU 23. Offset 0 begins/replaces the sole
in-progress assembly. Subsequent frames must match ID/total and exact next offset;
duplicates, gaps, interleaving and overflows reject/reset the partial message.
Completion occurs only at exactly total bytes. At >5s since the previous frame,
the partial assembly expires on the next write. Disconnect/host reset clears it.
Partial content is never displayed. ATT 0x0D rejects outer lengths; ATT 0x13 rejects
header, sequence or complete-content validation. Sender abandons rejected messages.

Complete content layout:
- Bytes 0–7: uint64 LE Android post timestamp in epoch milliseconds (presentation
  metadata only; not used to change watch RTC).
- Bytes 8–15: four uint16 LE **byte lengths**, in package/app-label/title/body order.
- Bytes 16+: those four UTF-8 fields concatenated in that order, no terminators.
- Field caps: **96 / 48 / 96 / 240 bytes**; actual maximum 496 bytes / 42 frames.
- Package must be nonempty; at least title or body must be nonempty; missing app
  label renders Phone. Validate exact field totals and text before queue insertion.
- First-version font coverage is printable basic Latin and Latin-1. Android removes
  emoji, unsupported scripts and control characters, collapses whitespace, and
  truncates without splitting a UTF-8 code point. Firmware rejects invalid/unsupported
  UTF-8 and embedded controls. No images, actions, remote views or extras are sent.

Android `NotificationListenerService` extracts only package, application label,
EXTRA_TITLE, EXTRA_BIG_TEXT/EXTRA_TEXT and post time. A centralized policy rejects
own-app, android/SystemUI, ongoing, foreground-service, service-category, group
summary, empty and unsupported-only notifications. Identical repost content is
deduplicated in a volatile 32-entry cache. No active-notification replay.
Notification access is explicit Android special access (NOT_GRANTED / GRANTED),
not POST_NOTIFICATIONS or a runtime permission. The UI links to listener settings.
Access revocation discards queued outbound content. No private bodies are logged.

Listener → application boundary → process-local repository → serialized GATT
writes. Listener never owns/accesses GATT. Android holds at most one transmitting
message and five waiting, dropping oldest waiting on overflow. While disconnected
or capability/access is missing, newly captured notifications are dropped. Disconnect
drops pending content; no history, persistence or backfill. A successful last-frame
response confirms validated queue insertion, **not human viewing**. No retries/replay
on an ambiguous transport loss (avoids duplicate private content).

Firmware owns **one displayed notification + five waiting**; queue-full drops oldest
waiting, not the one being read. A platform LVGL timer pops the oldest when no screen
is displayed; DISMISS/right-swipe consumes current and advances to the next, then
returns to the prior screen. Title/body wrap in a vertically scrollable region; app
label is separate. Notifications are RAM-only; timeout does not consume them.
Any complete new arrival wakes an IDLE display (including when another message is
already displayed), resets the inactivity deadline, and uses normal configured timeout
afterward. No vibration, phone dismissal, actions or replies. Motion/RTC/Wi-Fi remain
independent. Parser/queue have no LVGL dependency; shared UI has no BLE parser.

**Private content now travels only after trusted, authenticated LE Secure Connections
encryption is verified.** This transport is not a complete production security/privacy
audit: physical key extraction, compromised endpoints and advertising tracking remain
outside this feature. No private bodies, keys or comparison codes are logged.

### Watch telemetry notification policy

- Read after discovery to get the latest snapshot; subscribing also queues an initial notification.
- Battery: integer percentage changes, only while available and subscribed.
- Wi-Fi: state/intent changes, only while subscribed.
- Steps: a delta of at least **5 steps**, or a decreased count, is meaningful; notifications are capped
  at once per **5 seconds**. A remaining 1–4-step delta flushes within **15 seconds** of the last notification.
  The existing activity monitor supplies count changes independently of display state (about one second latency).
- Notification work and deferred flushes execute on the NimBLE host event queue, not producer/UI threads.
  No continuous BLE polling task. Values are cached even when BLE is not connected.
- Unsubscribing/disconnecting stops delivery. Notifications are not persisted or replayed after reboot,
  and are not application-level acknowledged. Reconnect and read again after any transport failure.

## Security policy — watch-ble-security

- **LE Secure Connections only, Numeric Comparison, MITM required, 128-bit encryption
  keys, persistent bonding.** Legacy pairing/debug keys disabled; SC-only mode and
  security level 4 configured. Watch IO capability is Display Yes/No. Unsupported
  methods (including Just Works and passkey entry) are rejected, never downgraded.
- Watch shows a zero-padded six-digit code and ACCEPT/REJECT. Android's native OS
  pairing dialog supplies the phone confirmation; the app calls `createBond()` and
  observes pairing/bond broadcasts, never auto-confirms or uses hidden Bluetooth APIs.
  BACK/right-swipe means reject. Pairing wakes IDLE, holds the screen while comparing,
  and restores normal timeout afterward. Security/pairing has a bounded 60s deadline.
- **One trusted phone, one live central.** Trusted identity (not its current rotating
  address) is checked after NimBLE IRK resolution. When a trust slot exists, unknown
  identities are disconnected before private access. Knowing/spoofing the identity
  alone cannot authenticate: the peer must prove possession of the authenticated SC key.
- NimBLE persists LTK/IRK/CCCD records in NVS; separate `watch_ble_sec/phone` NVS record
  reserves/commits the sole trust slot. Reservation is committed before accepting the
  comparison. If first pairing fails or power is lost after acceptance, the slot stays
  locked until explicit Forget; it is not silently offered to another device.
  Completion checks stored authenticated SC material, encryption, bonding, 16-byte
  keys and identity before permitting access. Storage failures fail closed.
- NimBLE storage capacity is **2**, not the logical phone limit: IDF 6.1's NVS migration
  implementation indexes slot 1 even at configured capacity 1 (GCC array-bounds error).
  Application admission enforces one phone. Unexpected multiple/legacy stored peers
  lock the slot. The round-robin store callback is deliberately not used: no eviction.
- All app read/write attributes have NimBLE encrypted/authenticated flags and minimum
  key size 16. An additional callback trust gate rejects unauthorized value access.
  Notification sends/initial subscriptions are gated on secure trusted state, including
  locally built value reads. No pre-authentication telemetry/private notification leaks.
- Security Status adds UUID `0009` without changing old UUIDs/payloads/version. Its
  payload is schema=1, flags=0x0f (bit0 encrypted, bit1 authenticated, bit2 SC, bit3 trusted),
  key bytes=16. Android requires BOND_BONDED and a successful protected status read
  before any other app value, then all reads/subscriptions before CONNECTED. Missing,
  weak, malformed or failed status rejects the session. This is protocol evidence from
  the bonded watch, not independent Android inspection of cipher/MITM parameters.
- Android exposes NOT_BONDED/PAIRING/BONDED/VERIFYING/SECURE/REJECTED_OR_FAILED. Public
  bond APIs do not reliably distinguish user rejection from every pairing failure;
  show a combined honest failure plus recovery instructions, not a guessed cause.
- **Settings → Bluetooth → Forget paired phone** requires confirmation. It revokes
  secure access, disconnects, deletes BLE bonds/CCCDs, and clears the trust record last.
  It does not erase other settings or flash. Repeat pairing/key-missing never evicts a
  phone or silently repairs it. Forget on BOTH watch and Android, then pair anew.
  Regular bonded reconnect/reboot needs encryption, not numeric comparison again.
- Remaining scope: NVS is not protected against physical flash extraction (no new flash
  encryption/secure boot), radio DoS is not prevented, advertising is public/static,
  nearby users can request the first pairing but the watch owner must explicitly accept,
  and a bonded phone trusts Android notification access. No accounts/provisioning/OTA,
  security redesign elsewhere or production certification. Android's foreground companion
  may recover an existing bond in background as described below; never silently re-pair.

## Compatibility rules

1. Bond, verify protected Security Status, then read Protocol Version before interpreting telemetry/content.
2. Keep v1 UUIDs, lengths, byte order and meanings stable. New optional characteristics may be added;
   clients ignore unknown characteristics and unknown enum values rather than treating them as connected.
3. Incompatible encoding/meaning changes require a new protocol version and new affected UUIDs
   (and a new service UUID if the old service cannot remain compatible).
4. Rediscover services after firmware changes; do not depend on cached handles or characteristic ordering.
5. Advertising name/address is not authentication. Use numeric comparison, the bond and protected status,
   not the name alone. An older unencrypted app/firmware pair is not a permitted fallback.

## Power and coexistence

ESP-IDF software Wi-Fi/BLE coexistence is enabled. BLE stays available in ACTIVE and IDLE, sharing the
2.4GHz radio with Wi-Fi/weather. Advertising, connection events, host/controller memory and occasional
IDLE battery samples add power/RAM cost; no deep/automatic light sleep behavior is introduced.
Central-selected connection parameters and subscriptions affect consumption. Measure on hardware
before making battery-life claims. RTC, motion acquisition, steps and daily rollover continue normally.
NimBLE host allocations use the existing external PSRAM to preserve internal DMA RAM for the LCD;
the default internal-only NimBLE allocator caused LCD DMA allocation failures on this board.
mbedTLS allocations also use PSRAM so verified HTTPS weather can coexist with BLE without exhausting
internal RAM. Certificate verification is unchanged; the board's PSRAM must remain enabled.
The added BLE controller code uses ESP-IDF's supported `BT_CTRL_RUN_IN_FLASH_ONLY` option to stay
within the ESP32-S3 Xtensa debug-vector jump range. Flash auto-suspend is **disabled**: enabling it
causes a boot assertion on this board's detected GD flash model. Wi-Fi IRAM optimizations are unchanged.
The existing PSRAM instruction-fetch configuration remains enabled. Controller placement can reduce
peak BLE performance, and flash/NVS writes can disrupt radio timing without suspend support:
validate a connected BLE peer during activity/settings NVS saves. No full-partition erase is introduced.

## Manual validation with nRF Connect or equivalent

1. Active-scan for **ESP32 Smartwatch** and the Watch Service UUID.
2. With the trust slot empty, pair using numeric comparison and confirm on both devices;
   read protected security (`01 0F 10`), version (`01 00`), battery, steps and Wi-Fi state.
3. Subscribe to all three notification values. Walk at least five steps, stop after a small delta,
   and verify rate-limited updates. Compare with Home and Activity.
4. Change local Wi-Fi intent, including disable/reboot/re-enable, and check enum 3/1/2/0 transitions.
5. Let the screen enter IDLE: steps still update; BLE must not wake the display. Check battery updates
   across sufficient time/percentage change, then double-tap wake and verify the previous screen.
6. Disconnect the phone and confirm advertising resumes; reconnect and read fresh snapshots.
7. Exercise Wi-Fi weather requests while BLE is connected; inspect for resets or resource failures.
8. Disconnect the trusted phone; try a second unbonded central. It must be rejected and
   must not read/subscribe/write private values. Reconnect the Pixel without re-pairing;
   reboot the watch and repeat. Test REJECT on watch/phone and interrupted pairing.
9. From Android change brightness and timeout; compare watch slider/dropdown and physical
   behavior, reboot and read both persisted values. Disable Wi-Fi remotely and wait beyond
   the normal retry interval: Disabled must persist and not auto-reconnect; re-enable and
   observe Connecting → Connected. Disconnect/reconnect and read actual settings again.
10. With a development BLE client send zero/oversized lengths, brightness 9/101, unsupported
    timeout and Wi-Fi 2/255. Require ATT rejection and unchanged settings/no crash. Test loss
    of connection during a write; do not assume an unacknowledged write was not applied.
11. Forget the phone on the watch (confirm), try the still-bonded Pixel (must fail safely),
    then forget the Android entry and repeat numeric comparison. Also test phone-only
    key loss with watch trust retained; it must not silently replace the bond.
12. Send a text notification on the authenticated link while ACTIVE and IDLE; verify
    app/title/body, queue/dismiss and normal timeout. Verify all three settings writes
    and readbacks plus telemetry remain usable after bonded reconnect/reboot.

### Security feature validation recorded 2026-10-06

Builds/tests/COM9 flash/Pixel install/boot passed. User confirmed first matching
Numeric Comparison with both confirmations, bonded reconnect without re-pairing,
and persistence across reboot. Pixel protected-status/read/subscription logs confirm
secure readiness. The reboot's first reconnect failed, but a later retry reused the
bond successfully; automatic reconnect stability is not considered fully validated.
Final firmware leaves encryption initiation to Android's protected read rather than
also sending a competing peripheral Security Request; that refinement built/flashed/
booted but the user skipped further physical testing. Independent untrusted central,
Forget/stale-bond recovery, Reject, secure control writes and notification delivery
remain manual follow-up, not claimed PASS. See mobile README for evidence/limitations.

## Android foreground companion (mobile-background-companion)

No firmware, UUID or payload change. A user-started `connectedDevice` foreground
service now owns the existing repository/GATT session independently of Activity
lifetime. ViewModel/UI and notification listener share the single repository StateFlow
through an application-level companion API. Listener never starts a service or owns GATT.
Foreground notification is status-only on low-importance `watch_connection`; tapping
opens the app. POST_NOTIFICATIONS visibility permission is separate from listener access.

Unexpected transport disconnect retries only the existing Android bond: 0/5/15/30/60/
120s backoff, capped at 120s, one 20s GATT attempt at a time, no recovery scanning or
telemetry polling. Security status `0009` and all setup reads/subscriptions remain
mandatory before CONNECTED/private content. Missing bonds or definitive security/
protocol/permission failures stop retries rather than weaken authentication. Manual
Disconnect cancels work and stops the service; it cannot auto-reconnect until enabled
again. Bluetooth OFF waits for ON rather than spinning.

Home/lock/Activity destruction/recents removal intentionally preserve an enabled FGS.
Process kill, force-stop/Active apps Stop, APK replacement and phone reboot do not
auto-start it (START_NOT_STICKY, no boot receiver); enable from the visible app again.
Disconnected private notifications and ambiguous writes are never replayed. Android/
OEM power policy can still delay recovery; see mobile README for physical results.

Validation: Android build/9 tests/lint, diff check and Pixel installs passed. User
confirmed secure connection and visible FGS notification. Three-minute Home/screen-off
soak retained the process/foreground service, and a new text notification completed
all ten secure BLE writes with status 0. Visual watch/UI reopening confirmation,
recents removal, power-off/on recovery and manual-disconnect checks were skipped by
user request. Reconnect policy unit tests are not claimed as a physical reconnect PASS.
# Current bonding policy (supersedes historical single-phone admission below)

The custom NVS trusted-phone slot, reserved-pairing lock and extra stored-key
verification gate have been permanently removed at the owner's request.
NimBLE owns persistent bonds. Protected application operations still require a
bonded, authenticated, encrypted connection with a 16-byte key, and new pairing
still requires numeric comparison confirmed on both devices. Any successfully
bonded phone can access the watch; there is no separate single-phone allowlist.
The legacy Security Status `01 0F 10` remains wire-compatible; its admission bit
now means the current platform-authenticated bonded session is admitted, not
that a separate persistent trusted identity was verified. Paid-AI consent and
backend authentication are unchanged. Historical test results below do not
validate this changed policy.
