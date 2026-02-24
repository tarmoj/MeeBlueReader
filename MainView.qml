import QtQuick
import QtQuick.Layouts


Rectangle {
    color: "#f0f0f0"
    width: 506
    height: 900

    Connections {
        target: meeBlueReader
        function onNewStationInfo(stationId, rssi, proximity, beaconIds) {
            // Format the station identifier
            var address = "Station " + stationId + " [" + beaconIds + "]";

            // Check if station already exists in the model
            var found = false;
            for (var i = 0; i < beaconModel.count; i++) {
                if (beaconModel.get(i).address === address) {
                    // Update existing station
                    beaconModel.set(i, {
                        "address": address,
                        "rssi": rssi,
                        "distance": proximity
                    });
                    found = true;
                    break;
                }
            }

            // Add new station if not found
            if (!found) {
                beaconModel.append({
                    "address": address,
                    "rssi": rssi,
                    "distance": proximity
                });
            }
        }
    }

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
}


