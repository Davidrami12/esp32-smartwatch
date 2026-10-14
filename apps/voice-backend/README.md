# Authenticated AI voice backend

## Current build-only upload configuration

Startup permission correction: `COPY` now explicitly sets `--chown=0:10001` and
`--chmod=0644`; `/app` is explicitly mode 0755 and runtime UID/GID remains 10001.
Unrestricted source-mode preservation previously allowed an uploaded root-owned
0600 `app.py` to cause `PermissionError` for the runtime user. A non-root Docker
build-time import/read/mode check catches this regression before image publication.
Python's Dockerfile regression test is a static guard, not proof of a Linux build.
Rebuild this corrected image only after approval; no build/deploy was triggered.

The configured build destination is now
`europe-southwest1-docker.pkg.dev/esp32-smartwatch-511417/watch-ai-backend/backend`.
Runtime identity is `watch-ai-backend@esp32-smartwatch-511417.iam.gserviceaccount.com`;
the existing secret name is `OPENAI_API_KEY`. These nonsecret names were supplied by
the user; remote existence/IAM has not been verified. See [CLOUD_SHELL.md](CLOUD_SHELL.md)
for the self-contained upload archive's exact extraction/build commands. The longer
setup runbook below is a generic provisioning template: do not recreate existing
resources or run its placeholder setup/deployment commands for this packaging task.

Deployable Python/FastAPI service for the watch MVP. No deployment or billable
resource creation has been performed. Production does not depend on a development
PC. Local mock tests do not call Firebase, Firestore or OpenAI.

## API and privacy

`POST /v1/voice`: `Authorization: Bearer <Firebase ID token>`, `Content-Type: audio/wav`,
body: 16 kHz, mono, signed 16-bit PCM WAV, 0.25–8 seconds, maximum 256,044 bytes.
Returns UTF-8 JSON `{ "transcription": "…", "answer": "…" }`. Spanish transcription
is explicit (`language="es"`); Responses instructions request concise Spanish text.
Limits: transcription 512 UTF-8 bytes, response 3,072 bytes, 600 output tokens.
Output is NFC-normalized and truncated at a valid UTF-8 boundary.

Missing/invalid/expired/revoked token: 401; account outside UID allowlist: 403;
bad WAV: 400; oversized recording: 413; wrong MIME: 415; no speech: 422;
quota/concurrency: 429; provider failure: sanitized 503; total timeout: 504.
No raw upstream error, secret, audio or transcript appears in application logs.
`GET /healthz` is a public liveness endpoint, not proof that secrets/IAM are configured.

Official OpenAI SDK uses `audio.transcriptions.create` with `gpt-4o-mini-transcribe`,
then `responses.create` with `gpt-4.1-mini`, `store=False`, no tools, no conversation
history and no automatic SDK retries. Do not enable SDK HTTP debug logging or
reverse-proxy body/header logging. Cloud infrastructure may log URL/status/latency;
audio and authentication belong in body/header, never URL. `store=False` is not
a guarantee of zero provider retention; review OpenAI's applicable data policies.
Mutable audio buffers are wiped in `finally`; Python/SDK/HTTP libraries can make
immutable copies managed by garbage collection. No audio/transcript file is saved.

## Environment

| Variable | Required/default |
|---|---|
| `OPENAI_API_KEY` | Required; inject from Secret Manager only |
| `FIREBASE_PROJECT_ID` | Required; Firebase/Auth/Firestore project |
| `ALLOWED_FIREBASE_UIDS` | Required, comma-separated nonempty UID allowlist |
| `USER_DAILY_LIMIT` | 20, capped at 100 |
| `GLOBAL_DAILY_LIMIT` | 100, capped at 1,000 |
| `TRANSCRIPTION_MODEL` | `gpt-4o-mini-transcribe` only in MVP |
| `RESPONSE_MODEL` | `gpt-4.1-mini` only in MVP |
| `PORT` | 8080 (container) |

Use Cloud Run's attached service-account Application Default Credentials. Do not
place service-account JSON or `.env` in the repo/container. Authentication verifies
Firebase ID-token signature, issuer/audience/project, expiry and revocation, then
checks the explicit UID allowlist. Enable Email/Password in Firebase Authentication
and create the authorized account administratively. An unrelated Firebase user—even
in the same project—cannot call OpenAI through this backend.

