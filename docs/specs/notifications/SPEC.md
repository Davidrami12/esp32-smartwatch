# Android notification forwarding

- **Status:** Ready for approval — retrospective; implemented.
- **Owner:** Project maintainer; historical formal approval unavailable.
- **Last updated:** 2026-10-23.
- **Sources:** `2c8dbd7`; Android `notifications/`, firmware `watch_notifications.*`, `watch_notification_protocol.h`.
- **Related ADR:** [0004](../../adr/0004-single-companion-and-serialized-gatt.md).

## 1. Goal and context
Show bounded phone notification text on the watch using the existing secured link.

## 2. Scope
Explicit notification-listener access, text forwarding and watch presentation.
Exclude general phone access, arbitrary remote actions and private-data logging.

## 3. Requirements (EARS)
| ID | Pattern | Requirement |
| --- | --- | --- |
| NOT-01 | State-driven | While notification access is not granted, the companion shall not claim forwarding is available. |
| NOT-02 | Event-driven | When an eligible notification is forwarded, the transfer shall obey bounded protocol framing and the secure link. |
| NOT-03 | Event-driven | When a notification interrupts another screen, navigation shall preserve the defined return route rather than corrupt its active session. |

## 4. Acceptance scenarios
- **AC-01 (NOT-01/02):** Given denied access then explicit grant, when a notification arrives, then forwarding is unavailable before grant and bounded content can arrive afterward.
- **AC-02 (NOT-03):** Given Mochi/calendar/media active, when a notification is shown/dismissed, then the user returns safely with canceled/stale private work handled by its owning feature.

## 5. Constraints and decisions
Android listener access differs from POST_NOTIFICATIONS permission for the
companion's own notifications. No separate BLE owner or backend is introduced.

## 6. Implementation record
Phone listener → shared repository → optional GATT extension → watch notification UI.

## 7. Test plan
ANDROID/FW/UI; manual allow/revoke, long/accented text, disconnect and app interruption.
Use synthetic notifications, not personal content in captured logs.

## 8. Documentation impact
BLE protocol, mobile setup and assistant interruption guidance.

## 9. Risks and approval
Notifications are sensitive; permission revocation and screen exposure matter.
Ratification pending.

## 10. Validation record
Protocol/UI checks are available; complete compatibility with every Android app is
not guaranteed. No new forwarding test or private-content inspection performed.

## 11. History
2026-10-23: reconstructed notification implementation and coexistence obligations.
