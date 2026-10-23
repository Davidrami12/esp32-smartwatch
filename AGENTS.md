# Repository and OpenCode instructions

## Boundaries and sources of truth

- Four independent projects: ESP-IDF firmware (`apps/watch`), SDL2 simulator (`apps/watch-simulator`), Android/Gradle (`apps/mobile`), and FastAPI (`apps/voice-backend`). There is no root application build.
- Firmware entrypoint is `apps/watch/main/main.cpp`; both CMake builds compile `components/watch-ui/watch_ui.cpp`. Keep shared UI free of platform HTTP/NVS/driver work; firmware adapters and Android `ble/WatchRepository.kt` own those effects and connection scheduling.
- Treat `apps/watch/managed_components/` as dependency-managed. Simulator LVGL is the `apps/watch-simulator/lvgl` submodule: initialize with `git submodule update --init --recursive`.
- Read the relevant `docs/specs/<feature>/SPEC.md` and `docs/adr/` record before changing behavior/contracts. Their indices distinguish retrospective reconstruction from approval: implemented code does not mean a reconstructed SPEC/ADR was formally ratified. Older feature notes sometimes describe superseded behavior; verify against source/manifests.
- Weather runs on watch Wi-Fi, not through Android. Calendar and voice reuse the existing secured BLE connection; do not introduce a second connection/service. Calendar caches/assembler/handoff belong in PSRAM: internal allocations previously starved display DMA.
- Preserve native authenticated/encrypted bonding, lossless PCM/CRC, bounded retries/cancellation and legacy voice fallback. Google calendar grants are separate from Mochi authentication; mock tests or configured OAuth do not establish live account access.

## Commands and prerequisites

Run each command in its project directory; these are procedures, not native permission grants.

| Directory | Command / non-obvious requirement |
| --- | --- |
| `apps/watch` | Activate ESP-IDF 6.1, then `idf.py build`. Flash/monitor use `idf.py -p COM9 flash` and `idf.py -p COM9 monitor`. |
| `apps/watch-simulator` | Configure with Visual Studio 2019, SDL2/vcpkg and `USE_FREERTOS=OFF`; exact configure command is in its README. FreeRTOS simulation is unsupported. |
| `apps/watch-simulator` | `cmake --build build --config Debug --target main watch_ui_tools_tests`; run `.\bin\Debug\watch_ui_tools_tests.exe` and `.\bin\Debug\main.exe`. |
| `apps/watch-simulator` | Tests are `EXCLUDE_FROM_ALL`: explicitly build `watch_2048_tests`, `watch_2048_store_tests`, `watch_astronomy_tests` or `watch_ui_tools_tests` before CTest. Focused UI check: `ctest --test-dir build -C Debug -R "^watch_ui_integration$" --output-on-failure`. |
| `apps/mobile` | JDK 17 / Android SDK 36 (min device API 33): `.\gradlew.bat :app:assembleDebug :app:testDebugUnitTest -PvoiceNotify=true`. |
| `apps/mobile` | Focused test: `.\gradlew.bat :app:testDebugUnitTest --tests "dev.esp32.smartwatch.calendar.CalendarProtocolTest" -PvoiceNotify=true`. APK: `app/build/outputs/apk/debug/app-debug.apk`. |
| `apps/mobile` | `voiceNotify` defaults **false** in Gradle. Use `-PvoiceNotify=true` for notification transport; add `-PvoiceTransferOnly=true` to build unpaid transport tests that block voice HTTP. |
| `apps/voice-backend` | In a Python virtualenv: `python -m pip install -r requirements-dev.txt`, then `python -m pytest`; focused example: `python -m pytest tests/test_language_policy.py`. Tests mock external services. |
| repository root | `git diff --check`. Documentation-only work needs document checks, not hardware flashing. |

- `apps/watch/CMakeLists.txt` loads optional `apps/watch/.env` and embeds `WIFI_SSID`/`WIFI_PASSWORD` in the image. Do not publish that file or credential-bearing firmware.
- Android's ignored `ai.local.properties` is for the **public Firebase Web key only**, never passwords, tokens or OpenAI keys. OpenAI credentials stay server-side; see backend setup before touching deployment.
- No automatic paid AI calls, cloud/IAM/secret changes, flash erase or deployment. Do not commit/push without an explicit request; the shipped role permissions deny them, and a request does not bypass a native deny.

## Feature completion and device safety

For firmware features, unless explicitly excluded: build firmware, build/test the simulator if shared UI is affected, run `git diff --check`, then flash the connected watch. Also run the affected Android/backend checks; compilation alone is not runtime acceptance.

