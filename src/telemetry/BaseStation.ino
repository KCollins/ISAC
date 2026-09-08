#include <SPI.h>
#include <SD.h>
#include <Wire.h>
#include <RH_RF95.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <RTClib.h>

// Feather RP2040 RFM95 Internal Pin Mappings
#define RFM95_CS    16
#define RFM95_RST   17
#define RFM95_INT   21

// Adalogger SD Chip Select
#define SD_CS       10
#define VBAT_PIN    A0
#define RF95_FREQ   915.0

// OLED FeatherWing Button Pins
#define BUTTON_A    9
#define BUTTON_B    6
#define BUTTON_C    5

RH_RF95 rf95(RFM95_CS, RFM95_INT);
Adafruit_SH1107 display = Adafruit_SH1107(64, 128, &Wire);
RTC_PCF8523 rtc;

// Packet format from Field Station
struct FieldPacket {
  uint32_t msgId;
  float latitude;
  float longitude;
  float altitude;
  float vbat;
};

// Response packet sent to Field Station
struct BaseResponse {
  uint32_t msgId;
  uint8_t ledColorState; // 1=Red, 2=Green, 3=Blue
};

uint8_t selectedLedState = 1; // Default to Red
int16_t lastRssi = 0;
uint32_t lastPacketReceived = 0;
unsigned long lastDisplayUpdate = 0;
bool sdWorking = false;

// Variables to store received Field GPS coordinates
float lastLat = 0.0;
float lastLon = 0.0;
float lastAlt = 0.0;
float lastFieldBat = 0.0;

void setup() {
  // Pre-deselect SPI CS pins immediately at boot
  pinMode(RFM95_CS, OUTPUT); digitalWrite(RFM95_CS, HIGH);
  pinMode(SD_CS, OUTPUT);    digitalWrite(SD_CS, HIGH);

  pinMode(BUTTON_A, INPUT_PULLUP);
  pinMode(BUTTON_B, INPUT_PULLUP);
  pinMode(BUTTON_C, INPUT_PULLUP);

  Serial.begin(115200);
  unsigned long start = millis();
  while (!Serial && (millis() - start < 2000));

  Serial.println("\n--- Base Station Booting ---");

  // Initialize SH1107 OLED
  Wire.begin();
  delay(250); // Give OLED display time to power up
  display.begin(0x3C, true);
  display.setRotation(1);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(0, 0);
  display.println("Hello, Kiefer!");
  delay(25000);
  display.println("Base Station Ready");
  display.println("Initializing hardware...");
  display.display();

  // Reset RFM95 LoRa Radio
  pinMode(RFM95_RST, OUTPUT);
  digitalWrite(RFM95_RST, HIGH); delay(10);
  digitalWrite(RFM95_RST, LOW);  delay(10);
  digitalWrite(RFM95_RST, HIGH); delay(10);

  if (!rf95.init()) {
    Serial.println("ERROR: RFM95 LoRa radio init failed!");
    display.println("LoRa Radio: FAIL");
    display.display();
  } else {
    Serial.println("RFM95 Radio Initialized.");
    rf95.setFrequency(RF95_FREQ);
    rf95.setTxPower(23, false);
    display.println("LoRa Radio: OK");
    display.display();
  }

  // Initialize Adalogger SD Card
  if (!SD.begin(SD_CS)) {
    Serial.println("Warning: SD initialization failed!");
    display.println("SD Card: FAIL");
    display.display();
  } else {
    sdWorking = true;
    Serial.println("SD Card Initialized.");
    display.println("SD Card: OK");
    display.display();

    File logFile = SD.open("BASELOG.CSV", FILE_WRITE);
    if (logFile) {
      logFile.println("Timestamp,FieldPacketID,Detected,RSSI,BaseBattVolt,FieldLat,FieldLon,FieldAlt,FieldBattVolt,SelectedLED");
      logFile.close();
    }
  }

  rtc.begin();
  delay(1000);
}

void loop() {
  // Check OLED Button Presses
  if (digitalRead(BUTTON_A) == LOW) selectedLedState = 1; // Red
  if (digitalRead(BUTTON_B) == LOW) selectedLedState = 2; // Green
  if (digitalRead(BUTTON_C) == LOW) selectedLedState = 3; // Blue

  // Listen for packets from Field Station
  if (rf95.available()) {
    FieldPacket packet;
    uint8_t len = sizeof(packet);

    if (rf95.recv((uint8_t*)&packet, &len)) {
      lastRssi = rf95.lastRssi();
      lastPacketReceived = packet.msgId;

      // Extract GPS & battery info from field packet
      lastLat = packet.latitude;
      lastLon = packet.longitude;
      lastAlt = packet.altitude;
      lastFieldBat = packet.vbat;

      Serial.print("Received Pkt #"); Serial.print(packet.msgId);
      Serial.print(" | RSSI: "); Serial.print(lastRssi); Serial.print(" dBm");
      Serial.print(" | Lat: "); Serial.print(lastLat, 6);
      Serial.print(" | Lon: "); Serial.println(lastLon, 6);

      // Transmit ACK back to Field Station containing requested LED color
      BaseResponse response;
      response.msgId = packet.msgId;
      response.ledColorState = selectedLedState;

      delay(50); // Give Field Station 10ms to switch from TX mode into RX mode 
      rf95.send((uint8_t*)&response, sizeof(response));
      rf95.waitPacketSent();

      if (sdWorking) {
        logToSD(packet.msgId, true, lastRssi);
      }
    }
  }

  // Refresh OLED Screen every 500ms
  if (millis() - lastDisplayUpdate > 500) {
    lastDisplayUpdate = millis();
    updateOLED();
  }
}

void updateOLED() {
  float vbat = analogRead(VBAT_PIN) * 2.0 * 3.3 / 1024.0;

  display.clearDisplay();
  display.setCursor(0, 0);
  
  // Header line
  display.print("BASE:"); display.print(vbat, 1); display.print("V ");
  display.print("Pkt:"); display.println(lastPacketReceived);
  
  display.print("RSSI: "); display.print(lastRssi); display.println(" dBm");

  // GPS Data from Field Station
  display.print("Lat: "); display.println(lastLat, 5);
  display.print("Lon: "); display.println(lastLon, 5);

  display.print("F.Bat:"); display.print(lastFieldBat, 1); display.print("V ");
  display.print("Alt:"); display.print(lastAlt, 0); display.println("m");

  display.print("LED Cmd: ");
  if (selectedLedState == 1) display.println("RED (A)");
  else if (selectedLedState == 2) display.println("GREEN (B)");
  else if (selectedLedState == 3) display.println("BLUE (C)");

  display.display();
}

void logToSD(uint32_t fieldMsgId, bool detected, int16_t rssi) {
  DateTime now = rtc.now();
  float vbat = analogRead(VBAT_PIN) * 2.0 * 3.3 / 1024.0;

  File logFile = SD.open("BASELOG.CSV", FILE_WRITE);
  if (logFile) {
    logFile.print(now.timestamp()); logFile.print(",");
    logFile.print(fieldMsgId); logFile.print(",");
    logFile.print(detected ? "1" : "0"); logFile.print(",");
    logFile.print(rssi); logFile.print(",");
    logFile.print(vbat, 2); logFile.print(",");
    logFile.print(lastLat, 6); logFile.print(",");
    logFile.print(lastLon, 6); logFile.print(",");
    logFile.print(lastAlt, 2); logFile.print(",");
    logFile.print(lastFieldBat, 2); logFile.print(",");
    logFile.println(selectedLedState);
    logFile.close();
  }
}
