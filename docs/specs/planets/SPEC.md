# Schematic heliocentric Planets app

- **Status:** Ready for approval — retrospective; implemented.
- **Owner:** Project maintainer; historical formal approval unavailable.
- **Last updated:** 2026-10-23.
- **Sources:** `28cba95`, `a611f89`; shared astronomy/UI helpers and `docs/planets.md`.
- **Related ADR:** [0012](../../adr/0012-offline-schematic-astronomy.md).

## 1. Goal and context
Show recognizable approximate planetary positions while keeping a small watch
responsive and labels legible. A detailed elliptical/zoom version was reverted.

## 2. Scope
Eight named planets, schematic circular rings and local approximate calculations.
Exclude to-scale distances/sizes, continuous orbit animation, sky visibility and tap-to-select.

## 3. Requirements (EARS)
| ID | Pattern | Requirement |
| --- | --- | --- |
| PLANET-01 | Event-driven | When entering/waking with a valid supported date, the app shall calculate approximate heliocentric positions locally. |
| PLANET-02 | Ubiquitous | The map shall show eight color-matched planet names and distinguish Earth with a white outline. |
| PLANET-03 | Unwanted behavior | If the clock is invalid or outside 2000–2050, the map shall not show apparently valid planetary positions. |
| PLANET-04 | Ubiquitous | Orbit and label rendering shall use bounded native primitives/search rather than an unbounded per-frame geometry workload. |

## 4. Acceptance scenarios
- **AC-01 (PLANET-01/02):** Given a supported date, when opening the app, then eight labeled planets appear with stable colors and Earth identification.
- **AC-02 (PLANET-03/04):** Given an invalid date or closely aligned positions, when rendering, then unavailable state or bounded placement applies without a large segment-based redraw workload.

## 5. Constraints and decisions
JPL approximate Keplerian elements/rates, bounded Newton solve, UTC approximating TDB,
Earth/Moon barycenter approximation. Rings and planet sizes are deliberately not to scale.

## 6. Implementation record
Shared approximate longitude helpers; compressed circular-orbit layout, bounded label
placement and eight native arcs replaced the heavier detailed variant.

## 7. Test plan
Astronomy/host UI tests plus FW/SIM; manual inspect names/overlap across dates and wake.

## 8. Documentation impact
`docs/planets.md` cites JPL coefficients and explicitly separates schematic from physical scale.

## 9. Risks and approval
Label overlap and approximation limitations; formal ratification pending.

## 10. Validation record
Calculation/layout regressions exist; not a certified ephemeris or visibility tool.
No new accuracy/latency measurement performed here.

## 11. History
2026-10-23: records initial app and responsiveness-driven simplification.
