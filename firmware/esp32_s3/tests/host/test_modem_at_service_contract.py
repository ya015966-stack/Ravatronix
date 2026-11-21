from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def test_modem_startup_is_at_diagnostic_only_and_hardware_gated():
    app = (ROOT / "main/app_main.c").read_text()
    source = (ROOT / "components/modem_at_service/modem_at_service.c").read_text()
    header = (ROOT / "components/modem_at_service/include/modem_at_service.h").read_text()
    kconfig = (ROOT / "main/Kconfig.projbuild").read_text()

    assert "modem_at_service_start()" in app
    assert "RAVA_MODEM_AT_UART_CONFIRMED" in app
    assert "CONFIG_RAVA_MODEM_AT_UART_CONFIRMED" in source
    assert 'send_command("AT\\r\\n"' in source
    assert 'send_command("ATE0\\r\\n"' in source
    assert 'send_command("AT+GMR\\r\\n"' in source
    assert "MODEM_COMMAND_TIMEOUT_MS" in source
    assert "AT diagnostic only" in header
    assert "config RAVA_MODEM_AT_UART_CONFIRMED" in kconfig
    assert "ESP_MODEM_MODE_DATA" not in app
    assert "network_service_start()" not in app


def test_legacy_network_service_cannot_start_ppp():
    source = (ROOT / "components/network_service/network_service.c").read_text()
    assert "ESP_MODEM_MODE_DATA" not in source
    assert "ESP_MODEM_MODE_COMMAND" not in source
    assert "ESP_ERR_NOT_SUPPORTED" in source
