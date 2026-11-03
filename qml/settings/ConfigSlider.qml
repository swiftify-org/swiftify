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
import QtQuick.Templates as T
import Swiftify

// a setting with a range: its name and current value above, the slider below.
Column {
    id: root

    property string text
    property real value
    property real from: 0
    property real to: 1
    property real stepSize: 0
    property string valueText: String(Math.round(value))
    signal moved(real value)
    
    readonly property bool pressed: slider.pressed

    width: parent ? parent.width : 0
    topPadding: 8
    bottomPadding: 8
    spacing: 4
    opacity: enabled ? 1 : 0.38

    Item {
        width: parent.width
        height: 24

        StyledText {
            x: 12
            anchors.verticalCenter: parent.verticalCenter
            text: root.text
        }
        StyledText {
            anchors.right: parent.right
            anchors.rightMargin: 12
            anchors.verticalCenter: parent.verticalCenter
            text: root.valueText
            tabularNumbers: true
            font.pixelSize: Appearance.fontSize.smallie
            color: Appearance.colors.onSurfaceVariant
        }
    }

    StyledSlider {
        id: slider

        x: 12
        width: parent.width - 24
        from: root.from
        to: root.to
        stepSize: root.stepSize
        snapMode: T.Slider.SnapAlways
        value: root.value
        trackThickness: 12
        onMoved: root.moved(value)
    }
}
