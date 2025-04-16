#include <SPI.h>
#include <LoRa.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <time.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>


// WiFi Credentials (home router)
const char* ssid = "Shantanu_2.4G";
const char* password = "letsbagp#123";

// Server API settings
const char* serverUrl = "http://192.168.1.100:9090";  // Replace with your computer's local IP
int sensorId = -1; // Will be set after registration

// NTP Configuration
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 19800; // IST offset +5:30
const int daylightOffset_sec = 0;


// LoRa Pins for ESP32
#define NSS 5
#define RST 14
#define DIO0 26


// AQI Reading Struct
struct AQIReading {
  String value;
  String category;
  String time;
};


#define MAX_READINGS 20
AQIReading history[MAX_READINGS];
int readingIndex = 0;


WebServer server(80);
String latestAQI = "No Data";
String latestCategory = "";


// AQI category and color
String getAQICategory(float aqi) {
  if (aqi <= 50) return "<span style='color:green'>Good</span>";
  if (aqi <= 100) return "<span style='color:darkorange'>Moderate</span>";
  if (aqi <= 150) return "<span style='color:orange'>Unhealthy for Sensitive Groups</span>";
  if (aqi <= 200) return "<span style='color:red'>Unhealthy</span>";
  if (aqi <= 300) return "<span style='color:purple'>Very Unhealthy</span>";
  return "<span style='color:maroon'>Hazardous</span>";
}


String getTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return "unknown time";
  }
  char timeString[30];
  strftime(timeString, sizeof(timeString), "%Y-%m-%dT%H:%M:%S", &timeinfo);
  return String(timeString);
}


String translateWiFiStatus(wl_status_t status) {
  switch (status) {
    case WL_NO_SSID_AVAIL: return "No SSID Available";
    case WL_CONNECT_FAILED: return "Connection Failed";
    case WL_CONNECTION_LOST: return "Connection Lost";
    case WL_DISCONNECTED: return "Disconnected";
    case WL_CONNECTED: return "Connected";
    default: return "Unknown Status";
  }
}

// Function to register sensor with the API server
bool registerSensor() {
  HTTPClient http;
  
  String url = String(serverUrl) + "/api/sensor/register";
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  
  StaticJsonDocument<200> doc;
  doc["name"] = "LoRa_Air_Quality_Sensor";
  doc["description"] = "Air quality sensor data received via LoRa";
  
  String requestBody;
  serializeJson(doc, requestBody);
  
  int httpResponseCode = http.POST(requestBody);
  bool success = false;
  
  if (httpResponseCode > 0) {
    String response = http.getString();
    Serial.printf("📡 API Response: %s\n", response.c_str());
    
    StaticJsonDocument<200> responseDoc;
    DeserializationError error = deserializeJson(responseDoc, response);
    
    if (!error && responseDoc.containsKey("sensor_id")) {
      sensorId = responseDoc["sensor_id"];
      success = true;
      Serial.printf("✅ Sensor registered with ID: %d\n", sensorId);
    }
  } else {
    Serial.printf("❌ API Error: %d\n", httpResponseCode);
  }
  
  http.end();
  return success;
}

// Function to send data to the API server
bool sendDataToServer(String value) {
  if (sensorId == -1) {
    Serial.println("❌ Sensor not registered with API server");
    return false;
  }
  
  HTTPClient http;
  
  // Use the special LoRa endpoint
  String url = String(serverUrl) + "/api/lora/data";
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  
  StaticJsonDocument<200> doc;
  doc["sensor_id"] = sensorId;
  doc["value"] = value.toFloat();
  doc["timestamp"] = getTime();
  
  String requestBody;
  serializeJson(doc, requestBody);
  
  int httpResponseCode = http.POST(requestBody);
  bool success = false;
  
  if (httpResponseCode > 0) {
    String response = http.getString();
    Serial.printf("📡 Data sent to API, response: %s\n", response.c_str());
    success = true;
  } else {
    Serial.printf("❌ API Error when sending data: %d\n", httpResponseCode);
  }
  
  http.end();
  return success;
}


void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("🔄 Starting ESP32 Receiver...");


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
    Serial.printf("\n❌ WiFi Failed. Status: %s\n", translateWiFiStatus(WiFi.status()).c_str());
    return;
  }


  if (MDNS.begin("esp32")) {
    Serial.println("✅ mDNS started - http://esp32.local");
  } else {
    Serial.println("❌ Failed to start mDNS");
  }


  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);


  LoRa.setPins(NSS, RST, DIO0);
  if (!LoRa.begin(433E6)) {
    Serial.println("❌ LoRa init failed. Check wiring.");
    while (1);
  }
  Serial.println("✅ LoRa Initialized (Receiver)");
  
  // Register with the API server
  bool registered = false;
  for (int i = 0; i < 3 && !registered; i++) {
    registered = registerSensor();
    if (!registered) {
      Serial.println("⚠️ Failed to register with API server. Retrying...");
      delay(2000);
    }
  }
  
  if (!registered) {
    Serial.println("⚠️ Will continue without API integration");
  }


  server.on("/", []() {
    String html = "<html><head><meta http-equiv='refresh' content='5'>"
                  "<style>body{font-family:sans-serif;text-align:center;padding-top:30px;}"
                  "table{margin:0 auto;border-collapse:collapse;width:70%;}"
                  "th,td{padding:10px;border:1px solid #ccc;}th{background:#2e7d32;color:white;}"
                  "</style></head><body>"
                  "<h2>Current AQI: " + latestAQI + " " + latestCategory + "</h2>"
                  "<h3>Past Readings</h3><table><tr><th>Time</th><th>AQI</th><th>Category</th></tr>";


    for (int i = 0; i < MAX_READINGS; i++) {
      int idx = (readingIndex + i) % MAX_READINGS;
      if (history[idx].value != "") {
        html += "<tr><td>" + history[idx].time + "</td><td>" + history[idx].value + "</td><td>" + history[idx].category + "</td></tr>";
      }
    }


    html += "</table></body></html>";
    server.send(200, "text/html", html);
  });


  server.begin();
  Serial.println("🌐 Web server running. Open in browser: http://" + WiFi.localIP().toString());
}


void loop() {
  server.handleClient();


  int packetSize = LoRa.parsePacket();
  if (packetSize > 0) {
    String data = "";
    while (LoRa.available()) {
      data += (char)LoRa.read();
    }
    data.trim();


    if (data.length() > 0) {
      Serial.print("📥 LoRa Received AQI: ");
      Serial.println(data);


      latestAQI = data;
      float aqiFloat = data.toFloat();
      latestCategory = getAQICategory(aqiFloat);


      history[readingIndex] = { data, latestCategory, getTime() };
      readingIndex = (readingIndex + 1) % MAX_READINGS;
      
      // Forward data to API server
      if (sensorId != -1) {
        if (sendDataToServer(data)) {
          Serial.println("✅ Data forwarded to API server");
        } else {
          Serial.println("⚠️ Failed to forward data to API server");
        }
      }
    } else {
      Serial.println("⚠️ Empty LoRa packet received.");
    }
  }
}
