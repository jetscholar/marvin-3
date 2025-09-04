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
    void run();
    int16_t* getAudioBuffer() { return audioBuffer; } // Public getter

private:
    AudioCapture capture;
    ManualDSCNN model;
    int16_t* audioBuffer;
};

#endif