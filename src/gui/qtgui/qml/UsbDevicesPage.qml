import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Emulated USB devices (ported from the Android port, settings/emulatedusbdevices):
// Skylanders portal, Disney Infinity base and LEGO Dimensions toypad.
// A figure is a file in emulatedUSBDevices/<device>: create it from the list, then load it in a slot.
Item {
    id: page

    property string title: qsTr("Emulated USB devices")

    readonly property var kinds: [
        {"kind": 0, "name": qsTr("Skylanders portal"), "setting": "usb_skylander"},
        {"kind": 1, "name": qsTr("Disney Infinity base"), "setting": "usb_infinity"},
        {"kind": 2, "name": qsTr("LEGO Dimensions toypad"), "setting": "usb_dimensions"}
    ]
    property int version: 0   // bumped to refresh the slots

    Connections {
        target: cemu
        function onUsbDevicesChanged() { page.version++ }
    }

    Flickable {
        anchors.fill: parent
        contentWidth: width
        contentHeight: column.implicitHeight + 24
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar {}

        ColumnLayout {
            id: column
            width: parent.width
            spacing: 0

            Label {
                Layout.fillWidth: true
                Layout.margins: 16
                wrapMode: Text.Wrap
                opacity: 0.7
                text: qsTr("Enable a device, then load figures into its slots. Changes to the enabled devices apply the next time a game is started.")
            }

            Repeater {
                model: page.kinds

                delegate: ColumnLayout {
                    id: deviceBox
                    required property var modelData
                    Layout.fillWidth: true
                    spacing: 0

                    SwitchDelegate {
                        Layout.fillWidth: true
                        leftPadding: 16
                        rightPadding: 16
                        text: deviceBox.modelData.name
                        checked: cemu.setting(deviceBox.modelData.setting) !== 0
                        onToggled: cemu.setSetting(deviceBox.modelData.setting, checked ? 1 : 0)
                    }

                    Repeater {
                        model: { page.version; return cemu.usbFigureSlots(deviceBox.modelData.kind) }

                        delegate: ItemDelegate {
                            id: slotRow
                            required property var modelData
                            Layout.fillWidth: true
                            contentItem: RowLayout {
                                spacing: 8
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 0
                                    Label {
                                        text: qsTr("Slot %1").arg(slotRow.modelData.slot + 1)
                                        font.pixelSize: 12
                                        opacity: 0.6
                                    }
                                    Label {
                                        Layout.fillWidth: true
                                        elide: Text.ElideRight
                                        text: slotRow.modelData.name.length > 0 ? slotRow.modelData.name : qsTr("None")
                                    }
                                }
                                Button {
                                    text: qsTr("Load")
                                    onClicked: {
                                        loadDialog.kind = deviceBox.modelData.kind
                                        loadDialog.slot = slotRow.modelData.slot
                                        loadDialog.figures = cemu.usbInstalledFigures(loadDialog.kind)
                                        loadDialog.open()
                                    }
                                }
                                Button {
                                    text: qsTr("Clear")
                                    enabled: slotRow.modelData.name.length > 0
                                    onClicked: cemu.usbClearFigure(deviceBox.modelData.kind, slotRow.modelData.slot)
                                }
                            }
                        }
                    }

                    RowLayout {
                        Layout.leftMargin: 16
                        Layout.topMargin: 6
                        Layout.bottomMargin: 6
                        spacing: 8
                        Button {
                            text: qsTr("Create figure")
                            onClicked: {
                                createDialog.kind = deviceBox.modelData.kind
                                createDialog.catalog = cemu.usbFigureCatalog(createDialog.kind)
                                createDialog.filter = ""
                                createDialog.open()
                            }
                        }
                        Button {
                            text: qsTr("Manage figures")
                            onClicked: {
                                manageDialog.kind = deviceBox.modelData.kind
                                manageDialog.figures = cemu.usbInstalledFigures(manageDialog.kind)
                                manageDialog.open()
                            }
                        }
                    }

                    MenuSeparator { Layout.fillWidth: true }
                }
            }
        }
    }

    // picks an installed figure file for a slot
    Dialog {
        id: loadDialog
        property int kind: 0
        property int slot: 0
        property var figures: []
        title: qsTr("Load figure")
        modal: true
        anchors.centerIn: parent
        width: Math.min(page.width - 32, 460)
        height: Math.min(page.height - 32, 480)
        standardButtons: Dialog.Cancel

        ListView {
            anchors.fill: parent
            clip: true
            model: loadDialog.figures
            ScrollBar.vertical: ScrollBar {}
            delegate: ItemDelegate {
                required property var modelData
                width: ListView.view.width
                text: modelData.name
                onClicked: {
                    cemu.usbLoadFigure(loadDialog.kind, modelData.path, loadDialog.slot)
                    loadDialog.close()
                }
            }
            Label {
                anchors.centerIn: parent
                visible: loadDialog.figures.length === 0
                text: qsTr("No figure yet. Create one first.")
                opacity: 0.7
            }
        }
    }

    // creates a new figure file from the catalog
    Dialog {
        id: createDialog
        property int kind: 0
        property var catalog: []
        property string filter: ""
        title: qsTr("Create figure")
        modal: true
        anchors.centerIn: parent
        width: Math.min(page.width - 32, 460)
        height: Math.min(page.height - 32, 520)
        standardButtons: Dialog.Cancel

        ColumnLayout {
            anchors.fill: parent
            spacing: 6
            TextField {
                Layout.fillWidth: true
                placeholderText: qsTr("Search")
                onTextChanged: createDialog.filter = text.toLowerCase()
            }
            ListView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                ScrollBar.vertical: ScrollBar {}
                model: createDialog.catalog.filter(function(f) {
                    return createDialog.filter.length === 0 || f.name.toLowerCase().indexOf(createDialog.filter) >= 0
                })
                delegate: ItemDelegate {
                    required property var modelData
                    width: ListView.view.width
                    text: modelData.name
                    onClicked: {
                        cemu.usbCreateFigure(createDialog.kind, modelData.number, modelData.variant)
                        createDialog.close()
                    }
                }
            }
        }
    }

    // lists the figure files and deletes them
    Dialog {
        id: manageDialog
        property int kind: 0
        property var figures: []
        title: qsTr("Figures")
        modal: true
        anchors.centerIn: parent
        width: Math.min(page.width - 32, 460)
        height: Math.min(page.height - 32, 480)
        standardButtons: Dialog.Close

        ListView {
            anchors.fill: parent
            clip: true
            model: manageDialog.figures
            ScrollBar.vertical: ScrollBar {}
            delegate: ItemDelegate {
                id: figureRow
                required property var modelData
                width: ListView.view.width
                contentItem: RowLayout {
                    Label {
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                        text: figureRow.modelData.name
                    }
                    ToolButton {
                        text: "✕"
                        onClicked: {
                            cemu.usbDeleteFigureFile(figureRow.modelData.path)
                            manageDialog.figures = cemu.usbInstalledFigures(manageDialog.kind)
                        }
                        Accessible.name: qsTr("Delete")
                    }
                }
            }
            Label {
                anchors.centerIn: parent
                visible: manageDialog.figures.length === 0
                text: qsTr("No figure yet.")
                opacity: 0.7
            }
        }
    }
}
