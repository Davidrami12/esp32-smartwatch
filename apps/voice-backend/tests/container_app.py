"""Mock application mounted only by local container tests, never copied into images."""
import asyncio

from fastapi import HTTPException

from app import create_app


class MockProvider:
    def __init__(self):
        self.used = set()

    def authenticate(self, token):
        if token not in {"mock-ok", "mock-denied", "mock-error", "mock-timeout"}:
            raise HTTPException(401, "Authentication required")
        if token == "mock-denied":
            raise HTTPException(403, "Account not enabled")
        return token

    def reserve(self, uid):
        if uid in self.used:
            raise HTTPException(429, "Request limit reached")
        self.used.add(uid)
        self.current = uid

    async def generate(self, body):
        if self.current == "mock-error":
            raise RuntimeError("sensitive upstream content must not escape")
        if self.current == "mock-timeout":
            await asyncio.sleep(10)
        return {"transcription": "¿Cómo estás?", "answer": "Estoy bien, gracias."}


app = create_app(MockProvider(), request_timeout=0.5)
