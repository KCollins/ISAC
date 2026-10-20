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

const char modelName0[] PROGMEM = "NONE";
const char modelName1[] PROGMEM = "VCNL4010";
const char modelName2[] PROGMEM = "VCNL4020";
const char modelName3[] PROGMEM = "VCNL4030";
const char modelName4[] PROGMEM = "VCNL4040";
const char* const modelNames[] PROGMEM = { modelName0, modelName1, modelName2, modelName3, modelName4 };

uint16_t serialNumber = 0;
uint32_t startEpoch = 0;
unsigned long startMillis = 0;
bool timeSynced = false;
volatile bool fileCreated = false;
char filename[13]; // Strictly 8.3 FAT filename buffer

unsigned long lastLogTime = 0;
unsigned long LOG_INTERVAL = 100; // Logging cadence in ms (default: 100 ms = 10 Hz)
uint16_t currentLedMa = 50;        // IR LED current in mA (default: 50 mA)
char deploymentNotes[80] = "";     // Buffer for optional notes

char rxBuffer[128]; // Expanded buffer for serial commands and note text
byte rxIndex = 0;

// Explicit Forward Function Prototypes
void writeRegister8(uint8_t address, uint8_t reg, uint8_t val);
void writeRegister16(uint8_t address, uint8_t reg, uint16_t val);
uint8_t readRegister8(uint8_t address, uint8_t reg);
uint16_t readRegister16_BE(uint8_t address, uint8_t reg);
uint16_t readRegister16_LE(uint8_t address, uint8_t reg);
void readVcnl4010OpenObs(uint16_t &proxOut, uint16_t &alsOut);
void applyLedCurrent(uint16_t mA);

void getModelName(uint8_t index, char* outBuf) {
  strcpy_P(outBuf, (char*)pgm_read_word(&(modelNames[index])));
}

void writeRegister8(uint8_t address, uint8_t reg, uint8_t val) {
  Wire.beginTransmission(address);
  Wire.write(reg);
  Wire.write(val);
  Wire.endTransmission();
}

void writeRegister16(uint8_t address, uint8_t reg, uint16_t val) {
  Wire.beginTransmission(address);
  Wire.write(reg);
  Wire.write(val & 0xFF);
  Wire.write((val >> 8) & 0xFF);
  Wire.endTransmission();
}

uint8_t readRegister8(uint8_t address, uint8_t reg) {
  Wire.beginTransmission(address);
  Wire.write(reg);
  if (Wire.endTransmission(true) != 0) return 0;
  Wire.requestFrom(address, (uint8_t)1);
  if (Wire.available() >= 1) {
    return Wire.read();
  }
  return 0;
}

// Big-Endian 16-Bit Read (VCNL4010 / VCNL4020)
uint16_t readRegister16_BE(uint8_t address, uint8_t reg) {
  Wire.beginTransmission(address);
  Wire.write(reg);
  if (Wire.endTransmission(true) != 0) return 0;
  Wire.requestFrom(address, (uint8_t)2);
  if (Wire.available() >= 2) {
    uint8_t high = Wire.read();
    uint8_t low  = Wire.read();
    return ((uint16_t)high << 8) | low;
  }
  return 0;
}

// Little-Endian 16-Bit Read (VCNL4030 / VCNL4040)
uint16_t readRegister16_LE(uint8_t address, uint8_t reg) {
  Wire.beginTransmission(address);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return 0;
  Wire.requestFrom(address, (uint8_t)2);
  if (Wire.available() >= 2) {
    uint8_t low  = Wire.read();
    uint8_t high = Wire.read();
    return ((uint16_t)high << 8) | low;
  }
  return 0;
}

