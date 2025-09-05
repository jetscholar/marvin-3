#ifndef MANUALDSCNN_H
#define MANUALDSCNN_H
#include <cstdint>
#include "env.h"

class ManualDSCNN {
public:
    ManualDSCNN();
    ~ManualDSCNN();
    bool init();
    float predict(const int8_t* input);

private:
    bool initialized;
    float* weights; // Example weights
    void cleanup();
};

#endif