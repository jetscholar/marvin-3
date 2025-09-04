#ifndef MANUALDSCNN_H
#define MANUALDSCNN_H

#include <cstdint>
#include "frontend_params.h"
#include "model_weights.h"

class ManualDSCNN {
public:
    ManualDSCNN();
    void infer(const int8_t* input, float* output);

private:
    void conv2D(const int8_t* input, const int8_t* weights, const int32_t* bias,
                int8_t* output, int in_h, int in_w, int in_channels, int out_channels,
                int kernel_h, int kernel_w, int stride_h, int stride_w);
    void depthwiseConv2D(const int8_t* input, const int8_t* weights, const int32_t* bias,
                        int8_t* output, int in_h, int in_w, int channels,
                        int kernel_h, int kernel_w, int stride_h, int stride_w);
    void globalAvgPool2D(const int8_t* input, int8_t* output, int in_h, int in_w, int channels);
    void dense(const int8_t* input, const int8_t* weights, const int32_t* bias,
                float* output, int in_size, int out_size);
    void softmax(float* input, float* output, int size);
};

#endif