# MeeBlueReader
Tryout project for reading RSSI signals from Meeblue beacons

## Overview
MeeBlueReader is a Qt6 QML application that continuously scans for Bluetooth Low Energy beacons and estimates their distance based on RSSI (Received Signal Strength Indicator) values.

## Features
- **Continuous BLE Scanning**: Scans for Bluetooth devices every 500ms
- **Device Filtering**: Detects devices with "meeblue" in their name or specific MAC addresses
- **Distance Estimation**: Calculates approximate distance using log-distance path loss model
- **Real-time Display**: Shows beacon information in the format: `{address} - {rssi} dB - {distance} m`

## Monitored Devices
The application monitors the following device addresses:
- DD:2B:7C:C0:A0:84
- EB:3B:E8:48:F4:90

## Distance Calculation
Distance is estimated using the log-distance path loss model:
```
distance = 10^((TX_POWER - rssi) / (10 * N))
```
Where:
- TX_POWER = -40 dBm (measured power at 1 meter)
- N = 2.0 (environmental attenuation factor)
- rssi = measured signal strength

## Building
Requirements:
- Qt 6.5 or later
- Qt Bluetooth module
- CMake 3.16 or later

```bash
mkdir build
cd build
cmake ..
cmake --build .
```

## Implementation
The core functionality is implemented in the `MeeBlueReader` class:
- **meebluereader.h**: Class definition with Qt properties and signals
- **meebluereader.cpp**: Implementation of BLE scanning and distance calculation
- **Main.qml**: QML UI for displaying beacon information
- **main.cpp**: Application entry point and MeeBlueReader initialization
