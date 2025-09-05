#include "WakeWordDetector.h"
#include <cstring>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_task_wdt.h"
#include "esp_heap_caps.h"
#include <Arduino.h>

WakeWordDetector::WakeWordDetector() 
    : audio_capture(nullptr), audio_processor(nullptr), dscnn(nullptr), 
      detection_count(0), last_detection_time(0), confidence_threshold(0.1f) {
    memset(audio_buffer, 0, MAX_AUDIO_BUFFER_SIZE * sizeof(int16_t));
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
    Serial.println("🧠 Initializing wake word detector...");
    cleanup(); // Ensure clean state
    esp_task_wdt_reset();

    Serial.printf("Heap before allocations: %u bytes\n", esp_get_free_heap_size());

    audio_capture = new AudioCapture();
    if (!audio_capture) {
        Serial.println("❌ Failed to create AudioCapture");
        return false;
    }
    Serial.println("✅ Audio capture created");
    esp_task_wdt_reset();

    if (!audio_capture->init()) {
        Serial.println("❌ Audio capture initialization failed");
        cleanup();
        return false;
    }
    Serial.println("✅ Audio capture initialized");
    esp_task_wdt_reset();

    audio_processor = new AudioProcessor();
    if (!audio_processor) {
        Serial.println("❌ Failed to create AudioProcessor");
        cleanup();
        return false;
    }
    Serial.println("✅ Audio processor created");
    esp_task_wdt_reset();

    dscnn = new ManualDSCNN();
    if (!dscnn) {
        Serial.println("❌ Failed to create ManualDSCNN");
        cleanup();
        return false;
    }
    Serial.println("🧠 Initializing ManualDSCNN...");
    esp_task_wdt_reset();

    if (!dscnn->init()) {
        Serial.println("❌ ManualDSCNN initialization failed");
        cleanup();
        return false;
    }
    Serial.println("✅ DSCNN model initialized");
    esp_task_wdt_reset();

    Serial.printf("Heap after allocations: %u bytes\n", esp_get_free_heap_size());
    Serial.println("🎉 Wake word detector fully initialized");
    return true;
}

bool WakeWordDetector::detect() {
    if (!audio_capture || !audio_processor || !dscnn) {
        Serial.println("⚠️ Detector components not initialized");
        return false;
    }

    size_t total_samples = 1000; // Match testAudioCapture
    if (!audio_capture->read(audio_buffer, total_samples)) {
        Serial.println("⚠️ Audio capture failed");
        return false;
    }
    esp_task_wdt_reset();

    int8_t mfcc_output[MFCC_NUM_FRAMES * MFCC_NUM_COEFFS];
    audio_processor->computeMFCC(audio_buffer, mfcc_output);
    esp_task_wdt_reset();

    float confidence = dscnn->predict(mfcc_output);
    bool detected = confidence > confidence_threshold;

    if (detected && (millis() - last_detection_time) > DETECTION_COOLDOWN_MS) {
        detection_count++;
        last_detection_time = millis();
        return true;
    }
    return false;
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
    return audio_capture && audio_processor && dscnn;
}