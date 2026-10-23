# <Feature name>

<!--
Copy to docs/specs/<feature-slug>/SPEC.md. Keep this template unchanged.
Use for features or meaningful behavior changes that benefit from explicit
requirements. Keep proportional detail and remove irrelevant sections with a brief
reason where useful. This is a specification plus its clearly separated execution
plan/evidence; requirements describe outcomes, not a list of implementation tasks.
-->

- **Status:** Draft <!-- Draft | Ready for approval | Approved | In progress | Completed | Blocked -->
- **Owner:** <person or team accountable for the outcome>
- **Last updated:** <YYYY-MM-DD>
- **Request / issue:** <reference>
- **Related specifications / ADRs:** <references, or None>
- **Approved scope and plan:** Pending <!-- Actual approver, date, reference, and approved revision. -->

## 1. Goal and context

<State the user/problem, intended outcome, and why the change is needed. Summarize
relevant existing behavior and source references. Distinguish observed behavior,
accepted intent, assumptions, and proposed changes.>

## 2. Scope

### In scope

- <User-visible behavior or outcome covered by this feature.>

### Out of scope

- <Explicitly excluded behavior, component, or follow-up.>

## 3. Requirements (EARS)

Write normative feature requirements using **EARS (Easy Approach to Requirements
Syntax)**. Use stable IDs and one independently verifiable obligation per requirement.
Choose only applicable patterns; a feature need not use every pattern. Define the
system/actor, trigger, state, and observable response precisely. Include measurable
bounds where required; avoid vague terms such as "fast", "appropriate", or "normally".
Define ambiguous terms and thresholds before approval, not during implementation.

| Pattern | Syntax | Use |
| --- | --- | --- |
| Ubiquitous | The <system> shall <response>. | Always-applicable behavior or constraint |
| Event-driven | When <trigger>, the <system> shall <response>. | Response to a discrete event |
| State-driven | While <state>, the <system> shall <response>. | Behavior required for the duration of a state |
| Unwanted behavior | If <unwanted condition>, then the <system> shall <response>. | Invalid input, failures, or other unwanted situations |
| Optional feature | Where <feature is included>, the <system> shall <response>. | Behavior conditional on an included capability, not an event |
| Combined | While <state>, when <trigger>, the <system> shall <response>. | An event response constrained by a state |

Include data/contracts, permissions, validation, concurrency, errors, compatibility,
accessibility, or operational requirements only where applicable. Do not assume a
technology stack. EARS defines behavior; implementation tasks and architectural
rationale belong in their own sections or linked ADRs.

| ID | EARS pattern | Requirement | Source / rationale |
| --- | --- | --- | --- |
| R-001 | Event-driven | When <trigger>, the <system> shall <observable response>. | <request or clarified decision> |
| R-002 | Unwanted behavior | If <invalid condition>, then the <system> shall <observable safe/error response>. | <source> |

## 4. Acceptance scenarios

### AC-001: <Primary successful outcome>

- **Covers:** R-001
- **Given** <relevant initial state>
- **When** <action or event>
- **Then** <observable result>

### AC-002: <Invalid input, denied action, or failure outcome>

- **Covers:** R-002
- **Given** <relevant initial state>
- **When** <invalid action or failure occurs>
- **Then** <agreed error/result and preserved state or safety invariant>

<Acceptance scenarios remain Given/When/Then examples that verify the EARS
requirements; they do not replace them. Cover every requirement with applicable
scenarios/checks, including meaningful boundaries, compatibility, and recovery.
Avoid scenarios that merely restate internal implementation steps.>

## 5. Constraints, dependencies, and decisions

- **Existing conventions/contracts:** <source paths and constraints to preserve>
- **Environment/dependencies:** <required services, tools, versions, or fixtures>
- **Architecture:** <existing pattern to follow, or approved ADR reference>
- **Assumptions:** <assumptions and how they will be confirmed>
- **Open questions:** <owner, impact, and affected work; or None>

<Route consequential boundary/contract/security/structural decisions through Leader
and Architect. Do not invent approval or introduce architecture as an incidental fix.>

## 6. Implementation plan

This section describes execution, separately from the requirements above.

| Step | Approach / affected files or components | Owner / authorized write scope | Dependency |
| --- | --- | --- | --- |
| 1 | <smallest adequate implementation following existing patterns> | <role and files> | <prerequisite or None> |
| 2 | <meaningful tests and regression protection> | <role and files> | <step> |
| 3 | <independent review and documentation impact> | <role and files/evidence> | <step> |

<Specify disjoint file ownership for concurrent writes. Identify migration,
rollout/rollback, or compatibility work only if relevant.>

## 7. Test plan and quality gates

Discover commands from the destination repository's instructions, manifests,
lockfiles, and CI. Inspect side effects and prerequisites before execution.

| Requirement / acceptance | Test or check | Command and working directory | Prerequisites | Required? |
| --- | --- | --- | --- | --- |
| R-001 / AC-001 | <unit, integration, or other meaningful check> | <exact command and cwd> | <setup/fixtures> | Yes |
| R-002 / AC-002 | <negative/regression check> | <exact command and cwd> | <setup/fixtures> | Yes |
| <repository-wide gate> | <configured lint/type/build/security gate as applicable> | <exact command and cwd> | <prerequisites> | <Yes/No and rationale> |

<Distinguish mocked behavior from real integration. Do not impose universal
test-first or coverage expansion. Read-only reviewers inspect Coder evidence for
artifact-writing tests/builds rather than running them outside their authority.>

## 8. Documentation and operational impact

- **Canonical documents:** <paths to update, or why no update is needed>
- **ADR impact:** <approved decision to record, or None>
- **Setup/configuration/user guidance:** <affected guidance, or None>
- **Operational impact:** <relevant rollout, monitoring, recovery, or None>

## 9. Risks and approval

- **Material risks / mitigation:** <risk, mitigation, owner>
- **Unresolved blockers:** <missing decision, evidence, or prerequisite; or None>
- **Human plan approval:** Pending

<Before implementation under /sdd-feature, present scope, approach, files, tests,
verification, docs, and open risks for explicit human approval. Record the actual
approval and approved revision. Approved scope/SPEC changes need renewed approval;
stop only affected work. Do not treat this blank template as authorization.>

## 10. Validation and review record

Fill after execution; planned checks start as Not run. Preserve traceability from
EARS requirement ID to acceptance scenario, implementation artifact, and evidence.

| Requirement / acceptance / check | Implementation artifacts | Command / cwd | Artifact identity | Exit / outcome and evidence |
| --- | --- | --- | --- | --- |
| R-001 / AC-001 / <check> | <changed paths or change reference> | <actual command / cwd> | <revision and dirty state, or hashes> | Not run |

- **Independent Reviewer verdict:** Not requested
- **Blocking findings / correction and re-review:** <actual findings and outcomes, or None>
- **Documentation result:** <actual Guardian status and files, or Not assessed>
- **Remaining limitations / deviations:** <explicitly identify failed, skipped, unrun, or blocked work>
- **Completion assessment:** Pending Leader verification

<Relevant edits invalidate affected evidence. Revalidate and re-review as needed.
After two unsuccessful correction/re-review attempts, reassess rather than blindly
repeating. Completed requires satisfied acceptance, current required checks,
independent review where required, and documentation—not merely implementation.>

## 11. Change and approval history

| Date | Change | Approval / evidence reference |
| --- | --- | --- |
| <YYYY-MM-DD> | Initial draft | Pending |
