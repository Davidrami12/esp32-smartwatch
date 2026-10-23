# ADR-0004: One companion connection owner and serialized GATT scheduling

- **Status:** Proposed for retrospective ratification; implemented approach.
- **Recorded:** 2026-10-23; historical formal approval unavailable.
- **Decision owner:** Project maintainer.
- **Related specifications:** [Android](../specs/android-companion/SPEC.md), [notifications](../specs/notifications/SPEC.md), [media](../specs/media/SPEC.md).
- **Sources:** Android `ble/WatchRepository.kt`, companion service and feature bridges.
- **Supersedes / superseded by:** None formally recorded.

## Context and drivers
Android expects correlated, serialized GATT operations. Independent voice/media/
calendar clients could corrupt callbacks, compete for the radio or duplicate services.

## Options considered retrospectively
| Option | Benefit | Cost / risk |
| --- | --- | --- |
| Separate BLE owner per feature | Local isolation | Races, duplicate connections and ambiguous callbacks |
| Existing repository plus feature bridges | Shared lifecycle and security | Queue fairness and bounded feature work are required |
| Arbitrary parallel GATT operations | Apparent throughput opportunity | Unsupported callback attribution and unreliable ordering |

## Implemented decision and rationale
Keep one repository/foreground-service path responsible for the watch connection,
discovery, operation queue and state. Feature bridges consume/produce snapshots
through it. Voice uses bounded pacing; media/notifications retain turns and calendar
is lower priority. User permission prompts originate from foreground UI, not background work.

## Consequences
- One recovery/readiness model and no second voice/calendar foreground service.
- GATT throughput is subject to scheduling; fairness needs explicit regression tests.
- A single owner has meaningful complexity and must reject stale operation/session callbacks.
- OEM background/battery policies can still interrupt service continuity.

## Adoption and compatibility
Existing base controls stay intact as optional extensions are discovered. No
separate media/calendar transport or new permissions are justified merely for queueing.

## Validation
Android queue/protocol tests and physical concurrent voice/media/notification checks.
Calendar must yield. Historic tests exist; no new radio performance claims or runs here.

## Open questions and approval history
Future queue redesign would require renewed contract/fairness review.
2026-10-23: retrospective implementation record; formal ratification pending.
