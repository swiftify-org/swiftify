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

// the theme studio: the person's own themes, edited with the whole app as the
// preview (Theme shows the draft while one is open) and saved.
Column {
    id: root

    readonly property var store: Theme.customThemes
    readonly property bool dark: Theme.dark
    property string pendingDelete

    // Variant fields by what they colour; the on-colours and containers follow from them
    readonly property var groups: [
        { title: qsTr("Accents"), colors: [
            { key: "primary", label: qsTr("Primary") },
            { key: "secondary", label: qsTr("Secondary") },
            { key: "tertiary", label: qsTr("Tertiary") },
            { key: "error", label: qsTr("Error") } ] },
        { title: qsTr("Surfaces"), colors: [
            { key: "window", label: qsTr("Background") },
            { key: "page", label: qsTr("Surface") },
            { key: "raised", label: qsTr("Raised surface") },
            { key: "raisedMore", label: qsTr("Most raised surface") },
            { key: "lowest", label: qsTr("Lowest surface") } ] },
        { title: qsTr("Text and lines"), colors: [
            { key: "text", label: qsTr("On surface") },
            { key: "subtext", label: qsTr("On surface, quiet") },
            { key: "muted", label: qsTr("Outline") },
            { key: "line", label: qsTr("Divider") } ] }
    ]
    readonly property var checks: [
        { a: "text", b: "window", minimum: 4.5, label: qsTr("Text on background") },
        { a: "text", b: "page", minimum: 4.5, label: qsTr("Text on surface") },
        { a: "subtext", b: "page", minimum: 4.5, label: qsTr("Quiet text on surface") },
        { a: "primary", b: "page", minimum: 3, label: qsTr("Accent on surface") }
    ]

    function colorOf(key) {
        root.store.draftVersion
        return root.store.draftColor(root.dark, key)
    }
    function report(ok) {
        if (!ok) {
            UiState.toastRequested(root.store.lastError)
        }
    }

    width: parent ? parent.width : 0
    spacing: 24

    ContentSection {
        visible: !root.store.editing
        title: qsTr("Theme Studio")
        symbol: "brush"

        Repeater {
            model: root.store.revision >= 0 ? root.store.entries() : []

            Item {
                id: entry

                required property var modelData

                width: parent ? parent.width : 0
                height: 52

                StyledText {
                    x: 12
                    anchors.left: parent.left
                    anchors.right: actions.left
                    anchors.verticalCenter: parent.verticalCenter
                    text: entry.modelData.name
                    elide: Text.ElideRight
                    color: entry.modelData.id === Settings.themeSource ? Appearance.colors.primary : Appearance.colors.onSurface
                }
                Row {
                    id: actions
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter

                    IconButton {
                        symbol: "edit"
                        size: 32
                        iconSize: 18
                        tooltip: qsTr("Edit")
                        onClicked: root.report(root.store.beginEdit(entry.modelData.id))
                    }
                    IconButton {
                        symbol: "content_copy"
                        size: 32
                        iconSize: 18
                        tooltip: qsTr("Duplicate")
                        onClicked: root.report(root.store.duplicate(entry.modelData.id).length > 0)
                    }
                    IconButton {
                        symbol: "delete"
                        size: 32
                        iconSize: 18
                        tooltip: qsTr("Delete")
                        onClicked: {
                            root.pendingDelete = entry.modelData.id
                            deleteDialog.open()
                        }
                    }
                }
            }
        }

        Flow {
            x: 8
            width: parent.width - 16
            spacing: 8
            topPadding: 4

            TextButton {
                symbol: "add"
                text: qsTr("New Theme")
                // from the selected theme; Matugen's and the app's own start from Default
                onClicked: root.store.beginNew(Settings.themeSource)
            }
        }

        StyledDialog {
            id: deleteDialog
            headline: qsTr("Delete this theme?")
            symbol: "delete"
            iconShape: "ghostish"
            body: qsTr("the theme file is removed from this computer.")

            buttons: [
                TextButton {
                    text: qsTr("Delete")
                    colBackground: Appearance.colors.error
                    colBackgroundHover: Appearance.colors.errorHover
                    labelColor: Appearance.colors.onError
                    onClicked: {
                        // the selection moves on only once the theme is really gone
                        const id = root.pendingDelete
                        if (root.store.remove(id)) {
                            if (Settings.themeSource === id) {
                                Settings.themeSource = "spotify"
                            }
                        } else {
                            root.report(false)
                        }
                        deleteDialog.accept()
                    }
                },
                TextButton {
                    variant: TextButton.Text
                    text: qsTr("Cancel")
                    onClicked: deleteDialog.close()
                }
            ]
        }
    }

    ContentSection {
        visible: root.store.editing
        title: qsTr("Edit Theme")
        symbol: "brush"

        OutlinedTextField {
            id: nameField
            x: 8
            width: Math.min(460, parent.width - 16)
            placeholder: qsTr("Theme name")
            onTextEdited: root.store.draftName = text
            Component.onCompleted: text = root.store.draftName

            // another theme opened, or a reset; what is typed is already the draft's name
            Connections {
                target: root.store
                function onDraftChanged() {
                    if (nameField.text !== root.store.draftName) {
                        nameField.text = root.store.draftName
                    }
                }
            }
        }
        Flow {
            x: 8
            width: parent.width - 16
            spacing: 8
            bottomPadding: 8

            TextButton {
                symbol: "save"
                text: qsTr("Save and Use")
                onClicked: {
                    const id = root.store.saveDraft()

                    if (id.length > 0) {
                        Settings.themeSource = id
                        UiState.toastRequested(qsTr("Theme saved and applied"))
                    } else {
                        root.report(false)
                    }
                }
            }
            TextButton {
                variant: TextButton.Tonal
                symbol: "restart_alt"
                text: qsTr("Reset All")
                enabled: root.store.dirty
                onClicked: root.store.resetDraft()
            }
            TextButton {
                variant: TextButton.Text
                text: qsTr("Discard")
                onClicked: root.store.discardDraft()
            }
        }
    }

    Repeater {
        model: root.store.editing ? root.groups : []

        ContentSection {
            id: group

            required property var modelData

            title: modelData.title

            Repeater {
                model: group.modelData.colors

                ThemeColorRow {
                    required property var modelData

                    store: root.store
                    editingDark: root.dark
                    colorKey: modelData.key
                    label: modelData.label
                }
            }
        }
    }

    ContentSection {
        visible: root.store.editing
        title: qsTr("Contrast")
        symbol: "contrast"

        Repeater {
            model: root.store.editing ? root.checks : []

            Item {
                id: check

                required property var modelData
                readonly property real ratio: root.store.contrast(root.colorOf(modelData.a), root.colorOf(modelData.b))
                readonly property bool low: ratio < modelData.minimum

                width: parent ? parent.width : 0
                height: 36

                StyledText {
                    x: 12
                    anchors.verticalCenter: parent.verticalCenter
                    text: check.modelData.label
                }
                StyledText {
                    anchors.right: parent.right
                    anchors.rightMargin: 12
                    anchors.verticalCenter: parent.verticalCenter
                    tabularNumbers: true
                    text: check.ratio.toFixed(1) + ":1" + (check.low ? qsTr(" · hard to read") : "")
                    color: check.low ? Appearance.colors.error : Appearance.colors.onSurfaceVariant
                }
            }
        }
    }
}
