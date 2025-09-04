#include "AudioCapture.h"
#include "driver/i2s.h"

AudioCapture::AudioCapture() {}

bool AudioCapture::init() {
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = 0,
        .dma_buf_count = 8,
        .dma_buf_len = 64,
        .use_apll = false,
        .tx_desc_auto_clear = false,
        .fixed_mclk = 0
    };
    i2s_pin_config_t pin_config = {
        .bck_io_num = I2S_BCLK_PIN,
        .ws_io_num = I2S_LRCL_PIN,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num = I2S_DOUT_PIN
    };
    esp_err_t err = i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
    if (err != ESP_OK) return false;
    err = i2s_set_pin(I2S_NUM_0, &pin_config);
    return err == ESP_OK;
}

bool AudioCapture::read(int16_t* buffer, size_t length) {
    size_t bytes_read;
    esp_err_t err = i2s_read(I2S_NUM_0, buffer, length * sizeof(int16_t), &bytes_read, portMAX_DELAY);
    return err == ESP_OK;
}