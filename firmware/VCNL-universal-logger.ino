#include <Wire.h>
#include <SPI.h>
#include <EEPROM.h>
#include <SdFat.h>

#define SD_CS_PIN 10
#define SN_ADDR 500

SdFat sd;
File csvFile;

enum VcnlModel { MODEL_NONE = 0, MODEL_VCNL4010 = 1, MODEL_VCNL4020 = 2, MODEL_VCNL4030 = 3, MODEL_VCNL4040 = 4 };
VcnlModel detectedModel = MODEL_NONE;
const char* modelNames[] = { "NONE", "VCNL4010", "VCNL4020", "VCNL4030", "VCNL4040" };

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

void writeRegister(uint8_t address, uint8_t reg, uint8_t val) {
  Wire.beginTransmission(address);
  Wire.write(reg);
  Wire.write(val);
  Wire.endTransmission();
}

uint16_t readRegister16(uint8_t address, uint8_t reg) {
  Wire.beginTransmission(address);
  Wire.write(reg);
  Wire.endTransmission(false);
  Wire.requestFrom(address, (uint8_t)2);
  if (Wire.available() >= 2) {
    uint8_t low = Wire.read();
    uint8_t high = Wire.read();
    return (high << 8) | low;
  }
  return 0;
}

void detectAndInitSensor() {
  // Probe I2C Address 0x13 (VCNL4010 / VCNL4020)
  Wire.beginTransmission(0x13);
  if (Wire.endTransmission() == 0) {
    uint16_t prodId = readRegister16(0x13, 0x81) & 0xFF;
    if (prodId == 0x21) {
      detectedModel = MODEL_VCNL4010;
    } else {
      detectedModel = MODEL_VCNL4020;
    }
    writeRegister(0x13, 0x83, 20); // 200mA LED current
    writeRegister(0x13, 0x80, 0x07); // Periodic measurement mode
    return;
  }

  // Probe I2C Address 0x60 (VCNL4030 / VCNL4040)
  Wire.beginTransmission(0x60);
  if (Wire.endTransmission() == 0) {
    uint16_t idReg = readRegister16(0x60, 0x0C) & 0xFF;
    if (idReg == 0x86) {
      detectedModel = MODEL_VCNL4040;
    } else {
      detectedModel = MODEL_VCNL4030;
    }
    writeRegister(0x60, 0x03, 0x00); // Clear PS shutdown
    writeRegister(0x60, 0x00, 0x00); // Clear ALS shutdown
    return;
  }

  detectedModel = MODEL_NONE;
}

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
}

void sendDeviceInfo() {
  // Format: ACK_SN:<SN>,<MODEL_NAME>
  Serial.print(F("ACK_SN:"));
  Serial.print(serialNumber);
  Serial.print(F(","));
  Serial.println(modelNames[detectedModel]);
  Serial.flush();
}

void createLogFile() {
  uint16_t fileIndex = 1;
  uint8_t sensorCode = (uint8_t)detectedModel;

  // Format: <SN>_<SensorCode><Index>.CSV (e.g., 150_3001.CSV)
  while (fileIndex < 999) {
    snprintf(filename, sizeof(filename), "%u_%u%03u.CSV", serialNumber, sensorCode, fileIndex);
    if (!sd.exists(filename)) {
      break;
    }
    fileIndex++;
  }

  if (csvFile.open(filename, FILE_WRITE)) {
    if (csvFile.fileSize() == 0) {
      if (detectedModel == MODEL_VCNL4010 || detectedModel == MODEL_VCNL4020) {
        csvFile.println(F("timestamp,millis,sensor_type,proximity,ambient_light"));
      } else {
        csvFile.println(F("timestamp,millis,sensor_type,proximity,lux,als,white"));
      }
      csvFile.flush();
    }
    fileCreated = true;

    // Send ACK with Epoch, Filename, and Sensor Type
    Serial.print(F("ACK_SYNC:"));
    Serial.print(startEpoch);
    Serial.print(F(","));
    Serial.print(filename);
    Serial.print(F(","));
    Serial.println(modelNames[detectedModel]);
    Serial.flush();
  } else {
    Serial.println(F("MSG: SD File Creation Failed!"));
    Serial.flush();
  }
}

void processCommand(char* input) {
  if (input[0] == 'T') {
    uint32_t p_epoch = strtoul(input + 1, NULL, 10);
    if (p_epoch > 1000000000UL) {
      startEpoch = p_epoch;
      startMillis = millis();
      timeSynced = true;
      createLogFile();
    }
  } else if (strcmp(input, "GET_SN") == 0) {
    sendDeviceInfo();
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
  detectAndInitSensor();

  if (!sd.begin(SD_CS_PIN, SD_SCK_MHZ(4))) {
    Serial.println(F("MSG: SD Card Init Failed!"));
  } else {
    Serial.println(F("MSG: SD Card Initialized.")); // Fixed: Uses println
  }

  sendDeviceInfo();
}

void loop() {
  checkSerialInput();

  unsigned long currentMillis = millis();
  if (currentMillis - lastLogTime >= LOG_INTERVAL) {
    lastLogTime = currentMillis;

    uint32_t currentEpoch = timeSynced ? (startEpoch + ((currentMillis - startMillis) / 1000)) : 0;

    uint16_t prox = 0, ch2 = 0, als = 0, white = 0;

    if (detectedModel == MODEL_VCNL4010 || detectedModel == MODEL_VCNL4020) {
      prox = readRegister16(0x13, 0x87);
      ch2  = readRegister16(0x13, 0x85);
    } else if (detectedModel == MODEL_VCNL4030) {
      prox = readRegister16(0x60, 0x08);
      ch2  = readRegister16(0x60, 0x0B);
      als  = readRegister16(0x60, 0x0A);
      white= readRegister16(0x60, 0x0C);
    } else if (detectedModel == MODEL_VCNL4040) {
      prox = readRegister16(0x60, 0x08);
      ch2  = readRegister16(0x60, 0x09);
      white= readRegister16(0x60, 0x0A);
    }

    // Stream CSV over Serial
    Serial.print(currentEpoch); Serial.print(',');
    Serial.print(currentMillis); Serial.print(',');
    Serial.print(modelNames[detectedModel]); Serial.print(',');
    Serial.print(prox); Serial.print(',');
    if (detectedModel == MODEL_VCNL4010 || detectedModel == MODEL_VCNL4020) {
      Serial.println(ch2);
    } else {
      Serial.print(ch2); Serial.print(',');
      Serial.print(als); Serial.print(',');
      Serial.println(white);
    }

    // Write CSV row to SD card file
    if (fileCreated && csvFile.isOpen()) {
      csvFile.print(currentEpoch); csvFile.print(',');
      csvFile.print(currentMillis); csvFile.print(',');
      csvFile.print(modelNames[detectedModel]); csvFile.print(',');
      csvFile.print(prox); csvFile.print(',');
      if (detectedModel == MODEL_VCNL4010 || detectedModel == MODEL_VCNL4020) {
        csvFile.println(ch2);
      } else {
        csvFile.print(ch2); csvFile.print(',');
        csvFile.print(als); csvFile.print(',');
        csvFile.println(white);
      }
      
      static byte flushCounter = 0;
      if (++flushCounter >= 10) {
        csvFile.flush();
        flushCounter = 0;
      }
    }
  }
}
