#ifndef MANUAL_DSCNN_H
#define MANUAL_DSCNN_H

#include <stdint.h>
#include <cstddef>

// Model constants
#define INPUT_HEIGHT 49
#define INPUT_WIDTH 10
#define NUM_CLASSES 12

class ManualDSCNN {
private:
    // Memory buffers
    uint8_t* arena_buffer;
    int8_t* input_buffer;
    int8_t* intermediate_buffer;
    float* output_buffer;
    
    // Arena size
    size_t arena_size;
    
    // Model parameters
    static const char* class_names[NUM_CLASSES];
    
public:
    // Constructor and destructor
    ManualDSCNN();
    ~ManualDSCNN();
    
    // Initialization
    bool init(size_t arena_size);
    
    // Main inference method
    void infer(const int8_t* input, float* output);
    
    // Utility methods
    int getPredictedClass(const float* predictions);
    float getConfidence(const float* predictions, int class_idx);
    const char* getClassName(int class_idx);
    size_t getArenaSize() const;
    
    // Memory management
    void cleanup();
    bool isInitialized() const { return arena_buffer != nullptr; }
};

#endif // MANUAL_DSCNN_H