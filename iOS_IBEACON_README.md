# iOS iBeacon Scanner Implementation

## Overview
This implementation provides native iOS iBeacon scanning using `CLLocationManager` for continuous RSSI updates without connecting to the beacons.

## Key Features
- **Continuous Scanning**: Uses `CLLocationManager` to continuously range iBeacons without stop-start cycles
- **No Connection Required**: Passive scanning only - multiple phones can detect the same beacons simultaneously
- **Frequent Updates**: RSSI updates at iOS native frequency (typically 1Hz or faster)
- **Distance Calculation**: Uses log-distance path loss model for distance estimation
- **Thread-Safe**: Properly bridges Objective-C to Qt with QMetaObject::invokeMethod

## Architecture

### Files
- `ibeaconscanner.h`: C++ header defining the Qt interface
- `ibeaconscanner_ios.mm`: Objective-C++ implementation using CLLocationManager
- Integration in `main.cpp` for iOS builds

### Classes

#### IBeaconScanner (C++)
Qt-based class that provides:
- `beaconList` property (QVariantList)
- `newBeaconInfo(uuid, rssi, distance, major, minor)` signal
- `updateBeacons(beacons)` slot for native code to call
- `startScanning()` / `stopScanning()` methods

#### IBeaconScannerDelegate (Objective-C)
Native iOS delegate that:
- Manages CLLocationManager instance
- Implements CLLocationManagerDelegate protocol
- Handles location authorization
- Monitors and ranges iBeacon regions
- Converts CLBeacon objects to Qt QVariantMap
- Invokes Qt methods thread-safely

## Configuration

### Beacon UUIDs
Edit the `startScanning` method in `ibeaconscanner_ios.mm` to add your beacon UUIDs:

```objective-c
NSArray *beaconUUIDs = @[
    [[NSUUID alloc] initWithUUIDString:@"E2C56DB5-DFFB-48D2-B060-D0F5A71096E0"],
    [[NSUUID alloc] initWithUUIDString:@"YOUR-UUID-HERE"],
];
```

### Distance Calculation Parameters
Adjust in the `calculateDistanceFromRSSI` method:

```objective-c
const int TX_POWER = -59; // Measured power at 1 meter (typical for iBeacons)
const double N = 2.0;     // Environmental attenuation factor
```

## Permissions

The following permissions are required in Info.plist:
- `NSLocationWhenInUseUsageDescription`
- `NSLocationAlwaysAndWhenInUseUsageDescription`
- `NSBluetoothAlwaysUsageDescription`

These are already configured in the project's Info.plist.

## Building

The iOS-specific code is automatically included when building for iOS:

```cmake
if (IOS)
    list(APPEND SOURCES
        ibeaconscanner.h
        ibeaconscanner_ios.mm
    )
endif()
```

Required frameworks are linked automatically:
- CoreLocation.framework
- Foundation.framework

## Usage

The scanner is automatically initialized and started in `main.cpp`:

```cpp
#ifdef Q_OS_IOS
    IBeaconScanner *ibeaconScanner = new IBeaconScanner(&app);
    engine.rootContext()->setContextProperty("ibeaconScanner", ibeaconScanner);
    ibeaconScanner->startScanning();
#endif
```

In QML, connect to the signals:

```qml
Connections {
    target: typeof ibeaconScanner !== 'undefined' ? ibeaconScanner : null
    function onNewBeaconInfo(uuid, rssi, distance, major, minor) {
        // Handle beacon update
    }
}
```

## Data Format

Each beacon update provides:
- `uuid`: Beacon UUID string
- `major`: Major value (integer)
- `minor`: Minor value (integer)
- `rssi`: Signal strength in dBm (integer)
- `distance`: Estimated distance in meters (double)
- `proximity`: iOS proximity classification (string: "Immediate", "Near", "Far")

## Testing

To test the implementation:
1. Build the app for iOS device (not simulator - iBeacon APIs require real hardware)
2. Deploy to a physical iOS device
3. Grant location permissions when prompted
4. Place iBeacons nearby with matching UUIDs
5. Monitor console output for beacon detection logs
6. Verify UI updates with beacon information

## Troubleshooting

### No beacons detected
- Verify location permissions are granted
- Check that beacon UUIDs match the configured UUIDs
- Ensure beacons are powered on and advertising
- Check console logs for error messages

### Permissions denied
- Go to iOS Settings → Privacy → Location Services
- Find your app and enable location access
- Reinstall the app if needed

### Build errors
- Ensure building for iOS target (not simulator for full functionality)
- Verify CoreLocation and Foundation frameworks are available
- Check that Objective-C++ files have .mm extension

## Implementation Notes

1. **CLLocationManager Lifecycle**: The location manager is created once and kept alive throughout the app lifecycle
2. **Ranging vs Monitoring**: We use both `startRangingBeaconsInRegion` for continuous updates and `startMonitoringForRegion` for entry/exit events
3. **Memory Management**: Uses ARC (Automatic Reference Counting) with proper `__bridge` and `__bridge_retained` for C++/ObjC interop
4. **Thread Safety**: All Qt signal emissions use `Qt::QueuedConnection` to ensure thread-safe operation
5. **No Connection**: This implementation does NOT use `QLowEnergyController` or connect to beacons, ensuring multiple devices can scan simultaneously

## Advantages over Qt Bluetooth API

- **No connection required**: Multiple phones can scan the same beacons
- **Continuous updates**: No stop-start cycles needed
- **Better iOS integration**: Uses native iBeacon APIs designed for this purpose
- **Battery efficient**: CLLocationManager is optimized for beacon ranging
- **More frequent updates**: Native iOS provides updates as fast as beacons advertise
