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
import Swiftify

// text in the app's face and colours. with `animateChange`, a new string slides
// out and the next one in, like end4's StyledText.
Text {
    id: root

    property bool animateChange: false
    property real changeDistanceX: 0
    property real changeDistanceY: 6
    property bool tabularNumbers: false

    // the face for hebrew text (the interface font has hebrew itself, but this
    // one is used when it is installed); the lyrics set their own. empty keeps
    // the interface font
    property string hebrewFamily: "Fredoka"

    font.family: hebrewFamily.length > 0 && /[\u0590-\u05FF]/.test(text) ? hebrewFamily : Appearance.fontFamily
    font.pixelSize: Appearance.fontSize.small
    font.features: tabularNumbers ? ({ "tnum": 1 }) : ({})
    color: Appearance.colors.onSurface
    linkColor: Appearance.colors.primary
    textFormat: Text.PlainText
    verticalAlignment: Text.AlignVCenter

    transform: Translate {
        id: shift
    }

    // the old text slides out and the new one in
    Behavior on text {
        enabled: root.animateChange && !Appearance.performanceMode

        SequentialAnimation {
            alwaysRunToEnd: true

            ParallelAnimation {
                NumberAnimation { target: shift; property: "x"; to: -root.changeDistanceX; duration: 150; easing.type: Easing.InSine }
                NumberAnimation { target: shift; property: "y"; to: -root.changeDistanceY; duration: 150; easing.type: Easing.InSine }
                NumberAnimation { target: root; property: "opacity"; to: 0; duration: 150; easing.type: Easing.InSine }
            }
            PropertyAction {}
            PropertyAction { target: shift; property: "x"; value: root.changeDistanceX }
            PropertyAction { target: shift; property: "y"; value: root.changeDistanceY }
            ParallelAnimation {
                NumberAnimation { target: shift; property: "x"; to: 0; duration: 150; easing.type: Easing.OutSine }
                NumberAnimation { target: shift; property: "y"; to: 0; duration: 150; easing.type: Easing.OutSine }
                NumberAnimation { target: root; property: "opacity"; to: 1; duration: 150; easing.type: Easing.OutSine }
            }
        }
    }
}
