#ifndef WAKEWORDDETECTOR_H
#define WAKEWORDDETECTOR_H
#include "AudioCapture.h"
#include "AudioProcessor.h"
#include "ManualDSCNN.h"
#include "env.h"

class WakeWordDetector {
public:
    WakeWordDetector();
    ~WakeWordDetector();
    bool init();
    bool detect();
    void setThreshold(float threshold);
    float getThreshold() const;
    int getDetectionCount() const;
    void resetDetectionCount();
    bool isInitialized() const;
    AudioProcessor* getAudioProcessor() const { return audio_processor; }
    int16_t* getAudioBuffer() { return audio_buffer; }

private:
    AudioCapture* audio_capture;
    AudioProcessor* audio_processor;
    ManualDSCNN* dscnn;
    int16_t audio_buffer[MAX_AUDIO_BUFFER_SIZE];
    int detection_count;
    unsigned long last_detection_time;
    float confidence_threshold;
    void cleanup();
};

#endif