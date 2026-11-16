#include "backend_client.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include <string.h>
#include <stdio.h>
#include <limits.h>

esp_err_t backend_client_upload_snapshot(const char *https_url,
                                         const char *bearer_token,
                                         const camera_frame_t *frame,
                                         int timeout_ms,
                                         int *http_status)
{
    if (!https_url || !bearer_token || !frame || !frame->data ||
        frame->length == 0 || timeout_ms <= 0 || !http_status) {
        return ESP_ERR_INVALID_ARG;
    }
    if (frame->length > INT_MAX) return ESP_ERR_INVALID_SIZE;
    *http_status = 0;
    if (strncmp(https_url, "https://", 8) != 0) return ESP_ERR_INVALID_ARG;

    esp_http_client_config_t cfg = {
        .url = https_url,
        .method = HTTP_METHOD_POST,
        .timeout_ms = timeout_ms,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .buffer_size = 1024,
        .buffer_size_tx = 1024
    };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (!client) return ESP_ERR_NO_MEM;

    esp_err_t err = esp_http_client_set_header(client, "Content-Type", "image/jpeg");
    if (err == ESP_OK) {
        char auth[384];
        int n = snprintf(auth, sizeof(auth), "Bearer %s", bearer_token);
        if (n < 0 || (size_t)n >= sizeof(auth)) err = ESP_ERR_INVALID_SIZE;
        else err = esp_http_client_set_header(client, "Authorization", auth);
    }
    if (err == ESP_OK) err = esp_http_client_open(client, (int)frame->length);
    if (err == ESP_OK) {
        int written = esp_http_client_write(client, (const char *)frame->data, (int)frame->length);
        if (written != (int)frame->length) err = ESP_FAIL;
    }
    if (err == ESP_OK) {
        int64_t content_length = esp_http_client_fetch_headers(client);
        (void)content_length;
        *http_status = esp_http_client_get_status_code(client);
    }
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    return err;
}
