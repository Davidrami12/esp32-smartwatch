# ADR-0014: Deterministic 2048 engine separated from LVGL and storage

- **Status:** Proposed for retrospective ratification; implemented approach.
- **Recorded:** 2026-10-23; historical formal approval unavailable.
- **Decision owner:** Project maintainer.
- **Related specification:** [2048](../specs/game-2048/SPEC.md).
- **Sources:** `e6e5b91`; shared `watch_2048.*`, firmware `watch_2048_store.*` and tests.
- **Supersedes / superseded by:** None formally recorded.

## Context and drivers
Game rules need deterministic testing and bounded memory, while gesture behavior
and persistent best scores belong to different platform/lifecycle concerns.

## Options considered retrospectively
| Option | Benefit | Cost / risk |
| --- | --- | --- |
| Rules embedded in LVGL handlers | Fewer files | Hard to test merges/RNG without display setup |
| External game framework | Existing abstraction | Dependency/memory complexity for a 4 × 4 board |
| Fixed-size pure engine with adapters | Deterministic host tests and no heap | Explicit UI/storage integration required |

## Implemented decision and rationale
Keep tiles/score/RNG/lifecycle in a fixed-size pure engine with no ESP-IDF, LVGL or
storage dependency. Inject seed from platform; test with fixed seeds. Shared UI
owns drawing/dialogs and consumes all four game gestures before normal navigation.
Firmware adapter owns best-score NVS; simulator best remains process-local.

## Consequences
- Blocked moves can be verified not to consume RNG/spawn or alter score.
- Merge-once, win/game-over and saturation bounds are independently testable.
- Right swipe in the game is a move, so explicit Back is required.
- Storage failures cannot be confused with engine rule failures.

## Adoption and compatibility
No network/API/health permissions and no external game library. Preserve existing
best-score schema and new-game confirmation. Desktop persistence differs explicitly.

## Validation
Engine/store host tests and LVGL gesture/dialog cases; manual real swipes and reboot
best-score retention. Existing tests are not rerun for this retrospective record.

## Open questions and approval history
No leaderboard, persistent full-board restoration or remote service is introduced.
2026-10-23: reconstructed engine boundary; formal ratification pending.
