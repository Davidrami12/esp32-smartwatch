# ADR-0011: Separate optional Google authorization for birthdays and Tasks

- **Status:** Proposed for retrospective ratification; implemented approach, live setup conditional.
- **Recorded:** 2026-10-23; original formal approval record unavailable.
- **Decision owner:** Project maintainer and Google account/project operator.
- **Related specification:** [Google agenda](../specs/google-agenda/SPEC.md).
- **Sources:** GoogleAgendaAuth/Client/Data/Settings and `docs/google-agenda.md`.
- **Related ADR:** [0010](0010-bounded-calendar-snapshots-in-psram.md).
- **Supersedes / superseded by:** None formally recorded.

## Context and drivers
Calendar Provider selection does not expose every cloud-only birthday/task source.
Firebase sign-in grants no Google Calendar/Tasks scope, and Tasks has a date-only
API that cannot reproduce the scheduled hour shown in first-party apps.

## Options considered retrospectively
| Option | Benefit | Cost / risk |
| --- | --- | --- |
| Relax local sync filter only | Reveals cached local calendars | Cannot create absent cloud sources |
| READ_CONTACTS birthdays | Local contact birthdays | Broader personal-data permission; not equivalent to all Google events/tasks |
| Explicit read-only Calendar/Tasks OAuth | Correct service-specific sources | OAuth package/certificate/API setup and separate consent |

## Implemented decision and rationale
Keep local provider support, list sync-OFF calendars and add optional GIS account
authorization with Calendar/Tasks read-only scopes. No sources are auto-selected.
Google Play services manages grants/refresh; app tokens remain short-lived in memory.
GET-only bounded fixed-origin HTTPS reads birthdays and selected dated pending Tasks,
then merges through the existing secured calendar snapshots, not the AI backend.

## Consequences
- No contacts/write/calendar credentials are sent over BLE or embedded in the APK.
- Undated/completed tasks are excluded; no scheduled hour is invented.
- Account/source changes invalidate stale work; disconnect clears local sharing and requests revocation.
- Duplicate suppression by provider NAME/Google ID is best-effort; user source selection still matters.
- Live function depends on OAuth Android package/SHA-1, enabled APIs and user consent.

## Adoption and compatibility
Local calendars work without Google selection. Status 5 requires request v4 and
maps to legacy permission status for older watches. Cloud/IAM/secret changes are
not automatically performed by application code or this retrospective task.

## Validation
Synthetic parser/protocol tests and Android build/install passed historically.
Real Google synchronization remains unvalidated pending setup/consent; do not mark
account integration PASS based on JSON fixtures or an APK build.

## Open questions and approval history
External OAuth configuration/account consent remains the operator's responsibility.
2026-10-23: retrospective record, formal ratification pending; no Google account inspected here.
