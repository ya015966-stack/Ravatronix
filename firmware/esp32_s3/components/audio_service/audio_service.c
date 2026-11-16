#include "sdkconfig.h"
#include "audio_service.h"
#include "board_config.h"
#include "driver/i2s_std.h"
#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include <string.h>

#define RAVA_AUDIO_RATE 16000
#define RAVA_AUDIO_DMA_FRAMES 256

static const char *TAG = "rava_audio";
static i2s_chan_handle_t s_mic_rx;
static i2s_chan_handle_t s_amp_tx;
static bool s_initialized;

esp_err_t audio_service_init(void)
{
#if !CONFIG_RAVA_ENABLE_AUDIO || !CONFIG_RAVA_AUDIO_WIRING_CONFIRMED || !CONFIG_RAVA_MIC_FORMAT_CONFIRMED
    ESP_LOGW(TAG, "audio init refused: feature, wiring, or microphone-format gate is disabled");
    return ESP_ERR_INVALID_STATE;
#else
    if (s_initialized) return ESP_OK;
    i2s_chan_config_t rx_chan = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    rx_chan.dma_desc_num = 6;
    rx_chan.dma_frame_num = RAVA_AUDIO_DMA_FRAMES;
    esp_err_t err = i2s_new_channel(&rx_chan, NULL, &s_mic_rx);
    if (err != ESP_OK) return err;

    i2s_std_config_t rx_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(RAVA_AUDIO_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED, .bclk = RAVA_MIC_BCLK, .ws = RAVA_MIC_WS,
            .dout = I2S_GPIO_UNUSED, .din = RAVA_MIC_DOUT,
            .invert_flags = { .mclk_inv = false, .bclk_inv = false, .ws_inv = false }
        }
    };
    rx_cfg.slot_cfg.slot_mask = I2S_STD_SLOT_LEFT;
    err = i2s_channel_init_std_mode(s_mic_rx, &rx_cfg);
    if (err != ESP_OK) return err;
    err = i2s_channel_enable(s_mic_rx);
    if (err != ESP_OK) return err;

    i2s_chan_config_t tx_chan = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_1, I2S_ROLE_MASTER);
    tx_chan.dma_desc_num = 6;
    tx_chan.dma_frame_num = RAVA_AUDIO_DMA_FRAMES;
    err = i2s_new_channel(&tx_chan, &s_amp_tx, NULL);
    if (err != ESP_OK) return err;

    i2s_std_config_t tx_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(RAVA_AUDIO_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED, .bclk = RAVA_AMP_BCLK, .ws = RAVA_AMP_WS,
            .dout = RAVA_AMP_DIN, .din = I2S_GPIO_UNUSED,
            .invert_flags = { .mclk_inv = false, .bclk_inv = false, .ws_inv = false }
        }
    };
    tx_cfg.slot_cfg.slot_mask = I2S_STD_SLOT_LEFT;
    err = i2s_channel_init_std_mode(s_amp_tx, &tx_cfg);
    if (err != ESP_OK) return err;
    err = i2s_channel_enable(s_amp_tx);
    if (err != ESP_OK) return err;

    s_initialized = true;
    ESP_LOGI(TAG, "I2S mic and speaker channels ready at %d Hz", RAVA_AUDIO_RATE);
    return ESP_OK;
#endif
}

esp_err_t audio_service_read_pcm(int32_t *dst, size_t capacity_samples,
                                 size_t *samples_read, uint32_t timeout_ms)
{
    if (!dst || !samples_read || capacity_samples == 0) return ESP_ERR_INVALID_ARG;
    *samples_read = 0;
    if (!s_initialized) return ESP_ERR_INVALID_STATE;
    size_t bytes = 0;
    esp_err_t err = i2s_channel_read(s_mic_rx, dst, capacity_samples * sizeof(int32_t),
                                     &bytes, pdMS_TO_TICKS(timeout_ms));
    *samples_read = bytes / sizeof(int32_t);
    return err;
}

esp_err_t audio_service_write_pcm(const int16_t *src, size_t samples,
                                  size_t *samples_written, uint32_t timeout_ms)
{
    if ((!src && samples) || !samples_written) return ESP_ERR_INVALID_ARG;
    *samples_written = 0;
    if (!s_initialized) return ESP_ERR_INVALID_STATE;
    size_t bytes = 0;
    esp_err_t err = i2s_channel_write(s_amp_tx, src, samples * sizeof(int16_t),
                                      &bytes, pdMS_TO_TICKS(timeout_ms));
    *samples_written = bytes / sizeof(int16_t);
    return err;
}
