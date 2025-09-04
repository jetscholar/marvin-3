#include <Arduino.h>
#include "WakeWordDetector.h"
#include "Logger.h"

WakeWordDetector detector;

void setup() {
    Logger::init();
    Logger::log(0, "Starting Marvin-3");
}

void loop() {
    // Task handles the work, loop can be empty
    delay(1000); // Prevent Arduino loop from hogging CPU
}