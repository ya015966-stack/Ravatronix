#include "network_service.h"
#include "board_config.h"
#include "esp_modem_api.h"
#include "driver/uart.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "sdkconfig.h"

static const char *TAG = "rava_network";
static esp_modem_dce_t *s_dce;
static esp_netif_t *s_ppp_netif;
static EventGroupHandle_t s_events;
#define PPP_READY_BIT BIT0

static void on_ip_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg; (void)base; (void)data;
    if (id == IP_EVENT_PPP_GOT_IP) {
        ESP_LOGI(TAG, "cellular PPP link acquired an IP address");
        if (s_events) xEventGroupSetBits(s_events, PPP_READY_BIT);
    } else if (id == IP_EVENT_PPP_LOST_IP) {
        ESP_LOGW(TAG, "cellular PPP link lost its IP address");
        if (s_events) xEventGroupClearBits(s_events, PPP_READY_BIT);
    }
}

esp_err_t network_service_start(void)
{
    if (s_dce) return ESP_OK;
#if !defined(CONFIG_RAVA_MODEM_APN) || CONFIG_RAVA_MODEM_APN[0] == '\0'
    ESP_LOGE(TAG, "cellular APN is not configured");
    return ESP_ERR_INVALID_STATE;
#endif
    esp_err_t err = esp_netif_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) return err;
    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) return err;
    err = esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, on_ip_event, NULL);
    if (err != ESP_OK) return err;

    esp_netif_config_t netif_cfg = ESP_NETIF_DEFAULT_PPP();
    s_ppp_netif = esp_netif_new(&netif_cfg);
    if (!s_ppp_netif) return ESP_ERR_NO_MEM;
    s_events = xEventGroupCreate();
    if (!s_events) return ESP_ERR_NO_MEM;

    esp_modem_dte_config_t dte = ESP_MODEM_DTE_DEFAULT_CONFIG();
    dte.uart_config.tx_io_num = RAVA_MODEM_TX;
    dte.uart_config.rx_io_num = RAVA_MODEM_RX;
    dte.uart_config.rts_io_num = GPIO_NUM_NC;
    dte.uart_config.cts_io_num = GPIO_NUM_NC;
    dte.uart_config.baud_rate = RAVA_MODEM_BAUD;
    dte.uart_config.flow_control = ESP_MODEM_FLOW_CONTROL_NONE;
    esp_modem_dce_config_t dce_cfg = ESP_MODEM_DCE_DEFAULT_CONFIG(CONFIG_RAVA_MODEM_APN);
    s_dce = esp_modem_new(&dte, &dce_cfg, s_ppp_netif);
    if (!s_dce) return ESP_FAIL;

    err = esp_modem_set_mode(s_dce, ESP_MODEM_MODE_DATA);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "failed to start PPP data mode: %s", esp_err_to_name(err));
        return err;
    }
    ESP_LOGI(TAG, "A7670E PPP startup requested; waiting for IP event");
    return ESP_OK;
}

bool network_service_is_ready(void)
{
    return s_events &&
           (xEventGroupGetBits(s_events) & PPP_READY_BIT) != 0;
}
