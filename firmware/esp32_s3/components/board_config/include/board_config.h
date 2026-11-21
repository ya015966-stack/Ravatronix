#pragma once
#include "driver/gpio.h"
#include "driver/uart.h"

/* Ravatronix ESP32-S3-N16R8 board profile.
 * GPIO values below are provisional records, not electrically verified.
 * Runtime startup is gated in Kconfig until each hardware map is confirmed.
 */
#define RAVA_BOARD_NAME "ESP32-S3-N16R8"
#define RAVA_FLASH_SIZE_MB 16
#define RAVA_EXPECTED_PSRAM_BYTES (8U * 1024U * 1024U)

#define RAVA_CAM_PIN_XCLK GPIO_NUM_15
#define RAVA_CAM_PIN_SIOD GPIO_NUM_4
#define RAVA_CAM_PIN_SIOC GPIO_NUM_5
#define RAVA_CAM_PIN_D7 GPIO_NUM_16
#define RAVA_CAM_PIN_D6 GPIO_NUM_17
#define RAVA_CAM_PIN_D5 GPIO_NUM_18
#define RAVA_CAM_PIN_D4 GPIO_NUM_12
#define RAVA_CAM_PIN_D3 GPIO_NUM_10
#define RAVA_CAM_PIN_D2 GPIO_NUM_8
#define RAVA_CAM_PIN_D1 GPIO_NUM_9
#define RAVA_CAM_PIN_D0 GPIO_NUM_11
#define RAVA_CAM_PIN_VSYNC GPIO_NUM_6
#define RAVA_CAM_PIN_HREF GPIO_NUM_7
#define RAVA_CAM_PIN_PCLK GPIO_NUM_13
#define RAVA_CAM_PIN_PWDN GPIO_NUM_NC
#define RAVA_CAM_PIN_RESET GPIO_NUM_NC

#define RAVA_MIC_BCLK GPIO_NUM_38
#define RAVA_MIC_WS GPIO_NUM_39
#define RAVA_MIC_DOUT GPIO_NUM_40
#define RAVA_AMP_BCLK GPIO_NUM_1
#define RAVA_AMP_WS GPIO_NUM_2
#define RAVA_AMP_DIN GPIO_NUM_21

/* ESP32 TX -> modem RX; modem TX -> ESP32 RX. */
#define RAVA_MODEM_TX GPIO_NUM_48
#define RAVA_MODEM_RX GPIO_NUM_47
#define RAVA_MODEM_UART_NUM UART_NUM_1
#define RAVA_MODEM_BAUD 115200
