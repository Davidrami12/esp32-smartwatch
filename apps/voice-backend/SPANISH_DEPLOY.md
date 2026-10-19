# Spanish language rollout — existing service only

Prepared commands, **not executed**. Build/deployment/Cloud Run invocations can
incur Google Cloud charges. No smoke test below sends valid audio or invokes
OpenAI. Do not run a live audio test without separate manual approval.

This replaces only the image on `watch-ai-backend` in Madrid. No API enablement,
IAM, Firebase, Firestore, secret creation/upload, Android or watch changes.
Existing Firebase revocation/UID checks, durable quotas, model/output limits and
timeouts are unchanged. Secret binding must already be `OPENAI_API_KEY:2`;
preflight stops if not. The runbook does not silently change secret versions.

## 1. Upload and verify

Upload `watch-ai-backend-spanish-fix.zip` through Cloud Shell Upload file.
Compare its SHA256 with the operator's locally reported hash before extraction.
Use Bash in one Cloud Shell session, without `set -x` (never echo tokens).

```bash
set -euo pipefail
umask 077
export PROJECT='esp32-smartwatch-511417'
export REGION='europe-southwest1'
export SERVICE='watch-ai-backend'
export IMAGE_BASE="${REGION}-docker.pkg.dev/${PROJECT}/watch-ai-backend/backend"
sha256sum "$HOME/watch-ai-backend-spanish-fix.zip"
WORKDIR="$(mktemp -d "$HOME/watch-ai-spanish.XXXXXX")"
unzip "$HOME/watch-ai-backend-spanish-fix.zip" -d "$WORKDIR"
cd "$WORKDIR"
sha256sum --check SHA256SUMS
```

## 2. Read-only preflight / rollback snapshot

These local snapshots contain configuration/UIDs, not secret values. Keep them
private; do not upload or commit them. This narrow runbook requires one existing
100%-serving revision and no intervening operator rollout. Stop for split traffic,
multiple containers, incompatible configuration, or pending deployments.

```bash
gcloud run services describe "$SERVICE" --project="$PROJECT" --region="$REGION" \
  --format=json > before-service.json
python3 - <<'PY'
import json
from pathlib import Path
s = json.loads(Path('before-service.json').read_text())
status = s['status']
assert status['latestCreatedRevisionName'] == status['latestReadyRevisionName'], 'Pending/failed deployment: review first'
assert any(c['type'] == 'Ready' and c['status'] == 'True' for c in status['conditions'])
traffic = [t for t in status['traffic'] if t.get('percent', 0)]
assert len(traffic) == 1 and traffic[0]['percent'] == 100, 'Split traffic: review rollback plan first'
spec = s['spec']['template']['spec']
assert len(spec['containers']) == 1
assert spec['serviceAccountName'] == 'watch-ai-backend@esp32-smartwatch-511417.iam.gserviceaccount.com'
env = {e['name']: e for e in spec['containers'][0]['env']}
secret = env['OPENAI_API_KEY']['valueFrom']['secretKeyRef']
assert secret['name'] == 'OPENAI_API_KEY' and secret['key'] == '2', 'Expected existing OPENAI_API_KEY:2; STOP'
assert env['FIREBASE_PROJECT_ID']['value'] == 'esp32-smartwatch-511417'
assert env['ALLOWED_FIREBASE_UIDS']['value'].strip(), 'Empty UID allowlist'
Path('previous-revision.txt').write_text(traffic[0]['revisionName'])
PY
PREVIOUS_REVISION="$(cat previous-revision.txt)"
gcloud run revisions describe "$PREVIOUS_REVISION" --project="$PROJECT" --region="$REGION" \
  --format=json > before-revision.json
python3 - <<'PY'
import json
from pathlib import Path
r = json.loads(Path('before-revision.json').read_text())
env = {e['name']: e for e in r['spec']['containers'][0]['env']}
secret = env['OPENAI_API_KEY']['valueFrom']['secretKeyRef']
assert secret == {'name': 'OPENAI_API_KEY', 'key': '2'}, 'Serving revision must also use version 2; STOP'
Path('previous-image.txt').write_text(r['status']['imageDigest'])
PY
gcloud secrets versions describe 2 --secret=OPENAI_API_KEY --project="$PROJECT" \
  --format='value(state)' | grep -Fx ENABLED
# Metadata only; do NOT run secrets versions access.
```

