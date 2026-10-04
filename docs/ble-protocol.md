# ESP32 Smartwatch BLE protocol — version 1

This is the source of truth for the ESP32 firmware and future Android companion.
The Android companion consumes telemetry and the additive v1 controls extension below.
Credentials, accounts, DFU, weather and history transfer remain excluded.
All application values are complete snapshots, not increments.

## Discovery and connection

- Stack: ESP-IDF NimBLE, BLE-only peripheral/GATT server, **one central connection**.
- GAP name: **ESP32 Smartwatch** (also readable via the standard GAP Device Name).
- Legacy, general-discoverable, undirected connectable advertising on default primary channels.
- Advertising payload: flags, custom Watch Service UUID and Battery Service UUID.
- Scan response: complete device name. Use active scanning to see the name reliably.
- Interval: **500–750 ms**. Public device identity is used; no rotating privacy address policy yet.
- Disconnected → advertising; connected → advertising stops; disconnection/failed connection → advertising resumes.
- Firmware status: DISABLED (not initialized/reset/error), ADVERTISING, CONNECTED.
- Apps has a Bluetooth tile beside Wi-Fi. Its screen enables/disables advertising and connection intent.
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

Standard GAP (0x1800) and GATT (0x1801) services are supplied by NimBLE.
Notification characteristics have stack-managed Client Characteristic Configuration Descriptors
(CCCD, 0x2902): write `01 00` to subscribe and `00 00` to unsubscribe.
Only the three new controls are writable. Write-without-response is not advertised.
Application values use notifications, not indications. Standard GATT Service Changed
(0x1801 / 0x2A05) uses indications for database cache invalidation (CCCD `02 00`).
All application values fit the default ATT MTU; no fragmentation/framing is needed.

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
The Android app discovers this optional group by UUID; older firmware still provides
telemetry but shows Controls unavailable. Older apps ignore the new characteristics.
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
when that characteristic is subscribed. Android explicitly subscribes before value
reads, handles `onServiceChanged`, drops stale operations and rediscovers, without
hidden `BluetoothGatt.refresh()` APIs, app-data resets or hardcoded handles. A short
750ms initial subscription grace period lets the indication invalidate the cache
before telemetry setup; old firmware without that indication still proceeds normally.
This development policy forces one rediscovery per connection, not continuous polling.

If a phone has previously paired the development watch in Android Bluetooth settings,
its persisted key can outlive the watch's nonbonding firmware/flash. On the tested Pixel,
logs showed LE key-missing recovery followed by a 30-second SMP response timeout and
phone-initiated disconnect (Android status 147; watch reason 531). Forget only the saved
ESP32 Smartwatch entry, then connect through the app without OS pairing. Removing that
stale pairing stopped repeated drops in a 90-second foreground observation; this is not
a guarantee of permanent/background connectivity or a replacement for future secure
bonding design. Do not erase the watch or reset unrelated phone Bluetooth devices.

## Notification policy

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

## Current security policy (development foundation)

Inspected ESP-IDF 6.1 NimBLE defaults in `components/bt/host/nimble/Kconfig.in`,
`port/include/esp_nimble_cfg.h` and NimBLE `host/src/ble_hs_cfg.c`:

- SMP enabled by default; legacy pairing and LE Secure Connections support default enabled.
- Default IO capability is No Input/No Output; default MITM is off.
- NimBLE's compiled host default requests bonding, but this module **explicitly overrides `sm_bonding=0`**.
- This project explicitly configures SMP, legacy and Secure Connections support on, debug keys off,
  bond NVS persistence off. At runtime: No Input/No Output, MITM off, bonding off, SC support on.
- No pairing, bonding, authentication or encryption is initiated or required by this application.
  A central-requested pairing can use unauthenticated Just Works; security level is not forced.
  Optional pairing is not a substitute for access control and needs separate phone validation.
- GATT values/CCCDs have no encryption/authentication flags. **Any nearby central can connect, read/subscribe, and change the three controls.**
  Advertising identity and activity data are not private. This is not a production privacy/security design.
- No credentials/secrets, Wi-Fi provisioning, firmware update or other remote commands are exposed.
  Wi-Fi enable/disable, brightness and timeout are the only remote settings controls.
- Future pairing/bonding requirements will be an explicit documented policy change, not an assumed default.

## Compatibility rules

1. Read Protocol Version before interpreting the Watch Service. Reject unsupported versions gracefully.
2. Keep v1 UUIDs, lengths, byte order and meanings stable. New optional characteristics may be added;
   clients ignore unknown characteristics and unknown enum values rather than treating them as connected.
3. Incompatible encoding/meaning changes require a new protocol version and new affected UUIDs
   (and a new service UUID if the old service cannot remain compatible).
4. Rediscover services after firmware changes; do not depend on cached handles or characteristic ordering.
5. Advertising name/address is not authentication. Future Android code must use UUID discovery and
   the eventual pairing policy, not the name alone.

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
2. Connect; read version (`01 00`), battery (0–100), steps (decode uint32 LE), and Wi-Fi state.
3. Subscribe to all three notification values. Walk at least five steps, stop after a small delta,
   and verify rate-limited updates. Compare with Home and Activity.
4. Change local Wi-Fi intent, including disable/reboot/re-enable, and check enum 3/1/2/0 transitions.
5. Let the screen enter IDLE: steps still update; BLE must not wake the display. Check battery updates
   across sufficient time/percentage change, then double-tap wake and verify the previous screen.
6. Disconnect the phone and confirm advertising resumes; reconnect and read fresh snapshots.
7. Exercise Wi-Fi weather requests while BLE is connected; inspect for resets or resource failures.
8. Test default unpaired access separately from optional central-requested pairing; do not claim
    pairing/security validation based only on successful unencrypted reads.
9. From Android change brightness and timeout; compare watch slider/dropdown and physical
   behavior, reboot and read both persisted values. Disable Wi-Fi remotely and wait beyond
   the normal retry interval: Disabled must persist and not auto-reconnect; re-enable and
   observe Connecting → Connected. Disconnect/reconnect and read actual settings again.
10. With a development BLE client send zero/oversized lengths, brightness 9/101, unsupported
    timeout and Wi-Fi 2/255. Require ATT rejection and unchanged settings/no crash. Test loss
    of connection during a write; do not assume an unacknowledged write was not applied.