Firestore transactionally reserves daily UTC per-user and global requests before
OpenAI. Quotas survive restarts and multiple instances; failures consume a reservation
so retries cannot bypass cost controls. A 15-second per-user cooldown and at most
two active requests/process apply. The recommended single-instance deployment
provides the per-user concurrent-request gate. Counters store hashed UID, counts,
timestamps and expiry only, never speech. Deny direct client access to `voice_quotas`.
Enable Firestore TTL on `expires` for housekeeping; expiry is not used to authorize
requests. Database failures fail closed with 503 rather than bypassing quotas.

Requests have a 55-second application deadline and 25-second SDK timeouts. No
guarantee is made that aborting an already submitted provider call reverses its
charge. Quotas and model/input/output caps are request-based cost safeguards, not
a hard currency budget. Also set project budgets/alerts and OpenAI project spend
controls; billing alerts themselves are not a hard spending cutoff. Protect public
ingress against abuse if deploying beyond a small allowlisted prototype: invalid
JWT verification and network ingress can themselves incur costs.

## Spanish language update for the existing deployed service

Use `SPANISH_DEPLOY.md` for the narrow, digest-pinned rollout to
`esp32-smartwatch-511417` / `europe-southwest1` / `watch-ai-backend`, preserving
the existing `OPENAI_API_KEY:2` binding and all runtime configuration. Its
preflight/build/stage/smoke/promote/rollback commands replace the generic initial
setup runbook below for this update. The Spanish prompt changes are local only
until explicitly built/deployed; mocked language tests do not prove recognition.

## Deployment readiness and region

**Prepared, NOT deployed. All commands below require explicit approval first.**
No project, billing, API enablement, source upload, secret upload or cloud deployment
has been performed. The branch remains `feature/mobile-android-foundation`.

The Dockerfile runs as UID 10001, binds `0.0.0.0:$PORT`, disables access logging,
and runs one Uvicorn worker. No local disk/database/history is needed. Durable
quotas live in Firestore; the process-local active set is only a concurrency guard.
Scale-to-zero/restarts do not reset daily allowances. `/healthz` is liveness only:
it deliberately does not invoke OpenAI, access secrets or verify database/auth IAM.

Initial production settings: **Madrid `europe-southwest1`, 1 vCPU, 512 MiB,
request-based CPU billing, min 0 / service max 1, revision max 1, concurrency 2,
70-second platform timeout, 55-second app deadline, 25-second SDK timeout**.
Cold starts are possible. Max instances is a scaling safeguard, not a hard billing
cap; deployments/draining and exceptional scaling can temporarily exceed limits.

