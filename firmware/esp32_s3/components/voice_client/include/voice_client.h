#pragma once
#include <stdint.h>
#include "esp_err.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    const char *https_url;
    const char *bearer_token;
    uint32_t record_ms;
    uint32_t request_timeout_ms;
    unsigned mic_right_shift;
} voice_client_config_t;
typedef struct {
    uint32_t captured_samples;
    uint32_t response_audio_bytes;
    int http_status;
} voice_client_result_t;
esp_err_t voice_client_turn(const voice_client_config_t *config, voice_client_result_t *result);
#ifdef __cplusplus
}
#endif
