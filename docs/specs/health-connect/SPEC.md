# Optional Health Connect step export

- **Status:** Ready for approval — retrospective; implemented.
- **Owner:** Project maintainer; historical formal approval unavailable.
- **Last updated:** 2026-10-23.
- **Sources:** `5c17bbf`; Android `health/`, `docs/mobile-health-connect.md`, BLE dated activity.
- **Related ADR:** [0006](../../adr/0006-watch-owned-dated-activity.md).

## 1. Goal and context
Export actual dated watch step records to Android Health Connect without reading
unrelated health data or manufacturing observations for missing dates.

## 2. Scope
WRITE_STEPS permission, manual/optional automatic sync, stable identities and recovery.
Exclude READ_STEPS, phone-sourced steps, invented walking timestamps and fitness cloud accounts.

## 3. Requirements (EARS)
| ID | Pattern | Requirement |
| --- | --- | --- |
| HC-01 | State-driven | While permission is absent/revoked, the companion shall pause exports without launching background consent prompts. |
| HC-02 | Event-driven | When synchronizing, the exporter shall use watch-dated aggregate records and explicit interval/offset metadata. |
| HC-03 | Unwanted behavior | If a date is missing or has zero steps, the exporter shall not fabricate a positive StepsRecord. |
| HC-04 | Event-driven | When a stored count is corrected or synchronization repeats, the exporter shall use stable app-owned identities and its durable journal to avoid duplicates. |

## 4. Acceptance scenarios
- **AC-01 (HC-01/02):** Given write permission and a watch history snapshot, when syncing, then only genuine watch-owned dated intervals are written; revocation pauses further writes.
- **AC-02 (HC-03/04):** Given missing/zero/corrected dates and a repeated or interrupted sync, when retried, then no fake one-step record or duplicate observation is created.

## 5. Constraints and decisions
Automatic sync defaults OFF. Positive-to-zero correction deletes only this app's
matching record identity. Interval coverage is not the exact time the user walked.

## 6. Implementation record
Secured dated history extension `000d` → existing repository → health exporter/journal.
Rationale/privacy activities support Android permission requirements.

## 7. Test plan
ANDROID plus health journal/record tests; manual provider availability, deny/revoke,
repeat sync, process interruption and positive-to-zero correction on a test account.

## 8. Documentation impact
Health Connect feature note, manifest and app privacy guidance.

## 9. Risks and approval
Sensitive activity data, SDK validation and durable retry behavior; ratification pending.

## 10. Validation record
Mock/export tests do not prove real-provider integration on every Android version.
No real health records read or modified by this documentation task.

## 11. History
2026-10-23: retrospective dated export contract and minimal permissions.
