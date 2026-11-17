#include "network_transport.h"

#include "esp_crt_bundle.h"
#include "esp_http_client.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#define NETWORK_TRANSPORT_MAX_TIMEOUT_MS 90000U
#define NETWORK_TRANSPORT_MAX_HEADER 512U

struct network_transport_context {
    esp_http_client_handle_t client;
    bool finished;
};

esp_err_t network_transport_open(const char *https_url,
                                 uint32_t timeout_ms,
                                 uint32_t content_length,
                                 const char *content_type,
                                 const char *authorization,
                                 network_transport_handle_t *out_handle)
{
    if (!https_url || !content_type || !authorization || !out_handle ||
        strncmp(https_url, "https://", 8) != 0 ||
        timeout_ms < 1000U || timeout_ms > NETWORK_TRANSPORT_MAX_TIMEOUT_MS ||
        content_length == 0U ||
        strlen(content_type) >= NETWORK_TRANSPORT_MAX_HEADER ||
        strlen(authorization) >= NETWORK_TRANSPORT_MAX_HEADER) {
        return ESP_ERR_INVALID_ARG;
    }

    *out_handle = NULL;
    struct network_transport_context *ctx = calloc(1, sizeof(*ctx));
    if (!ctx) return ESP_ERR_NO_MEM;

    esp_http_client_config_t config = {
        .url = https_url,
        .method = HTTP_METHOD_POST,
        .timeout_ms = (int)timeout_ms,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .buffer_size = 1024,
        .buffer_size_tx = 1024,
        .keep_alive_enable = false
    };
    ctx->client = esp_http_client_init(&config);
    if (!ctx->client) {
        free(ctx);
        return ESP_ERR_NO_MEM;
    }

    esp_err_t err = esp_http_client_set_header(ctx->client, "Content-Type", content_type);
    if (err == ESP_OK) {
        err = esp_http_client_set_header(ctx->client, "Authorization", authorization);
    }
    if (err == ESP_OK) {
        err = esp_http_client_open(ctx->client, (int)content_length);
    }
    if (err != ESP_OK) {
        esp_http_client_cleanup(ctx->client);
        free(ctx);
        return err;
    }

    *out_handle = ctx;
    return ESP_OK;
}

esp_err_t network_transport_write(network_transport_handle_t handle,
                                  const uint8_t *data,
                                  size_t length,
                                  size_t *written)
{
    if (!handle || !data || length == 0U || !written || handle->finished) {
        return ESP_ERR_INVALID_ARG;
    }
    *written = 0U;
    if (length > (size_t)INT32_MAX) return ESP_ERR_INVALID_SIZE;
    int result = esp_http_client_write(handle->client, (const char *)data, (int)length);
    if (result <= 0) return ESP_FAIL;
    *written = (size_t)result;
    return ESP_OK;
}

esp_err_t network_transport_finish(network_transport_handle_t handle,
                                   int *http_status,
                                   int64_t *response_content_length)
{
    if (!handle || !http_status || !response_content_length || handle->finished) {
        return ESP_ERR_INVALID_ARG;
    }
    int64_t length = esp_http_client_fetch_headers(handle->client);
    *http_status = esp_http_client_get_status_code(handle->client);
    *response_content_length = length;
    handle->finished = true;
    return ESP_OK;
}

esp_err_t network_transport_read(network_transport_handle_t handle,
                                 uint8_t *buffer,
                                 size_t capacity,
                                 size_t *received)
{
    if (!handle || !buffer || capacity == 0U || !received || !handle->finished) {
        return ESP_ERR_INVALID_ARG;
    }
    *received = 0U;
    if (capacity > (size_t)INT32_MAX) return ESP_ERR_INVALID_SIZE;
    int result = esp_http_client_read(handle->client, (char *)buffer, (int)capacity);
    if (result < 0) return ESP_FAIL;
    *received = (size_t)result;
    return ESP_OK;
}

void network_transport_close(network_transport_handle_t handle)
{
    if (!handle) return;
    if (handle->client) {
        esp_http_client_close(handle->client);
        esp_http_client_cleanup(handle->client);
    }
    free(handle);
}
