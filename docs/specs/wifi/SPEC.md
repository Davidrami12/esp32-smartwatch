# Wi-Fi connection intent and recovery

- **Status:** Ready for approval — retrospective; implemented.
- **Owner:** Project maintainer; historical formal approval unavailable.
- **Last updated:** 2026-10-23.
- **Sources:** `9611056`, `ec624ec`; `apps/watch/main/watch_wifi.*`, `docs/dashboard-connectivity.md`.
- **Related ADR:** [0005](../../adr/0005-platform-owned-network-workers.md).

## 1. Goal and context
Support time/weather connectivity without blocking UI or reconnecting against an
explicit user disconnect request.

## 2. Scope
Connect/disconnect intent, state, retry and persistence. Exclude on-watch credential
editing, phone tether management and location/GPS services.

## 3. Requirements (EARS)
| ID | Pattern | Requirement |
| --- | --- | --- |
| WIFI-01 | Event-driven | When the user disables Wi-Fi, the worker shall persist disabled intent and suppress automatic reconnect. |
| WIFI-02 | State-driven | While association/DHCP is incomplete, the UI shall not report an IP-connected state. |
| WIFI-03 | Unwanted behavior | If an enabled connection is unexpectedly lost, the worker shall apply the documented bounded retry cadence. |
| WIFI-04 | Ubiquitous | Wi-Fi operations shall execute outside LVGL event callbacks. |

## 4. Acceptance scenarios
- **AC-01 (WIFI-01/02):** Given a connected watch, when disconnected manually, then it stays disconnected; reconnect shows Connecting until IP acquisition.
- **AC-02 (WIFI-03/04):** Given an unavailable AP, when connection fails, then UI remains navigable while retries follow policy.

## 5. Constraints and decisions
`watch_wifi/enabled` stores intent; missing/invalid defaults enabled. Historical
policy: 5-second retries in bursts of five, 60-second pause, 30-second connection
deadline. Credentials originate from the private build configuration.

## 6. Implementation record
Serialized requests/events in `watch_wifi.*`; `main.cpp` hands snapshots to shared UI.

## 7. Test plan
FW/UI/BOOT; manual AP loss/recovery, duplicate requests, reboot intent and explicit OFF.

## 8. Documentation impact
`docs/dashboard-connectivity.md` and README setup/security.

## 9. Risks and approval
NVS errors can prevent persistence; router/DHCP behavior requires physical tests.
Ratification pending.

## 10. Validation record
Recent weather boot logs imply connectivity in that environment, not exhaustive
retry/intent validation. No network tests executed for this reconstruction.

## 11. History
2026-10-23: retrospective network ownership and recovery requirements.
