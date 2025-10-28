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
            anchors.centerIn: parent
            spacing: 20
            
            Text {
                text: "MeeBlue Beacon Reader"
                font.pixelSize: 24
                font.bold: true
                anchors.horizontalCenter: parent.horizontalCenter
            }
            
            Rectangle {
                width: 500
                height: 150
                color: "white"
                border.color: "#cccccc"
                border.width: 2
                radius: 5
                
                Text {
                    anchors.centerIn: parent
                    text: meeBlueReader.beaconInfo
                    font.pixelSize: 18
                    wrapMode: Text.WordWrap
                    width: parent.width - 20
                    horizontalAlignment: Text.AlignHCenter
                }
            }
            
            Text {
                text: "Scanning for beacons..."
                font.pixelSize: 14
                color: "#666666"
                anchors.horizontalCenter: parent.horizontalCenter
            }
        }
    }
}
