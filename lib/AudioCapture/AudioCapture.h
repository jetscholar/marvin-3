#ifndef AUDIOCAPTURE_H
#define AUDIOCAPTURE_H

#include <cstdint>
#include <cstddef>  // For size_t
#include "env.h"    // For SAMPLE_RATE and pin definitions

class AudioCapture {
public:
    AudioCapture();
    bool init();
    bool read(int16_t* buffer, size_t length);
};

#endif