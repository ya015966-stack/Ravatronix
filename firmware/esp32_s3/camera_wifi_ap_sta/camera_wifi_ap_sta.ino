#include <Arduino.h>
#include "esp_camera.h"
#include <WiFi.h>
#include "esp_http_server.h"
#include <Preferences.h>
#include <cstring>
#include <cstdio>

// Ravatronix ESP32-S3-N16R8 + OV3660
// Independent AP+STA build. The original AP-only firmware remains unchanged.
static const char *AP_SSID = "Ravatronix-Camera";
static const char *AP_PASSWORD = "RavaCam2026";

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

static bool formValue(const char *body, const char *key, char *out, size_t cap) {
  char needle[24];
  snprintf(needle, sizeof(needle), "%s=", key);
  const char *p = strstr(body, needle);
  if (!p || (p != body && *(p - 1) != '&')) return false;
  p += strlen(needle);
  size_t n = 0;
  while (*p && *p != '&') {
    char c = *p++;
    if (c == '+') c = ' ';
    else if (c == '%') {
      if (!p[0] || !p[1]) return false;
      auto hex = [](char h) -> int {
        if (h >= '0' && h <= '9') return h - '0';
        if (h >= 'a' && h <= 'f') return h - 'a' + 10;
        if (h >= 'A' && h <= 'F') return h - 'A' + 10;
        return -1;
      };
      int hi = hex(p[0]), lo = hex(p[1]);
      if (hi < 0 || lo < 0) return false;
      c = (char)((hi << 4) | lo);
      p += 2;
    }
    if (n + 1 >= cap || c == '\0') return false;
    out[n++] = c;
  }
  out[n] = '\0';
  return true;
}

