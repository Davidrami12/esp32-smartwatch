# ADR-0012: Offline approximate astronomy with bounded schematic rendering

- **Status:** Proposed for retrospective ratification; implemented approach.
- **Recorded:** 2026-10-23; historical formal approval unavailable.
- **Decision owner:** Project maintainer.
- **Related specifications:** [Sun & Moon](../specs/sun-moon/SPEC.md), [Planets](../specs/planets/SPEC.md).
- **Sources:** `9d9b549`, `28cba95`, `a611f89`; astronomy helpers and feature notes.
- **Supersedes / superseded by:** None formally recorded; heavier detailed planet UI was reverted.

## Context and drivers
Astronomy should work offline on a small embedded screen. Detailed ephemeris/network
services and large geometry workloads are unnecessary for an educational watch app.

## Options considered retrospectively
| Option | Benefit | Cost / risk |
| --- | --- | --- |
| Remote ephemeris/location service | Potential precision/configurability | Network, privacy and service dependency |
| Local approximations and schematic UI | Offline, bounded work | Explicit date/location/precision limitations |
| Detailed animated/elliptical planet renderer | Rich geometry | Prior detailed variant harmed responsiveness and was reverted |

## Implemented decision and rationale
Sun/Moon uses fixed Madrid coordinates and local solar/mean lunar estimates.
Planets uses bounded JPL approximate Keplerian calculations and schematic circular
orbit spacing. Calculate on relevant entry/time/wake events, not continuous network
polling or orbital animation. Keep eight names and bounded label placement visible.

## Consequences
- No GPS permission, API secret or astronomy cloud cost.
- Planet rings/distances/sizes are not to scale, and the view is heliocentric, not local sky visibility.
- Lunar event estimates can differ by about a day; solar plausibility tests are not certified error bounds.
- Fixed location, supported date intervals and third-party attribution remain explicit.

## Adoption and compatibility
Uses existing watch clock. Sun/Moon supports 2000–2099; Planets 2000–2050.
Retain app-pixels MIT notice and JPL formula references; no new time/location service.

## Validation
Portable astronomy tests, host layout and manual readability/wake checks. No
scientific/navigation-grade accuracy claim and no new ephemeris comparison performed here.

## Open questions and approval history
Configurable worldwide location or higher precision would be new scope.
2026-10-23: retrospective design record; formal ratification pending.
