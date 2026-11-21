import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Wii U accounts (ported from the Android port, settings/account). Stored like the desktop interface.
// The selected account is the one used by the emulator; its data and online status are shown below the list.
Item {
    id: page

    property string title: qsTr("Accounts")

    property var accountList: []
    property var countries: []
    property double activeId: 0
    property var active: null          // data of the active account
    property var editing: ({})         // working copy shown in the form
    property bool dirty: false
    property int networkService: 0
    property var onlineErrors: []

    function reload() {
        accountList = cemu.accounts()
        activeId = cemu.activeAccountId()
        active = null
        for (var i = 0; i < accountList.length; i++) {
            if (accountList[i].id === activeId)
                active = accountList[i]
        }
        if (active === null && accountList.length > 0)
            active = accountList[0]
        if (active !== null) {
            editing = JSON.parse(JSON.stringify(active))
            networkService = cemu.accountNetworkService(active.id)
            onlineErrors = cemu.accountValidationErrors(active.id)
        }
        dirty = false
    }

    function setField(name, value) {
        var copy = editing
        copy[name] = value
        editing = copy
        dirty = true
    }

    Component.onCompleted: {
        countries = cemu.accountCountries()
        reload()
    }

    Connections {
        target: cemu
        function onAccountsChanged() { page.reload() }
    }

    component SectionHeader: Label {
        Layout.fillWidth: true
        Layout.topMargin: 20
        Layout.leftMargin: 16
        Layout.rightMargin: 16
        font.pixelSize: 14
        font.bold: true
        opacity: 0.7
    }

    component FieldLabel: Label {
        Layout.leftMargin: 16
        Layout.rightMargin: 16
        Layout.topMargin: 10
        opacity: 0.8
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

            SectionHeader { text: qsTr("ACCOUNTS") }

            Repeater {
                model: page.accountList

                delegate: ItemDelegate {
                    id: row
                    required property var modelData
                    readonly property bool isActive: modelData.id === page.activeId
                    Layout.fillWidth: true
                    highlighted: isActive
                    onClicked: cemu.setActiveAccount(modelData.id)
                    contentItem: RowLayout {
                        spacing: 8
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 0
                            Label {
                                Layout.fillWidth: true
                                text: row.modelData.miiName
                                elide: Text.ElideRight
                            }
                            Label {
                                Layout.fillWidth: true
                                text: row.modelData.id.toString(16).toUpperCase()
                                      + (row.modelData.valid ? " · " + qsTr("Online account") : "")
                                font.pixelSize: 12
                                opacity: 0.6
                            }
                        }
                        Label {
                            visible: row.isActive
                            text: "✓"
                            font.pixelSize: 20
                        }
                    }
                }
            }

            Button {
                Layout.leftMargin: 16
                Layout.topMargin: 8
                text: qsTr("Create account")
                enabled: page.accountList.length < cemu.maxAccountCount()
                onClicked: {
                    idField.text = cemu.suggestedAccountId().toString(16).toUpperCase()
                    nameField.text = ""
                    createDialog.open()
                }
            }

            // ---- active account ----
            SectionHeader {
                text: qsTr("ACTIVE ACCOUNT")
                visible: page.active !== null
            }

            ColumnLayout {
                Layout.fillWidth: true
                visible: page.active !== null
                spacing: 0

                FieldLabel { text: qsTr("Name") }
                TextField {
                    Layout.fillWidth: true
                    Layout.leftMargin: 16
                    Layout.rightMargin: 16
                    maximumLength: 10
                    text: page.editing.miiName === undefined ? "" : page.editing.miiName
                    onTextEdited: page.setField("miiName", text)
                }

                FieldLabel { text: qsTr("Birthday") }
                RowLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: 16
                    Layout.rightMargin: 16
                    spacing: 8
                    SpinBox {
                        Layout.fillWidth: true
                        from: 1900
                        to: 2100
                        editable: true
                        value: page.editing.birthYear === undefined ? 2000 : page.editing.birthYear
                        onValueModified: page.setField("birthYear", value)
                    }
                    SpinBox {
                        from: 1
                        to: 12
                        editable: true
                        value: page.editing.birthMonth === undefined ? 1 : page.editing.birthMonth
                        onValueModified: page.setField("birthMonth", value)
                    }
                    SpinBox {
                        from: 1
                        to: 31
                        editable: true
                        value: page.editing.birthDay === undefined ? 1 : page.editing.birthDay
                        onValueModified: page.setField("birthDay", value)
                    }
                }

                FieldLabel { text: qsTr("Gender") }
                ComboBox {
                    Layout.fillWidth: true
                    Layout.leftMargin: 16
                    Layout.rightMargin: 16
                    model: [qsTr("Female"), qsTr("Male")]
                    currentIndex: page.editing.gender === 1 ? 1 : 0
                    onActivated: function(index) { page.setField("gender", index) }
                }

                FieldLabel { text: qsTr("Email") }
                TextField {
                    Layout.fillWidth: true
                    Layout.leftMargin: 16
                    Layout.rightMargin: 16
                    inputMethodHints: Qt.ImhEmailCharactersOnly | Qt.ImhNoPredictiveText
                    text: page.editing.email === undefined ? "" : page.editing.email
                    onTextEdited: page.setField("email", text)
                }

                FieldLabel { text: qsTr("Country") }
                ComboBox {
                    Layout.fillWidth: true
                    Layout.leftMargin: 16
                    Layout.rightMargin: 16
                    model: page.countries
                    textRole: "name"
                    valueRole: "index"
                    currentIndex: {
                        for (var i = 0; i < page.countries.length; i++) {
                            if (page.countries[i].index === page.editing.country)
                                return i
                        }
                        return 0
                    }
                    onActivated: function(index) { page.setField("country", page.countries[index].index) }
                }

                RowLayout {
                    Layout.leftMargin: 16
                    Layout.topMargin: 12
                    spacing: 8
                    Button {
                        text: qsTr("Save")
                        enabled: page.dirty
                        onClicked: cemu.saveAccount(page.editing)
                    }
                    Button {
                        text: qsTr("Delete")
                        enabled: page.accountList.length > 1
                        onClicked: deleteDialog.open()
                    }
                }

                SectionHeader { text: qsTr("NETWORK") }

                FieldLabel { text: qsTr("Network service") }
                ComboBox {
                    Layout.fillWidth: true
                    Layout.leftMargin: 16
                    Layout.rightMargin: 16
                    model: [qsTr("Offline"), qsTr("Nintendo"), qsTr("Pretendo"), qsTr("Custom")]
                    currentIndex: page.networkService
                    onActivated: function(index) { cemu.setAccountNetworkService(page.active.id, index) }
                }

                Label {
                    Layout.fillWidth: true
                    Layout.leftMargin: 16
                    Layout.rightMargin: 16
                    Layout.topMargin: 12
                    wrapMode: Text.Wrap
                    text: page.onlineErrors.length === 0 && cemu.otpPresent() && cemu.seepromPresent()
                          ? qsTr("This account can be used online.")
                          : qsTr("This account can not be used online yet:")
                }

                Label {
                    Layout.fillWidth: true
                    Layout.leftMargin: 16
                    Layout.rightMargin: 16
                    Layout.topMargin: 4
                    visible: text.length > 0
                    wrapMode: Text.Wrap
                    opacity: 0.7
                    text: {
                        var lines = page.onlineErrors.slice()
                        if (!cemu.otpPresent() && lines.indexOf(qsTr("otp.bin is missing")) < 0)
                            lines.push(qsTr("otp.bin is missing"))
                        if (!cemu.seepromPresent() && lines.indexOf(qsTr("seeprom.bin is missing")) < 0)
                            lines.push(qsTr("seeprom.bin is missing"))
                        return lines.join("\n")
                    }
                }
            }
        }
    }

    Dialog {
        id: createDialog
        title: qsTr("Create account")
        modal: true
        anchors.centerIn: parent
        width: Math.min(page.width - 32, 420)
        standardButtons: Dialog.Ok | Dialog.Cancel

        ColumnLayout {
            width: createDialog.availableWidth
            spacing: 8
            Label { text: qsTr("Persistent id (hexadecimal)") }
            TextField {
                id: idField
                Layout.fillWidth: true
                inputMethodHints: Qt.ImhNoPredictiveText | Qt.ImhUppercaseOnly
            }
            Label { text: qsTr("Name") }
            TextField {
                id: nameField
                Layout.fillWidth: true
                maximumLength: 10
            }
        }

        onAccepted: cemu.createAccount(idField.text, nameField.text)
    }

    Dialog {
        id: deleteDialog
        title: qsTr("Delete account")
        modal: true
        anchors.centerIn: parent
        width: Math.min(page.width - 32, 420)
        standardButtons: Dialog.Yes | Dialog.No

        Label {
            width: deleteDialog.availableWidth
            wrapMode: Text.Wrap
            text: page.active ? qsTr("Delete the account \"%1\"?").arg(page.active.miiName) : ""
        }

        onAccepted: cemu.deleteAccount(page.active.id)
    }
}
