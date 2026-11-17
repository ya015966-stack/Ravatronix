from pathlib import Path
ROOT = Path(__file__).resolve().parents[2]
def test_voice_client_uses_authenticated_https_and_bounded_audio():
    source = (ROOT / "components/voice_client/voice_client.c").read_text()
    header = (ROOT / "components/voice_client/include/voice_client.h").read_text()
    assert 'strncmp(config->https_url, "https://", 8)' in source
    assert '"Content-Type","audio/wav"' in source
    assert '"Authorization",authorization' in source
    assert "MAX_RECORD_MS 8000U" in source
    assert "MAX_RESPONSE_WAV_BYTES" in source
    assert "esp_crt_bundle_attach" in source
    assert "voice_client_turn" in header
    assert "bearer_token" in header
    assert "RAVA_AI_API_KEY" not in source
def test_voice_client_does_not_start_microphone_without_explicit_turn_request():
    app = (ROOT / "main/app_main.c").read_text()
    assert "voice_client_turn(" not in app
