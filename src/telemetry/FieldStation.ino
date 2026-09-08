#include <SPI.h>
#include <SD.h>
#include <Wire.h>
#include <RH_RF95.h>
#include <TinyGPS++.h>
#include <Adafruit_NeoPixel.h>

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

// Initialize RadioHead LoRa Driver, GPS Parser, and NeoPixel
RH_RF95 rf95(RFM95_CS, RFM95_INT);
TinyGPSPlus gps;
Adafruit_NeoPixel pixel(1, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800);

// Packet format sent to Base Station (MUST match Base Station exactly)
struct FieldPacket {
  uint32_t msgId;
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
uint32_t msgCounter = 1;               // Message counter for tracking lost packets
unsigned long lastTxTime = 0;
const unsigned long TX_INTERVAL = 5000; // Send packet every 5 seconds
bool sdWorking = false;

// Function prototype for NeoPixel control
void setFieldNeoPixel(uint8_t colorState);

void setup() {
  // Pre-deselect SPI Chip Select pins to avoid SPI bus collision
  pinMode(RFM95_CS, OUTPUT); digitalWrite(RFM95_CS, HIGH);
  pinMode(SD_CS, OUTPUT);    digitalWrite(SD_CS, HIGH);

  Serial.begin(115200);

  // Initialize Onboard NeoPixel
  pixel.begin();
  pixel.setBrightness(30);
  setFieldNeoPixel(0); // Off initially

  // Initialize Serial1 for GPS Receiver
  Serial1.begin(GPS_BAUD);

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
    rf95.setTxPower(23, false); // Max power output (23 dBm)
  }

  // Initialize SD Card Logging
  if (!SD.begin(SD_CS)) {
    Serial.println("Warning: SD Card initialization failed!");
  } else {
    sdWorking = true;
    Serial.println("SD Card Initialized.");
    File logFile = SD.open("FIELDLOG.CSV", FILE_WRITE);
    if (logFile) {
      logFile.println("MsgID,Lat,Lon,Alt,VBat,BaseAckReceived,BaseRSSI,RequestedLED");
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

  // 2. Transmit telemetry packet every TX_INTERVAL milliseconds
  if (millis() - lastTxTime >= TX_INTERVAL) {
    lastTxTime = millis();
    sendTelemetryPacket();
  }
}

void sendTelemetryPacket() {
  FieldPacket packet;
  packet.msgId = msgCounter++; // Increments ID on every transmission
  packet.vbat = analogRead(VBAT_PIN) * 2.0 * 3.3 / 1024.0;

  // Extract valid GPS coordinates if satellite lock is acquired
  if (gps.location.isValid()) {
    packet.latitude  = (float)gps.location.lat();
    packet.longitude = (float)gps.location.lng();
    packet.altitude  = (float)gps.altitude.meters();
  } else {
    packet.latitude  = 0.0;
    packet.longitude = 0.0;
    packet.altitude  = 0.0;
  }

  Serial.print("Sending Pkt #"); Serial.print(packet.msgId);
  Serial.print(" | Lat: "); Serial.print(packet.latitude, 6);
  Serial.print(" | Lon: "); Serial.print(packet.longitude, 6);
  Serial.print(" | Bat: "); Serial.print(packet.vbat); Serial.println("V");

  // 1. Send packet over LoRa
  rf95.send((uint8_t*)&packet, sizeof(packet));

  // 2. Wait until transmission physically completes
  rf95.waitPacketSent();

  // 3. Open a clean 2-second window to listen for the Base Station ACK
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

        // Update physical NeoPixel color based on command from Base Station
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

// Helper function to update physical onboard RGB NeoPixel
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
    default: // Off / Default White flash
      pixel.setPixelColor(0, pixel.Color(0, 0, 0));
      break;
  }
  pixel.show();
}
