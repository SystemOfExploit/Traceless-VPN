import QtQuick

import PageEnum 1.0

import "./"
import "../Controls2"
import "../Controls2/TextTypes"

PageType {
    id: root

    initialFocusItem: SettingsController.isOnTv() ? startButton : root

    Item {
        id: logoArea

        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.topMargin: 16 + PageController.safeAreaTopMargin
        anchors.bottomMargin: 96 + PageController.safeAreaBottomMargin

        Column {
            anchors.centerIn: parent
            spacing: 24

            Image {
                anchors.horizontalCenter: parent.horizontalCenter
                width: 100
                height: 100
                source: "qrc:/images/traceless.svg"
                sourceSize: Qt.size(200, 200)
                fillMode: Image.PreserveAspectFit
            }

            H1TextType {
                anchors.horizontalCenter: parent.horizontalCenter

                text: Qt.application.name
                horizontalAlignment: Text.AlignHCenter
                lineHeight: 60
                font.family: Qt.platform.os === "windows" ? "Bahnschrift" : "PT Root UI VF"
                font.pixelSize: 44
                font.weight: Font.DemiBold
                font.letterSpacing: 2
            }
        }
    }

    ButtonType {
        id: startButton

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.leftMargin: 16
        anchors.rightMargin: 16
        anchors.bottomMargin: 8 + PageController.safeAreaBottomMargin

        text: qsTr("Let's get started")

        onClicked: {
            PageController.goToPage(PageEnum.PageSetupWizardConfigSource)
        }
    }
}
