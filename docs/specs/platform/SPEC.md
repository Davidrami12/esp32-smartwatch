# Platform bring-up and hardware integration

- **Status:** Ready for approval — retrospective specification; implemented.
- **Owner:** Project maintainer; historical formal approver not recorded.
- **Last updated:** 2026-10-23.
- **Sources:** `f1c2478`, `364ace2`, `5c6aecd`; `apps/watch/main/main.cpp`.
- **Related ADR:** [0001](../../adr/0001-platform-and-shared-ui.md).
- **Approved scope and plan:** Reconstruction requested by the user; no original signed plan is asserted.

## 1. Goal and context
Bring up the Waveshare ESP32-S3 Touch AMOLED 2.06 as a functioning watch rather
than a standalone display demo. Later modules reuse the board's initialized buses.

## 2. Scope
Include display/touch, PMU, RTC, motion and audio integration. Exclude custom PCB
design, automatic flash erasure, microSD recording and generic-board portability.

## 3. Requirements (EARS)
| ID | Pattern | Requirement |
| --- | --- | --- |
| PLAT-01 | Ubiquitous | The firmware shall target the Waveshare ESP32-S3 board and its 410 × 502 display. |
| PLAT-02 | Event-driven | When startup initializes peripherals, the firmware shall use platform adapters rather than embedding board drivers in shared UI. |
| PLAT-03 | Unwanted behavior | If initialization fails, the firmware shall expose diagnostic failure rather than report fabricated sensor success. |
| PLAT-04 | Ubiquitous | The build shall keep credentials out of tracked source and treat managed components as dependencies. |

## 4. Acceptance scenarios
- **AC-01 (PLAT-01/02):** Given the target board, when the firmware boots, then the display/touch and configured peripherals initialize through the platform layer.
- **AC-02 (PLAT-03/04):** Given a missing peripheral or private build configuration, when startup/build is inspected, then failures remain diagnosable and credentials are not published.

## 5. Constraints, dependencies and decisions
ESP-IDF 6.1, Waveshare BSP 2.x, LVGL 9.5.0 and board I2C/audio drivers.
Detected 32 MB flash versus configured 16 MB is a known warning, not a storage expansion feature.

## 6. Implementation record
Initial firmware and power support were followed by modular adapters in
`apps/watch/main/watch_{display_start,power,rtc,motion,voice}.*`. This is historical
execution, not a new implementation plan.

## 7. Test plan and quality gates
Use FW, FLASH and BOOT from the [catalogue](../README.md#shared-verification-commands).
Physically inspect touch, battery readings and unavailable-peripheral diagnostics.

## 8. Documentation and operational impact
Root README, board manifest and repository build instructions are canonical setup references.

## 9. Risks and approval
Board-specific assumptions, DMA/internal-memory pressure and hardware variation remain relevant.
Historical formal approval unavailable; retrospective approval pending.

## 10. Validation and review record
Recent project boot/build checks were reported successful; not rerun for this document.
Physical certification and exhaustive peripheral fault injection are not claimed.
Independent review of this retrospective record: not requested.

## 11. Change history
2026-10-23: reconstructed from source and Git history; no implementation changes.
