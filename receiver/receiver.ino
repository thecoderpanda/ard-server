#include <SPI.h>
#include <LoRa.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <time.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// WiFi Credentials
const char* ssid = "Pixel_8";
const char* password = "12345678910";

// Server API settings
const char* serverUrl = "http://192.168.1.10:9090";  // Replace with your server IP
int sensorId = -1;

// NTP Configuration
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 19800;  // IST offset +5:30
const int daylightOffset_sec = 0;

// LoRa Pins for ESP32
#define NSS 5
#define RST 14
#define DIO0 26

// Data structure for readings history
struct Reading {
    String value;
    String category;
    String time;
    float co2_ppm;
    int aqi;
};

#define MAX_READINGS 20
Reading history[MAX_READINGS];
int readingIndex = 0;

WebServer server(80);
String latestAQI = "No Data";
String latestCategory = "";
float latestCO2 = 0;

// HTML and styling for the web interface
const char* HTML_STYLE = "<style>"
    "body { font-family: Arial, sans-serif; margin: 0; padding: 20px; background: #f0f2f5; }"
    ".container { max-width: 800px; margin: 0 auto; background: white; padding: 20px; border-radius: 10px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }"
    "h1, h2 { color: #1a73e8; }"
    ".current-readings { background: #e8f0fe; padding: 15px; border-radius: 8px; margin: 20px 0; }"
    ".reading-good { color: #0d904f; }"
    ".reading-moderate { color: #ff9800; }"
    ".reading-unhealthy { color: #f44336; }"
    "table { width: 100%; border-collapse: collapse; margin-top: 20px; }"
    "th, td { padding: 12px; text-align: left; border-bottom: 1px solid #ddd; }"
    "th { background: #1a73e8; color: white; }"
    "tr:hover { background: #f5f5f5; }"
    "</style>";

String getAQICategory(int aqi) {
    if (aqi <= 50) return "<span class='reading-good'>Good</span>";
    if (aqi <= 100) return "<span class='reading-moderate'>Moderate</span>";
    if (aqi <= 150) return "<span class='reading-unhealthy'>Unhealthy for Sensitive Groups</span>";
    if (aqi <= 200) return "<span class='reading-unhealthy'>Unhealthy</span>";
    if (aqi <= 300) return "<span class='reading-unhealthy'>Very Unhealthy</span>";
    return "<span class='reading-unhealthy'>Hazardous</span>";
}

String getTime() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        return "Time not set";
    }
    char timeString[30];
    strftime(timeString, sizeof(timeString), "%Y-%m-%dT%H:%M:%S", &timeinfo);
    return String(timeString);
}

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

bool sendDataToServer(const String& rawData, float co2_ppm, int aqi, const String& category) {
    if (sensorId == -1) {
        Serial.println("❌ Sensor not registered with API server");
        return false;
    }

    HTTPClient http;
    String url = String(serverUrl) + "/api/lora/data";
    http.begin(url);
    http.addHeader("Content-Type", "application/json");

    StaticJsonDocument<300> doc;
    doc["sensor_id"] = sensorId;
    doc["value"] = rawData;
    doc["timestamp"] = getTime();

    String requestBody;
    serializeJson(doc, requestBody);

    int httpResponseCode = http.POST(requestBody);
    bool success = false;

    if (httpResponseCode > 0) {
        String response = http.getString();
        Serial.printf("📡 Server Response: %s\n", response.c_str());
        success = true;
    } else {
        Serial.printf("❌ API Error: %d\n", httpResponseCode);
    }

    http.end();
    return success;
}

