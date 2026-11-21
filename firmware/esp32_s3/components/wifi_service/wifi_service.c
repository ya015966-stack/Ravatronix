#include "wifi_service.h"

#include <stdio.h>
#include <string.h>

#include "cJSON.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_random.h"
#include "esp_wifi.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define TAG "rava_wifi"
#define WIFI_NVS_NAMESPACE "rava_wifi"
#define WIFI_NVS_SSID_KEY "ssid"
#define WIFI_NVS_PASSWORD_KEY "pass"
#define WIFI_SSID_MAX 32U
#define WIFI_PASSWORD_MAX 64U
#define WIFI_SETUP_BODY_MAX 256U
#define WIFI_PROVISIONING_TIMEOUT_MS (15U * 60U * 1000U)

static esp_netif_t *s_sta_netif;
static esp_netif_t *s_ap_netif;
static httpd_handle_t s_httpd;
static volatile wifi_service_state_t s_state = WIFI_SERVICE_STATE_STOPPED;
static volatile bool s_ready;
static bool s_started;
static bool s_have_credentials;
static volatile bool s_provisioning;

static esp_err_t start_setup_server(void);

static esp_err_t save_credentials(const char *ssid, const char *password)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(WIFI_NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) return err;

    err = nvs_set_str(handle, WIFI_NVS_SSID_KEY, ssid);
    if (err == ESP_OK) err = nvs_set_str(handle, WIFI_NVS_PASSWORD_KEY, password);
    if (err == ESP_OK) err = nvs_commit(handle);
    nvs_close(handle);
    return err;
}

static bool load_credentials(char *ssid, size_t ssid_capacity,
                             char *password, size_t password_capacity)
{
    nvs_handle_t handle;
    if (nvs_open(WIFI_NVS_NAMESPACE, NVS_READONLY, &handle) != ESP_OK) {
        return false;
    }

    size_t ssid_length = ssid_capacity;
    size_t password_length = password_capacity;
    esp_err_t ssid_err = nvs_get_str(handle, WIFI_NVS_SSID_KEY, ssid, &ssid_length);
    esp_err_t password_err = nvs_get_str(handle, WIFI_NVS_PASSWORD_KEY,
                                         password, &password_length);
    nvs_close(handle);

    return ssid_err == ESP_OK && password_err == ESP_OK &&
           ssid[0] != '\0' && ssid_length <= ssid_capacity &&
           password_length <= password_capacity;
}

static esp_err_t configure_station(const char *ssid, const char *password)
{
    wifi_config_t config = {0};
    size_t ssid_length = strlen(ssid);
    size_t password_length = strlen(password);
    if (ssid_length == 0U || ssid_length > sizeof(config.sta.ssid) ||
        password_length > sizeof(config.sta.password)) {
        return ESP_ERR_INVALID_SIZE;
    }
    memcpy(config.sta.ssid, ssid, ssid_length);
    memcpy(config.sta.password, password, password_length);
    config.sta.threshold.authmode = WIFI_AUTH_OPEN;
    config.sta.pmf_cfg.capable = true;
    config.sta.pmf_cfg.required = false;
    return esp_wifi_set_config(WIFI_IF_STA, &config);
}

