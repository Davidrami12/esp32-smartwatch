import io
import wave

from fastapi import HTTPException
from fastapi.testclient import TestClient

from app import bounded, create_app


def wav():
    buffer = io.BytesIO()
    with wave.open(buffer, "wb") as f:
        f.setparams((1, 2, 16000, 0, "NONE", "not compressed"))
        f.writeframes(b"\0" * 16000)
    return buffer.getvalue()


class Mock:
    calls = 0
    def authenticate(self, token):
        if token != "test-user":
            raise HTTPException(401, "Authentication required")
        return "mock-uid"
    def reserve(self, uid):
        if self.calls >= 1:
            raise HTTPException(429, "Daily request limit reached")
        self.calls += 1
    async def generate(self, body):
        return {"transcription": "¿Qué tiempo hace en Madrid?", "answer": "No tengo datos meteorológicos en tiempo real."}


def test_mock_audio_to_spanish_response():
    mock = Mock()
    client = TestClient(create_app(mock))
    response = client.post("/v1/voice", content=wav(), headers={"Authorization": "Bearer test-user", "Content-Type": "audio/wav"})
    assert response.status_code == 200
    assert "¿Qué" in response.json()["transcription"]
    assert "meteorológicos" in response.json()["answer"]
    assert client.post("/v1/voice", content=wav(), headers={"Authorization": "Bearer test-user", "Content-Type": "audio/wav"}).status_code == 429


def test_auth_and_audio_bounds():
    client = TestClient(create_app(Mock()))
    assert client.post("/v1/voice", content=wav()).status_code == 401
    headers = {"Authorization": "Bearer test-user", "Content-Type": "audio/wav"}
    assert client.post("/v1/voice", content=b"bad", headers=headers).status_code == 400
    assert client.post("/v1/voice", content=b"a" * 256045, headers=headers).status_code == 413
    assert client.post("/v1/voice", content=wav(), headers={**headers,"Authorization":"Bearer wrong"}).status_code == 401


def test_utf8_bounds():
    assert bounded("á" * 1000, 513) == "á" * 256


def test_official_sdk_calls_mocked_through_endpoint():
    from types import SimpleNamespace
    from unittest.mock import AsyncMock

    from app import Production
    provider = Production.__new__(Production)
    provider.transcription_model = "gpt-4o-mini-transcribe"
    provider.response_model = "gpt-4.1-mini"
    transcription = AsyncMock(return_value=SimpleNamespace(text="¿Cómo estás?"))
    response = AsyncMock(return_value=SimpleNamespace(output_text="Estoy bien, gracias."))
    provider.client = SimpleNamespace(audio=SimpleNamespace(transcriptions=SimpleNamespace(create=transcription)), responses=SimpleNamespace(create=response))
    provider.authenticate = lambda token: "mock-uid"
    provider.reserve = lambda uid: None
    client = TestClient(create_app(provider))
    result = client.post("/v1/voice", content=wav(), headers={"Authorization":"Bearer mock-token", "Content-Type":"audio/wav"})
    assert result.status_code == 200
    assert result.json() == {"transcription":"¿Cómo estás?", "answer":"Estoy bien, gracias."}
    assert transcription.call_args.kwargs["language"] == "es"
    assert transcription.call_args.kwargs["file"][1] == wav()
    assert response.call_args.kwargs["store"] is False
    assert response.call_args.kwargs["max_output_tokens"] == 600


def test_provider_errors_do_not_leak_sensitive_details():
    class Broken(Mock):
        async def generate(self, body):
            raise RuntimeError("private transcript or credential")
    result = TestClient(create_app(Broken())).post("/v1/voice", content=wav(), headers={"Authorization":"Bearer test-user", "Content-Type":"audio/wav"})
    assert result.status_code == 503
    assert "private" not in result.text


