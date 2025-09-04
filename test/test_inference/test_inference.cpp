#include <unity.h>
#include "ManualDSCNN.h"
#include "frontend_params.h"

void test_inference_shape() {
    int8_t input[KWS_FRAMES * KWS_NUM_MFCC] = {0};
    float output[KWS_NUM_CLASSES];
    ManualDSCNN::infer(input, output);
    for (int i = 0; i < KWS_NUM_CLASSES; i++) {
        TEST_ASSERT_FLOAT_WITHIN(1.0f, 0.0f, output[i]);  // Placeholder
    }
}

void setup() {
    UNITY_BEGIN();
    RUN_TEST(test_inference_shape);
    UNITY_END();
}

void loop() {}