#pragma once
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

esp_err_t audio_service_init(void);
esp_err_t audio_service_read_pcm(int32_t *dst, size_t capacity_samples,
                                 size_t *samples_read, uint32_t timeout_ms);
esp_err_t audio_service_write_pcm(const int16_t *src, size_t samples,
                                  size_t *samples_written, uint32_t timeout_ms);
