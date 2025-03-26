/*
 * ESP8266 Tissue Dispenser System
 * 
 * Controls a tissue dispenser using a servo motor, 
 * a microswitch for tissue detection, and LEDs/buzzer for status indication.
 * Communicates with Water and Soap Dispensers via HTTP.
 * Hosts a web server to show real-time status updates.
 */

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>
#include <Servo.h>

// WiFi Credentials
const char* ssid = "YOUR_SSID";
const char* password = "YOUR_PASSWORD";

// IP addresses of connected nodes
const char* node1_IP = "YOUR_NODE1_IP";  
const char* node2_IP = "YOUR_NODE2_IP"; 

ESP8266WebServer server(80);

// LEDs
const int greenLed = D2;    // LED indicator
const int redLed = D3;     // LED indicator

// Buzzer
const int buzzer = D5;

int processStage = 0;               // Tracks the current stage of the process flow
bool hasRotated = false;            // Tracks whether servo motor has finished rotating
bool buzzerActive = false;          // Tracks whether buzzer is on
unsigned long buzzerStartTime = 0;  // Tracks buzzer start time

// Servo Motor and Microswitch
Servo myServo;
const int servoPin = D1;   // Servo connected to D1
const int microswitch = D6; // Microswitch connected to D6

void setup() {
  Serial.begin(9600);

  // Setup LEDs and Buzzer
  pinMode(greenLed, OUTPUT);
  pinMode(redLed, OUTPUT);
  pinMode(buzzer, OUTPUT);

  // Setup Microswitch and Servo
  pinMode(microswitch, INPUT_PULLUP);
  myServo.attach(servoPin);

  // Set Initial States
  digitalWrite(greenLed, LOW);
  digitalWrite(redLed, LOW);
  digitalWrite(buzzer, LOW);
  myServo.write(90);

  // Connect to WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi!");

  // Print the Dashboard URL in Serial Monitor
  Serial.println("===============================");
  Serial.print("Dashboard Link: http://");
  Serial.print(WiFi.localIP());
  Serial.println("/");
  Serial.println("===============================");

  // Define server routes
  server.on("/", HTTP_GET, serveDashboard);
  server.on("/status", HTTP_GET, serveStatus);
  server.on("/update_stage", HTTP_GET, updateStage);

  server.begin(); // Begin Server
}

void loop() {
  server.handleClient(); // Handle Incoming HTTP Requests

  // Rotate Servo Motor Based on Process Stage 
  if (processStage == 3) {

    // Switch is Open
    if (digitalRead(microswitch) == HIGH) {   
      Serial.println("Tissue is OUT!");
      digitalWrite(redLed, HIGH); // Turn on red LED if tissue is low
      digitalWrite(greenLed, LOW);
      noTone(buzzer);
    } 
    // Servo Motor has not rotated Switch is pressed 
    else if (!hasRotated && digitalRead(microswitch) == LOW) {  
      delay(200);
      Serial.println("Tissue OK.");
      digitalWrite(greenLed, HIGH); // Turn on green LED
      digitalWrite(redLed, LOW);
      
      // Activate buzzer for a short time
      if (!buzzerActive) {  
        tone(buzzer, 3000);
        buzzerActive = true;
        buzzerStartTime = millis();
      }
      
      // Rotate Servo Motor to dispense tissue
      rotateServoSlowly(90, 180, 40);
      delay(1000);
      myServo.write(90);

      // Reset processStage and notify the other nodes
      if (hasRotated) {
        digitalWrite(greenLed, LOW);
        sendResetSignal();
        processStage = 0;
        hasRotated = false;
      }
    }
    
    // Stop the buzzer after 200ms
    if (buzzerActive && millis() - buzzerStartTime >= 200) {
      noTone(buzzer); 
      buzzerActive = false;
      Serial.println("Buzzer OFF after 200ms.");
    }
  }
}

// Handle Dashboard Rendering
void serveDashboard() {
  String html = "<html><head>";
  html += "<meta http-equiv='refresh' content='2'>"; // Auto-refresh every 2s
  html += "<style>";
  html += "body { font-family: Arial, sans-serif; text-align: center; padding-top: 50px; }";
  html += ".status-box { border: 2px solid #333; padding: 10px; margin: 10px; display: inline-block; }";
  html += ".low { color: red; font-weight: bold; }";  // Red warning text
  html += "</style></head><body>";

  html += "<h2>Tissue Dispenser Dashboard</h2>";

  // Check Tissue Status
  bool isLow = digitalRead(microswitch) == HIGH;

  html += "<div class='status-box'><h3>Tissue</h3>";
  if (isLow) {
    html += "<p class='low'>Running Low</p>";  // Red warning text
  } else {
    html += "<p>Available</p>";
  }
  html += "</div>";

  html += "<div class='status-box'><h3>Servo Status</h3><p>" + String(hasRotated ? "Rotated" : "Neutral") + "</p></div>";
  html += "<div class='status-box'><h3>Process Stage</h3><p>" + String(processStage) + "</p></div>";

  html += "</body></html>";

  server.send(200, "text/html", html);
}

// Serve JSON Status Data
void serveStatus() {
  String json = "{";
  json += "\"process_stage\":" + String(processStage) + ",";
  json += "\"microswitch\":" + String(digitalRead(microswitch) == HIGH ? "true" : "false") + ",";
  json += "\"servo_rotated\":" + String(hasRotated ? "true" : "false") + ",";
  json += "}";
  server.send(200, "application/json", json);
}

// Handle Process Stage Updates
void updateStage() {
  if (server.hasArg("stage")) {
    processStage = server.arg("stage").toInt();
    Serial.print("Received processStage: ");
    Serial.println(processStage);
    server.send(200, "text/plain", "Stage Updated");
  } else {
    server.send(400, "text/plain", "Missing stage data");
  }
}

// Function to Rotate Servo Motor Slowly
void rotateServoSlowly(int fromAngle, int toAngle, int stepDelay) {
  if (fromAngle < toAngle) {
    for (int angle = fromAngle; angle <= toAngle; angle += 2) {
      myServo.write(angle);
      delay(stepDelay);
    }
  } else {
    for (int angle = fromAngle; angle >= toAngle; angle -= 2) {
      myServo.write(angle);
      delay(stepDelay);
    }
  }

  hasRotated = true;
  Serial.println("Servo rotation completed.");
}

// Function to Notify Nodes
void sendResetSignal() {
  WiFiClient client;
  HTTPClient http;

  // Notify Water Dispenser to reset process
  String serverPath1 = "http://" + String(node1_IP) + "/update_stage?stage=0";
  Serial.println("Resetting processStage to 0 (Restarting Process)...");
  http.begin(client, serverPath1);
  int httpResponseCode1 = http.GET();
  if (httpResponseCode1 > 0) {
    Serial.println("Node 1 Response: " + String(httpResponseCode1));
  } else {
    Serial.println("Error resetting.");
  }
  http.end();

  // Notify Soap Dispenser to reset process
  String serverPath2 = "http://" + String(node2_IP) + "/update_stage?stage=0";
  Serial.println("Resetting processStage to 0 (Restarting Process)...");
  http.begin(client, serverPath2);
  int httpResponseCode2 = http.GET();
  if (httpResponseCode2 > 0) {
    Serial.println("Node 2 Response: " + String(httpResponseCode2));
  } else {
    Serial.println("Error resetting.");
  }
  http.end();
}
