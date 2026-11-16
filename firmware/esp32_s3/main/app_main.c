#include <stdio.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_psram.h"
#include "esp_heap_caps.h"
#include "board_config.h"
#include "camera_service.h"
#include "audio_service.h"

static const char *TAG = "ravatronix";

static void audio_monitor_task(void *arg)
{
    int32_t samples[256];
    while (1) {
        size_t count = 0;
        esp_err_t err = audio_service_read_pcm(samples, 256, &count, 500);
        if (err == ESP_OK && count > 0) {
            double sum = 0.0;
            for (size_t i = 0; i < count; ++i) {
                int32_t s = samples[i] >> 14;
                sum += (double)s * (double)s;
            }
            ESP_LOGI("rava_audio", "mic rms=%.0f samples=%u",
                     sqrt(sum / count), (unsigned)count);
        } else if (err != ESP_ERR_TIMEOUT) {
            ESP_LOGW("rava_audio", "mic read: %s", esp_err_to_name(err));
        }
        vTaskDelay(pdMS_TO_TICKS(250));
    }
}

static void camera_monitor_task(void *arg)
{
    while (1) {
        camera_frame_t frame = {0};
        esp_err_t err = camera_service_capture_jpeg(&frame);
        if (err == ESP_OK) {
            ESP_LOGI("rava_camera", "JPEG snapshot %ux%u, %u bytes",
                     frame.width, frame.height, (unsigned)frame.length);
            camera_service_release(&frame);
        } else {
            ESP_LOGW("rava_camera", "capture: %s", esp_err_to_name(err));
        }
        vTaskDelay(pdMS_TO_TICKS(30000));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "Ravatronix integrated firmware boot");
    ESP_LOGI(TAG, "Board: %s", RAVA_BOARD_NAME);
    ESP_LOGI(TAG, "Free heap: %u bytes", (unsigned)esp_get_free_heap_size());
    ESP_LOGI(TAG, "PSRAM: %u bytes", (unsigned)esp_psram_get_size());

    esp_err_t cam_err = camera_service_init();
    if (cam_err == ESP_OK) {
        xTaskCreate(camera_monitor_task, "camera_monitor", 4096, NULL, 4, NULL);
    } else {
        ESP_LOGE(TAG, "camera service unavailable: %s", esp_err_to_name(cam_err));
    }

    esp_err_t audio_err = audio_service_init();
    if (audio_err == ESP_OK) {
        xTaskCreate(audio_monitor_task, "audio_monitor", 4096, NULL, 5, NULL);
    } else {
        ESP_LOGE(TAG, "audio service unavailable: %s", esp_err_to_name(audio_err));
    }

    while (1) {
        ESP_LOGI(TAG, "heartbeat heap=%u psram_free=%u",
                 (unsigned)esp_get_free_heap_size(),
                 (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}
