# syntax=docker/dockerfile:1
FROM python:3.12-slim-bookworm
ENV PYTHONDONTWRITEBYTECODE=1 PYTHONUNBUFFERED=1 PIP_DISABLE_PIP_VERSION_CHECK=1
RUN groupadd --gid 10001 appuser \
    && useradd --uid 10001 --gid 10001 --create-home appuser \
    && install -d -o 0 -g 10001 -m 0755 /app
WORKDIR /app
COPY --chown=0:10001 --chmod=0644 requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt
COPY --chown=0:10001 --chmod=0644 app.py .
COPY --chown=0:10001 --chmod=0644 latency.py .
USER 10001:10001
# Fail the build if the actual non-root runtime cannot traverse/read/import the app.
RUN python -c "import os, stat; from pathlib import Path; assert os.getuid() == 10001 and os.getgid() == 10001; assert os.access('/app', os.R_OK | os.X_OK); assert stat.S_IMODE(os.stat('/app').st_mode) == 0o755; assert all(os.access(p, os.R_OK) and stat.S_IMODE(os.stat(p).st_mode) == 0o644 for p in ('/app/app.py', '/app/requirements.txt')); Path('/app/app.py').read_text(); import app; assert app.app is not None"
ENV PORT=8080
EXPOSE 8080
CMD ["sh", "-c", "exec uvicorn app:app --host 0.0.0.0 --port ${PORT} --no-access-log --log-level warning"]
