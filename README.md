# MeeBlueReader
Tryout project for reading RSSI signals from Meeblue beacons

## Overview
MeeBlueReader is a Qt6 QML application that continuously scans for Bluetooth Low Energy beacons and estimates their distance based on RSSI (Received Signal Strength Indicator) values.

## Platforms

### Android
- Uses Qt Bluetooth with `QBluetoothDeviceDiscoveryAgent`
- Continuous BLE scanning with periodic restarts
- Connects to devices to get RSSI updates

### iOS (New!)
- **Native iBeacon Scanner** using `CLLocationManager`
- Continuous scanning without stop-start cycles
- **No connection required** - multiple phones can scan simultaneously
- Native iOS frequency updates (1Hz or faster)
- Modern iOS 13+ API (CLBeaconIdentityConstraint)

See [iOS_IBEACON_README.md](iOS_IBEACON_README.md) for iOS implementation details.

## Features
- **Continuous BLE Scanning**: Scans for Bluetooth devices continuously
- **Device Filtering**: Detects devices with "meeblue" in their name or specific MAC addresses
- **Distance Estimation**: Calculates approximate distance using log-distance path loss model
- **Real-time Display**: Shows beacon information in the format: `{address} - {rssi} dB - {distance} m`
- **iOS iBeacon Support**: Native CLLocationManager integration for iOS
- **Configurable UUIDs**: Dynamic beacon UUID configuration

## Monitored Devices
The application monitors the following device addresses:
- DD:2B:7C:C0:A0:84
- EB:3B:E8:48:F4:90

For iOS, default beacon UUID: E2C56DB5-DFFB-48D2-B060-D0F5A71096E0

## Distance Calculation
Distance is estimated using the log-distance path loss model:
```
distance = 10^((TX_POWER - rssi) / (10 * N))
```
Where:
- TX_POWER = -40 dBm (Android) / -59 dBm (iOS, measured power at 1 meter)
- N = 2.0 (environmental attenuation factor)
- rssi = measured signal strength

## Building
Requirements:
- Qt 6.5 or later
- Qt Bluetooth module
- CMake 3.16 or later
- For iOS: Xcode with iOS 16+ SDK

```bash
mkdir build
cd build
cmake ..
cmake --build .
```

### iOS Build
For iOS, additional frameworks are linked automatically:
- CoreLocation.framework
- Foundation.framework

See [iOS_IBEACON_README.md](iOS_IBEACON_README.md) for iOS-specific build instructions.

## Implementation

### Android/Desktop
The core functionality is implemented in the `MeeBlueReader` class:
- **meebluereader.h**: Class definition with Qt properties and signals
- **meebluereader.cpp**: Implementation of BLE scanning and distance calculation
- **Main.qml**: QML UI for displaying beacon information
- **main.cpp**: Application entry point and MeeBlueReader initialization

### iOS
Native iBeacon implementation:
- **ibeaconscanner.h**: Qt interface class with properties and signals
- **ibeaconscanner_ios.mm**: Native Objective-C++ implementation using CLLocationManager
- **Main.qml**: QML UI with iOS scanner connections (platform-independent)
- **main.cpp**: iOS scanner initialization (conditionally compiled)

## Documentation

Comprehensive documentation available:
- **[iOS_IBEACON_README.md](iOS_IBEACON_README.md)** - iOS implementation technical details
- **[IMPLEMENTATION_SUMMARY.md](IMPLEMENTATION_SUMMARY.md)** - Architecture and design overview
- **[USAGE_EXAMPLES.md](USAGE_EXAMPLES.md)** - Practical usage examples and patterns
- **[ARCHITECTURE.md](ARCHITECTURE.md)** - Detailed architecture with diagrams

## Testing

### Android
Can be tested on Android emulator or physical device.

### iOS
⚠️ **Requires physical iOS device** - iOS Simulator does not support iBeacon ranging.

Testing requirements:
- Physical iOS device running iOS 16+
- Actual iBeacon hardware
- Location permissions granted

## Configuration

### iOS Beacon UUIDs
Configure beacon UUIDs programmatically:

```cpp
#ifdef Q_OS_IOS
    IBeaconScanner *scanner = new IBeaconScanner(&app);
    QStringList uuids;
    uuids << "E2C56DB5-DFFB-48D2-B060-D0F5A71096E0";  // MeeBlue
    uuids << "FDA50693-A4E2-4FB1-AFCF-C6EB07647825";  // Estimote
    scanner->setBeaconUUIDs(uuids);
    scanner->startScanning();
#endif
```

See [USAGE_EXAMPLES.md](USAGE_EXAMPLES.md) for more examples.

## Permissions

### Android
- `BLUETOOTH_SCAN`
- `BLUETOOTH_CONNECT` (optional, for connections)
- `ACCESS_FINE_LOCATION` (pre-Android 12)

### iOS
- `NSLocationWhenInUseUsageDescription`
- `NSLocationAlwaysAndWhenInUseUsageDescription`
- `NSBluetoothAlwaysUsageDescription`

All configured in Info.plist.

## License
See [LICENSE](LICENSE) file for details.
