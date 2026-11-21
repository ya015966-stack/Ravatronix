#pragma once

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Performs bounded UART diagnostics only (AT, ATE0, AT+GMR).
 * Ready means the modem answered AT commands; it does not mean cellular
 * registration, HTTPS capability, or gateway reachability.
 */
esp_err_t modem_at_service_start(void);
bool modem_at_service_is_ready(void);
const char *modem_at_service_get_firmware_info(void);

#ifdef __cplusplus
}
#endif
