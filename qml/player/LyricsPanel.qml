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
import QtQuick.Controls.Basic
import Swiftify

// the lyrics column (LyricsPanel): the current line full size and bright, the
// rest smaller and faded, kept in the middle as the song moves. a line seeks
// when clicked, if the lyrics are synchronised.
Item {
    id: root

    /// under the immersive player: the lines change, nothing scrolls
    property bool suspended: false
    property MaterialScheme scheme: Appearance.colors

    Column {
        anchors.centerIn: parent
        Reveal.shown: Player.loadingLyrics
        spacing: 12
        LoadingSpinner { anchors.horizontalCenter: parent.horizontalCenter }
        StyledText {
            text: qsTr("Loading lyrics…")
            color: root.scheme.onSurfaceVariant
        }
    }

    EmptyState {
        anchors.centerIn: parent
        width: parent.width - 32
        Reveal.shown: !Player.loadingLyrics && !Player.hasLyrics
        symbol: "lyrics"
        title: qsTr("Lyrics Unavailable")
        description: Player.lyricsMessage.length > 0 ? qsTranslate("Errors", Player.lyricsMessage) : qsTr("i guess bro")
    }

    ListView {
        id: list
        anchors.fill: parent
        Reveal.shown: !Player.loadingLyrics && Player.hasLyrics
        model: Player.lyrics
        spacing: 22
        topMargin: 18
        bottomMargin: 18
        leftMargin: 24
        rightMargin: 24
        clip: true
        // dragged past either end, it stretches and springs back
        boundsBehavior: Flickable.DragAndOvershootBounds
        cacheBuffer: 2000
        // a step moves every line at once; each then springs from where it
        // was to its new place, the ones nearest the sung line first, so the
        // column arrives as a wave rather than a block
        signal stepped(real delta)

        ScrollBar.vertical: StyledScrollBar {}

        function follow(animated: bool) {
            const index = Player.activeLyricIndex
            if (index < 0 || root.suspended) {
                return
            }
            const item = list.itemAtIndex(index)
            if (!item) {
                scroll.stop()
                list.positionViewAtIndex(index, ListView.Center)
                return
            }
            const target = Math.max(list.originY - list.topMargin,
                                    Math.min(item.y + item.height / 2 - list.height / 2,
                                             list.contentHeight + list.originY - list.height + list.bottomMargin))
            const delta = target - list.contentY
            scroll.stop()
            // with reduced motion the column scrolls there on a plain ease
            // out, every line together, rather than as a wave
            if (animated && Appearance.reduceMotion && !Appearance.performanceMode) {
                scroll.to = target
                scroll.start()
                return
            }
            list.contentY = target
            if (animated && !Appearance.reduceMotion) {
                list.stepped(delta)
            }
        }

        NumberAnimation {
            id: scroll
            target: list
            property: "contentY"
            duration: 420
            easing.type: Easing.OutCubic
        }

        Connections {
            target: Player
            function onActiveLyricIndexChanged() { list.follow(true) }
        }
        Connections {
            target: root
            function onSuspendedChanged() { if (!root.suspended) list.follow(false) }
        }

        delegate: Item {
            id: line

            required property int index
            required property string words
            required property int startTimeMs
            readonly property bool active: !Player.lyricsSynchronized || Player.activeLyricIndex === index

            width: list.width - list.leftMargin - list.rightMargin
            height: text.implicitHeight

            property real lag: 0
            transform: Translate { y: line.lag }

            Connections {
                target: list
                function onStepped(delta: real) {
                    line.lag += delta
                    settle.restart()
                }
            }
            SequentialAnimation {
                id: settle
                // a gentle ripple: a short stagger and a small overshoot
                PauseAnimation { duration: Math.min(Math.abs(line.index - Player.activeLyricIndex), 8) * 16 }
                NumberAnimation { target: line; property: "lag"; to: 0; duration: 560; easing.type: Easing.OutBack; easing.overshoot: 0.6 }
            }


            StyledText {
                id: text
                hebrewFamily: "Gveret Levin"
                width: parent.width
                transformOrigin: Item.TopLeft
                text: line.words.length > 0 ? line.words : "♪"
                wrapMode: Text.Wrap
                font.pixelSize: 22
                font.weight: Font.Bold
                color: root.scheme.onSurface
                // the sung line grows on a spring, or with reduced motion on
                // the plain ease out the column scrolls with
                property real springing: line.active ? 1 : 19 / 22
                property real eased: line.active ? 1 : 19 / 22
                scale: Appearance.reduceMotion ? eased : springing
                opacity: line.active ? 1 : 0.34

                Behavior on springing {
                    enabled: !root.suspended && !Appearance.reduceMotion
                    Bounce {}
                }
                Behavior on eased {
                    enabled: !root.suspended && Appearance.reduceMotion && !Appearance.performanceMode
                    NumberAnimation { duration: 420; easing.type: Easing.OutCubic }
                }
                Behavior on opacity {
                    enabled: !root.suspended
                    NumberAnimation { duration: Appearance.duration(280); easing.type: Easing.InOutQuad }
                }
            }

            MouseArea {
                anchors.fill: parent
                enabled: Player.lyricsSynchronized
                cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                onClicked: Player.seekToLyricLine(line.index)
            }
        }

        footer: StyledText {
            topPadding: 18
            text: qsTr("Lyrics provided by ") + Player.lyricsProvider
            font.pixelSize: Appearance.fontSize.smaller
            color: root.scheme.outline
        }
    }
}