static esp_err_t index_handler(httpd_req_t *req) {
  static const char page[] =
    "<!doctype html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>Ravatronix Camera</title><style>body{font-family:Arial;background:#111;color:#eee;text-align:center}"
    "img{width:100%;max-width:900px;height:auto}a{color:#8cf}</style></head><body>"
    "<h2>Ravatronix Camera</h2><img src='/stream'><p><a href='/capture'>Capture JPEG</a></p>"
    "<p><a href='/wifi'>تنظیم اتصال اینترنت / Wi-Fi setup</a></p><p><a href='/status'>وضعیت اتصال</a></p></body></html>";
  httpd_resp_set_type(req, "text/html; charset=utf-8");
  return httpd_resp_send(req, page, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t wifi_page_handler(httpd_req_t *req) {
  static const char page[] =
    "<!doctype html><html lang='fa'><meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<meta charset='utf-8'><title>Ravatronix Wi-Fi</title>"
    "<body style='font-family:sans-serif;background:#111;color:#eee;max-width:520px;margin:30px auto;padding:16px'>"
    "<h2>اتصال راواترونیکس به اینترنت</h2><p>نام و رمز هات‌اسپات گوشی را وارد کنید.</p>"
    "<form method='POST' action='/wifi-save'>"
    "<label>نام هات‌اسپات (SSID)</label><br><input name='ssid' maxlength='32' required style='width:100%;padding:12px;margin:8px 0'>"
    "<label>رمز هات‌اسپات</label><br><input name='password' type='password' maxlength='63' style='width:100%;padding:12px;margin:8px 0'>"
    "<button style='padding:12px 22px;margin-top:12px'>ذخیره و اتصال</button></form>"
    "<p><a style='color:#8cf' href='/status'>مشاهده وضعیت اتصال</a></p>"
    "<p><a style='color:#8cf' href='/'>بازگشت به دوربین</a></p></body></html>";
  httpd_resp_set_type(req, "text/html; charset=utf-8");
  return httpd_resp_send(req, page, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t wifi_save_handler(httpd_req_t *req) {
  if (req->content_len <= 0 || req->content_len > 255) {
    httpd_resp_set_status(req, "400 Bad Request");
    return httpd_resp_sendstr(req, "Invalid form size");
  }
  char body[256] = {};
  size_t received = 0;
  while (received < (size_t)req->content_len) {
    int n = httpd_req_recv(req, body + received, req->content_len - received);
    if (n <= 0) {
      httpd_resp_set_status(req, "408 Request Timeout");
      return httpd_resp_sendstr(req, "Could not read form");
    }
    received += (size_t)n;
  }
  body[received] = '\0';

  char ssid[33] = {};
  char password[64] = {};
  if (!formValue(body, "ssid", ssid, sizeof(ssid)) ||
      !formValue(body, "password", password, sizeof(password)) ||
      strlen(ssid) == 0 || (strlen(password) > 0 && strlen(password) < 8)) {
    httpd_resp_set_status(req, "400 Bad Request");
    return httpd_resp_sendstr(req, "SSID or password is invalid. Wi-Fi password must be empty or at least 8 characters.");
  }

  Preferences prefs;
  if (!prefs.begin("rava-wifi", false)) {
    httpd_resp_set_status(req, "500 Internal Server Error");
    return httpd_resp_sendstr(req, "Could not save Wi-Fi settings");
  }
  prefs.putString("ssid", ssid);
  prefs.putString("pass", password);
  prefs.end();

  WiFi.disconnect(false, false);
  WiFi.begin(ssid, password);
  httpd_resp_set_type(req, "text/html; charset=utf-8");
  return httpd_resp_sendstr(req,
    "<meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<body style='font-family:sans-serif;background:#111;color:#eee;padding:24px'>"
    "<h3>تنظیمات ذخیره شد؛ برد در حال اتصال است.</h3>"
    "<p>صفحه وضعیت را چند لحظه دیگر تازه‌سازی کنید.</p>"
    "<p><a style='color:#8cf' href='/status'>وضعیت اتصال</a> | <a style='color:#8cf' href='/'>دوربین</a></p></body>");
}

static esp_err_t status_handler(httpd_req_t *req) {
  char page[512];
  if (WiFi.status() == WL_CONNECTED) {
    snprintf(page, sizeof(page),
      "<!doctype html><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>"
      "<body style='font-family:sans-serif;background:#111;color:#eee;padding:24px'>"
      "<h2>اتصال اینترنت برقرار است</h2><p>Wi-Fi: متصل</p><p>IP: %s</p>"
      "<p>آدرس دسترسی دوربین از طریق هات‌اسپات: همین IP</p><p><a style='color:#8cf' href='/'>دوربین</a></p></body>",
      WiFi.localIP().toString().c_str());
  } else {
    snprintf(page, sizeof(page),
      "<!doctype html><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>"
      "<body style='font-family:sans-serif;background:#111;color:#eee;padding:24px'>"
      "<h2>هنوز به هات‌اسپات وصل نشده</h2><p>وضعیت: در حال اتصال یا ناموفق</p>"
      "<p><a style='color:#8cf' href='/wifi'>تنظیم دوباره Wi-Fi</a></p><p><a style='color:#8cf' href='/'>دوربین</a></p></body>");
  }
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
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  char header[80];
  while (true) {
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) { result = ESP_FAIL; break; }
    result = httpd_resp_send_chunk(req, STREAM_BOUNDARY, strlen(STREAM_BOUNDARY));
    if (result == ESP_OK) {
      int hlen = snprintf(header, sizeof(header),
        "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n", (unsigned)fb->len);
      result = httpd_resp_send_chunk(req, header, hlen);
    }
    if (result == ESP_OK) result = httpd_resp_send_chunk(req, (const char *)fb->buf, fb->len);
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
  config.max_uri_handlers = 6;
  if (httpd_start(&camera_httpd, &config) != ESP_OK) return false;

  httpd_uri_t routes[] = {
    { .uri = "/", .method = HTTP_GET, .handler = index_handler, .user_ctx = nullptr },
    { .uri = "/stream", .method = HTTP_GET, .handler = stream_handler, .user_ctx = nullptr },
    { .uri = "/capture", .method = HTTP_GET, .handler = capture_handler, .user_ctx = nullptr },
    { .uri = "/wifi", .method = HTTP_GET, .handler = wifi_page_handler, .user_ctx = nullptr },
    { .uri = "/wifi-save", .method = HTTP_POST, .handler = wifi_save_handler, .user_ctx = nullptr },
    { .uri = "/status", .method = HTTP_GET, .handler = status_handler, .user_ctx = nullptr }
  };
  for (auto &route : routes) {
    if (httpd_register_uri_handler(camera_httpd, &route) != ESP_OK) return false;
  }
  return true;
}

void setup() {
  Serial.begin(115200);
  delay(800);
  Serial.println("\n[RAVA] ESP32-S3 OV3660 AP+STA Wi-Fi");

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
    Serial.println("[RAVA] WARNING: PSRAM not detected; using DRAM QVGA.");
    config.fb_location = CAMERA_FB_IN_DRAM;
    config.fb_count = 1;
    config.frame_size = FRAMESIZE_QVGA;
  }

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("[RAVA] Camera init FAILED: 0x%x\n", err);
    return;
  }
  sensor_t *sensor = esp_camera_sensor_get();
  if (sensor && sensor->id.PID == OV3660_PID) {
    sensor->set_vflip(sensor, 1);
    sensor->set_brightness(sensor, 1);
    sensor->set_saturation(sensor, -2);
    Serial.println("[RAVA] OV3660 detected.");
  }

  WiFi.mode(WIFI_AP_STA);
  WiFi.setAutoReconnect(true);
  WiFi.setSleep(false);
  if (!WiFi.softAP(AP_SSID, AP_PASSWORD)) {
    Serial.println("[RAVA] Camera AP start FAILED.");
    return;
  }
  Serial.printf("[RAVA] Camera AP: http://%s\n", WiFi.softAPIP());

  Preferences prefs;
  if (prefs.begin("rava-wifi", true)) {
    String savedSsid = prefs.getString("ssid", "");
    String savedPass = prefs.getString("pass", "");
    prefs.end();
    if (savedSsid.length() > 0) {
      Serial.println("[RAVA] Saved hotspot found; connecting.");
      WiFi.begin(savedSsid.c_str(), savedPass.c_str());
    } else {
      Serial.println("[RAVA] Open /wifi to configure phone hotspot.");
    }
  }

  if (startCameraServer()) Serial.println("[RAVA] Camera and Wi-Fi setup server ready.");
  else Serial.println("[RAVA] HTTP server start FAILED.");
}

void loop() {
  static unsigned long lastStatus = 0;
  if (millis() - lastStatus > 10000) {
    lastStatus = millis();
    if (WiFi.status() == WL_CONNECTED) {
      Serial.printf("[RAVA] Hotspot connected; STA IP: %s\n", WiFi.localIP().toString().c_str());
    }
  }
  delay(100);
}
