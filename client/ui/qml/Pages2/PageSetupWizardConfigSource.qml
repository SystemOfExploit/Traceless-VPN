import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

import QtCore

import PageEnum 1.0
import Style 1.0

import "./"
import "../Controls2"
import "../Controls2/TextTypes"
import "../Config"

PageType {
    id: root

    property bool isRestoringBackup: false

    function restoreBackup() {
        if (root.isRestoringBackup) {
            return
        }
        const path = SystemController.getFileName(qsTr("Open backup file"), qsTr("Backup files (*.backup)"))
        if (path === "") {
            return
        }
        root.isRestoringBackup = true
        PageController.showBusyIndicator(true)
        Qt.callLater(function() {
            try {
                SettingsController.restoreAppConfig(path)
            } finally {
                root.isRestoringBackup = false
                PageController.showBusyIndicator(false)
            }
        })
    }

    Connections {
        target: ImportController

        function onQrDecodingFinished() {
            if (Qt.platform.os === "ios") {
                PageController.closePage()
            }
            PageController.goToPage(PageEnum.PageSetupWizardViewConfig)
        }
    }

    FlickableType {
        id: flickable

        anchors.top: parent.top

        contentHeight: content.implicitHeight + 16 + PageController.safeAreaBottomMargin

        ColumnLayout {
            id: content

            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right

            spacing: 0

            NavigationBarType {
                Layout.fillWidth: true

                title: qsTr("Connection")
                backButtonVisible: !PageController.isStartPageVisible() && root.stackView !== null && root.stackView.depth > 1

                IconButtonType {
                    id: moreButton

                    visible: SettingsController.getInstallationUuid() !== "" || PageController.isStartPageVisible()
                    icon.source: "qrc:/images/controls/more-vertical.svg"

                    onClicked: moreActionsDrawer.open()
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16

                spacing: 8

                TextFieldType {
                    id: textKey

                    Layout.fillWidth: true

                    headerText: qsTr("Insert key")
                    buttonText: textField.text === "" ? qsTr("Insert") : ""
                    trailingIconSource: textField.text === "" ? "" : "qrc:/images/controls/close.svg"

                    clickedFunc: function() {
                        textField.text = ""
                        textField.paste()
                    }

                    onTrailingClicked: {
                        textField.clear()
                        textField.forceActiveFocus()
                    }
                }

                ButtonType {
                    id: continueButton

                    Layout.fillWidth: true

                    visible: textKey.textField.text !== ""

                    text: qsTr("Continue")

                    onClicked: {
                        if (ImportController.extractConfigFromData(textKey.textField.text)) {
                            PageController.goToPage(PageEnum.PageSetupWizardViewConfig)
                        }
                    }
                }
            }

            SmallTextType {
                Layout.fillWidth: true
                Layout.topMargin: 14
                Layout.leftMargin: 16
                Layout.rightMargin: 16

                text: qsTr("Other connection options")
            }

            BasicCardType {
                Layout.fillWidth: true
                Layout.topMargin: 12
                Layout.rightMargin: 16
                Layout.leftMargin: 16

                text: qsTr("Self-hosted VPN")
                subtitle: qsTr("Configure Traceless on your own server")

                icon.source: "qrc:/images/controls/server.svg"

                enabled: !root.isRestoringBackup

                onClicked: PageController.goToPage(PageEnum.PageSetupWizardCredentials)
            }

            BasicCardType {
                Layout.fillWidth: true
                Layout.topMargin: 12
                Layout.rightMargin: 16
                Layout.leftMargin: 16

                visible: PageController.isStartPageVisible()

                text: qsTr("Restore from backup")

                icon.source: "qrc:/images/controls/archive-restore.svg"

                enabled: !root.isRestoringBackup

                onClicked: root.restoreBackup()
            }

            BasicCardType {
                Layout.fillWidth: true
                Layout.topMargin: 12
                Layout.rightMargin: 16
                Layout.leftMargin: 16

                text: qsTr("File with connection settings")

                icon.source: "qrc:/images/controls/folder-search-2.svg"

                enabled: !root.isRestoringBackup

                onClicked: {
                    var nameFilter = "Config files (*.vpn *.ovpn *.conf *.json)"
                    var fileName = SystemController.getFileName(qsTr("Open config file"), nameFilter)
                    if (fileName !== "") {
                        if (ImportController.extractConfigFromFile(fileName)) {
                            PageController.goToPage(PageEnum.PageSetupWizardViewConfig)
                        }
                    }
                }
            }

            BasicCardType {
                Layout.fillWidth: true
                Layout.topMargin: 12
                Layout.rightMargin: 16
                Layout.leftMargin: 16

                visible: SettingsController.isCameraPresent()

                text: qsTr("QR code")

                icon.source: "qrc:/images/controls/scan-line.svg"

                enabled: !root.isRestoringBackup

                onClicked: {
                    ImportController.startDecodingQr()
                    if (Qt.platform.os === "ios") {
                        PageController.goToPage(PageEnum.PageSetupWizardQrReader)
                    }
                }
            }
        }
    }

    SheetType {
        id: moreActionsDrawer

        readonly property bool loggingRowsVisible: PageController.isStartPageVisible()

        expandedHeight: moreSheetContent.implicitHeight + 24 + 8 + PageController.safeAreaBottomMargin

        ColumnLayout {
            id: moreSheetContent

            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            spacing: 0

            H2TextType {
                Layout.fillWidth: true
                Layout.topMargin: 16
                Layout.leftMargin: 16
                Layout.rightMargin: 16

                text: qsTr("Settings")
            }

            ListItemType {
                Layout.fillWidth: true
                Layout.topMargin: 32
                Layout.leftMargin: 16
                Layout.rightMargin: 16

                visible: moreActionsDrawer.loggingRowsVisible

                text: qsTr("Enable logs")

                onClicked: {
                    loggingSwitch.toggle()
                    loggingSwitch.toggled()
                }

                SwitchType {
                    id: loggingSwitch

                    focusPolicy: Qt.NoFocus

                    checked: SettingsController.isLoggingEnabled
                    onToggled: {
                        if (checked !== SettingsController.isLoggingEnabled) {
                            SettingsController.isLoggingEnabled = checked
                        }
                    }
                }
            }

            ListItemType {
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16

                visible: moreActionsDrawer.loggingRowsVisible
                showSeparator: false

                text: qsTr("Export client logs")
                trailingIconSource: "qrc:/images/controls/chevron-right.svg"

                onClicked: {
                    var fileName = ""
                    if (GC.isMobile()) {
                        fileName = "Traceless.log"
                    } else {
                        fileName = SystemController.getFileName(qsTr("Save"),
                                                                qsTr("Logs files (*.log)"),
                                                                StandardPaths.standardLocations(StandardPaths.DocumentsLocation) + "/Traceless",
                                                                true,
                                                                ".log")
                    }
                    if (fileName !== "") {
                        PageController.showBusyIndicator(true)
                        SettingsController.exportLogsFile(fileName)
                        PageController.showBusyIndicator(false)
                        PageController.showNotificationMessage(qsTr("Logs file saved"))
                    }
                }
            }

            RowLayout {
                id: supportUuidRow

                Layout.fillWidth: true
                Layout.topMargin: moreActionsDrawer.loggingRowsVisible ? 24 : 32
                Layout.leftMargin: 16
                Layout.rightMargin: 16

                spacing: 16

                visible: SettingsController.getInstallationUuid() !== ""

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.topMargin: 12
                    Layout.bottomMargin: 12

                    spacing: 8

                    SmallTextType {
                        Layout.fillWidth: true

                        text: qsTr("Support tag")
                        color: AmneziaStyle.color.textTertiary
                        elide: Text.ElideRight
                        wrapMode: Text.NoWrap
                    }

                    BodyTextType {
                        id: supportUuidText

                        Layout.fillWidth: true

                        text: SettingsController.getInstallationUuid()
                        color: AmneziaStyle.color.textSecondary
                        elide: Text.ElideRight
                        wrapMode: Text.NoWrap
                    }
                }

                IconButtonType {
                    id: supportUuid

                    icon.source: "qrc:/images/controls/copy.svg"
                    Accessible.name: qsTr("Support tag")

                    onClicked: {
                        GC.copyToClipBoard(supportUuidText.text)
                        PageController.showNotificationMessage(qsTr("Copied"))
                    }
                }
            }
        }
    }
}
