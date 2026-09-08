## Telemetry Hardware Components

### Base Station
* **Microcontroller:** Adafruit Feather RP2040 with RFM95 LoRa Radio (915 MHz)
* **Display:** Adafruit OLED FeatherWing (128x64, SH1107 Driver Chip)
* **Datalogger:** Adafruit Adalogger FeatherWing (MicroSD Card Slot + PCF8523 Real-Time Clock)
* **Power:** 3.7V LiPo Battery or USB-C Power

### Field Station
* **Microcontroller:** Adafruit Feather RP2040 with RFM95 LoRa Radio (915 MHz)
* **GPS Receiver:** Adafruit Ultimate GPS FeatherWing (or equivalent NMEA-compatible GPS module via Serial1)
* **Datalogger:** Adafruit Adalogger FeatherWing (MicroSD Card Slot)
* **Visual Indicator:** Onboard RGB NeoPixel LED
* **Power:** 3.7V LiPo Battery

---

## Required Libraries

To compile and upload the firmware for both stations using the Arduino IDE, install the following libraries via the **Library Manager** (`Ctrl+Shift+I` or `Cmd+Shift+I`):

| Library Name | Version / Author | Usage |
| :--- | :--- | :--- |
| **RadioHead** | by Mike McCauley | Low-level driver for the RFM95 LoRa Transceiver (`RH_RF95.h`) |
| **Adafruit SH110X** | by Adafruit | Graphics and driver library for the SH1107 128x64 OLED display |
| **Adafruit GFX Library** | by Adafruit | Core graphics library required by Adafruit display drivers |
| **TinyGPS++** | by Mikal Hart | NMEA sentence parsing for latitude, longitude, altitude, and fix status |
| **RTClib** | by Adafruit | Interface driver for the PCF8523 Real-Time Clock on the Adalogger |
| **Adafruit NeoPixel** | by Adafruit | Controls the onboard RGB LED status indicator |
| **SD** | Built-in / Arduino | Hardware SPI interface for reading/writing CSV log files to MicroSD cards |
| **Wire & SPI** | Built-in / Arduino | Hardware buses for I2C (OLED/RTC) and SPI (LoRa/SD) communications |

---

## CSV Data Logging Output

Both units log telemetry data locally to MicroSD cards upon every successful transmission cycle.

* **`BASELOG.CSV`** (Base Station):
  `Timestamp, FieldPacketID, Detected, RSSI, BaseBattVolt, FieldLat, FieldLon, FieldAlt, FieldBattVolt, SelectedLED`
* **`FIELDLOG.CSV`** (Field Station):
  `MsgID, Lat, Lon, Alt, VBat, BaseAckReceived, BaseRSSI, RequestedLED`
