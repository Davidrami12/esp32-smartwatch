# ADR-0010: Bounded weekly calendar snapshots and PSRAM cache ownership

- **Status:** Proposed for retrospective ratification; implemented approach.
- **Recorded:** 2026-10-23; historical formal approval unavailable.
- **Decision owner:** Project maintainer.
- **Related specification:** [Calendar](../specs/calendar/SPEC.md).
- **Sources:** `634347f`; CalendarBridge/Protocol, shared receiver/UI and firmware handoff.
- **Related ADRs:** [0003](0003-native-ble-security-and-additive-contracts.md), [0011](0011-optional-read-only-google-agenda.md).
- **Supersedes / superseded by:** None formally recorded.

## Context and drivers
A 42-cell month spans six weeks, but transferring an entire unbounded calendar
would expose excess private data and exceed small-device memory. Initial internal
calendar allocations caused display DMA starvation/watchdog stalls during development.

## Options considered retrospectively
| Option | Benefit | Cost / risk |
| --- | --- | --- |
| Whole calendar stored on watch | Broad offline search | Unbounded memory/privacy/storage scope |
| One-day payloads only | Small request | Repeated queries and poor month/week reuse |
| Six bounded weekly RAM snapshots | Month coverage and reuse | Overflow/freshness and serialized request management |

## Implemented decision and rationale
Android expands provider instances into Madrid week masks; watch serializes six
week requests and caches up to 32 events each. Allocate cache, assembler and handoff
buffers in PSRAM, preserving scarce internal/DMA memory. Secured optional `0015`
uses request IDs, contiguous offsets, bounded length, CRC32 and assembly timeout.
Selection/permission/link identity changes clear queued/cached private content.

## Consequences
- Reboot clears data; disconnected watch can retain RAM until reconnect/reboot.
- Overflow and partial/unavailable states must be visible; absent data is not proof of an empty day.
- Provider scan continues for holiday masks even after agenda slots fill.
- Month and whole-week UI reuse the same contract; date taps do not require a new daily transport.

## Adoption and compatibility
Requests v1–v4 negotiate color/holiday/error semantics; framing stays v1 and current
payload schema 2. Google auth status 5 maps to legacy permission status 2 for old requesters.
Exact 7,000-byte/120-second bounds and layouts are in the BLE protocol.

## Validation
Host receiver, CRC/stale/overflow/date/style/navigation tests and Android protocol tests;
physical boot after PSRAM fix reported without stalls in bounded capture. Actual calendars,
memory stress and revocation behavior remain distinct manual checks. No reruns here.

## Open questions and approval history
No durable calendar database or universal duplicate resolver is included.
2026-10-23: retrospective memory/contract record; formal ratification pending.
