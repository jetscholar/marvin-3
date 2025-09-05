#include "ManualDSCNN.h"
#include <cstring>
#include <cstdlib>
#include <algorithm>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_task_wdt.h"

#if defined(ARDUINO)
#include "Arduino.h"
#define DEBUG_PRINT(x) Serial.println(x)
#else
#define DEBUG_PRINT(x) printf("%s\n", x)
#endif

// Class name mapping
const char* ManualDSCNN::class_names[NUM_CLASSES] = {
    "_silence_", "_unknown_", "yes", "no", "up", "down", 
    "left", "right", "on", "off", "stop", "go"
};

ManualDSCNN::ManualDSCNN() : 
    arena_buffer(nullptr),
    input_buffer(nullptr), 
    intermediate_buffer(nullptr),
    output_buffer(nullptr),
    arena_size(0) {
}

ManualDSCNN::~ManualDSCNN() {
    cleanup();
}

void ManualDSCNN::cleanup() {
    if (arena_buffer) {
        free(arena_buffer);
        arena_buffer = nullptr;
    }
    input_buffer = nullptr;
    intermediate_buffer = nullptr;
    output_buffer = nullptr;
    arena_size = 0;
}

bool ManualDSCNN::init(size_t requested_arena_size) {
    DEBUG_PRINT("🧠 Initializing ManualDSCNN...");
    
    // Clean up any existing allocation
    cleanup();
    
    // Set minimum arena size
    arena_size = std::max(requested_arena_size, (size_t)32768);
    
    // Allocate arena buffer
    arena_buffer = (uint8_t*)malloc(arena_size);
    if (!arena_buffer) {
        DEBUG_PRINT("❌ Failed to allocate arena buffer");
        return false;
    }
    
    // Set up buffer pointers within arena
    size_t offset = 0;
    
    // Input buffer (INPUT_HEIGHT * INPUT_WIDTH = 490 bytes)
    input_buffer = (int8_t*)(arena_buffer + offset);
    offset += INPUT_HEIGHT * INPUT_WIDTH;
    
    // Intermediate buffer (8KB for processing)
    intermediate_buffer = (int8_t*)(arena_buffer + offset);
    offset += 4096; // Reduced from 8192 to save memory
    
    // Output buffer (NUM_CLASSES * 4 = 48 bytes for floats)
    output_buffer = (float*)(arena_buffer + offset);
    offset += NUM_CLASSES * sizeof(float);
    
    // Check if we have enough space
    if (offset > arena_size) {
        DEBUG_PRINT("❌ Arena too small for required buffers");
        cleanup();
        return false;
    }
    
    // Initialize buffers
    memset(input_buffer, 0, INPUT_HEIGHT * INPUT_WIDTH);
    memset(intermediate_buffer, 0, 4096);
    memset(output_buffer, 0, NUM_CLASSES * sizeof(float));
    
    DEBUG_PRINT("✅ ManualDSCNN initialized successfully");
    return true;
}

void ManualDSCNN::infer(const int8_t* input, float* output) {
    if (!arena_buffer) {
        DEBUG_PRINT("❌ Model not initialized");
        return;
    }
    
    // Feed watchdog to prevent reset
    esp_task_wdt_reset();
    
    DEBUG_PRINT("🧠 Starting inference...");
    
    // Copy input to buffer
    memcpy(input_buffer, input, INPUT_HEIGHT * INPUT_WIDTH);
    
    // Simulate DSCNN processing with watchdog feeding
    // Layer 1: Depthwise separable convolution
    for (int i = 0; i < INPUT_HEIGHT; i++) {
        for (int j = 0; j < INPUT_WIDTH; j++) {
            int idx = i * INPUT_WIDTH + j;
            intermediate_buffer[idx] = input_buffer[idx] * 2; // Simple scaling
        }
        
        // Feed watchdog every 16 frames
        if (i % 16 == 0) {
            esp_task_wdt_reset();
            vTaskDelay(pdMS_TO_TICKS(1)); // Yield to other tasks
        }
    }
    
    // Layer 2: Pointwise convolution (simplified)
    esp_task_wdt_reset();
    for (int i = 0; i < NUM_CLASSES; i++) {
        float sum = 0.0f;
        for (int j = 0; j < 40; j++) { // Reduced computation
            sum += intermediate_buffer[j] * 0.1f; // Simple weights
        }
        output_buffer[i] = sum;
    }
    
    // Apply activation (ReLU + Softmax approximation)
    esp_task_wdt_reset();
    float max_val = output_buffer[0];
    for (int i = 1; i < NUM_CLASSES; i++) {
        if (output_buffer[i] > max_val) {
            max_val = output_buffer[i];
        }
    }
    
    // Softmax approximation
    float sum_exp = 0.0f;
    for (int i = 0; i < NUM_CLASSES; i++) {
        output_buffer[i] = exp(output_buffer[i] - max_val);
        sum_exp += output_buffer[i];
    }
    
    for (int i = 0; i < NUM_CLASSES; i++) {
        output_buffer[i] /= sum_exp;
    }
    
    // Copy to output
    memcpy(output, output_buffer, NUM_CLASSES * sizeof(float));
    
    esp_task_wdt_reset();
    DEBUG_PRINT("✅ Inference completed");
}

int ManualDSCNN::getPredictedClass(const float* predictions) {
    int best_class = 0;
    float best_score = predictions[0];
    
    for (int i = 1; i < NUM_CLASSES; i++) {
        if (predictions[i] > best_score) {
            best_score = predictions[i];
            best_class = i;
        }
    }
    
    return best_class;
}

float ManualDSCNN::getConfidence(const float* predictions, int class_idx) {
    if (class_idx >= 0 && class_idx < NUM_CLASSES) {
        return predictions[class_idx];
    }
    return 0.0f;
}

const char* ManualDSCNN::getClassName(int class_idx) {
    if (class_idx >= 0 && class_idx < NUM_CLASSES) {
        return class_names[class_idx];
    }
    return "unknown";
}

size_t ManualDSCNN::getArenaSize() const {
    return arena_size;
}