#pragma once

#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct network_transport_context *network_transport_handle_t;

/*
 * Opens one pinned HTTPS POST request. The returned handle remains bound to
 * the selected adapter for the full request; this implementation currently
 * supplies the ESP-IDF Wi-Fi/socket adapter only.
 */
esp_err_t network_transport_open(const char *https_url,
                                 uint32_t timeout_ms,
                                 uint32_t content_length,
                                 const char *content_type,
                                 const char *authorization,
                                 network_transport_handle_t *out_handle);

esp_err_t network_transport_write(network_transport_handle_t handle,
                                  const uint8_t *data,
                                  size_t length,
                                  size_t *written);

esp_err_t network_transport_finish(network_transport_handle_t handle,
                                   int *http_status,
                                   int64_t *response_content_length);

esp_err_t network_transport_read(network_transport_handle_t handle,
                                 uint8_t *buffer,
                                 size_t capacity,
                                 size_t *received);

/* Closes and releases the request. No implicit retries or POST replay occur. */
void network_transport_close(network_transport_handle_t handle);

#ifdef __cplusplus
}
#endif
