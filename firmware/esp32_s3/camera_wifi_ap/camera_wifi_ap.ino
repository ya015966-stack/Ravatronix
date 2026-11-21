#include <Arduino.h>
#include "esp_camera.h"
#include <WiFi.h>
#include "esp_http_server.h"
#include "camera_secrets.h"

// Ravatronix ESP32-S3-N16R8 + OV3660
// Reported FFC pin map: matches Espressif CAMERA_MODEL_ESP32S3_EYE.
// Verify against the exact board schematic before treating the pin map as hardware-confirmed.

#ifndef RAVA_CAMERA_AP_PASSWORD
#error "Create camera_secrets.h with a unique RAVA_CAMERA_AP_PASSWORD before building."
#endif

static const char *AP_SSID = "Ravatronix-Camera";
static constexpr char AP_PASSWORD[] = RAVA_CAMERA_AP_PASSWORD;
static_assert(sizeof(AP_PASSWORD) - 1 >= 16,
              "RAVA_CAMERA_AP_PASSWORD must contain at least 16 characters.");
static_assert(sizeof(AP_PASSWORD) - 1 <= 63,
              "RAVA_CAMERA_AP_PASSWORD must contain no more than 63 characters.");

#define PWDN_GPIO_NUM  -1
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM  15
#define SIOD_GPIO_NUM  4
#define SIOC_GPIO_NUM  5
#define Y2_GPIO_NUM    11
#define Y3_GPIO_NUM    9
#define Y4_GPIO_NUM    8
#define Y5_GPIO_NUM    10
#define Y6_GPIO_NUM    12
#define Y7_GPIO_NUM    18
#define Y8_GPIO_NUM    17
#define Y9_GPIO_NUM    16
#define VSYNC_GPIO_NUM 6
#define HREF_GPIO_NUM  7
#define PCLK_GPIO_NUM  13

static httpd_handle_t camera_httpd = nullptr;
static const char *STREAM_BOUNDARY = "\r\n--ravaframe\r\n";

static esp_err_t index_handler(httpd_req_t *req) {
  static const char page[] =
    "<!doctype html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>Ravatronix Camera</title><style>body{font-family:Arial;background:#111;color:#eee;text-align:center}"
    "img{width:100%;max-width:900px;height:auto}a{color:#8cf}</style></head><body>"
    "<h2>Ravatronix Camera</h2><img src='/stream'><p><a href='/capture'>Capture JPEG</a></p></body></html>";
  httpd_resp_set_type(req, "text/html; charset=utf-8");
  return httpd_resp_send(req, page, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t capture_handler(httpd_req_t *req) {
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) return httpd_resp_send_500(req);
  httpd_resp_set_type(req, "image/jpeg");
  httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=rava.jpg");
  esp_err_t result = httpd_resp_send(req, (const char *)fb->buf, fb->len);
  esp_camera_fb_return(fb);
  return result;
}

static esp_err_t stream_handler(httpd_req_t *req) {
  esp_err_t result = httpd_resp_set_type(req, "multipart/x-mixed-replace;boundary=ravaframe");
  if (result != ESP_OK) return result;

  char header[80];
  while (true) {
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
      result = ESP_FAIL;
      break;
    }
    result = httpd_resp_send_chunk(req, STREAM_BOUNDARY, strlen(STREAM_BOUNDARY));
    if (result == ESP_OK) {
      int hlen = snprintf(header, sizeof(header),
                          "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n",
                          (unsigned)fb->len);
      result = httpd_resp_send_chunk(req, header, hlen);
    }
    if (result == ESP_OK) {
      result = httpd_resp_send_chunk(req, (const char *)fb->buf, fb->len);
    }
    esp_camera_fb_return(fb);
    if (result != ESP_OK) break;
    vTaskDelay(pdMS_TO_TICKS(10));
  }
  httpd_resp_send_chunk(req, nullptr, 0);
  return result;
}

static bool startCameraServer() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = 80;
  config.max_uri_handlers = 3;
  if (httpd_start(&camera_httpd, &config) != ESP_OK) return false;

  httpd_uri_t index_uri = { .uri = "/", .method = HTTP_GET, .handler = index_handler, .user_ctx = nullptr };
  httpd_uri_t stream_uri = { .uri = "/stream", .method = HTTP_GET, .handler = stream_handler, .user_ctx = nullptr };
  httpd_uri_t capture_uri = { .uri = "/capture", .method = HTTP_GET, .handler = capture_handler, .user_ctx = nullptr };

  httpd_register_uri_handler(camera_httpd, &index_uri);
  httpd_register_uri_handler(camera_httpd, &stream_uri);
  httpd_register_uri_handler(camera_httpd, &capture_uri);
  return true;
}

void setup() {
  Serial.begin(115200);
  delay(800);
  Serial.println("\n[RAVA] ESP32-S3 OV3660 camera + Wi-Fi AP");

  camera_config_t config = {};
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size = FRAMESIZE_VGA;
  config.jpeg_quality = 12;
  config.fb_count = 2;
  config.grab_mode = CAMERA_GRAB_LATEST;
  config.fb_location = CAMERA_FB_IN_PSRAM;

  if (!psramFound()) {
    Serial.println("[RAVA] WARNING: PSRAM not detected; using one DRAM frame buffer.");
    config.fb_location = CAMERA_FB_IN_DRAM;
    config.fb_count = 1;
    config.frame_size = FRAMESIZE_QVGA;
  } else {
    Serial.printf("[RAVA] PSRAM detected: %u bytes\n", ESP.getPsramSize());
  }

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("[RAVA] Camera init FAILED: 0x%x\n", err);
    return;
  }

  sensor_t *sensor = esp_camera_sensor_get();
  if (sensor && sensor->id.PID == OV3660_PID) {
    Serial.println("[RAVA] OV3660 detected.");
    sensor->set_vflip(sensor, 1);
    sensor->set_brightness(sensor, 1);
    sensor->set_saturation(sensor, -2);
  } else if (sensor) {
    Serial.printf("[RAVA] Camera initialized; sensor PID=0x%04x\n", sensor->id.PID);
  }

  WiFi.mode(WIFI_AP);
  if (!WiFi.softAP(AP_SSID, AP_PASSWORD)) {
    Serial.println("[RAVA] Wi-Fi AP start FAILED.");
    return;
  }

  Serial.printf("[RAVA] Wi-Fi: %s\n", AP_SSID);
  Serial.printf("[RAVA] Open http://%s in your phone browser.\n", WiFi.softAPIP());

  if (startCameraServer()) {
    Serial.println("[RAVA] HTTP camera server ready.");
  } else {
    Serial.println("[RAVA] HTTP camera server FAILED.");
  }
}

void loop() {
  delay(1000);
}
