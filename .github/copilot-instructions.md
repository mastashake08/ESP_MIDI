# ESP MIDI Project - AI Coding Instructions

## Project Overview
This is a **PlatformIO-based embedded project** for building a **capacitive touch drum kit** that sends MIDI over USB to work with BandLab/WebMIDI. Built on the **Seeed XIAO ESP32-S3** board using the Arduino framework for ESP32.

## Architecture & Key Components

### Hardware Target
- **Board**: Seeed XIAO ESP32-S3 (compact ESP32-S3 development board)
- **Framework**: Arduino (ESP32 variant)
- **Platform**: Custom ESP32 platform from pioarduino repository

### Project Structure
- `src/main.cpp` - Main application entry point (setup/loop pattern)
- `platformio.ini` - Build configuration, board definition, and dependencies
- `lib/` - Project-specific custom libraries (currently empty, but structured for modular code)
- `include/` - Project header files
- `test/` - Test files (PlatformIO native testing)

## Development Workflows

### Building & Uploading
```bash
# Build the project
pio run

# Upload to connected board
pio run --target upload

# Build and upload in one command
pio run -t upload

# Monitor serial output
pio device monitor

# Clean build artifacts
pio run --target clean
```

### Key PlatformIO Commands
- Always use `pio` commands, not Arduino IDE
- The custom platform URL in `platformio.ini` is required for XIAO ESP32-S3 support
- Serial monitor baud rate defaults to 115200 (can be configured in platformio.ini)

## Project-Specific Conventions

### Arduino Framework Patterns
- Use standard Arduino `setup()` and `loop()` structure in [src/main.cpp](src/main.cpp)
- `setup()` runs once at boot for initialization
- `loop()` runs continuously for main logic

### ESP32-Specific Considerations
- This board has **dual-core ESP32-S3** - utilize FreeRTOS tasks if needed for parallel operations
- Built-in capacitive touch pins available (use `touchRead()` function)
- XIAO ESP32-S3 has **touch pins T1-T14** - typically use 8-10 pads for drum kit
- WiFi/BLE capabilities available but not currently utilized
- Use `#include <Arduino.h>` instead of platform-specific ESP-IDF headers unless low-level control needed

### Code Organization
- Keep hardware-specific logic modular - consider creating libraries in `lib/` for:
  - Touch sensor management with velocity detection
  - MIDI message generation/handling
  - Drum pad mapping and calibration
- Use `include/` for shared header files across modules
- Main application orchestration stays in `src/main.cpp`

## Drum Kit Implementation

