# Usage Examples for iOS iBeacon Scanner

## Basic Usage (Default Configuration)

The scanner starts automatically with a default MeeBlue beacon UUID:

```cpp
// In main.cpp
#ifdef Q_OS_IOS
    IBeaconScanner *ibeaconScanner = new IBeaconScanner(&app);
    engine.rootContext()->setContextProperty("ibeaconScanner", ibeaconScanner);
    ibeaconScanner->startScanning();
#endif
```

Default UUID: `E2C56DB5-DFFB-48D2-B060-D0F5A71096E0`

## Custom Beacon UUIDs

To scan for different beacon types, configure UUIDs before starting:

```cpp
#ifdef Q_OS_IOS
    IBeaconScanner *ibeaconScanner = new IBeaconScanner(&app);
    engine.rootContext()->setContextProperty("ibeaconScanner", ibeaconScanner);
    
    // Configure custom UUIDs
    QStringList customUUIDs;
    customUUIDs << "FDA50693-A4E2-4FB1-AFCF-C6EB07647825";  // Estimote
    customUUIDs << "E2C56DB5-DFFB-48D2-B060-D0F5A71096E0";  // MeeBlue
    customUUIDs << "B9407F30-F5F8-466E-AFF9-25556B57FE6D";  // Kontakt.io
    
    ibeaconScanner->setBeaconUUIDs(customUUIDs);
    ibeaconScanner->startScanning();
#endif
```

## Common Beacon UUIDs

### Commercial Beacons
- **Estimote**: `FDA50693-A4E2-4FB1-AFCF-C6EB07647825`
- **Kontakt.io**: `B9407F30-F5F8-466E-AFF9-25556B57FE6D`
- **Radius Networks**: `2F234454-CF6D-4A0F-ADF2-F4911BA9FFA6`
- **MeeBlue**: `E2C56DB5-DFFB-48D2-B060-D0F5A71096E0`

### Apple's iBeacon UUID
- **Apple AirLocate**: `E2C56DB5-DFFB-48D2-B060-D0F5A71096E0`

## QML Usage

### Displaying Beacon Information

```qml
ListView {
    model: ibeaconScanner.beaconList
    delegate: Item {
        Text {
            text: modelData.uuid + " (" + modelData.major + ":" + modelData.minor + ")"
        }
        Text {
            text: "RSSI: " + modelData.rssi + " dBm"
        }
        Text {
            text: "Distance: " + modelData.distance.toFixed(2) + " m"
        }
    }
}
```

### Handling Individual Beacon Updates

```qml
Connections {
    target: typeof ibeaconScanner !== 'undefined' ? ibeaconScanner : null
    
    function onNewBeaconInfo(uuid, rssi, distance, major, minor) {
        console.log("Beacon detected:");
        console.log("  UUID:", uuid);
        console.log("  Major:", major);
        console.log("  Minor:", minor);
        console.log("  RSSI:", rssi, "dBm");
        console.log("  Distance:", distance.toFixed(2), "m");
        
        // Custom logic based on beacon
        if (major === 1 && minor === 100) {
            // This is a specific beacon we're interested in
            handleSpecialBeacon(rssi, distance);
        }
    }
}
```

## Dynamic UUID Configuration from QML

You can also configure UUIDs from QML after the scanner is created:

```qml
Button {
    text: "Scan Estimote Beacons"
    onClicked: {
        if (typeof ibeaconScanner !== 'undefined') {
            // Stop current scanning
            ibeaconScanner.stopScanning();
            
            // Configure new UUIDs
            ibeaconScanner.setBeaconUUIDs(["FDA50693-A4E2-4FB1-AFCF-C6EB07647825"]);
            
            // Restart with new configuration
            ibeaconScanner.startScanning();
        }
    }
}
```

## Filtering Beacons by Major/Minor

```qml
Connections {
    target: typeof ibeaconScanner !== 'undefined' ? ibeaconScanner : null
    
    function onNewBeaconInfo(uuid, rssi, distance, major, minor) {
        // Filter for specific major value
        if (major === 1) {
            // Process only beacons with major == 1
            updateBeaconDisplay(uuid, major, minor, rssi, distance);
        }
        
        // Filter for specific major AND minor
        if (major === 1 && minor >= 100 && minor <= 200) {
            // Process only beacons in specific range
            handleBeaconInRange(uuid, major, minor, rssi, distance);
        }
    }
}
```

## Proximity-Based Actions

```qml
Connections {
    target: typeof ibeaconScanner !== 'undefined' ? ibeaconScanner : null
    
    function onNewBeaconInfo(uuid, rssi, distance, major, minor) {
        var beaconId = uuid + "-" + major + "-" + minor;
        
        // Proximity-based logic
        if (distance < 0.5) {
            // Very close (< 0.5 meters)
            console.log("Beacon", beaconId, "is immediate");
            triggerImmediateAction(beaconId);
        } else if (distance < 2.0) {
            // Near (0.5-2 meters)
            console.log("Beacon", beaconId, "is near");
            showNearNotification(beaconId);
        } else if (distance < 10.0) {
            // Far (2-10 meters)
            console.log("Beacon", beaconId, "is far");
            updateFarBeaconList(beaconId);
        }
    }
}
```

