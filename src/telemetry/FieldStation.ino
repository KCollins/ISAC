#include <SPI.h>
#include <SD.h>
#include <Wire.h>
#include <RH_RF95.h>
#include <TinyGPS++.h>
#include <Adafruit_NeoPixel.h>
#include <RTClib.h>

// Feather RP2040 RFM95 Internal Pin Mappings
#define RFM95_CS    16
#define RFM95_RST   17
#define RFM95_INT   21

// Adalogger SD Chip Select & Battery Pin
#define SD_CS       10
#define VBAT_PIN    A0
#define RF95_FREQ   915.0

// Hardware Serial1 for GPS (TX/RX pins on Feather)
#define GPS_BAUD    9600

// Hardware Drivers
RH_RF95 rf95(RFM95_CS, RFM95_INT);
TinyGPSPlus gps;
Adafruit_NeoPixel pixel(1, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800);
RTC_PCF8523 rtc; // Adalogger RTC driver

// Telemetry packet sent to Base Station
struct FieldPacket {
  uint32_t msgId;
  uint32_t utcTimestamp; // UTC Unix Epoch Time from GPS
  float latitude;
  float longitude;
  float altitude;
  float vbat;
};

// Response packet received from Base Station
struct BaseResponse {
  uint32_t msgId;
  uint8_t ledColorState; // 1=Red, 2=Green, 3=Blue
};

// Global State Variables
uint32_t msgCounter = 1;
unsigned long lastTxTime = 0;
const unsigned long TX_INTERVAL = 5000; // Send packet every 5 seconds
bool sdWorking = false;
bool rtcWorking = false;

void setFieldNeoPixel(uint8_t colorState);

// Helper to format date/time string
// Helper to format date/time string safely
String getDateTimeString() {
  char buf[25];
  
  if (rtcWorking) {
    DateTime now = rtc.now();
    snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02d",
             now.year(), now.month(), now.day(),
             now.hour(), now.minute(), now.second());
    return String(buf);
  } else if (gps.date.isValid() && gps.time.isValid()) {
    snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02d",
             gps.date.year(), gps.date.month(), gps.date.day(),
             gps.time.hour(), gps.time.minute(), gps.time.second());
    return String(buf);
  }
  
  return "1970-01-01T00:00:00";
}

void setup() {
  // Pre-deselect SPI Chip Select pins to avoid SPI bus collision
  pinMode(RFM95_CS, OUTPUT); digitalWrite(RFM95_CS, HIGH);
  pinMode(SD_CS, OUTPUT);    digitalWrite(SD_CS, HIGH);

  Serial.begin(115200);

  // Initialize Onboard NeoPixel
  pixel.begin();
  pixel.setBrightness(30);
  setFieldNeoPixel(0);

  // Initialize Serial1 for GPS Receiver
  Serial1.begin(GPS_BAUD);

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

  // Initialize LoRa Radio Hardware Reset
  pinMode(RFM95_RST, OUTPUT);
  digitalWrite(RFM95_RST, HIGH); delay(10);
  digitalWrite(RFM95_RST, LOW);  delay(10);
  digitalWrite(RFM95_RST, HIGH); delay(10);

  if (!rf95.init()) {
    Serial.println("ERROR: Field RFM95 Radio init failed!");
  } else {
    Serial.println("Field RFM95 Radio Initialized.");
    rf95.setFrequency(RF95_FREQ);
    rf95.setTxPower(23, false);
  }

  // Initialize SD Card Logging
  if (!SD.begin(SD_CS)) {
    Serial.println("Warning: SD Card initialization failed!");
  } else {
    sdWorking = true;
    Serial.println("SD Card Initialized.");
    File logFile = SD.open("FIELDLOG.CSV", FILE_WRITE);
    if (logFile) {
      logFile.println("DateTime,MsgID,Lat,Lon,Alt,VBat,BaseAckReceived,BaseRSSI,RequestedLED");
      logFile.close();
    }
  }

  Serial.println("--- Field Station Ready ---");
}

