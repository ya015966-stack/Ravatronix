from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def test_voice_client_uses_authenticated_https_and_bounded_audio():
    source = (ROOT / "components/voice_client/voice_client.c").read_text()
    transport = (ROOT / "components/network_transport/network_transport.c").read_text()
    normalized_source = "".join(source.split())
    header = (ROOT / "components/voice_client/include/voice_client.h").read_text()
    assert 'strncmp(config->https_url,"https://",8)' in normalized_source
    assert "network_transport_open(" in source
    assert "network_transport_write(" in source
    assert "network_transport_finish(" in source
    assert "network_transport_read(" in source
    assert "MAX_RECORD_MS 8000U" in source
    assert "MAX_RESPONSE_WAV_BYTES" in source
    assert "esp_crt_bundle_attach" in transport
    assert '"Content-Type", content_type' in transport
    assert '"Authorization", authorization' in transport
    assert "voice_client_turn" in header
    assert "bearer_token" in header
    assert "RAVA_AI_API_KEY" not in source
    assert "esp_http_client" not in source


def test_transport_pins_request_and_does_not_retry_ambiguous_post():
    source = (ROOT / "components/network_transport/network_transport.c").read_text()
    header = (ROOT / "components/network_transport/include/network_transport.h").read_text()
    assert "network_transport_open" in source
    assert "network_transport_close" in source
    assert "esp_http_client_open" in source
    assert "esp_http_client_write" in source
    assert "esp_http_client_cleanup" in source
    assert "retry" not in source.lower()
    assert "No implicit retries or POST replay" in header


def test_voice_client_does_not_start_microphone_without_explicit_turn_request():
    app = (ROOT / "main/app_main.c").read_text()
    assert "voice_client_turn(" not in app