## 3. Build — only after explicit billing approval

```bash
TAG="spanish-$(date -u +%Y%m%d-%H%M%S)"
IMAGE="${IMAGE_BASE}:${TAG}"
gcloud builds submit . --project="$PROJECT" --region="$REGION" \
  --config=cloudbuild.yaml --substitutions="_IMAGE=${IMAGE}"
DIGEST="$(gcloud artifacts docker images describe "$IMAGE" --project="$PROJECT" \
  --format='value(image_summary.digest)')"
[[ "$DIGEST" =~ ^sha256:[0-9a-f]{64}$ ]] || { echo 'Invalid/missing digest; STOP'; exit 1; }
PINNED_IMAGE="${IMAGE_BASE}@${DIGEST}"
printf '%s\n' "$PINNED_IMAGE" > new-image.txt
```

The existing configured Cloud Build identity is used, never the runtime account.
If permissions fail, stop; do not grant IAM or create infrastructure automatically.
The non-root Docker permission/import regression check executes during this build.

## 4. Stage digest-pinned revision — separately approve deployment

Only image, revision suffix and temporary traffic tag are changed. Existing env,
secret version 2, service account, ingress/IAM, resource limits and quotas remain.
Before running, ensure nobody changed the service since the snapshot.

```bash
NEW_REVISION="${SERVICE}-${TAG}"
gcloud run services update "$SERVICE" --project="$PROJECT" --region="$REGION" \
  --image="$PINNED_IMAGE" --revision-suffix="$TAG" --no-traffic --tag="$TAG"
gcloud run services describe "$SERVICE" --project="$PROJECT" --region="$REGION" \
  --format=json > staged-service.json
gcloud run revisions describe "$NEW_REVISION" --project="$PROJECT" --region="$REGION" \
  --format=json > staged-revision.json
python3 - "$NEW_REVISION" "$TAG" "$DIGEST" <<'PY'
import copy, json, sys
from pathlib import Path
before = json.loads(Path('before-service.json').read_text())
after = json.loads(Path('staged-service.json').read_text())
revision = json.loads(Path('staged-revision.json').read_text())
assert after['status']['latestReadyRevisionName'] == sys.argv[1]
assert revision['status']['imageDigest'].endswith('@' + sys.argv[3])
assert any(c['type'] == 'Ready' and c['status'] == 'True' for c in revision['status']['conditions'])
old_spec = copy.deepcopy(before['spec']['template']['spec'])
new_spec = copy.deepcopy(after['spec']['template']['spec'])
old_spec['containers'][0].pop('image')
new_spec['containers'][0].pop('image')
assert old_spec == new_spec, 'Runtime config changed; STOP and review'
def runtime_annotations(service):
    annotations = dict(service['spec']['template']['metadata'].get('annotations', {}))
    # CLI bookkeeping/image metadata changes are not runtime settings.
    for key in ('client.knative.dev/user-image', 'run.googleapis.com/client-name', 'run.googleapis.com/client-version', 'run.googleapis.com/operation-id'):
        annotations.pop(key, None)
    return annotations
assert runtime_annotations(before) == runtime_annotations(after), 'Revision settings changed; STOP'
old_traffic = [(t['revisionName'], t['percent']) for t in before['status']['traffic'] if t.get('percent', 0)]
new_traffic = [(t['revisionName'], t['percent']) for t in after['status']['traffic'] if t.get('percent', 0)]
assert old_traffic == new_traffic, 'Serving traffic changed; STOP'
url = next(t['url'] for t in after['status']['traffic'] if t.get('tag') == sys.argv[2])
Path('staged-url.txt').write_text(url)
PY
export STAGED_URL="$(cat staged-url.txt)"
```

