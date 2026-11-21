import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: station
    width: 200
    height: 30

    property int stationNumber: 0
    property int rssi: 0
    property string proximity: "Unknown"
    property color color: "green"
    property double level:  rssiToLevel(rssi)
    property var notationImageRef: null
    property bool manualMode: false

    signal manualRssiRequested(int rssi)

    function rssiToLevel(rssi) {
        if (rssi <= -80) return 0;
        if (rssi >= -40) return 1;
        return (rssi + 80) / 40;
    }

    function levelToRssi(level) {
        return Math.round(level * 40 - 80)
    }

    RowLayout {
        anchors.fill: parent

        spacing: 5

        Label {
            id: numberLabel
            text: stationNumber
        }

        Label {
            id: rssiLabel
            text: "Rssi: " + rssi
        }

        Rectangle {
            id: meterRect
            color: "black"
            border.color: Material.dividerColor
            border.width: 1

            Layout.preferredHeight: 20
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignVCenter


            Rectangle {
                id: levelRect
                width: (parent.width - 2*parent.border.width) * station.level
                height: parent.height - 2*parent.border.width


                anchors.left: parent.left
                anchors.topMargin: parent.border.width
                anchors.leftMargin: parent.border.width

                color: station.color
                gradient: Gradient {
                    GradientStop {
                        position: 0.00;
                        color: station.color.lighter(1+level);
                    }
                    GradientStop {
                        position: 1.00;
                        color: station.color.darker(4);
                    }
                }

                Behavior on width {
                    enabled: !manualMode
                    NumberAnimation { duration: 1000 }
                }
            }

            MouseArea {
                anchors.fill: parent
                enabled: manualMode
                cursorShape: manualMode ? Qt.SizeHorCursor : Qt.ArrowCursor
                onPressed:         setVal(mouseX)
                onPositionChanged: if (pressed) setVal(mouseX)
                function setVal(x) {
                    var frac = x / meterRect.width
                    station.manualRssiRequested(station.levelToRssi(Math.max(0.0, Math.min(1.0, frac))))
                }
            }
        }

        Label {
            id: proximityLabel
            text: proximity
            Layout.preferredWidth: contentMetrics.width

            TextMetrics {
                id: contentMetrics
                text: "Immediate"
                font: statusLabel.font
            }


        }

    }

}
