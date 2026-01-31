#include <Arduino.h>
#include <Adafruit_TinyUSB.h>
#include <MIDI.h>

// USB MIDI device
Adafruit_USBD_MIDI usb_midi;
MIDI_CREATE_INSTANCE(Adafruit_USBD_MIDI, usb_midi, MIDI);

// Drum kit configuration
const int NUM_PADS = 8;

// Touch pins for ESP32-S3 (adjust based on your wiring)
const int touchPins[NUM_PADS] = {T1, T2, T3, T4, T5, T6, T7, T8}; // GPIO 1-8

// General MIDI drum notes (Channel 10 percussion)
const uint8_t drumNotes[NUM_PADS] = {
    36,  // Kick (Bass Drum 1)
    38,  // Snare
    42,  // Closed Hi-Hat
    46,  // Open Hi-Hat
    45,  // Low Tom
    48,  // Mid Tom
    50,  // High Tom
    49   // Crash Cymbal
};

// Touch sensitivity and calibration
int touchBaseline[NUM_PADS];
const int TOUCH_THRESHOLD = 15;  // Decrease from baseline to trigger
const int MIN_VELOCITY = 40;     // Minimum MIDI velocity
const int MAX_VELOCITY = 127;    // Maximum MIDI velocity

// Debouncing
unsigned long lastHitTime[NUM_PADS] = {0};
const unsigned long RETRIGGER_TIME = 50; // ms

// LED feedback (built-in LED)
const int LED_PIN = LED_BUILTIN;
unsigned long ledOffTime = 0;

void calibrateTouchSensors() {
    Serial.println("Calibrating touch sensors...");
    Serial.println("Please don't touch the pads during calibration.");
    
    delay(1000);
    
    // Read baseline values (not touched)
    for (int i = 0; i < NUM_PADS; i++) {
        int sum = 0;
        for (int j = 0; j < 10; j++) {
            sum += touchRead(touchPins[i]);
            delay(10);
        }
        touchBaseline[i] = sum / 10;
        Serial.print("Pad ");
        Serial.print(i);
        Serial.print(" (Note ");
        Serial.print(drumNotes[i]);
        Serial.print("): Baseline = ");
        Serial.println(touchBaseline[i]);
    }
    
    Serial.println("Calibration complete! Ready to play.");
}

void sendDrumHit(int padIndex, int touchValue) {
    // Calculate touch strength (how much lower than baseline)
    int touchStrength = touchBaseline[padIndex] - touchValue;
    
    if (touchStrength < TOUCH_THRESHOLD) {
        return; // Not a strong enough touch
    }
    
    // Map touch strength to MIDI velocity (40-127 range)
    int velocity = map(touchStrength, TOUCH_THRESHOLD, touchBaseline[padIndex] / 2, 
                       MIN_VELOCITY, MAX_VELOCITY);
    velocity = constrain(velocity, MIN_VELOCITY, MAX_VELOCITY);
    
    // Send MIDI Note On (Channel 10 for drums, 0-indexed as channel 9)
    MIDI.sendNoteOn(drumNotes[padIndex], velocity, 10);
    
    // Visual feedback
    digitalWrite(LED_PIN, HIGH);
    ledOffTime = millis() + 50;
    
    // Debug output
    Serial.print("Pad ");
    Serial.print(padIndex);
    Serial.print(" (Note ");
    Serial.print(drumNotes[padIndex]);
    Serial.print("): Velocity ");
    Serial.println(velocity);
    
    // Send Note Off after a short duration
    delay(10);
    MIDI.sendNoteOff(drumNotes[padIndex], 0, 10);
}

void scanTouchPads() {
    unsigned long currentTime = millis();
    
    for (int i = 0; i < NUM_PADS; i++) {
        // Check if enough time has passed since last hit (debouncing)
        if (currentTime - lastHitTime[i] < RETRIGGER_TIME) {
            continue;
        }
        
        // Read touch value
        int touchValue = touchRead(touchPins[i]);
        
        // Check if pad is touched
        if (touchBaseline[i] - touchValue > TOUCH_THRESHOLD) {
            sendDrumHit(i, touchValue);
            lastHitTime[i] = currentTime;
        }
    }
    
    // Turn off LED after timeout
    if (ledOffTime > 0 && currentTime >= ledOffTime) {
        digitalWrite(LED_PIN, LOW);
        ledOffTime = 0;
    }
}

void setup() {
    // Initialize serial for debugging (note: USB MIDI mode limits Serial availability)
    Serial.begin(115200);
    delay(1000); // Wait for serial connection
    
    Serial.println("ESP32 MIDI Drum Kit Initializing...");
    
    // Initialize LED
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);
    
    // Initialize USB MIDI
    #ifdef USE_TINYUSB
    TinyUSBDevice.setManufacturerDescriptor("ESP32 Drums");
    TinyUSBDevice.setProductDescriptor("Capacitive Touch Drum Kit");
    #endif
    
    MIDI.begin(MIDI_CHANNEL_OMNI);
    
    Serial.println("USB MIDI initialized");
    
    // Flash LED to indicate startup
    for (int i = 0; i < 3; i++) {
        digitalWrite(LED_PIN, HIGH);
        delay(100);
        digitalWrite(LED_PIN, LOW);
        delay(100);
    }
    
    // Calibrate touch sensors
    calibrateTouchSensors();
    
    Serial.println("Ready! Touch pads to play drums.");
    Serial.println("Connect to BandLab via WebMIDI in Chrome/Edge.");
}

void loop() {
    // Scan touch pads for hits
    scanTouchPads();
    
    // Process any incoming MIDI (for future features like configuration)
    MIDI.read();
    
    // Small delay to prevent overwhelming the system
    delay(1);
}
