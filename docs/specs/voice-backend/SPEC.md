# Authenticated Spanish voice API and deployment boundary

- **Status:** Ready for approval — retrospective; implemented, deployed during development.
- **Owner:** Project maintainer/cloud operator; original formal approval record unavailable.
- **Last updated:** 2026-10-23.
- **Sources:** `2ed4821`; `apps/voice-backend/app.py`, tests, README and `SPANISH_DEPLOY.md`.
- **Related ADR:** [0007](../../adr/0007-authenticated-cloud-voice-and-quotas.md).

## 1. Goal and context
Process bounded speech without distributing AI secrets to clients or allowing
unauthenticated callers to consume the user's AI quota.

## 2. Scope
Firebase-authenticated WAV API, UID allowlist, quotas, OpenAI transcription/response,
Spanish defaults and sanitized errors. Exclude anonymous AI, TTS and cloud provisioning in tests.

## 3. Requirements (EARS)
| ID | Pattern | Requirement |
| --- | --- | --- |
| API-01 | Event-driven | When a voice request arrives, the API shall validate the Firebase token and authorized UID before paid model use. |
| API-02 | Unwanted behavior | If WAV format/duration/size is invalid or quota storage is unavailable, the API shall reject the request without bypassing validation/cost controls. |
| API-03 | Event-driven | When a valid request is admitted, the backend shall reserve durable quota before transcribing and generating bounded text. |
| API-04 | Ubiquitous | The API shall keep secrets server-side, avoid sensitive logs and use the configured Spanish transcription/response policy. |

## 4. Acceptance scenarios
- **AC-01 (API-01/02):** Given invalid/revoked credentials, disallowed UID or malformed WAV, when posting, then sanitized 4xx/5xx outcomes occur without a paid model call.
- **AC-02 (API-03/04):** Given mocked authorized models and quota store, when a valid clip is posted, then quota is reserved, output limits apply, defaults are Spanish and logs contain no audio/token/transcript.

## 5. Constraints and decisions
16 kHz mono PCM16 WAV, 0.25–8 s, max 256,044 bytes. Output bounds: 512 UTF-8
transcription bytes, 3,072 answer bytes, 600 output tokens. Defaults: user 20/global
100 requests per UTC day, cooldown 15 s; these are source defaults, not a live cloud audit.
`GET /healthz` is liveness only. Model defaults: `gpt-4o-mini-transcribe`, `gpt-4.1-mini`.

## 6. Implementation record
FastAPI validators/auth → Firestore reservation → OpenAI → bounded normalized JSON.
Non-root container/readability guards and reviewed deployment/rollback runbooks accompany it.

## 7. Test plan
API; mocked auth, quota/concurrency, model failures, language and Dockerfile tests.
Live smoke/deploy/model calls require separate authorization and may incur costs.

## 8. Documentation impact
Backend README and deployment guides contain historical preparation states; root
README distinguishes delivered behavior from current unverified cloud configuration.

## 9. Risks and approval
Remote accepted calls cannot be uncharged; quotas are not a currency budget.
Provider retention/regional processing are not guaranteed by Madrid hosting.

## 10. Validation record
Physical end-to-end AI response and Spanish deployment were reported historically.
Mock tests do not certify current IAM/secrets/quota configuration. No cloud inspection,
deployment or paid calls performed here; retrospective ratification pending.

## 11. History
2026-10-23: records service, security, language and operational packaging boundaries.
