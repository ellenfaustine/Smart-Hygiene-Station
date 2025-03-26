/*
 * ESP8266 Water Dispenser System
 * 
 * Controls a water dispensing system with ultrasonic sensors for level and hand detection,
 * a relay to activate the pump, and LEDs/buzzer for status indication.
 * Communicates with Soap and Tissue Dispensers via HTTP requests.
 * Hosts a web server to display real-time system status.
 */

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>

// WiFi Credentials
const char* ssid = "YOUR_SSID";
const char* password = "YOUR_PASSWORD";

// IP addresses of connected nodes
const char* node2_IP = "YOUR_NODE2_IP";  
const char* node3_IP = "YOUR_NODE3_IP"; 

ESP8266WebServer server(80); 

// Water pump relay
const int relay = D1;  

// LEDs
const int blueLed = D5;
const int redLed = D3;

// Buzzer
const int buzzer = D2;

// Ultrasonic Sensor for water level
const int trig1 = D4;   
const int echo1 = D8;   

// Ultrasonic Sensor for hand detection
const int trig2 = D6;   
const int echo2 = D7;   

int processStage = 0;                 // Tracks the current stage of the process flow
int distance1, distance2;             // Stores measured distances from ultrasonic sensors
bool pumpRunning = false;            // Indicates whether the water pump is currently running
unsigned long pumpStartTime = 0;     // Stores the time when the pump was activated
const float containerHeight = 13.0;  // Height of the water container (in cm) used for level calculations

void setup() {
  Serial.begin(9600);

  // Setup Ultrasonic Sensors
  pinMode(trig1, OUTPUT);
  pinMode(echo1, INPUT);
  pinMode(trig2, OUTPUT);
  pinMode(echo2, INPUT);

  // Setup Pump, LEDs, and Buzzer
  pinMode(relay, OUTPUT);
  pinMode(blueLed, OUTPUT);
  pinMode(redLed, OUTPUT);
  pinMode(buzzer, OUTPUT);

  // Set Initial States
  digitalWrite(relay, HIGH); 
  digitalWrite(blueLed, LOW);
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
  Serial.print(WiFi.localIP());  // Get ESP8266's IP Address
  Serial.println("/");
  Serial.println("===============================");

  // Define server routes
  server.on("/", HTTP_GET, serveDashboard);
  server.on("/status", HTTP_GET, serveStatus);
  server.on("/update_stage", HTTP_GET, updateStage);

  server.begin();  // Begin Server
}

void loop() {
  server.handleClient();  // Handle Incoming HTTP Requests

  // Measure Water Level
  distance1 = getDistance(trig1, echo1);
  delay(50); 

  // Measure Hand distance
  distance2 = getDistance(trig2, echo2);

  // Turn on red LED if water level is low
  digitalWrite(redLed, (getWaterLevelPercentage(distance1) < 20) ? HIGH : LOW);

  // Activate pump on based on process stage or hand detection
  if (((processStage == 0 && distance2 < 10) || (processStage == 2)) && !pumpRunning) {
    
    // 5 seconds delay for hand washing after soap is dispensed
    if (processStage == 2) {
      delay(5000);
    }

    Serial.println("Turning ON Water Pump...");
    digitalWrite(relay, LOW);
    digitalWrite(blueLed, HIGH); // Turn on blue LED
    tone(buzzer, 1000); // Turn on buzzer
    pumpRunning = true; 
    pumpStartTime = millis(); // Start pump timer
  } 

  // Stop Pump after 5 seconds
  if (pumpRunning && millis() - pumpStartTime >= 5000) {
    Serial.println("Turning OFF pump.");
    digitalWrite(relay, HIGH);
    digitalWrite(blueLed, LOW); // Turn off blue LED
    noTone(buzzer); // Turn off buzzer
    pumpRunning = false;

    if(processStage == 0) {
      sendCompletionSignal(); // Send signal that process stage 0 is done
      processStage = 1; // Update process stage to 1
    } else if (processStage == 2) {
      sendCompletionSignal(); // Send signal that process stage 2 is done
      processStage = 3; // Update process stage to 3
    }

    delay(500);
  }
}

// Function to Convert Water Level to Percentage
float getWaterLevelPercentage(int sensorDistance) {
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
  html += ".low { color: red; font-weight: bold; }";  
  html += "</style></head><body>";

  html += "<h2>Water Dispenser Dashboard</h2>";

  // Check Water Level Status
  float waterLevel = getWaterLevelPercentage(distance1);
  bool isLow = (waterLevel < 20);

  html += "<div class='status-box'><h3>Water Level</h3>";
  if (isLow) {
    html += "<p class='low'>" + String(waterLevel, 1) + " % (LOW)</p>";  // Red warning text
  } else {
    html += "<p>" + String(waterLevel, 1) + " %</p>";
  }
  html += "</div>";

  html += "<div class='status-box'><h3>Hand Distance</h3><p>" + String(distance2) + " cm</p></div>";
  html += "<div class='status-box'><h3>Pump Status</h3><p>" + String(pumpRunning ? "ON" : "OFF") + "</p></div>";
  html += "<div class='status-box'><h3>Process Stage</h3><p>" + String(processStage) + "</p></div>";

  html += "</body></html>";

  server.send(200, "text/html", html);
}

// Serve JSON Status Data
void serveStatus() {
  String json = "{";
  json += "\"water_level\":" + String(distance1) + ",";
  json += "\"hand_distance\":" + String(distance2) + ",";
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

// Function to Measure Distance for any Ultrasonic Sensor
int getDistance(int trigPin, int echoPin) {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  long duration = pulseIn(echoPin, HIGH, 30000);
  return (duration > 0) ? (duration * 0.034) / 2 : -1;
}

// Function to notify nodes when process stage changes
void sendCompletionSignal() {
  WiFiClient client;
  HTTPClient http;
  bool success1 = false, success2 = false;

  // If the process is at stae 0, notify both the Soap Dispenser and Tissue Dispenser to update to stage 1
  if (processStage == 0) {
    String serverPath1 = "http://" + String(node2_IP) + "/update_stage?stage=1";
    Serial.println("Sending signal to Soap Dispenser (Node 2)...");
    http.begin(client, serverPath1);
    int httpResponseCode1 = http.GET();
    if (httpResponseCode1 > 0) {
      Serial.println("Node 2 Response: " + String(httpResponseCode1));
    } else {
      Serial.println("Error sending request to Node 2.");
    }
    http.end();

    String serverPath2 = "http://" + String(node3_IP) + "/update_stage?stage=1";
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
  // If the process is at stage 2, notify the Soap Dispenser and Tissue Dispenser to update to stage 3
  else if (processStage == 2) {
    String serverPath3 = "http://" + String(node2_IP) + "/update_stage?stage=3";
    Serial.println("Sending signal to Soap Dispenser (Node 2)...");
    http.begin(client, serverPath3);
    int httpResponseCode3 = http.GET();
    if (httpResponseCode3 > 0) {
      Serial.println("Node 2 Response: " + String(httpResponseCode3));
    } else {
      Serial.println("Error sending request to Node 2.");
    }
    http.end();

    String serverPath4 = "http://" + String(node3_IP) + "/update_stage?stage=3";
    Serial.println("Sending signal to Tissue Dispenser (Node 3)...");
    http.begin(client, serverPath4);
    int httpResponseCode4 = http.GET();
    if (httpResponseCode4 > 0) {
      Serial.println("Node 3 Response: " + String(httpResponseCode4));
    } else {
      Serial.println("Error sending request to Node 3.");
    }
    http.end();
  } 
  
}