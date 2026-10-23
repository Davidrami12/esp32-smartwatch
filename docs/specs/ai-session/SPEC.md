# Encrypted automatic AI session and request consent

- **Status:** Ready for approval — retrospective; implemented.
- **Owner:** Project maintainer; historical formal approval unavailable.
- **Last updated:** 2026-10-23.
- **Sources:** `e01b6f6`; Android `ai/`, paid consent/session tests, mobile README.
- **Related ADR:** [0009](../../adr/0009-encrypted-session-and-generation-consent.md).

## 1. Goal and context
Avoid repeated manual login without storing passwords or admitting old recordings
after a consent/logout race. This supersedes the first memory-only login design.

## 2. Scope
Firebase REST login/refresh, Keystore ciphertext, backend access verification,
paid preference and generation tickets. Exclude embedded secrets and Google calendar grants.

## 3. Requirements (EARS)
| ID | Pattern | Requirement |
| --- | --- | --- |
| AUTH-01 | Event-driven | When a manual login succeeds, the app shall persist only the permitted session material as AES-256-GCM ciphertext under noBackupFilesDir using Android Keystore. |
| AUTH-02 | State-driven | While restoration/access verification is incomplete, the app shall block paid recordings even if the preference defaults ON. |
| AUTH-03 | Event-driven | When consent changes or the user logs out, the app shall invalidate old consent generations and cancel local pending/in-flight work. |
| AUTH-04 | Unwanted behavior | If refresh is revoked/invalid, the app shall clear the session; if failure is transient, it shall retain recoverable state without silently authorizing requests. |

## 4. Acceptance scenarios
- **AC-01 (AUTH-01/02):** Given saved ciphertext and enabled preference, when starting/reconnecting, then token refresh and backend authorization precede paid use; no password is persisted.
- **AC-02 (AUTH-03/04):** Given queued audio or a pending refresh, when disabling/logout/revocation occurs, then late completion cannot restore permission; manually OFF remains OFF after later login/restart.

## 5. Constraints and decisions
Firebase public Web key is configuration, not an OpenAI secret. Backend-specific
empty-WAV access probe authenticates then rejects before quota/model use; do not
generalize this to arbitrary endpoints. Remote accepted charges cannot be canceled.

## 6. Implementation record
REST auth/session store → verified restoration → PaidVoiceConsent ticket → voice
request admission; sign-out removes encrypted state and Keystore key.

## 7. Test plan
ANDROID mock session/consent race tests; manual revoked credential, app/process
restart, reconnect and logout. No paid requests needed for consent-gate tests.

## 8. Documentation impact
Mobile README and root security section replace historical temporary-only login wording.

## 9. Risks and approval
Device key invalidation, immutable token copies and transient offline recovery;
historical formal ratification pending.

## 10. Validation record
Mock refresh/race tests and installed session implementation exist. Reboot automatic
restoration and long-term reliability are not fully established by the available record.
No authentication/network performed in this task.

## 11. History
2026-10-23: reconstructed persistent-session evolution and consent safety requirements.
