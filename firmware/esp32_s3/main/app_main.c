#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_psram.h"
#include "esp_heap_caps.h"
#include "board_config.h"

static const char *TAG = "ravatronix";

void app_main(void)
{
    ESP_LOGI(TAG, "Ravatronix integrated firmware boot");
    ESP_LOGI(TAG, "Board: %s", RAVA_BOARD_NAME);
    ESP_LOGI(TAG, "IDF target: ESP32-S3");
    ESP_LOGI(TAG, "Free heap: %u bytes", (unsigned)esp_get_free_heap_size());
    ESP_LOGI(TAG, "PSRAM: %u bytes", (unsigned)esp_psram_get_size());
    ESP_LOGI(TAG, "Firmware baseline alive; services are brought up in staged order.");
    while (1) {
        ESP_LOGI(TAG, "heartbeat heap=%u psram_free=%u",
                 (unsigned)esp_get_free_heap_size(),
                 (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}
