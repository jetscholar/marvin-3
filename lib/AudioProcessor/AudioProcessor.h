#ifndef AUDIO_PROCESSOR_H
#define AUDIO_PROCESSOR_H

#include <cstdint>
#include "env.h"  // For MFCC_NUM_COEFFS and MFCC_NUM_FRAMES

class AudioProcessor {
public:
    static void computeMFCC(const int16_t* audio_samples, int8_t* mfcc_output);
    static void applyWindow(float* samples, int length);
    static void computeFFT(float* samples, float* output, int length);
    static const int16_t mfcc_coefficients[MFCC_NUM_COEFFS * MFCC_NUM_FRAMES];
};

#endif