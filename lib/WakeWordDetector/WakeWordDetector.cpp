#include "WakeWordDetector.h"
#include <cstring>  // For memcpy
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_task_wdt.h"
#include "esp_heap_caps.h"

#if defined(ARDUINO)
#include "Arduino.h"
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
    
    // Initialize audio buffer
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
    
    // Clean up any existing instances
    cleanup();
    
    // Feed watchdog
    esp_task_wdt_reset();
    
    // Initialize audio capture
    audio_capture = new AudioCapture();
    if (!audio_capture || !audio_capture->init()) {
        DEBUG_PRINT("❌ Failed to initialize audio capture");
        cleanup();
        return false;
    }
    DEBUG_PRINT("✅ Audio capture initialized");
    
    // Feed watchdog
    esp_task_wdt_reset();
    
    // Initialize audio processor (just create instance, no init needed)
    audio_processor = new AudioProcessor();
    if (!audio_processor) {
        DEBUG_PRINT("❌ Failed to create audio processor");
        cleanup();
        return false;
    }
    DEBUG_PRINT("✅ Audio processor created");
    
    // Feed watchdog
    esp_task_wdt_reset();
    
    // Initialize DSCNN model
    dscnn = new ManualDSCNN();
    if (!dscnn || !dscnn->init(65536)) { // 64KB arena
        DEBUG_PRINT("❌ Failed to initialize DSCNN model");
        cleanup();
        return false;
    }
    DEBUG_PRINT("✅ DSCNN model initialized");
    
    // Final watchdog feed
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
    
    // Implement detection cooldown
    if (current_time - last_detection_time < DETECTION_COOLDOWN_MS) {
        return false;
    }
    
    // Feed watchdog at start
    esp_task_wdt_reset();
    
    // Step 1: Capture audio using simple read method
    size_t samples_to_read = 16000; // 1 second at 16kHz
    if (samples_to_read > MAX_AUDIO_BUFFER_SIZE) {
        samples_to_read = MAX_AUDIO_BUFFER_SIZE;
    }
    
    unsigned long start_time = GET_MILLIS();
    
    if (!audio_capture->read(audio_buffer, samples_to_read)) {
        DEBUG_PRINT("❌ Failed to read audio samples");
        return false;
    }
    
    unsigned long capture_time = GET_MILLIS() - start_time;
    if (capture_time > 50) { // Warn if capture takes too long
        DEBUG_PRINT("⚠️ Slow audio capture detected");
    }
    
    // Feed watchdog after capture
    esp_task_wdt_reset();
    
    // Step 2: Process audio to MFCC features using static method
    int8_t mfcc_features[49 * 10]; // INPUT_HEIGHT * INPUT_WIDTH
    start_time = GET_MILLIS();
    
    // Use the static AudioProcessor::computeMFCC method
    AudioProcessor::computeMFCC(audio_buffer, mfcc_features);
    
    unsigned long mfcc_time = GET_MILLIS() - start_time;
    if (mfcc_time > 100) { // Warn if MFCC takes too long
        DEBUG_PRINT("⚠️ Slow MFCC computation detected");
    }
    
    DEBUG_PRINT("✅ MFCC features computed");
    
    // Feed watchdog after MFCC
    esp_task_wdt_reset();
    
    // Log memory usage
    size_t free_heap = esp_get_free_heap_size();
    if (free_heap < 50000) { // Less than 50KB free
        DEBUG_PRINT("⚠️ Low memory detected");
    }
    
    // Step 3: Run inference
    float predictions[12]; // NUM_CLASSES
    start_time = GET_MILLIS();
    
    dscnn->infer(mfcc_features, predictions);
    
    unsigned long inference_time = GET_MILLIS() - start_time;
    if (inference_time > 200) { // Warn if inference takes too long
        DEBUG_PRINT("⚠️ Slow inference detected");
    }
    
    DEBUG_PRINT("✅ Inference completed");
    
    // Feed watchdog after inference
    esp_task_wdt_reset();
    
    // Step 4: Get prediction results
    int predicted_class = dscnn->getPredictedClass(predictions);
    float confidence = dscnn->getConfidence(predictions, predicted_class);
    const char* class_name = dscnn->getClassName(predicted_class);
    
    // Step 5: Check for wake word detection
    // Look for "marvin" or similar patterns in the classes
    // For now, we'll check for high confidence on non-silence/unknown classes
    bool wake_word_detected = false;
    
    if (predicted_class >= 2 && confidence >= confidence_threshold) { // Skip silence and unknown
        wake_word_detected = true;
        detection_count++;
        last_detection_time = current_time;
        
        char result_msg[128];
        snprintf(result_msg, sizeof(result_msg), 
                "🎯 WAKE WORD DETECTED! Class: %s, Confidence: %.2f", 
                class_name, confidence);
        DEBUG_PRINT(result_msg);
    } else {
        // Log low confidence detections for debugging
        if (confidence > 0.3f) {
            char result_msg[128];
            snprintf(result_msg, sizeof(result_msg), 
                    "🔍 Detection: %s (%.2f) - below threshold", 
                    class_name, confidence);
            DEBUG_PRINT(result_msg);
        }
    }
    
    // Add small delay to prevent overwhelming the system
    vTaskDelay(pdMS_TO_TICKS(10));
    
    // Final watchdog feed
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