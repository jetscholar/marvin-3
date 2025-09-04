#include "ManualDSCNN.h"
#include "Logger.h"
#include <cmath>
#include <freertos/FreeRTOS.h> // Added for vTaskDelay and pdMS_TO_TICKS

ManualDSCNN::ManualDSCNN() {}

void ManualDSCNN::conv2D(const int8_t* input, const int8_t* weights, const int32_t* bias,
                         int8_t* output, int in_h, int in_w, int in_channels, int out_channels,
                         int kernel_h, int kernel_w, int stride_h, int stride_w) {
    int out_h = (in_h - kernel_h) / stride_h + 1;
    int out_w = (in_w - kernel_w) / stride_w + 1;
    for (int oc = 0; oc < out_channels; oc++) {
        for (int oh = 0; oh < out_h; oh++) {
            for (int ow = 0; ow < out_w; ow++) {
                int32_t sum = bias ? bias[oc] : 0;
                for (int ic = 0; ic < in_channels; ic++) {
                    for (int kh = 0; kh < kernel_h; kh++) {
                        for (int kw = 0; kw < kernel_w; kw++) {
                            int ih = oh * stride_h + kh;
                            int iw = ow * stride_w + kw;
                            if (ih < in_h && iw < in_w) {
                                sum += input[(ic * in_h + ih) * in_w + iw] * 
                                       weights[(oc * in_channels + ic) * kernel_h * kernel_w + kh * kernel_w + kw];
                            }
                        }
                    }
                }
                sum = sum > 0 ? sum : 0;
                output[(oc * out_h + oh) * out_w + ow] = (int8_t)(sum >> 8);
            }
        }
    }
}

void ManualDSCNN::depthwiseConv2D(const int8_t* input, const int8_t* weights, const int32_t* bias,
                                  int8_t* output, int in_h, int in_w, int channels,
                                  int kernel_h, int kernel_w, int stride_h, int stride_w) {
    int out_h = (in_h - kernel_h) / stride_h + 1;
    int out_w = (in_w - kernel_w) / stride_w + 1;
    for (int c = 0; c < channels; c++) {
        for (int oh = 0; oh < out_h; oh++) {
            for (int ow = 0; ow < out_w; ow++) {
                int32_t sum = bias ? bias[c] : 0;
                for (int kh = 0; kh < kernel_h; kh++) {
                    for (int kw = 0; kw < kernel_w; kw++) {
                        int ih = oh * stride_h + kh;
                        int iw = ow * stride_w + kw;
                        if (ih < in_h && iw < in_w) {
                            sum += input[(c * in_h + ih) * in_w + iw] * 
                                   weights[(c * kernel_h + kh) * kernel_w + kw];
                        }
                    }
                }
                sum = sum > 0 ? sum : 0;
                output[(c * out_h + oh) * out_w + ow] = (int8_t)(sum >> 8);
            }
        }
    }
}

void ManualDSCNN::globalAvgPool2D(const int8_t* input, int8_t* output, int in_h, int in_w, int channels) {
    for (int c = 0; c < channels; c++) {
        int32_t sum = 0;
        for (int h = 0; h < in_h; h++) {
            for (int w = 0; w < in_w; w++) {
                sum += input[(c * in_h + h) * in_w + w];
            }
        }
        output[c] = (int8_t)(sum / (in_h * in_w));
    }
}

void ManualDSCNN::dense(const int8_t* input, const int8_t* weights, const int32_t* bias,
                        float* output, int in_size, int out_size) {
    for (int o = 0; o < out_size; o++) {
        int32_t sum = bias ? bias[o] : 0;
        for (int i = 0; i < in_size; i++) {
            sum += input[i] * weights[o * in_size + i];
        }
        output[o] = (sum - input_zero_point) * input_scale;
    }
}

void ManualDSCNN::softmax(float* input, float* output, int size) {
    float sum = 0.0f;
    for (int i = 0; i < size; i++) {
        output[i] = expf(input[i]);
        sum += output[i];
    }
    for (int i = 0; i < size; i++) {
        output[i] /= sum;
    }
}

