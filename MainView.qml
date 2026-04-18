import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtMultimedia
import QtWebSockets


Rectangle {
    id: mainView
    width: 506
    height: 900
    property var settingsRef: null
    property alias wsSocket: socket
    property var fileDownloaderRef: null

    // Rules loaded from rules{N}.json.  Each element is a JS object matching the event schema.
    property var eventRules: []

    // Per-station tracking state: { stationId: { prevRssi, prevProximity, cooldowns: {name: ms} } }
    property var _stationState: ({})

    // Proximity ordering used for "in" / "out" matching
    readonly property var _proxOrder: ({ "Unknown": 0, "Far": 1, "Near": 2, "Immediate": 3 })

    gradient: Gradient {
        GradientStop { position: 0.0; color: Material.backgroundColor }
        GradientStop { position: 0.6; color: Material.backgroundColor }
        GradientStop { position: 0.8; color: "#194eb4" }
        GradientStop { position: 1; color: "#750d95" }
    }

    property int strongestStation: 0;

    WebSocket {
        id: socket
        url: settingsRef ? ("ws://" + settingsRef.serverIP + ":" + settingsRef.serverPort) : ""

        onTextMessageReceived: function (message) {
            console.log("WS message received:", message)
        }

        onStatusChanged: {
            if (socket.status === WebSocket.Error) {
                console.log("WebSocket error:", socket.errorString, url)
                socket.active = false
            } else if (socket.status === WebSocket.Open) {
                console.log("WebSocket open:", url)
            } else if (socket.status === WebSocket.Closed) {
                console.log("WebSocket closed")
                socket.active = false
            } else if (socket.status === WebSocket.Connecting) {
                console.log("WebSocket connecting:", url)
            }
        }
        active: true
    }

    function sendWsMessage(message) {
        if (socket.status === WebSocket.Open) {
            socket.sendTextMessage(message)
        } else {
            console.warn("sendWsMessage: socket not open")
        }
    }

    function sendStationSnapshot() {
        if (stationModel.count === 0 || socket.status !== WebSocket.Open) {
            return
        }

        var stations = []
        for (var i = 0; i < stationModel.count; i++) {
            var item = stationModel.get(i)
            stations.push({
                stationId: item.stationId,
                rssi: item.rssi,
                proximity: item.proximity,
                beaconIds: item.beaconIds
            })
        }

        var payload = {
            userId: settingsRef ? settingsRef.userID : 0,
            userName: settingsRef ? settingsRef.userName : "",
            timestamp: new Date().toISOString(),
            strongestStation: strongestStation,
            stations: stations
        }

        var message = JSON.stringify(payload)
        //console.log("WS OUT:", message)
        sendWsMessage(message)
    }


    // FOR TESING ONLY
    function rssiToProximity(rssi) {
        if (rssi >= -50) return "Immediate"
        if (rssi >= -65) return "Near"
        return "Far"
    }

    function sendTestInfo() {
        var stationIds = [1, 2, 3]
        var stations = []
        var maxRssi = -999
        var strongestId = stationIds[0]

        for (var i = 0; i < stationIds.length; i++) {
            var rssi = Math.round(-80 + Math.random() * 50)  // -80 .. -30
            if (rssi > maxRssi) {
                maxRssi = rssi
                strongestId = stationIds[i]
            }
            stations.push({
                stationId: stationIds[i],
                rssi: rssi,
                proximity: rssiToProximity(rssi)
            })
        }

        var payload = {
            userId: settingsRef ? settingsRef.userID : 0,
            userName: settingsRef ? settingsRef.userName : "Test",
            timestamp: new Date().toISOString(),
            strongestStation: strongestId,
            stations: stations
        }
        var message = JSON.stringify(payload)
        console.log("WS TEST OUT:", message)
        sendWsMessage(message)
    }

    Timer {
        id: wsBatchTimer
        interval: 300
        repeat: false
        onTriggered: sendStationSnapshot()
    }

    // --- Sound / image player ---
    MediaPlayer {
        id: soundPlayer
        audioOutput: AudioOutput { volume: 1.0 }
    }

    // Trigger sound from local data directory (file:// URL)
    function triggerEventSound(fileName) {
        if (!fileName || fileName === "" || fileDownloaderRef === null) return
        const path = "file://" + fileDownloaderRef.localDataPath + "/sounds/" + fileName
        console.log("triggerEventSound:", path)
        soundPlayer.source = path
        soundPlayer.play()
    }

    // Show image from local data directory (file:// URL).
    // Pass "none" to clear the current image without showing a new one.
    function triggerEventImage(fileName) {
        if (!fileName || fileName === "") return
        if (fileName.toLowerCase() === "none") {
            notationImage.source = ""
            return
        }
        if (fileDownloaderRef === null) return
        const path = "file://" + fileDownloaderRef.localDataPath + "/images/" + fileName
        console.log("triggerEventImage:", path)
        eventText.text = ""
        notationImage.source = path
    }

    // Display a text message in the content area.
    // Pass "none" to clear the current text without showing a new one.
    function triggerEventText(msg) {
        if (!msg || msg === "") return
        if (msg.toLowerCase() === "none") {
            eventText.text = ""
            return
        }
        notationImage.source = ""
        eventText.text = msg
    }

    // Evaluate all rules against the current station update.
    function evaluateEvents(stationId, rssi, proximity) {
        if (eventRules.length === 0) return

        const state = _stationState[stationId] || { prevRssi: -999, prevProximity: "Unknown", cooldowns: {} }
        const prevRssi   = state.prevRssi
        const prevProx   = state.prevProximity
        const now        = Date.now()

        for (let i = 0; i < eventRules.length; i++) {
            const rule = eventRules[i]

            // --- Station filter ---
            const rStation = rule.station !== undefined ? parseInt(rule.station) : 0
            if (rStation > 0 && rStation !== stationId) continue
            if (rStation <= 0 && stationId !== strongestStation) continue

            // --- RSSI threshold check ---
            const rRssi = rule.rssi !== undefined ? parseInt(rule.rssi) : 0
            if (rRssi !== 0) {
                const dir = (rule.updDown || "up").toLowerCase()
                const crossedUp   = rssi > rRssi && prevRssi <= rRssi
                const crossedDown = rssi < rRssi && prevRssi >= rRssi
                const matched = (dir === "up" && crossedUp) || (dir === "down" && crossedDown)
                if (!matched) continue
            }

            // --- Proximity zone check ---
            const rProx = (rule.proximity || "").trim()
            if (rProx !== "") {
                const inOut = (rule.inOut || "in").toLowerCase()
                const curOrd  = _proxOrder[proximity]  !== undefined ? _proxOrder[proximity]  : 0
                const prevOrd = _proxOrder[prevProx]   !== undefined ? _proxOrder[prevProx]   : 0
                const rOrd    = _proxOrder[rProx]      !== undefined ? _proxOrder[rProx]      : 0
                const enteredZone = curOrd >= rOrd && prevOrd < rOrd
                const leftZone    = curOrd < rOrd  && prevOrd >= rOrd
                const matched = (inOut === "in" && enteredZone) || (inOut === "out" && leftZone)
                if (!matched) continue
            }

            // --- Retrigger cooldown ---
            const cooldownMs = (rule.retriggerAllowedAfter !== undefined
                                    ? parseFloat(rule.retriggerAllowedAfter) : 0) * 1000
            const ruleName = rule.name || ("rule_" + i)
            const lastFired = state.cooldowns[ruleName] || 0
            if (cooldownMs > 0 && (now - lastFired) < cooldownMs) continue

            // --- All conditions passed: trigger ---
            console.log("Event triggered:", ruleName, "station", stationId)
            triggerEventSound(rule.sound || "")
            if (rule.text && rule.text !== "")
                triggerEventText(rule.text)
            else
                triggerEventImage(rule.image || "")

            // Update cooldown timestamp (must re-read _stationState to avoid stale copy)
            const updated = _stationState[stationId] || state
            updated.cooldowns[ruleName] = now
            const newState = Object.assign({}, _stationState)
            newState[stationId] = updated
            _stationState = newState
        }
    }

    ListModel {
        id: stationModel
    }


    Connections {
        target: meeBlueReader
        function onNewStationInfo(stationId, rssi, proximity, beaconIds) {
            // Check if station already exists in the model
            var found = false;
            for (var i = 0; i < stationModel.count; i++) {
                if (stationModel.get(i).stationId === stationId) {
                    // Update existing station
                    stationModel.set(i, {
                        "stationId": stationId,
                        "rssi": rssi,
                        "proximity": proximity,
                        "beaconIds": beaconIds
                    });
                    found = true;
                    break;
                }
            }

            // Add new station if not found
            if (!found) {
                stationModel.append({
                    "stationId": stationId,
                    "rssi": rssi,
                    "proximity": proximity,
                    "beaconIds": beaconIds
                });
            }

            // Find strongest station (highest RSSI)
            var maxRssi = -999;
            var maxStationId = 0;
            for (var j = 0; j < stationModel.count; j++) {
                if (stationModel.get(j).rssi > maxRssi) {
                    maxRssi = stationModel.get(j).rssi;
                    maxStationId = stationModel.get(j).stationId;
                }
            }
            strongestStation = maxStationId;
            wsBatchTimer.restart()

            // Evaluate rules-based events; must happen after strongestStation is updated
            // and before we write the new prev values.
            evaluateEvents(stationId, rssi, proximity)

            // Update previous-state tracking for this station
            var st = Object.assign({}, _stationState)
            var prev = st[stationId] || { prevRssi: -999, prevProximity: "Unknown", cooldowns: {} }
            prev.prevRssi      = rssi
            prev.prevProximity = proximity
            st[stationId] = prev
            _stationState = st
        }
    }

    // onStrongestStationChanged: {
    //     statusLabel.text = qsTr("Strongest station: ") + strongestStation;
    // }


    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 10

        RowLayout {
            id: statusRow
            spacing: 10

            Label {
                id: statusLabel
                text: qsTr("Connected: ") + (socket.status === WebSocket.Open ? qsTr("YES") : qsTr("NO"))
                      + " | " + qsTr("Strongest station: ") + strongestStation
            }

            Button {
                text: qsTr("Send test info")
                onClicked: sendTestInfo()
            }
        }



        Label {
            text: qsTr("Stations:")
        }


        GridLayout {
            id: stationsArea
            Layout.fillWidth: true
            columns: 2
            property double cellWidth: width/columns - columns*columnSpacing

            rowSpacing: 5
            columnSpacing: 15

            Repeater {
                id: stationRepeater
                model: stationModel

                StationInfo {
                    stationNumber: model.stationId
                    rssi: model.rssi
                    proximity: model.proximity
                    color: Qt.hsla(model.stationId * 0.1618, 0.7, 0.5, 1)
                    Layout.fillWidth: true
                    notationImageRef: notationImage
                    //strongestStation: mainView.strongestStation
                }
            }
        }

        Rectangle {
            id: contentArea
            Layout.fillHeight: true
            Layout.fillWidth:  true

            color: "transparent"
            border.color: Material.dividerColor

            ToolButton {
                id: clearButton
                anchors.right: parent.right
                anchors.margins: 5
                anchors.top: parent.top
                text: qsTr("Clear")
                onClicked: {
                    notationImage.source = ""
                    eventText.text = ""
                }
            }

            Image {
                id: notationImage
                anchors.centerIn: parent
                width: parent.width * 0.8
                height: Math.min(implicitHeight, contentArea.height)
                fillMode: Image.PreserveAspectFit
                source: ""
            }

            Label {
                id: eventText
                anchors.centerIn: parent
                width: parent.width * 0.9
                text: ""
                visible: text !== ""
                font.pointSize: 32
                font.bold: true
                //color: "white"
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
            }

        }

    }

}


