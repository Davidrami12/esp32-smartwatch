"""Build/run the real Dockerfile locally; no cloud calls, credentials or real AI."""
import io
import json
import shutil
import subprocess
import time
import urllib.error
import urllib.request
import wave
from pathlib import Path


def run(*arguments):
    return subprocess.check_output(["docker", *arguments], text=True, stderr=subprocess.STDOUT).strip()


def request(port, path, data=None, token=None):
    headers = {"Content-Type": "audio/wav"}
    if token:
        headers["Authorization"] = f"Bearer {token}"
    req = urllib.request.Request(f"http://127.0.0.1:{port}{path}", data=data, headers=headers)
    try:
        with urllib.request.urlopen(req, timeout=10) as response:
            return response.status, response.read()
    except urllib.error.HTTPError as error:
        return error.code, error.read()


def wait_health(container):
    port = int(run("port", container, "9090/tcp").rsplit(":", 1)[1])
    for _ in range(100):
        try:
            if request(port, "/healthz")[0] == 200:
                return port
        except (OSError, urllib.error.URLError):
            pass
        time.sleep(0.1)
    raise RuntimeError("Container health endpoint did not start")


def main():
    if not shutil.which("docker"):
        raise SystemExit("SKIPPED: Docker CLI/daemon required; no container PASS claimed")
    try:
        run("info")
    except (OSError, subprocess.CalledProcessError):
        raise SystemExit("SKIPPED: Docker daemon unavailable; no container PASS claimed") from None
    root = Path(__file__).resolve().parent.parent
    image = "watch-voice-backend:local-smoke"
    run("build", "--platform", "linux/amd64", "-t", image, str(root))
    containers = []
    try:
        production = run("run", "-d", "--env", "PORT=9090", "-p", "127.0.0.1::9090", image)
        containers.append(production)
        port = wait_health(production)
        assert run("exec", production, "id", "-u") == "10001"
        assert run("exec", production, "id", "-g") == "10001"
        run("exec", production, "python", "-c",
            "import os; from pathlib import Path; assert os.access('/app',os.R_OK|os.X_OK); Path('/app/app.py').read_text(); import app; assert app.app is not None")
        assert request(port, "/v1/voice", b"no-audio")[0] == 401
        assert request(port, "/v1/voice", b"no-audio", "mock-ok")[0] == 503  # Missing deployment config fails closed.
        mocked = run("run", "-d", "--env", "PORT=9090", "--env", "PYTHONPATH=/app:/smoke", "-p", "127.0.0.1::9090",
                     "--mount", f"type=bind,source={root / 'tests'},target=/smoke,readonly", image,
                     "sh", "-c", 'exec uvicorn container_app:app --host 0.0.0.0 --port "$PORT" --no-access-log --log-level warning')
        containers.append(mocked)
        port = wait_health(mocked)
        buffer = io.BytesIO()
        with wave.open(buffer, "wb") as audio:
            audio.setparams((1, 2, 16000, 0, "NONE", "not compressed"))
            audio.writeframes(b"\0" * 16000)
        wav = buffer.getvalue()
        assert request(port, "/v1/voice", wav, "invalid")[0] == 401
        assert request(port, "/v1/voice", wav, "mock-denied")[0] == 403
        assert request(port, "/v1/voice", b"bad", "mock-ok")[0] == 400
        assert request(port, "/v1/voice", b"x" * 256045, "mock-ok")[0] == 413
        status, body = request(port, "/v1/voice", wav, "mock-ok")
        assert status == 200 and json.loads(body)["transcription"] == "¿Cómo estás?"
        assert request(port, "/v1/voice", wav, "mock-ok")[0] == 429
        assert request(port, "/v1/voice", wav, "mock-error")[0] == 503
        assert request(port, "/v1/voice", wav, "mock-timeout")[0] == 504
        for container in containers:
            logs = run("logs", container)
            assert "¿Cómo" not in logs and "sensitive upstream" not in logs
        print("PASS: image/non-root/PORT/health/auth/bounds/quota/Spanish/error/timeout/log privacy")
    finally:
        for container in containers:
            run("rm", "-f", container)


if __name__ == "__main__":
    main()
