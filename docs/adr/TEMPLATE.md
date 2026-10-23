# ADR-<NNNN>: <Decision title>

<!--
Copy to docs/adr/<NNNN>-<short-decision-slug>.md using the next available number.
Keep this template unchanged. Replace placeholders and remove guidance comments.
Create an ADR only for a meaningful, durable architectural decision. Routine local
implementation choices do not need one. Architect proposes decision content;
Leader obtains required human approval; Guardian records approved content.
-->

- **Status:** Proposed <!-- Proposed | Accepted | Rejected | Superseded -->
- **Proposed on:** <YYYY-MM-DD>
- **Decision owner:** <person or team accountable for the decision>
- **Approval:** Pending <!-- Record actual approver, date, and reference when granted. -->
- **Related specification:** <path or reference, or None>
- **Related ADRs:** <references, or None>
- **Supersedes:** None
- **Superseded by:** None

## Context and problem

<Describe the problem, current architecture, and why a decision is needed now.
Separate verified facts from assumptions and unresolved questions.>

## Decision drivers and constraints

- <Accepted requirement or acceptance criterion affected.>
- <Relevant boundary, contract, data ownership, security, reliability, or compatibility constraint.>
- <Operational, migration, dependency, cost, or delivery constraint where material.>

## Options considered

| Option | Benefits | Costs and risks | Meets constraints? |
| --- | --- | --- | --- |
| Keep the existing approach | <benefits> | <limitations> | <assessment> |
| <Recommended option> | <benefits> | <tradeoffs> | <assessment> |
| <Other realistic option, if needed> | <benefits> | <tradeoffs> | <assessment> |

## Decision

<While Proposed, state the recommendation, not an approved fact. Once Accepted,
state the chosen approach and its scope. Define responsibilities, affected contracts,
data/trust/transaction boundaries, and ownership only as deeply as implementation
needs. Explicitly identify what is outside the decision.>

## Rationale

<Explain why this is the smallest adequate solution, how it meets the drivers,
and why realistic alternatives were not selected. Justify any new layer,
dependency, service, or infrastructure. Link supporting evidence where available.>

## Consequences

### Benefits

- <Expected improvement and affected users/components.>

### Tradeoffs and risks

- <Accepted cost, limitation, or risk; mitigation and owner where needed.>

## Adoption and compatibility

<Identify affected components/contracts and required implementation work. Describe
breaking changes, migration order, data handling, rollout, and rollback where
applicable. Use Not applicable with a reason when no migration is involved.>

## Validation

| Decision claim or constraint | Planned check / evidence | Owner | Result |
| --- | --- | --- | --- |
| <claim> | <test, analysis, or operational check> | <owner> | Not run |

<Record command, work directory, exit status/output, and artifact identity for
executed checks. Distinguish proposed validation from actual evidence.>

## Open questions

- <Question, impact, decision owner, and affected work; or None.>

## Approval and history

| Date | Event / status change | Decision or approval reference |
| --- | --- | --- |
| <YYYY-MM-DD> | Proposed | <proposal reference> |

<!--
Do not mark Accepted without actual approval. Preserve accepted decision history;
use a new ADR to supersede a decision and cross-link both records. Guardian must
return ARCHITECTURE_REVIEW_REQUIRED for unresolved decisions or ADR conflicts.
-->
