from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

def test_board_profile_declares_expected_controller_and_memory():
    header = (ROOT / "components/board_config/include/board_config.h").read_text()
    defaults = (ROOT / "sdkconfig.defaults").read_text()
    assert 'RAVA_BOARD_NAME "ESP32-S3-N16R8"' in header
    assert "RAVA_EXPECTED_PSRAM_BYTES (8U * 1024U * 1024U)" in header
    assert 'CONFIG_IDF_TARGET="esp32s3"' in defaults
    assert "CONFIG_ESPTOOLPY_FLASHSIZE_16MB=y" in defaults
    assert "CONFIG_SPIRAM_MODE_OCT=y" in defaults

def test_no_credentials_or_legacy_wifi_password_in_core_sources():
    paths = [ROOT / "components/board_config/include/board_config.h",
             ROOT / "main/app_main.c", ROOT / "sdkconfig.defaults"]
    for path in paths:
        text = path.read_text().lower()
        assert "ravamcam2026" not in text
        assert "password=" not in text
        assert "bearer " not in text
