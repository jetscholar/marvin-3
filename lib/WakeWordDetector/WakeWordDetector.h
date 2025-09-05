#ifndef WAKEWORDDETECTOR_H
#define WAKEWORDDETECTOR_H

#include "AudioCapture.h"
#include "AudioProcessor.h"
#include "ManualDSCNN.h"

// Constants
#define WAKE_WORD_THRESHOLD 0.8f
#define MAX_AUDIO_BUFFER_SIZE 16000
#define DETECTION_COOLDOWN_MS 2000

class WakeWordDetector {
private:
    // Component pointers
    AudioCapture* audio_capture;
    AudioProcessor* audio_processor;
    ManualDSCNN* dscnn;
    
    // Detection state
    int detection_count;
    unsigned long last_detection_time;
    float confidence_threshold;
    
    // Audio buffer
    int16_t audio_buffer[MAX_AUDIO_BUFFER_SIZE];
    
public:
    // Constructor and destructor
    WakeWordDetector();
    ~WakeWordDetector();
    
    // Main methods
    bool init();
    bool detect();
    
    // Configuration
    void setThreshold(float threshold);
    float getThreshold() const;
    
    // Statistics
    int getDetectionCount() const;
    void resetDetectionCount();
    
    // Utility
    bool isInitialized() const;
    void cleanup();
};

#endif // WAKEWORDDETECTOR_H