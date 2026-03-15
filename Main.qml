import QtQuick
import QtQuick.Window
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import QtQuick.Dialogs
import QtCore
import QtWebSockets


ApplicationWindow {
    id: app
    width: 506
    height: 900
    minimumWidth: 350
    visible: true
    property string version: "0.5.0"
    title: qsTr("MeeBlue Reader " + version)
    color: Material.background

    flags: Qt.ExpandedClientAreaHint | Qt.NoTitleBarBackgroundHint


    Settings {
        id: appSettings

        property alias userID:    settingsView.userID
        property alias userName:  settingsView.userName
        property alias serverIP:  settingsView.serverIP
        property alias serverPort: settingsView.serverPort

    }

    // MeeBlueReader station connection

    WebSocket {
        id: socket
        url: "ws://" + settingsView.serverIP + ":" + settingsView.serverPort

        onTextMessageReceived: function (message) {
            console.log("WS message received:", message)
        }

        onStatusChanged: {
            if (socket.status === WebSocket.Error) {
                console.log("WebSocket error:", socket.errorString, url)
                socket.active = false
            } else if (socket.status === WebSocket.Open) {
                console.log("WebSocket open:", url)
            } else if (socket.status === WebSocket.Closed) {
                console.log("WebSocket closed")
                socket.active = false
            } else if (socket.status === WebSocket.Connecting) {
                console.log("WebSocket connecting:", url)
            }
        }
        active: true
    }

    function sendWsMessage(message) {
        if (socket.status === WebSocket.Open) {
            socket.sendTextMessage(message)
        } else {
            console.warn("sendWsMessage: socket not open")
        }
    }


    header: ToolBar {
        id: toolBar
        width: parent.width
        // height: titleLabel.height + 20

        implicitHeight: contentItem.implicitHeight + topPadding + bottomPadding

        background: Rectangle {color: "transparent" }

        topPadding: parent.SafeArea ? parent.SafeArea.margins.top : 10
        bottomPadding: 10

        contentItem:  Item {
            //anchors.fill: parent
            anchors.topMargin: 10
            implicitHeight: titleLabel.implicitHeight + 10

            //Rectangle {color: "darkgreen"; anchors.fill: parent; visible: true}


            Label {
                id: titleLabel
                anchors.centerIn: parent
                //anchors.verticalCenter: parent.verticalCenter
                text: app.title
                font.pointSize: 16
                font.bold: true
                horizontalAlignment: Qt.AlignHCenter

            }

            ToolButton {
                id: menuButton

                anchors.left: parent.left
                anchors.leftMargin: 5
                anchors.verticalCenter: parent.verticalCenter
                icon.source: "qrc:/images/menu.svg"
                onClicked: drawer.opened ? drawer.close() : drawer.open()
            }
        }
    }


    Drawer {
        id: drawer
        //width is automatic
        height: app.height - toolBar.height
        y: toolBar.height
        property int marginLeft: 20

        background: Rectangle {anchors.fill:parent; color: Material.backgroundColor.lighter()}


        ColumnLayout {
            anchors.fill: parent
            spacing: 5
            visible: true


            MenuItem {
                text: qsTr("Info")
                icon.source: "qrc:/images/info.svg"
                onTriggered: {
                    drawer.close()
                    helpDialog.open()
                }
            }


            Item {Layout.fillHeight: true}

        }

    }

    MessageDialog { // maybe replace with normal Dialog later
        id: helpDialog
        buttons: MessageDialog.Ok

        text: qsTr(`
MeeBlue Reader

More info comes here.
Built using Qt framework.

(c) Tarmo Johannes trmjhnns@gmail.com`)

        onButtonClicked: function (button, role) { // does not close on Android otherwise
            switch (button) {
            case MessageDialog.Ok:
                helpDialog.close()
            }
        }
    }



    SwipeView {
        id: swipeView
        anchors.fill: parent

        MainView { }

        SettingsView { id: settingsView }
    }

    

}
