"""Content-free stage telemetry: random clip ID only, never user/account data."""
import contextvars
import json
import logging
import re
import time
import uuid

trace = contextvars.ContextVar("voice_trace", default="offline")
logger = logging.getLogger("voice_latency")
logger.setLevel(logging.INFO)
if not logger.handlers:
    logger.addHandler(logging.StreamHandler())
logger.propagate = False


def mark(stage, started):
    logger.info(json.dumps({"trace": trace.get(), "stage": stage,
                            "timestamp_ms": int(time.time()*1000),
                            "duration_ms": round((time.perf_counter()-started)*1000, 3)}))


class LatencyMiddleware:
    def __init__(self, app):
        self.app = app

    async def __call__(self, scope, receive, send):
        if scope["type"] != "http" or scope["path"] != "/v1/voice":
            return await self.app(scope, receive, send)
        candidate = next((v.decode("ascii", "ignore") for k, v in scope["headers"] if k == b"x-voice-trace"), "")
        identity = candidate if re.fullmatch(r"[0-9a-f]{8}", candidate) else uuid.uuid4().hex[:8]
        token = trace.set(identity)
        started = time.perf_counter()
        transmission = None
        async def timed_send(message):
            nonlocal transmission
            if message["type"] == "http.response.start":
                transmission = time.perf_counter()
                mark("response_ready", started)
            await send(message)
            if message["type"] == "http.response.body" and not message.get("more_body", False) and transmission is not None:
                mark("asgi_response_send", transmission)
        try:
            await self.app(scope, receive, timed_send)
        finally:
            mark("request_total", started)
            trace.reset(token)
