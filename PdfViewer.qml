import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Pdf

// Single reusable PDF viewer. Its parent is swapped at runtime between the
// Controls overlay layer (fullscreen, covers the app header too) and
// dockParent (a small view embedded in the normal content area) — this keeps
// the loaded document/scroll position intact across the toggle, since it's
// the same Item rather than two separate viewers.
Item {
    id: root

    property string source: ""
    property bool fullscreen: true
    property Item dockParent: null

    visible: source !== ""

    PdfDocument {
        id: doc
        source: root.source
    }

    PdfMultiPageView {
        id: pageView
        anchors.fill: parent
        anchors.topMargin: toolBar.height
        document: doc
    }

    Rectangle {
        id: toolBar
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 40
        color: "#cc000000"

        RowLayout {
            anchors.fill: parent
            anchors.margins: 6
            spacing: 8

            Label {
                text: root.source.substring(root.source.lastIndexOf('/') + 1)
                color: "white"
                elide: Text.ElideLeft
                Layout.fillWidth: true
            }
            ToolButton {
                text: root.fullscreen ? qsTr("Shrink") : qsTr("Expand")
                onClicked: root.fullscreen = !root.fullscreen
            }
            ToolButton {
                text: qsTr("Close")
                onClicked: root.source = ""
            }
        }
    }

    function reparentSelf() {
        if (fullscreen) {
            root.parent = Overlay.overlay
            root.anchors.fill = Overlay.overlay
            root.z = 1000
        } else if (dockParent) {
            root.parent = dockParent
            root.anchors.fill = undefined
            root.anchors.horizontalCenter = dockParent.horizontalCenter
            root.anchors.bottom = dockParent.bottom
            root.width = dockParent.width * 0.8
            root.height = Math.min(dockParent.height, 400)
            root.z = 0
        }
    }

    onFullscreenChanged: reparentSelf()
    Component.onCompleted: reparentSelf()
}
