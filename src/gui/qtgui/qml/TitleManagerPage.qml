import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

// Title manager (ported from the Android port, titlemanager): lists the titles Cemu knows (games, updates, DLC),
// installs a title from a folder, deletes titles and converts them to a single .wua file.
Item {
    id: page

    property string title: qsTr("Title manager")

    property var entries: []
    property string filter: ""
    property var pending: null          // entry used by the delete / convert dialogs
    property string resultText: ""

    readonly property var typeNames: [qsTr("Game"), qsTr("Update"), qsTr("DLC"), qsTr("System")]
    readonly property var formatNames: ["", "WUD", "WUA", "NUS", "WUHB"]

    function reload() {
        entries = cemu.titleEntries()
    }

    Component.onCompleted: reload()

    Connections {
        target: cemu
        function onScanningChanged() { page.reload() }
        function onGamePathsChanged() { page.reload() }
        function onTitleInstallFinished(result) {
            page.reload()
            if (result === "finished")
                page.resultText = qsTr("Title installed.")
            else if (result === "notEnoughSpace")
                page.resultText = qsTr("Not enough free space to install the title.")
            else if (result === "cancelled")
                page.resultText = qsTr("Installation cancelled.")
            else
                page.resultText = qsTr("The installation failed.")
            resultDialog.open()
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        RowLayout {
            Layout.fillWidth: true
            Layout.margins: 8
            spacing: 8
            TextField {
                Layout.fillWidth: true
                placeholderText: qsTr("Search")
                onTextChanged: page.filter = text.toLowerCase()
            }
            Button {
                text: qsTr("Install")
                enabled: !cemu.installingTitle
                onClicked: installFolderDialog.open()
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 16
            Layout.rightMargin: 16
            visible: cemu.installingTitle
            Label { text: qsTr("Installing...") }
            ProgressBar {
                Layout.fillWidth: true
                value: cemu.titleInstallProgress
            }
            Button {
                text: qsTr("Cancel")
                onClicked: cemu.cancelTitleInstall()
            }
        }

        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            ScrollBar.vertical: ScrollBar {}
            model: page.entries.filter(function(e) {
                return page.filter.length === 0 || e.name.toLowerCase().indexOf(page.filter) >= 0
                       || e.titleId.indexOf(page.filter) >= 0
            })

            delegate: ItemDelegate {
                id: row
                required property var modelData
                width: ListView.view.width
                onClicked: {
                    page.pending = modelData
                    entryMenu.popup()
                }
                contentItem: ColumnLayout {
                    spacing: 0
                    Label {
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                        text: row.modelData.name.length > 0 ? row.modelData.name : row.modelData.titleId
                    }
                    Label {
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                        font.pixelSize: 12
                        opacity: 0.6
                        text: page.typeNames[row.modelData.type] + " · v" + row.modelData.version
                              + (row.modelData.region.length > 0 ? " · " + row.modelData.region : "")
                              + (page.formatNames[row.modelData.format].length > 0 ? " · " + page.formatNames[row.modelData.format] : "")
                              + "\n" + row.modelData.path
                    }
                }
            }

            Label {
                anchors.centerIn: parent
                visible: list.count === 0
                text: qsTr("No title found. Add a game folder or install a title.")
                opacity: 0.7
            }
        }
    }

    Menu {
        id: entryMenu
        MenuItem {
            text: qsTr("Convert to WUA")
            enabled: page.pending !== null && page.pending.type !== 3 && page.pending.format !== 2
            onTriggered: {
                wuaDialog.entry = page.pending
                wuaFileDialog.suggestedName = cemu.wuaSuggestedName(page.pending.titleId, page.pending.uid)
                wuaFileDialog.open()
            }
        }
        MenuItem {
            text: qsTr("Delete")
            onTriggered: deleteDialog.open()
        }
    }

    FolderDialog {
        id: installFolderDialog
        title: qsTr("Select the folder of the title (it contains code, content and meta)")
        onAccepted: {
            var info = cemu.checkInstallTitle(selectedFolder)
            installConfirm.folder = selectedFolder
            installConfirm.info = info
            installConfirm.open()
        }
    }

    FileDialog {
        id: wuaFileDialog
        property string suggestedName: ""
        fileMode: FileDialog.SaveFile
        // the dialog opens in the folder of the last file; the suggested name is shown in the title
        title: suggestedName.length > 0 ? qsTr("Save as %1").arg(suggestedName) : qsTr("Save as")
        nameFilters: [qsTr("Wii U archive (*.wua)")]
        defaultSuffix: "wua"
        onAccepted: {
            if (cemu.startWuaConversion(wuaDialog.entry.titleId, wuaDialog.entry.uid, selectedFile))
                wuaDialog.open()
        }
    }

    // install confirmation with the result of the check
    Dialog {
        id: installConfirm
        property url folder
        property var info: ({"status": "invalid", "name": "", "target": ""})
        title: qsTr("Install title")
        modal: true
        anchors.centerIn: parent
        width: Math.min(page.width - 32, 460)
        standardButtons: info.status === "invalid" || info.status === "differentType" || info.status === "newerInstalled"
                         ? Dialog.Close : Dialog.Ok | Dialog.Cancel

        Label {
            width: installConfirm.availableWidth
            wrapMode: Text.Wrap
            text: {
                switch (installConfirm.info.status) {
                case "ok": return qsTr("Install \"%1\"?").arg(installConfirm.info.name)
                case "overwrite": return qsTr("An older version of \"%1\" is installed. It will be replaced.").arg(installConfirm.info.name)
                case "sameVersion": return qsTr("This version of \"%1\" is already installed. Install it again?").arg(installConfirm.info.name)
                case "newerInstalled": return qsTr("A newer version of \"%1\" is already installed.").arg(installConfirm.info.name)
                case "differentType": return qsTr("Something of another type is installed at the same place.")
                default: return qsTr("This folder is not a valid title.")
                }
            }
        }
        onAccepted: cemu.installTitle(folder)
    }

    Dialog {
        id: deleteDialog
        title: qsTr("Delete title")
        modal: true
        anchors.centerIn: parent
        width: Math.min(page.width - 32, 460)
        standardButtons: Dialog.Yes | Dialog.No
        Label {
            width: deleteDialog.availableWidth
            wrapMode: Text.Wrap
            text: page.pending ? qsTr("Delete \"%1\"?\n\n%2").arg(page.pending.name).arg(page.pending.path) : ""
        }
        onAccepted: {
            cemu.deleteTitle(page.pending.path)
            page.reload()
        }
    }

    Dialog {
        id: resultDialog
        title: qsTr("Title manager")
        modal: true
        anchors.centerIn: parent
        width: Math.min(page.width - 32, 460)
        standardButtons: Dialog.Ok
        Label {
            width: resultDialog.availableWidth
            wrapMode: Text.Wrap
            text: page.resultText
        }
    }

    // progress of the .wua conversion
    Dialog {
        id: wuaDialog
        property var entry: null
        property var progress: cemu.wuaProgress()
        readonly property bool running: ["starting", "collecting", "compressing", "finalizing"].indexOf(progress.stage) >= 0
        title: qsTr("Convert to WUA")
        modal: true
        closePolicy: Popup.NoAutoClose
        anchors.centerIn: parent
        width: Math.min(page.width - 32, 460)
        standardButtons: running ? Dialog.NoButton : Dialog.Close

        Timer {
            interval: 300
            repeat: true
            running: wuaDialog.visible
            onTriggered: wuaDialog.progress = cemu.wuaProgress()
        }

        ColumnLayout {
            width: wuaDialog.availableWidth
            spacing: 8
            Label {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                text: {
                    switch (wuaDialog.progress.stage) {
                    case "collecting": return qsTr("Collecting files...")
                    case "compressing": return qsTr("Compressing...")
                    case "finalizing": return qsTr("Finalizing...")
                    case "finished": return qsTr("Done.")
                    case "cancelled": return qsTr("Cancelled.")
                    case "error": return qsTr("The conversion failed.")
                    default: return qsTr("Starting...")
                    }
                }
            }
            ProgressBar {
                Layout.fillWidth: true
                visible: wuaDialog.running
                from: 0
                to: Math.max(1, wuaDialog.progress.total)
                value: wuaDialog.progress.done
                indeterminate: wuaDialog.progress.stage !== "compressing"
            }
            Button {
                visible: wuaDialog.running
                text: qsTr("Cancel")
                onClicked: cemu.cancelWuaConversion()
            }
        }
        onClosed: page.reload()
    }
}
