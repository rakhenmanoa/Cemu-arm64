import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Settings of one game (game profile). Same files as the desktop interface (gameProfiles/<title id>.ini).
// They are saved right away and apply the next time the game is started.
Item {
    id: page

    required property string titleId
    required property string gameName
    property string title: qsTr("Game settings")

    readonly property var quantums: [20000, 45000, 60000, 80000, 100000]

    function reloadAll() {
        cpuMode.currentIndex = cemu.gameSetting(page.titleId, "cpu_mode")
        var q = page.quantums.indexOf(cemu.gameSetting(page.titleId, "thread_quantum"))
        quantum.currentIndex = q >= 0 ? q : 1
        shaderMul.currentIndex = cemu.gameSetting(page.titleId, "accurate_shader_mul")
        precompiled.currentIndex = cemu.gameSetting(page.titleId, "precompiled_shaders")
        sharedLibs.checked = cemu.gameSetting(page.titleId, "load_shared_libraries") !== 0
        runAhead.currentIndex = cemu.gameSetting(page.titleId, "run_ahead_of_gpu")
        limit30.currentIndex = cemu.gameSetting(page.titleId, "frame_limit_30")
    }

    Component.onCompleted: reloadAll()

    component RowLabel: Label {
        Layout.fillWidth: true
        Layout.leftMargin: 16
        Layout.rightMargin: 16
        Layout.topMargin: 10
        wrapMode: Text.Wrap
    }

    component RowHint: Label {
        Layout.fillWidth: true
        Layout.leftMargin: 16
        Layout.rightMargin: 16
        wrapMode: Text.Wrap
        font.pixelSize: 12
        opacity: 0.6
    }

    Flickable {
        anchors.fill: parent
        contentHeight: column.implicitHeight + 24
        clip: true
        ScrollBar.vertical: ScrollBar {}

        ColumnLayout {
            id: column
            width: parent.width
            spacing: 4

            Label {
                Layout.fillWidth: true
                padding: 16
                wrapMode: Text.Wrap
                font.pixelSize: 13
                opacity: 0.7
                text: page.gameName + "\n" + qsTr("These settings only apply to this game, from its next start.")
            }

            RowLabel { text: qsTr("CPU mode") }
            ComboBox {
                id: cpuMode
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                model: [qsTr("Single-core interpreter"), qsTr("Single-core recompiler"), qsTr("Multi-core recompiler"), qsTr("Auto (recommended)")]
                onActivated: function(i) { cemu.setGameSetting(page.titleId, "cpu_mode", i) }
            }
            RowHint { text: qsTr("The recompilers are much faster. Auto uses the multi-core recompiler when the device has enough cores.") }

            RowLabel { text: qsTr("Thread quantum") }
            ComboBox {
                id: quantum
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                model: page.quantums.map(function(v) { return String(v) })
                onActivated: function(i) { cemu.setGameSetting(page.titleId, "thread_quantum", page.quantums[i]) }
            }
            RowHint { text: qsTr("Time slice of the emulated threads. Only change it if a game needs it (45000 is the default).") }

            RowLabel { text: qsTr("Accurate shader multiplication") }
            ComboBox {
                id: shaderMul
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                model: [qsTr("Fast"), qsTr("Accurate (recommended)")]
                onActivated: function(i) { cemu.setGameSetting(page.titleId, "accurate_shader_mul", i) }
            }

            RowLabel { text: qsTr("Precompiled shaders") }
            ComboBox {
                id: precompiled
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                model: [qsTr("Auto"), qsTr("Enabled"), qsTr("Disabled")]
                onActivated: function(i) { cemu.setGameSetting(page.titleId, "precompiled_shaders", i) }
            }

            RowLabel { text: qsTr("Run ahead of the GPU") }
            ComboBox {
                id: runAhead
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                model: [qsTr("Default (global setting)"), qsTr("Off"), qsTr("On")]
                onActivated: function(i) { cemu.setGameSetting(page.titleId, "run_ahead_of_gpu", i) }
            }
            RowHint { text: qsTr("Much faster on heavy scenes (the game no longer waits for the GPU at every frame); may cause rare glitches.") }

            RowLabel { text: qsTr("Limit to 30 FPS") }
            ComboBox {
                id: limit30
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                model: [qsTr("Default (global setting)"), qsTr("Off"), qsTr("On")]
                onActivated: function(i) { cemu.setGameSetting(page.titleId, "frame_limit_30", i) }
            }
            RowHint { text: qsTr("Keeps menus and light scenes at 30 FPS so the game speed does not change.") }

            SwitchDelegate {
                id: sharedLibs
                Layout.fillWidth: true
                Layout.topMargin: 8
                leftPadding: 16
                rightPadding: 16
                text: qsTr("Load shared libraries")
                onToggled: cemu.setGameSetting(page.titleId, "load_shared_libraries", checked ? 1 : 0)
            }

            Button {
                Layout.fillWidth: true
                Layout.margins: 16
                text: qsTr("Reset this game to the default settings")
                onClicked: {
                    cemu.resetGameSettings(page.titleId)
                    page.reloadAll()
                }
            }
        }
    }
}
