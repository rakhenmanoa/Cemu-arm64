import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Graphic packs that apply to one game: enable/disable them and pick their presets.
// Changes are saved right away and take effect the next time the game is started.
Item {
    id: page

    required property string titleId
    required property string gameName
    property string title: qsTr("Graphic packs")

    property var packs: []
    property string filter: ""
    // "all" lists every pack and can be searched
    readonly property bool searchable: titleId === "all"
    readonly property var shown: {
        var f = page.filter.trim().toLowerCase()
        if (f.length === 0)
            return page.packs
        return page.packs.filter(function(p) {
            return p.name.toLowerCase().indexOf(f) >= 0 || p.path.toLowerCase().indexOf(f) >= 0
        })
    }

    function reload() {
        var y = list.contentY
        page.packs = cemu.graphicPacks(page.titleId)
        list.contentY = y
    }

    Component.onCompleted: reload()

    ListView {
        id: list
        anchors.fill: parent
        clip: true
        model: page.shown
        spacing: 0

        ScrollBar.vertical: ScrollBar {}

        header: ColumnLayout {
            width: list.width
            spacing: 0

            Label {
                Layout.fillWidth: true
                padding: 16
                wrapMode: Text.Wrap
                font.pixelSize: 13
                opacity: 0.7
                text: page.gameName + "\n" + qsTr("Changes apply the next time the game is started.")
            }
            TextField {
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                Layout.bottomMargin: 8
                visible: page.searchable
                placeholderText: qsTr("Search a game or a pack")
                onTextChanged: page.filter = text
            }
        }

        delegate: ColumnLayout {
            id: packRow

            required property var modelData

            width: list.width
            spacing: 0

            SwitchDelegate {
                id: toggle
                Layout.fillWidth: true
                leftPadding: 16
                rightPadding: 16
                text: packRow.modelData.name // also places the switch indicator on the right
                checked: packRow.modelData.enabled
                onToggled: {
                    cemu.setGraphicPackEnabled(packRow.modelData.index, checked)
                    page.reload()
                }
                contentItem: ColumnLayout {
                    spacing: 2
                    Label {
                        Layout.fillWidth: true
                        Layout.rightMargin: 56
                        text: toggle.text
                        font.bold: true
                        wrapMode: Text.Wrap
                    }
                    Label {
                        Layout.fillWidth: true
                        Layout.rightMargin: 56
                        text: packRow.modelData.path
                        font.pixelSize: 12
                        opacity: 0.6
                        wrapMode: Text.Wrap
                    }
                }
            }

            // presets, only for enabled packs
            Repeater {
                model: packRow.modelData.enabled ? packRow.modelData.presets : []

                delegate: ColumnLayout {
                    id: presetRow
                    required property var modelData

                    Layout.fillWidth: true
                    Layout.leftMargin: 32
                    Layout.rightMargin: 16
                    Layout.bottomMargin: 8
                    spacing: 2

                    Label {
                        text: presetRow.modelData.category.length > 0 ? presetRow.modelData.category : qsTr("Preset")
                        font.pixelSize: 13
                        opacity: 0.8
                    }
                    ComboBox {
                        Layout.fillWidth: true
                        model: presetRow.modelData.presets
                        currentIndex: presetRow.modelData.presets.indexOf(presetRow.modelData.active)
                        onActivated: function(index) {
                            cemu.setGraphicPackPreset(packRow.modelData.index, presetRow.modelData.category,
                                                      presetRow.modelData.presets[index])
                            // a preset can show or hide presets of other categories
                            page.reload()
                        }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                color: Qt.rgba(0.5, 0.5, 0.5, 0.2)
            }
        }
    }

    // empty state
    Label {
        anchors.centerIn: parent
        width: Math.min(parent.width - 48, 360)
        visible: page.shown.length === 0
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.Wrap
        font.pixelSize: 15
        text: page.packs.length > 0 ? qsTr("No graphic pack matches.")
            : qsTr("No graphic pack found for this game.\n\n%1 graphic packs are loaded (game id %2). The community graphic packs can be downloaded from the Graphic packs menu.")
                  .arg(cemu.graphicPackCount()).arg(page.titleId)
    }
}
