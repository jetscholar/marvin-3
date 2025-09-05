#include "ManualDSCNN.h"
#include <Arduino.h>
#include "esp_task_wdt.h"

ManualDSCNN::ManualDSCNN() : initialized(false) {}

ManualDSCNN::~ManualDSCNN() {
    cleanup();
}

void ManualDSCNN::cleanup() {
    if (weights) {
        heap_caps_free(weights);
        weights = nullptr;
    }
    initialized = false;
}

bool ManualDSCNN::init() {
    Serial.println("🧠 Initializing ManualDSCNN...");
    esp_task_wdt_reset();

    if (initialized) {
        Serial.println("⚠️ ManualDSCNN already initialized");
        return true;
    }

    weights = (float*)heap_caps_malloc(1024 * sizeof(float), MALLOC_CAP_8BIT);
    if (!weights) {
        Serial.println("❌ Failed to allocate weights");
        return false;
    }
    Serial.println("✅ Weights allocated");
    esp_task_wdt_reset();

    for (int i = 0; i < 1024; i++) {
        weights[i] = 0.01f; // Simple non-zero weights
        if (i % 256 == 0) {
            vTaskDelay(1 / portTICK_PERIOD_MS);
            esp_task_wdt_reset();
        }
    }
    Serial.println("✅ Weights initialized");
    esp_task_wdt_reset();

    Serial.printf("Heap after ManualDSCNN init: %u bytes\n", esp_get_free_heap_size());
    initialized = true;
    Serial.println("✅ ManualDSCNN initialized successfully");
    return true;
}

float ManualDSCNN::predict(const int8_t* input) {
    if (!initialized || !weights) {
        Serial.println("⚠️ ManualDSCNN not initialized or weights null");
        return 0.0f;
    }

    float confidence = 0.0f;
    int non_zero_count = 0;
    for (int i = 0; i < MFCC_NUM_FRAMES * MFCC_NUM_COEFFS; i++) {
        if (input[i] != 0) non_zero_count++;
        confidence += input[i] * weights[i % 1024];
        if (i % 100 == 0) {
            vTaskDelay(1 / portTICK_PERIOD_MS);
            esp_task_wdt_reset();
        }
    }
    confidence /= (MFCC_NUM_FRAMES * MFCC_NUM_COEFFS);
    Serial.printf("DSCNN predict: %d non-zero MFCCs, confidence: %.3f\n", non_zero_count, confidence);
    return confidence;
}