If validation fails, do not promote. Existing serving traffic should still be on
the previous revision; confirm before taking any rollback action.

## 5. Health/auth smoke — no audio, no OpenAI

Use an existing authorized Firebase ID token, entered at the hidden prompt. Never
paste credentials/tokens into command arguments, shell history or logs. This is
a Firebase token, not a Google Cloud identity token or OpenAI key.

```bash
python3 - <<'PY'
import getpass, json, os, urllib.request, urllib.error
url = os.environ['STAGED_URL']
def request(path, token=None, body=None):
    headers = {'Content-Type': 'audio/wav'}
    if token: headers['Authorization'] = 'Bearer ' + token
    req = urllib.request.Request(url + path, data=body, headers=headers)
    try:
        with urllib.request.urlopen(req, timeout=70) as r: return r.status, r.read(1024)
    except urllib.error.HTTPError as e: return e.code, e.read(1024)
status, body = request('/healthz')
assert status == 200 and json.loads(body) == {'status': 'ok'}
assert request('/v1/voice', body=b'')[0] == 401
assert request('/v1/voice', token='invalid-token', body=b'')[0] == 401
token = getpass.getpass('Existing allowlisted Firebase ID token (hidden): ')
assert token.strip(), 'Token required'
status, body = request('/v1/voice', token=token, body=b'')
token = None
assert status == 400 and json.loads(body)['detail'] == 'Invalid bounded 16 kHz mono PCM WAV'
print('PASS: health, missing/invalid token, allowlisted UID; no valid audio submitted')
PY
```

401 on the authorized probe means rejected/expired token; 403 means UID not
allowed; 503 means service/auth configuration unavailable. Stop rather than
changing IAM/Firebase. If an existing non-allowlisted account token is available,
repeat the empty-body probe with it and expect 403 (do not create users).
The specific allowlisted 400 occurs before Firestore reservation or OpenAI.
These smoke checks do **not** prove Firestore quota writes, OpenAI readiness or
Spanish recognition. Inspect sanitized startup/error logs in Cloud Console;
do not log request headers, recordings, transcripts or secret values.

## 6. Promote after smoke PASS and operator approval

```bash
gcloud run services update-traffic "$SERVICE" --project="$PROJECT" --region="$REGION" \
  --to-revisions="${NEW_REVISION}=100"
gcloud run services describe "$SERVICE" --project="$PROJECT" --region="$REGION" \
  --format='table(status.traffic.revisionName,status.traffic.percent)'
```

Then manually approve ONE short Spanish watch recording. Check transcription,
Spanish (Spain) answer/punctuation and an explicit other-language request only
if another paid test is approved. Mocked tests verify API arguments/policy, not
model outputs. Prompt guidance reduces confusion; it cannot guarantee accuracy.

## 7. Rollback

Keep the previous revision/image and secret version 2 available. Do not delete
revisions/images or disable/destroy secrets as part of this rollout.

```bash
PREVIOUS_REVISION="$(cat previous-revision.txt)"
gcloud run services update-traffic "$SERVICE" --project="$PROJECT" --region="$REGION" \
  --to-revisions="${PREVIOUS_REVISION}=100"
# After confirming serving traffic, remove only this rollout's temporary tag:
gcloud run services update-traffic "$SERVICE" --project="$PROJECT" --region="$REGION" \
  --remove-tags="$TAG"
```

Rollback routes to the saved revision with its original image/config/secret;
it does not reset Firestore quotas or change the service template to the old
image. Subsequent rollouts must explicitly choose a reviewed image. If the prior
revision had a different secret binding, stop and review that snapshot rather
than changing secrets. The service URL remains unchanged. Other tags are retained.
