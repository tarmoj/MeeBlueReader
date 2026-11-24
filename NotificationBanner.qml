import QtQuick
import QtQuick.Controls

// Full-width notification banner shown across the top of the app for a few
// seconds. Reparents itself to Overlay.overlay on completion (same trick as
// PdfViewer.qml's fullscreen mode) so it renders above the ApplicationWindow
// header without any extra window/StackView plumbing.
Item {
    id: root

    property string message: ""
    property int displayMs: 5000

    visible: message !== ""
    height: 110

    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#000033" }
            GradientStop { position: 1.0; color: "#001a66" }
        }
    }

    Label {
        anchors.centerIn: parent
        width: parent.width - 20
        text: root.message
        color: "yellow"
        font.pointSize: 26
        font.bold: true
        wrapMode: Text.WordWrap
        horizontalAlignment: Text.AlignHCenter
    }

    Timer {
        id: hideTimer
        interval: root.displayMs
        onTriggered: root.message = ""
    }

    onMessageChanged: if (message !== "") hideTimer.restart()

    Component.onCompleted: {
        root.parent = Overlay.overlay
        root.anchors.left = Overlay.overlay.left
        root.anchors.right = Overlay.overlay.right
        root.anchors.top = Overlay.overlay.top
        root.z = 2000
    }
}