void handleRoot() {
    String html = "<!DOCTYPE html><html><head>";
    html += "<meta http-equiv='refresh' content='5'>";
    html += "<title>Air Quality Monitor</title>";
    html += HTML_STYLE;
    html += "</head><body><div class='container'>";
    
    // Current readings section
    html += "<h1>Air Quality Monitor</h1>";
    html += "<div class='current-readings'>";
    html += "<h2>Current Readings</h2>";
    html += "<p>AQI: " + latestAQI + " " + latestCategory + "</p>";
    html += "<p>CO2: " + String(latestCO2, 1) + " ppm</p>";
    html += "<p>Last Update: " + getTime() + "</p>";
    html += "</div>";

    // Historical data table
    html += "<h2>Historical Data</h2>";
    html += "<table><tr><th>Time</th><th>AQI</th><th>CO2 (ppm)</th><th>Category</th></tr>";

    for (int i = 0; i < MAX_READINGS; i++) {
        int idx = (readingIndex - 1 - i + MAX_READINGS) % MAX_READINGS;
        if (history[idx].value.length() > 0) {
            html += "<tr><td>" + history[idx].time + "</td>";
            html += "<td>" + String(history[idx].aqi) + "</td>";
            html += "<td>" + String(history[idx].co2_ppm, 1) + "</td>";
            html += "<td>" + history[idx].category + "</td></tr>";
        }
    }

    html += "</table></div></body></html>";
    server.send(200, "text/html", html);
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n🔄 Starting ESP32 Air Quality Monitor...");

    // Connect to WiFi
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
        Serial.println("\n❌ WiFi Connection Failed!");
        ESP.restart();
    }

    // Configure time
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

    // Initialize LoRa
    LoRa.setPins(NSS, RST, DIO0);
    if (!LoRa.begin(433E6)) {
        Serial.println("❌ LoRa initialization failed!");
        while (1);
    }
    
    // Set LoRa parameters to match sender
    LoRa.setSpreadingFactor(12);
    LoRa.setSignalBandwidth(125E3);
    LoRa.setCodingRate4(8);
    LoRa.enableCrc();
    
    Serial.println("✅ LoRa Initialized");

    // Register with API server
    bool registered = false;
    for (int i = 0; i < 3 && !registered; i++) {
        registered = registerSensor();
        if (!registered) delay(2000);
    }

    // Setup web server
    server.on("/", handleRoot);
    server.begin();
    Serial.printf("🌐 Web server started: http://%s\n", WiFi.localIP().toString().c_str());
}

void loop() {
    server.handleClient();

    int packetSize = LoRa.parsePacket();
    if (packetSize) {
        String data = "";
        while (LoRa.available()) {
            data += (char)LoRa.read();
        }
        data.trim();

        if (data.length() > 0) {
            Serial.println("\n📥 Received LoRa Data: " + data);

            // Parse CO2 and AQI values
            float co2_ppm = 0;
            int aqi = 0;
            String category = "";

            int co2Start = data.indexOf("CO2:") + 4;
            int co2End = data.indexOf(" ppm");
            int aqiStart = data.indexOf("AQI:") + 4;
            int aqiEnd = data.indexOf(",Zone:");
            int zoneStart = data.indexOf("Zone:") + 5;

            if (co2Start > 4 && co2End > co2Start) {
                co2_ppm = data.substring(co2Start, co2End).toFloat();
            }
            if (aqiStart > 4 && aqiEnd > aqiStart) {
                aqi = data.substring(aqiStart, aqiEnd).toInt();
            }
            if (zoneStart > 5) {
                category = data.substring(zoneStart);
            }

            // Update latest values
            latestAQI = String(aqi);
            latestCO2 = co2_ppm;
            latestCategory = getAQICategory(aqi);

            // Update history
            history[readingIndex] = {
                String(aqi),
                category,
                getTime(),
                co2_ppm,
                aqi
            };
            readingIndex = (readingIndex + 1) % MAX_READINGS;

            // Send to server
            if (sensorId != -1) {
                if (sendDataToServer(data, co2_ppm, aqi, category)) {
                    Serial.println("✅ Data forwarded to server");
                } else {
                    Serial.println("❌ Failed to forward data");
                }
            }
        }
    }
} 