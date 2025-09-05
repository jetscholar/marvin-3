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
#include "env.h"

WakeWordDetector* detector = nullptr;
TaskHandle_t wakeWordTaskHandle = nullptr;
TaskHandle_t healthCheckTaskHandle = nullptr;
unsigned long last_health_check = 0;
unsigned long system_start_time = 0;
size_t min_free_heap = SIZE_MAX;

void testAudioCapture() {
    Serial.println("🔬 Testing AudioCapture integration...");
    Serial.printf("Heap before test: %u bytes\n", esp_get_free_heap_size());
    esp_task_wdt_reset();
    
    AudioCapture audio;
    if (!audio.init()) {
        Serial.println("❌ AudioCapture init failed");
        return;
    }
    Serial.println("✅ AudioCapture initialized");
    esp_task_wdt_reset();
    
    int16_t test_buffer[1000];
    bool read_success = false;
    for (int attempt = 1; attempt <= 3; attempt++) {
        Serial.printf("🔍 Read attempt %d/3\n", attempt);
        if (audio.read(test_buffer, 1000)) {
            read_success = true;
            break;
        }
        Serial.println("⚠️ AudioCapture read failed, retrying...");
        vTaskDelay(100 / portTICK_PERIOD_MS);
        esp_task_wdt_reset();
    }
    
    if (read_success) {
        Serial.println("✅ AudioCapture read successful");
        int non_zero_count = 0;
        int max_amplitude = 0;
        for (int i = 0; i < 1000; i++) {
            if (test_buffer[i] != 0) {
                non_zero_count++;
                max_amplitude = max(max_amplitude, abs(test_buffer[i]));
            }
            if (i % 200 == 0) {
                vTaskDelay(1 / portTICK_PERIOD_MS);
                esp_task_wdt_reset();
            }
        }
        if (non_zero_count > 0) {
            Serial.printf("✅ Audio data detected (%d non-zero samples, max amplitude: %d)\n", 
                         non_zero_count, max_amplitude);
        } else {
            Serial.println("⚠️ Audio data appears to be all zeros (check microphone wiring or environment)");
        }
        Serial.print("First 10 samples: ");
        for (int i = 0; i < 10; i++) {
            Serial.print(test_buffer[i]);
            Serial.print(" ");
            if (i % 5 == 0) {
                vTaskDelay(1 / portTICK_PERIOD_MS);
                esp_task_wdt_reset();
            }
        }
        Serial.println();
    } else {
        Serial.println("❌ AudioCapture read failed after 3 attempts");
        Serial.println("🔍 Check I2S pins (GPIO22=SCK, GPIO25=WS, GPIO26=SD, GND, 3.3V, L/R=GND)");
    }
    Serial.printf("Heap after test: %u bytes\n", esp_get_free_heap_size());
    vTaskDelay(10 / portTICK_PERIOD_MS);
    esp_task_wdt_reset();
}

void testAudioProcessor() {
    Serial.println("🔬 Testing AudioProcessor integration...");
    Serial.printf("Heap before test: %u bytes\n", esp_get_free_heap_size());
    esp_task_wdt_reset();
    
    unsigned long start_time = millis();
    int16_t test_audio[4096]; // Reduced from 16000 to 0.25s
    for (int i = 0; i < 4096; i++) {
        test_audio[i] = (int16_t)(sin(2.0 * PI * 440.0 * i / 16000.0) * 1000);
        if (i % 1024 == 0) {
            vTaskDelay(1 / portTICK_PERIOD_MS);
            esp_task_wdt_reset();
        }
    }
    Serial.printf("Generated test audio in %lu ms\n", millis() - start_time);
    esp_task_wdt_reset();
    
    try {
        int8_t mfcc_output[MFCC_NUM_FRAMES * MFCC_NUM_COEFFS]; // 65 * 10 = 650
        start_time = millis();
        AudioProcessor::computeMFCC(test_audio, mfcc_output);
        Serial.printf("Computed MFCC in %lu ms\n", millis() - start_time);
        esp_task_wdt_reset();
        
        bool has_mfcc_data = false;
        for (int i = 0; i < MFCC_NUM_FRAMES * MFCC_NUM_COEFFS; i++) {
            if (mfcc_output[i] != 0) {
                has_mfcc_data = true;
                break;
            }
            if (i % 100 == 0) {
                vTaskDelay(1 / portTICK_PERIOD_MS);
                esp_task_wdt_reset();
            }
        }
        
        if (has_mfcc_data) {
            Serial.println("✅ MFCC computation successful");
            Serial.print("First 10 MFCC values: ");
            for (int i = 0; i < 10; i++) {
                Serial.print((int)mfcc_output[i]);
                Serial.print(" ");
                vTaskDelay(1 / portTICK_PERIOD_MS);
                esp_task_wdt_reset();
            }
            Serial.println();
        } else {
            Serial.println("⚠️ MFCC output appears to be all zeros");
        }
    } catch (...) {
        Serial.println("❌ Exception in AudioProcessor test");
        Serial.printf("Heap after exception: %u bytes\n", esp_get_free_heap_size());
        esp_restart();
    }
    
    Serial.printf("Heap after test: %u bytes\n", esp_get_free_heap_size());
    esp_task_wdt_reset();
}