def test_duplicate_rejection_does_not_release_original_slot():
    import asyncio

    import httpx
    async def scenario():
        started, release = asyncio.Event(), asyncio.Event()
        class Slow(Mock):
            async def generate(self, body):
                started.set()
                await release.wait()
                return {"transcription":"Hola", "answer":"Hola"}
        async with httpx.AsyncClient(transport=httpx.ASGITransport(app=create_app(Slow())),base_url="http://mock") as client:
            headers = {"Authorization":"Bearer test-user", "Content-Type":"audio/wav"}
            first = asyncio.create_task(client.post("/v1/voice",content=wav(),headers=headers))
            await started.wait()
            assert (await client.post("/v1/voice",content=wav(),headers=headers)).status_code == 429
            assert (await client.post("/v1/voice",content=wav(),headers=headers)).status_code == 429
            release.set()
            assert (await first).status_code == 200
    asyncio.run(scenario())


def test_timeout_cancels_generation_and_wipes_audio():
    import asyncio
    class Stuck(Mock):
        cancelled = False
        captured = None
        async def generate(self, body):
            self.captured = body
            try:
                await asyncio.sleep(10)
            finally:
                self.cancelled = True
    provider = Stuck()
    result = TestClient(create_app(provider,request_timeout=0.05)).post("/v1/voice",content=wav(),headers={"Authorization":"Bearer test-user", "Content-Type":"audio/wav"})
    assert result.status_code == 504
    assert provider.cancelled and not any(provider.captured)


def test_disconnect_cancels_generation():
    import asyncio
    async def scenario():
        started = asyncio.Event()
        class Stuck(Mock):
            cancelled = False
            async def generate(self, body):
                started.set()
                try:
                    await asyncio.sleep(10)
                finally:
                    self.cancelled = True
        provider = Stuck()
        queue = asyncio.Queue()
        await queue.put({"type":"http.request", "body":wav(), "more_body":False})
        responses = []
        async def send(event):
            responses.append(event)
        scope = {"type":"http", "asgi":{"version":"3.0"}, "method":"POST", "path":"/v1/voice", "raw_path":b"/v1/voice", "query_string":b"", "scheme":"http", "http_version":"1.1", "headers":[(b"authorization",b"Bearer test-user"),(b"content-type",b"audio/wav")], "server":("mock",80), "client":("mock",1)}
        task = asyncio.create_task(create_app(provider)(scope,queue.get,send))
        await started.wait()
        await queue.put({"type":"http.disconnect"})
        await asyncio.wait_for(task,2)
        assert provider.cancelled
        assert next(event["status"] for event in responses if event["type"] == "http.response.start") == 499
    asyncio.run(scenario())


def test_public_health_and_fail_closed_missing_configuration(monkeypatch):
    monkeypatch.delenv("FIREBASE_PROJECT_ID", raising=False)
    client = TestClient(create_app())
    assert client.get("/healthz").json() == {"status":"ok"}
    assert client.post("/v1/voice", content=wav(),headers={"Content-Type":"audio/wav"}).status_code == 401
    result = client.post("/v1/voice",content=wav(),headers={"Content-Type":"audio/wav","Authorization":"Bearer fake-token"})
    assert result.status_code == 503
    assert "FIREBASE_PROJECT_ID" not in result.text


def test_firebase_verification_uses_revocation_and_allowlist():
    from types import SimpleNamespace
    from unittest.mock import Mock as MockFunction

    import pytest

    from app import Production
    provider = Production.__new__(Production)
    verify = MockFunction(return_value={"uid":"allowed"})
    provider.auth = SimpleNamespace(verify_id_token=verify)
    provider.allowed = {"allowed"}
    assert provider.authenticate("mock-token") == "allowed"
    verify.assert_called_with("mock-token",check_revoked=True)
    verify.return_value = {"uid":"other-project-user"}
    with pytest.raises(HTTPException) as forbidden:
        provider.authenticate("mock-token")
    assert forbidden.value.status_code == 403
    verify.side_effect = ValueError("expired/revoked credential details")
    with pytest.raises(HTTPException) as invalid:
        provider.authenticate("mock-token")
    assert invalid.value.status_code == 401 and "credential" not in invalid.value.detail
