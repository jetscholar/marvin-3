#include "WakeWordDetector.h"
#include <cstring>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_task_wdt.h"
#include "esp_heap_caps.h"
#include <Arduino.h>
#include <cstddef>
#include <cstdio>

#if defined(ARDUINO)
#define DEBUG_PRINT(x) Serial.println(x)
#define GET_MILLIS() millis()
#else
#define DEBUG_PRINT(x) printf("%s\n", x)
#define GET_MILLIS() (xTaskGetTickCount() * portTICK_PERIOD_MS)
#endif

WakeWordDetector::WakeWordDetector() :
    audio_capture(nullptr),
    audio_processor(nullptr),
    dscnn(nullptr),
    detection_count(0),
    last_detection_time(0),
    confidence_threshold(WAKE_WORD_THRESHOLD) {
    memset(audio_buffer, 0, sizeof(audio_buffer));
}

WakeWordDetector::~WakeWordDetector() {
    cleanup();
}

void WakeWordDetector::cleanup() {
    if (audio_capture) {
        delete audio_capture;
        audio_capture = nullptr;
    }
    if (audio_processor) {
        delete audio_processor;
        audio_processor = nullptr;
    }
    if (dscnn) {
        delete dscnn;
        dscnn = nullptr;
    }
}

bool WakeWordDetector::init() {
    DEBUG_PRINT("🧠 Initializing wake word detector...");
    
    cleanup();
    esp_task_wdt_reset();
    
    audio_capture = new AudioCapture();
    if (!audio_capture || !audio_capture->init()) {
        DEBUG_PRINT("❌ Failed to initialize audio capture");
        cleanup();
        return false;
    }
    DEBUG_PRINT("✅ Audio capture initialized");
    
    esp_task_wdt_reset();
    
    audio_processor = new AudioProcessor();
    if (!audio_processor) {
        DEBUG_PRINT("❌ Failed to create audio processor");
        cleanup();
        return false;
    }
    DEBUG_PRINT("✅ Audio processor created");
    
    esp_task_wdt_reset();
    
    dscnn = new ManualDSCNN();
    if (!dscnn || !dscnn->init(65536)) {
        DEBUG_PRINT("❌ Failed to initialize DSCNN model");
        cleanup();
        return false;
    }
    DEBUG_PRINT("✅ DSCNN model initialized");
    
    esp_task_wdt_reset();
    
    DEBUG_PRINT("🎉 Wake word detector fully initialized");
    return true;
}

bool WakeWordDetector::detect() {
    if (!isInitialized()) {
        DEBUG_PRINT("❌ Detector not initialized");
        return false;
    }
    
    unsigned long current_time = GET_MILLIS();
    
    if (current_time - last_detection_time < DETECTION_COOLDOWN_MS) {
        return false;
    }
    
    esp_task_wdt_reset();
    
    size_t samples_per_chunk = WINDOW_SIZE;
    size_t total_samples = 16000;
    size_t chunks = total_samples / samples_per_chunk;
    size_t buffer_offset = 0;
    
    unsigned long start_time = GET_MILLIS();
    
    for (size_t i = 0; i < chunks; i++) {
        if (!audio_capture->read(&audio_buffer[buffer_offset], samples_per_chunk)) {
            DEBUG_PRINT("❌ Failed to read audio samples");
            return false;
        }
        buffer_offset += samples_per_chunk;
        vTaskDelay(1 / portTICK_PERIOD_MS);
        esp_task_wdt_reset();
    }
    
    unsigned long capture_time = GET_MILLIS() - start_time;
    if (capture_time > 50) {
        char msg[64];
        snprintf(msg, sizeof(msg), "⚠️ Slow audio capture: %lu ms", capture_time);
        DEBUG_PRINT(msg);
    }
    
    int8_t mfcc_features[MFCC_NUM_FRAMES * MFCC_NUM_COEFFS];
    start_time = GET_MILLIS();
    
    AudioProcessor::computeMFCC(audio_buffer, mfcc_features);
    
    unsigned long mfcc_time = GET_MILLIS() - start_time;
    if (mfcc_time > 100) {
        char msg[64];
        snprintf(msg, sizeof(msg), "⚠️ Slow MFCC computation: %lu ms", mfcc_time);
        DEBUG_PRINT(msg);
    }
    
    DEBUG_PRINT("✅ MFCC features computed");
    
    esp_task_wdt_reset();
    
    size_t free_heap = esp_get_free_heap_size();
    if (free_heap < 50000) {
        char msg[64];
        snprintf(msg, sizeof(msg), "⚠️ Low memory: %u bytes", free_heap);
        DEBUG_PRINT(msg);
    }
    
    float predictions[12];
    start_time = GET_MILLIS();
    
    dscnn->infer(mfcc_features, predictions);
    
    unsigned long inference_time = GET_MILLIS() - start_time;
    if (inference_time > 200) {
        char msg[64];
        snprintf(msg, sizeof(msg), "⚠️ Slow inference: %lu ms", inference_time);
        DEBUG_PRINT(msg);
    }
    
    DEBUG_PRINT("✅ Inference completed");
    
    esp_task_wdt_reset();
    
    int predicted_class = dscnn->getPredictedClass(predictions);
    float confidence = dscnn->getConfidence(predictions, predicted_class);
    const char* class_name = dscnn->getClassName(predicted_class);
    
    bool wake_word_detected = false;
    
    if (predicted_class >= 2 && confidence >= confidence_threshold) {
        wake_word_detected = true;
        detection_count++;
        last_detection_time = current_time;
        
        char result_msg[128];
        snprintf(result_msg, sizeof(result_msg), 
                "🎯 WAKE WORD DETECTED! Class: %s, Confidence: %.2f", 
                class_name, confidence);
        DEBUG_PRINT(result_msg);
    } else if (confidence > 0.3f) {
        char result_msg[128];
        snprintf(result_msg, sizeof(result_msg), 
                "🔍 Detection: %s (%.2f) - below threshold", 
                class_name, confidence);
        DEBUG_PRINT(result_msg);
    }
    
    vTaskDelay(pdMS_TO_TICKS(10));
    
    esp_task_wdt_reset();
    
    return wake_word_detected;
}

void WakeWordDetector::setThreshold(float threshold) {
    confidence_threshold = threshold;
}

float WakeWordDetector::getThreshold() const {
    return confidence_threshold;
}

int WakeWordDetector::getDetectionCount() const {
    return detection_count;
}

void WakeWordDetector::resetDetectionCount() {
    detection_count = 0;
}

bool WakeWordDetector::isInitialized() const {
    return (audio_capture != nullptr && 
            audio_processor != nullptr && 
            dscnn != nullptr &&
            dscnn->isInitialized());
}