- Default watch port: **COM9**. If busy, you may stop a safely identified ESP-IDF monitor/process tree using COM9 and retry. Stop the monitor rather than its terminal; never kill unrelated processes or an active flash. If ownership is uncertain or retry fails, report the blocker.
- After flash, capture a bounded monitor window when runtime validation is useful; inspect crashes, watchdogs, initialization failures and feature regressions. Stop the monitor cleanly to release COM9.
- Missing hardware/COM9 means physical validation **SKIPPED**, with reason, not automatic implementation failure. Never substitute a host mock for hardware/production evidence.
- Report firmware build, simulator build, diff check, device flash and runtime boot as **PASS / FAIL / SKIPPED / N/A**, as applicable; include affected mobile/backend checks, changed files, warnings and manual touch/walking/reboot/visual tests still required.
- Known warnings include detected 32 MB flash versus configured 16 MB, BLE/light-sleep restrictions and LVGL development/deprecation messages; distinguish these from new regressions.

## Orchestration and approvals

- Only **Leader** orchestrates. Delegate useful scoped work, not every role by default. **Coder** owns implementation/tests/configuration and explicitly requested harness edits; **Architect** is read-only and recommends consequential boundaries/contracts/security decisions.
- **Reviewer** independently reviews material generated implementation, never edits fixes. **Guardian** updates permitted documentation and records approved ADRs, never chooses architecture; **Auditor** is direct user `@auditor` only, not an automatic task.
- Leaf agents do not delegate or interview the user; return consequential questions to Leader. Assign disjoint files for concurrent writes. Handoffs carry directory, revision + dirty state, outcome/acceptance, approved scope, owned files/operations, relevant decisions, checks/prerequisites and blockers.
- Native `.opencode/agents/*.md` permissions and plugin guards bound authority. Leader's document grant is `docs/*`; Guardian adds root `README.md`, **not AGENTS.md**. Repository instructions/agent/config edits require the authorized Coder route; do not evade guards with another tool, shell wrapper or agent.
- Reuse canonical specs and `docs/specs/TEMPLATE.md`: stable EARS requirement IDs, explicit bounds, linked Given/When/Then acceptance and requirement-to-check evidence. ADRs belong in `docs/adr/`; never silently supersede accepted decisions or invent approvals.
- `/sdd-feature`: inspect/clarify without edits → write SPEC and implementation/test plan only → obtain explicit human approval → implement, verify, independently review and document. Scope/SPEC changes need renewed approval for affected work. Small established changes need no ceremonial architecture/spec; spec-only requests stop at documentation.
- Read-only roles obtain artifact-writing test/build evidence from Coder unless an exact non-writing command was inspected and permitted. Commands, scripts/config and side effects must be checked; shell flags/operators cannot bypass exact-command restrictions.
- Evidence includes command, directory, exit status, relevant output and checked revision + dirty state (or hashes). Revalidate checks affected by source/test/config/dependency/environment changes; label missing prerequisites, skipped checks and pre-existing failures explicitly.
- Reviewer verdicts: `APPROVED`, `CHANGES_REQUIRED`, `BLOCKED`. BLOCKER/MAJOR findings require Coder correction, affected checks and re-review; MINOR/NIT alone does not force a cycle. After two unsuccessful cycles, reassess design or ask for missing human input rather than blindly retrying.
- Guardian returns `UPDATED`, `NO_UPDATE_NEEDED`, `BLOCKED` or `ARCHITECTURE_REVIEW_REQUIRED`; these do not replace Leader's final acceptance check. Detailed workflow: `docs/agent-harness.md`; do not copy the generic installation tutorial into project instructions.

## Harness setup caveats

- Root `opencode.json` currently points `instructions` at **missing `.opencode/AGENTS.md`**. This root file is the repository instruction source; do not assume the extra configured file exists or silently change config during unrelated work. `.opencode/scripts/verify-harness.mjs` currently resolves its root to `.opencode/` and reads a missing `.opencode/opencode.json`; it also assumes the older Context7 config shape. `node .opencode/scripts/verify-harness.mjs` is the intended check but needs alignment before it can validate this tree. Do not claim harness PASS from its presence or fix it outside approved scope.
- `.opencode/package.json` owns the harness verify script; there is no root npm application. Inspect verifier side effects before running it or `--native`; native discovery uses a temporary copy and is not proof of live delegation/model availability.
- Root config has Context7 under `mcp.servers` with a remote URL, not the API-key header setup. Do not invent credentials/configuration. Inspect local dependency versions, then use Context7 for current/version-specific external API docs; never send secrets or confidential source in queries.