static esp_err_t setup_root_handler(httpd_req_t *request)
{
    static const char page[] =
        "<!doctype html><html><meta name='viewport' content='width=device-width'>"
        "<title>RAVA Wi-Fi setup</title><body><h2>Ravatronix Wi-Fi setup</h2>"
        "<input id='s' placeholder='Wi-Fi name'><br>"
        "<input id='p' type='password' placeholder='Wi-Fi password'><br>"
        "<button onclick='save()'>Connect</button><p id='m'></p>"
        "<script>async function save(){let r=await fetch('/config',{method:'POST',"
        "headers:{'Content-Type':'application/json'},body:JSON.stringify({ssid:"
        "document.getElementById('s').value,password:document.getElementById('p').value})});"
        "document.getElementById('m').textContent=r.ok?'Saved. RAVA is connecting.':"
        "'Could not save settings.';}</script></body></html>";
    httpd_resp_set_type(request, "text/html; charset=utf-8");
    return httpd_resp_send(request, page, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t setup_config_handler(httpd_req_t *request)
{
    if (request->content_len == 0U || request->content_len > WIFI_SETUP_BODY_MAX) {
        httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "invalid body size");
        return ESP_FAIL;
    }

    char body[WIFI_SETUP_BODY_MAX + 1U] = {0};
    size_t received_total = 0U;
    while (received_total < request->content_len) {
        int received = httpd_req_recv(request, body + received_total,
                                      request->content_len - received_total);
        if (received <= 0) {
            httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "incomplete body");
            return ESP_FAIL;
        }
        received_total += (size_t)received;
    }

    cJSON *json = cJSON_Parse(body);
    if (json == NULL) {
        httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "invalid JSON");
        return ESP_FAIL;
    }

    const cJSON *ssid_item = cJSON_GetObjectItemCaseSensitive(json, "ssid");
    const cJSON *password_item = cJSON_GetObjectItemCaseSensitive(json, "password");
    esp_err_t err = ESP_ERR_INVALID_ARG;
    if (cJSON_IsString(ssid_item) && ssid_item->valuestring != NULL &&
        cJSON_IsString(password_item) && password_item->valuestring != NULL) {
        err = wifi_service_set_credentials(ssid_item->valuestring,
                                           password_item->valuestring);
    }
    cJSON_Delete(json);

    if (err != ESP_OK) {
        httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "settings rejected");
        return err;
    }

    httpd_resp_set_status(request, "202 Accepted");
    return httpd_resp_sendstr(request, "Wi-Fi settings saved. RAVA is connecting.");
}

static esp_err_t start_setup_server(void)
{
    if (s_httpd != NULL) return ESP_OK;

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.max_uri_handlers = 4;
    esp_err_t err = httpd_start(&s_httpd, &config);
    if (err != ESP_OK) return err;

    httpd_uri_t root = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = setup_root_handler,
        .user_ctx = NULL
    };
    httpd_uri_t configure = {
        .uri = "/config",
        .method = HTTP_POST,
        .handler = setup_config_handler,
        .user_ctx = NULL
    };
    err = httpd_register_uri_handler(s_httpd, &root);
    if (err == ESP_OK) err = httpd_register_uri_handler(s_httpd, &configure);
    if (err != ESP_OK) {
        (void)httpd_stop(s_httpd);
        s_httpd = NULL;
    }
    return err;
}

static void provisioning_timeout_task(void *argument)
{
    (void)argument;
    vTaskDelay(pdMS_TO_TICKS(WIFI_PROVISIONING_TIMEOUT_MS));
    if (s_provisioning) {
        s_provisioning = false;
        if (s_httpd != NULL) {
            (void)httpd_stop(s_httpd);
            s_httpd = NULL;
        }
        (void)esp_wifi_stop();
        s_state = WIFI_SERVICE_STATE_FAILED;
        ESP_LOGW(TAG, "Wi-Fi provisioning window expired; restart to reopen setup");
    }
    vTaskDelete(NULL);
}

static esp_err_t start_provisioning(void)
{
    wifi_config_t ap = {0};
    uint8_t random_bytes[6];
    char setup_password[17] = {0};
    static const char hex[] = "0123456789ABCDEF";

    esp_fill_random(random_bytes, sizeof(random_bytes));
    memcpy(ap.ap.ssid, "RAVA-SETUP", 10U);
    ap.ap.ssid_len = 10U;
    for (size_t i = 0; i < sizeof(random_bytes); ++i) {
        setup_password[i * 2U] = hex[(random_bytes[i] >> 4U) & 0x0FU];
        setup_password[i * 2U + 1U] = hex[random_bytes[i] & 0x0FU];
    }
    memcpy(ap.ap.password, setup_password, 16U);
    ap.ap.channel = 1;
    ap.ap.max_connection = 1;
    ap.ap.authmode = WIFI_AUTH_WPA2_PSK;

    if (s_ap_netif == NULL) {
        s_ap_netif = esp_netif_create_default_wifi_ap();
        if (s_ap_netif == NULL) return ESP_ERR_NO_MEM;
    }

    esp_err_t err = esp_wifi_set_mode(WIFI_MODE_APSTA);
    if (err == ESP_OK) err = esp_wifi_set_config(WIFI_IF_AP, &ap);
    if (err == ESP_OK) err = esp_wifi_start();
    if (err == ESP_OK) err = start_setup_server();
    if (err == ESP_OK) {
        s_provisioning = true;
        s_state = WIFI_SERVICE_STATE_PROVISIONING;
        if (xTaskCreate(provisioning_timeout_task, "wifi_setup_timeout",
                        3072, NULL, 2, NULL) != pdPASS) {
            s_provisioning = false;
            (void)httpd_stop(s_httpd);
            s_httpd = NULL;
            (void)esp_wifi_stop();
            return ESP_ERR_NO_MEM;
        }
#if CONFIG_RAVA_WIFI_LOG_SETUP_SECRET
        ESP_LOGW(TAG, "temporary setup AP: RAVA-SETUP; passphrase: %s; open http://192.168.4.1",
                 setup_password);
#else
        ESP_LOGW(TAG, "temporary setup AP active; passphrase logging is disabled; open http://192.168.4.1");
#endif
    }
    return err;
}

