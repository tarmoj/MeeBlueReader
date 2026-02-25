# Refactoring notes

Aim: restructure the code for better maintainability and cross plaform support (iOS, Android, Linux).


## General description

The program detects iBeacons in the space, displays the data and sends it to server over websocket protocol.

The program can also display images and play sounds according to specific data.

Two iBeacons are always placed next to each other in the space, to ensure bigger reliability. It is called a Station.

A Station acts as a macro-beacon, it smoothes individual rssi values and calulates the medium rssi  and proximities of its child beacons.




## General structure

The main blocks/classes of the proagram are:

- MeeBlueReader: starts beacon scanner, holds beacons' and stations' info, sends info (via signals) to UI.

- IBeaconScanner  - deals with detecting the beacons and reads their rssi and proximity and forwards the data to beaconInfo list of MeeBlueReader; 

- UI - displays the info, more details will to be specified later.


## Changes to be done

### IBeaconScanner 
Keep one header file but provide different .cpp / .mm  files for different operating systems. Support with iOS, Android and Linux.

- drop setBeaconUUIDs(),  beaconListChanged(); newBeaconInfo();  m_updateTimer and other variables not needed any more
- move functionality of averageRssi to MeeBlueReader, class Station (see below)
- in upDateBeacons() send the beaconinfo to MeeBlueReader, call its update() slot.
- declare struct BeaconInfo {uuid, major, minor, rssi, proximity} (to be used also in MeeBlueReader
- set enum of proximities (Unknown, Immediate, Near, Far)


### MeeBlueReader

- keep in list m_beaconInfo of struct BeaconInfo data coming from the beacon scanner.

- declare class Station(int id, int major1, int minor1, int major2, int minor2) with method:

    * update() 
        -  find data from m_beaconInfo according to majors and minors
        - smooth rssi readings (similar to current MeeblueHelper::smoothReadings)
        - calculate average rssi and proximity similar to current IBeaconScanner::averageRssi()
        - emit signal newStationInfo, forwarded through MeeBlueReader to UI

- remove all code that deals with beacons/BLE directly (or move it to the iBeaconScanner Android/Linux file).
- drop measuring/calculating distance
- when slot update() is called, evoke updateInfo() of the stations

### Drop MeeBlueHelper


### UI
- update necessary signals and slots logic, other things leave as it is for now.
- do not implement websocket connection yet.


#### Defining sound/image events:

"event" : {   
    "name": "name of the event or empty"
    "station": 1,
    "zone": "proximity|(empty string - not used)",
    "inOut": "in|out",
    "rssi": 0 (not used) |-80..-40,
    "updDown": "up|down",
    "retriggerAllowedAfter": 10, // in seconds
    "sound": " fileName.mp3 | ''",
    "image": " fileName| '' "

}
