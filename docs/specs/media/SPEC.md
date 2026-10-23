# Phone media metadata and controls

- **Status:** Ready for approval — retrospective; implemented.
- **Owner:** Project maintainer; historical formal approval unavailable.
- **Last updated:** 2026-10-23.
- **Sources:** `2c8dbd7`; Android `media/`, `watch_media.*`, `watch_media_protocol.h`.
- **Related ADR:** [0004](../../adr/0004-single-companion-and-serialized-gatt.md).

## 1. Goal and context
Control the phone's active media session from the watch without confusing phone
volume with the watch's independent speaker setting.

## 2. Scope
Media state/title, playback commands and existing relative phone-volume actions.
Exclude audio streaming to the watch and the removed absolute phone-volume extension.

## 3. Requirements (EARS)
| ID | Pattern | Requirement |
| --- | --- | --- |
| MED-01 | Event-driven | When a supported phone session changes, the companion shall publish bounded metadata/playback state over secure BLE. |
| MED-02 | Event-driven | When the user selects a supported media action, the companion shall route it to the active phone media session. |
| MED-03 | Unwanted behavior | If no session or permission is available, the watch shall show media unavailable rather than fabricate playback. |
| MED-04 | Ubiquitous | Media phone-volume actions shall remain separate from watch-local output volume. |

## 4. Acceptance scenarios
- **AC-01 (MED-01/02):** Given a permitted active player, when play/pause/track changes are invoked, then the player and watch metadata converge.
- **AC-02 (MED-03/04):** Given no player or a local speaker edit, when Media opens, then unavailable is explicit and local volume does not silently change phone volume.

## 5. Constraints and decisions
Android app/session support determines available commands. Shared GATT scheduling
must avoid starvation during voice/calendar traffic. Media occupies launcher tile 4.

## 6. Implementation record
Android media bridge and existing repository feed an additive secured extension;
firmware hands snapshots and commands to shared UI.

## 7. Test plan
ANDROID/FW/UI; manually test active/no session, accented/long metadata,
play/pause/skip, relative volume and concurrent voice/notification/calendar transfers.

## 8. Documentation impact
BLE protocol and `docs/volume.md` distinguish phone versus local audio ownership.

## 9. Risks and approval
Third-party players differ; no universal compatibility claim. Ratification pending.

## 10. Validation record
Host UI/protocol coverage exists; final multi-player matrix not recorded. No tests rerun here.

## 11. History
2026-10-23: includes corrected volume ownership and final launcher placement.
