#include <Arduino.h>
#include "USB.h"
#include "USBMIDI.h"
#include <WiFi.h>
#include <WebServer.h>
#include <Update.h>

// ESP32's built-in USB MIDI
USBMIDI MIDI;

// WiFi AP Configuration
const char* ap_ssid = "DrumKit-OTA";
const char* ap_password = "drumkit123";

// Web server on port 80
WebServer server(80);

// Drum kit configuration
const int NUM_PADS = 8;

// GPIO pins on XIAO ESP32-S3 configured as digital inputs
const int touchPins[NUM_PADS] = {
    GPIO_NUM_1,   // D0
    GPIO_NUM_2,   // D1  
    GPIO_NUM_3,   // D2
    GPIO_NUM_4,   // D3
    GPIO_NUM_5,   // D4
    GPIO_NUM_6,   // D5
    GPIO_NUM_7,   // D6
    GPIO_NUM_8    // D7
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
    49   // Crash Cymbal
};

// Touch sensitivity and calibration
int touchBaseline[NUM_PADS];
int lastTouchValue[NUM_PADS] = {0};
const int TOUCH_THRESHOLD = 2;  // Extremely low threshold
const int MIN_VELOCITY = 40;
const int MAX_VELOCITY = 127;

// Debouncing
unsigned long lastHitTime[NUM_PADS] = {0};
const unsigned long RETRIGGER_TIME = 30; // Faster retriggering

// LED feedback
const int LED_PIN = 21;
unsigned long ledOffTime = 0;

// WiFi OTA mode flag
bool wifiEnabled = false;

// Diagnostic mode - prints touch values
bool diagnosticMode = true;
unsigned long lastDiagPrint = 0;

// OTA Update HTML page
const char* otaHTML = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Drum Kit OTA Update</title>
    <style>
        body {
            font-family: Arial, sans-serif;
            max-width: 600px;
            margin: 50px auto;
            padding: 20px;
            background: #f0f0f0;
        }
        .container {
            background: white;
            padding: 30px;
            border-radius: 10px;
            box-shadow: 0 2px 10px rgba(0,0,0,0.1);
        }
        h1 {
            color: #333;
            text-align: center;
        }
        .info {
            background: #e3f2fd;
            padding: 15px;
            border-radius: 5px;
            margin: 20px 0;
        }
        input[type="file"] {
            width: 100%;
            padding: 10px;
            margin: 10px 0;
            border: 2px dashed #ccc;
            border-radius: 5px;
            cursor: pointer;
        }
        input[type="submit"] {
            width: 100%;
            padding: 15px;
            background: #4CAF50;
            color: white;
            border: none;
            border-radius: 5px;
            font-size: 16px;
            cursor: pointer;
        }
        input[type="submit"]:hover {
            background: #45a049;
        }
        .progress {
            width: 100%;
            height: 30px;
            background: #f0f0f0;
            border-radius: 5px;
            overflow: hidden;
            display: none;
            margin: 20px 0;
        }
        .progress-bar {
            height: 100%;
            background: #4CAF50;
            width: 0%;
            transition: width 0.3s;
            text-align: center;
            line-height: 30px;
            color: white;
        }
        .status {
            text-align: center;
            margin: 20px 0;
            font-weight: bold;
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>🥁 Drum Kit OTA Update</h1>
        <div class="info">
            <strong>Device:</strong> Shake Drum Kit<br>
            <strong>WiFi:</strong> DrumKit-OTA<br>
            <strong>Instructions:</strong> Select a .bin firmware file and click Update
        </div>
        <form method="POST" action="/update" enctype="multipart/form-data" id="uploadForm">
            <input type="file" name="firmware" accept=".bin" required>
            <input type="submit" value="Update Firmware">
        </form>
        <div class="progress" id="progress">
            <div class="progress-bar" id="progressBar">0%</div>
        </div>
        <div class="status" id="status"></div>
    </div>
    <script>
        document.getElementById('uploadForm').addEventListener('submit', function(e) {
            e.preventDefault();
            var formData = new FormData(this);
            var xhr = new XMLHttpRequest();
            
            document.getElementById('progress').style.display = 'block';
            document.getElementById('status').textContent = 'Uploading...';
            
            xhr.upload.addEventListener('progress', function(e) {
                if (e.lengthComputable) {
                    var percent = (e.loaded / e.total) * 100;
                    document.getElementById('progressBar').style.width = percent + '%';
                    document.getElementById('progressBar').textContent = Math.round(percent) + '%';
                }
            });
            
            xhr.addEventListener('load', function() {
                if (xhr.status === 200) {
                    document.getElementById('status').textContent = 'Update successful! Device rebooting...';
                    document.getElementById('status').style.color = 'green';
                } else {
                    document.getElementById('status').textContent = 'Update failed: ' + xhr.responseText;
                    document.getElementById('status').style.color = 'red';
                }
            });
            
            xhr.addEventListener('error', function() {
                document.getElementById('status').textContent = 'Upload error occurred';
                document.getElementById('status').style.color = 'red';
            });
            
            xhr.open('POST', '/update');
            xhr.send(formData);
        });
    </script>
</body>
</html>
)rawliteral";

void handleRoot() {
    server.send(200, "text/html", otaHTML);
}

void handleUpdate() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
    delay(500);
    ESP.restart();
}

void handleUpload() {
    HTTPUpload& upload = server.upload();
    
    if (upload.status == UPLOAD_FILE_START) {
        digitalWrite(LED_PIN, HIGH);
        
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
            Update.printError(Serial);
        }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        static unsigned long lastBlink = 0;
        if (millis() - lastBlink > 100) {
            digitalWrite(LED_PIN, !digitalRead(LED_PIN));
            lastBlink = millis();
        }
        
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
            Update.printError(Serial);
        }
    } else if (upload.status == UPLOAD_FILE_END) {
        if (Update.end(true)) {
            digitalWrite(LED_PIN, HIGH);
        } else {
            Update.printError(Serial);
            digitalWrite(LED_PIN, LOW);
        }
    }
}

