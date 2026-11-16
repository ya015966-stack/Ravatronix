#include "sdkconfig.h"
#include "camera_service.h"
#include "board_config.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "rava_camera";
static bool s_initialized;

esp_err_t camera_service_init(void)
{
#if !CONFIG_RAVA_ENABLE_CAMERA || !CONFIG_RAVA_CAMERA_PINMAP_CONFIRMED
    ESP_LOGW(TAG, "camera init refused: feature or GPIO-map confirmation gate is disabled");
    return ESP_ERR_INVALID_STATE;
#else
    if (s_initialized) return ESP_OK;
    camera_config_t config = {
        .pin_pwdn = RAVA_CAM_PIN_PWDN,
        .pin_reset = RAVA_CAM_PIN_RESET,
        .pin_xclk = RAVA_CAM_PIN_XCLK,
        .pin_sccb_sda = RAVA_CAM_PIN_SIOD,
        .pin_sccb_scl = RAVA_CAM_PIN_SIOC,
        .pin_d7 = RAVA_CAM_PIN_D7,
        .pin_d6 = RAVA_CAM_PIN_D6,
        .pin_d5 = RAVA_CAM_PIN_D5,
        .pin_d4 = RAVA_CAM_PIN_D4,
        .pin_d3 = RAVA_CAM_PIN_D3,
        .pin_d2 = RAVA_CAM_PIN_D2,
        .pin_d1 = RAVA_CAM_PIN_D1,
        .pin_d0 = RAVA_CAM_PIN_D0,
        .pin_vsync = RAVA_CAM_PIN_VSYNC,
        .pin_href = RAVA_CAM_PIN_HREF,
        .pin_pclk = RAVA_CAM_PIN_PCLK,
        .xclk_freq_hz = 20000000,
        .ledc_timer = LEDC_TIMER_0,
        .ledc_channel = LEDC_CHANNEL_0,
        .pixel_format = PIXFORMAT_JPEG,
        .frame_size = FRAMESIZE_VGA,
        .jpeg_quality = 12,
        .fb_count = 1,
        .fb_location = CAMERA_FB_IN_PSRAM,
        .grab_mode = CAMERA_GRAB_WHEN_EMPTY
    };
    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "camera init failed: %s", esp_err_to_name(err));
        return err;
    }
    sensor_t *sensor = esp_camera_sensor_get();
    if (sensor && sensor->id.PID != OV3660_PID) {
        ESP_LOGW(TAG, "camera PID differs from expected OV3660: 0x%04x", sensor->id.PID);
    }
    s_initialized = true;
    ESP_LOGI(TAG, "camera initialized");
    return ESP_OK;
#endif
}

esp_err_t camera_service_capture_jpeg(camera_frame_t *out)
{
    if (!out) return ESP_ERR_INVALID_ARG;
    memset(out, 0, sizeof(*out));
    if (!s_initialized) return ESP_ERR_INVALID_STATE;
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) return ESP_ERR_TIMEOUT;
    if (!fb->buf || fb->len == 0 || fb->format != PIXFORMAT_JPEG) {
        esp_camera_fb_return(fb);
        return ESP_ERR_INVALID_RESPONSE;
    }
    out->data = fb->buf;
    out->length = fb->len;
    out->width = fb->width;
    out->height = fb->height;
    out->format = fb->format;
    out->driver_frame = fb;
    return ESP_OK;
}

void camera_service_release(camera_frame_t *frame)
{
    if (!frame) return;
    if (frame->driver_frame) esp_camera_fb_return(frame->driver_frame);
    memset(frame, 0, sizeof(*frame));
}