void applyLedCurrent(uint16_t mA) {
  currentLedMa = mA;
  if (detectedModel == MODEL_VCNL4010 || detectedModel == MODEL_VCNL4020) {
    // VCNL4010/4020 Register 0x83: Current in 10 mA steps (0 to 20 -> 0 to 200 mA)
    uint8_t val = mA / 10;
    if (val > 20) val = 20;
    writeRegister8(0x13, 0x83, val);
  } else if (detectedModel == MODEL_VCNL4030 || detectedModel == MODEL_VCNL4040) {
    // VCNL4030/4040 Register 0x03 PS_MS: Bits [2:0] set LED_I
    uint8_t code = 0;
    if (mA >= 200) code = 7;
    else if (mA >= 180) code = 6;
    else if (mA >= 160) code = 5;
    else if (mA >= 140) code = 4;
    else if (mA >= 120) code = 3;
    else if (mA >= 100) code = 2;
    else if (mA >= 70)  code = 1;
    else code = 0; // 50 mA default

    uint16_t currentReg = readRegister16_LE(0x60, 0x03);
    currentReg = (currentReg & ~0x0007) | code;
    writeRegister16(0x60, 0x03, currentReg);
  }
}

// OpenOBS Sequential Measurement Routine for VCNL4010 / VCNL4020
void readVcnl4010OpenObs(uint16_t &proxOut, uint16_t &alsOut) {
  // STEP 1: Ambient Light Measurement (ALS)
  writeRegister8(0x13, 0x80, 0x10);
  for (uint8_t i = 0; i < 200; i++) {
    delayMicroseconds(100);
    if (readRegister8(0x13, 0x80) & 0x40) {
      break;
    }
  }
  alsOut = readRegister16_BE(0x13, 0x85);

  // STEP 2: Photodiode Settling delay
  delayMicroseconds(2000);

  // STEP 3: Backscatter / Proximity Measurement
  writeRegister8(0x13, 0x80, 0x08);
  for (uint8_t i = 0; i < 200; i++) {
    delayMicroseconds(100);
    if (readRegister8(0x13, 0x80) & 0x20) {
      break;
    }
  }
  proxOut = readRegister16_BE(0x13, 0x87);
}

