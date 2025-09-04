# Marvin-3: ESP32 Wake Word Detection System

[![PlatformIO](https://img.shields.io/badge/PlatformIO-ESP32-blue.svg)](https://platformio.org/)
[![Model Accuracy](https://img.shields.io/badge/Model%20Accuracy-99.69%25-brightgreen.svg)](https://github.com)
[![License](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

A high-performance wake word detection system for ESP32 that responds to the keyword **"marvin"** using a custom-trained deep learning model with 99.69% validation accuracy.

## 🚀 Features

- **🎯 High Accuracy**: 99.69% validation accuracy on trained dataset
- **⚡ Real-time Processing**: Sub-100ms inference on ESP32-D0WD-V3
- **💾 Memory Efficient**: Optimized for ESP32 without PSRAM (20KB footprint)
- **🎤 INMP441 Support**: High-quality I2S microphone interface
- **🔧 Pure C++**: No TensorFlow dependencies - manual CNN implementation
- **📊 MFCC Features**: 49×10 Mel-frequency cepstral coefficients
- **🛡️ Robust Detection**: Handles background noise and false positives

## 📋 Hardware Requirements

### **ESP32 Board**
- **Chip**: ESP32-D0WD-V3 (dual-core Xtensa LX6)
- **RAM**: 520KB SRAM (no PSRAM required)
- **Flash**: 4MB minimum
- **GPIO**: Available pins for I2S interface

### **Microphone Module**
- **Model**: INMP441 I2S MEMS Microphone
- **Interface**: I2S digital audio
- **Sample Rate**: 16kHz
- **Bit Depth**: 16-bit

### **Wiring Diagram**
```
ESP32-D0WD-V3    INMP441
─────────────    ───────
GPIO22     ───── SCK    (Serial Clock)
GPIO25     ───── WS     (Word Select)
GPIO26     ───── SD     (Serial Data)
GND        ───── GND
3.3V       ───── VDD
           ───── L/R    (GND for left channel)
```

## 🧠 Model Architecture

### **Deep CNN Structure**
- **Input**: 49×10×1 MFCC features (1.96KB)
- **Conv2D**: 64 filters, 10×1 kernel, stride 2×1
- **BatchNorm**: Channel-wise normalization
- **ReLU**: Non-linear activation
- **AvgPool2D**: 4×4 pooling for dimensionality reduction
- **Dense**: Fully connected layer
- **Softmax**: 3-class output (marvin, unknown, silence)

### **Training Results**
```
Final Model Performance:
├── Validation Accuracy: 99.69%
├── Training Accuracy: 99.54% 
├── Model Size: 17.17KB (4,395 parameters)
├── Inference Time: ~50ms on ESP32
└── Memory Usage: 20KB arena
```

## 🛠️ Quick Start

### **1. Clone Repository**
```bash
git clone https://github.com/your-username/marvin-3.git
cd marvin-3
```

### **2. Configure Environment**
Create `include/env.h`:
```cpp
#pragma once

// WiFi Configuration (optional)
#define WIFI_SSID "your-wifi-network"
#define WIFI_PASSWORD "your-wifi-password"

// Debug Settings
#define DEBUG_LEVEL 2          // 0=Off, 1=Error, 2=Info, 3=Verbose
#define ENABLE_SERIAL_PLOT 0   // Enable for Arduino Serial Plotter

// Wake Word Settings
#define WAKE_WORD_THRESHOLD 0.75f  // Confidence threshold
#define SAMPLE_RATE 16000          // Audio sample rate
#define FRAME_DURATION_MS 30       // MFCC frame duration
```

### **3. Build and Upload**
```bash
pio run -t upload -t monitor
```

### **4. Test Wake Word**
- Say "marvin" clearly within 1-2 meters
- Monitor serial output for detection confidence
- LED should activate on successful detection

## 📊 Performance Metrics

### **ESP32-D0WD-V3 Benchmarks**
| Metric | Value |
|--------|--------|
| Inference Time | ~50ms |
| Memory Usage | 20KB arena + 320KB free heap |
| CPU Usage | ~15% during inference |
| Power Consumption | ~80mA @ 3.3V |
| Detection Range | 1-3 meters |
| False Positive Rate | <1% |

### **Model Statistics**
| Parameter | Value |
|-----------|--------|
| Total Parameters | 4,395 |
| Model Size | 17.17KB |
| Input Dimensions | 49×10×1 |
| Output Classes | 3 |
| Quantization | Float32 |

## 🔧 Configuration Options

### **Audio Processing**
```cpp
// In AudioProcessor.h
#define MFCC_NUM_COEFFS 10     // MFCC coefficients per frame
#define MFCC_NUM_FRAMES 49     // Time frames per inference
#define WINDOW_SIZE 512        // FFT window size
#define HOP_LENGTH 160         // Frame hop length
```

### **Wake Word Detection**
```cpp
// In WakeWordDetector.h
#define CONFIDENCE_THRESHOLD 0.75f    // Detection threshold
#define SMOOTHING_WINDOW 3           // Temporal smoothing
#define COOLDOWN_MS 2000            // Post-detection cooldown
```

## 📁 Project Structure

```
marvin-3/
├── src/main.cpp              # Main application
├── lib/ManualDSCNN/          # CNN inference engine
├── lib/AudioCapture/         # I2S microphone interface
├── lib/AudioProcessor/       # MFCC feature extraction
├── lib/WakeWordDetector/     # Main detection coordinator
└── include/env.h            # Configuration (sensitive data)
```

## 🧪 Testing

### **Unit Tests**
```bash
pio test -e test
```

### **Audio Validation**
```bash
python tools/audio_validator.py
```

### **Performance Profiling**
```bash
pio run -e profile -t upload -t monitor
```

## 🚨 Troubleshooting

### **Common Issues**

**Low Detection Accuracy**
- Check microphone wiring and orientation
- Verify sample rate (16kHz)
- Adjust `WAKE_WORD_THRESHOLD` in `env.h`

**Memory Crashes**
- Your ESP32-D0WD-V3 has no PSRAM
- Default arena size is 20KB (conservative)
- Monitor heap usage with `ESP.getFreeHeap()`

**I2S Audio Issues**
- Verify GPIO pin connections
- Check power supply (3.3V stable)
- Enable I2S debug logging

### **Debug Commands**
```cpp
// In serial monitor
help           // Show available commands
stats          // Memory and performance stats
threshold 0.8  // Adjust detection threshold
calibrate      // Background noise calibration
```

## 📈 Optimization Tips

### **For Better Accuracy**
1. **Calibrate Background Noise**: Run calibration in your target environment
2. **Adjust Threshold**: Lower for more sensitive, higher for fewer false positives
3. **Microphone Positioning**: 1-2 meters, avoid obstacles
4. **Train Custom Model**: Use your voice samples for personalization

### **For Better Performance**
1. **Reduce Arena Size**: Try 15KB if memory is tight
2. **Optimize Sample Rate**: 8kHz works but reduces accuracy
3. **Enable Dual Core**: Use Core 1 for audio, Core 0 for inference
4. **Flash Optimization**: Use fastest SPI mode in `platformio.ini`

## 🔄 Model Updates

To update with a newly trained model:

1. **Export Weights**:
```python
python tools/model_converter.py your_model.h5
```

2. **Replace Files**:
```bash
cp model_weights.h lib/ManualDSCNN/
```

3. **Rebuild**:
```bash
pio run -t upload
```

## 📚 Documentation

- [Hardware Setup Guide](docs/HARDWARE.md)
- [Model Training Documentation](docs/MODEL_TRAINING.md)
- [Performance Benchmarks](docs/PERFORMANCE.md)
- [Troubleshooting Guide](docs/TROUBLESHOOTING.md)

## 🤝 Contributing

1. Fork the repository
2. Create feature branch (`git checkout -b feature/amazing-feature`)
3. Commit changes (`git commit -m 'Add amazing feature'`)
4. Push to branch (`git push origin feature/amazing-feature`)
5. Open Pull Request

## 📄 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## 🙏 Acknowledgments

- **TensorFlow Team**: For the original model architecture inspiration
- **ESP32 Community**: For excellent documentation and examples
- **Speech Recognition Research**: For MFCC feature extraction techniques

---

**Built with ❤️ for ESP32-D0WD-V3 | Wake word detection without the bloat**