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
    float imag[WINDOW_SIZE] = {0};
    ArduinoFFT<float> FFT = ArduinoFFT<float>(samples, imag, length, SAMPLE_RATE);
    FFT.windowing(FFT_WIN_TYP_HAMMING, FFT_FORWARD);
    FFT.compute(FFT_FORWARD);
    FFT.complexToMagnitude();
    for (int i = 0; i < length; i++) {
        output[i] = samples[i];
        if (i % 128 == 0) {
            vTaskDelay(1 / portTICK_PERIOD_MS);
            esp_task_wdt_reset();
        }
    }
    #if DEBUG_LEVEL >= 3
    Serial.println("FFT computation completed");
    #endif
}

void AudioProcessor::computeMFCC(const int16_t* audio_samples, int8_t* mfcc_output) {
    #if DEBUG_LEVEL >= 2
    Serial.printf("Starting MFCC computation for %d samples\n", WINDOW_SIZE);
    #endif
    float samples[WINDOW_SIZE];
    for (int i = 0; i < WINDOW_SIZE; i++) {
        samples[i] = (float)audio_samples[i];
        if (i % 128 == 0) {
            vTaskDelay(1 / portTICK_PERIOD_MS);
            esp_task_wdt_reset();
        }
    }
    #if DEBUG_LEVEL >= 3
    Serial.println("Converted samples to float");
    #endif

    float fft_out[WINDOW_SIZE];
    applyWindow(samples, WINDOW_SIZE);
    computeFFT(samples, fft_out, WINDOW_SIZE);

    for (int i = 0; i < MFCC_NUM_FRAMES * MFCC_NUM_COEFFS; i++) {
        int value = (int)fft_out[i % WINDOW_SIZE];
        mfcc_output[i] = (int8_t)(value >> 8);
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
}

const int16_t AudioProcessor::mfcc_coefficients[MFCC_NUM_COEFFS * MFCC_NUM_FRAMES] = {0};