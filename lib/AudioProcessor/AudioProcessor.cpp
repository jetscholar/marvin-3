#include "AudioProcessor.h"
#include <cstdint>
#include "esp_task_wdt.h"
#include <Arduino.h>
#include <ArduinoFFT.h>

void AudioProcessor::applyWindow(float* samples, int length) {
    #if DEBUG_LEVEL >= 2
    Serial.printf("Applying window to %d samples\n", length);
    #endif
    for (int i = 0; i < length; i++) {
        float window = 0.54f - 0.46f * cos(2.0f * PI * i / (length - 1));
        samples[i] *= window;
        if (i % 128 == 0) {
            vTaskDelay(1 / portTICK_PERIOD_MS);
            esp_task_wdt_reset();
        }
    }
    #if DEBUG_LEVEL >= 3
    Serial.println("Window application completed");
    #endif
}

void AudioProcessor::computeFFT(float* samples, float* output, int length) {
    #if DEBUG_LEVEL >= 2
    Serial.printf("Computing FFT for %d samples\n", length);
    #endif
    float* imag = (float*)heap_caps_malloc(WINDOW_SIZE * sizeof(float), MALLOC_CAP_8BIT);
    if (!imag) {
        Serial.println("❌ Failed to allocate imag buffer for FFT");
        return;
    }
    memset(imag, 0, WINDOW_SIZE * sizeof(float));
    ArduinoFFT<float> FFT = ArduinoFFT<float>(samples, imag, length, SAMPLE_RATE);
    FFT.windowing(FFT_WIN_TYP_HAMMING, FFT_FORWARD);
    FFT.compute(FFT_FORWARD);
    FFT.complexToMagnitude();
    for (int i = 0; i < length / 2; i++) {
        output[i] = samples[i] * 100.0f; // Scale magnitude
        if (i % 128 == 0) {
            vTaskDelay(1 / portTICK_PERIOD_MS);
            esp_task_wdt_reset();
        }
    }
    #if DEBUG_LEVEL >= 3
    Serial.println("FFT computation completed");
    Serial.print("First 10 FFT values: ");
    for (int i = 0; i < min(10, length / 2); i++) {
        Serial.print(output[i]);
        Serial.print(" ");
    }
    Serial.println();
    #endif
    heap_caps_free(imag);
}

void AudioProcessor::computeMFCC(const int16_t* audio_samples, int8_t* mfcc_output) {
    #if DEBUG_LEVEL >= 2
    Serial.printf("Starting MFCC computation for %d samples\n", WINDOW_SIZE);
    #endif
    float* samples = (float*)heap_caps_malloc(WINDOW_SIZE * sizeof(float), MALLOC_CAP_8BIT);
    if (!samples) {
        Serial.println("❌ Failed to allocate samples buffer for MFCC");
        return;
    }
    for (int i = 0; i < WINDOW_SIZE; i++) {
        samples[i] = (float)audio_samples[i % WINDOW_SIZE] / 32768.0f; // Normalize
        if (i % 128 == 0) {
            vTaskDelay(1 / portTICK_PERIOD_MS);
            esp_task_wdt_reset();
        }
    }
    #if DEBUG_LEVEL >= 3
    Serial.println("Converted samples to float");
    #endif

    float* fft_out = (float*)heap_caps_malloc((WINDOW_SIZE / 2) * sizeof(float), MALLOC_CAP_8BIT);
    if (!fft_out) {
        Serial.println("❌ Failed to allocate fft_out buffer for MFCC");
        heap_caps_free(samples);
        return;
    }
    applyWindow(samples, WINDOW_SIZE);
    computeFFT(samples, fft_out, WINDOW_SIZE);

    for (int i = 0; i < MFCC_NUM_FRAMES * MFCC_NUM_COEFFS; i++) {
        float value = fft_out[i % (WINDOW_SIZE / 2)] * 10.0f; // Scale for int8_t
        mfcc_output[i] = (int8_t)min(max((int)value, -128), 127);
        if (i % 100 == 0) {
            #if DEBUG_LEVEL >= 3
            Serial.printf("Processed %d MFCC coefficients\n", i);
            #endif
            vTaskDelay(1 / portTICK_PERIOD_MS);
            esp_task_wdt_reset();
        }
    }
    #if DEBUG_LEVEL >= 2
    Serial.println("MFCC computation completed");
    #endif
    heap_caps_free(samples);
    heap_caps_free(fft_out);
}

const int16_t AudioProcessor::mfcc_coefficients[MFCC_NUM_COEFFS * MFCC_NUM_FRAMES] = {0};