# Optional Google calendars, birthdays and Tasks

- **Status:** Ready for approval — retrospective; implemented, live account validation conditional.
- **Owner:** Project maintainer/account owner; historical formal approval unavailable.
- **Last updated:** 2026-10-23.
- **Sources:** Android `GoogleAgendaAuth.kt`, `GoogleAgendaClient.kt`, `GoogleAgendaData.kt`, `GoogleAgendaSettings.kt`; `docs/google-agenda.md`.
- **Related ADR:** [0011](../../adr/0011-optional-read-only-google-agenda.md).

## 1. Goal and context
Provide cloud-only birthday/task sources missing from Android's local Calendar
Provider without broad contacts access or reusing Mochi consent incorrectly.

## 2. Scope
Explicit Google account choice/read-only authorization, calendars, primary-calendar
birthday occurrences, dated pending Tasks and disconnect. Exclude task edits, invented task times and automatic Cloud/OAuth provisioning.

## 3. Requirements (EARS)
| ID | Pattern | Requirement |
| --- | --- | --- |
| GOO-01 | Event-driven | When Connect Google is selected, the app shall request the selected account's Calendar/Tasks read-only scopes through Google Play services. |
| GOO-02 | State-driven | While no sources are selected, Google sign-in alone shall not enable sharing; background queries shall not launch consent UI. |
| GOO-03 | Event-driven | When selected sources are queried, the bridge shall map birthday occurrences and pending dated tasks to bounded week snapshots without inventing scheduled hours/dates. |
| GOO-04 | Event-driven | When account/selections/authorization change, stale work shall be discarded and private watch data invalidated through the existing clear path. |
| GOO-05 | Ubiquitous | Tokens shall remain phone-only in memory/Google Play services, with GET-only bounded HTTPS to fixed Google API origins and no AI backend forwarding. |

## 4. Acceptance scenarios
- **AC-01 (GOO-01/02):** Given no Google grant, when connecting explicitly, then account consent is shown and no calendar is selected automatically; silent background work cannot display a consent prompt.
- **AC-02 (GOO-03/05):** Given synthetic birthdays/tasks, when parsing, then date masks/colors match and undated/completed tasks are excluded with no token in BLE/logs.
- **AC-03 (GOO-04):** Given an in-flight old-account request, when switching/disconnecting, then old results do not publish; denied consent/auth-required state is visible rather than an empty calendar.

## 5. Constraints and decisions
Scopes `calendar.readonly` and `tasks.readonly`; no READ_CONTACTS or write permissions.
Eight selected Google sources maximum; catalogues max 128 entries/type. Query budget
32 HTTP calls/70 seconds plus silent token acquisition; per-source max three 100-item pages.
Duplicate suppression by matching local NAME/Google ID is best-effort, not universal.
Protocol request v4 adds status 5, mapped to legacy permission status 2 for v1–v3 peers.

## 6. Implementation record
Google account chooser → GIS token → bounded Calendar/Tasks client → pure parsers →
existing calendar merge/transport. Account metadata/selections are local; no OAuth secret in APK.

## 7. Test plan
ANDROID parser/protocol tests; manual package/SHA-1 OAuth registration, enabled APIs,
real consent, account switch, dated task/birthday matching and offline/revoke recovery.
Cloud configuration/consent remain explicit operator actions, not test automation.

## 8. Documentation impact
Google setup note and root pending-validation statement; Calendar Provider and Google
grants are distinct. Disconnect removes local sharing immediately; offline server revocation may need account settings.

## 9. Risks and approval
Error 10 often indicates OAuth client mismatch; HTTP 403 is not proof of empty sources.
Rate limits, account policies and unavailable task times remain limitations. Ratification pending.

## 10. Validation record
Android build/parser tests and install were reported PASS; actual Google synchronization
was SKIPPED pending setup/consent. Do not promote that to live PASS. No cloud/account calls here.

## 11. History
2026-10-23: retrospective optional integration, not proof of external authorization completion.
