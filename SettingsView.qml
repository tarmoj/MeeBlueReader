import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtWebSockets

Rectangle {
    id: settingsView

    property alias userID:      idSpinBox.value
    property alias userName:    nameTextField.text
    property alias serverIP:    serverIPTextField.text
    property alias serverPort:  serverPortSpinBox.value
    property alias connectButton: connectButton
    property alias contentUrl:  contentUrlTextField.text
    property alias selectedRules: rulesComboBox.currentIndex
    property var socketRef: null
    property var fileDownloaderRef: null

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
                text: !socketRef ? qsTr("Connect")
                                 : (socketRef.status === WebSocket.Open ? qsTr("Connected")
                                   : (socketRef.status === WebSocket.Connecting ? qsTr("Connecting...") : qsTr("Connect")))
                enabled: socketRef && socketRef.status !== WebSocket.Open
                onClicked: {
                    if (socketRef) {
                        if (socketRef.status === WebSocket.Connecting) { // disconnect if clicked on connecting state
                            socketRef.active = false
                        } else {
                            socketRef.active = true
                        }
                    }
                }
            }
        }



        // ---- Content download section ----------------------------------------
        Label {
            text: qsTr("Content")
            font.bold: true
            font.pointSize: 14
        }

        Flow {
            id: contentUrlRow
            Layout.fillWidth: true
            spacing: 5

            Label {
                height: contentUrlTextField.height
                text: qsTr("Content URL:")
                verticalAlignment: Text.AlignVCenter
            }
            TextField {
                id: contentUrlTextField
                width: 260
                text: "https://tarmo.uuu.ee/meeblue"
                placeholderText: qsTr("https://…")
            }
        }

        RowLayout {
            spacing: 10

            Label {
                text: qsTr("Rules file:")
                verticalAlignment: Text.AlignVCenter
            }
            ComboBox {
                id: rulesComboBox
                model: ["rules1", "rules2", "rules3", "rules4"]
            }

            Button {
                text: qsTr("Check && Download")
                enabled: fileDownloaderRef !== null
                onClicked: {
                    if (fileDownloaderRef)
                        fileDownloaderRef.checkAndDownload(contentUrlTextField.text,
                                                           rulesComboBox.currentText)
                }
            }
        }

        Button {
            id: refreshRulesButton
            text: qsTr("Refresh rules")
            onClicked: {
                if (fileDownloaderRef)
                    fileDownloaderRef.loadLocalRules(rulesComboBox.currentText)
            }
        }

        Label {
            id: downloadStatusLabel
            text: ""
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        Connections {
            target: fileDownloaderRef
            function onStatusMessage(msg)  { downloadStatusLabel.text = msg }
            function onDownloadError(msg)  { downloadStatusLabel.text = qsTr("Error: ") + msg }
        }

        Item {Layout.fillHeight: true}

    }


}
