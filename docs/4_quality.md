---
layout: page
title: Quality, Calibration and Test Procedures
permalink: /testing/
mermaid: true
---

This page concerns quality, calibration and test procedures for subsystems. 
*Like ISAC, this page is a work in progress.*


## OpenOBS Sensors
### QA/QC
Some of the quality issues we encountered with the OpenOBS sensors on our first deployment are reflected in the Ishikawa diagram below. 
```mermaid
ishikawa-beta
    OpenOBS-328 Failures
    Personnel
        Incomplete deployment protocol
    Machines
        Printer settings
        Epoxy protocol
    Materials
        Sensor heads
            Use of PLA (should be using PETG)
        Sensor endcaps
            Automotive cap leaks (should use threaded inserts)
    Methods
        Uneven PVC cuts
            Should brace PVC to ensure even cuts
        Uneven mating of sensor heads
            Should tape around sensor head
    Measurements
        Uncalibrated sensors
    Environment
        Saltwater infiltration     
```