void wakeWordTask(void* parameter) {
    Serial.println("🎤 Wake word detection task started");
    esp_task_wdt_add(NULL);
    
    while (true) {
        esp_task_wdt_reset();
        if (detector && detector->isInitialized()) {
            bool detected = detector->detect();
            if (detected) {
                Serial.println("🎉 WAKE WORD 'MARVIN' DETECTED!");
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
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    esp_task_wdt_delete(NULL);
    vTaskDelete(NULL);
}

void healthCheckTask(void* parameter) {
    Serial.println("💗 Health monitoring task started");
    
    while (true) {
        unsigned long current_time = millis();
        if (current_time - last_health_check >= 30000) {
            last_health_check = current_time;
            size_t free_heap = esp_get_free_heap_size();
            size_t total_heap = heap_caps_get_total_size(MALLOC_CAP_DEFAULT);
            if (free_heap < min_free_heap) {
                min_free_heap = free_heap;
            }
            unsigned long uptime = current_time - system_start_time;
            Serial.printf("💗 Health check: heap=%u, min=%u, uptime=%lu ms\n", 
                         free_heap, min_free_heap, uptime);
            if (free_heap < 30000) {
                Serial.println("⚠️ CRITICAL: Low memory condition detected");
                if (free_heap < 10000) {
                    Serial.println("🚨 EMERGENCY: Memory exhausted, restarting system");
                    vTaskDelay(pdMS_TO_TICKS(1000));
                    esp_restart();
                }
            }
            if (detector && !detector->isInitialized()) {
                Serial.println("⚠️ Detector not initialized, attempting recovery");
                if (!detector->init()) {
                    Serial.println("❌ Recovery failed, system may be unstable");
                }
            }
        }
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
    esp_task_wdt_reset();
    
    Serial.println("🧪 Running component tests...");
    testAudioCapture();
    Serial.println("🔬 Starting AudioProcessor test...");
    testAudioProcessor();
    Serial.println("🧪 Component tests completed\n");
    esp_task_wdt_reset();
    
    system_start_time = millis();
    last_health_check = system_start_time;
    
    Serial.println("⚙️ Configuring watchdog timer...");
    esp_task_wdt_init(15, true); // Increased to 15 seconds
    esp_task_wdt_reset();
    
    Serial.println("🧠 Initializing wake word detector...");
    Serial.printf("Heap before detector init: %u bytes\n", esp_get_free_heap_size());
    detector = new WakeWordDetector();
    esp_task_wdt_reset();
    
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
    Serial.printf("🎯 Using detection threshold: %.3f\n", detector->getThreshold());
    Serial.printf("Heap after detector init: %u bytes\n", esp_get_free_heap_size());
    esp_task_wdt_reset();
    
    xTaskCreatePinnedToCore(
        wakeWordTask,
        "WakeWordTask",
        8192,
        NULL,
        2,
        &wakeWordTaskHandle,
        1
    );
    
    if (wakeWordTaskHandle == nullptr) {
        Serial.println("❌ Failed to create wake word task");
        Serial.println("🔄 Restarting system...");
        delay(2000);
        esp_restart();
        return;
    }
    
    xTaskCreatePinnedToCore(
        healthCheckTask,
        "HealthCheckTask",
        4096,
        NULL,
        1,
        &healthCheckTaskHandle,
        0
    );
    
    if (healthCheckTaskHandle == nullptr) {
        Serial.println("⚠️ Failed to create health check task (non-critical)");
    }
    
    size_t free_heap = esp_get_free_heap_size();
    min_free_heap = free_heap;
    
    Serial.printf("📊 Initial heap: %u bytes\n", free_heap);
    Serial.printf("⚡ CPU frequency: %u MHz\n", getCpuFrequencyMhz());
    Serial.println("🎤 Listening for 'marvin'...");
    Serial.println("=====================================\n");
    esp_task_wdt_reset();
}

void loop() {
    static unsigned long last_loop_time = 0;
    unsigned long current_time = millis();
    
    if (current_time - last_loop_time >= 60000) {
        last_loop_time = current_time;
        if (eTaskGetState(wakeWordTaskHandle) == eDeleted) {
            Serial.println("❌ Wake word task died, restarting system");
            esp_restart();
        }
        if (detector) {
            int detection_count = detector->getDetectionCount();
            Serial.printf("📈 Total detections: %d\n", detection_count);
        }
    }
    
    delay(1000);
    esp_task_wdt_reset();
}