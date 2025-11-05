# iOS iBeacon Scanner Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                         QML UI Layer                             │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │  Main.qml                                                 │   │
│  │  - ListView displaying beacons                            │   │
│  │  - Connections to ibeaconScanner signals                  │   │
│  │  - Real-time RSSI and distance updates                    │   │
│  └──────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────┘
                              │
                              │ Signals: newBeaconInfo(uuid, rssi, distance, major, minor)
                              │ Properties: beaconList
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│                      Qt/C++ Layer                                │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │  IBeaconScanner (ibeaconscanner.h)                        │   │
│  │  - Q_OBJECT with Qt properties                            │   │
│  │  - beaconList: QVariantList                               │   │
│  │  - Signals: beaconListChanged, newBeaconInfo             │   │
│  │  - Slots: updateBeacons(QVariantList)                     │   │
│  │  - Methods: startScanning(), stopScanning()               │   │
│  │            setBeaconUUIDs(QStringList)                    │   │
│  │  - m_nativeScanner: void* (opaque pointer)                │   │
│  └──────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────┘
                              │
                              │ Bridge: __bridge / __bridge_retained
                              │ Thread-safe: QMetaObject::invokeMethod
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│                   Objective-C Layer                              │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │  IBeaconScannerDelegate (ibeaconscanner_ios.mm)          │   │
│  │  - Implements CLLocationManagerDelegate                   │   │
│  │  - Properties:                                             │   │
│  │    * locationManager: CLLocationManager                   │   │
│  │    * monitoredRegions: NSMutableArray<CLBeaconRegion>    │   │
│  │    * constraints: NSMutableArray<CLBeaconIdentityConstraint>│
│  │    * beaconUUIDs: NSMutableArray<NSString>                │   │
│  │    * qtScanner: IBeaconScanner* (weak ref)                │   │
│  │  - Methods:                                                │   │
│  │    * initWithQtScanner:                                   │   │
│  │    * startScanning                                         │   │
│  │    * stopScanning                                          │   │
│  │    * setBeaconUUIDs:                                       │   │
│  │    * calculateDistanceFromRSSI:                           │   │
│  └──────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────┘
                              │
                              │ Delegate callbacks
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│                    iOS CoreLocation Framework                    │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │  CLLocationManager                                        │   │
│  │  - startRangingBeaconsSatisfyingConstraint:               │   │
│  │  - startMonitoringForRegion:                              │   │
│  │  - Delegate callbacks:                                     │   │
│  │    * didRangeBeacons:satisfyingConstraint:                │   │
│  │    * didEnterRegion:                                      │   │
│  │    * didExitRegion:                                       │   │
│  │    * didChangeAuthorizationStatus:                        │   │
│  └──────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────┘
                              │
                              │ Bluetooth LE signals
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│                      Physical iBeacons                           │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │  iBeacon Device 1                                         │   │
│  │  - UUID: E2C56DB5-DFFB-48D2-B060-D0F5A71096E0            │   │
│  │  - Major: 1, Minor: 100                                   │   │
│  │  - Broadcasting BLE advertisements                        │   │
│  └──────────────────────────────────────────────────────────┘   │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │  iBeacon Device 2                                         │   │
│  │  - UUID: E2C56DB5-DFFB-48D2-B060-D0F5A71096E0            │   │
│  │  - Major: 1, Minor: 101                                   │   │
│  │  - Broadcasting BLE advertisements                        │   │
│  └──────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────┘

## Data Flow

### Upward (Beacon → UI)
1. iBeacon broadcasts BLE advertisement
2. CLLocationManager detects beacon via CoreLocation
3. Delegate callback: didRangeBeacons:satisfyingConstraint:
4. Convert CLBeacon → QVariantMap (UUID, major, minor, RSSI, distance)
5. QMetaObject::invokeMethod → IBeaconScanner::updateBeacons()
6. Emit newBeaconInfo signal to QML
7. QML UI updates with beacon information

### Downward (UI → Beacon Configuration)
1. User/App calls setBeaconUUIDs() from QML or C++
2. IBeaconScanner::setBeaconUUIDs() forwards to delegate
3. Delegate updates beaconUUIDs array
4. On next startScanning(), new UUIDs are monitored
5. CLLocationManager configured with new constraints
6. Begin ranging for new beacon UUIDs

## Threading Model

- **Main Thread**: QML UI updates, Qt signals/slots
- **CoreLocation Thread**: Beacon ranging, delegate callbacks
- **Thread Safety**: QMetaObject::invokeMethod with Qt::QueuedConnection
  ensures all Qt operations happen on main thread

## Memory Management

- **Objective-C**: ARC (Automatic Reference Counting)
- **Qt Objects**: Parent-child hierarchy (app owns scanner)
- **Bridge**: __bridge_retained for C++ → ObjC transfer
            __bridge for temporary ObjC access from C++
            __bridge_transfer for ObjC → C++ cleanup

## Configuration Points

1. **Beacon UUIDs**: setBeaconUUIDs() - configurable at runtime
2. **TX_POWER**: In calculateDistanceFromRSSI() - calibration constant
3. **Environmental Factor N**: In calculateDistanceFromRSSI() - path loss
4. **Location Permissions**: Info.plist - NSLocationWhenInUseUsageDescription

## Key Design Decisions

### Why CLLocationManager?
- Native iOS API for iBeacon ranging
- Optimized for battery efficiency
- No connection required (passive scanning)
- Multiple apps can range simultaneously
- Continuous updates without stop-start cycles

### Why Opaque Pointer?
- Clean separation between C++ and Objective-C
- Header file remains pure C++ (no Objective-C includes)
- Allows .h file to be included in C++ code

### Why QVariantMap for Beacon Data?
- Dynamic, flexible data structure
- Easy to extend with new beacon properties
- Natural mapping to QML JavaScript objects
- No need for custom C++ beacon class

### Why Thread-Safe Signals?
- CoreLocation callbacks on background thread
- Qt signals must be emitted on main thread
- QMetaObject::invokeMethod with Qt::QueuedConnection
  safely queues signal emission to main thread

## Build Integration

### CMakeLists.txt
```cmake
if (IOS)
    list(APPEND SOURCES ibeaconscanner.h ibeaconscanner_ios.mm)
    set_source_files_properties(ibeaconscanner_ios.mm 
        PROPERTIES COMPILE_FLAGS "-x objective-c++")
    find_library(CORELOCATION_FRAMEWORK CoreLocation)
    target_link_libraries(... ${CORELOCATION_FRAMEWORK})
endif()
```

### Conditional Compilation
- All iOS-specific code wrapped in `#ifdef Q_OS_IOS`
- Android/other platforms use existing Bluetooth implementation
- No impact on non-iOS builds

## Future Enhancements

- [ ] Background scanning (requires additional Info.plist keys)
- [ ] Eddystone beacon support
- [ ] Per-UUID TX_POWER configuration
- [ ] Beacon data persistence
- [ ] Advanced filtering (major/minor ranges)
- [ ] Kalman filtering for distance smoothing
