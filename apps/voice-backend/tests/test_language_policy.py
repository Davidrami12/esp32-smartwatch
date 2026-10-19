"""Offline policy/UTF-8 regressions; not evidence of live model accuracy."""
import asyncio
from types import SimpleNamespace
from unittest.mock import AsyncMock

import pytest

from app import Production, bounded


@pytest.mark.parametrize("question,answer", [
    ("Canto é dous máis dous?", "Dos más dos son cuatro."),
    ("Quanto é dois mais dois?", "Dos más dos son cuatro."),
    ("¿Cuánto es dos más dos?", "¡Son cuatro!"),
    ("Responde en portugués: ¿cuánto es dos más dos?", "Dois mais dois são quatro."),
    ("Responde en gallego: ¿cuánto es dos más dos?", "Dous máis dous son catro."),
])
def test_language_policy_and_explicit_requests(question, answer):
    provider = Production.__new__(Production)
    provider.transcription_model = "gpt-4o-mini-transcribe"
    provider.response_model = "gpt-4.1-mini"
    transcribe = AsyncMock(return_value=SimpleNamespace(text=question))
    respond = AsyncMock(return_value=SimpleNamespace(output_text=answer))
    provider.client = SimpleNamespace(
        audio=SimpleNamespace(transcriptions=SimpleNamespace(create=transcribe)),
        responses=SimpleNamespace(create=respond))
    result = asyncio.run(provider.generate(bytearray(b"mock-audio")))
    transcription_args = transcribe.call_args.kwargs
    assert transcription_args["language"] == "es"
    assert "español de España" in transcription_args["prompt"]
    assert "intencionadamente" in transcription_args["prompt"]
    instructions = respond.call_args.kwargs["instructions"]
    assert "por defecto en español de España" in instructions
    assert "gallego o portugués" in instructions
    assert "pida explícitamente otro idioma" in instructions
    assert respond.call_args.kwargs["input"] == question
    assert respond.call_args.kwargs["store"] is False
    assert respond.call_args.kwargs["max_output_tokens"] == 600
    assert result == {"transcription": question, "answer": answer}


def test_spanish_punctuation_and_accents_are_preserved():
    text = "¿Qué tal, España? ¡Sí! áéíóúüñ Ñ; año, pingüino."
    assert bounded(text, 512) == text
    assert bounded("¿" + "ñ" * 300, 512) == "¿" + "ñ" * 255
