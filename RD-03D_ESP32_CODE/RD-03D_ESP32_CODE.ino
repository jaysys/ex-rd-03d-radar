
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include "RadarSensor.h"
// #include "web_interface.h"

// WiFi credentials - Update these with your network details
// const char* ssid = "YOUR_WIFI_SSID";
// const char* password = "YOUR_WIFI_PASSWORD";

const char* ssid = "1234";
const char* password = "12345";

// Hardware pins
const int buzzerPin = 7;
const int ledPin = 8;    // Built-in LED
const int ledRPin = 21;   // Red LED
const int ledGPin = 22;   // Green LED
const int ledBPin = 23;   // Blue LED

// Radar sensor
RadarSensor radar(Serial1);

// Web server
WebServer server(80);

// System state variables
struct SystemConfig {
  bool armed = true;
  int detectionDistance = 1000;  // mm (1 meter default)
  int alarmDuration = 10000;     // ms (10 seconds default)
  bool alarmActive = false;
  unsigned long alarmStartTime = 0;
  bool systemEnabled = true;
};

SystemConfig config;
RadarTarget currentTarget;
bool targetDetected = false;
unsigned long lastBlinkTime = 0;
bool blinkState = false;

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial1.begin(256000, SERIAL_8N1, 2, 1); // RX=GPIO2, TX=GPIO1
  delay(1000);
  Serial.println("Radar Sensor Started");

  // Initialize pins
  pinMode(buzzerPin, OUTPUT);
  pinMode(ledPin, OUTPUT);
  pinMode(ledRPin, OUTPUT);
  pinMode(ledGPin, OUTPUT);
  pinMode(ledBPin, OUTPUT);
  
  // Initialize radar
  radar.begin();
  Serial.println("Radar Sensor Started");
  
  // Initialize WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("Connected! IP address: ");
  Serial.println(WiFi.localIP());
  
  // Setup web server routes
  setupWebServer();
  server.begin();
  
  // Initial LED state
  updateLEDs();
}

void loop() {
  server.handleClient();
  
  // Handle radar updates
  if (radar.update()) {
    currentTarget = radar.getTarget();
    targetDetected = (currentTarget.distance > 0 && 
                     currentTarget.distance <= config.detectionDistance);
    
    Serial.print("Distance: "); Serial.print(currentTarget.distance);
    Serial.print("mm, Angle: "); Serial.print(currentTarget.angle);
    Serial.print("°, Target: "); Serial.println(targetDetected ? "YES" : "NO");
  }
  
  // Handle alarm logic
  handleAlarm();
  
  // Update LEDs and buzzer
  updateLEDs();
  updateBuzzer();
  
  delay(50);
}

void handleAlarm() {
  if (!config.systemEnabled || !config.armed) {
    config.alarmActive = false;
    return;
  }
  
  if (targetDetected && !config.alarmActive) {
    // Start alarm
    config.alarmActive = true;
    config.alarmStartTime = millis();
    Serial.println("ALARM TRIGGERED!");
  }
  
  if (config.alarmActive) {
    // Check if alarm duration exceeded
    if (millis() - config.alarmStartTime >= config.alarmDuration) {
      config.alarmActive = false;
      Serial.println("Alarm timeout - stopping");
    }
  }
}

void updateLEDs() {
  if (!config.systemEnabled) {
    // System disabled - all LEDs off
    digitalWrite(ledRPin, LOW);
    digitalWrite(ledGPin, LOW);
    digitalWrite(ledBPin, LOW);
    return;
  }
  
  if (!config.armed) {
    // System disarmed - blue LED on
    digitalWrite(ledRPin, LOW);
    digitalWrite(ledGPin, LOW);
    digitalWrite(ledBPin, HIGH);
    return;
  }
  
  if (config.alarmActive) {
    // Alarm active - red LED blinks
    unsigned long currentTime = millis();
    if (currentTime - lastBlinkTime >= 250) {
      blinkState = !blinkState;
      lastBlinkTime = currentTime;
    }
    digitalWrite(ledRPin, blinkState ? HIGH : LOW);
    digitalWrite(ledGPin, LOW);
    digitalWrite(ledBPin, LOW);
  } else if (targetDetected) {
    // Target detected but not alarming - red LED solid
    digitalWrite(ledRPin, HIGH);
    digitalWrite(ledGPin, LOW);
    digitalWrite(ledBPin, LOW);
  } else {
    // Normal operation - green LED on
    digitalWrite(ledRPin, LOW);
    digitalWrite(ledGPin, HIGH);
    digitalWrite(ledBPin, LOW);
  }
}

