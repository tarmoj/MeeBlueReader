# Dynamic UUID Discovery Implementation

## Overview

This document describes the implementation of dynamic iBeacon UUID discovery from MeeBlue devices via GATT services.

## Problem

MeeBlue beacons can have different UUIDs configured. Instead of manually configuring each UUID in the iOS iBeacon scanner, we want to automatically discover the UUID from the beacon itself and add it to the monitoring list.

## Solution

The implementation uses GATT (Generic Attribute Profile) services to read the iBeacon configuration data directly from MeeBlue devices.

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│  1. BLE Discovery (MeeBlueReader)                           │
│     - Discovers MeeBlue devices via advertisement           │
│     - isTargetDevice() identifies MeeBlue beacons           │
└─────────────────────┬───────────────────────────────────────┘
                      │
                      ▼
┌─────────────────────────────────────────────────────────────┐
│  2. GATT Connection                                          │
│     - Connect to device using QLowEnergyController          │
│     - Discover services (Service 0x2000)                    │
└─────────────────────┬───────────────────────────────────────┘
                      │
                      ▼
┌─────────────────────────────────────────────────────────────┐
│  3. Read Characteristic                                      │
│     - Read Characteristic 0x2001 (20 bytes)                 │
│     - Contains first 20 bytes of iBeacon advertisement      │
└─────────────────────┬───────────────────────────────────────┘
                      │
                      ▼
┌─────────────────────────────────────────────────────────────┐
│  4. Extract UUID                                             │
│     - Parse iBeacon data format                             │
│     - Extract 16-byte UUID from bytes 8-23                  │
│     - Format as standard UUID string                        │
└─────────────────────┬───────────────────────────────────────┘
                      │
                      ▼ emit beaconUuidDiscovered(uuid)
