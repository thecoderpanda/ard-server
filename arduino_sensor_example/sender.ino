#include <SPI.h>
#include <LoRa.h>


#define MQ135_PIN A0
#define NSS 10
#define RST 9
#define DIO0 2


void setup() {
  Serial.begin(9600);
  while (!Serial && millis() < 3000);  // Avoid freeze if no serial connection


  LoRa.setPins(NSS, RST, DIO0);
 
  if (!LoRa.begin(433E6)) {
    Serial.println("❌ LoRa init failed. Check connections.");
    while (1);
  }


  Serial.println("✅ LoRa Initialized (Sender)");
}


void loop() {
  int airQuality = analogRead(MQ135_PIN);


  Serial.print("📤 Sending Air Quality: ");
  Serial.println(airQuality);


  LoRa.beginPacket();
  LoRa.println(airQuality);  // <-- sends with newline for clean parsing
  LoRa.endPacket();


  delay(2000);  // Send every 2 seconds
}
