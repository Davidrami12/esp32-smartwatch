import json
from unittest.mock import patch

from fastapi.testclient import TestClient
from test_app import Mock, wav

from app import create_app


def test_correlated_timings_do_not_log_credentials_or_text():
    with patch("latency.logger.info") as info:
        response = TestClient(create_app(Mock())).post('/v1/voice', content=wav(), headers={
            'Authorization': 'Bearer test-user', 'Content-Type': 'audio/wav', 'X-Voice-Trace': 'aabbccdd'})
    assert response.status_code == 200
    records = [json.loads(call.args[0]) for call in info.call_args_list]
    assert {r['trace'] for r in records} == {'aabbccdd'}
    assert {'firebase_authentication', 'firestore_reservation', 'asgi_response_send', 'request_total'} <= {r['stage'] for r in records}
    assert all(set(r) == {'trace', 'stage', 'timestamp_ms', 'duration_ms'} and r['duration_ms'] >= 0 for r in records)
    serialized = json.dumps(records)
    assert 'test-user' not in serialized and 'Madrid' not in serialized and 'mock-uid' not in serialized


def test_untrusted_trace_header_is_not_logged():
    with patch("latency.logger.info") as info:
        TestClient(create_app(Mock())).post('/v1/voice', content=b'', headers={
            'Content-Type': 'audio/wav', 'X-Voice-Trace': 'sensitive-value'})
    assert info.call_args_list
    assert all('sensitive-value' not in c.args[0] for c in info.call_args_list)
