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
        vTaskDelay(1000 / portTICK_PERIOD_MS); // Delay to capture speech
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
    int16_t* test_audio = (int16_t*)malloc(WINDOW_SIZE * sizeof(int16_t));
    if (!test_audio) {
        Serial.println("❌ Failed to allocate test_audio buffer");
        esp_task_wdt_reset();
        return;
    }
    for (int i = 0; i < WINDOW_SIZE; i++) {
        test_audio[i] = (int16_t)(sin(2.0 * PI * 440.0 * i / 16000.0) * 10000);
        if (i % 128 == 0) {
            vTaskDelay(1 / portTICK_PERIOD_MS);
            esp_task_wdt_reset();
        }
    }
    Serial.printf("Generated test audio in %lu ms\n", millis() - start_time);
    esp_task_wdt_reset();
    
    try {
        int8_t mfcc_output[MFCC_NUM_FRAMES * MFCC_NUM_COEFFS];
        start_time = millis();
        Serial.println("Starting MFCC computation...");
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
        free(test_audio);
        esp_task_wdt_reset();
        esp_restart();
    }
    
    free(test_audio);
    Serial.printf("Heap after test: %u bytes\n", esp_get_free_heap_size());
    esp_task_wdt_reset();
}

void wakeWordTask(void* pvParameters) {
    if (!detector) {
        Serial.println("❌ Detector not initialized");
        vTaskDelete(NULL);
    }

    while (true) {
        vTaskDelay(1000 / portTICK_PERIOD_MS); // Delay to capture speech
        if (detector->detect()) {
            Serial.printf("🎯 Wake word detected! Confidence: %.3f, Count: %d\n",
                          detector->getThreshold(), detector->getDetectionCount());
            esp_task_wdt_reset();
        } else {
            Serial.printf("No wake word detected, confidence: %.3f\n", detector->getThreshold());
        }

        int8_t mfcc_output[MFCC_NUM_FRAMES * MFCC_NUM_COEFFS];
        AudioProcessor* processor = detector->getAudioProcessor();
        int16_t* buffer = detector->getAudioBuffer();
        if (processor && buffer) {
            processor->computeMFCC(buffer, mfcc_output);
            Serial.print("Wake word MFCC (first 10): ");
            for (int i = 0; i < 10; i++) {
                Serial.print((int)mfcc_output[i]);
                Serial.print(" ");
                vTaskDelay(1 / portTICK_PERIOD_MS);
                esp_task_wdt_reset();
            }
            Serial.println();
        } else {
            Serial.println("❌ AudioProcessor or audio_buffer not available");
        }
        vTaskDelay(DETECTION_COOLDOWN_MS / portTICK_PERIOD_MS);
        esp_task_wdt_reset();
    }
}

void healthCheckTask(void* pvParameters) {
    while (true) {
        unsigned long current_time = millis();
        if (current_time - last_health_check >= 10000) {
            min_free_heap = min(min_free_heap, esp_get_free_heap_size());
            Serial.printf("💗 Health check: Heap free: %u bytes, Min heap: %u bytes, Uptime: %lu ms\n",
                          esp_get_free_heap_size(), min_free_heap, current_time - system_start_time);
            last_health_check = current_time;
            esp_task_wdt_reset();
        }
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

void setup() {
    Serial.begin(115200);
    Serial.println("\n🚀 Marvin-3 Wake Word Detection System");
    Serial.println("=====================================");
    Serial.println("🧪 Running component tests...");
    
    testAudioCapture();
    testAudioProcessor();
    Serial.println("🧪 Component tests completed\n");
    esp_task_wdt_reset();

    Serial.println("⚙️ Configuring watchdog timer...");
    esp_task_wdt_init(15, true);
    esp_task_wdt_add(NULL);
    
    Serial.println("🧠 Initializing wake word detector...");
    detector = new WakeWordDetector();
    if (!detector || !detector->init()) {
        Serial.println("❌ Wake word detector initialization failed");
        esp_restart();
    }
    Serial.println("✅ Wake word detector initialized");
    Serial.printf("🎯 Using detection threshold: %.3f\n", detector->getThreshold());
    Serial.printf("Heap after detector init: %u bytes\n", esp_get_free_heap_size());
    
    system_start_time = millis();
    min_free_heap = esp_get_free_heap_size();
    Serial.printf("📊 Initial heap: %u bytes\n", min_free_heap);
    Serial.printf("⚡ CPU frequency: %u MHz\n", getCpuFrequencyMhz());
    Serial.println("🎤 Listening for 'marvin'...");
    Serial.println("=====================================");
    
    xTaskCreatePinnedToCore(wakeWordTask, "WakeWordTask", 8192, NULL, 5, &wakeWordTaskHandle, 1);
    Serial.println("🎤 Wake word detection task started");
    xTaskCreatePinnedToCore(healthCheckTask, "HealthCheckTask", 4096, NULL, 1, &healthCheckTaskHandle, 0);
    Serial.println("💗 Health monitoring task started");
}

void loop() {
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    esp_task_wdt_reset();
}