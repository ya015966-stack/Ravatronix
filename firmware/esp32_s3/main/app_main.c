#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_psram.h"
#include "esp_heap_caps.h"

#if CONFIG_RAVA_ENABLE_CAMERA && CONFIG_RAVA_CAMERA_PINMAP_CONFIRMED
#include "camera_service.h"
#endif

#if CONFIG_RAVA_ENABLE_AUDIO && CONFIG_RAVA_AUDIO_WIRING_CONFIRMED && CONFIG_RAVA_MIC_FORMAT_CONFIRMED
#include "audio_service.h"
#endif

#if CONFIG_RAVA_ENABLE_CELLULAR && CONFIG_RAVA_MODEM_ELECTRICAL_CONFIRMED
#include "network_service.h"
#endif

static const char *TAG = "ravatronix";

#if CONFIG_RAVA_ENABLE_CAMERA && CONFIG_RAVA_CAMERA_PINMAP_CONFIRMED
static void camera_monitor_task(void *arg)
{
    (void)arg;
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
#endif

#if CONFIG_RAVA_ENABLE_AUDIO && CONFIG_RAVA_AUDIO_WIRING_CONFIRMED && CONFIG_RAVA_MIC_FORMAT_CONFIRMED
static void audio_monitor_task(void *arg)
{
    (void)arg;
    int32_t samples[256];
    while (1) {
        size_t count = 0;
        esp_err_t err = audio_service_read_pcm(samples, 256, &count, 500);
        if (err == ESP_OK && count > 0) {
            int64_t sum = 0;
            for (size_t i = 0; i < count; ++i) {
                int32_t sample = samples[i] >> 14;
                sum += (int64_t)sample * sample;
            }
            ESP_LOGI("rava_audio", "mic rms=%lld samples=%u",
                     (long long)sqrt((double)sum / count), (unsigned)count);
        } else if (err != ESP_ERR_TIMEOUT) {
            ESP_LOGW("rava_audio", "mic read: %s", esp_err_to_name(err));
        }
        vTaskDelay(pdMS_TO_TICKS(250));
    }
}
#endif

void app_main(void)
{
    ESP_LOGI(TAG, "Ravatronix ESP32-S3-N16R8 safe boot");
    ESP_LOGI(TAG, "Free heap: %u bytes", (unsigned)esp_get_free_heap_size());
    ESP_LOGI(TAG, "PSRAM detected: %u bytes", (unsigned)esp_psram_get_size());

#if CONFIG_RAVA_ENABLE_CAMERA && CONFIG_RAVA_CAMERA_PINMAP_CONFIRMED
    esp_err_t cam_err = camera_service_init();
    if (cam_err == ESP_OK) {
        if (xTaskCreate(camera_monitor_task, "camera_monitor", 4096, NULL, 4, NULL) != pdPASS) {
            ESP_LOGE(TAG, "camera monitor task creation failed");
        }
    } else {
        ESP_LOGE(TAG, "camera service unavailable: %s", esp_err_to_name(cam_err));
    }
#elif CONFIG_RAVA_ENABLE_CAMERA
    ESP_LOGW(TAG, "camera requested but GPIO map confirmation gate is closed");
#else
    ESP_LOGI(TAG, "camera service disabled by default configuration");
#endif

#if CONFIG_RAVA_ENABLE_AUDIO && CONFIG_RAVA_AUDIO_WIRING_CONFIRMED && CONFIG_RAVA_MIC_FORMAT_CONFIRMED
    esp_err_t audio_err = audio_service_init();
    if (audio_err == ESP_OK) {
        if (xTaskCreate(audio_monitor_task, "audio_monitor", 4096, NULL, 5, NULL) != pdPASS) {
            ESP_LOGE(TAG, "audio monitor task creation failed");
        }
    } else {
        ESP_LOGE(TAG, "audio service unavailable: %s", esp_err_to_name(audio_err));
    }
#elif CONFIG_RAVA_ENABLE_AUDIO
    ESP_LOGW(TAG, "audio requested but wiring or microphone-format confirmation gate is closed");
#else
    ESP_LOGI(TAG, "audio service disabled by default configuration");
#endif

#if CONFIG_RAVA_ENABLE_CELLULAR && CONFIG_RAVA_MODEM_ELECTRICAL_CONFIRMED
    esp_err_t net_err = network_service_start();
    if (net_err != ESP_OK) {
        ESP_LOGE(TAG, "cellular network startup failed: %s", esp_err_to_name(net_err));
    }
#elif CONFIG_RAVA_ENABLE_CELLULAR
    ESP_LOGW(TAG, "cellular requested but carrier power/UART electrical gate is closed");
#else
    ESP_LOGI(TAG, "A7670E service disabled by default configuration");
#endif

    while (1) {
        ESP_LOGI(TAG, "heartbeat heap=%u psram_free=%u",
                 (unsigned)esp_get_free_heap_size(),
                 (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}
