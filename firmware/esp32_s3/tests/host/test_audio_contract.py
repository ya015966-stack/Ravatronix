from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

def test_audio_service_has_independent_i2s_rx_and_tx():
    source = (ROOT / "components/audio_service/audio_service.c").read_text()
    header = (ROOT / "components/audio_service/include/audio_service.h").read_text()
    assert "I2S_NUM_0" in source and "s_mic_rx" in source
    assert "I2S_NUM_1" in source and "s_amp_tx" in source
    assert "audio_service_read_pcm" in header
    assert "audio_service_write_pcm" in header
    assert "pdMS_TO_TICKS(timeout_ms)" in source
