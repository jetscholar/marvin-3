#include <Arduino.h>
#include "WakeWordDetector.h"
#include "AudioCapture.h"
#include "AudioProcessor.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_task_wdt.h"
#include "esp_heap_caps.h"
#include "esp_system.h"
#include <cmath>

// Global detector instance
WakeWordDetector* detector = nullptr;

// Task handles
TaskHandle_t wakeWordTaskHandle = nullptr;
TaskHandle_t healthCheckTaskHandle = nullptr;

// System health variables
unsigned long last_health_check = 0;
unsigned long system_start_time = 0;
size_t min_free_heap = SIZE_MAX;

void testAudioCapture() {
    Serial.println("🔬 Testing AudioCapture integration...");
    
    AudioCapture audio;
    if (!audio.init()) {
        Serial.println("❌ AudioCapture init failed");
        return;
    }
    Serial.println("✅ AudioCapture initialized");
    
    // Test reading some samples
    int16_t test_buffer[1000];
    if (audio.read(test_buffer, 1000)) {
        Serial.println("✅ AudioCapture read successful");
        
        // Check if we're getting real audio data (not all zeros)
        bool has_data = false;
        for (int i = 0; i < 1000; i++) {
            if (test_buffer[i] != 0) {
                has_data = true;
                break;
            }
        }
        
        if (has_data) {
            Serial.println("✅ Audio data detected");
        } else {
            Serial.println("⚠️ Audio data appears to be all zeros (check microphone)");
        }
        
        // Print first few samples for debugging
        Serial.print("First 10 samples: ");
        for (int i = 0; i < 10; i++) {
            Serial.print(test_buffer[i]);
            Serial.print(" ");
        }
        Serial.println();
        
    } else {
        Serial.println("❌ AudioCapture read failed");
    }
}

void testAudioProcessor() {
    Serial.println("🔬 Testing AudioProcessor integration...");
    
    // Create some dummy audio data
    int16_t test_audio[16000];
    for (int i = 0; i < 16000; i++) {
        test_audio[i] = (int16_t)(sin(2.0 * PI * 440.0 * i / 16000.0) * 1000); // 440Hz tone
    }
    
    // Test MFCC computation
    int8_t mfcc_output[490]; // 49 * 10
    AudioProcessor::computeMFCC(test_audio, mfcc_output);
    
    // Check if MFCC output is reasonable
    bool has_mfcc_data = false;
    for (int i = 0; i < 490; i++) {
        if (mfcc_output[i] != 0) {
            has_mfcc_data = true;
            break;
        }
    }
    
    if (has_mfcc_data) {
        Serial.println("✅ MFCC computation successful");
        
        // Print first few MFCC values
        Serial.print("First 10 MFCC values: ");
        for (int i = 0; i < 10; i++) {
            Serial.print((int)mfcc_output[i]);
            Serial.print(" ");
        }
        Serial.println();
    } else {
        Serial.println("⚠️ MFCC output appears to be all zeros");
    }
}

void wakeWordTask(void* parameter) {
    Serial.println("🎤 Wake word detection task started");
    
    // Add this task to watchdog monitoring
    esp_task_wdt_add(NULL);
    
    while (true) {
        try {
            // Feed watchdog at start of loop
            esp_task_wdt_reset();
            
            if (detector && detector->isInitialized()) {
                // Perform wake word detection
                bool detected = detector->detect();
                
                if (detected) {
                    Serial.println("🎉 WAKE WORD 'MARVIN' DETECTED!");
                    
                    // Add your wake word action here
                    // e.g., start voice assistant, send notification, etc.
                    
                    // Brief delay after detection
                    vTaskDelay(pdMS_TO_TICKS(500));
                }
            } else {
                Serial.println("⚠️ Detector not ready, reinitializing...");
                if (detector && !detector->init()) {
                    Serial.println("❌ Reinitialization failed, restarting system");
                    esp_restart();
                }
                vTaskDelay(pdMS_TO_TICKS(1000));
            }
            
            // Small delay to prevent overwhelming the system
            vTaskDelay(pdMS_TO_TICKS(50));
            
        } catch (...) {
            Serial.println("❌ Exception in wake word task, restarting...");
            esp_restart();
        }
    }
    
    // Remove from watchdog (should never reach here)
    esp_task_wdt_delete(NULL);
    vTaskDelete(NULL);
}