static void wifi_event_handler(void *argument, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    (void)argument;
    (void)event_data;

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        if (s_have_credentials) {
            s_state = WIFI_SERVICE_STATE_CONNECTING;
            (void)esp_wifi_connect();
        }
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        s_ready = false;
        if (s_have_credentials) {
            s_state = WIFI_SERVICE_STATE_CONNECTING;
            (void)esp_wifi_connect();
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        s_ready = true;
        s_state = WIFI_SERVICE_STATE_CONNECTED;
        ESP_LOGI(TAG, "Wi-Fi station acquired an IP address");
        if (s_provisioning) {
            s_provisioning = false;
            if (s_httpd != NULL) {
                (void)httpd_stop(s_httpd);
                s_httpd = NULL;
            }
            (void)esp_wifi_set_mode(WIFI_MODE_STA);
        }
    }
}

esp_err_t wifi_service_set_credentials(const char *ssid, const char *password)
{
    if (ssid == NULL || password == NULL) return ESP_ERR_INVALID_ARG;
    size_t ssid_length = strlen(ssid);
    size_t password_length = strlen(password);
    if (ssid_length == 0U || ssid_length > WIFI_SSID_MAX ||
        password_length > WIFI_PASSWORD_MAX) return ESP_ERR_INVALID_SIZE;

    esp_err_t err = save_credentials(ssid, password);
    if (err != ESP_OK) return err;

    s_have_credentials = true;
    s_ready = false;
    s_state = WIFI_SERVICE_STATE_CONNECTING;
    err = configure_station(ssid, password);
    if (err != ESP_OK) return err;
    err = esp_wifi_disconnect();
    if (err != ESP_OK && err != ESP_ERR_WIFI_NOT_CONNECT) return err;
    return esp_wifi_connect();
}

esp_err_t wifi_service_start(void)
{
    if (s_started) return ESP_OK;

    esp_err_t err = nvs_flash_init();
    if (err != ESP_OK) return err;

    err = esp_netif_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) return err;
    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) return err;

    s_sta_netif = esp_netif_create_default_wifi_sta();
    if (s_sta_netif == NULL) return ESP_ERR_NO_MEM;

    wifi_init_config_t wifi_config = WIFI_INIT_CONFIG_DEFAULT();
    err = esp_wifi_init(&wifi_config);
    if (err != ESP_OK) return err;

    err = esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                     wifi_event_handler, NULL);
    if (err == ESP_OK) {
        err = esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                         wifi_event_handler, NULL);
    }
    if (err != ESP_OK) return err;

    char ssid[WIFI_SSID_MAX + 1U] = {0};
    char password[WIFI_PASSWORD_MAX + 1U] = {0};
    s_have_credentials = load_credentials(ssid, sizeof(ssid),
                                          password, sizeof(password));

    if (s_have_credentials) {
        err = configure_station(ssid, password);
        if (err == ESP_OK) err = esp_wifi_set_mode(WIFI_MODE_STA);
        if (err == ESP_OK) err = esp_wifi_start();
        if (err == ESP_OK) {
            s_state = WIFI_SERVICE_STATE_CONNECTING;
            err = esp_wifi_connect();
        }
    } else {
        err = start_provisioning();
    }

    if (err == ESP_OK) s_started = true;
    else s_state = WIFI_SERVICE_STATE_FAILED;
    return err;
}

wifi_service_state_t wifi_service_get_state(void)
{
    return s_state;
}

bool wifi_service_is_ready(void)
{
    return s_ready;
}
