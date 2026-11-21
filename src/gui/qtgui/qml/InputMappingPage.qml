import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Button mapping of player 1 (ported from the Android port, ControllerInputSettingsScreen / InputMapper).
// Tap a row, then press the button or move the stick of the assigned controller. "Clear" removes the mapping.
Item {
    id: page

    property string title: qsTr("Input mapping")
    readonly property int player: 0

    property var mappings: []
    property int capturingId: 0       // mapping waiting for a press, 0 = none
    property string capturingName: ""

    function reload() {
        mappings = cemu.inputMappings(player)
    }

    Component.onCompleted: reload()

    Connections {
        target: cemu
        function onInputChanged() {
            page.capturingId = 0
            page.reload()
        }
    }

    Timer {
        interval: 50
        repeat: true
        running: page.capturingId !== 0
        onTriggered: cemu.pollInputCapture(page.player, page.capturingId)
    }

    // gives up after a few seconds
    Timer {
        id: captureTimeout
        interval: 8000
        running: page.capturingId !== 0
        onTriggered: page.capturingId = 0
    }

    ListView {
        id: list
        anchors.fill: parent
        clip: true
        model: page.mappings
        ScrollBar.vertical: ScrollBar {}

        header: Label {
            width: list.width
            padding: 16
            wrapMode: Text.Wrap
            font.pixelSize: 13
            opacity: 0.7
            text: page.mappings.length === 0
                  ? qsTr("Assign a controller first (Controllers page).")
                  : qsTr("Tap a button, then press the matching button on your controller.")
        }

        delegate: ItemDelegate {
            id: row
            required property var modelData
            readonly property bool capturing: page.capturingId === modelData.id
            width: ListView.view.width
            highlighted: capturing
            onClicked: {
                cemu.startInputCapture()
                captureTimeout.restart()
                page.capturingId = modelData.id
            }
            contentItem: RowLayout {
                spacing: 8
                Label {
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                    text: row.modelData.name
                }
                Label {
                    opacity: 0.7
                    text: row.capturing ? qsTr("Press a button...")
                                        : (row.modelData.device.length > 0 ? row.modelData.device : "—")
                }
                ToolButton {
                    visible: row.modelData.device.length > 0
                    text: "✕"
                    onClicked: cemu.clearInputMapping(page.player, row.modelData.id)
                    Accessible.name: qsTr("Clear")
                }
            }
        }
    }
}
