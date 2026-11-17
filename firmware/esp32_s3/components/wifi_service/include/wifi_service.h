#pragma once

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    WIFI_SERVICE_STATE_STOPPED = 0,
    WIFI_SERVICE_STATE_PROVISIONING,
    WIFI_SERVICE_STATE_CONNECTING,
    WIFI_SERVICE_STATE_CONNECTED,
    WIFI_SERVICE_STATE_FAILED
} wifi_service_state_t;

esp_err_t wifi_service_start(void);
esp_err_t wifi_service_set_credentials(const char *ssid, const char *password);
wifi_service_state_t wifi_service_get_state(void);
bool wifi_service_is_ready(void);

#ifdef __cplusplus
}
#endif
