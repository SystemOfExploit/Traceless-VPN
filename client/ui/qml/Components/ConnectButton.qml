import QtQuick
import QtQuick.Controls
import QtQuick.Shapes

import Style 1.0
import ConnectionState 1.0

import "../Controls2"
import "../Controls2/TextTypes"

Button {
    id: root

    property bool connected: ConnectionController.isConnected
    property bool inProgress: ConnectionController.isConnectionInProgress
    property real labelOpacity: 1

    implicitWidth: 190
    implicitHeight: 190

    padding: 0
    hoverEnabled: true

    text: d.idle ? qsTr("Disconnected") : ConnectionController.connectionStateText
    Accessible.name: d.idle ? qsTr("Connect") : root.text

    Connections {
        target: ConnectionController

        function onPreparingConfig() {
            PageController.showNotificationMessage(qsTr("Unable to disconnect during configuration preparation"))
        }

        function onConnectionStateChanged() {
            d.applyState()
        }
    }

    FontLoader {
        id: statusFont
        source: "qrc:/fonts/Inter-Regular.ttf"
    }

    QtObject {
        id: d

        property bool ready: false
        property string phase: "idle"
        property string shownText: ""
        property real sweep: 360
        property bool spinOn: false
        property bool reverseSpin: false
        property real releaseStarted: 0

        readonly property real ringRadius: (Math.min(root.width, root.height) - 2) / 2
        readonly property bool idle: !root.connected && !root.inProgress
        readonly property int glowMargin: 24
        readonly property string statusFontFamily: statusFont.status === FontLoader.Ready && statusFont.name.length > 0
                                                     ? statusFont.name : "Inter"
        property color ringColor: phase === "idle" ? AmneziaStyle.color.borderMedium : AmneziaStyle.color.textPrimary

        Behavior on ringColor {
            ColorAnimation {
                duration: 420
                easing.type: Easing.OutCubic
            }
        }

        function applySpin() {
            if (!spinOn) {
                spin.stop()
                return
            }
            const from = reverseSpin ? 360 : 0
            const to = reverseSpin ? 0 : 360
            if (spin.running && spin.from === from && spin.to === to) {
                return
            }
            spin.stop()
            spin.from = from
            spin.to = to
            spin.start()
        }

        function beginRelease() {
            if (phase === "release" || phase === "settle") {
                return
            }
            phase = "release"
            releaseStarted = Date.now()
            reverseSpin = true
            spinOn = true
            openArc.restart()
        }

        function finishRelease() {
            if (phase !== "release") {
                return
            }
            phase = "settle"
            openArc.stop()
            settleArc.restart()
        }

        function queueSettle() {
            const elapsed = Date.now() - releaseStarted
            const wait = Math.max(0, 680 - elapsed)
            releaseHold.interval = wait
            releaseHold.restart()
        }

        function applyState() {
            const state = ConnectionController.connectionState
            if (state === ConnectionState.Disconnecting) {
                beginRelease()
                return
            }
            if (state === ConnectionState.Disconnected && (phase === "up" || phase === "release")) {
                if (phase === "up") {
                    beginRelease()
                }
                queueSettle()
                return
            }
            if (state === ConnectionState.Connected) {
                releaseHold.stop()
                openArc.stop()
                settleArc.stop()
                phase = "up"
                reverseSpin = false
                spinOn = false
                sweep = 360
                rotor.rotation = 0
                return
            }
            if (state === ConnectionState.Connecting || state === ConnectionState.Reconnecting || state === ConnectionState.Preparing) {
                releaseHold.stop()
                openArc.stop()
                settleArc.stop()
                phase = "connect"
                reverseSpin = false
                spinOn = true
                sweep = 160
                return
            }
            releaseHold.stop()
            openArc.stop()
            settleArc.stop()
            phase = "idle"
            reverseSpin = false
            spinOn = false
            sweep = 360
            rotor.rotation = 0
        }

        function presentText() {
            if (!ready) {
                shownText = root.text
                root.labelOpacity = 1
                return
            }
            const state = ConnectionController.connectionState
            const closing = state === ConnectionState.Disconnecting
                    || (state === ConnectionState.Disconnected && (phase === "up" || phase === "release" || phase === "settle"))
            if (!closing) {
                textDelay.stop()
                fadeIn.stop()
                shownText = root.text
                root.labelOpacity = 1
                return
            }
            if (shownText === root.text) {
                return
            }
            fadeIn.stop()
            root.labelOpacity = 0
            textDelay.restart()
        }

        onSpinOnChanged: applySpin()
        onReverseSpinChanged: applySpin()
    }

    onTextChanged: d.presentText()

    RotationAnimation {
        id: spin
        target: rotor
        property: "rotation"
        loops: Animation.Infinite
        duration: 1000
        direction: RotationAnimation.Numerical
    }

    NumberAnimation {
        id: openArc
        target: d
        property: "sweep"
        to: 160
        duration: 420
        easing.type: Easing.InOutCubic
    }

    Timer {
        id: releaseHold
        onTriggered: d.finishRelease()
    }

    NumberAnimation {
        id: settleArc
        target: d
        property: "sweep"
        to: 360
        duration: 620
        easing.type: Easing.OutCubic

        onFinished: {
            d.spinOn = false
            rotor.rotation = 0
            d.phase = "idle"
        }
    }

    Timer {
        id: textDelay
        interval: 150
        onTriggered: {
            d.shownText = root.text
            fadeIn.restart()
        }
    }

    NumberAnimation {
        id: fadeIn
        target: root
        property: "labelOpacity"
        to: 1
        duration: 420
        easing.type: Easing.OutCubic
    }

    background: Item {
        implicitWidth: 190
        implicitHeight: 190

        Item {
            anchors.fill: parent

            visible: root.inProgress
            opacity: 0.1

            Item {
                id: diskSource

                anchors.fill: parent
                anchors.margins: -d.glowMargin

                Shape {
                    anchors.fill: parent

                    preferredRendererType: Shape.CurveRenderer

                    ShapePath {
                        fillColor: AmneziaStyle.color.transparent
                        strokeColor: d.ringColor
                        strokeWidth: 1

                        PathAngleArc {
                            centerX: diskSource.width / 2
                            centerY: diskSource.height / 2
                            radiusX: d.ringRadius
                            radiusY: d.ringRadius
                            startAngle: 0
                            sweepAngle: 360
                        }
                    }
                }
            }

            ShadowType {
                anchors.fill: diskSource
                z: -1

                source: diskSource
                color: d.ringColor
                blur: 6
            }
        }

        Item {
            id: rotor
            anchors.fill: parent

            RingGlowType {
                opacity: d.phase === "idle" ? 0 : 1
                visible: opacity > 0
                anchors.fill: parent
                anchors.margins: -d.glowMargin

                color: d.ringColor
                radius: d.ringRadius
                startAngle: 100
                sweepAngle: d.sweep

                Behavior on opacity {
                    NumberAnimation {
                        duration: 420
                        easing.type: Easing.OutCubic
                    }
                }
            }

            Shape {
                id: ringSource

                anchors.fill: parent
                anchors.margins: -d.glowMargin

                preferredRendererType: Shape.CurveRenderer

                ShapePath {
                    fillColor: AmneziaStyle.color.transparent
                    strokeColor: d.ringColor
                    strokeWidth: 2
                    capStyle: ShapePath.RoundCap

                    PathAngleArc {
                        centerX: ringSource.width / 2
                        centerY: ringSource.height / 2
                        radiusX: d.ringRadius
                        radiusY: d.ringRadius
                        startAngle: 100
                        sweepAngle: d.sweep
                    }
                }
            }
        }

        FocusRingType {
            radius: root.width / 2
            visible: root.visualFocus
        }
    }

    contentItem: ButtonTextType {
        id: stateLabel

        text: d.shownText
        opacity: root.labelOpacity

        color: (d.phase === "settle" || d.phase === "release")
                ? (d.shownText === root.text && d.idle ? AmneziaStyle.color.textTertiary : AmneziaStyle.color.textPrimary)
                : (d.idle ? AmneziaStyle.color.textTertiary : AmneziaStyle.color.textPrimary)
        lineHeight: 24 + LanguageUiController.getLineHeightAppend()
        font.family: d.statusFontFamily
        font.pixelSize: 20
        font.weight: Font.Normal
        font.bold: false
        font.letterSpacing: -0.41
        font.hintingPreference: Font.PreferNoHinting
        font.features: ({})
        wrapMode: Text.NoWrap
        fontSizeMode: Text.HorizontalFit
        minimumPixelSize: 14

        leftPadding: 16
        rightPadding: 16

        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }

    HoverHandler {
        cursorShape: Qt.PointingHandCursor
    }

    onClicked: {
        ConnectionController.connectButtonClicked()
    }

    Keys.onEnterPressed: root.clicked()
    Keys.onReturnPressed: root.clicked()

    Component.onCompleted: {
        d.shownText = root.text
        d.applyState()
        d.ready = true
    }
}
