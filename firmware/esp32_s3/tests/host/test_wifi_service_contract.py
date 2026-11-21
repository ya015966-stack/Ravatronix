from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def test_wifi_service_uses_sta_nvs_and_temporary_provisioning_portal():
    source_path = ROOT / "components/wifi_service/wifi_service.c"
    header_path = ROOT / "components/wifi_service/include/wifi_service.h"
    assert source_path.is_file(), "Wi-Fi service implementation is missing"
    assert header_path.is_file(), "Wi-Fi service public contract is missing"

    source = source_path.read_text()
    header = header_path.read_text()
    assert "esp_wifi_set_mode(WIFI_MODE_STA)" in source
    assert "esp_netif_create_default_wifi_sta" in source
    assert "nvs_open" in source
    assert "nvs_get_str" in source
    assert "nvs_set_str" in source
    assert "httpd_start" in source
    assert "WIFI_PROVISIONING_TIMEOUT_MS" in source
    assert "while (received_total < request->content_len)" in source
    assert '"/config"' in source
    assert "esp_wifi_disconnect" in source
    assert "WIFI_EVENT_STA_DISCONNECTED" in source
    assert "IP_EVENT_STA_GOT_IP" in source
    assert "wifi_service_get_state" in header
    assert "wifi_service_start" in header


def test_wifi_setup_passphrase_logging_is_opt_in_and_disabled_by_default():
    source = (ROOT / "components/wifi_service/wifi_service.c").read_text()
    kconfig = (ROOT / "main/Kconfig.projbuild").read_text()
    assert "#if CONFIG_RAVA_WIFI_LOG_SETUP_SECRET" in source
    assert "temporary setup AP: RAVA-SETUP; passphrase: %s" in source
    option = kconfig.split("config RAVA_WIFI_LOG_SETUP_SECRET", 1)[1]
    assert "default n" in option
