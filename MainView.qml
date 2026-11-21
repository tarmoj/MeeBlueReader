import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtMultimedia
import QtWebSockets


Rectangle {
    id: mainView
    width: 506
    height: 900
    property var settingsRef: null
    property alias wsSocket: socket
    property var fileDownloaderRef: null

    // Rules loaded from rules{N}.json.  Each element is a JS object matching the event schema.
    property var eventRules: []

    // Per-station tracking state: { stationId: { prevRssi, cooldowns: {name: ms} } }
    property var _stationState: ({})

    // When true, station meters are set by dragging instead of by the Bluetooth scanner.
    property bool manualMode: false

    // Content-area stopwatch
    property int elapsedSeconds: 0
    property bool timerRunning: false

    // Content-area media picker
    property bool mediaPickerVisible: false

    gradient: Gradient {
        GradientStop { position: 0.0; color: Material.backgroundColor }
        GradientStop { position: 0.6; color: Material.backgroundColor }
        GradientStop { position: 0.8; color: "#194eb4" }
        GradientStop { position: 1; color: "#750d95" }
    }

    property int strongestStation: 0;

    WebSocket {
        id: socket
        url: settingsRef ? ("ws://" + settingsRef.serverIP + ":" + settingsRef.serverPort) : ""

        // Set after the first successful connection; enables auto-reconnect.
        // Cleared when the user cancels a connection attempt manually.
        property bool wasConnected: false

        onTextMessageReceived: function (message) {
            console.log("WS message received:", message)
        }

        onStatusChanged: {
            if (socket.status === WebSocket.Error) {
                console.log("WebSocket error:", socket.errorString, url)
                socket.active = false
                if (socket.wasConnected) reconnectTimer.restart()
            } else if (socket.status === WebSocket.Open) {
                console.log("WebSocket open:", url)
                socket.wasConnected = true
                reconnectTimer.stop()
            } else if (socket.status === WebSocket.Closed) {
                console.log("WebSocket closed")
                socket.active = false
                if (socket.wasConnected) reconnectTimer.restart()
            } else if (socket.status === WebSocket.Connecting) {
                console.log("WebSocket connecting:", url)
            }
        }
        active: true
    }

    // Tries to reconnect 15 s after the connection was lost — only once there
    // has been at least one successful connection. A failed attempt ends in
    // Error/Closed, which restarts the timer, so it keeps retrying.
    Timer {
        id: reconnectTimer
        interval: 10000
        repeat: false
        onTriggered: {
            if (!socket.wasConnected || socket.status === WebSocket.Open
                    || socket.status === WebSocket.Connecting)
                return
            if (Qt.application.state !== Qt.ApplicationActive)
                return
            console.log("Trying to reconnect WebSocket:", socket.url)
            socket.active = true
        }
    }

    Connections {
        target: Qt.application
        function onStateChanged() {
            if (Qt.application.state === Qt.ApplicationSuspended) {
                console.log("App suspended — closing WebSocket")
                socket.active = false
                reconnectTimer.stop()
            } else if (Qt.application.state === Qt.ApplicationActive) {
                console.log("App active — opening WebSocket")
                socket.active = true
            }
        }
    }

    function sendWsMessage(message) {
        if (socket.status === WebSocket.Open) {
            socket.sendTextMessage(message)
        } else {
            console.warn("sendWsMessage: socket not open")
        }
    }

    function sendStationSnapshot() {
        if (stationModel.count === 0 || socket.status !== WebSocket.Open) {
            return
        }

        var stations = []
        for (var i = 0; i < stationModel.count; i++) {
            var item = stationModel.get(i)
            stations.push({
                stationId: item.stationId,
                rssi: item.rssi,
                proximity: item.proximity,
                beaconIds: item.beaconIds
            })
        }

        var payload = {
            userId: settingsRef ? settingsRef.userID : "",
            userName: settingsRef ? settingsRef.userName : "",
            timestamp: new Date().toISOString(),
            strongestStation: strongestStation,
            stations: stations
        }

        var message = JSON.stringify(payload)
        //console.log("WS OUT:", message)
        sendWsMessage(message)
    }


    // FOR TESING ONLY
    function rssiToProximity(rssi) {
        if (rssi >= -50) return "Immediate"
        if (rssi >= -65) return "Near"
        return "Far"
    }

    function sendTestInfo(sendZeros=false)  {
        var stationIds = [1, 2, 3]
        var stations = []
        var maxRssi = -999
        var strongestId = stationIds[0]

        for (var i = 0; i < stationIds.length; i++) {

            var rssi = sendZeros ? 0 : Math.round(-80 + Math.random() * 50)  // -80 .. -30
            if (rssi > maxRssi) {
                maxRssi = rssi
                strongestId = stationIds[i]
            }
            stations.push({
                stationId: stationIds[i],
                rssi: rssi,
                proximity: rssiToProximity(rssi)
            })
        }

        var payload = {
            userId: settingsRef ? settingsRef.userID : "",
            userName: settingsRef ? settingsRef.userName : "Test",
            timestamp: new Date().toISOString(),
            strongestStation: strongestId,
            stations: stations
        }
        var message = JSON.stringify(payload)
        console.log("WS TEST OUT:", message)
        sendWsMessage(message)
    }

    Timer {
        id: wsBatchTimer
        interval: 300
        repeat: false
        onTriggered: sendStationSnapshot()
    }

    Timer {
        id: stopwatchTimer
        interval: 1000
        repeat: true
        onTriggered: elapsedSeconds++
    }

    // --- Sound / image player ---
    MediaPlayer {
        id: soundPlayer
        audioOutput: AudioOutput { id: soundOutput; volume: settingsRef ? settingsRef.volume : 1.0 }
    }

    // Trigger sound from local data directory (file:// URL)
    function triggerEventSound(fileName) {
        if (!fileName || fileName === "" || fileDownloaderRef === null) return
        const path = "file://" + fileDownloaderRef.localDataPath + "/sounds/" + fileName
        console.log("triggerEventSound:", path)
        soundPlayer.source = path
        soundPlayer.play()
    }

    // Show image from local data directory (file:// URL).
    // Pass "none" to clear the current image without showing a new one.
    function triggerEventImage(fileName) {
        if (!fileName || fileName === "") return
        if (fileName.toLowerCase() === "none") {
            notationImage.source = ""
            return
        }
        if (fileDownloaderRef === null) return
        const path = "file://" + fileDownloaderRef.localDataPath + "/images/" + fileName
        console.log("triggerEventImage:", path)
        eventText.text = ""
        notationImage.source = path
    }

    // Display a text message in the content area.
    // Pass "none" to clear the current text without showing a new one.
    function triggerEventText(msg) {
        if (!msg || msg === "") return
        if (msg.toLowerCase() === "none") {
            eventText.text = ""
            return
        }
        notationImage.source = ""
        eventText.text = msg
    }

    function formatElapsed(sec) {
        const m = Math.floor(sec / 60)
        const s = sec % 60
        return (m < 10 ? "0" + m : m) + ":" + (s < 10 ? "0" + s : s)
    }

    function startContentTimer() {
        elapsedSeconds = 0
        timerRunning = true
        stopwatchTimer.restart()
    }

    function stopContentTimer() {
        stopwatchTimer.stop()
        timerRunning = false
        elapsedSeconds = 0
    }

    // Evaluate all rules against the current station update.
    function evaluateEvents(stationId, rssi) {
        if (eventRules.length === 0) return

        const state = _stationState[stationId] || { prevRssi: -999, cooldowns: {} }
        const prevRssi   = state.prevRssi
        const now        = Date.now()

        for (let i = 0; i < eventRules.length; i++) {
            const rule = eventRules[i]

            // --- Station filter ---
            const rStation = rule.station !== undefined ? parseInt(rule.station) : 0
            if (rStation > 0 && rStation !== stationId) continue
            if (rStation <= 0 && stationId !== strongestStation) continue

            // --- RSSI threshold check ---
            const rRssi = rule.rssi !== undefined ? parseInt(rule.rssi) : 0
            if (rRssi !== 0) {
                const dir = (rule.updDown || "up").toLowerCase()
                const crossedUp   = rssi > rRssi && prevRssi <= rRssi
                const crossedDown = rssi < rRssi && prevRssi >= rRssi
                const matched = (dir === "up" && crossedUp) || (dir === "down" && crossedDown)
                if (!matched) continue
            }

            // --- Retrigger cooldown ---
            const cooldownMs = (rule.retriggerAllowedAfter !== undefined
                                    ? parseFloat(rule.retriggerAllowedAfter) : 0) * 1000
            const ruleName = rule.name || ("rule_" + i)
            const lastFired = state.cooldowns[ruleName] || 0
            if (cooldownMs > 0 && (now - lastFired) < cooldownMs) continue

            // --- All conditions passed: trigger ---
            console.log("Event triggered:", ruleName, "station", stationId)
            if (ruleName==="timer-on-4")
                console.log ("timer rule ", rule.timer)
            triggerEventSound(fileDownloaderRef !== null
                               ? fileDownloaderRef.pickSoundFile(rule.sound || "")
                               : (rule.sound || ""))
            if (rule.text && rule.text !== "")
                triggerEventText(rule.text)
            else
                triggerEventImage(rule.image || "")

            if (rule.timer === "start")
                startContentTimer()
            else if (rule.timer === "stop")
                stopContentTimer()

            // Update cooldown timestamp (must re-read _stationState to avoid stale copy)
            const updated = _stationState[stationId] || state
            updated.cooldowns[ruleName] = now
            const newState = Object.assign({}, _stationState)
            newState[stationId] = updated
            _stationState = newState
        }
    }

    ListModel {
        id: stationModel
    }

    ListModel {
        id: mediaModel
    }

    function refreshMediaList() {
        if (fileDownloaderRef === null) return
        mediaModel.clear()
        mediaModel.append({ "label": "", "fileName": "", "kind": "" })
        const sounds = fileDownloaderRef.listSoundFiles()
        for (let i = 0; i < sounds.length; i++)
            mediaModel.append({ "label": sounds[i], "fileName": sounds[i], "kind": "sound" })
        const images = fileDownloaderRef.listImageFiles()
        for (let j = 0; j < images.length; j++)
            mediaModel.append({ "label": images[j], "fileName": images[j], "kind": "image" })
    }

    // Selects mediaModel[index] in the combo box and triggers its sound/image.
    function activateMediaItem(index) {
        if (index < 0 || index >= mediaModel.count) return
        mediaCombo.currentIndex = index
        const item = mediaModel.get(index)
        if (item.kind === "sound")
            triggerEventSound(item.fileName)
        else if (item.kind === "image")
            triggerEventImage(item.fileName)
    }

    Connections {
        target: fileDownloaderRef
        function onRulesLoaded() { refreshMediaList() }
        function onStatusMessage(msg) { if (msg === qsTr("Done")) refreshMediaList() }
    }

    Component.onCompleted: refreshMediaList()

    // Applies one station reading (from the Bluetooth scanner or a manual drag) to
    // stationModel, recomputes the strongest station, fires matching rules, and
    // sends the updated snapshot — the single path both update sources go through.
    function applyStationReading(stationId, rssi, proximity, beaconIds) {
        // Check if station already exists in the model
        var found = false;
        for (var i = 0; i < stationModel.count; i++) {
            if (stationModel.get(i).stationId === stationId) {
                // Update existing station
                stationModel.set(i, {
                    "stationId": stationId,
                    "rssi": rssi,
                    "proximity": proximity,
                    "beaconIds": beaconIds
                });
                found = true;
                break;
            }
        }

        // Add new station if not found
        if (!found) {
            stationModel.append({
                "stationId": stationId,
                "rssi": rssi,
                "proximity": proximity,
                "beaconIds": beaconIds
            });
        }

        // Find strongest station (highest RSSI)
        var maxRssi = -999;
        var maxStationId = 0;
        for (var j = 0; j < stationModel.count; j++) {
            if (stationModel.get(j).rssi > maxRssi) {
                maxRssi = stationModel.get(j).rssi;
                maxStationId = stationModel.get(j).stationId;
            }
        }
        strongestStation = maxStationId;
        wsBatchTimer.restart()

        // Evaluate rules-based events; must happen after strongestStation is updated
        // and before we write the new prev values.
        evaluateEvents(stationId, rssi)

        // Update previous-state tracking for this station
        var st = Object.assign({}, _stationState)
        var prev = st[stationId] || { prevRssi: -999, cooldowns: {} }
        prev.prevRssi = rssi
        st[stationId] = prev
        _stationState = st
    }

    Connections {
        target: meeBlueReader
        function onNewStationInfo(stationId, rssi, proximity, beaconIds) {
            if (manualMode) return  // ignore the Bluetooth scanner while dragging meters by hand
            applyStationReading(stationId, rssi, proximity, beaconIds)
        }
    }

    // onStrongestStationChanged: {
    //     statusLabel.text = qsTr("Strongest station: ") + strongestStation;
    // }


    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 10

        RowLayout {
            id: statusRow
            spacing: 10

            Button {
                text: socket.status === WebSocket.Open ? qsTr("Connected")
                      : (socket.status === WebSocket.Connecting ? qsTr("Connecting...") : qsTr("Connect"))
                enabled: socket.status !== WebSocket.Open
                onClicked: {
                    if (socket.status === WebSocket.Connecting) { // disconnect if clicked on connecting state
                        socket.wasConnected = false // user cancelled: no auto-reconnect
                        reconnectTimer.stop()
                        socket.active = false
                        return;
                    }
                    socket.active = true
                }
            }

            Label {
                id: statusLabel
                //height: zerosCheck.height
                text: qsTr("Strongest station: ") + strongestStation
                verticalAlignment: Text.AlignVCenter
            }

        }

        RowLayout {
            Layout.fillWidth: true

            Label {
                text: qsTr("Volume")
                verticalAlignment: Text.AlignVCenter
            }
            Slider {
                id: volumeSlider
                Layout.preferredWidth: 150
                from: 0.0
                to: 1.0
                value: soundOutput.volume
                onMoved: if (settingsRef) settingsRef.volume = value
            }
        }



        Label {
            text: qsTr("Stations:")
        }


        GridLayout {
            id: stationsArea
            Layout.fillWidth: true
            columns: 2
            property double cellWidth: width/columns - columns*columnSpacing

            rowSpacing: 5
            columnSpacing: 15

            Repeater {
                id: stationRepeater
                model: stationModel

                StationInfo {
                    stationNumber: model.stationId
                    rssi: model.rssi
                    proximity: model.proximity
                    color: Qt.hsla(model.stationId * 0.1618, 0.7, 0.5, 1)
                    Layout.fillWidth: true
                    notationImageRef: notationImage
                    //strongestStation: mainView.strongestStation
                    manualMode: mainView.manualMode
                    onManualRssiRequested: function(newRssi) {
                        applyStationReading(model.stationId, newRssi,
                                             rssiToProximity(newRssi), model.beaconIds)
                    }
                }
            }
        }

        Rectangle {
            id: contentArea
            Layout.fillHeight: true
            Layout.fillWidth:  true

            color: "transparent"
            border.color: Material.dividerColor

            RowLayout {
                id: contentHeader
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: 5
                spacing: 12

                RowLayout {
                    id: timerRow
                    spacing: 8

                    Label {
                        font.pointSize: 18
                        text: formatElapsed(elapsedSeconds)
                    }
                    Button {
                        text: timerRunning ? qsTr("Stop") : qsTr("Start")
                        onClicked: timerRunning ? stopContentTimer() : startContentTimer()
                    }
                }

                RowLayout {
                    id: mediaRow
                    spacing: 8

                    Label {
                        text: qsTr("Show material")
                        visible: !mediaPickerVisible
                        font.underline: false

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: mediaPickerVisible = true
                        }
                    }
                    ToolButton {
                        text: "<"
                        visible: mediaPickerVisible
                        enabled: mediaCombo.currentIndex > 0
                        onClicked: activateMediaItem(mediaCombo.currentIndex - 1)
                    }
                    ComboBox {
                        id: mediaCombo
                        visible: mediaPickerVisible
                        model: mediaModel
                        textRole: "label"
                        Layout.preferredWidth: 200
                        Layout.maximumWidth: 300
                        Layout.fillWidth: true
                        Layout.preferredHeight: 36
                        background: Item {}
                        onActivated: function(index) { activateMediaItem(index) }
                    }
                    ToolButton {
                        text: qsTr(">")
                        visible: mediaPickerVisible
                        enabled: mediaCombo.currentIndex < mediaModel.count - 1
                        onClicked: activateMediaItem(mediaCombo.currentIndex + 1)
                    }
                    Label {
                        text: qsTr("Hide")
                        visible: mediaPickerVisible
                        font.underline: false

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: mediaPickerVisible = false
                        }
                    }
                }

                Item { Layout.fillWidth: true }

                ToolButton {
                    id: clearButton
                    text: qsTr("Clear")
                    onClicked: {
                        notationImage.source = ""
                        eventText.text = ""
                    }
                }
            }

            Image {
                id: notationImage
                anchors.centerIn: parent
                width: parent.width * 0.8
                height: Math.min(implicitHeight, contentArea.height)
                fillMode: Image.PreserveAspectFit
                source: ""
            }

            Label {
                id: eventText
                anchors.centerIn: parent
                width: parent.width * 0.9
                text: ""
                visible: text !== ""
                font.pointSize: 32
                font.bold: true
                //color: "white"
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
            }

        }

    }

}


