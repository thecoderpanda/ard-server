/*
  Arduino Server Integration
  Connects to WiFi and sends air quality data to the Python server
  Compatible with ESP32 boards
*/

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// WiFi credentials
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

// Server settings
const char* serverUrl = "http://YOUR_SERVER_IP:9090";
int sensorId = -1; // Will be set after registration

// AQI sensor settings
const int airQualityPin = A0;

// Function prototypes
bool registerSensor();
bool sendSensorData(int value);
int readAirQuality();

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("🔄 Starting ESP32 Server Integration...");

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.printf("📶 Connecting to WiFi: %s\n", ssid);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("\n✅ WiFi Connected! IP: %s\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.println("\n❌ WiFi Failed. Check credentials or try again.");
    return;
  }
  
  // Register sensor with server
  bool registered = false;
  while (!registered) {
    registered = registerSensor();
    if (!registered) {
      Serial.println("❌ Failed to register sensor. Retrying in 5 seconds...");
      delay(5000);
    }
  }
  
  Serial.printf("✅ Sensor registered with ID: %d\n", sensorId);
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    // Read air quality value
    int airQuality = readAirQuality();
    Serial.printf("📊 Air Quality: %d\n", airQuality);
    
    // Send data to server
    if (sendSensorData(airQuality)) {
      Serial.println("✅ Data sent successfully to server");
    } else {
      Serial.println("❌ Failed to send data to server");
    }
  } else {
    Serial.println("📶 WiFi disconnected, attempting to reconnect...");
    WiFi.begin(ssid, password);
  }
  
  delay(30000); // Send data every 30 seconds
}

bool registerSensor() {
  HTTPClient http;
  
  // Construct URL
  String url = String(serverUrl) + "/api/sensor/register";
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  
  // Prepare JSON data
  StaticJsonDocument<200> doc;
  doc["name"] = "ESP32_Air_Quality_Sensor";
  doc["description"] = "MQ135 air quality sensor";
  
  String requestBody;
  serializeJson(doc, requestBody);
  
  // Send POST request
  int httpResponseCode = http.POST(requestBody);
  
  bool success = false;
  
  if (httpResponseCode > 0) {
    String response = http.getString();
    Serial.printf("📡 HTTP Response: %s\n", response.c_str());
    
    // Parse response
    StaticJsonDocument<200> responseDoc;
    DeserializationError error = deserializeJson(responseDoc, response);
    
    if (!error) {
      if (responseDoc.containsKey("sensor_id")) {
        sensorId = responseDoc["sensor_id"];
        success = true;
      }
    }
  } else {
    Serial.printf("❌ Error on sending POST: %d\n", httpResponseCode);
  }
  
  http.end();
  return success;
}

bool sendSensorData(int value) {
  if (sensorId == -1) {
    Serial.println("❌ Error: Sensor not registered");
    return false;
  }
  
  HTTPClient http;
  
  // Construct URL
  String url = String(serverUrl) + "/api/sensor/data";
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  
  // Prepare JSON data
  StaticJsonDocument<200> doc;
  doc["sensor_id"] = sensorId;
  doc["value"] = value;
  
  String requestBody;
  serializeJson(doc, requestBody);
  
  // Send POST request
  int httpResponseCode = http.POST(requestBody);
  
  bool success = false;
  
  if (httpResponseCode > 0) {
    String response = http.getString();
    Serial.printf("📡 HTTP Response: %s\n", response.c_str());
    success = true;
  } else {
    Serial.printf("❌ Error on sending POST: %d\n", httpResponseCode);
  }
  
  http.end();
  return success;
}

int readAirQuality() {
  // Read the analog value from the MQ135 sensor
  int sensorValue = analogRead(airQualityPin);
  
  // You can implement your own AQI calculation logic here
  // For now we'll just return the raw analog value
  return sensorValue;
} 