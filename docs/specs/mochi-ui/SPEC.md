# Mochi assistant interaction and text presentation

- **Status:** Ready for approval — retrospective; implemented.
- **Owner:** Project maintainer; historical formal approval unavailable.
- **Last updated:** 2026-10-23.
- **Sources:** `548c9ae`, `4ea7fbe`, `8aead05`, `b60da6e`; shared UI and voice state tests.
- **Related ADR:** [0015](../../adr/0015-display-lifecycle-and-bounded-rendering.md).

## 1. Goal and context
Make a multistage asynchronous voice request understandable on a small display
without moving the assistant's identity or hiding long answers.

## 2. Scope
Mochi naming, face, recording/transfer/processing/result/error states, Clear/Back,
scrolling, Spanish glyphs and interruption handling. Exclude TTS and automatically started recording.

## 3. Requirements (EARS)
| ID | Pattern | Requirement |
| --- | --- | --- |
| MOCHI-01 | Ubiquitous | The face shall retain x=117, y=64 and 176 × 176 geometry, with the name at y=66 and a static halo across states. |
| MOCHI-02 | Event-driven | When request state changes, the UI shall show the corresponding capture/transfer/processing/result/error state without inventing responses. |
| MOCHI-03 | State-driven | While a long response is displayed, the user shall be able to scroll its text independently of fixed navigation. |
| MOCHI-04 | Event-driven | When Clear/Back/interruption invalidates a session, late results shall not replace another screen or resurrect canceled content. |

## 4. Acceptance scenarios
- **AC-01 (MOCHI-01/02):** Given all assistant states, when transitioning between them, then fixed geometry remains stable and expressions/status match the active request.
- **AC-02 (MOCHI-03/04):** Given long Spanish text, when scrolling then canceling or receiving a notification, then text is legible and stale result delivery cannot hijack navigation.

## 5. Constraints and decisions
English interface, Spanish (Spain) default answer content and explicit response-language
overrides. Idle blinks every three seconds; no moving halo. Security/consent is not owned by UI.

## 6. Implementation record
Initial voice screen → expressions and fixed layout → Mochi rename, scrolling and
privacy-safe navigation; firmware/Android drive state through existing callbacks.

## 7. Test plan
FW/SIM/UI; host Spanish glyph, result framing and navigation cases; manual long
answer scroll, idle blink, errors, notifications and offscreen hold entry.

## 8. Documentation impact
Root assistant description; earlier AI assistant notes may predate rename/session restoration.

## 9. Risks and approval
Clipping, unsafe late callbacks and navigation interruption; ratification pending.

## 10. Validation record
Historical physical text response received and host UI tests exist. No new paid
request or visual certification performed for this reconstruction.

## 11. History
2026-10-23: records assistant UI evolution without changing the delivered geometry.
