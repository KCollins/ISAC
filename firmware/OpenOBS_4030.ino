#include <Wire.h>
#include <SPI.h>
#include <EEPROM.h>
#include <SdFat.h>
#include <Adafruit_VCNL4030.h>

#define SD_CS_PIN 10
#define SN_ADDR 500

SdFat sd;
File csvFile;
Adafruit_VCNL4030 vcnl;

uint16_t serialNumber = 0;
uint32_t startEpoch = 0;
unsigned long startMillis = 0;
bool timeSynced = false;
bool fileCreated = false;
char filename[16];

unsigned long lastLogTime = 0;
const unsigned long LOG_INTERVAL = 500; // 0.5s cadence

char rxBuffer[32];
byte rxIndex = 0;

void getSerialNumber() {
  EEPROM.get(SN_ADDR, serialNumber);
  if (serialNumber == 0xFFFF || serialNumber == 0) {
    Serial.println(F("MSG: Missing SN. Send 'SN=<number>':"));
    while (serialNumber == 0xFFFF || serialNumber == 0) {
      if (Serial.available() > 0) {
        String input = Serial.readStringUntil('\n');
        input.trim();
        if (input.startsWith("SN=")) input = input.substring(3);
        uint16_t tmp_SN = input.toInt();
        if (tmp_SN > 0 && tmp_SN < 65535) {
          EEPROM.put(SN_ADDR, tmp_SN);
          EEPROM.get(SN_ADDR, serialNumber);
        }
      }
    }
  }
  Serial.print(F("ACK_SN:"));
  Serial.println(serialNumber);
  Serial.flush();
}

void createLogFile() {
  uint16_t fileIndex = 1;
  while (fileIndex < 9999) {
    if (serialNumber < 1000) {
      snprintf(filename, sizeof(filename), "%u_%04u.CSV", serialNumber, fileIndex);
    } else if (serialNumber < 10000) {
      snprintf(filename, sizeof(filename), "%u_%03u.CSV", serialNumber, fileIndex);
    } else {
      snprintf(filename, sizeof(filename), "%u_%02u.CSV", serialNumber, fileIndex);
    }

    if (!sd.exists(filename)) {
      break;
    }
    fileIndex++;
  }

  if (csvFile.open(filename, FILE_WRITE)) {
    if (csvFile.fileSize() == 0) {
      csvFile.println(F("timestamp,millis,proximity,lux,als,white"));
      csvFile.flush();
    }
    fileCreated = true;

    Serial.print(F("ACK_SYNC:"));
    Serial.print(startEpoch);
    Serial.print(F(","));
    Serial.println(filename);
    Serial.flush();
  } else {
    Serial.println(F("MSG: SD File Creation Failed!"));
    Serial.flush();
  }
}

void processCommand(char* input) {
  Serial.print(F("MSG: Received input -> "));
  Serial.println(input);
  Serial.flush();

  if (input[0] == 'T') {
    uint32_t p_epoch = strtoul(input + 1, NULL, 10);
    
    if (p_epoch > 1000000000UL) {
      startEpoch = p_epoch;
      startMillis = millis();
      timeSynced = true;

      createLogFile();
    } else {
      Serial.println(F("MSG: Invalid epoch value received."));
      Serial.flush();
    }
  } else if (strcmp(input, "GET_SN") == 0) {
    Serial.print(F("ACK_SN:"));
    Serial.println(serialNumber);
    Serial.flush();
  }
}

void checkSerialInput() {
  while (Serial.available() > 0) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (rxIndex > 0) {
        rxBuffer[rxIndex] = '\0';
        processCommand(rxBuffer);
        rxIndex = 0;
      }
    } else if (rxIndex < sizeof(rxBuffer) - 1) {
      rxBuffer[rxIndex++] = c;
    }
  }
}

void setup() {
  Serial.begin(57600);
  Wire.begin();

  getSerialNumber();

  if (!vcnl.begin()) {
    Serial.println(F("MSG: VCNL4030 NOT found!"));
  } else {
    Serial.println(F("MSG: VCNL4030 initialized."));
  }

  if (!sd.begin(SD_CS_PIN, SD_SCK_MHZ(4))) {
    Serial.println(F("MSG: SD Card Init Failed!"));
  } else {
    Serial.print(F("MSG: SD Card Initialized. FAT Type: FAT"));
    Serial.println(int(sd.fatType()));
  }

  Serial.println(F("MSG: Ready for Web Serial sync."));
  Serial.flush();
}

void loop() {
  checkSerialInput();

  unsigned long currentMillis = millis();
  if (currentMillis - lastLogTime >= LOG_INTERVAL) {
    lastLogTime = currentMillis;

    uint32_t currentEpoch = timeSynced ? (startEpoch + ((currentMillis - startMillis) / 1000)) : 0;

    uint16_t prox = vcnl.readProximity();
    float lux = vcnl.readLux();
    uint16_t als = vcnl.readALS();
    uint16_t white = vcnl.readWhite();

    // 1. Stream CSV row over Serial
    Serial.print(currentEpoch); Serial.print(',');
    Serial.print(currentMillis); Serial.print(',');
    Serial.print(prox);         Serial.print(',');
    Serial.print(lux, 2);       Serial.print(',');
    Serial.print(als);          Serial.print(',');
    Serial.println(white);

    // 2. Append CSV row to SD card file
    if (fileCreated && csvFile.isOpen()) {
      csvFile.print(currentEpoch); csvFile.print(',');
      csvFile.print(currentMillis); csvFile.print(',');
      csvFile.print(prox);         csvFile.print(','); // Fixed: csvFile.print
      csvFile.print(lux, 2);       csvFile.print(','); // Fixed: csvFile.print
      csvFile.print(als);          csvFile.print(','); // Fixed: csvFile.print
      csvFile.println(white);
      
      static byte flushCounter = 0;
      if (++flushCounter >= 10) { // Flush to card every 5s
        csvFile.flush();
        flushCounter = 0;
      }
    }
  }
}
