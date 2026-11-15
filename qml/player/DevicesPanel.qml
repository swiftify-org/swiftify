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
import QtQuick.Templates as T
import Swiftify

// Spotify Connect (DevicesPanel): lists "This Computer" (local playback) and
// available Spotify Connect devices on the account. Clicking one transfers
// playback to it.
Item {
    id: root

    property MaterialScheme scheme: Appearance.colors

    component DeviceRow: T.AbstractButton {
        id: row

        required property string deviceName
        required property string subtitle
        required property string symbol
        required property bool active
        property bool restricted: false
        property MaterialScheme scheme: root.scheme

        implicitHeight: 56
        hoverEnabled: true
        focusPolicy: Qt.TabFocus
        enabled: !restricted && !active

        background: Rectangle {
            radius: 14
            color: row.active
                ? row.scheme.secondaryContainer
                : (row.hovered && !row.restricted ? row.scheme.layer1Hover : "transparent")

            Behavior on color {
                ColorAnimation { duration: Appearance.duration(140) }
            }
        }

        contentItem: Item {
            opacity: row.restricted ? 0.38 : 1

            Rectangle {
                id: iconBox
                anchors.left: parent.left
                anchors.leftMargin: 10
                anchors.verticalCenter: parent.verticalCenter
                width: 36
                height: 36
                radius: 10
                color: row.active ? row.scheme.primary : row.scheme.surfaceContainerHigh

                MaterialSymbol {
                    anchors.centerIn: parent
                    text: row.symbol
                    iconSize: 20
                    fill: row.active ? 1 : 0
                    color: row.active ? row.scheme.onPrimary : row.scheme.onSurfaceVariant
                }
            }

            Column {
                anchors.left: iconBox.right
                anchors.leftMargin: 12
                anchors.right: checkIcon.visible ? checkIcon.left : parent.right
                anchors.rightMargin: 10
                anchors.verticalCenter: parent.verticalCenter
                spacing: 2

                StyledText {
                    width: parent.width
                    text: row.deviceName
                    font.pixelSize: Appearance.fontSize.normal
                    font.weight: row.active ? Font.DemiBold : Font.Medium
                    color: row.active ? row.scheme.onSecondaryContainer : row.scheme.onSurface
                    elide: Text.ElideRight
                }

                StyledText {
                    width: parent.width
                    text: row.subtitle
                    font.pixelSize: Appearance.fontSize.smaller
                    color: row.active ? row.scheme.onSecondaryContainer : row.scheme.onSurfaceVariant
                    opacity: row.active ? 0.85 : 1
                    elide: Text.ElideRight
                }
            }

            MaterialSymbol {
                id: checkIcon
                anchors.right: parent.right
                anchors.rightMargin: 12
                anchors.verticalCenter: parent.verticalCenter
                visible: row.active
                text: "check"
                iconSize: 20
                color: row.scheme.onSecondaryContainer
            }
        }
    }

    Item {
        id: headerRow
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.leftMargin: 18
        anchors.rightMargin: 12
        anchors.topMargin: 12
        height: 32

        StyledText {
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            text: qsTr("Devices")
            font.pixelSize: Appearance.fontSize.normal
            font.weight: Font.DemiBold
            color: root.scheme.onSurfaceVariant
        }

        LoadingSpinner {
            anchors.right: refreshButton.left
            anchors.rightMargin: 4
            anchors.verticalCenter: parent.verticalCenter
            width: 18
            height: 18
            running: Player.loadingDevices
            visible: Player.loadingDevices
            color: root.scheme.primary
        }

        IconButton {
            id: refreshButton
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            scheme: root.scheme
            symbol: "refresh"
            size: 30
            iconSize: 18
            enabled: !Player.loadingDevices && !Session.libraryNeedsAuthorizationUpgrade
            tooltip: qsTr("Refresh devices")
            onClicked: Player.refreshDevices()
        }
    }

    ListView {
        id: list
        anchors.fill: parent
        anchors.topMargin: headerRow.y + headerRow.height + 8
        leftMargin: 12
        rightMargin: 12
        bottomMargin: 20
        clip: true
        spacing: 4
        boundsBehavior: Flickable.StopAtBounds
        model: Session.libraryNeedsAuthorizationUpgrade ? 0 : Player.connectDevices.length
        ScrollBar.vertical: StyledScrollBar {}

        header: Column {
            width: list.width - list.leftMargin - list.rightMargin
            spacing: 8

            StyledText {
                leftPadding: 6
                text: qsTr("Current Device")
                font.pixelSize: Appearance.fontSize.smaller
                font.weight: Font.Bold
                color: root.scheme.onSurfaceVariant
            }

            DeviceRow {
                width: parent.width
                deviceName: qsTr("This Computer")
                subtitle: !Player.remoteDeviceActive
                    ? (Player.playing ? qsTr("Playing locally") : qsTr("Swiftify"))
                    : qsTr("Play locally in Swiftify")
                symbol: "computer"
                active: !Player.remoteDeviceActive
                onClicked: Player.selectConnectDevice("")
            }

            Rectangle {
                visible: Session.libraryNeedsAuthorizationUpgrade
                width: parent.width
                height: upgradeCol.implicitHeight + 24
                radius: 14
                color: root.scheme.surfaceContainerHigh

                Column {
                    id: upgradeCol
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.margins: 12
                    spacing: 10

                    StyledText {
                        width: parent.width
                        text: qsTr("Reconnect your Spotify library to grant playback control permissions for Spotify Connect.")
                        font.pixelSize: Appearance.fontSize.smaller
                        color: root.scheme.onSurfaceVariant
                        wrapMode: Text.Wrap
                    }

                    TextButton {
                        variant: TextButton.Filled
                        text: qsTr("Reconnect Library")
                        scheme: root.scheme
                        onClicked: Session.beginLibraryLogin()
                    }
                }
            }

            StyledText {
                visible: !Session.libraryNeedsAuthorizationUpgrade && Player.connectDevices.length > 0
                leftPadding: 6
                topPadding: 8
                bottomPadding: 2
                text: qsTr("Spotify Connect")
                font.pixelSize: Appearance.fontSize.smaller
                font.weight: Font.Bold
                color: root.scheme.onSurfaceVariant
            }
        }

        delegate: DeviceRow {
            required property int index
            readonly property connectDevice device: Player.connectDevices[index]

            width: list.width - list.leftMargin - list.rightMargin
            deviceName: device.name
            subtitle: {
                if (device.isRestricted) {
                    return qsTr("%1 · Restricted").arg(device.type)
                }
                if (device.supportsVolume) {
                    return qsTr("%1 · %2%").arg(device.type).arg(device.volumePercent)
                }
                return device.type
            }
            symbol: device.symbol
            active: Player.remoteDeviceActive && Player.activeConnectDevice.id === device.id
            restricted: device.isRestricted
            onClicked: Player.selectConnectDevice(device.id)
        }
    }

    EmptyState {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 48
        width: parent.width - 32
        Reveal.shown: !Session.libraryNeedsAuthorizationUpgrade
            && Player.connectDevices.length === 0
            && !Player.loadingDevices
        symbol: "devices"
        title: qsTr("No other devices found")
        description: qsTr("Open Spotify on another device on your account to transfer playback.")
        scheme: root.scheme
    }
}
