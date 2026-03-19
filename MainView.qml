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
        if (stationModel.count === 0) {
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
        console.log("WS OUT:", message)
        sendWsMessage(message)
    }

    Timer {
        id: wsBatchTimer
        interval: 300
        repeat: false
        onTriggered: sendStationSnapshot()
    }

    // --- Sound player (MP3) ---
    // Threshold -65 → sound3.mp3, -55 → sound2.mp3, -45 → sound1.mp3
    MediaPlayer {
        id: soundPlayer
        audioOutput: AudioOutput { volume: 1.0 }
    }

    // Called by StationInfo when the strongest station crosses an RSSI threshold upward.
    // threshold: -65, -55, or -45
    function triggerSound(threshold) {
        console.log("Triggering sound on: ", threshold)
        if (threshold === -45) {
            soundPlayer.source = "qrc:/sounds/sound1.mp3"
        } else if (threshold === -55) {
            soundPlayer.source = "qrc:/sounds/sound2.mp3"
        } else if (threshold === -65) {
            soundPlayer.source = "qrc:/sounds/sound3.mp3"
        }
        soundPlayer.play()
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

            Image {
                id: notationImage
                anchors.centerIn: parent
                width: parent.width * 0.8
                fillMode: Image.PreserveAspectFit

                source: "" //"qrc:/images/notation/1-immediate.png"
            }

        }

    }

}


