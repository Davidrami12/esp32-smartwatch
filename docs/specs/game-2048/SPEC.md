# 2048 game and saved best score

- **Status:** Ready for approval — retrospective; implemented.
- **Owner:** Project maintainer; historical formal approval unavailable.
- **Last updated:** 2026-10-23.
- **Sources:** `e6e5b91`; `watch_2048.*`, firmware `watch_2048_store.*`, `docs/watch-2048.md`.
- **Related ADR:** [0014](../../adr/0014-pure-game-engine-and-platform-storage.md).

## 1. Goal and context
Provide a self-contained touch game without adding network, storage or LVGL
dependencies to the game engine.

## 2. Scope
4 × 4 board, score/best, win/game-over, gestures and platform persistence.
Exclude online leaderboards, external game assets and extra allocation in the engine.

## 3. Requirements (EARS)
| ID | Pattern | Requirement |
| --- | --- | --- |
| GAME-01 | Event-driven | When a valid directional move occurs, the engine shall compact tiles, merge each original pair at most once and spawn one new tile. |
| GAME-02 | Unwanted behavior | If a move is ineffective, the engine shall leave score, RNG and spawning unchanged. |
| GAME-03 | State-driven | While the game/dialog owns input, directional gestures shall not trigger normal Back navigation or duplicate moves. |
| GAME-04 | Event-driven | When a new game is confirmed, the engine shall reset progress while retaining best-score semantics and numeric safety bounds. |

## 4. Acceptance scenarios
- **AC-01 (GAME-01/02):** Given fixed boards/RNG, when all four moves and blocked moves run, then deterministic merges/spawns and unchanged invalid moves are observed.
- **AC-02 (GAME-03/04):** Given a win/game-over or new-game dialog, when gestures/confirmation occur, then only intended actions execute and explicit Back remains usable.

## 5. Constraints and decisions
Fixed arrays, xorshift test determinism, uint32 saturation and tile upper bound.
Firmware supplies NVS best-score persistence; desktop best is process-local.

## 6. Implementation record
Pure engine, shared board/dialog UI and firmware persistence adapter were separated.

## 7. Test plan
`tests/watch_2048_test.cpp`, `tests/watch_2048_store_test.cpp`, UI host game cases;
FW/SIM/UI and manual four-direction touch/dialog/reboot checks.

## 8. Documentation impact
`docs/watch-2048.md`; launcher tile 6 remains the final placement.

## 9. Risks and approval
Swipe bubbling and persistence failure require regression coverage; ratification pending.

## 10. Validation record
Deterministic engine/storage/UI tests exist. Not rerun for documentation; no online
game service or score certification is claimed.

## 11. History
2026-10-23: retrospective game requirements and storage boundary.
