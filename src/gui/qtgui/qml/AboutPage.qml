import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// About Cemu (ported from the Android port, about/AboutCemuScreen.kt)
Item {
    id: page

    property string title: qsTr("About Cemu")

    readonly property var libraries: [
        {"name": "zlib", "url": "https://www.zlib.net"},
        {"name": "OpenSSL", "url": "https://www.openssl.org"},
        {"name": "libcurl", "url": "https://curl.haxx.se/libcurl"},
        {"name": "imgui", "url": "https://github.com/ocornut/imgui"},
        {"name": "fontawesome", "url": "https://github.com/FortAwesome/Font-Awesome"},
        {"name": "boost", "url": "https://www.boost.org"},
        {"name": "libusb", "url": "https://libusb.info"},
        {"name": "SDL", "url": "https://www.libsdl.org"},
        {"name": "Qt", "url": "https://www.qt.io"},
        {"name": "Modified ih264 from Android project", "url": "https://github.com/cemu-project/Cemu/tree/main/dependencies/ih264d"}
    ]

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
            spacing: 8

            Label {
                Layout.fillWidth: true
                Layout.margins: 16
                font.pixelSize: 18
                font.bold: true
                text: qsTr("Version: %1").arg(cemu.version)
            }
            Label {
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                wrapMode: Text.Wrap
                text: qsTr("Cemu is a Wii U emulator.\n\nWii and Wii U are trademarks of Nintendo.\nCemu is not affiliated with Nintendo.")
            }
            Label {
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.topMargin: 12
                font.bold: true
                text: qsTr("Used libraries:")
            }
            Repeater {
                model: page.libraries
                delegate: Label {
                    required property var modelData
                    Layout.fillWidth: true
                    Layout.leftMargin: 16
                    Layout.rightMargin: 16
                    wrapMode: Text.Wrap
                    textFormat: Text.StyledText
                    linkColor: palette.link
                    text: modelData.name + " (<a href=\"" + modelData.url + "\">" + modelData.url + "</a>)"
                    onLinkActivated: function(link) { Qt.openUrlExternally(link) }
                }
            }
        }
    }
}
