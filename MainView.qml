import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtMultimedia


Item {
    id: mainView
    width: 506
    height: 900

    property int strongestStation: 0;

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
        }
    }

    onStrongestStationChanged: {
        statusLabel.text = qsTr("Strongest station: ") + strongestStation;
    }


    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 10

        RowLayout {
            id: statusRow

            Label {
                id: statusLabel

                text: qsTr("Status messages come here.")
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
                    // strongestStation: mainView.strongestStation
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


