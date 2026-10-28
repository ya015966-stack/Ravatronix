#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_timer.h"

void app_main(void)
{
    printf("\n================================\n");
    printf("RAVA ESP32-S3 IDF SERIAL TEST\n");
    printf("No Arduino, PSRAM, camera, modem, or Wi-Fi initialization\n");
    printf("================================\n");
    printf("Chip: %s\n", CONFIG_IDF_TARGET);

    while (1) {
        printf("RAVA_IDF_BOOT_OK uptime_ms=%lld free_heap=%lu\n",
               (long long)(esp_timer_get_time() / 1000),
               (unsigned long)esp_get_free_heap_size());
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
