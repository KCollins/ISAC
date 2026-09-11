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
RTC_PCF8523 rtc; // Adalogger RTC driver

// Telemetry packet sent by Field Station
struct FieldPacket {
  uint32_t msgId;
  uint32_t utcTimestamp; // UTC Unix Epoch Time from GPS
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
bool rtcWorking = false;
uint32_t lastReceivedMsgId = 0;
int16_t lastRssi = 0;
float fieldLat = 0.0, fieldLon = 0.0, fieldVbat = 0.0;

// Helper function to read battery voltage
float readBaseBattery() {
  float measuredvbat = analogRead(VBAT_PIN);
  measuredvbat *= 2.0;   // Divided by 2 on board, so multiply by 2
  measuredvbat *= 3.3;   // Multiply by 3.3V reference
  measuredvbat /= 1024.0;// Convert to voltage
  return measuredvbat;
}

// Helper to get formatted date/time string
String getDateTimeString() {
  if (!rtcWorking) return "1970-01-01T00:00:00";
  
  DateTime now = rtc.now();
  char buf[25];
  snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02d",
           now.year(), now.month(), now.day(),
           now.hour(), now.minute(), now.second());
           
  return String(buf);
}
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
  display.display();

  // Initialize RTC
  if (!rtc.begin()) {
    Serial.println("Warning: RTC not found!");
  } else {
    rtcWorking = true;
    if (!rtc.isrunning()) {
      Serial.println("RTC is NOT running, setting system build time...");
      rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    }
  }

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
      logFile.println("DateTime,MsgID,RSSI,Lat,Lon,FieldVBat,BaseVBat,SentColor");
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

      // Synchronize Base RTC with incoming GPS UTC timestamp from Field Station
      if (rtcWorking && packet.utcTimestamp > 0) {
        rtc.adjust(DateTime(packet.utcTimestamp));
        Serial.print("Base RTC synced to Field UTC Epoch: ");
        Serial.println(packet.utcTimestamp);
      }

      // Determine ACK response color (RESTORED LOGIC)
      uint8_t responseColor = 0;
      if (overrideBlue) {
        responseColor = 3; // 3 = BLUE
      } else {
        responseColor = (packet.msgId % 2 == 0) ? 1 : 2; // 1 = RED (Even), 2 = GREEN (Odd)
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

      // Read Base Battery Voltage
      float baseVbat = readBaseBattery();

      // Log locally to Base Station SD Card
      if (sdWorking) {
        File logFile = SD.open("BASELOG.CSV", FILE_WRITE);
        if (logFile) {
          logFile.print(getDateTimeString()); logFile.print(",");
          logFile.print(packet.msgId); logFile.print(",");
          logFile.print(lastRssi); logFile.print(",");
          logFile.print(packet.latitude, 6); logFile.print(",");
          logFile.print(packet.longitude, 6); logFile.print(",");
          logFile.print(packet.vbat, 2); logFile.print(",");
          logFile.print(baseVbat, 2); logFile.print(",");
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
  else if (currentSentColor == 3) display.println("BLUE (Manual) -- Press C to end");

  display.display();
}
