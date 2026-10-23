# Development, physical validation and delivery evidence

- **Status:** Ready for approval — retrospective documentation of existing workflow.
- **Owner:** Project maintainer; historical formal approval unavailable.
- **Last updated:** 2026-10-23.
- **Sources:** `0987b90`, `6199767`, `1558a18`; root `AGENTS.md`, manifests and host/mobile/backend tests.
- **Related ADR:** None; this is operational process rather than a new product architecture.

## 1. Goal and context
Make feature completion distinguish compilation, mocked checks, real-device
behavior and user interaction. Preserve evidence without treating a single benchmark as a guarantee.

## 2. Scope
Firmware/simulator/Android/backend checks, diff quality, bounded boot logs and manual
follow-up. Existing agent harness/templates are tooling, not shipped watch behavior;
their configuration remains untouched and is described in `docs/agent-harness.md`.

## 3. Requirements (EARS)
| ID | Pattern | Requirement |
| --- | --- | --- |
| QA-01 | Event-driven | When a firmware feature is completed, validation shall include firmware build and relevant shared-UI/simulator checks, not compilation alone. |
| QA-02 | State-driven | While the physical watch is available, applicable validation shall include flash and a bounded runtime log inspection, leaving the serial port released afterward. |
| QA-03 | Unwanted behavior | If hardware/account/cloud prerequisites are unavailable, reports shall state SKIPPED/blockers rather than fabricate PASS. |
| QA-04 | Ubiquitous | Validation shall avoid automatic paid AI, cloud changes, flash erasure, commits/pushes or destruction of unrelated user work without authorization. |
| QA-05 | Ubiquitous | Reports shall separate measured, simulated and historical evidence and list manual physical tests and known warnings. |

## 4. Acceptance scenarios
- **AC-01 (QA-01/02):** Given a shared UI change and connected COM9 watch, when finishing, then builds/tests, flash, bounded boot review and remaining manual checks are recorded.
- **AC-02 (QA-03/04/05):** Given unavailable Google consent or hardware, when summarizing, then that area is marked SKIPPED without triggering paid/cloud operations or overstating mock evidence.

## 5. Constraints and decisions
Commands/prerequisites live in the [catalogue](../README.md#shared-verification-commands).
Current instructions allow releasing an identified COM9 ESP-IDF monitor, not killing
unrelated processes or an active flash. Documentation-only work needs no hardware flash.

## 6. Implementation record
Repository instructions and test targets evolved alongside features. Retrospective
SPEC/ADR creation records existing decisions; it does not authorize implementation or deployment.

## 7. Test plan
DIFF and documentation local-link/anchor/requirement coverage checks for this task.
Future code changes use the applicable firmware/mobile/backend gates and explicit review requirements.

## 8. Documentation impact
SPEC and ADR indices provide source/decision links; templates remain unchanged.
Do not overwrite original technical notes merely to hide their historical context.

## 9. Risks and approval
Stale documentation, false green reports and unclear artifact identity; formal
retrospective ratification remains pending, not fabricated from commit existence.

## 10. Validation record
Historical project PASS reports are summarized in root README, with scope limits.
This reconstruction checked Markdown links/anchors, sections, requirement-to-scenario
coverage, commit references and whitespace successfully, plus `git diff --check`
(exit 0). See the catalogue's reconstruction validation record; no binary validation is implied.

## 11. History
2026-10-23: documents original repository workflow and evidence rules without modifying the agent harness.