void healthCheckTask(void* parameter) {
    Serial.println("💗 Health monitoring task started");
    
    while (true) {
        unsigned long current_time = millis();
        
        // Check system health every 30 seconds
        if (current_time - last_health_check >= 30000) {
            last_health_check = current_time;
            
            // Monitor heap usage
            size_t free_heap = esp_get_free_heap_size();
            size_t total_heap = heap_caps_get_total_size(MALLOC_CAP_DEFAULT);
            
            if (free_heap < min_free_heap) {
                min_free_heap = free_heap;
            }
            
            // Log system health
            unsigned long uptime = current_time - system_start_time;
            Serial.printf("💗 Health check: heap=%u, min=%u, uptime=%lu ms\n", 
                         free_heap, min_free_heap, uptime);
            
            // Check for critical memory conditions
            if (free_heap < 30000) { // Less than 30KB free
                Serial.println("⚠️ CRITICAL: Low memory condition detected");
                
                if (free_heap < 10000) { // Less than 10KB free
                    Serial.println("🚨 EMERGENCY: Memory exhausted, restarting system");
                    vTaskDelay(pdMS_TO_TICKS(1000)); // Give time for message to send
                    esp_restart();
                }
            }
            
            // Check detector health
            if (detector && !detector->isInitialized()) {
                Serial.println("⚠️ Detector not initialized, attempting recovery");
                if (!detector->init()) {
                    Serial.println("❌ Recovery failed, system may be unstable");
                }
            }
        }
        
        // Check every 5 seconds
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

void setup() {
    Serial.begin(115200);
    while (!Serial) {
        delay(10);
    }
    
    Serial.println("\n🚀 Marvin-3 Wake Word Detection System");
    Serial.println("=====================================");
    
    // Run basic component tests first
    Serial.println("🧪 Running component tests...");
    testAudioCapture();
    testAudioProcessor();
    Serial.println("🧪 Component tests completed\n");
    
    system_start_time = millis();
    last_health_check = system_start_time;
    
    // Configure watchdog timer
    Serial.println("⚙️ Configuring watchdog timer...");
    esp_task_wdt_init(10, true); // 10 second timeout, panic on timeout
    
    // Initialize detector
    Serial.println("🧠 Initializing wake word detector...");
    detector = new WakeWordDetector();
    
    if (!detector) {
        Serial.println("❌ Failed to create detector instance");
        Serial.println("🔄 Restarting system...");
        delay(2000);
        esp_restart();
        return;
    }
    
    if (!detector->init()) {
        Serial.println("❌ Failed to initialize wake word detector");
        Serial.println("🔄 Restarting system...");
        delay(2000);
        esp_restart();
        return;
    }
    
    Serial.println("✅ Wake word detector initialized");
    
    // Set detection threshold (use the one from env.h)
    Serial.printf("🎯 Using detection threshold: %.3f\n", detector->getThreshold());
    
    // Create wake word detection task on Core 1
    xTaskCreatePinnedToCore(
        wakeWordTask,           // Task function
        "WakeWordTask",         // Task name
        8192,                   // Stack size (8KB)
        NULL,                   // Parameters
        2,                      // Priority (higher than default)
        &wakeWordTaskHandle,    // Task handle
        1                       // Core 1
    );
    
    if (wakeWordTaskHandle == nullptr) {
        Serial.println("❌ Failed to create wake word task");
        Serial.println("🔄 Restarting system...");
        delay(2000);
        esp_restart();
        return;
    }
    
    // Create health monitoring task on Core 0
    xTaskCreatePinnedToCore(
        healthCheckTask,         // Task function
        "HealthCheckTask",       // Task name
        4096,                    // Stack size (4KB)
        NULL,                    // Parameters
        1,                       // Priority (lower than wake word task)
        &healthCheckTaskHandle,  // Task handle
        0                        // Core 0
    );
    
    if (healthCheckTaskHandle == nullptr) {
        Serial.println("⚠️ Failed to create health check task (non-critical)");
    }
    
    // Log initial system state
    size_t free_heap = esp_get_free_heap_size();
    min_free_heap = free_heap;
    
    Serial.printf("📊 Initial heap: %u bytes\n", free_heap);
    Serial.printf("⚡ CPU frequency: %u MHz\n", getCpuFrequencyMhz());
    Serial.println("🎤 Listening for 'marvin'...");
    Serial.println("=====================================\n");
}

void loop() {
    // Main loop is kept minimal since tasks handle the work
    
    // Just monitor system stability
    static unsigned long last_loop_time = 0;
    unsigned long current_time = millis();
    
    if (current_time - last_loop_time >= 60000) { // Every minute
        last_loop_time = current_time;
        
        // Verify tasks are still running
        if (eTaskGetState(wakeWordTaskHandle) == eDeleted) {
            Serial.println("❌ Wake word task died, restarting system");
            esp_restart();
        }
        
        // Log detection statistics
        if (detector) {
            int detection_count = detector->getDetectionCount();
            Serial.printf("📈 Total detections: %d\n", detection_count);
        }
    }
    
    // Minimal delay to prevent watchdog issues in main loop
    delay(1000);
}