import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import PageEnum 1.0
import Style 1.0

import "./"
import "../Controls2"
import "../Config"
import "../Controls2/TextTypes"
import "../Components"

PageType {
    id: root

    Connections {
        target: UpdateController

        function onUpdateNotFound() {
            PageController.showNotificationMessage(qsTr("You have the latest version of Traceless"))
        }

        function onUpdateCheckFailed() {
            PageController.showNotificationMessage(qsTr("Failed to check for updates"))
        }
    }

    FlickableType {
        id: flickable

        anchors.top: parent.top

        contentHeight: content.implicitHeight

        ColumnLayout {
            id: content

            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right

            spacing: 0

            NavigationBarType {
                Layout.fillWidth: true
            }

            Image {
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: 24
                Layout.preferredWidth: 72
                Layout.preferredHeight: 72
                source: "qrc:/images/traceless.svg"
                sourceSize: Qt.size(144, 144)
            }

            H1TextType {
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: 12

                text: Qt.application.name
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.topMargin: 32
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                Layout.bottomMargin: 24 + PageController.safeAreaBottomMargin

                spacing: 24

                ColumnLayout {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.maximumWidth: content.width - 32

                    spacing: 12

                    RowLayout {
                        Layout.alignment: Qt.AlignHCenter

                        spacing: 4

                        CaptionTextType {
                            text: SettingsController.getAppVersion()

                            MouseArea {
                                property int clickCount: 0
                                anchors.fill: parent
                                onClicked: {
                                    if (clickCount > 10) {
                                        SettingsController.enableDevMode()
                                    } else {
                                        clickCount++
                                    }
                                }
                            }
                        }

                        IconButtonType {
                            size: IconButtonType.Small
                            icon.source: "qrc:/images/controls/copy.svg"
                            Accessible.name: qsTr("Copy version")

                            onClicked: {
                                GC.copyToClipBoard(SettingsController.getAppVersion())
                                PageController.showNotificationMessage(qsTr("Copied"))
                            }
                        }
                    }

                    ButtonType {
                        id: checkUpdatesButton
                        visible: AppUpdatesEnabled

                        Layout.alignment: Qt.AlignHCenter
                        Layout.maximumWidth: content.width - 32

                        variant: ButtonType.Text
                        size: ButtonType.Medium
                        loading: UpdateController.isCheckRunning
                        enabled: !UpdateController.isCheckRunning

                        text: UpdateController.isCheckRunning ? qsTr("Checking...") : qsTr("Check for updates")

                        onClicked: {
                            UpdateController.checkForUpdates()
                        }
                    }

                }
            }
        }
    }
}
