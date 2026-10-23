# ADR-0009: Keystore-encrypted restoration and generation-bound AI consent

- **Status:** Proposed for retrospective ratification; implemented approach.
- **Recorded:** 2026-10-23; historical formal approval unavailable.
- **Decision owner:** Project maintainer.
- **Related specification:** [AI session/consent](../specs/ai-session/SPEC.md).
- **Sources:** `e01b6f6`; Android auth/session store and PaidVoiceConsent.
- **Supersedes / superseded by:** None formally recorded; original memory-only login behavior was replaced.

## Context and drivers
Repeated sign-in is inconvenient, but passwords or cleartext refresh tokens must
not be persisted. Logout or toggling paid requests can race with pending login,
refresh, audio claim and HTTP work.

## Options considered retrospectively
| Option | Benefit | Cost / risk |
| --- | --- | --- |
| Memory-only session | Minimal persistent exposure | Manual login after process loss |
| Plain preferences/password storage | Easy restoration | Unacceptable credential exposure |
| Encrypted refresh session and generation tickets | Restoration with explicit request admission | Key invalidation, refresh and race handling |

## Implemented decision and rationale
Use Firebase REST login/refresh, persist permitted session material as AES-256-GCM
ciphertext in noBackupFilesDir with Android Keystore, never passwords. Restore and
verify backend access before admitting paid use. Bind each claimed recording to
the current consent generation. OFF/logout invalidates tickets and cancels local work;
late refresh cannot resurrect a signed-out session.

## Consequences
- Manual OFF survives restart/reconnect even though the initial preference is ON.
- Invalid/revoked refresh clears session; transient failures allow bounded retry.
- Uninstall, key invalidation or clearing data requires manual sign-in.
- Provider-accepted work may still cost money after local cancellation.
- Google Calendar/Tasks scopes are unrelated and need their own authorization.

## Adoption and compatibility
No Firebase Android SDK/Google Services file is required for the existing REST
Email/Password flow. Public Web key is configuration, not an AI secret or authorization.
Backend verification probe depends on the known invalid-WAV-before-quota behavior.

## Validation
Mock refresh/rotation/logout/consent race tests and manual restart/revocation checks.
Historical tests exist; reboot restoration and long-term behavior are not fully
established by documentation. No login/network calls executed here.

## Open questions and approval history
Future auth-method changes must preserve logout/consent invariants.
2026-10-23: retrospective record; formal ratification pending.
