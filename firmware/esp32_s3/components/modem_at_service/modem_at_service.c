#include "sdkconfig.h"
#include "modem_at_service.h"
#include "modem_at_parser.h"
#include "board_config.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "driver/uart.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define TAG "rava_modem_at"
#define MODEM_COMMAND_TIMEOUT_MS 2500U
#define MODEM_FIRMWARE_INFO_CAPACITY 96U

static bool s_uart_installed;
static bool s_ready;
static char s_firmware_info[MODEM_FIRMWARE_INFO_CAPACITY];

static esp_err_t send_command(const char *command, uint32_t timeout_ms,
                              char *info_out, size_t info_capacity)
{
    if (command == NULL || !s_uart_installed) return ESP_ERR_INVALID_STATE;
    if (info_out != NULL && info_capacity == 0U) return ESP_ERR_INVALID_ARG;
    if (info_out != NULL) info_out[0] = '\0';

    (void)uart_flush_input(RAVA_MODEM_UART_NUM);
    int written = uart_write_bytes(RAVA_MODEM_UART_NUM, command, strlen(command));
    if (written < 0 || (size_t)written != strlen(command)) return ESP_FAIL;
    esp_err_t err = uart_wait_tx_done(RAVA_MODEM_UART_NUM, pdMS_TO_TICKS(500U));
    if (err != ESP_OK) return err;

    modem_at_parser_t parser;
    modem_at_parser_init(&parser);
    char line[MODEM_AT_LINE_CAPACITY];
    uint8_t chunk[32];
    const TickType_t started = xTaskGetTickCount();
    const TickType_t timeout_ticks = pdMS_TO_TICKS(timeout_ms);

    while ((xTaskGetTickCount() - started) < timeout_ticks) {
        int count = uart_read_bytes(RAVA_MODEM_UART_NUM, chunk, sizeof(chunk),
                                    pdMS_TO_TICKS(100U));
        if (count <= 0) continue;
        for (int i = 0; i < count; ++i) {
            modem_at_event_t event = modem_at_parser_feed(
                &parser, chunk[i], line, sizeof(line));
            if (event == MODEM_AT_EVENT_OVERFLOW) return ESP_ERR_INVALID_SIZE;
            if (event != MODEM_AT_EVENT_LINE) continue;
            if (strcmp(line, "OK") == 0) return ESP_OK;
            if (strcmp(line, "ERROR") == 0 || strncmp(line, "+CME ERROR:", 11U) == 0 ||
                strncmp(line, "+CMS ERROR:", 11U) == 0) return ESP_FAIL;
            if (info_out != NULL && line[0] != '\0' && line[0] != '+') {
                size_t used = strlen(info_out);
                int added = snprintf(info_out + used, info_capacity - used,
                                     "%s%s", used == 0U ? "" : " ", line);
                if (added < 0 || (size_t)added >= info_capacity - used) {
                    return ESP_ERR_INVALID_SIZE;
                }
            }
        }
    }
    return ESP_ERR_TIMEOUT;
}

esp_err_t modem_at_service_start(void)
{
#if !CONFIG_RAVA_ENABLE_CELLULAR || !CONFIG_RAVA_MODEM_ELECTRICAL_CONFIRMED || !CONFIG_RAVA_MODEM_AT_UART_CONFIRMED
    ESP_LOGW(TAG, "AT diagnostic blocked by Kconfig safety gates");
    return ESP_ERR_INVALID_STATE;
#else
    if (s_ready) return ESP_OK;

    const uart_config_t config = {
        .baud_rate = RAVA_MODEM_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT
    };

    esp_err_t err = uart_driver_install(RAVA_MODEM_UART_NUM, 1024U, 0U, 0, NULL, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "UART1 is unavailable; refusing to share modem UART");
        return err;
    }
    s_uart_installed = true;

    err = uart_param_config(RAVA_MODEM_UART_NUM, &config);
    if (err == ESP_OK) {
        err = uart_set_pin(RAVA_MODEM_UART_NUM, RAVA_MODEM_TX, RAVA_MODEM_RX,
                           UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    }
    if (err == ESP_OK) err = send_command("AT\r\n", MODEM_COMMAND_TIMEOUT_MS, NULL, 0U);
    if (err == ESP_OK) err = send_command("ATE0\r\n", MODEM_COMMAND_TIMEOUT_MS, NULL, 0U);
    if (err == ESP_OK) {
        err = send_command("AT+GMR\r\n", MODEM_COMMAND_TIMEOUT_MS,
                           s_firmware_info, sizeof(s_firmware_info));
    }

    if (err != ESP_OK) {
        ESP_LOGW(TAG, "A7670E did not complete bounded AT diagnostics: %s", esp_err_to_name(err));
        (void)uart_driver_delete(RAVA_MODEM_UART_NUM);
        s_uart_installed = false;
        s_firmware_info[0] = '\0';
        return err;
    }

    s_ready = true;
    ESP_LOGI(TAG, "A7670E AT interface responsive; firmware identity query completed");
    return ESP_OK;
#endif
}

bool modem_at_service_is_ready(void)
{
    return s_ready;
}

const char *modem_at_service_get_firmware_info(void)
{
    return s_firmware_info;
}
