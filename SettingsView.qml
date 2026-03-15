import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtWebSockets

Rectangle {
    id: settingsView

    property alias userID:    idSpinBox.value
    property alias userName:  nameTextField.text
    property alias serverIP:  serverIPTextField.text
    property alias serverPort: serverPortSpinBox.value
    property alias connectButton: connectButton

    gradient: Gradient {
        GradientStop { position: 0.0; color: Material.backgroundColor }
        GradientStop { position: 0.7; color: Material.backgroundColor }
        GradientStop { position: 0.85; color: "#194eb4" }
        GradientStop { position: 1; color: "#c86be5" }
    }


    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 15


        Label {
            text: qsTr("Settings")
            font.bold: true
            font.pointSize: 18
        }

        RowLayout {
            id: nameRow
            spacing: 5

            Label { text: qsTr("ID:") }
            SpinBox {
                id: idSpinBox
                from: 1
                to: 25
            }
            Item { Layout.preferredWidth: 20 }
            Label { text: qsTr("Name:") }
            TextField {
                id: nameTextField
                placeholderText: qsTr("Enter your name")
            }
        }

        Flow {
            id: serverRow
            Layout.fillWidth: true
            spacing: 5

            Label {
                height: serverIPTextField.height
                text: qsTr("Websocket server IP:")
                verticalAlignment: Text.AlignVCenter
            }
            TextField {
                id: serverIPTextField
                width: 165
                text: qsTr("192.168.1.199")

            }

            Label {
                height: serverPortSpinBox.height
                verticalAlignment: Text.AlignVCenter
                text: qsTr("Port:")
            }
            SpinBox {
                id: serverPortSpinBox
                width: 80
                up.indicator:   Item { width: 0 }
                down.indicator: Item { width: 0 }
                from: 1024
                to: 65535
                editable: true
                value: 6789
            }

            Button {
                id: connectButton
                text: socket.status === WebSocket.Open ?  qsTr("Connected")  :  ( socket.status === WebSocket.Connecting ? qsTr("Connecting...") :  qsTr("Connect") )
                enabled: socket.status !== WebSocket.Open
                onClicked: socket.active = true
            }
        }

        Item {Layout.fillHeight: true}

    }


}