Official location lists reviewed 2026-10-12 list Madrid for:
[Cloud Run](https://cloud.google.com/run/docs/locations),
[Cloud Build](https://cloud.google.com/build/docs/locations),
[Artifact Registry](https://cloud.google.com/artifact-registry/docs/repositories/repo-locations),
[Firestore Native](https://cloud.google.com/firestore/native/docs/locations), and
[Secret Manager](https://cloud.google.com/secret-manager/docs/locations).
No App Engine is needed (Madrid Firestore does not support App Engine). Firebase
Auth is project-managed, not a Madrid-local deployment. OpenAI is an external API;
Madrid hosting does not guarantee Madrid/EU processing by Firebase/Auth or OpenAI.
Confirm residency requirements separately before approval. Never silently migrate
an existing Firestore database: its location is immutable. If an existing default
database is elsewhere, approve that explicitly or use a separate reviewed project.

## Exact manual runbook — Cloud Shell, after approval ONLY

Use Google Cloud Shell (Bash), so build/deployment and production need no local PC.
Upload a **reviewed source snapshot** of `apps/voice-backend` through Cloud Shell
Editor into `~/watch-voice-backend`, or obtain that same snapshot from your existing
repository. A Git clone does not include the current uncommitted feature files.
Do not commit/push just to deploy, and do not upload `.env`, credential JSON, keys
or recordings. `.gcloudignore`/`.dockerignore` provide defense in depth. No startup
or test script automatically runs gcloud or provisions anything.

### 1. Choose the project and obtain deliberate billing approval

Prefer a dedicated project for this prototype. In the Cloud Console, select/create
the project and link the approved billing account manually. Configure billing
budgets/alerts before resource creation. The operator needs the administrative
permissions for setup; the runtime/build identities below do **not** get Owner or
Editor. If organization policy forbids public ingress or Madrid, stop and review
with the administrator; do not weaken policy automatically.

```bash
export PROJECT_ID='REPLACE_WITH_APPROVED_PROJECT_ID'
export REGION='europe-southwest1'
export SERVICE='watch-voice-ai'
export REPOSITORY='watch-voice'
export SECRET='openai-api-key'
export RUNTIME_SA="voice-backend@${PROJECT_ID}.iam.gserviceaccount.com"
export BUILD_SA="voice-build@${PROJECT_ID}.iam.gserviceaccount.com"
export SOURCE_BUCKET="${PROJECT_ID}-voice-build-source"
gcloud config set project "$PROJECT_ID"
gcloud services enable run.googleapis.com cloudbuild.googleapis.com \
  artifactregistry.googleapis.com secretmanager.googleapis.com \
  firestore.googleapis.com identitytoolkit.googleapis.com \
  firebase.googleapis.com iam.googleapis.com storage.googleapis.com \
  --project="$PROJECT_ID"
```

### 2. Firebase user and durable quotas

In Firebase Console, **Add Firebase to the approved existing project**. Enable
Authentication → Email/Password. Administratively create the intended user and
copy its UID. Copy the project's public Web API key (create/register a Firebase
Web app if needed). Restrict that key to the required Firebase Auth APIs; do not
add browser-referrer restrictions incompatible with native REST login. It is a
public project identifier, not the OpenAI secret or authorization for this backend.

```bash
export FIREBASE_UID='REPLACE_WITH_CREATED_USER_UID'
# Inspect first. If a database already exists, do not recreate or relocate it.
gcloud firestore databases describe --database='(default)' --project="$PROJECT_ID"
# Run ONLY if the default database is absent and its creation/location is approved:
gcloud firestore databases create --database='(default)' --location="$REGION" \
  --type=firestore-native --project="$PROJECT_ID"
# Optional approved housekeeping: TTL deletion is billable, not a quota mechanism.
gcloud firestore fields ttls update expires --collection-group=voice_quotas \
  --database='(default)' --enable-ttl --project="$PROJECT_ID"
```

Use deny-by-default Firestore client Security Rules; ensure no overlapping broad
allow grants clients write access to `voice_quotas`. Admin runtime access uses IAM,
not Firebase client rules. Do not overwrite another application's rules/database.
Reservations are two document reads and two writes per accepted attempt, possibly
more on transactional retries. Failures still consume allowances; redeployment
and rollback do not reset them.

### 3. Dedicated runtime and build identities

```bash
gcloud iam service-accounts create voice-backend --project="$PROJECT_ID" \
  --display-name='Watch voice runtime'
gcloud iam service-accounts create voice-build --project="$PROJECT_ID" \
  --display-name='Watch voice image builder'
gcloud projects add-iam-policy-binding "$PROJECT_ID" \
  --member="serviceAccount:${RUNTIME_SA}" --role=roles/datastore.user
# Revocation verification needs user lookup, not user creation or admin ownership.
gcloud iam roles create watchVoiceAuthLookup --project="$PROJECT_ID" \
  --title='Watch voice Auth lookup' --permissions=firebaseauth.users.get --stage=GA
gcloud projects add-iam-policy-binding "$PROJECT_ID" \
  --member="serviceAccount:${RUNTIME_SA}" \
  --role="projects/${PROJECT_ID}/roles/watchVoiceAuthLookup"
gcloud artifacts repositories create "$REPOSITORY" --repository-format=docker \
  --location="$REGION" --project="$PROJECT_ID"
gcloud artifacts repositories add-iam-policy-binding "$REPOSITORY" \
  --location="$REGION" --project="$PROJECT_ID" \
  --member="serviceAccount:${BUILD_SA}" --role=roles/artifactregistry.writer
gcloud projects add-iam-policy-binding "$PROJECT_ID" \
  --member="serviceAccount:${BUILD_SA}" --role=roles/logging.logWriter
gcloud storage buckets create "gs://${SOURCE_BUCKET}" --location="$REGION" \
  --uniform-bucket-level-access --public-access-prevention --project="$PROJECT_ID"
gcloud storage buckets add-iam-policy-binding "gs://${SOURCE_BUCKET}" \
  --member="serviceAccount:${BUILD_SA}" --role=roles/storage.objectViewer
```

`roles/datastore.user` is the least practical predefined data-access role here; it
is project-wide, not collection-isolated. A dedicated project limits that scope.
Runtime has no build/upload/deploy permissions. Builder has no runtime role, Auth
lookup or OpenAI secret access. Cloud-managed service agents keep their standard
service-agent roles; do not grant them to these user-managed accounts. The human
deployer needs Cloud Build submit, Cloud Run deploy/IAM administration, artifact
read/source upload and `iam.serviceAccounts.actAs` on both chosen accounts. Grant
those to the approved operator separately, not to the runtime or builder.

### 4. OpenAI secret: global resource, Madrid replica, pinned version

Cloud Run's env-secret integration uses a **global Secret Manager resource** with
user-managed Madrid replication, not a regional-secret name.

```bash
gcloud secrets create "$SECRET" --replication-policy=user-managed \
  --locations="$REGION" --project="$PROJECT_ID"
gcloud secrets add-iam-policy-binding "$SECRET" --project="$PROJECT_ID" \
  --member="serviceAccount:${RUNTIME_SA}" --role=roles/secretmanager.secretAccessor
```

In Secret Manager Console, open this secret → **Add version**, paste the OpenAI
project key, save, and note its numeric version. Give that OpenAI project restricted
model/API permissions where available and configure its spend/rate controls. Do not
paste the value into chat, command arguments/history, Git, Android or an env YAML.
Do not export the key in Cloud Shell. The builder never receives it.

```bash
export SECRET_VERSION='1'  # Replace with the actual numeric version; never "latest".
```

### 5. Build the reviewed image remotely and deploy by immutable digest

```bash
cd "$HOME/watch-voice-backend"
export TAG="voice-$(date -u +%Y%m%d-%H%M%S)"
export IMAGE="${REGION}-docker.pkg.dev/${PROJECT_ID}/${REPOSITORY}/backend:${TAG}"
gcloud builds submit . --config=cloudbuild.yaml --region="$REGION" \
  --service-account="projects/${PROJECT_ID}/serviceAccounts/${BUILD_SA}" \
  --gcs-source-staging-dir="gs://${SOURCE_BUCKET}/source" \
  --substitutions="_IMAGE=${IMAGE}" --project="$PROJECT_ID"
export DIGEST="$(gcloud artifacts docker images describe "$IMAGE" \
  --project="$PROJECT_ID" --format='value(image_summary.digest)')"
test -n "$DIGEST" || { echo 'Missing image digest; stop'; exit 1; }
export PINNED_IMAGE="${REGION}-docker.pkg.dev/${PROJECT_ID}/${REPOSITORY}/backend@${DIGEST}"
# Nonsecret values only. YAML safely supports a future comma-separated UID list.
cat > /tmp/voice-public-config.yaml <<EOF
FIREBASE_PROJECT_ID: "${PROJECT_ID}"
ALLOWED_FIREBASE_UIDS: "${FIREBASE_UID}"
USER_DAILY_LIMIT: "20"
GLOBAL_DAILY_LIMIT: "100"
TRANSCRIPTION_MODEL: "gpt-4o-mini-transcribe"
RESPONSE_MODEL: "gpt-4.1-mini"
EOF
gcloud run deploy "$SERVICE" --image="$PINNED_IMAGE" \
  --project="$PROJECT_ID" --region="$REGION" --service-account="$RUNTIME_SA" \
  --allow-unauthenticated --ingress=all --execution-environment=gen2 \
  --cpu=1 --memory=512Mi --cpu-throttling --no-cpu-boost \
  --min=0 --max=1 --min-instances=0 --max-instances=1 --concurrency=2 \
  --timeout=70s --port=8080 \
  --startup-probe='httpGet.path=/healthz,httpGet.port=8080,initialDelaySeconds=0,periodSeconds=5,timeoutSeconds=2,failureThreshold=12' \
  --env-vars-file=/tmp/voice-public-config.yaml \
  --set-secrets="OPENAI_API_KEY=${SECRET}:${SECRET_VERSION}"
export BACKEND_URL="$(gcloud run services describe "$SERVICE" \
  --project="$PROJECT_ID" --region="$REGION" --format='value(status.url)')"
gcloud run services describe "$SERVICE" --project="$PROJECT_ID" --region="$REGION" \
  --format='yaml(status.url,status.latestReadyRevisionName,status.traffic)'
curl --fail --silent --show-error "${BACKEND_URL}/healthz"
# Expected HTTP 401, not 200. This request cannot invoke OpenAI.
curl --silent --show-error --output /dev/null --write-out '%{http_code}\n' \
  -X POST -H 'Content-Type: audio/wav' --data-binary '' "${BACKEND_URL}/v1/voice"
```

Public ingress (`--allow-unauthenticated`) applies only to **Cloud Run IAM**. The
existing Android Firebase ID token is a Firebase-project token, not a Cloud Run
IAM audience token; putting Cloud Run IAM in front without a gateway would reject
the existing client. The application verifies signature/issuer/audience/expiry/
revocation and UID allowlist before OpenAI. Never replace it with an embedded APK
API secret. Public `/healthz` deliberately proves only process liveness. Cloud Run
platform HTTP metadata logging is possible; never enable header/body/debug logging.
The image does not contain a mock-mode env switch; local mocks require a test-only
mount and command override and are excluded from cloud build source.

### 6. Android runtime configuration and approved live acceptance

Open **AI Assistant · configure / sign in**. Enter `BACKEND_URL` without a
`/v1/voice` suffix, Firebase public Web API key, authorized email and password. The
same Firebase project must be used on both sides. URL/key are saved; password/ID
token are memory-only. Re-login after expiry (about one hour) or process restart.
No APK rebuild, BLE security change or firmware update is needed for the Cloud Run
URL. Re-enable the saved-watch companion if an APK was previously replaced.

Only after deployment approval and a deliberate real-AI test, verify allowed-user
login, forbidden/expired/revoked tokens, quota/timeout paths, and one authorized
Spanish voice request. Do not log audio, transcript, password or bearer token while
testing. Mock PASS is not evidence of deployed IAM, real provider access or physical
watch transfer. The latest physical transfer-performance fixes remain unverified.

## Exact rollback, rotation and emergency-access commands

Before each **later** deployment, record the currently serving revision and pinned
image/secret versions. First deployment has no previous revision to roll back to.

```bash
gcloud run services describe "$SERVICE" --region="$REGION" --project="$PROJECT_ID" \
  --format='yaml(status.traffic,status.latestReadyRevisionName)'
export PREVIOUS_REVISION='REPLACE_WITH_RECORDED_SERVING_REVISION'
# Inspect its configuration before rollback; output contains secret references, not values.
gcloud run revisions describe "$PREVIOUS_REVISION" --region="$REGION" --project="$PROJECT_ID"
gcloud run services update-traffic "$SERVICE" --region="$REGION" --project="$PROJECT_ID" \
  --to-revisions="${PREVIOUS_REVISION}=100"
```

Keep the old image and pinned secret version available until rollback is no longer
needed; rollback restores revision configuration, not Firestore counters, Firebase
user changes or revoked/deleted keys. To rotate: add a new secret version in the
console, then explicitly update the service (creates a revision):

```bash
export NEW_SECRET_VERSION='REPLACE_WITH_APPROVED_NUMERIC_VERSION'
gcloud run services update "$SERVICE" --region="$REGION" --project="$PROJECT_ID" \
  --update-secrets="OPENAI_API_KEY=${SECRET}:${NEW_SECRET_VERSION}"
# Emergency: revoke public invocation. Existing Firebase Android calls will be blocked.
gcloud run services remove-iam-policy-binding "$SERVICE" --region="$REGION" \
  --project="$PROJECT_ID" --member=allUsers --role=roles/run.invoker
```

Revoking ingress does not cancel all in-flight provider charges or remove billed
storage. It is not an automatic deletion/erase/billing-disable procedure. No such
emergency, rotation or rollback command is run automatically.

## Costs and safeguards

Check current **Madrid/project/currency** prices before approval; free tiers are
conditional/shared allowances, not a guarantee of zero cost:

- [Cloud Run](https://cloud.google.com/run/pricing): request CPU/memory duration,
  requests, startup/shutdown and outbound networking. Min 0 avoids intentionally
  provisioned idle instances; max 1 limits normal scaling but is not a currency cap.
- [Secret Manager](https://cloud.google.com/secret-manager/pricing): active versions
  and access operations, including cold starts. Preserve only needed versions after
  the rollback window; do not disable a version used by a live/rollback revision.
- [Cloud Build](https://cloud.google.com/build/pricing): build minutes; each image
  rebuild can be billed even without runtime traffic. Build timeout is 900 seconds.
- [Artifact Registry](https://cloud.google.com/artifact-registry/pricing) and
  [Cloud Storage](https://cloud.google.com/storage/pricing): images, uploaded source,
  storage/operations/network. Review retention/cleanup policies without removing
  needed rollback images. [Cloud Logging](https://cloud.google.com/logging/pricing)
  can add ingestion/retention charges; avoid verbose access/debug logs.
- [Firestore](https://cloud.google.com/firestore/pricing): transactional reads/writes,
  stored counters and optional billable TTL deletes. [Firebase Auth](https://firebase.google.com/pricing)
  depends on account type, Identity Platform settings and monthly activity; this MVP
  uses email/password, not paid SMS verification.
- [OpenAI](https://openai.com/api/pricing/): transcription audio duration and GPT
  input/output tokens. At defaults, application maximum is 100 accepted reservations
  per UTC day globally, at most 8 seconds audio and 600 output tokens each: up to
  13.34 audio minutes plus 60,000 output tokens/day, plus bounded prompt/input tokens.
  Quota is reserved before calls; errors still count. No SDK automatic retries.

Create Google Cloud budget alerts (for example a small monthly budget with 50/90/100%
thresholds), plus OpenAI project usage/rate/spend controls. **Budget alerts do not
stop billing.** The Firestore request caps are hard application request limits, not
hard total-dollar limits; attacks on public ingress, builds, storage and auth can
cost money without successful AI requests. Tighten defaults if necessary; do not
raise them to hide failures. Reserve a reviewed manual emergency response plan.

## Automated mock validation

```sh
python -m venv .venv
# Activate the venv for your platform.
python -m pip install -r requirements-dev.txt
python -m pytest -q
python -m ruff check app.py tests
# With a working local Docker engine (no cloud credentials/resources needed):
python tests/container_smoke.py
```

Tests include mocked WAV→Spanish JSON, real route invocation with mocked official
OpenAI SDK methods, authentication, quota, invalid/oversized audio, bounded Unicode
and sanitized provider failure. They use no real credentials or external provider
requests. Android tests cover captured PCM/CRC→WAV→mock Spanish result BLE frames;
host UI tests cover result reassembly, bounds, Unicode and scrollable UI. These are
layered mock integration tests, not real GATT/cloud/device end-to-end proof.

Container smoke builds this Dockerfile, verifies non-root UID, a nondefault `PORT`,
health and production fail-closed configuration; mounts test-only mocks to exercise
401/403/400/413/429/503/504 and Spanish JSON over HTTP. It removes only its own test
containers. The local image remains available. Tests use no OpenAI key or Firebase
credentials. On the current Windows environment Docker/Podman is unavailable:
**local container build/run SKIPPED**, not PASS. Python tests/lint can still pass;
production Linux/container behavior and cloud IAM/deployment remain unverified.

### Preparation validation record (2026-10-12)

- Backend Python tests: **PASS (10)**, including public liveness, fail-closed missing
  configuration and mocked Firebase revocation/UID checks.
- Ruff lint and repository `git diff --check`: **PASS**.
- Actual local container build/HTTP smoke: **SKIPPED/BLOCKED**; Docker/Podman is
  absent and no WSL distribution is installed. The smoke script reports this rather
  than claiming an image PASS. Install/start a reviewed Docker engine and rerun it
  before deployment approval; no system/container engine was installed automatically.
- Android tests: **N/A for this preparation change**; Android/auth/BLE/firmware code
  was inspected but not modified. Existing runtime HTTPS configuration is retained.
- Cloud build/deploy/real IAM/secret injection/OpenAI/physical transfer: **NOT RUN**.
  Gcloud runbook syntax was reviewed against documentation, not executed in a project.
- Remaining warning: upstream Starlette/httpx TestClient deprecation (tests pass).
  No branch creation, resource creation, billing enablement, secrets upload,
  deployment, commit or push was performed.

TTS, persistent sign-in, conversational history, automatic chunk retry and ADPCM/
notification-stream transport optimization are not part of this text-response MVP.