void loop() {
  // 1. Continuously feed incoming characters from GPS module into TinyGPS++
  while (Serial1.available() > 0) {
    gps.encode(Serial1.read());
  }

  // Sync Field RTC time directly from GPS fix when valid GPS data is received
  if (rtcWorking && gps.date.isValid() && gps.time.isValid() && gps.date.year() > 2020) {
    DateTime gpsTime(gps.date.year(), gps.date.month(), gps.date.day(), 
                     gps.time.hour(), gps.time.minute(), gps.time.second());
    rtc.adjust(gpsTime);
  }

  // 2. Transmit telemetry packet every TX_INTERVAL milliseconds
  if (millis() - lastTxTime >= TX_INTERVAL) {
    lastTxTime = millis();
    sendTelemetryPacket();
  }
}

void sendTelemetryPacket() {
  FieldPacket packet;
  packet.msgId = msgCounter++;
  packet.vbat = analogRead(VBAT_PIN) * 2.0 * 3.3 / 1024.0;

  // Extract GPS Position and Calculate UTC Epoch
  if (gps.location.isValid()) {
    packet.latitude  = (float)gps.location.lat();
    packet.longitude = (float)gps.location.lng();
    packet.altitude  = (float)gps.altitude.meters();
  } else {
    packet.latitude  = 0.0;
    packet.longitude = 0.0;
    packet.altitude  = 0.0;
  }

  if (gps.date.isValid() && gps.time.isValid() && gps.date.year() > 2020) {
    DateTime gpsTime(gps.date.year(), gps.date.month(), gps.date.day(),
                     gps.time.hour(), gps.time.minute(), gps.time.second());
    packet.utcTimestamp = gpsTime.unixtime();
  } else if (rtcWorking) {
    packet.utcTimestamp = rtc.now().unixtime();
  } else {
    packet.utcTimestamp = 0;
  }

  Serial.print("Sending Pkt #"); Serial.print(packet.msgId);
  Serial.print(" | UTC Epoch: "); Serial.print(packet.utcTimestamp);
  Serial.print(" | Lat: "); Serial.print(packet.latitude, 6);
  Serial.print(" | Lon: "); Serial.print(packet.longitude, 6);
  Serial.print(" | Bat: "); Serial.print(packet.vbat); Serial.println("V");

  // Send packet over LoRa
  rf95.send((uint8_t*)&packet, sizeof(packet));
  rf95.waitPacketSent();

  // Listen for Base Station ACK
  bool ackReceived = false;
  int16_t baseRssi = 0;
  uint8_t requestedLedColor = 0;

  if (rf95.waitAvailableTimeout(2000)) {
    BaseResponse response;
    uint8_t len = sizeof(response);

    if (rf95.recv((uint8_t*)&response, &len)) {
      if (response.msgId == packet.msgId) {
        ackReceived = true;
        baseRssi = rf95.lastRssi();
        requestedLedColor = response.ledColorState;

        Serial.print(" -> ACK Received! RSSI: "); Serial.print(baseRssi);
        Serial.print(" dBm | Base LED Command: "); Serial.println(requestedLedColor);

        setFieldNeoPixel(requestedLedColor);
      }
    }
  } else {
    Serial.println(" -> No ACK received (timed out).");
  }

  // Log data locally to SD Card
  if (sdWorking) {
    File logFile = SD.open("FIELDLOG.CSV", FILE_WRITE);
    if (logFile) {
      logFile.print(getDateTimeString()); logFile.print(",");
      logFile.print(packet.msgId); logFile.print(",");
      logFile.print(packet.latitude, 6); logFile.print(",");
      logFile.print(packet.longitude, 6); logFile.print(",");
      logFile.print(packet.altitude, 2); logFile.print(",");
      logFile.print(packet.vbat, 2); logFile.print(",");
      logFile.print(ackReceived ? "1" : "0"); logFile.print(",");
      logFile.print(baseRssi); logFile.print(",");
      logFile.println(requestedLedColor);
      logFile.close();
    }
  }
}

// Helper function to update physical onboard RGB NeoPixel (RESTORED LOGIC)
void setFieldNeoPixel(uint8_t colorState) {
  switch (colorState) {
    case 1: // RED
      pixel.setPixelColor(0, pixel.Color(255, 0, 0));
      break;
    case 2: // GREEN
      pixel.setPixelColor(0, pixel.Color(0, 255, 0));
      break;
    case 3: // BLUE
      pixel.setPixelColor(0, pixel.Color(0, 0, 255));
      break;
    default: // Off
      pixel.setPixelColor(0, pixel.Color(0, 0, 0));
      break;
  }
  pixel.show();
}
