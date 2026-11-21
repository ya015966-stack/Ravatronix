#pragma once
#include <stdint.h>
#include "esp_err.h"
#include "camera_service.h"

/* URL and bearer token are supplied at runtime; never compile credentials in. */
esp_err_t backend_client_upload_snapshot(const char *https_url,
                                         const char *bearer_token,
                                         const camera_frame_t *frame,
                                         int timeout_ms,
                                         int *http_status);
