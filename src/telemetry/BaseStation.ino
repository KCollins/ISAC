#include <SPI.h>
#include <SD.h>
#include <Wire.h>
#include <RH_RF95.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>

// Feather RP2040 RFM95 Internal Pin Mappings
#define RFM95_CS    16
#define RFM95_RST   17
#define RFM95_INT   21

// OLED FeatherWing Pins (RP2040 default I2C + Buttons)
#define OLED_CS     -1
#define BUTTON_A     9
#define BUTTON_B     6
#define BUTTON_C     5

// Adalogger SD Chip Select & Battery Pin
#define SD_CS       10
#define VBAT_PIN    A0
#define RF95_FREQ   915.0

// Hardware Drivers
RH_RF95 rf95(RFM95_CS, RFM95_INT);
Adafruit_SH1107 display = Adafruit_SH1107(64, 128, &Wire);

// Packet format sent by Field Station
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

// Global State Variables
bool overrideBlue = false;
bool sdWorking = false;
uint32_t lastReceivedMsgId = 0;
int16_t lastRssi = 0;
float fieldLat = 0.0, fieldLon = 0.0, fieldVbat = 0.0;

void setup() {
  // Pre-deselect SPI pins to prevent bus collisions
  pinMode(RFM95_CS, OUTPUT); digitalWrite(RFM95_CS, HIGH);
  pinMode(SD_CS, OUTPUT);    digitalWrite(SD_CS, HIGH);

  Serial.begin(115200);

  // Initialize Buttons
  pinMode(BUTTON_A, INPUT_PULLUP);
  pinMode(BUTTON_B, INPUT_PULLUP);
  pinMode(BUTTON_C, INPUT_PULLUP);

  // Initialize OLED
  display.begin(0x3C, true);
  display.clearDisplay();
  display.setRotation(1);
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  display.println("Hello, Kiefer!");
  display.println("Base Station Ready");
  display.println("Range Test Mode");
  delay(50000);
  display.display();

  // Reset RFM95 Radio
  pinMode(RFM95_RST, OUTPUT);
  digitalWrite(RFM95_RST, HIGH); delay(10);
  digitalWrite(RFM95_RST, LOW);  delay(10);
  digitalWrite(RFM95_RST, HIGH); delay(10);

  if (!rf95.init()) {
    Serial.println("ERROR: Base RFM95 Radio init failed!");
  } else {
    rf95.setFrequency(RF95_FREQ);
    rf95.setTxPower(23, false);
    Serial.println("Base RFM95 Radio Initialized.");
  }

  // Initialize SD Logging
  if (!SD.begin(SD_CS)) {
    Serial.println("Warning: SD Card initialization failed!");
  } else {
    sdWorking = true;
    File logFile = SD.open("BASELOG.CSV", FILE_WRITE);
    if (logFile) {
      logFile.println("MsgID,RSSI,Lat,Lon,FieldVBat,SentColor");
      logFile.close();
    }
  }
}

void loop() {
  // Button C toggles persistent Blue override mode on/off
  if (digitalRead(BUTTON_C) == LOW) {
    delay(50); // Debounce
    if (digitalRead(BUTTON_C) == LOW) {
      overrideBlue = !overrideBlue;
      Serial.print("Blue Override Toggled: ");
      Serial.println(overrideBlue ? "ON" : "OFF");
      while (digitalRead(BUTTON_C) == LOW); // Wait for release
    }
  }

  // Listen for incoming telemetry from Field Station
  if (rf95.available()) {
    FieldPacket packet;
    uint8_t len = sizeof(packet);

    if (rf95.recv((uint8_t*)&packet, &len)) {
      lastReceivedMsgId = packet.msgId;
      lastRssi = rf95.lastRssi();
      fieldLat = packet.latitude;
      fieldLon = packet.longitude;
      fieldVbat = packet.vbat;

      // Determine ACK response color
      uint8_t responseColor = 0;
      if (overrideBlue) {
        responseColor = 3; // 3 = BLUE
      } else {
        // Toggle based on packet ID sequence: Even = Red (1), Odd = Green (2)
        responseColor = (packet.msgId % 2 == 0) ? 1 : 2;
      }

      // Build ACK response
      BaseResponse response;
      response.msgId = packet.msgId;
      response.ledColorState = responseColor;

      // CRITICAL PAUSE: Allow Field Station hardware to switch to RX mode
      delay(50);

      // Transmit ACK
      rf95.send((uint8_t*)&response, sizeof(response));
      rf95.waitPacketSent();

      // Log locally to Base Station SD Card
      if (sdWorking) {
        File logFile = SD.open("BASELOG.CSV", FILE_WRITE);
        if (logFile) {
          logFile.print(packet.msgId); logFile.print(",");
          logFile.print(lastRssi); logFile.print(",");
          logFile.print(packet.latitude, 6); logFile.print(",");
          logFile.print(packet.longitude, 6); logFile.print(",");
          logFile.print(packet.vbat, 2); logFile.print(",");
          logFile.println(responseColor);
          logFile.close();
        }
      }

      // Refresh Display Info
      updateOLED(responseColor);
    }
  }
}

void updateOLED(uint8_t currentSentColor) {
  display.clearDisplay();
  display.setCursor(0, 0);

  display.print("Pkt #"); display.print(lastReceivedMsgId);
  display.print(" | RSSI:"); display.println(lastRssi);

  display.print("Lat: "); display.println(fieldLat, 5);
  display.print("Lon: "); display.println(fieldLon, 5);
  display.print("F-Bat: "); display.print(fieldVbat); display.println("V");

  display.print("CMD Sent: ");
  if (currentSentColor == 1) display.println("RED (Even)");
  else if (currentSentColor == 2) display.println("GREEN (Odd)");
  else if (currentSentColor == 3) display.println("BLUE (Manual C)");

  display.display();
}
