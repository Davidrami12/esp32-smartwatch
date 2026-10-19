from pathlib import Path


def test_explicit_modes_and_non_root_build_regression_check():
    dockerfile = (Path(__file__).resolve().parents[1] / "Dockerfile").read_text()
    # Textual guard only; actual Linux permission/import assertions execute in Docker RUN.
    assert "# syntax=docker/dockerfile:1" in dockerfile
    assert "install -d -o 0 -g 10001 -m 0755 /app" in dockerfile
    assert "COPY --chown=0:10001 --chmod=0644 app.py ." in dockerfile
    assert "COPY --chown=0:10001 --chmod=0644 requirements.txt ." in dockerfile
    user_position = dockerfile.index("USER 10001:10001")
    check_position = dockerfile.index('RUN python -c "import os, stat;')
    assert user_position < check_position < dockerfile.index("CMD ")
    assert "Path('/app/app.py').read_text(); import app" in dockerfile
    assert "USER root" not in dockerfile
    cloudbuild = (Path(__file__).resolve().parents[1] / "cloudbuild.yaml").read_text()
    assert "env: ['DOCKER_BUILDKIT=1']" in cloudbuild
