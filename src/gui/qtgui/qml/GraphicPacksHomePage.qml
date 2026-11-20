import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Community graphic packs: download / update them, and open the packs that apply to every game.
// Packs of one game are in the long press menu of the game.
Item {
    id: page

    property string title: qsTr("Graphic packs")
    signal openUniversal()

    ColumnLayout {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 16
        spacing: 12

        Label {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            opacity: 0.7
            text: qsTr("Graphic packs add resolution, FPS and other improvements. They are shared with the desktop interface.")
        }

        Button {
            Layout.fillWidth: true
            text: cemu.downloadingGraphicPacks ? qsTr("Cancel") : qsTr("Download community graphic packs")
            onClicked: cemu.downloadingGraphicPacks ? cemu.cancelGraphicPackDownload() : cemu.downloadGraphicPacks()
        }

        ProgressBar {
            Layout.fillWidth: true
            visible: cemu.downloadingGraphicPacks
            value: cemu.graphicPackDownloadProgress
            indeterminate: cemu.graphicPackDownloadProgress <= 0
        }

        Label {
            Layout.fillWidth: true
            visible: text.length > 0
            wrapMode: Text.Wrap
            text: cemu.graphicPackDownloadText
        }

        Button {
            Layout.fillWidth: true
            text: qsTr("Graphic packs for all games")
            enabled: !cemu.downloadingGraphicPacks
            onClicked: page.openUniversal()
        }
    }
}
