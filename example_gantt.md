```mermaid
gantt
    title All-Hands Science Timeline
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

section Logistics and Milestones
    Thanksgiving 2026: milestone, 2026-11-26, 1d

section ISAC Team
    %% Logistics and Milestones (Include academic calendar and OOF notices; merge these into one logistics section when combining multiple Gantt charts)
        BK OOF: done, bkoof1, 2026-06-01, 5d %% Ben will be in DC.
        KC OOF: done, kcoff1, 2026-07-10, 8d %% Kristina will be in MI.

    %% Telemetry
        Purchase LoRaWAN parts:done, lorabuy, 2026-05-15, 5d
        Verify LoRa bidirectional comms:done, lora_initial_test, 2026-05-15, 2d
        Verify LoRa GPS: lora_gps, 2026-06-01, 15d
        Actuate motor via LoRa:active,  loraactuate, after lora_initial_test, 4d 
        Validate LoRa GPS: lora_gps_test, after lora_gps, 10d

    %% Backscatter Sensors
        Redesign OBS Head: done, obshead, 2026-05-20, 4d
        OBS Board Revision: obsboard, 2026-06-01, 20d
        Build OBS Sensors: obsbuild, after obsboard, 10d

    %% Chla Sensors
        Chla Sensor Selection: done, chlaselect, 2026-05-01, 10d
        Chla Sensor Bench Testing and Programming: active, chla_sensor_test, 2026-05-20, 20d
        Chla Sensor Build: chlabuild, after chla_sensor_test, 30d
        Lake Test for OBS and Chla sensors: laketst, after obsbuild chlabuild, 4d

    %% Ash Dispersal
        Design fixture for ash dispersal: done, fixturedesign, 2026-05-10, 5d
        Ruggedize fixture: ruggedize, 2026-06-05, 3d
        Implement radio control of ash dispersal: radiofixture, after loraactuate ruggedize, 4d
        Identify and order float for ash dispersal system:active, floatfind, 2026-06-01, 2d
        Build ash dispersal float: floatbuild, after floatfind radiofixture, 7d
        Ash dispersal system pool test: ashpool, after floatbuild, 1d

    %% ISAC Column Build and Test
        Buy webbing for column: webbing, 2026-06-05, 1d
        Sew duplicate column: sew, after webbing, 2d
        Lake test of column with ash dispersal float: floattest, after ashpool sew, 2d
        Write procedure checklist for sensors: sensorprotocol, after chlabuild obsbuild, 2d
        Write procedure checklist for ISAC deployment: protocol, after floattest, 2d
        Pack and Ship Sensors to CA: sensorship, after laketest sensorprotocol, 15d
        Pack and Ship Ash Dispersal System to CA: ashship, after floattest, 15d