## RSSI-Based Signal Strength Indicator

```qml
Rectangle {
    width: parent.width
    height: 30
    
    property int beaconRssi: -70  // Updated from beacon signal
    
    Rectangle {
        width: parent.width * signalStrength
        height: parent.height
        color: signalColor
        
        property real signalStrength: {
            // Convert RSSI to 0-1 scale
            // Typical range: -90 (weak) to -40 (strong)
            var normalized = (beaconRssi + 90) / 50.0;
            return Math.max(0, Math.min(1, normalized));
        }
        
        property color signalColor: {
            if (signalStrength > 0.7) return "green";
            if (signalStrength > 0.4) return "yellow";
            return "red";
        }
    }
    
    Connections {
        target: typeof ibeaconScanner !== 'undefined' ? ibeaconScanner : null
        function onNewBeaconInfo(uuid, rssi, distance, major, minor) {
            parent.beaconRssi = rssi;
        }
    }
}
```

## Multi-Zone Detection

```qml
Item {
    property var zones: ({
        "entrance": {uuid: "...", major: 1, minor: 1},
        "lobby": {uuid: "...", major: 1, minor: 2},
        "office": {uuid: "...", major: 1, minor: 3}
    })
    
    property string currentZone: "unknown"
    property var zoneDistances: ({})
    
    Connections {
        target: typeof ibeaconScanner !== 'undefined' ? ibeaconScanner : null
        
        function onNewBeaconInfo(uuid, rssi, distance, major, minor) {
            // Update distances for all zones
            for (var zoneName in zones) {
                var zone = zones[zoneName];
                if (zone.major === major && zone.minor === minor) {
                    zoneDistances[zoneName] = distance;
                }
            }
            
            // Determine current zone (closest beacon)
            var closestZone = "unknown";
            var closestDistance = 999.0;
            
            for (var name in zoneDistances) {
                if (zoneDistances[name] < closestDistance) {
                    closestDistance = zoneDistances[name];
                    closestZone = name;
                }
            }
            
            if (closestZone !== currentZone && closestDistance < 5.0) {
                currentZone = closestZone;
                console.log("Entered zone:", currentZone);
                onZoneChanged(currentZone);
            }
        }
    }
    
    function onZoneChanged(newZone) {
        // Handle zone transitions
        console.log("Now in zone:", newZone);
    }
}
```

## Performance Tips

1. **Limit UUID Count**: Monitor only the UUIDs you need
   ```cpp
   // Good: Specific UUIDs
   customUUIDs << "E2C56DB5-DFFB-48D2-B060-D0F5A71096E0";
   
   // Avoid: Too many UUIDs
   // Scanning for 10+ different UUIDs may impact performance
   ```

2. **Filter in QML**: Do additional filtering in your QML code
   ```qml
   function onNewBeaconInfo(uuid, rssi, distance, major, minor) {
       // Filter out weak signals
       if (rssi < -90) return;
       
       // Filter out distant beacons
       if (distance > 20.0) return;
       
       // Process only beacons of interest
       processBeacon(uuid, rssi, distance, major, minor);
   }
   ```

3. **Debounce Updates**: Avoid updating UI too frequently
   ```qml
   Timer {
       id: updateTimer
       interval: 500  // Update UI every 500ms max
       repeat: false
       onTriggered: updateUI()
   }
   
   Connections {
       target: ibeaconScanner
       function onNewBeaconInfo(uuid, rssi, distance, major, minor) {
           // Store data but delay UI update
           storeBeaconData(uuid, rssi, distance, major, minor);
           updateTimer.restart();
       }
   }
   ```

## Troubleshooting

### No beacons detected
1. Check UUID matches your beacon's UUID exactly
2. Verify location permissions are granted
3. Ensure beacon is powered on and advertising
4. Check minimum iOS version (iOS 16+ required)

### Distance inaccurate
1. Adjust TX_POWER constant in implementation (line ~192)
2. Adjust environmental factor N (default 2.0)
3. Calibrate based on your specific beacon hardware
4. Consider environmental interference (walls, metal, etc.)

### Permissions not requested
1. Verify Info.plist contains location usage descriptions
2. Check that location services are enabled in iOS Settings
3. Try reinstalling the app to trigger permission prompt

## Advanced: Custom Distance Calculation

To use custom distance calculation parameters, modify `calculateDistanceFromRSSI` in `ibeaconscanner_ios.mm`:

```objective-c
- (double)calculateDistanceFromRSSI:(NSInteger)rssi {
    if (rssi == 0) {
        return -1.0; // Invalid
    }
    
    // Adjust these values for your beacon hardware
    const int TX_POWER = -59;  // Measured power at 1 meter
    const double N = 2.0;      // Environmental factor (2.0-4.0)
    
    double ratio = (double)(TX_POWER - rssi) / (10.0 * N);
    double distance = pow(10.0, ratio);
    
    return distance;
}
```

Calibration tips:
- TX_POWER: Measure actual RSSI at exactly 1 meter from beacon
- N values: 2.0 (open space), 2.5 (office), 3.0 (mall), 4.0 (dense obstacles)
