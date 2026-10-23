# ADR-0007: Server-side AI credentials, Firebase authorization and durable quotas

- **Status:** Proposed for retrospective ratification; implemented approach.
- **Recorded:** 2026-10-23; historical formal approval unavailable.
- **Decision owner:** Project maintainer/cloud operator.
- **Related specification:** [Voice backend](../specs/voice-backend/SPEC.md).
- **Sources:** `2ed4821`; backend API, tests and deployment/security documentation.
- **Supersedes / superseded by:** None formally recorded.

## Context and drivers
A distributed APK/firmware cannot protect an embedded OpenAI secret. Authentication
alone is insufficient if any Firebase account can consume the prototype's paid API.

## Options considered retrospectively
| Option | Benefit | Cost / risk |
| --- | --- | --- |
| Client calls OpenAI with embedded key | Fewer services | Extractable secret and no trusted quota boundary |
| Authenticated backend with allowlist/Firestore | Central authorization and durable limits | Cloud setup, latency, costs and operational dependencies |
| Process-only usage counters | Low implementation cost | Restarts/instances bypass durable request accounting |

## Implemented decision and rationale
Android sends bounded WAV over authenticated HTTPS to FastAPI/Cloud Run. Firebase
token validation and explicit UID allowlist precede paid use. Firestore transactionally
reserves daily quotas before OpenAI; unavailable quota storage fails closed. Secret
Manager supplies the server key. Responses are bounded text without conversation tools/history.

## Consequences
- Clients never receive OpenAI keys and cannot bypass server allowlisting via UI.
- Quota reservations survive restarts; failed calls may still consume reservations.
- Request limits are not a hard monetary cap; budgets/alerts are separate safeguards.
- Canceling a remote accepted request cannot guarantee no charge. Madrid hosting
  does not establish end-to-end regional residency or zero provider retention.

## Adoption and compatibility
Existing WAV/JSON contract is stable. Cloud configuration and deployment are explicit
operator actions; packaging/runbook instructions do not prove current remote state.

## Validation
Mock auth/allowlist/quota/model/deadline tests and authorized live smoke only when
approved. Historical deployed end-to-end success does not replace a current IAM audit.
No resources, secrets, models or policies changed by this documentation task.

## Open questions and approval history
Large/public distribution would require a new abuse/cost review.
2026-10-23: reconstructed implemented boundary; formal ratification pending.
