#include "sdkconfig.h"
#include "esp_err.h"
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
    camera_frame_t frame = {0};
    esp_err_t err = camera_service_capture_jpeg(&frame);
    if (err == ESP_OK) {
        ESP_LOGI("rava_camera", "initial JPEG snapshot %ux%u, %u bytes",
                 frame.width, frame.height, (unsigned)frame.length);
        camera_service_release(&frame);
    } else {
        ESP_LOGW("rava_camera", "initial capture: %s", esp_err_to_name(err));
    }
    vTaskDelete(NULL);
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
        ESP_LOGI(TAG, "I2S audio ready; microphone is reserved for explicit voice-turn requests");
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
