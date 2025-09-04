#include "WakeWordDetector.h"
#include "Logger.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static void wakeWordTask(void* pvParameters) {
    WakeWordDetector* detector = static_cast<WakeWordDetector*>(pvParameters);
    Logger::log(2, "WakeWordTask started");
    AudioCapture capture;

    if (!capture.init()) {
        Logger::log(1, "Failed to initialize AudioCapture");
        vTaskDelete(NULL); // Delete task on failure
        return;
    }

    while (true) {
        Logger::log(2, "Attempting to read audio");
        if (!capture.read(detector->getAudioBuffer(), WINDOW_SIZE)) {
            Logger::log(1, "Failed to read audio buffer");
        } else {
            Logger::log(2, "Audio read successful, computing MFCC");
            int8_t mfcc[MFCC_NUM_FRAMES * MFCC_NUM_COEFFS];
            AudioProcessor::computeMFCC(detector->getAudioBuffer(), mfcc);

            Logger::log(2, "MFCC computed, starting inference");
            float probs[3];
            ManualDSCNN model;
            model.infer(mfcc, probs);

            Logger::log(2, "Inference completed, probs: %.3f, %.3f, %.3f", probs[0], probs[1], probs[2]);
            if (probs[0] > 0.5) {
                Logger::log(0, "Wake word 'Marvin' detected with probability %.3f", probs[0]);
            } else {
                Logger::log(2, "No wake word detected: Marvin=%.3f, Unknown=%.3f, Silence=%.3f",
                            probs[0], probs[1], probs[2]);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(100)); // Yield to watchdog every 100ms
    }
}

WakeWordDetector::WakeWordDetector() {
    audioBuffer = new int16_t[WINDOW_SIZE];
    if (audioBuffer == nullptr) {
        Logger::log(1, "Failed to allocate audioBuffer");
    } else {
        Logger::log(2, "AudioBuffer allocated successfully");
        xTaskCreate(wakeWordTask, "WakeWordTask", 12288, this, 5, NULL); // Increased to 12288 bytes
    }
}

WakeWordDetector::~WakeWordDetector() {
    delete[] audioBuffer;
}

void WakeWordDetector::run() {
    // Empty, as task handles the work
}