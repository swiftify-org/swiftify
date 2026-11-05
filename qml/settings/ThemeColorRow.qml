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

// one colour of the theme being edited: a swatch and its hex; opened, hue,
// saturation and brightness sliders, a hex field and a reset.
Column {
    id: root

    required property var store
    required property bool editingDark
    required property string colorKey
    required property string label
    property bool open: false
    // the sliders are built again to show what was set outside them
    property bool rebuilding: false
    property bool rebuildPending: false
    property bool slidersPressed: false
    // a grey or a black has no hue in its rgb, and a black no saturation: the
    // last ones are kept for the sliders to come back to
    property real keptHue: 0
    property real keptSaturation: 0
    property bool applying: false
    readonly property color current: {
        root.store.draftVersion
        return root.store.draftColor(root.editingDark, root.colorKey)
    }
    readonly property string hexText: root.store.hex(root.current)

    function keep() {
        if (root.current.hsvSaturation > 0 && root.current.hsvValue > 0) {
            root.keptHue = root.current.hsvHue
        }
        if (root.current.hsvValue > 0) {
            root.keptSaturation = root.current.hsvSaturation
        }
    }
    function apply(hue, saturation, value) {
        root.keptHue = hue
        root.keptSaturation = saturation
        root.applying = true
        root.store.setDraftColor(root.editingDark, root.colorKey, Qt.hsva(Math.min(hue, 0.999), saturation, value, 1))
        root.applying = false
    }
    // never while a slider is held, or the one under the pointer would be
    // destroyed in the middle of its own click: it is done on the release
    function rebuildSliders() {
        if (!root.rebuildPending || root.slidersPressed) {
            return
        }

        root.rebuildPending = false
        root.rebuilding = true

        Qt.callLater(() => {
            root.rebuilding = false
        })
    }
    function refresh() {
        hex.text = root.hexText
        if (sliders.active) {
            root.rebuildPending = true
            Qt.callLater(root.rebuildSliders)
        }
    }
    function commitHex(announce) {
        const before = root.hexText
        if (root.store.setDraftColorHex(root.editingDark, root.colorKey, hex.text)) {
            hex.text = root.hexText
            if (root.hexText !== before) {
                root.refresh()
            }
            return
        }
        if (announce) {
            UiState.toastRequested(root.store.lastError)
        }
        hex.text = root.hexText
    }

    onCurrentChanged: {
        if (!root.applying) {
            root.keep()
        }
    }
    Component.onCompleted: keep()

    width: parent ? parent.width : 0

    RippleButton {
        width: parent.width
        implicitHeight: 52
        leftPadding: 12
        rightPadding: 12
        buttonRadius: Appearance.rounding.small
        onClicked: root.open = !root.open

        contentItem: Item {
            Rectangle {
                id: chip
                anchors.verticalCenter: parent.verticalCenter
                width: 28
                height: 28
                radius: 14
                color: root.current
                border.width: 1
                border.color: Qt.alpha(Appearance.colors.onSurface, 0.2)
            }
            StyledText {
                anchors.left: chip.right
                anchors.leftMargin: 12
                anchors.verticalCenter: parent.verticalCenter
                text: root.label
            }
            StyledText {
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                text: root.hexText
                tabularNumbers: true
                font.pixelSize: Appearance.fontSize.smallie
                color: Appearance.colors.onSurfaceVariant
            }
        }
    }

    Loader {
        id: sliders
        width: parent.width
        active: root.open && !root.rebuilding

        onActiveChanged: {
            if (!active) {
                root.slidersPressed = false
            }
        }

        sourceComponent: Column {
            readonly property bool pressed: hue.pressed || saturation.pressed || brightness.pressed
            onPressedChanged: root.slidersPressed = pressed

            ConfigSlider {
                id: hue
                text: qsTr("Hue")
                from: 0
                to: 360
                stepSize: 1
                value: root.keptHue * 360
                valueText: Math.round(value) + "°"
                onMoved: (value) => root.apply(value / 360, root.keptSaturation, root.current.hsvValue)
                onPressedChanged: {
                    if (!pressed) {
                        Qt.callLater(root.rebuildSliders)
                    }
                }
            }
            ConfigSlider {
                id: saturation
                text: qsTr("Saturation")
                from: 0
                to: 100
                stepSize: 1
                value: root.keptSaturation * 100
                valueText: Math.round(value) + "%"
                onMoved: (value) => root.apply(root.keptHue, value / 100, root.current.hsvValue)
                onPressedChanged: {
                    if (!pressed) {
                        Qt.callLater(root.rebuildSliders)
                    }
                }
            }
            ConfigSlider {
                id: brightness
                text: qsTr("Brightness")
                from: 0
                to: 100
                stepSize: 1
                value: root.current.hsvValue * 100
                valueText: Math.round(value) + "%"
                onMoved: (value) => root.apply(root.keptHue, root.keptSaturation, value / 100)
                onPressedChanged: {
                    if (!pressed) {
                        Qt.callLater(root.rebuildSliders)
                    }
                }
            }
        }
    }

    Item {
        width: parent.width
        height: 56
        visible: root.open

        OutlinedTextField {
            id: hex
            x: 8
            anchors.verticalCenter: parent.verticalCenter
            width: Math.min(220, parent.width - 72)
            placeholder: "#rrggbb"
            onAccepted: root.commitHex(true)
            Component.onCompleted: text = root.hexText

            Connections {
                target: hex.inputItem
                function onActiveFocusChanged() {
                    if (!hex.inputItem.activeFocus) {
                        root.commitHex(false)
                    }
                }
            }
            // a slider moved it: not while it is being typed in
            Connections {
                target: root
                function onHexTextChanged() {
                    if (!hex.inputItem.activeFocus) {
                        hex.text = root.hexText
                    }
                }
            }
        }
        IconButton {
            anchors.right: parent.right
            anchors.rightMargin: 4
            anchors.verticalCenter: parent.verticalCenter
            symbol: "restart_alt"
            size: 32
            iconSize: 18
            tooltip: qsTr("Reset")
            onClicked: {
                root.store.resetDraftColor(root.editingDark, root.colorKey)
                root.refresh()
            }
        }
    }
}