#pragma once
#include <stdbool.h>
#include "esp_err.h"

esp_err_t network_service_start(void);
bool network_service_is_ready(void);
