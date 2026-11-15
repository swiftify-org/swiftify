/*
 * Swiftify, a spotify player for desktop
 * Copyright (C) 2026  shred0s
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Effects
import Swiftify

// the floating player (FloatingPlayerBar): transport, what is playing with its
// seek line, and lyrics, queue and volume. its colours follow the cover.
Item {
    id: root

    // the app's theme, as the rest of the window is
    property MaterialScheme scheme: Appearance.colors
    signal immersiveRequested()

    // Give the title useful space before the controls start colliding with it.
    readonly property bool compact: width < 560
    readonly property bool tiny: width < 400
    readonly property real buttonSize: tiny ? 26 : (compact ? 28 : 32)
    readonly property real innerGap: tiny ? 6 : (compact ? 8 : 14)
    readonly property bool titleFits: nowPlaying.width - artwork.width - 8 >= 48
    readonly property bool showsMeter: !compact && Settings.visualizersEnabled && Settings.playerBarVisualizer && Player.hasSong
    property bool progressExpanded: seek.expanded

    implicitHeight: tiny ? 56 : 64
    opacity: Window.active ? 1 : 0.82

    Behavior on opacity {
        NumberAnimation { duration: Appearance.duration(160); easing.type: Easing.OutCubic }
    }

    SoftShadow {
        anchors.fill: surface
        radius: surface.radius
        blur: 18
        offset: Qt.vector2d(0, 6)
        color: Qt.rgba(0, 0, 0, 0.2)
    }

    Rectangle {
        id: surface
        anchors.fill: parent

        // the bar is solid: clicks, hovers and the wheel stop here rather
        // than reaching the page underneath
        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.AllButtons
            hoverEnabled: true
            onWheel: (wheel) => wheel.accepted = true
        }
        radius: 20
        color: root.scheme.surfaceContainerHigh
        border.width: 1
        border.color: Qt.rgba(root.scheme.outlineVariant.r, root.scheme.outlineVariant.g, root.scheme.outlineVariant.b, 0.4)
    }

    Row {
        id: transport
        objectName: "playerTransport"
        anchors.left: parent.left
        anchors.leftMargin: root.compact ? 8 : 12
        anchors.verticalCenter: parent.verticalCenter
        spacing: root.tiny ? 0 : 4

        TransportButton {
            visible: !root.tiny
            anchors.verticalCenter: parent.verticalCenter
            scheme: root.scheme
            effect: TransportButton.Bounce
            symbol: "shuffle"
            toggledShape: "clover4"
            size: root.buttonSize
            iconSize: root.tiny ? 18 : 20
            toggled: Player.shuffle
            fillWhenToggled: false
            tooltip: Player.shuffle ? qsTr("Disable shuffle") : qsTr("Enable shuffle")
            onClicked: Player.toggleShuffle()
        }
        TransportButton {
            anchors.verticalCenter: parent.verticalCenter
            scheme: root.scheme
            effect: TransportButton.NudgeLeft
            symbol: "skip_previous"
            hoverShape: "arrow"
            hoverAngle: -90
            size: root.buttonSize
            iconSize: root.tiny ? 20 : 24
            fillWhenToggled: false
            enabled: Player.canSkipBackward
            tooltip: qsTr("Previous song")
            onClicked: Player.skipBackward()
            alwaysFilled: true
        }

        PlayButton {
            anchors.verticalCenter: parent.verticalCenter
            implicitWidth: root.tiny ? 36 : (root.compact ? 40 : 44)
            implicitHeight: implicitWidth
            enabled: Player.hasSong
            color: root.scheme.primary
            hoverColor: root.scheme.primaryHover
            iconColor: root.scheme.onPrimary
            onClicked: Player.togglePlayback()
        }

        TransportButton {
            anchors.verticalCenter: parent.verticalCenter
            scheme: root.scheme
            effect: TransportButton.NudgeRight
            symbol: "skip_next"
            hoverShape: "arrow"
            hoverAngle: 90
            size: root.buttonSize
            iconSize: root.tiny ? 20 : 24
            enabled: Player.canSkipForward
            tooltip: qsTr("Next song")
            onClicked: Player.skipForward()
            alwaysFilled: true
        }
        TransportButton {
            anchors.verticalCenter: parent.verticalCenter
            scheme: root.scheme
            effect: TransportButton.Bounce
            symbol: Player.repeatMode === Player.RepeatMode.One ? "repeat_one" : "repeat"
            visible: !root.tiny
            toggledShape: Player.repeatMode === Player.RepeatMode.One ? "cookie4" : "cookie6"
            size: root.buttonSize
            iconSize: root.tiny ? 18 : 20
            toggled: Player.repeatMode !== Player.RepeatMode.Off
            fillWhenToggled: false
            enabled: Player.hasSong
            tooltip: Player.repeatMode === Player.RepeatMode.One ? qsTr("Repeat song") : (Player.repeatMode === Player.RepeatMode.All ? qsTr("Repeat queue") : qsTr("Repeat off"))
            onClicked: Player.cycleRepeatMode()
        }
    }

    Item {
        id: nowPlaying
        objectName: "playerNowPlaying"
        anchors.left: transport.right
        anchors.leftMargin: root.innerGap
        anchors.right: utilities.left
        anchors.rightMargin: root.innerGap
        anchors.top: parent.top
        anchors.bottom: parent.bottom

        // the cover: grows on hover and opens the immersive player
        Item {
            id: artwork
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            anchors.verticalCenterOffset: -4
            width: root.tiny ? 30 : 40
            height: width
            z: 2
            scale: artHover.hovered ? 1.12 : 1
            // a new song's cover pops in with a spring
            transform: Scale {
                id: pop
                origin.x: artwork.width / 2
                origin.y: artwork.height / 2
            }

            Connections {
                target: Player
                function onCurrentSongChanged() {
                    if (!Appearance.performanceMode) {
                        popIn.restart()
                    }
                }
            }
            NumberAnimation {
                id: popIn
                target: pop
                properties: "xScale,yScale"
                from: 0.7
                to: 1
                duration: 560
                easing.type: Easing.OutBack
                easing.overshoot: 2.4
            }

            Behavior on scale {
                NumberAnimation {
                    duration: Appearance.motion(artHover.hovered ? 220 : 340)
                    easing.type: artHover.hovered ? Easing.OutBack : Easing.OutCubic
                }
            }

            SoftShadow {
                anchors.fill: parent
                radius: Appearance.sizes.smallArtworkRadius
                blur: 7
                offset: Qt.vector2d(0, 3)
                color: Qt.rgba(0, 0, 0, artHover.hovered ? 0.22 : 0)
            }

            ArtworkImage {
                anchors.fill: parent
                radius: Appearance.sizes.smallArtworkRadius
                source: Player.currentSong.artworkUrl
                crossfade: true
            }

            Rectangle {
                anchors.fill: parent
                radius: Appearance.sizes.smallArtworkRadius
                color: Qt.rgba(0, 0, 0, artHover.hovered && Player.hasSong ? 0.25 : 0)

                Behavior on color {
                    ColorAnimation { duration: Appearance.duration(160) }
                }

                MaterialSymbol {
                    anchors.centerIn: parent
                    text: "open_in_full"
                    iconSize: 16
                    weight: 600
                    color: "white"
                    opacity: artHover.hovered && Player.hasSong ? 1 : 0
                }
            }

            HoverHandler {
                id: artHover
                cursorShape: Player.hasSong ? Qt.PointingHandCursor : Qt.ArrowCursor
            }
            TapHandler {
                acceptedButtons: Qt.LeftButton
                acceptedModifiers: Qt.NoModifier
                onTapped: if (Player.hasSong) root.immersiveRequested()
            }
            TapHandler {
                acceptedButtons: Qt.LeftButton
                acceptedModifiers: Qt.AltModifier
                onTapped: if (Player.hasSong) UiState.showCatchphrase("")
            }
            TapHandler {
                acceptedButtons: Qt.RightButton
                onTapped: artMenu.popupHere()
            }
        }

        Item {
            id: info
            visible: root.titleFits
            clip: true

            // the song blurs away behind the seek line's times while the line is
            // hovered, easing in rather than switching
            property real blurAmount: root.progressExpanded ? 1 : 0

            Behavior on blurAmount {
                NumberAnimation { duration: Appearance.duration(260); easing.type: Easing.OutCubic }
            }

            anchors.left: artwork.right
            anchors.leftMargin: 8
            anchors.right: root.showsMeter ? meter.left : parent.right
            anchors.rightMargin: root.showsMeter ? 9 : 0
            anchors.top: parent.top
            anchors.topMargin: 7
            anchors.bottom: seek.top
            layer.enabled: blurAmount > 0.001
            layer.effect: MultiEffect {
                blurEnabled: true
                blur: info.blurAmount
                blurMax: 40
                saturation: -0.2 * info.blurAmount
            }

            Column {
                anchors.left: parent.left
                anchors.right: more.visible ? more.left : parent.right
                anchors.rightMargin: more.visible ? 4 : 0
                anchors.verticalCenter: parent.verticalCenter
                spacing: 1

                TitleText {
                    width: parent.width
                    text: Player.hasSong ? Player.currentSong.name : qsTr("Not Playing")
                    elide: Text.ElideRight
                    animateChange: true
                    changeDistanceX: 6
                    changeDistanceY: 0
                    font.pixelSize: Appearance.fontSize.small
                    color: root.scheme.onSurface

                    // a gleam sweeps under the title while the song loads
                    // (end4-pC's title scan line). it comes in from past one
                    // end and leaves past the other, fading towards both, so
                    // each sweep runs on into the next; it fades in as
                    // loading starts and out when it is done
                    ShaderEffect {
                        id: scan

                        property color color: root.scheme.primary
                        property real halfWidth: 0.22
                        property real edge: 0.18
                        property real position: -halfWidth

                        anchors.top: parent.bottom
                        anchors.topMargin: 1
                        width: parent.width
                        height: 2
                        opacity: Player.loading ? 1 : 0
                        visible: opacity > 0
                        fragmentShader: "qrc:/shaders/gleam.frag.qsb"

                        Behavior on opacity {
                            NumberAnimation { duration: Appearance.duration(260); easing.type: Easing.InOutSine }
                        }

                        NumberAnimation on position {
                            running: scan.visible && !Appearance.reduceMotion
                            from: -scan.halfWidth
                            to: 1 + scan.halfWidth
                            duration: 1300
                            loops: Animation.Infinite
                        }
                    }
                }
                StyledText {
                    width: parent.width
                    text: Player.hasSong ? Player.currentSong.artists : qsTr("Choose something to play")
                    elide: Text.ElideRight
                    font.pixelSize: Appearance.fontSize.smaller
                    color: root.scheme.onSurfaceVariant
                }
            }

            IconButton {
                id: more
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                Reveal.shown: Player.hasSong && !root.compact
                scheme: root.scheme
                symbol: "more_horiz"
                size: 28
                iconSize: 18
                onClicked: SongMenus.open(Player.currentSong, false)
            }
        }

        // centred on the bar; the seek line stops short of it
        CompactMeter {
            id: meter
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            width: 30
            height: 34
            Reveal.shown: root.showsMeter
            active: Player.playing
        }

        // the seek line starts where the cover ends, so the first seconds of
        // a song stay clickable
        SeekBar {
            id: seek

            // hovered, the line springs up out of the bar and bounces back
            // down when the pointer leaves
            property real lift: root.progressExpanded ? -5 : 0

            Behavior on lift {
                enabled: !Appearance.performanceMode
                Bounce {}
            }

            transform: Translate {
                y: seek.lift
            }
            anchors.left: root.titleFits ? artwork.right : parent.left
            anchors.leftMargin: root.titleFits ? 8 : 0
            anchors.right: parent.right
            anchors.rightMargin: root.showsMeter ? 39 : 0
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 2
            height: 18
            scheme: root.scheme
        }

        // the times ride up with the seek line
        Row {
            anchors.left: seek.left
            anchors.right: seek.right
            anchors.bottom: seek.top
            anchors.bottomMargin: -2
            transform: Translate {
                y: seek.lift
            }
            opacity: root.progressExpanded ? 1 : 0
            visible: opacity > 0

            Behavior on opacity {
                NumberAnimation { duration: Appearance.duration(160) }
            }

            StyledText {
                width: parent.width / 2
                text: Format.playbackTime(seek.displayPositionMs)
                tabularNumbers: true
                font.pixelSize: Appearance.fontSize.smallest
                font.weight: Font.Medium
                color: root.scheme.onSurface
            }
            StyledText {
                width: parent.width / 2
                horizontalAlignment: Text.AlignRight
                text: "−" + Format.playbackTime(Math.max(seek.durationMs - seek.displayPositionMs, 0))
                tabularNumbers: true
                font.pixelSize: Appearance.fontSize.smallest
                font.weight: Font.Medium
                color: root.scheme.onSurface
            }
        }
    }

    Row {
        id: utilities
        objectName: "playerUtilities"
        anchors.right: parent.right
        anchors.rightMargin: root.compact ? 8 : 12
        anchors.verticalCenter: parent.verticalCenter
        spacing: root.tiny ? 0 : 6
        z: 3

        IconButton {
            anchors.verticalCenter: parent.verticalCenter
            scheme: root.scheme
            symbol: "lyrics"
            size: root.buttonSize
            iconSize: root.tiny ? 18 : 20
            toggled: Player.showingLyrics
            tooltip: qsTr("Lyrics")
            onClicked: Player.toggleLyricsPanel()
        }
        IconButton {
            anchors.verticalCenter: parent.verticalCenter
            scheme: root.scheme
            symbol: "queue_music"
            size: root.buttonSize
            iconSize: root.tiny ? 18 : 20
            toggled: Player.showingQueue
            tooltip: qsTr("Playing Next")
            onClicked: Player.toggleQueuePanel()
        }
        IconButton {
            anchors.verticalCenter: parent.verticalCenter
            scheme: root.scheme
            symbol: Player.remoteDeviceActive ? "cast_connected" : "devices"
            size: root.buttonSize
            iconSize: root.tiny ? 18 : 20
            toggled: Player.showingDevices || Player.remoteDeviceActive
            tooltip: Player.remoteDeviceActive
                ? qsTr("Playing on %1").arg(Player.activeConnectDevice.name)
                : qsTr("Connect to a device")
            onClicked: Player.toggleDevicesPanel()
        }
        VolumeControl {
            anchors.verticalCenter: parent.verticalCenter
            scheme: root.scheme
        }
    }

    MenuLoader {
        id: artMenu
        sourceComponent: StyledMenu {
            StyledMenuItem {
                text: qsTr("Open Immersive Player")
                symbol: "open_in_full"
                enabled: Player.hasSong
                onTriggered: root.immersiveRequested()
            }
        }
    }
}