void ManualDSCNN::infer(const int8_t* input, float* output) {
    Logger::log(2, "Starting inference");
    // Input: [1, 65, 10, 1] (quantized INT8 MFCC)
    int8_t conv1_out[63 * 8 * 16];  // (65-3)/1 + 1 = 63, (10-0)/1 + 1 = 10, 16 channels
    conv2D(input, ds_cnn_tiny_v2_conv2d_Conv2D, ds_cnn_tiny_v2_batch_normalization_FusedBatchNormV3,
           conv1_out, 65, 10, 1, 16, 3, 3, 1, 1);
    vTaskDelay(pdMS_TO_TICKS(1)); // Yield to feed watchdog

    int8_t b1_dw_out[63 * 8 * 16];  // Same size after ReLU6
    depthwiseConv2D(conv1_out, ds_cnn_tiny_v2_b1_dw_depthwise,
                    ds_cnn_tiny_v2_batch_normalization_1_FusedBatchNormV3, b1_dw_out, 63, 8, 16, 3, 3, 1, 1);
    vTaskDelay(pdMS_TO_TICKS(1));

    int8_t b1_pw_out[63 * 8 * 24];  // Pointwise to 24 channels
    conv2D(b1_dw_out, ds_cnn_tiny_v2_b1_pw_Conv2D, ds_cnn_tiny_v2_batch_normalization_2_FusedBatchNormV3,
           b1_pw_out, 63, 8, 16, 24, 1, 1, 1, 1);
    vTaskDelay(pdMS_TO_TICKS(1));

    int8_t b2_dw_out[61 * 6 * 24];  // After AvgPool: (63-3)/1 + 1 = 61, (8-3)/1 + 1 = 6
    depthwiseConv2D(b1_pw_out, ds_cnn_tiny_v2_b2_dw_depthwise,
                    ds_cnn_tiny_v2_batch_normalization_3_FusedBatchNormV3, b2_dw_out, 63, 8, 24, 3, 3, 1, 1);
    vTaskDelay(pdMS_TO_TICKS(1));

    int8_t b2_pw_out[61 * 6 * 32];  // Pointwise to 32 channels
    conv2D(b2_dw_out, ds_cnn_tiny_v2_b2_pw_Conv2D, ds_cnn_tiny_v2_batch_normalization_4_FusedBatchNormV3,
           b2_pw_out, 61, 6, 24, 32, 1, 1, 1, 1);
    vTaskDelay(pdMS_TO_TICKS(1));

    int8_t b3_dw_out[59 * 4 * 32];  // After AvgPool: (61-3)/1 + 1 = 59, (6-3)/1 + 1 = 4
    depthwiseConv2D(b2_pw_out, ds_cnn_tiny_v2_b3_dw_depthwise,
                    ds_cnn_tiny_v2_batch_normalization_5_FusedBatchNormV3, b3_dw_out, 61, 6, 32, 3, 3, 1, 1);
    vTaskDelay(pdMS_TO_TICKS(1));

    int8_t b3_pw_out[59 * 4 * 48];  // Pointwise to 48 channels
    conv2D(b3_dw_out, ds_cnn_tiny_v2_b3_pw_Conv2D, ds_cnn_tiny_v2_batch_normalization_6_FusedBatchNormV3,
           b3_pw_out, 59, 4, 32, 48, 1, 1, 1, 1);
    vTaskDelay(pdMS_TO_TICKS(1));

    int8_t pool_out[17 * 10 * 48];  // After AvgPool: (59-3)/1 + 1 = 17, (4-0)/1 + 1 = 4 (corrected to 10 based on model)
    globalAvgPool2D(b3_pw_out, pool_out, 17, 10, 48);
    vTaskDelay(pdMS_TO_TICKS(1));

    int8_t global_pool[48];
    globalAvgPool2D(pool_out, global_pool, 17, 10, 48);
    vTaskDelay(pdMS_TO_TICKS(1));

    float dense_out[3];
    dense(global_pool, ds_cnn_tiny_v2_dense_MatMul, ds_cnn_tiny_v2_dense_BiasAdd_ReadVariableOp,
          dense_out, 48, 3);
    vTaskDelay(pdMS_TO_TICKS(1));

    softmax(dense_out, output, 3);
    Logger::log(2, "Inference completed: Marvin=%.3f, Unknown=%.3f, Silence=%.3f",
                output[0], output[1], output[2]);
}