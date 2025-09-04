# Marvin-3 Project Structure

```
marvin-3/
├── platformio.ini                 # PlatformIO configuration
├── README.md                      # Project documentation
├── .gitignore                     # Git ignore file
│
├── include/
│   └── env.h                      # Sensitive data (WiFi, API keys, etc.)
│
├── src/
│   └── main.cpp                   # Main application entry point
│
├── lib/
│   ├── AudioCapture/
│   │   ├── AudioCapture.h         # I2S microphone interface
│   │   └── AudioCapture.cpp
│   │
│   ├── AudioProcessor/
│   │   ├── AudioProcessor.h       # MFCC feature extraction
│   │   ├── AudioProcessor.cpp
│   │   └── mfcc_coefficients.h    # Pre-computed MFCC tables
│   │
│   ├── ManualDSCNN/
│   │   ├── ManualDSCNN.h          # Manual CNN inference engine
│   │   ├── ManualDSCNN.cpp
│   │   └── model_weights.h        # Exported TensorFlow weights
│   │
│   ├── WakeWordDetector/
│   │   ├── WakeWordDetector.h     # Main KWS coordinator
│   │   └── WakeWordDetector.cpp
│   │
│   └── Utils/
│       ├── RingBuffer.h           # Circular audio buffer
│       ├── RingBuffer.cpp
│       ├── Logger.h               # Debug logging utilities
│       └── Logger.cpp
│
├── test/
│   ├── test_audio_capture/
│   │   └── test_audio.cpp         # Unit tests for audio capture
│   ├── test_mfcc/
│   │   └── test_mfcc.cpp          # Unit tests for MFCC extraction
│   └── test_inference/
│       └── test_inference.cpp     # Unit tests for CNN inference
│
├── tools/
│   ├── model_converter.py         # Convert TF model to C arrays
│   ├── audio_validator.py         # Validate audio input/output
│   └── performance_profiler.py   # Profile inference timing
│
├── docs/
│   ├── HARDWARE.md               # Hardware setup guide
│   ├── MODEL_TRAINING.md         # Model training documentation
│   ├── PERFORMANCE.md            # Benchmarks and optimization
│   └── TROUBLESHOOTING.md        # Common issues and solutions
│
└── data/
    ├── calibration/
    │   └── noise_samples.wav     # Background noise for calibration
    └── test_samples/
        ├── marvin_test.wav       # Test audio samples
        ├── unknown_test.wav
        └── silence_test.wav
```

## **Directory Descriptions**

### **Core Directories**
- **`src/`** - Main application code
- **`lib/`** - Modular libraries for each component
- **`include/`** - Configuration headers (sensitive data)

### **Library Modules**
- **`AudioCapture/`** - I2S microphone interface (INMP441)
- **`AudioProcessor/`** - MFCC feature extraction pipeline
- **`ManualDSCNN/`** - Manual CNN inference (no TensorFlow deps)
- **`WakeWordDetector/`** - High-level wake word detection coordinator
- **`Utils/`** - Supporting utilities (buffers, logging)

### **Development Support**
- **`test/`** - Unit tests for each component
- **`tools/`** - Python utilities for model conversion
- **`docs/`** - Comprehensive documentation
- **`data/`** - Test samples and calibration data

## **Key Files**

### **Configuration**
- `platformio.ini` - ESP32-D0WD-V3 specific settings
- `include/env.h` - WiFi credentials, debug settings

### **Model Files**
- `lib/ManualDSCNN/model_weights.h` - Your trained model (99.69% accuracy)
- `tools/model_converter.py` - Converts your .h5 model to C arrays

### **Hardware Interface**
- `lib/AudioCapture/` - INMP441 I2S microphone driver
- GPIO definitions for your specific ESP32 wiring

## **Build Targets**
- `debug` - Full logging, slower inference
- `release` - Optimized for production
- `test` - Unit test runner
- `profile` - Performance measurement build