```mermaid
---
displayMode: compact
---
gantt
    title Year 1
    dateFormat  YYYY-MM-DD

section Natural Variables
    Nitrogen Fixation Annual Cycle: N, 2026-11-01, 150d
    Whale Season: whales, 2026-07-01, 90d

section Meetings and Conferences
    All-Hands Science Discussion: active, milestone, 2026-07-01, 1d
    All-Hands Science Data Forum: milestone, 2026-10-15, 1d
    AGU26, San Francisco: 2026-12-07, 5d

section Field Work
    November cruise (tentative): 2026-11-10, 10d
    January cruise: 2027-01-08, 10d
    March cruise: 2027-03-15, 10d

section Publishing and Reports
    Milestone A : milestone, m1, 2026-06-10, 1d
    Milestone B : milestone, sameline m1, 2026-07-12, 1d
    Milestone C : milestone, sameline m1, 2026-09-15, 1d

section Telemetry
        Purchase LoRaWAN parts:done, lorabuy, 2026-05-15, 5d
        Verify LoRa bidirectional comms:done, lora_initial_test, 2026-05-15, 2d
        Verify LoRa GPS: lora_gps, 2026-06-01, 15d
        Actuate motor via LoRa:active,  loraactuate, after lora_initial_test, 4d 
        Validate LoRa GPS: lora_gps_test, after lora_gps, 10d

section Backscatter Sensors
        Redesign OBS Head: done, obshead, 2026-05-20, 4d
        OBS Board Revision: obsboard, 2026-06-01, 20d
        Build OBS Sensors: obsbuild, after obsboard, 10d

section Chla Sensors
        Chla Sensor Selection: done, chlaselect, 2026-05-01, 10d
        Chla Sensor Bench Testing and Programming: active, chla_sensor_test, 2026-05-20, 20d
        Chla Sensor Build: chlabuild, after chla_sensor_test, 30d
        Lake Test for OBS and Chla sensors: laketst, after obsbuild chlabuild, 4d

section Ash Dispersal
        Design fixture for ash dispersal: done, fixturedesign, 2026-05-10, 5d
        Ruggedize fixture: ruggedize, 2026-06-05, 3d
        Implement radio control of ash dispersal: radiofixture, after loraactuate ruggedize, 4d
        Identify and order float for ash dispersal system:crit, floatfind, 2026-06-01, 2d
        Build ash dispersal float: floatbuild, after floatfind radiofixture, 7d
        Ash dispersal system pool test: ashpool, after floatbuild, 1d

section ISAC Column Build and Test
        Buy webbing for column: webbing, 2026-06-05, 1d
        Sew duplicate column: sew, after webbing, 2d
        Lake test of column with ash dispersal float: floattest, after ashpool sew, 2d
        Write procedure checklist for sensors: sensorprotocol, after chlabuild obsbuild, 2d
        Write procedure checklist for ISAC deployment: protocol, after floattest, 2d
        Pack and Ship Sensors to CA: sensorship, after laketest sensorprotocol, 15d
        Pack and Ship Ash Dispersal System to CA: ashship, after floattest, 15d
```
```mermaid
---
displayMode: compact
---
gantt
    dateFormat  YYYY-MM-DD
    axisFormat  %d
    title Compact Mode Example
    Requirements  :a1, 2021-01-11, 4w
    Design  :a2, after a1, 4w
    Development  :a3, after a2, 4w
    Test  :a4, after a3, 4w
    Demo  :crit, a5, after a4, 1d
```

```mermaid
timeline
    title Repeated Events
    Period A : Event 1 : Event 2 : Event 3
    Period B : Event 1 : Event 2
```
