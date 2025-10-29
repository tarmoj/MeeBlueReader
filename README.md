# MeeBlueReader
Tryout project for reading RSSI signals from Meeblue beacons

## Overview
MeeBlueReader is a Qt6 QML application that continuously scans for Bluetooth Low Energy beacons and estimates their distance based on RSSI (Received Signal Strength Indicator) values.

## Features
- **Continuous BLE Scanning**: Scans for Bluetooth devices every 5 seconds
- **Dual Scanner Support**: Uses native Android BLE scanner or Qt BLE scanner
- **Device Filtering**: Detects devices with "meeblue" in their name or specific MAC addresses
- **Distance Estimation**: Calculates approximate distance using log-distance path loss model
- **Real-time Display**: Shows beacon information in the format: `{address} - {rssi} dB - {distance} m`

## Bluetooth Scanning Methods

### Native Android Scanner (Default on Android)
The app uses Android's native `BluetoothLeScanner` API via JNI for better compatibility with various BLE beacons, including MeeBlue M52-SA beacons. This provides:
- Low-latency scanning mode
- Better beacon detection rates
- Immediate advertisement reporting
- Superior compatibility with iBeacon-compatible devices

### Qt BLE Scanner (Fallback)
Falls back to Qt's `QBluetoothDeviceDiscoveryAgent` when:
- Native scanner is not available
- Running on non-Android platforms
- User explicitly switches to Qt scanner

The scanner automatically detects the best available method and uses it.

## Permissions

### Android 12+ (API 31+)
- `BLUETOOTH_SCAN` - Required for BLE scanning
- `BLUETOOTH_CONNECT` - Required for BLE connections
- `BLUETOOTH_ADVERTISE` - Required for advertising

### Android 11 and below
- `ACCESS_FINE_LOCATION` - Required for BLE scanning
- `ACCESS_COARSE_LOCATION` - Alternative location permission

All permissions are requested at runtime when the app starts.

## Monitored Devices
The application monitors the following device addresses:
- DD:2B:7C:C0:A0:84
- EB:3B:E8:48:F4:90

It also automatically detects any device with "meeblue" in its name (case-insensitive).

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
- Qt 6.5 or later (tested with Qt 6.9.3)
- Qt Bluetooth module
- CMake 3.16 or later
- For Android: Android SDK with API level 31+ recommended

```bash
mkdir build
cd build
cmake ..
cmake --build .
```

## Implementation
The core functionality is implemented across several files:

### C++ Backend
- **meebluereader.h/cpp**: Main BLE scanning logic and beacon processing
- **nativeblescanner.h/cpp**: C++ wrapper for native Android BLE scanner
- **main.cpp**: Application entry point and permission handling

### Java/Android
- **android/src/org/tarmoj/meeblue/NativeBleScanner.java**: Native Android BLE scanner implementation using BluetoothLeScanner API

### QML Frontend
- **Main.qml**: QML UI for displaying beacon information

### Configuration
- **CMakeLists.txt**: Build configuration
- **android/AndroidManifest.xml**: Android permissions and configuration

## Troubleshooting

### MeeBlue beacons not detected
1. Ensure all permissions are granted (check app settings)
2. The native scanner should be used automatically on Android
3. Check that Bluetooth and Location are enabled on the device
4. Verify beacons are advertising (use nRF Connect to test)

### Build issues
- Ensure Qt 6.5+ with Bluetooth module is installed
- Check that Android SDK is properly configured
- Verify CMake can find Qt installation

## License
See LICENSE file for details.
