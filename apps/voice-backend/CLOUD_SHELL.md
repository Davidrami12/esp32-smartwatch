# Reviewed build-only upload package

Project: `esp32-smartwatch-511417`; region: `europe-southwest1`;
Artifact Registry repository: `watch-ai-backend`.
Runtime service account: `watch-ai-backend@esp32-smartwatch-511417.iam.gserviceaccount.com`.
Existing Secret Manager resource: `OPENAI_API_KEY`.

The runtime account and secret are **not** Cloud Build inputs. No secret value,
Firebase token, service-account key, env file, tests, caches or venv is included.
This package contains the backend source and pinned runtime requirements, Dockerfile,
Cloud Build YAML and ignore files. It never deploys a service or creates resources.

The Dockerfile now normalizes source permissions regardless of upload/ZIP modes:
`/app` is traversable mode 0755, source/requirements are mode 0644, owned by root
with runtime group 10001. Runtime remains UID/GID 10001 and cannot rewrite these
root-owned files. A build-time RUN as that user checks modes/readability and imports
the app without credentials or provider calls. Cloud Build explicitly enables
BuildKit for `COPY --chmod` support. The startup permission fix requires
a new image build after approval; it cannot repair an already-built image. Local
Docker execution remains unverified while Docker is unavailable.

Upload `watch-ai-backend-cloud-shell-permissions-fix.zip` using Cloud Shell's Upload file action.
Then run these extraction and read-only inspection commands:

```bash
ZIP="$HOME/watch-ai-backend-cloud-shell-permissions-fix.zip"
WORKDIR="$(mktemp -d "$HOME/watch-ai-backend-build.XXXXXX")"
unzip "$ZIP" -d "$WORKDIR"
cd "$WORKDIR"
sha256sum --check SHA256SUMS
cat cloudbuild.yaml
gcloud artifacts repositories describe watch-ai-backend \
  --project=esp32-smartwatch-511417 --location=europe-southwest1
```

**The next command submits a potentially paid build and uploads source. Run it only
when you approve that action.** It does not deploy or access the OpenAI secret.

```bash
TAG="voice-$(date -u +%Y%m%d-%H%M%S)"
IMAGE="europe-southwest1-docker.pkg.dev/esp32-smartwatch-511417/watch-ai-backend/backend:${TAG}"
gcloud builds submit . --project=esp32-smartwatch-511417 \
  --region=europe-southwest1 --config=cloudbuild.yaml \
  --substitutions="_IMAGE=${IMAGE}"
# Read-only image inspection after a successful build:
gcloud artifacts docker images describe "$IMAGE" \
  --project=esp32-smartwatch-511417 --format='value(image_summary.digest)'
```

The submit command uses the project's configured default Cloud Build identity;
its existence/permissions have **not** been verified remotely. It needs repository
write, build-source read and Cloud Logging write access. The submitting user needs
build submission and source-upload permissions. Cloud Build uses its normal source
staging bucket; absent staging infrastructure can be created as part of submission.
If a dedicated builder is required, obtain its actual approved email and add
`--service-account=projects/esp32-smartwatch-511417/serviceAccounts/APPROVED_BUILD_EMAIL`
and the approved source-staging bucket argument. Do not use the runtime account or
grant it broad builder permissions. Do not enable APIs, create accounts/buckets or
change IAM to bypass errors without approval.

Runtime deployment remains a separate approved operation. Firebase project/UID
allowlist, durable Firestore quotas, runtime IAM and a pinned numeric version of
`OPENAI_API_KEY` are still required. Do not deploy from this build-only guide.
The archive and image have not been built/tested with Docker in the current environment
(Docker unavailable); archive integrity and Python tests/lint are independent checks.
