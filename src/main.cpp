#include <Arduino.h>
#include "USB.h"
#include "USBMIDI.h"

// ESP32's built-in USB MIDI
USBMIDI MIDI;

// Drum kit configuration
const int NUM_PADS = 10;  // Increase from 8 to 10 (or more)

// Touch-capable GPIO pins on XIAO ESP32-S3
const int touchPins[NUM_PADS] = {
    GPIO_NUM_1,   // D0 / T1
    GPIO_NUM_2,   // D1 / T2  
    GPIO_NUM_3,   // D2 / T3
    GPIO_NUM_4,   // D3 / T4
    GPIO_NUM_5,   // D4 / T5
    GPIO_NUM_6,   // D5 / T6
    GPIO_NUM_7,   // D6 / T7
    GPIO_NUM_8,   // D7 / T8
    GPIO_NUM_9,   // D8 / T9
    GPIO_NUM_10   // D9 / T10
};

// General MIDI drum notes (Channel 10 percussion)
const uint8_t drumNotes[NUM_PADS] = {
    36,  // Kick (Bass Drum 1)
    38,  // Snare
    42,  // Closed Hi-Hat
    46,  // Open Hi-Hat
    45,  // Low Tom
    48,  // Mid Tom
    50,  // High Tom
    49,  // Crash Cymbal
    51,  // Ride Cymbal - NEW
    37   // Side Stick - NEW
};

// Touch sensitivity and calibration
int touchBaseline[NUM_PADS];
const int TOUCH_THRESHOLD = 15;  // Decrease from baseline to trigger
const int MIN_VELOCITY = 40;     // Minimum MIDI velocity
const int MAX_VELOCITY = 127;    // Maximum MIDI velocity

// Debouncing
unsigned long lastHitTime[NUM_PADS] = {0};
const unsigned long RETRIGGER_TIME = 50; // ms

// LED feedback
const int LED_PIN = 21;
unsigned long ledOffTime = 0;

void calibrateTouchSensors() {
    Serial.println("Calibrating touch sensors...");
    Serial.println("Don't touch the pads for 2 seconds...");
    delay(2000);
    
    // Read baseline values (not touched)
    for (int i = 0; i < NUM_PADS; i++) {
        int sum = 0;
        int validReadings = 0;
        
        for (int j = 0; j < 10; j++) {
            int reading = touchRead(touchPins[i]);
            if (reading > 0 && reading < 200) {
                sum += reading;
                validReadings++;
            }
            delay(10);
        }
        
        if (validReadings > 0) {
            touchBaseline[i] = sum / validReadings;
        } else {
            touchBaseline[i] = 50; // Default if no valid readings
        }
        
        Serial.print("Pad ");
        Serial.print(i);
        Serial.print(" (Note ");
        Serial.print(drumNotes[i]);
        Serial.print("): Baseline = ");
        Serial.println(touchBaseline[i]);
    }
    
    Serial.println("Calibration complete! Touch pads to play drums.");
}

void sendDrumHit(int padIndex, int touchValue) {
    // Calculate touch strength
    int touchStrength = touchBaseline[padIndex] - touchValue;
    
    if (touchStrength < TOUCH_THRESHOLD) {
        return;
    }
    
    // Map to MIDI velocity (40-127)
    int velocity = map(touchStrength, TOUCH_THRESHOLD, touchBaseline[padIndex] / 2, 
                       MIN_VELOCITY, MAX_VELOCITY);
    velocity = constrain(velocity, MIN_VELOCITY, MAX_VELOCITY);
    
    // Send MIDI Note On
    MIDI.noteOn(drumNotes[padIndex], velocity, 10);
    
    // Visual feedback
    digitalWrite(LED_PIN, HIGH);
    ledOffTime = millis() + 50;
    
    // Debug
    Serial.print("Hit Pad ");
    Serial.print(padIndex);
    Serial.print(" (Note ");
    Serial.print(drumNotes[padIndex]);
    Serial.print("): Velocity ");
    Serial.println(velocity);
    
    // Send Note Off
    delay(10);
    MIDI.noteOff(drumNotes[padIndex], 0, 10);
}

void scanTouchPads() {
    unsigned long currentTime = millis();
    
    for (int i = 0; i < NUM_PADS; i++) {
        // Debouncing
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
    Serial.begin(115200);
    delay(1000);
    Serial.println("ESP32 USB MIDI Drum Kit Starting...");
    
    // Initialize LED
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);
    
    // Flash LED 3 times
    for (int i = 0; i < 3; i++) {
        digitalWrite(LED_PIN, HIGH);
        delay(200);
        digitalWrite(LED_PIN, LOW);
        delay(200);
    }
    
    // Initialize USB MIDI
    MIDI.begin();
    USB.begin();
    
    Serial.println("USB MIDI initialized");
    
    // Calibrate touch sensors
    calibrateTouchSensors();
    
    // Long flash to indicate ready
    digitalWrite(LED_PIN, HIGH);
    delay(500);
    digitalWrite(LED_PIN, LOW);
    
    Serial.println("Ready to play!");
}

void loop() {
    // Scan touch pads for hits
    scanTouchPads();
    
    delay(1);
}







