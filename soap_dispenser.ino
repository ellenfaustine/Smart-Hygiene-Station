/*
 * ESP8266 Soap Dispenser System
 * 
 * Controls a soap dispenser using an ultrasonic sensor to measure soap levels,
 * a relay to activate the pump, and LEDs/buzzer for status indication.
 * Communicates with Water and Tissue Dispensers via HTTP requests.
 * Hosts a web server to display real-time system status.
 */

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>

// WiFi Credentials
const char* ssid = "YOUR_SSID";
const char* password = "YOUR_PASSWORD";

// IP addresses of connected nodes
const char* node1_IP = "YOUR_NODE1_IP";  
const char* node3_IP = "YOUR_NODE3_IP"; 

ESP8266WebServer server(80);

// Soap pump relay
const int relay = D1;  

// LEDs
const int yellowLed = D5;
const int redLed = D3;

// Buzzer
const int buzzer = D2;

// Ultrasonic Sensor to measure soap level
const int trig1 = D4;   
const int echo1 = D8;   

int processStage = 0;                 // Tracks the current stage of the process flow
int distance1;                        // Stores measured distances from ultrasonic sensors
bool pumpRunning = false;             // Indicates whether the water pump is currently running
unsigned long pumpStartTime = 0;      // Stores the time when the pump was activated
const float containerHeight = 13.0;   // Height of the water container (in cm) used for level calculations

void setup() {
  Serial.begin(9600);

  // Setup Ultrasonic Sensor
  pinMode(trig1, OUTPUT);
  pinMode(echo1, INPUT);

  // Setup Pump, LEDs, and Buzzer
  pinMode(relay, OUTPUT);
  pinMode(yellowLed, OUTPUT);
  pinMode(redLed, OUTPUT);
  pinMode(buzzer, OUTPUT);

  // Set Initial States
  digitalWrite(relay, HIGH); 
  digitalWrite(yellowLed, LOW);
  digitalWrite(redLed, LOW);
  digitalWrite(buzzer, LOW); 

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

  server.begin(); // Begin server
}

void loop() {
  server.handleClient(); // Handle Incoming HTTP Requests

  // Measure Soap Level
  distance1 = getDistance(trig1, echo1);
  delay(50);

  // Turn on red LED if soap level is low
  digitalWrite(redLed, (getSoapLevelPercentage(distance1) < 20) ? HIGH : LOW);

  // Activate pump on based on process stage
  if (processStage == 1 && !pumpRunning) {
    Serial.println("Turning ON Soap Dispenser...");
    digitalWrite(relay, LOW);
    digitalWrite(yellowLed, HIGH); // Turn on yellow LED 
    tone(buzzer, 500); // Turn on buzzer 
    pumpRunning = true;
    pumpStartTime = millis(); // Start pump timer
  } 

  // Stop Pump after 2 seconds
  if (pumpRunning && millis() - pumpStartTime >= 2000) {
    Serial.println("2 seconds passed. Turning OFF Soap Dispenser.");
    digitalWrite(relay, HIGH);
    digitalWrite(yellowLed, LOW); // Turn off yellow LED
    noTone(buzzer); // Turn off buzzer
    pumpRunning = false;

    sendCompletionSignal(); // Send signal that process stage 1 is done
    processStage = 2; // Update process stage to 2

    delay(500);
  }
}

// Function to Convert Soap Level to Percentage
float getSoapLevelPercentage(int sensorDistance) {
  float waterLevel = (1 - (sensorDistance / containerHeight)) * 100;
  return (waterLevel < 0) ? 0 : (waterLevel > 100) ? 100 : waterLevel;  // Ensure 0-100%
}

// Handle Dashboard Rendering
void serveDashboard() {
  String html = "<html><head>";
  html += "<meta http-equiv='refresh' content='2'>"; // Auto-refresh every 2s
  html += "<style>";
  html += "body { font-family: Arial, sans-serif; text-align: center; padding-top: 50px; }";
  html += ".status-box { border: 2px solid #333; padding: 10px; margin: 10px; display: inline-block; }";
  html += ".low { color: red; font-weight: bold; }";  // Red text for low soap level
  html += "</style></head><body>";

  html += "<h2>Soap Dispenser Dashboard</h2>";

  // Check Soap Level Status
  float soapLevel = getSoapLevelPercentage(distance1);
  bool isLow = (soapLevel < 20);

  html += "<div class='status-box'><h3>Soap Level</h3>";
  if (isLow) {
    html += "<p class='low'>" + String(soapLevel, 1) + " % (LOW)</p>";  // Red warning text
  } else {
    html += "<p>" + String(soapLevel, 1) + " %</p>";
  }
  html += "</div>";

  html += "<div class='status-box'><h3>Pump Status</h3><p>" + String(pumpRunning ? "ON" : "OFF") + "</p></div>";
  html += "<div class='status-box'><h3>Process Stage</h3><p>" + String(processStage) + "</p></div>";

  html += "</body></html>";

  server.send(200, "text/html", html);
}

// Serve JSON Status Data
void serveStatus() {
  String json = "{";
  json += "\"soap_level\":" + String(distance1) + ",";
  json += "\"pump_running\":" + String(pumpRunning ? "true" : "false") + ",";
  json += "\"process_stage\":" + String(processStage);
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

// Function to Measure Distance for Ultrasonic Sensor
int getDistance(int trigPin, int echoPin) {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  long duration = pulseIn(echoPin, HIGH, 30000);
  return (duration > 0) ? (duration * 0.034) / 2 : -1;
}

// Function to Notify Nodes
void sendCompletionSignal() {
  WiFiClient client;
  HTTPClient http;

  // Notify Water Dispenser to update to stage 2
  String serverPath1 = "http://" + String(node1_IP) + "/update_stage?stage=2";
  Serial.println("Sending signal to Water Dispenser (Node 1)...");
  http.begin(client, serverPath1);
  int httpResponseCode1 = http.GET();
  if (httpResponseCode1 > 0) {
    Serial.println("Node 1 Response: " + String(httpResponseCode1));
  } else {
    Serial.println("Error sending request to Node 1.");
  }
  http.end();

  // Notify Tissue Dispenser to update to stage 2
  String serverPath2 = "http://" + String(node3_IP) + "/update_stage?stage=2";
  Serial.println("Sending signal to Tissue Dispenser (Node 3)...");
  http.begin(client, serverPath2);
  int httpResponseCode2 = http.GET();
  if (httpResponseCode2 > 0) {
    Serial.println("Node 3 Response: " + String(httpResponseCode2));
  } else {
    Serial.println("Error sending request to Node 3.");
  }
  http.end();
}