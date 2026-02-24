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

}