### General MIDI Percussion Mapping (Channel 10)
- Use MIDI channel 10 (0-indexed: channel 9) for percussion
- Standard drum notes:
  - `36` (C1) - Bass Drum 1 / Kick
  - `38` (D1) - Snare
  - `42` (F#1) - Closed Hi-Hat
  - `46` (A#1) - Open Hi-Hat
  - `45` (A1) - Low Tom
  - `48` (C2) - Mid Tom
  - `50` (D2) - High Tom
  - `49` (C#2) - Crash Cymbal
  - `51` (D#2) - Ride Cymbal
  - `39` (D#1) - Hand Clap

### Touch Pad Configuration
- Map each capacitive touch pad to a drum sound
- Example 8-pad layout: Kick, Snare, Closed HH, Open HH, 3 Toms, Crash
- Store mappings in array: `const uint8_t drumNotes[8] = {36, 38, 42, 46, 45, 48, 50, 49};`

### Velocity Sensitivity
- ESP32 `touchRead()` returns values ~0-100+ (higher = less touch, lower = more touch)
- Convert touch strength to MIDI velocity (0-127):
  - Read baseline values when not touched
  - Calculate difference from baseline on touch
  - Map difference to velocity range (e.g., 40-127 for natural dynamics)
- Example: `velocity = map(touchStrength, minThreshold, maxThreshold, 40, 127);`

### Debouncing & Retriggering
- Drum pads send **Note On (0x99)** on channel 10 with velocity, **Note Off (0x89)** after short duration
- Typical pattern: `sendNoteOn(note, velocity, 10)` immediately followed by `sendNoteOff(note, 0, 10)` after 50-100ms
- Some drum machines use Note On with velocity 0 instead of Note Off
- WebMIDI expects proper MIDI timing - use `millis()` for tracking pad state
- Implement minimum retrigger time (~50ms) to prevent double-hits
- Track last hit time per pad: `unsigned long lastHitTime[8];`
- Only send Note On if `millis() - lastHitTime[pad] > 50`

## MIDI Implementation Notes

### Target: WebMIDI API Compatibility (BandLab)
- **Primary goal**: Device must appear as USB MIDI device for browser-based WebMIDI API
- WebMIDI works with USB MIDI class-compliant devices and BLE-MIDI
- BandLab requires either USB MIDI or BLE-MIDI connection

### USB MIDI Implementation (Recommended)
- Use **ESP32-S3 native USB** with TinyUSB stack for MIDI device class
- Add to `platformio.ini`:
  ```ini
  build_flags = 
    -DARDUINO_USB_MODE=1
    -DARDUINO_USB_CDC_ON_BOOT=0
  lib_deps = 
    adafruit/Adafruit TinyUSB Library@^3.0.0
  ```
- Include `<Adafruit_TinyUSB.h>` and `<MIDI.h>` in main.cpp
- Create USB MIDI device instance: `Adafruit_USBD_MIDI usb_midi;`
- Device will enumerate as "USB MIDI Device" visible to WebMIDI API

### Alternative: BLE-MIDI (Wireless)
- Use ESP32 BLE stack with BLE-MIDI profile
- Library: `ESP32-BLE-MIDI` by lathoub
- Requires pairing with computer's Bluetooth before WebMIDI can access
- Higher latency than USB but wireless convenience

##Drum pads send **Note On (0x99)** on channel 10 with velocity, **Note Off (0x89)** after short duration
- Typical pattern: `sendNoteOn(note, velocity, 10)` immediately followed by `sendNoteOff(note, 0, 10)` after 50-100ms
- Some drum machines use Note On with velocity 0 instead of Note Off
- WebMIDI expects proper MIDI timing - use `millis()` for tracking pad state
- Common messages for beat machine: Note On (0x90), Note Off (0x80), Control Change (0xB0)
- WebMIDI expects proper MIDI timing - consider using `millis()` for rhythm accuracy

## Dependencies Management
- Add libraries via `platformio.ini` under `lib_deps = ` section
- Prefer PlatformIO library registry over manual Arduino library installation
- Required for WebMIDI/BandLab connectivity:
  ```ini
  lib_deps = 
    adafruit/Adafruit TinyUSB Library@^3.0.0
    fortyseveneffects/MIDI Library@^5.0.2
  ```
- Optional: `lathoub/ESP32-BLE-MIDI` for wireless BLE-MIDI

## Testibedded testing, use Unity test framework (built into PlatformIO)
- **USB MIDI vs CDC**: When using USB MIDI, set `ARDUINO_USB_CDC_ON_BOOT=0` to prevent conflicts between MIDI and Serial
- **WebMIDI testing**: Use Chrome/Edge (not Safari/Firefox) for WebMIDI API - test at https://www.miditest.com/
- **BandLab permissions**: Browser needs MIDI device permissions granted - check chrome://settings/content/midi
- Hardware-in-the-loop testing requires physical board connection

## Common Pitfalls
- **Custom platform URL**: Don't modify the platform URL in platformio.ini - it's specifically for XIAO ESP32-S3 support
- **Touch sensitivity**: ESP32-S3 touch pins may need calibration - use `touchSetCycles()` and threshold tuning
- **USB MIDI vs CDC**: When using USB MIDI, set `ARDUINO_USB_CDC_ON_BOOT=0` to prevent conflicts between MIDI and Serial
- **WebMIDI testing**: Use Chrome/Edge (not Safari/Firefox) for WebMIDI API - test at https://www.miditest.com/
- **BandLab permissions**: Browser needs MIDI device permissions granted - check chrome://settings/content/midi
- **Memory constraints**: ESP32-S3 has 8MB flash but monitor heap usage for real-time MIDI

## Next Development Steps
The codebase is currently a template scaffold. Key areas to implement:
1. **USB MIDI device setup** with TinyUSB library - must be initialized in `setup()`
2. **Touch calibration routine** - read baseline values for each pad at startup, store thresholds
3. **Drum pad scanning** - poll touch sensors in `loop()`, detect threshold crossing
4. **Velocity calculation** - convert touch intensity to MIDI velocity (40-127 range)
5. **MIDI message generation** - Note On with velocity on channel 10, Note Off after duration
6. **Debounce logic** - prevent double-triggers with minimum retrigger time
7. **WebMIDI validation** - test device enumeration in browser console with `navigator.requestMIDIAccess()`
8. **User interface feedback** - LED indicators for touch events and MIDI activity
3. Beat pattern sequencer or trigger logic
4. User interface feedback (LEDs, serial output, or display)