void updateBuzzer() {
  if (config.alarmActive && config.systemEnabled) {
    // Buzzer blinks at same rate as red LED
    digitalWrite(buzzerPin, blinkState ? HIGH : LOW);
  } else {
    digitalWrite(buzzerPin, LOW);
  }
}

void setupWebServer() {
  // Serve main HTML page
  server.on("/", HTTP_GET, []() {
    server.send(200, "text/html", getMainHTML());
  });
  
  // API endpoints
  server.on("/api/status", HTTP_GET, handleGetStatus);
  server.on("/api/config", HTTP_POST, handleSetConfig);
  server.on("/api/config", HTTP_GET, handleGetConfig);
  server.on("/api/arm", HTTP_POST, handleArmDisarm);
  server.on("/api/stop-alarm", HTTP_POST, handleStopAlarm);
  server.on("/api/radar-data", HTTP_GET, handleGetRadarData);
  
  // Enable CORS
  server.enableCORS(true);
}

void handleGetStatus() {
  StaticJsonDocument<300> doc;
  doc["armed"] = config.armed;
  doc["alarmActive"] = config.alarmActive;
  doc["targetDetected"] = targetDetected;
  doc["systemEnabled"] = config.systemEnabled;
  doc["detectionDistance"] = config.detectionDistance;
  doc["alarmDuration"] = config.alarmDuration;
  doc["uptime"] = millis();
  
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

void handleGetConfig() {
  StaticJsonDocument<200> doc;
  doc["detectionDistance"] = config.detectionDistance;
  doc["alarmDuration"] = config.alarmDuration;
  doc["systemEnabled"] = config.systemEnabled;
  
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

void handleSetConfig() {
  if (server.hasArg("plain")) {
    StaticJsonDocument<200> doc;
    deserializeJson(doc, server.arg("plain"));
    
    if (doc.containsKey("detectionDistance")) {
      config.detectionDistance = doc["detectionDistance"];
    }
    if (doc.containsKey("alarmDuration")) {
      config.alarmDuration = doc["alarmDuration"];
    }
    if (doc.containsKey("systemEnabled")) {
      config.systemEnabled = doc["systemEnabled"];
    }
    
    server.send(200, "application/json", "{\"success\":true}");
  } else {
    server.send(400, "application/json", "{\"error\":\"Invalid request\"}");
  }
}

void handleArmDisarm() {
  if (server.hasArg("plain")) {
    StaticJsonDocument<100> doc;
    deserializeJson(doc, server.arg("plain"));
    
    if (doc.containsKey("armed")) {
      config.armed = doc["armed"];
      config.alarmActive = false; // Stop any active alarm
      server.send(200, "application/json", "{\"success\":true}");
    } else {
      server.send(400, "application/json", "{\"error\":\"Missing armed parameter\"}");
    }
  } else {
    server.send(400, "application/json", "{\"error\":\"Invalid request\"}");
  }
}

void handleStopAlarm() {
  config.alarmActive = false;
  server.send(200, "application/json", "{\"success\":true}");
}

void handleGetRadarData() {
  StaticJsonDocument<200> doc;
  doc["distance"] = currentTarget.distance;
  doc["angle"] = currentTarget.angle;
  doc["x"] = currentTarget.x;
  doc["y"] = currentTarget.y;
  doc["speed"] = currentTarget.speed;
  doc["detected"] = targetDetected;
  
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

String getMainHTML() {
  return R"rawstring(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Radar Security System</title>
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body { 
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            background: linear-gradient(135deg, #1e3c72, #2a5298);
            color: white;
            min-height: 100vh;
            padding: 20px;
        }
        .container { 
            max-width: 1200px; 
            margin: 0 auto;
            display: grid;
            grid-template-columns: 1fr 1fr;
            gap: 20px;
            height: calc(100vh - 40px);
        }
        .panel {
            background: rgba(255,255,255,0.1);
            backdrop-filter: blur(10px);
            border-radius: 15px;
            padding: 20px;
            border: 1px solid rgba(255,255,255,0.2);
        }
        .radar-panel {
            display: flex;
            flex-direction: column;
            align-items: center;
        }
        .status { 
            text-align: center; 
            margin-bottom: 20px;
        }
        .status h1 { 
            font-size: 2.5em; 
            margin-bottom: 10px;
            text-shadow: 2px 2px 4px rgba(0,0,0,0.3);
        }
        .status-indicator {
            display: inline-block;
            width: 20px;
            height: 20px;
            border-radius: 50%;
            margin-left: 10px;
            animation: pulse 2s infinite;
        }
        .armed { background: #4CAF50; }
        .disarmed { background: #FF9800; }
        .alarm { background: #F44336; animation: blink 0.5s infinite; }
        @keyframes pulse { 0%, 100% { opacity: 1; } 50% { opacity: 0.5; } }
        @keyframes blink { 0%, 100% { opacity: 1; } 50% { opacity: 0; } }
        
        .radar-display {
            width: 400px;
            height: 400px;
            position: relative;
            margin: 20px 0;
        }
        .radar-svg {
            width: 100%;
            height: 100%;
            background: radial-gradient(circle, rgba(0,255,0,0.1) 0%, rgba(0,100,0,0.05) 100%);
            border-radius: 50%;
            border: 2px solid #00ff00;
        }
        .controls {
            display: grid;
            grid-template-columns: 1fr 1fr;
            gap: 15px;
            margin-top: 20px;
        }
        .control-group {
            background: rgba(255,255,255,0.05);
            padding: 15px;
            border-radius: 10px;
        }
        .control-group label {
            display: block;
            margin-bottom: 5px;
            font-weight: bold;
        }
        .control-group input, .control-group select {
            width: 100%;
            padding: 8px;
            border: none;
            border-radius: 5px;
            background: rgba(255,255,255,0.1);
            color: white;
            margin-bottom: 10px;
        }
        .control-group input::placeholder {
            color: rgba(255,255,255,0.7);
        }
        .btn {
            padding: 12px 24px;
            border: none;
            border-radius: 8px;
            cursor: pointer;
            font-weight: bold;
            text-transform: uppercase;
            transition: all 0.3s ease;
            margin: 5px;
        }
        .btn-primary { background: #2196F3; color: white; }
        .btn-success { background: #4CAF50; color: white; }
        .btn-danger { background: #F44336; color: white; }
        .btn-warning { background: #FF9800; color: white; }
        .btn:hover { transform: translateY(-2px); box-shadow: 0 4px 12px rgba(0,0,0,0.3); }
        .info-grid {
            display: grid;
            grid-template-columns: 1fr 1fr;
            gap: 10px;
            margin-top: 20px;
        }
        .info-item {
            background: rgba(255,255,255,0.05);
            padding: 10px;
            border-radius: 8px;
            text-align: center;
        }
        .info-item .label { font-size: 0.9em; opacity: 0.8; }
        .info-item .value { font-size: 1.2em; font-weight: bold; margin-top: 5px; }
        @media (max-width: 768px) {
            .container { grid-template-columns: 1fr; }
            .radar-display { width: 300px; height: 300px; }
        }
    </style>
</head>
<body>
    <div class="container">
        <div class="panel">
            <div class="status">
                <h1>Radar Security <span id="statusIndicator" class="status-indicator armed"></span></h1>
                <p id="statusText">System Armed</p>
            </div>
            
            <div class="controls">
                <div class="control-group">
                    <label>Detection Distance (mm)</label>
                    <input type="number" id="detectionDistance" value="1000" min="100" max="8000">
                    <button class="btn btn-primary" onclick="updateConfig()">Update</button>
                </div>
                
                <div class="control-group">
                    <label>Alarm Duration (seconds)</label>
                    <input type="number" id="alarmDuration" value="10" min="1" max="300">
                    <button class="btn btn-primary" onclick="updateConfig()">Update</button>
                </div>
                
                <div class="control-group">
                    <label>System Control</label>
                    <button class="btn btn-success" id="armBtn" onclick="armSystem()">ARM</button>
                    <button class="btn btn-warning" id="disarmBtn" onclick="disarmSystem()">DISARM</button>
                </div>
                
                <div class="control-group">
                    <label>Alarm Control</label>
                    <button class="btn btn-danger" onclick="stopAlarm()">STOP ALARM</button>
                    <button class="btn btn-primary" onclick="toggleSystem()">ENABLE/DISABLE</button>
                </div>
            </div>
            
            <div class="info-grid">
                <div class="info-item">
                    <div class="label">Distance</div>
                    <div class="value" id="targetDistance">-- mm</div>
                </div>
                <div class="info-item">
                    <div class="label">Angle</div>
                    <div class="value" id="targetAngle">--deg</div>
                </div>
                <div class="info-item">
                    <div class="label">Speed</div>
                    <div class="value" id="targetSpeed">-- cm/s</div>
                </div>
                <div class="info-item">
                    <div class="label">Status</div>
                    <div class="value" id="detectionStatus">Clear</div>
                </div>
            </div>
        </div>
        
        <div class="panel radar-panel">
            <h2>Radar Display</h2>
            <div class="radar-display">
                <svg class="radar-svg" viewBox="0 0 400 400">
                    <!-- Radar grid -->
                    <defs>
                        <pattern id="radarGrid" width="40" height="40" patternUnits="userSpaceOnUse">
                            <path d="M 40 0 L 0 0 0 40" fill="none" stroke="rgba(0,255,0,0.2)" stroke-width="1"/>
                        </pattern>
                    </defs>
                    <rect width="400" height="400" fill="url(#radarGrid)"/>
                    
                    <!-- Distance circles -->
                    <circle cx="200" cy="400" r="50" fill="none" stroke="rgba(0,255,0,0.3)" stroke-width="1"/>
                    <circle cx="200" cy="400" r="100" fill="none" stroke="rgba(0,255,0,0.3)" stroke-width="1"/>
                    <circle cx="200" cy="400" r="150" fill="none" stroke="rgba(0,255,0,0.3)" stroke-width="1"/>
                    <circle cx="200" cy="400" r="200" fill="none" stroke="rgba(0,255,0,0.3)" stroke-width="1"/>
                    
                    <!-- 120-degree arc -->
                    <path d="M 27 273 A 200 200 0 0 1 373 273" fill="none" stroke="#00ff00" stroke-width="2"/>
                    
                    <!-- Center lines -->
                    <line x1="200" y1="400" x2="200" y2="200" stroke="rgba(0,255,0,0.5)" stroke-width="1"/>
                    <line x1="200" y1="400" x2="27" y2="273" stroke="rgba(0,255,0,0.5)" stroke-width="1"/>
                    <line x1="200" y1="400" x2="373" y2="273" stroke="rgba(0,255,0,0.5)" stroke-width="1"/>
                    
                    <!-- Target dot -->
                    <circle id="targetDot" cx="200" cy="400" r="0" fill="#ff0000" stroke="#ffffff" stroke-width="2" opacity="0">
                        <animate attributeName="r" values="5;8;5" dur="1s" repeatCount="indefinite"/>
                    </circle>
                    
                    <!-- Radar sweep -->
                    <line id="radarSweep" x1="200" y1="400" x2="200" y2="200" stroke="#00ff00" stroke-width="2" opacity="0.7">
                        <animateTransform attributeName="transform" attributeType="XML" type="rotate" 
                                        values="240 200 400;300 200 400;240 200 400" dur="3s" repeatCount="indefinite"/>
                    </line>
                </svg>
            </div>
            
            <div style="text-align: center; margin-top: 10px;">
                <small>Detection Range: 8m | Field of View: 120deg </small>
            </div>
        </div>
    </div>

    <script>
        let systemConfig = {
            armed: true,
            systemEnabled: true,
            detectionDistance: 1000,
            alarmDuration: 10000
        };
        
        function updateStatus() {
            fetch('/api/status')
                .then(response => response.json())
                .then(data => {
                    const indicator = document.getElementById('statusIndicator');
                    const statusText = document.getElementById('statusText');
                    
                    if (!data.systemEnabled) {
                        indicator.className = 'status-indicator disarmed';
                        statusText.textContent = 'System Disabled';
                    } else if (data.alarmActive) {
                        indicator.className = 'status-indicator alarm';
                        statusText.textContent = 'ALARM ACTIVE!';
                    } else if (data.armed) {
                        indicator.className = 'status-indicator armed';
                        statusText.textContent = 'System Armed';
                    } else {
                        indicator.className = 'status-indicator disarmed';
                        statusText.textContent = 'System Disarmed';
                    }
                    
                    systemConfig = data;
                });
        }
        
        function updateRadarData() {
            fetch('/api/radar-data')
                .then(response => response.json())
                .then(data => {
                    document.getElementById('targetDistance').textContent = data.distance + ' mm';
                    document.getElementById('targetAngle').textContent = data.angle + '°';
                    document.getElementById('targetSpeed').textContent = data.speed + ' cm/s';
                    document.getElementById('detectionStatus').textContent = data.detected ? 'TARGET' : 'Clear';
                    
                    updateRadarDisplay(data);
                });
        }
        
        function updateRadarDisplay(data) {
            const targetDot = document.getElementById('targetDot');
            
            if (data.detected && data.distance > 0) {
                // Convert distance to radar display coordinates
                const maxDistance = 8000; // 8 meters in mm
                const radarRadius = 200; // pixels
                const scale = radarRadius / maxDistance;
                
                // Convert angle and distance to x,y coordinates
                const angleRad = (data.angle) * Math.PI / 180;
                const distance = Math.min(data.distance, maxDistance);
                const x = 200 + (distance * scale * Math.sin(angleRad));
                const y = 400 - (distance * scale * Math.cos(angleRad));
                
                targetDot.setAttribute('cx', x);
                targetDot.setAttribute('cy', y);
                targetDot.style.opacity = '1';
            } else {
                targetDot.style.opacity = '0';
            }
        }
        
        function updateConfig() {
            const distance = document.getElementById('detectionDistance').value;
            const duration = document.getElementById('alarmDuration').value * 1000; // Convert to ms
            
            fetch('/api/config', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({
                    detectionDistance: parseInt(distance),
                    alarmDuration: parseInt(duration)
                })
            })
            .then(response => response.json())
            .then(data => {
                if (data.success) {
                    alert('Configuration updated successfully!');
                }
            });
        }
        
        function armSystem() {
            fetch('/api/arm', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ armed: true })
            });
        }
        
        function disarmSystem() {
            fetch('/api/arm', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ armed: false })
            });
        }
        
        function stopAlarm() {
            fetch('/api/stop-alarm', { method: 'POST' });
        }
        
        function toggleSystem() {
            fetch('/api/config', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ systemEnabled: !systemConfig.systemEnabled })
            });
        }
        
        // Update data every 500ms
        setInterval(() => {
            updateStatus();
            updateRadarData();
        }, 500);
        
        // Initial load
        updateStatus();
        updateRadarData();
    </script>
</body>
</html>
)rawstring";
}

