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
    assert '"/config"' in source
    assert "esp_wifi_disconnect" in source
    assert "WIFI_EVENT_STA_DISCONNECTED" in source
    assert "IP_EVENT_STA_GOT_IP" in source
    assert "wifi_service_get_state" in header
    assert "wifi_service_start" in header


def test_wifi_service_does_not_log_saved_network_credentials():
    source_path = ROOT / "components/wifi_service/wifi_service.c"
    if not source_path.is_file():
        return
    source = source_path.read_text()
    assert 'ESP_LOGI(TAG, "Wi-Fi station acquired an IP address")' in source
    assert 'ESP_LOGI(TAG, "Wi-Fi password' not in source
    assert 'ESP_LOGI(TAG, "SSID:' not in source
    assert 'ESP_LOGI(TAG, "Authorization:' not in source
    assert 'ESP_LOGW(TAG, "temporary setup AP: RAVA-SETUP; passphrase: %s' in source
