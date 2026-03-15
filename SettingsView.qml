import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

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
        spacing: 10


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

        RowLayout {
            id: serverRow
            Layout.fillWidth: true
            spacing: 5

            Label { text: qsTr("Websocket server IP:") }
            TextField {
                id: serverIPTextField
                Layout.preferredWidth: 180
                text: qsTr("192.168.1.199")
            }

            Label { text: qsTr("Port:") }
            SpinBox {
                id: serverPortSpinBox
                from: 1024
                to: 65535
                value: 6789
            }

            Button {
                id: connectButton
                text: qsTr("Connect")
            }
        }

        Item {Layout.fillHeight: true}

    }


}
