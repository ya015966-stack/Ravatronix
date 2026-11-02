#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_chip_info.h"
#include "esp_flash.h"

void app_main(void)
{
    esp_chip_info_t chip;
    uint32_t flash_size = 0;
    esp_chip_info(&chip);
    esp_flash_get_size(NULL, &flash_size);

    printf("\n================================\n");
    printf("RAVATRONIX ESP32-S3 BOOT ISOLATION TEST\n");
    printf("No Arduino, PSRAM, camera, modem, or Wi-Fi initialization\n");
    printf("================================\n");
    printf("Target: %s\n", CONFIG_IDF_TARGET);
    printf("CPU cores: %d | Revision: %d\n", chip.cores, chip.revision);
    printf("Flash size: %lu bytes\n", (unsigned long)flash_size);
    printf("Reset reason: %d\n", (int)esp_reset_reason());

    while (1) {
        printf("RAVATRONIX_BOOT_OK uptime_ms=%lld free_heap=%lu\n",
               (long long)(esp_timer_get_time() / 1000),
               (unsigned long)esp_get_free_heap_size());
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
