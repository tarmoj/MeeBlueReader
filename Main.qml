import QtQuick

Window {
    width: 640
    height: 480
    visible: true
    title: qsTr("MeeBlue Reader")
    
    Rectangle {
        anchors.fill: parent
        color: "#f0f0f0"
        
        Column {
            anchors.fill: parent
            anchors.margins: 20
            spacing: 20
            
            Text {
                text: "MeeBlue Beacon Reader"
                font.pixelSize: 24
                font.bold: true
                anchors.horizontalCenter: parent.horizontalCenter
            }
            
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
                            text: model.address + " - " + model.rssi + " dB - " + model.distance + " m"
                            font.pixelSize: 16
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
    
    Connections {
        target: meeBlueReader
        function onNewBeaconInfo(address, rssi, distance) {
            // Check if beacon already exists in the model
            var found = false;
            for (var i = 0; i < beaconModel.count; i++) {
                if (beaconModel.get(i).address === address) {
                    // Update existing beacon
                    beaconModel.set(i, {
                        "address": address,
                        "rssi": rssi,
                        "distance": distance.toFixed(2)
                    });
                    found = true;
                    break;
                }
            }
            
            // Add new beacon if not found
            if (!found) {
                beaconModel.append({
                    "address": address,
                    "rssi": rssi,
                    "distance": distance.toFixed(2)
                });
            }
        }
    }
}
