#include <WiFi.h>
#include <WebServer.h>
#include <ESP32Servo.h>

// ==========================================
// PIN CONFIGURATIONS
// ==========================================
const int M1_IN1 = 25;
const int M1_IN2 = 26;
const int M1_IN3 = 27;
const int M1_IN4 = 14;

const int M2_IN1 = 21;
const int M2_IN2 = 22;

const int SERVO_PIN = 33;
const int TRIG_FRONT = 18;
const int ECHO_FRONT = 19;
const int TRIG_REAR = 16;
const int ECHO_REAR = 17;
const int IR_PIN = 32;
const int BUZZER_PIN = 13;

// ==========================================
// CONSTANTS & GLOBALS
// ==========================================
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

WebServer server(80);
Servo frontServo;

bool turboMode = false;
bool autoMode = false;
bool yoloPersonDetected = false;
bool emergencyStop = false;
bool wasConnected = true;

long frontDistance = 0;
long rearDistance = 0;
bool irObstacle = false;

const int OBSTACLE_THRESHOLD = 25; 
unsigned long lastSensorRead = 0;
const unsigned long sensorInterval = 200; 
unsigned long lastWifiCheck = 0;

// ==========================================
// PROGMEM HTML (Zero Heap Fragmentation)
// ==========================================
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta name='viewport' content='width=device-width, initial-scale=1.0'>
  <style>
    body{font-family:Arial; text-align:center; background:#222; color:#fff;}
    .btn{width:80px; height:50px; margin:5px; font-size:16px; background:#444; color:#fff; border:none; border-radius:5px; cursor:pointer;}
    .btn:active{background:orange;} 
    .grid{display:inline-grid; grid-template-columns: auto auto auto;}
    .panel{text-align:left; display:inline-block; background:#333; padding:15px; border-radius:8px;}
  </style>
  <script>
    // Fetch data asynchronously without reloading the page
    setInterval(() => {
      fetch('/status').then(res => res.json()).then(data => {
        document.getElementById('fd').innerText = data.fd + " cm";
        document.getElementById('rd').innerText = data.rd + " cm";
        document.getElementById('ir').innerText = data.ir ? "BLOCKED" : "CLEAR";
        document.getElementById('tm').innerText = data.tm ? "ON" : "OFF";
        document.getElementById('am').innerText = data.am ? "ON" : "OFF";
        document.getElementById('yd').innerText = data.yd ? "PERSON DETECTED" : "CLEAR";
        let es = document.getElementById('es');
        es.innerText = data.es ? "TRIGGERED" : "NORMAL";
        es.style.color = data.es ? "red" : "lightgreen";
      }).catch(err => console.log("Connection lost"));
    }, 1000);

    function sendCmd(cmd) { fetch('/control?cmd=' + cmd); }
  </script>
</head>
<body>
  <h1>Surveillance Rover</h1>
  <div class='panel'>
    <p>Front Distance: <span id='fd'>-- cm</span></p>
    <p>Rear Distance: <span id='rd'>-- cm</span></p>
    <p>IR Sensor: <span id='ir'>--</span></p>
    <p>Turbo Mode: <span id='tm'>--</span></p>
    <p>Auto Mode: <span id='am'>--</span></p>
    <p>YOLO Detection: <span id='yd'>--</span></p>
    <p style='font-weight:bold;'>E-Stop Status: <span id='es'>--</span></p>
  </div><br><br>

  <h3>Manual Controls</h3>
  <div class='grid'>
    <td></td><td><button class='btn' onclick="sendCmd('F')">Forward</button></td><td></td>
    <td><button class='btn' onclick="sendCmd('L')">Left</button></td>
    <td><button class='btn' style='background:red;' onclick="sendCmd('S')">Stop</button></td>
    <td><button class='btn' onclick="sendCmd('R')">Right</button></td>
    <td></td><td><button class='btn' onclick="sendCmd('B')">Back</button></td><td></td>
  </div><br><br>

  <button class='btn' style='width:120px;' onclick="sendCmd('TURBO')">Toggle Turbo</button>
  <button class='btn' style='width:120px;' onclick="sendCmd('AUTO')">Toggle Auto</button>
  <button class='btn' style='width:120px; background:blue;' onclick="sendCmd('BEEP')">Buzzer Test</button>
</body>
</html>
)rawliteral";

// ==========================================
// FUNCTION DECLARATIONS
// ==========================================
void initMotors();
void initSensors();
void moveForward();
void moveBackward();
void turnLeft();
void turnRight();
void stopMotors();
void toggleTurbo(bool enable);
long readDistance(int trigPin, int echoPin);
void checkSensors();
void handleEmergencyStop();
void performServoScan();
void smartDelay(unsigned long ms);

// ==========================================
// SETUP
// ==========================================
void setup() {
  Serial.begin(115200);
  
  initMotors();
  initSensors();
  
  frontServo.attach(SERVO_PIN);
  frontServo.write(90);
  
  tone(BUZZER_PIN, 1000, 200); delay(250);
  tone(BUZZER_PIN, 1200, 200); delay(250);

  WiFi.begin(ssid, password);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected! IP: " + WiFi.localIP().toString());
  tone(BUZZER_PIN, 1500, 500);

  server.on("/", []() { server.send_P(200, "text/html", index_html); });
  server.on("/status", handleStatus);
  server.on("/control", handleControl);
  server.begin();
}

// ==========================================
// MAIN LOOP
// ==========================================
void loop() {
  server.handleClient();

  // Non-blocking Wi-Fi Check (Every 2 seconds)
  if (millis() - lastWifiCheck > 2000) {
    lastWifiCheck = millis();
    bool connected = (WiFi.status() == WL_CONNECTED);
    if (!connected && wasConnected) {
      stopMotors();
      toggleTurbo(false);
      frontServo.write(90);
      Serial.println("Wi-Fi Lost! Safety Stop.");
      wasConnected = false;
    } else if (connected && !wasConnected) {
      wasConnected = true;
    }
  }

  // If Wi-Fi is down, skip processing the rest
  if (!wasConnected) return;

  if (millis() - lastSensorRead > sensorInterval) {
    lastSensorRead = millis();
    checkSensors();
  }

  if (emergencyStop) {
    handleEmergencyStop();
    return; 
  }

  if (autoMode) {
    if (frontDistance <= OBSTACLE_THRESHOLD && frontDistance > 0) {
      performServoScan();
    } else {
      moveForward();
    }
  }

  if (yoloPersonDetected) {
    digitalWrite(BUZZER_PIN, HIGH);
  } else if (!emergencyStop) {
    digitalWrite(BUZZER_PIN, LOW);
  }
}

// ==========================================
// NON-BLOCKING DELAY FOR SERVER
// ==========================================
void smartDelay(unsigned long ms) {
  unsigned long start = millis();
  while (millis() - start < ms) {
    server.handleClient(); // Keep responding to web requests during scans
    yield(); 
  }
}

// ==========================================
// HARDWARE INITIALIZATION & MOTOR CONTROL
// ==========================================
void initMotors() {
  pinMode(M1_IN1, OUTPUT); pinMode(M1_IN2, OUTPUT);
  pinMode(M1_IN3, OUTPUT); pinMode(M1_IN4, OUTPUT);
  pinMode(M2_IN1, OUTPUT); pinMode(M2_IN2, OUTPUT);
  stopMotors();
}

void initSensors() {
  pinMode(TRIG_FRONT, OUTPUT); pinMode(ECHO_FRONT, INPUT);
  pinMode(TRIG_REAR, OUTPUT); pinMode(ECHO_REAR, INPUT);
  pinMode(IR_PIN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);
}

void moveForward() {
  if (emergencyStop) return;
  digitalWrite(M1_IN1, HIGH); digitalWrite(M1_IN2, LOW);
  digitalWrite(M1_IN3, HIGH); digitalWrite(M1_IN4, LOW);
}

void moveBackward() {
  if (emergencyStop) return;
  digitalWrite(M1_IN1, LOW);  digitalWrite(M1_IN2, HIGH);
  digitalWrite(M1_IN3, LOW);  digitalWrite(M1_IN4, HIGH);
}

void turnLeft() {
  if (emergencyStop) return;
  digitalWrite(M1_IN1, HIGH); digitalWrite(M1_IN2, LOW);
  digitalWrite(M1_IN3, LOW);  digitalWrite(M1_IN4, HIGH);
}

void turnRight() {
  if (emergencyStop) return;
  digitalWrite(M1_IN1, LOW);  digitalWrite(M1_IN2, HIGH);
  digitalWrite(M1_IN3, HIGH); digitalWrite(M1_IN4, LOW);
}

void stopMotors() {
  digitalWrite(M1_IN1, LOW); digitalWrite(M1_IN2, LOW);
  digitalWrite(M1_IN3, LOW); digitalWrite(M1_IN4, LOW);
}

void toggleTurbo(bool enable) {
  turboMode = enable;
  digitalWrite(M2_IN1, (turboMode && !emergencyStop) ? HIGH : LOW);
  digitalWrite(M2_IN2, LOW);
}

// ==========================================
// SENSORS & AUTONOMOUS
// ==========================================
long readDistance(int trigPin, int echoPin) {
  digitalWrite(trigPin, LOW); delayMicroseconds(2);
  digitalWrite(trigPin, HIGH); delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  
  // Timeout reduced from 30000us to 10000us (~1.7 meters max range)
  // This drastically reduces blocking time when no object is nearby
  long duration = pulseIn(echoPin, HIGH, 10000); 
  if (duration == 0) return 999; 
  return duration * 0.034 / 2;
}

void checkSensors() {
  frontDistance = readDistance(TRIG_FRONT, ECHO_FRONT);
  rearDistance = readDistance(TRIG_REAR, ECHO_REAR);
  emergencyStop = (digitalRead(IR_PIN) == LOW);
}

void handleEmergencyStop() {
  stopMotors();
  toggleTurbo(false);
  digitalWrite(BUZZER_PIN, HIGH); 
}

void performServoScan() {
  stopMotors();
  tone(BUZZER_PIN, 800, 100);
  
  frontServo.write(45);
  smartDelay(400); // Replaced normal delay
  long leftDist = readDistance(TRIG_FRONT, ECHO_FRONT);
  
  frontServo.write(135);
  smartDelay(400);
  long rightDist = readDistance(TRIG_FRONT, ECHO_FRONT);
  
  frontServo.write(90); 
  smartDelay(300);

  if (leftDist > rightDist && leftDist > OBSTACLE_THRESHOLD) {
    turnLeft(); smartDelay(600);
  } else if (rightDist > leftDist && rightDist > OBSTACLE_THRESHOLD) {
    turnRight(); smartDelay(600);
  } else {
    moveBackward(); smartDelay(800);
    turnRight(); smartDelay(800);
  }
  stopMotors();
}

// ==========================================
// WEB SERVER HANDLERS
// ==========================================
void handleStatus() {
  // Construct a lightweight JSON response
  char json[200];
  snprintf(json, sizeof(json), 
    "{\"fd\":%ld, \"rd\":%ld, \"ir\":%d, \"tm\":%d, \"am\":%d, \"yd\":%d, \"es\":%d}",
    frontDistance, rearDistance, emergencyStop, turboMode, autoMode, yoloPersonDetected, emergencyStop);
  server.send(200, "application/json", json);
}

void handleControl() {
  String cmd = server.arg("cmd");
  
  if (cmd == "F") { autoMode = false; moveForward(); }
  else if (cmd == "B") { autoMode = false; moveBackward(); }
  else if (cmd == "L") { autoMode = false; turnLeft(); }
  else if (cmd == "R") { autoMode = false; turnRight(); }
  else if (cmd == "S") { autoMode = false; stopMotors(); }
  else if (cmd == "TURBO") { toggleTurbo(!turboMode); }
  else if (cmd == "AUTO") { autoMode = !autoMode; if(!autoMode) stopMotors(); }
  else if (cmd == "BEEP") { tone(BUZZER_PIN, 1000, 300); }
  else if (cmd == "PERSON_DETECTED") { yoloPersonDetected = true; }
  else if (cmd == "CLEAR") { yoloPersonDetected = false; }

  server.send(200, "text/plain", "OK");
}