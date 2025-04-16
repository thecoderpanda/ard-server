#include <SPI.h>
#include <LoRa.h>
#include <math.h>

// --------- Sensor Calibration Settings ---------
#define MQ135_PIN A0
#define RL_VALUE 10.0          // Load resistor in kilo ohms
#define RZERO 4.0              // Tuned RZERO value (adjusted for ppm accuracy)

// --------- LoRa Pins ---------
#define NSS 10
#define RST 9
#define DIO0 2

// --------- Function Prototype ---------
const char* getAQIZone(int AQI);

void setup() {
  Serial.begin(9600);
  while (!Serial && millis() < 3000);

  Serial.println("Starting Air Quality Sensor...");

  // Initialize LoRa
  LoRa.setPins(NSS, RST, DIO0);
  if (!LoRa.begin(433E6)) {
    Serial.println("❌ LoRa init failed. Check connections.");
    while (1);
  }

  // Set LoRa parameters for better reliability
  LoRa.setTxPower(20);          // Maximum power
  LoRa.setSpreadingFactor(12);   // Highest spreading factor for better range
  LoRa.setSignalBandwidth(125E3); // 125 kHz bandwidth
  LoRa.setCodingRate4(8);        // 4/8 coding rate
  LoRa.enableCrc();              // Enable CRC checking

  Serial.println("✅ LoRa Initialized (Sender)");
}

void loop() {
  int adcValue = analogRead(MQ135_PIN);

  if (adcValue == 0) {
    Serial.println("⚠️ Sensor not connected or loose wiring (ADC = 0)");
    delay(2000);
    return;
  }

  // Convert ADC to voltage
  float voltage = adcValue * (5.0 / 1023.0);
  float rs = 0;
  float ratio = 1;

  // Calculate sensor resistance
  if (voltage > 0.01) {
    rs = ((5.0 * RL_VALUE) / voltage) - RL_VALUE;
    ratio = rs / RZERO;
    ratio = constrain(ratio, 0.01, 10.0);
  } else {
    ratio = 1.0;
  }

  // Calculate CO2 PPM using calibration curve
  float ppm = 116.6020682 * pow(ratio, -2.769034857);
  ppm = constrain(ppm * 1000.0, 400.0, 5000.0); // Constrain to realistic CO2 values

  // Calculate AQI based on PPM
  int AQI = (int)((ppm - 400.0) * (500.0 / (5000.0 - 400.0)));
  AQI = constrain(AQI, 0, 500);

  // Debug output
  Serial.println("\n----- Sensor Readings -----");
  Serial.print("ADC Raw: ");
  Serial.println(adcValue);
  Serial.print("Voltage: ");
  Serial.print(voltage, 3);
  Serial.println(" V");
  Serial.print("Sensor Resistance: ");
  Serial.print(rs, 2);
  Serial.println(" kΩ");
  Serial.print("Rs/R0 Ratio: ");
  Serial.println(ratio, 3);
  Serial.print("CO2 Concentration: ");
  Serial.print(ppm, 1);
  Serial.println(" ppm");
  Serial.print("Air Quality Index: ");
  Serial.println(AQI);
  Serial.print("AQI Category: ");
  Serial.println(getAQIZone(AQI));
  Serial.println("------------------------");

  // Prepare and send LoRa packet
  LoRa.beginPacket();
  LoRa.print("CO2:");
  LoRa.print(ppm, 1);
  LoRa.print(" ppm,AQI:");
  LoRa.print(AQI);
  LoRa.print(",Zone:");
  LoRa.print(getAQIZone(AQI));
  LoRa.endPacket();

  delay(2000);  // 2-second delay between readings
}

// AQI Zone categorization
const char* getAQIZone(int AQI) {
  if (AQI <= 50) return "Good";
  else if (AQI <= 100) return "Moderate";
  else if (AQI <= 150) return "Unhealthy for Sensitive Groups";
  else if (AQI <= 200) return "Unhealthy";
  else if (AQI <= 300) return "Very Unhealthy";
  else return "Hazardous";
} 