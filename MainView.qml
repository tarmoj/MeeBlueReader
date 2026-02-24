import QtQuick
import QtQuick.Layouts
import QtQuick.Controls


Item {
    //color: "#f0f0f0"
    width: 506
    height: 900

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
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 10

        RowLayout {
            id: statusRow

            Label {
                id: statusLabel

                text: "Status"
            }
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

                StationView {
                    stationNumber: model.stationId
                    rssi: model.rssi
                    proximity: model.proximity
                    color: Qt.hsla(model.stationId * 0.1618, 0.7, 0.5, 1)
                    Layout.fillWidth: true
                }
            }
        }

        Rectangle {
            id: contentArea
            Layout.fillHeight: true
            Layout.fillWidth:  true

            color: "transparent"
            border.color: Material.dividerColor

        }

    }

    /* original:
    Column {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 20

        Rectangle {
            width: parent.width
            height: parent.height - 100
            color: "white"
            border.color: "#cccccc"
            border.width: 2
            radius: 5

            ListView {
                id: beaconListView
                anchors.fill: parent
                anchors.margins: 10
                spacing: 5
                clip: true

                model: ListModel {
                    id: beaconModel
                }

                delegate: Rectangle {
                    width: beaconListView.width
                    height: 40
                    color: index % 2 === 0 ? "#f9f9f9" : "#ffffff"
                    radius: 3

                    Text {
                        anchors.centerIn: parent
                        text: model.address + " | " + model.rssi + " dB | " + model.distance + " m"
                        font.pointSize: 10
                    }
                }

                Text {
                    anchors.centerIn: parent
                    text: "Waiting for beacons..."
                    font.pixelSize: 16
                    color: "#999999"
                    visible: beaconListView.count === 0
                }
            }
        }

        Text {
            text: "Scanning for beacons... (" + beaconListView.count + " found)"
            font.pixelSize: 14
            color: "#666666"
            anchors.horizontalCenter: parent.horizontalCenter
        }
    }
    */
}