┌─────────────────────────────────────────────────────────────┐
│  5. Add to iOS Scanner (IBeaconScanner)                     │
│     - addBeaconUUID(uuid) called automatically              │
│     - Create CLBeaconIdentityConstraint for UUID            │
│     - Start ranging immediately                             │
└─────────────────────────────────────────────────────────────┘
```

## Implementation Details

### MeeBlue GATT Service Structure

**Service 0x2000**
- Characteristic 0x2001: Read/Write, 20 Bytes - First 20 bytes of 1st channel
- Characteristic 0x2002: Read/Write, 12 Bytes - Last 12 bytes of 1st channel

The first channel transmits iBeacon data in the standard format.

### iBeacon Data Format

The characteristic 0x2001 contains:
```
Byte  0:    Effective length (e.g., 0x1E = 30 bytes)
Byte  1-2:  0x06 0x1A (length and type)
Byte  3-7:  0xFF 0x4C 0x00 0x02 0x15 (Apple's iBeacon prefix)
Byte  8-23: 16 bytes - Proximity UUID
Byte 24-25: 2 bytes - Major value (only in characteristic 0x2001)
Byte 26-27: 2 bytes - Minor value (requires characteristic 0x2002)
Byte 28:    1 byte - Measured power (requires characteristic 0x2002)
```

### Code Flow

#### MeeBlueReader::deviceDiscovered()
1. Check if device is a target (isTargetDevice)
2. Create QLowEnergyController for the device
3. Connect to device
4. On connection, call readBeaconUuidFromDevice()

#### MeeBlueReader::readBeaconUuidFromDevice()
1. Discover all services
2. Look for Service 0x2000
3. Create service object
4. Discover characteristics
5. Find Characteristic 0x2001
6. Read the characteristic value
7. Call extractUuidFromBeaconData()
8. Emit beaconUuidDiscovered(uuid) signal
9. Disconnect from device

#### MeeBlueReader::extractUuidFromBeaconData()
1. Validate data length (minimum 20 bytes)
2. Check for iBeacon header (0x06 0x1A 0xFF 0x4C 0x00 0x02 0x15)
3. Extract 16 bytes starting at byte 8
4. Format as UUID string: XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX
5. Return formatted UUID

#### IBeaconScanner::addBeaconUUID()
1. Convert QString to NSString
2. Check if UUID already exists in monitoring list
3. Add UUID to beaconUUIDs array
4. Create NSUUID from string
5. Create CLBeaconIdentityConstraint
6. Create CLBeaconRegion
7. Start monitoring and ranging immediately
8. Add to monitored regions and constraints lists

### Connection in main.cpp

```cpp
QObject::connect(&reader, &MeeBlueReader::beaconUuidDiscovered,
                 ibeaconScanner, &IBeaconScanner::addBeaconUUID);
```

This automatic connection ensures that any UUID discovered by MeeBlueReader is immediately added to the iOS scanner.

## Example Scenario

1. **App Starts**
   - iOS scanner starts with default UUID (E2C56DB5-DFFB-48D2-B060-D0F5A71096E0)
   - MeeBlueReader starts BLE discovery

2. **MeeBlue Device Discovered**
   - Device name contains "meeblue"
   - isTargetDevice() returns true
   - QLowEnergyController created

3. **GATT Connection**
   - Connect to device
   - Discover Service 0x2000
   - Read Characteristic 0x2001
   - Data: `1E 06 1A FF 4C 00 02 15 D3 5B 76 E2 E0 1C 9F AC BA 8D 7C E2 0B DB A0 C6 ...`

4. **UUID Extraction**
   - Extract bytes 8-23: `D3 5B 76 E2 E0 1C 9F AC BA 8D 7C E2 0B DB A0 C6`
   - Format: `D35B76E2-E01C-9FAC-BA8D-7CE20BDBA0C6`

5. **iOS Scanner Update**
   - addBeaconUUID("D35B76E2-E01C-9FAC-BA8D-7CE20BDBA0C6")
   - iOS scanner now monitors both UUIDs
   - Beacon is detected and ranging begins

## Benefits

1. **No Manual Configuration**: UUIDs are discovered automatically
2. **Dynamic Updates**: New beacons are added at runtime
3. **No Duplicates**: Checks prevent adding the same UUID twice
4. **Immediate Ranging**: Starts monitoring as soon as UUID is discovered
5. **Multiple Beacons**: Each MeeBlue device's UUID is discovered independently

## Error Handling

- Invalid UUIDs are logged and skipped
- GATT connection errors are logged
- Service/characteristic not found scenarios handled gracefully
- Disconnection after reading prevents resource leaks
- Controllers cleaned up in destructor

## Debugging

Enable debug output to see the flow:
- `qDebug() << "Connected to MeeBlue device:"`
- `qDebug() << "Service discovery finished"`
- `qDebug() << "Characteristic 0x2001 read, size:"`
- `qDebug() << "Data:" << value.toHex(' ')`
- `qDebug() << "Extracted iBeacon UUID:"`
- `qDebug() << "Added UUID to monitoring list:"`
- `qDebug() << "Started monitoring dynamically added UUID:"`

## Limitations

1. Requires iOS device (iOS 16+) for iBeacon ranging
2. Requires actual MeeBlue hardware for testing
3. GATT connection adds slight delay before UUID is available
4. One-time read per device (reconnection needed if beacon is reconfigured)

## Testing

To test the implementation:
1. Deploy app to iOS device
2. Place MeeBlue beacon nearby
3. App discovers beacon via BLE
4. App connects via GATT
5. App reads UUID from characteristic 0x2001
6. Check debug log for "Extracted iBeacon UUID: ..."
7. Check debug log for "Started monitoring dynamically added UUID: ..."
8. Verify beacon appears in UI with correct UUID

## Future Enhancements

1. Cache discovered UUIDs to avoid repeated GATT connections
2. Support reading Major/Minor from characteristics 0x2001 and 0x2002
3. Add UI to show discovered vs. configured UUIDs
4. Support Characteristic 0x2005 (Custom Beacon UUID+Major+Minor)
5. Periodic re-reading to detect UUID changes
