#ifndef AUDIOCAPTURE_H
#define AUDIOCAPTURE_H

#include <driver/i2s.h>
#include "env.h" // For SAMPLE_RATE, I2S pins

class AudioCapture {
public:
    AudioCapture();
    ~AudioCapture(); // Declare destructor

    bool init();
    bool read(int16_t* buffer, size_t samples);

private:
    bool is_initialized; // Declare member variable
};

#endif