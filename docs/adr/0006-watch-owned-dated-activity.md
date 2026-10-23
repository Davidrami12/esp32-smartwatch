# ADR-0006: Watch-owned dated activity and optional write-only health export

- **Status:** Proposed for retrospective ratification; implemented approach.
- **Recorded:** 2026-10-23; historical formal approval unavailable.
- **Decision owner:** Project maintainer.
- **Related specifications:** [Steps/history](../specs/steps-history/SPEC.md), [Health Connect](../specs/health-connect/SPEC.md), [mobile dashboard](../specs/mobile-activity/SPEC.md).
- **Sources:** `watch_activity.*`, dated BLE extension, Android activity/health modules.
- **Supersedes / superseded by:** None formally recorded.

## Context and drivers
An undated live count is insufficient for trustworthy health export. Phone timezone
or UI zero-fill must not relabel measurements or create observations that never occurred.

## Options considered retrospectively
| Option | Benefit | Cost / risk |
| --- | --- | --- |
| Label count using phone's current date | Simple export | Wrong provenance around midnight/reconnect/timezone changes |
| Export watch-dated aggregates | Preserves origin and intervals | Additive history protocol and retry identity required |
| Read/merge all Health Connect steps | Broader dashboard | Extra permissions and duplicate-source ambiguity outside scope |

## Implemented decision and rationale
Watch activity storage owns calendar dates and up to seven real records. Secured
history extension exports date/count/interval/offset snapshots. Android dashboard
reads these records; optional Health Connect writes use stable app-owned identities
and a durable journal. Display-only missing zeros do not become fabricated records.

## Consequences
- Phone timezone cannot silently relabel watch dates.
- No READ_STEPS permission or alternate phone step source is needed.
- Positive-to-zero correction deletes only the app's matching record; zero/missing
  dates never become fake one-step records.
- Aggregate intervals do not imply precise walking timestamps or medical accuracy.

## Adoption and compatibility
Existing live count remains compatible; dated history/goal characteristics are
optional additions. Automatic Health Connect export defaults OFF.

## Validation
History/day-rollover, dashboard date models and health journal tests; manual SDK
permission/revoke/correction checks. No real health data accessed in this task.

## Open questions and approval history
Step accuracy and long-term reliability are not certified by this architecture.
2026-10-23: retrospective record; formal ratification pending.
