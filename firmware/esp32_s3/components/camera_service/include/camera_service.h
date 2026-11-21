#pragma once
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"
#include "esp_camera.h"

typedef struct {
    const uint8_t *data;
    size_t length;
    uint16_t width;
    uint16_t height;
    pixformat_t format;
    camera_fb_t *driver_frame;
} camera_frame_t;

esp_err_t camera_service_init(void);
esp_err_t camera_service_capture_jpeg(camera_frame_t *out);
void camera_service_release(camera_frame_t *frame);