void setupWiFiAP() {
    // Disconnect from any previous WiFi connections
    WiFi.disconnect(true);
    delay(100);
    
    // Set WiFi mode to AP only
    WiFi.mode(WIFI_AP);
    delay(100);
    
    // Configure and start AP with stronger settings
    WiFi.softAP(ap_ssid, ap_password, 1, 0, 4);
    delay(500);
    
    // Verify AP started
    IPAddress IP = WiFi.softAPIP();
    
    // LED pattern: 5 rapid blinks = WiFi AP ready
    for (int i = 0; i < 5; i++) {
        digitalWrite(LED_PIN, HIGH);
        delay(100);
        digitalWrite(LED_PIN, LOW);
        delay(100);
    }
    
    // Setup web server routes
    server.on("/", HTTP_GET, handleRoot);
    server.on("/update", HTTP_POST, handleUpdate, handleUpload);
    
    server.begin();
    delay(100);
}

void initializeTouchSensors() {
    // Configure pins as digital inputs with internal pullup
    // Touch pin to GND to trigger
    for (int i = 0; i < NUM_PADS; i++) {
        pinMode(touchPins[i], INPUT_PULLUP);
        delay(5);
    }
    delay(100);
}

void calibrateTouchSensors() {
    // Quick LED blink during calibration
    digitalWrite(LED_PIN, HIGH);
    delay(1000);
    digitalWrite(LED_PIN, LOW);
    delay(500);
    
    for (int i = 0; i < NUM_PADS; i++) {
        int sum = 0;
        int validReadings = 0;
        
        // Take 20 readings for better baseline
        for (int j = 0; j < 20; j++) {
            int reading = touchRead(touchPins[i]);
            if (reading > 0 && reading < 300) {
                sum += reading;
                validReadings++;
            }
            delay(5);
        }
        
        if (validReadings > 5) {
            touchBaseline[i] = sum / validReadings;
        } else {
            // Default to a high value so any touch will trigger
            touchBaseline[i] = 80;
        }
    }
}

void sendDrumHit(int padIndex, int touchValue) {
    int touchStrength = touchBaseline[padIndex] - touchValue;
    
    if (touchStrength < TOUCH_THRESHOLD) {
        return;
    }
    
    // Map to MIDI velocity - more sensitive range
    int velocity = map(touchStrength, TOUCH_THRESHOLD, 50, MIN_VELOCITY, MAX_VELOCITY);
    velocity = constrain(velocity, MIN_VELOCITY, MAX_VELOCITY);
    
    MIDI.noteOn(drumNotes[padIndex], velocity, 10);
    
    // Visual feedback - blink pattern based on pad
    digitalWrite(LED_PIN, HIGH);
    ledOffTime = millis() + 100;
    
    delay(10);
    MIDI.noteOff(drumNotes[padIndex], 0, 10);
}

void scanTouchPads() {
    unsigned long currentTime = millis();
    
    // LED heartbeat every 500ms = system alive
    if (currentTime - lastDiagPrint > 500) {
        digitalWrite(LED_PIN, HIGH);
        delay(20);
        digitalWrite(LED_PIN, LOW);
        lastDiagPrint = currentTime;
    }
    
    for (int i = 0; i < NUM_PADS; i++) {
        if (currentTime - lastHitTime[i] < RETRIGGER_TIME) {
            continue;
        }
        
        // Read digital pin state (LOW = pressed/grounded)
        int pinState = digitalRead(touchPins[i]);
        
        // If pin is LOW (connected to ground or touched), trigger MIDI
        if (pinState == LOW) {
            // Send MIDI note
            MIDI.noteOn(drumNotes[i], 100, 10);
            digitalWrite(LED_PIN, HIGH);
            delay(10);
            MIDI.noteOff(drumNotes[i], 0, 10);
            delay(10);
            digitalWrite(LED_PIN, LOW);
            
            lastHitTime[i] = currentTime;
        }
    }
}

void setup() {
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);
    
    // Check if pad 0 is held down during boot for WiFi mode
    delay(500);
    int bootCheck = touchRead(touchPins[0]);
    
    // If pad 0 touched during boot, enable WiFi OTA mode
    if (bootCheck < 40) {
        wifiEnabled = true;
        // 10 rapid blinks = WiFi mode enabled
        for (int i = 0; i < 10; i++) {
            digitalWrite(LED_PIN, HIGH);
            delay(50);
            digitalWrite(LED_PIN, LOW);
            delay(50);
        }
        delay(500);
    } else {
        // 3 slow blinks = normal MIDI mode
        for (int i = 0; i < 3; i++) {
            digitalWrite(LED_PIN, HIGH);
            delay(200);
            digitalWrite(LED_PIN, LOW);
            delay(200);
        }
    }
    
    // Initialize USB MIDI FIRST
    USB.VID(0x2886);
    USB.PID(0x0080);
    USB.productName("Shake Drum Kit");
    USB.manufacturerName("Mastashake");
    USB.serialNumber("008");
    USB.begin();
    delay(500);
    
    MIDI.begin();
    delay(500);
    
    // Initialize pin modes
    initializeTouchSensors();
    
    // Long solid = ready
    digitalWrite(LED_PIN, HIGH);
    delay(1000);
    digitalWrite(LED_PIN, LOW);
}

void loop() {
    if (wifiEnabled) {
        server.handleClient();
    }
    
    scanTouchPads();
    delay(1);
}







