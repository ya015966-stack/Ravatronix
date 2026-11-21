from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
KCONFIG = ROOT / "main" / "Kconfig.projbuild"


def test_project_kconfig_is_discovered_from_main_directory():
    assert KCONFIG.is_file(), "ESP-IDF project options must live in main/Kconfig.projbuild"
    assert not (ROOT / "Kconfig.projbuild").exists(), (
        "root-level Kconfig.projbuild is not discovered for this project layout"
    )


def test_kconfig_safety_gates_default_to_closed():
    config = KCONFIG.read_text()
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
    config = KCONFIG.read_text()
    block = re.search(r'config RAVA_MODEM_APN\b(.*?)(?=\n\s*config |\nendmenu)', config, re.S)
    assert block is not None
    assert re.search(r'default ""', block.group(1))


def test_camera_remains_enabled_but_unverified_audio_and_cellular_stay_gated():
    defaults = (ROOT / "sdkconfig.defaults").read_text()
    assert "CONFIG_RAVA_ENABLE_CAMERA=y" in defaults
    assert "CONFIG_RAVA_CAMERA_PINMAP_CONFIRMED=y" in defaults
    assert "CONFIG_RAVA_ENABLE_AUDIO=n" in defaults
    assert "CONFIG_RAVA_AUDIO_WIRING_CONFIRMED=n" in defaults
    assert "CONFIG_RAVA_MIC_FORMAT_CONFIRMED=n" in defaults
    assert "CONFIG_RAVA_ENABLE_CELLULAR=n" in defaults


def test_managed_dependencies_are_declared_in_their_component_manifests():
    main_manifest = (ROOT / "main" / "idf_component.yml").read_text()
    camera_manifest = (ROOT / "components" / "camera_service" / "idf_component.yml").read_text()
    network_manifest = (ROOT / "components" / "network_service" / "idf_component.yml").read_text()
    assert 'idf: ">=5.3.2,<5.4.0"' in main_manifest
    assert 'espressif/esp32-camera: "2.1.9"' in camera_manifest
    assert 'espressif/esp_modem: "1.4.0"' in network_manifest
    assert not (ROOT / "idf_component.yml").exists()