void detectAndInitSensor() {
  // 1. Probe I2C Address 0x13 (VCNL4010 / VCNL4020)
  Wire.beginTransmission(0x13);
  if (Wire.endTransmission() == 0) {
    uint8_t prodRev  = readRegister8(0x13, 0x81);
    uint8_t alsParam = readRegister8(0x13, 0x84);

    if (prodRev == 0x22 || (prodRev == 0x21 && alsParam != 0x9D)) {
      detectedModel = MODEL_VCNL4020;
    } else {
      detectedModel = MODEL_VCNL4010;
    }

    writeRegister8(0x13, 0x80, 0x00); // Clear command register
    writeRegister8(0x13, 0x82, 0x02); // Proximity Rate: 14.14 measurements/s
    applyLedCurrent(currentLedMa);     // Default LED Current (50 mA)
    writeRegister8(0x13, 0x84, 0x9D); // Fast ALS parameters
    return;
  }

  // 2. Probe I2C Address 0x60 (VCNL4030 / VCNL4040)
  Wire.beginTransmission(0x60);
  if (Wire.endTransmission() == 0) {
    uint16_t idCheck = readRegister16_LE(0x60, 0x0C);
    
    if ((idCheck & 0xFF) == 0x86) {
      detectedModel = MODEL_VCNL4040;
    } else {
      detectedModel = MODEL_VCNL4030;
    }

    writeRegister16(0x60, 0x03, 0x0000); // Power on Proximity
    writeRegister16(0x60, 0x00, 0x0000); // Power on ALS + White
    applyLedCurrent(currentLedMa);     // Default LED Current (50 mA)
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
  char modelStr[12];
  getModelName(detectedModel, modelStr);

  Serial.print(F("ACK_SN:"));
  Serial.print(serialNumber);
  Serial.print(F(","));
  Serial.println(modelStr);
  Serial.flush();
}

void createLogFile() {
  fileCreated = false;
  if (csvFile.isOpen()) {
    csvFile.close();
  }

  uint16_t fileIndex = 1;
  uint8_t sensorCode = (uint8_t)detectedModel;

  while (fileIndex < 999) {
    if (serialNumber < 1000) {
      snprintf(filename, sizeof(filename), "%u_%u%03u.CSV", serialNumber, sensorCode, fileIndex);
    } else if (serialNumber < 10000) {
      snprintf(filename, sizeof(filename), "%u_%u%02u.CSV", serialNumber, sensorCode, fileIndex);
    } else {
      snprintf(filename, sizeof(filename), "%u_%u%01u.CSV", serialNumber, sensorCode, fileIndex);
    }

    if (!sd.exists(filename)) {
      break;
    }
    fileIndex++;
  }

  Serial.print(F("MSG: Target filename -> "));
  Serial.println(filename);
  Serial.flush();

  if (csvFile.open(filename, FILE_WRITE)) {
    if (csvFile.fileSize() == 0) {
      char modelStr[12];
      getModelName(detectedModel, modelStr);

      // Commented Header Section
      csvFile.print(F("# OpenOBS Log File - Serial Number: "));
      csvFile.println(serialNumber);
      csvFile.print(F("# Sensor Model: "));
      csvFile.println(modelStr);
      csvFile.print(F("# Cadence: "));
      csvFile.print(LOG_INTERVAL);
      csvFile.println(F(" ms"));
      csvFile.print(F("# LED Current: "));
      csvFile.print(currentLedMa);
      csvFile.println(F(" mA"));
      if (strlen(deploymentNotes) > 0) {
        csvFile.print(F("# Notes: "));
        csvFile.println(deploymentNotes);
      }

      // CSV Data Column Headers
      if (detectedModel == MODEL_VCNL4010 || detectedModel == MODEL_VCNL4020) {
        csvFile.println(F("timestamp,millis,sensor_type,proximity,ambient_light"));
      } else {
        csvFile.println(F("timestamp,millis,sensor_type,proximity,lux,als,white"));
      }
      csvFile.flush();
    }
    fileCreated = true;

    char modelStr[12];
    getModelName(detectedModel, modelStr);

    Serial.print(F("ACK_SYNC:"));
    Serial.print(startEpoch);
    Serial.print(F(","));
    Serial.print(filename);
    Serial.print(F(","));
    Serial.println(modelStr);
    Serial.flush();
  } else {
    Serial.println(F("MSG: SD File Creation Failed!"));
    Serial.flush();
  }
}

void processCommand(char* input) {
  while (*input == ' ' || *input == '\t' || *input == '\r' || *input == '\n') {
    input++;
  }

  Serial.print(F("MSG: Received input -> "));
  Serial.println(input);
  Serial.flush();

  if (input[0] == 'T') {
    uint32_t p_epoch = strtoul(input + 1, NULL, 10);
    if (p_epoch > 1000000000UL) {
      startEpoch = p_epoch;
      startMillis = millis();
      lastLogTime = startMillis;
      timeSynced = true;
      createLogFile();
    } else {
      Serial.println(F("MSG: Invalid epoch value received."));
      Serial.flush();
    }
  } else if (strncmp(input, "NOTE:", 5) == 0) {
    strncpy(deploymentNotes, input + 5, sizeof(deploymentNotes) - 1);
    deploymentNotes[sizeof(deploymentNotes) - 1] = '\0';
    Serial.println(F("ACK_NOTE:OK"));
    Serial.flush();
  } else if (strncmp(input, "SET_CFG:", 8) == 0) {
    // Format: SET_CFG:<interval_ms>,<led_mA>
    char* token = strtok(input + 8, ",");
    if (token != NULL) {
      unsigned long interval = strtoul(token, NULL, 10);
      if (interval >= 20 && interval <= 60000) {
        LOG_INTERVAL = interval;
      }
      token = strtok(NULL, ",");
      if (token != NULL) {
        uint16_t ledMa = atoi(token);
        if (ledMa > 0 && ledMa <= 200) {
          applyLedCurrent(ledMa);
        }
      }
    }
    Serial.print(F("ACK_CFG:"));
    Serial.print(LOG_INTERVAL);
    Serial.print(F(","));
    Serial.println(currentLedMa);
    Serial.flush();
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
  
  #if defined(WIRE_HAS_TIMEOUT)
    Wire.setWireTimeout(3000, true);
  #endif

  getSerialNumber();
  detectAndInitSensor();

  if (!sd.begin(SD_CS_PIN, SD_SCK_MHZ(4))) {
    Serial.println(F("MSG: SD Card Init Failed!"));
  } else {
    Serial.println(F("MSG: SD Card Initialized."));
  }

  sendDeviceInfo();
}

void loop() {
  checkSerialInput();

  unsigned long currentMillis = millis();
  if (currentMillis - lastLogTime >= LOG_INTERVAL) {
    if (currentMillis - lastLogTime > 5 * LOG_INTERVAL) {
      lastLogTime = currentMillis;
    } else {
      lastLogTime += LOG_INTERVAL;
    }

    double currentTimestamp = 0.0;
    if (timeSynced) {
      unsigned long elapsed = currentMillis - startMillis;
      currentTimestamp = (double)startEpoch + ((double)elapsed / 1000.0);
    }

    uint16_t prox = 0, als = 0, white = 0;
    float lux = 0.0;

    if (detectedModel == MODEL_VCNL4010 || detectedModel == MODEL_VCNL4020) {
      readVcnl4010OpenObs(prox, als);
      lux = als;
    } else if (detectedModel == MODEL_VCNL4030) {
      prox  = readRegister16_LE(0x60, 0x08);
      als   = readRegister16_LE(0x60, 0x0B);
      white = readRegister16_LE(0x60, 0x0C);
      lux   = als * 0.0536;
    } else if (detectedModel == MODEL_VCNL4040) {
      prox  = readRegister16_LE(0x60, 0x08);
      als   = readRegister16_LE(0x60, 0x09);
      white = readRegister16_LE(0x60, 0x0A);
      lux   = als * 0.1000;
    }

    char modelStr[12];
    getModelName(detectedModel, modelStr);

    // Stream CSV line over Serial
    if (timeSynced) {
      Serial.print(currentTimestamp, 3);
    } else {
      Serial.print(0);
    }
    Serial.print(',');
    Serial.print(currentMillis);
    Serial.print(',');
    Serial.print(modelStr);
    Serial.print(',');
    Serial.print(prox);
    Serial.print(',');
    if (detectedModel == MODEL_VCNL4010 || detectedModel == MODEL_VCNL4020) {
      Serial.println(als);
    } else {
      Serial.print(lux, 2); Serial.print(',');
      Serial.print(als); Serial.print(',');
      Serial.println(white);
    }

    // Write to SD file
    if (fileCreated && csvFile.isOpen()) {
      if (timeSynced) {
        csvFile.print(currentTimestamp, 3);
      } else {
        csvFile.print(0);
      }
      csvFile.print(',');
      csvFile.print(currentMillis);
      csvFile.print(',');
      csvFile.print(modelStr);
      csvFile.print(',');
      csvFile.print(prox);
      csvFile.print(',');
      if (detectedModel == MODEL_VCNL4010 || detectedModel == MODEL_VCNL4020) {
        csvFile.println(als);
      } else {
        csvFile.print(lux, 2);
        csvFile.print(',');
        csvFile.print(als);
        csvFile.print(',');
        csvFile.println(white);
      }
      
      // Flush every 200 cycles (~20 seconds at 10 Hz)
      static uint16_t flushCounter = 0;
      if (++flushCounter >= 200) {
        csvFile.flush();
        flushCounter = 0;
      }
    }
  }
}
