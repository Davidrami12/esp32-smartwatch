---
description: Plan and implement a feature using the project's SDD workflow
agent: leader
---

Feature request:

$ARGUMENTS

Start in **Plan Mode**: a no-edit requirements phase, not an agent switch.

## 1. Feature interview

Inspect existing code, documentation, conventions, and related specifications before
asking questions the repository answers. Interview the user only about relevant
unresolved goals, expected behavior, scope/non-goals, UX/flows, rules, security,
data/contracts, edge/error handling, constraints, and acceptance. Continue until
requirements are clear enough to implement. Do not modify files during this phase.

## 2. Prepare the feature

Create/update the specification at the destination's configured location (default
`docs/specs/<feature-slug>/SPEC.md`) using `docs/specs/TEMPLATE.md` where available.
Define normative requirements in EARS with stable IDs and observable responses.
Link Given/When/Then acceptance scenarios to those requirements. Define the
implementation approach, affected files/components, test plan, quality gates,
documentation impact, risks, and unresolved decisions. Follow repository conventions
and the shared workflow. This preparation phase may write the spec, not implementation.

## 3. Approval gate

Present a concise plan: scope; architecture/approach; affected files; tests;
verification; documentation; risks/open questions. Do not implement until the human
approves. Resolve meaningful architecture decisions through Architect and Leader.

## 4. Implementation

After approval delegate appropriate work, implement the approved SPEC, add/update
tests, update relevant documentation, run required gates, obtain independent review,
and verify the final artifact against acceptance. Maintain EARS requirement ->
acceptance scenario -> implementation artifact -> validation evidence traceability.
If implementation requires changing approved scope or SPEC, stop
affected work and ask for approval.

Finish with changes, tests added/updated, actual verification results, documentation
updates, and remaining issues/deviations. Do not claim unperformed validation.
