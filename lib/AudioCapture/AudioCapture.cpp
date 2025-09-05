#include "AudioCapture.h"
#include <driver/i2s.h>
#include <Arduino.h>
#include <algorithm>
#include <esp_task_wdt.h>
#include "env.h"

AudioCapture::AudioCapture() : is_initialized(false) {}

AudioCapture::~AudioCapture() {
    if (is_initialized) {
        i2s_driver_uninstall(I2S_NUM_0);
    }
}

bool AudioCapture::init() {
    if (is_initialized) {
        Serial.println("⚠️ AudioCapture already initialized");
        return true;
    }

    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 8,
        .dma_buf_len = 64,
        .use_apll = true,
        .tx_desc_auto_clear = false,
        .fixed_mclk = 0
    };

    i2s_pin_config_t pin_config = {
        .bck_io_num = I2S_BCLK_PIN,  // GPIO26 (SD)
        .ws_io_num = I2S_LRCL_PIN,   // GPIO25 (WS)
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num = I2S_DOUT_PIN  // GPIO22 (SCK)
    };

    esp_err_t err = i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
    if (err != ESP_OK) {
        Serial.printf("❌ I2S driver install failed: %s\n", esp_err_to_name(err));
        return false;
    }

    err = i2s_set_pin(I2S_NUM_0, &pin_config);
    if (err != ESP_OK) {
        Serial.printf("❌ I2S set pin failed: %s\n", esp_err_to_name(err));
        i2s_driver_uninstall(I2S_NUM_0);
        return false;
    }

    err = i2s_start(I2S_NUM_0);
    if (err != ESP_OK) {
        Serial.printf("❌ I2S start failed: %s\n", esp_err_to_name(err));
        i2s_driver_uninstall(I2S_NUM_0);
        return false;
    }

    Serial.println("I2S initialized successfully");
    Serial.println("I2S started");
    is_initialized = true;
    return true;
}

bool AudioCapture::read(int16_t* buffer, size_t samples) {
    if (!is_initialized) {
        Serial.println("❌ AudioCapture not initialized");
        return false;
    }

    size_t bytes_to_read = samples * sizeof(int16_t);
    size_t bytes_read = 0;

    esp_err_t err = i2s_read(I2S_NUM_0, buffer, bytes_to_read, &bytes_read, pdMS_TO_TICKS(100));
    if (err != ESP_OK) {
        Serial.printf("❌ I2S read failed: %s\n", esp_err_to_name(err));
        return false;
    }

    if (bytes_read != bytes_to_read) {
        Serial.printf("⚠️ Incomplete I2S read: %u/%u bytes\n", bytes_read, bytes_to_read);
        return false;
    }

    // Dynamic gain adjustment
    int max_amplitude = 0;
    for (size_t i = 0; i < samples; i++) {
        max_amplitude = max(max_amplitude, abs(buffer[i]));
    }
    float gain = (max_amplitude < 1000) ? 32.0f : (max_amplitude < 5000) ? 16.0f : 8.0f;
    for (size_t i = 0; i < samples; i++) {
        buffer[i] = (int16_t)(buffer[i] * gain);
        if (buffer[i] > 32767) buffer[i] = 32767;
        if (buffer[i] < -32768) buffer[i] = -32768;
    }
    Serial.printf("Applied gain: %.1f\n", gain);

    Serial.printf("I2S read %u bytes\n", bytes_read);
    #if DEBUG_LEVEL >= 3
    Serial.print("Raw I2S buffer (first 10 samples): ");
    for (int i = 0; i < min(10, (int)(bytes_read / sizeof(int16_t))); i++) {
        Serial.print(buffer[i]);
        Serial.print(" ");
    }
    Serial.println();
    #endif

    vTaskDelay(1 / portTICK_PERIOD_MS);
    esp_task_wdt_reset();
    return true;
}