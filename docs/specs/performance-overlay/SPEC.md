# Shared UI performance overlay

- **Status:** Ready for approval — retrospective; implemented when enabled by LVGL configuration.
- **Owner:** Project maintainer; historical formal approval unavailable.
- **Last updated:** 2026-10-23.
- **Sources:** `components/watch-ui/watch_perf_monitor.h`, firmware/simulator integration.
- **Related ADR:** [0015](../../adr/0015-display-lifecycle-and-bounded-rendering.md).

## 1. Goal and context
Expose lightweight display diagnostics during development without modifying managed
LVGL or mislabeling its handler idle measurement as whole-device CPU utilization.

## 2. Scope
FPS, LVGL busy percentage and average render-plus-flush time in a shared overlay.
Exclude whole-device profiling, power measurement and guaranteed physical scanout latency.

## 3. Requirements (EARS)
| ID | Pattern | Requirement |
| --- | --- | --- |
| PERF-01 | Optional feature | Where LVGL sysmon/performance monitoring is enabled, the adapter shall show FPS, handler busy percentage and render-plus-flush average milliseconds. |
| PERF-02 | Ubiquitous | The displayed CPU percentage shall use LVGL handler busy time rather than claim unsupported FreeRTOS whole-device idle accounting. |
| PERF-03 | Optional feature | Where monitoring is disabled or no display exists, monitor creation shall be a safe no-op. |
| PERF-04 | Ubiquitous | The overlay shall not consume touch input or require edits to managed LVGL dependencies. |

## 4. Acceptance scenarios
- **AC-01 (PERF-01/02):** Given sysmon enabled and an active display, when its subject updates, then bounded numeric diagnostics render and their documented scope is UI workload only.
- **AC-02 (PERF-03/04):** Given disabled monitoring or user touches at the overlay, when creating/interacting, then no missing-display failure or input interception occurs.

## 5. Constraints and decisions
LVGL-private adapter is isolated in one header because it observes internal sysmon
state shared by both builds. Bottom-centered white text on translucent black uses
the system layer and removes CLICKABLE. This is a version-sensitive integration.

## 6. Implementation record
Shared helper creates a label/observer when configured; no independent radio,
network, recording or power-management path is introduced.

## 7. Test plan
FW/SIM/UI compile compatibility; manual overlay updates and unobstructed touches.
Check the adapter on LVGL upgrades; numeric diagnostics are not end-to-end benchmarks.

## 8. Documentation impact
This SPEC qualifies what CPU/FPS/ms values mean; latency notes retain separate
BLE/backend/display measurement boundaries.

## 9. Risks and approval
Private LVGL interfaces can change and overlay work has overhead; ratification pending.

## 10. Validation record
Shared builds historically compiled this helper. Whole-device utilization, power
and physical scanout are not established by these values. No fresh run in this task.

## 11. History
2026-10-23: retrospective diagnostics specification; not a new profiling implementation.
