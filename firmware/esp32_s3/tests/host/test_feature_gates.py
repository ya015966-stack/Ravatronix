from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]


def test_kconfig_safety_gates_default_to_closed():
    config = (ROOT / "Kconfig.projbuild").read_text()
    for option in (
        "RAVA_ENABLE_CAMERA",
        "RAVA_CAMERA_PINMAP_CONFIRMED",
        "RAVA_ENABLE_AUDIO",
        "RAVA_AUDIO_WIRING_CONFIRMED",
        "RAVA_MIC_FORMAT_CONFIRMED",
        "RAVA_ENABLE_CELLULAR",
        "RAVA_MODEM_ELECTRICAL_CONFIRMED",
    ):
        block = re.search(
            rf"config {option}\b(.*?)(?=\n\s*config |\nendmenu)",
            config,
            re.S,
        )
        assert block is not None, f"missing safety gate: {option}"
        assert re.search(r"\bdefault n\b", block.group(1)), option


def test_apn_is_not_assumed_for_an_unconfirmed_sim_operator():
    config = (ROOT / "Kconfig.projbuild").read_text()
    block = re.search(r"config RAVA_MODEM_APN\b(.*?)(?=\n\s*config |\nendmenu)", config, re.S)
    assert block is not None
    assert re.search(r'default ""', block.group(1))



def test_camera_is_enabled_for_initial_capture_while_unverified_modules_stay_off():
    defaults = (ROOT / "sdkconfig.defaults").read_text()
    assert "CONFIG_RAVA_ENABLE_CAMERA=y" in defaults
    assert "CONFIG_RAVA_CAMERA_PINMAP_CONFIRMED=y" in defaults
    assert "CONFIG_RAVA_ENABLE_AUDIO=n" in defaults
    assert "CONFIG_RAVA_ENABLE_CELLULAR=n" in defaults
