#include "network_service.h"
#include "esp_log.h"

/*
 * Legacy PPP entry point retained for source compatibility only.
 * Ravatronix uses the A7670E internal AT data stack; PPP is intentionally
 * not started by this component.
 */
esp_err_t network_service_start(void)
{
    ESP_LOGW("rava_network", "legacy PPP service is retired; use modem_at_service");
    return ESP_ERR_NOT_SUPPORTED;
}

bool network_service_is_ready(void)
{
    return false;
}
