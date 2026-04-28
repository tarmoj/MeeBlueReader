import QtQuick
import QtQuick.Window
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import QtQuick.Dialogs
import QtCore
import MeeBlueReader


ApplicationWindow {
    id: app
    width: 506
    height: 900
    minimumWidth: 350
    visible: true
    property string version: "0.6.2"
    title: qsTr("MeeBlue Reader " + version)
    color: Material.background

    flags: Qt.ExpandedClientAreaHint | Qt.NoTitleBarBackgroundHint


    Settings {
        id: appSettings

        property alias userID:        settingsView.userID
        property alias userName:      settingsView.userName
        property alias serverIP:      settingsView.serverIP
        property alias serverPort:    settingsView.serverPort
        property alias contentUrl:    settingsView.contentUrl
        property alias selectedRules: settingsView.selectedRules
    }

    // FileDownloader: downloads rules JSON + media files from configured URL.
    FileDownloader {
        id: fileDownloader

        onRulesLoaded: function(rules) {
            mainPage.eventRules = rules
            console.log("FileDownloader: rulesLoaded, count =", rules.length)
        }
        onStatusMessage: function(msg) {
            console.log("FileDownloader:", msg)
        }
        onDownloadError: function(msg) {
            console.error("FileDownloader error:", msg)
        }
    }

    Component.onCompleted: {
        fileDownloader.loadLocalRules("rules" + (appSettings.selectedRules + 1))
    }

    // When the user changes the rules selection, reload the matching local file.
    Connections {
        target: settingsView
        function onSelectedRulesChanged() {
            fileDownloader.loadLocalRules("rules" + (settingsView.selectedRules + 1))
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

        MainView {
            id: mainPage
            settingsRef: settingsView
            fileDownloaderRef: fileDownloader
        }

        SettingsView {
            id: settingsView
            socketRef: mainPage.wsSocket
            fileDownloaderRef: fileDownloader
        }
    }

    

}
