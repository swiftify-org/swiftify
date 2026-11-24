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
import Qt.labs.platform as Platform
import Swiftify

// the main window: drawer, a content sheet with the top bar and the page, the
// inspector beside it, and the player floating over the bottom.
ApplicationWindow {
    id: window

    property real sidebarWidth: 232
    /// the window's size as its content lays out in it, smaller than the
    /// window's own when zoomed in (ctrl +) and larger when zoomed out
    readonly property real zoom: Settings.zoom
    readonly property real layoutWidth: width / zoom
    readonly property real layoutHeight: height / zoom
    readonly property real effectiveSidebarWidth: Math.max(132, Math.min(sidebarWidth, layoutWidth * 0.28))
    readonly property bool inAppWindowControls: !App.macOS && (App.windows ? !Settings.nativeTitleBar : Settings.showWindowControls)
    /// the strip the traffic lights sit in, kept clear above the sidebar
    readonly property real titleBarInset: App.macOS ? Math.max(28, shell.SafeArea.margins.top)
                                                 : inAppWindowControls && !Settings.windowControlsOnRight ? 36 : 0

    width: 1280
    height: 780
    minimumWidth: Math.ceil(526 * zoom)
    minimumHeight: Math.ceil(410 * zoom)
    readonly property bool rightPanelFits: layoutWidth - effectiveSidebarWidth - Appearance.sizes.sideSheetWidth - 32 >= 404

    function closeCrowdedPanels() {
        if (!rightPanelFits) {
            Player.showingLyrics = false
            Player.showingQueue = false
        }
    }
    onRightPanelFitsChanged: closeCrowdedPanels()
    Connections {
        target: Player
        function onPanelsChanged() { window.closeCrowdedPanels() }
    }
    title: "Swiftify"
    // on linux the app draws its whole frame. on macos the content runs under
    // a transparent title bar that keeps the traffic lights and nothing else.
    flags: App.macOS ? Qt.Window | Qt.ExpandedClientAreaHint | Qt.NoTitleBarBackgroundHint
                     : App.windows ? Qt.Window | Qt.WindowTitleHint | Qt.WindowSystemMenuHint
                                             | Qt.WindowMinMaxButtonsHint | Qt.WindowCloseButtonHint
                     : Qt.Window | Qt.FramelessWindowHint
    // since qt 6.9 an ApplicationWindow keeps its content inside the safe
    // area, which on macos starts below the title bar; the app runs right up
    // to the top and keeps the traffic lights' strip clear itself
    topPadding: 0
    leftPadding: 0
    rightPadding: 0
    bottomPadding: 0
    color: App.windows && immersiveReveal > 0 ? PlayerStyle.darkScheme.layer0 : Appearance.colors.layer0
    // shown and hidden by hand: closing hides it to the tray
    visible: true

    Component.onCompleted: {
        if (App.windows) App.setNativeTitleBar(window, Settings.nativeTitleBar)
        findZoomedItems()
    }

    Connections {
        target: Settings
        function onNativeTitleBarChanged() {
            if (App.windows) App.setNativeTitleBar(window, Settings.nativeTitleBar)
        }
    }

    /// the window is on screen: not hidden to the tray, nor minimised. what it
    /// holds stops while it is not, as an item does not know its window is
    /// hidden: the shapes, waves and meters went on asking a hidden window for
    /// frames it never drew, and the display link woke the app for them on every
    /// tick
    readonly property bool onScreen: visible && visibility !== Window.Minimized

    function reveal() {
        window.show()
        window.raise()
        window.requestActivate()
    }

    // closing the window is not quitting: it hides, the music plays on, and
    // the dock (or the tray) brings it back, as on any mac app. a quit (cmd-q,
    // the dock's or the tray's) closes it for good. elsewhere, with nothing
    // playing and no tray to come back from, closing it quits.
    onClosing: (close) => {
        if (App.quitting) {
            return
        }
        if (App.macOS || App.windows || Player.playing || (Settings.trayIcon && App.trayAvailable)) {
            close.accepted = false
            window.hide()
            App.trimMemory()
        } else {
            Qt.quit()
        }
    }

    Connections {
        target: App
        function onRaiseRequested() { window.reveal() }
        function onImmersiveRequested() {
            UiState.immersiveOpen = true
            window.reveal()
        }
        function onImmersiveCloseRequested() {
            UiState.immersiveOpen = false
        }
    }

    Connections {
        target: UiState
        function onToastRequested(message: string) { toast.show(message) }
        function onDeletePlaylistRequested(playlist: playlist) {
            deleteDialog.active = true
            // through a name: a line opening with a bracket would run on
            // from the one before, as `true(...)`
            const dialog = deleteDialog.item as DeletePlaylistDialog
            dialog.ask(playlist)
        }
        function onCreatePlaylistRequested() {
            playlistEditor.active = true
            const editor = playlistEditor.item as PlaylistEditor
            editor.openForCreate()
        }
        function onCreateLocalPlaylistRequested(first: var) {
            playlistEditor.active = true
            const editor = playlistEditor.item as PlaylistEditor
            editor.openForCreateLocal(first)
        }
        function onEditLocalTrackRequested(song: song) {
            trackEditor.active = true
            const editor = trackEditor.item as LocalTrackEditor
            editor.openFor(song)
        }
        function onSettingsRequested() {
            // the settings show over this window: it comes back first, from
            // the tray
            if (!window.visible) {
                window.reveal()
            }
            const open = settings.item as SettingsDialog
            if (open) {
                open.open()
            } else {
                settings.active = true
            }
        }
        function onEditPlaylistRequested() {
            playlistEditor.active = true
            const editor = playlistEditor.item as PlaylistEditor
            editor.openForEdit(Library.selectedPlaylist)
        }
    }

    // tracks copied into a playlist made here, as far as they went
    Connections {
        target: LocalFiles
        function onTracksAdded(playlistId: string, count: int, problem: string) {
            const added = count > 0 ? qsTr("Tracks added: %1").arg(count) : ""
            const said = [added, problem].filter(part => part.length > 0).join(" ")
            if (said.length > 0) {
                toast.show(said)
            }
        }
    }

    Connections {
        target: SongMenus
        function onRequested(song: song, allowsPlaylistRemoval: bool) {
            songMenu.openFor(song, allowsPlaylistRemoval)
        }
    }

    MacTitleBar {
        target: window
    }

    // the zoom scales the window's content and its overlay (the popups,
    // menus and dialogs) from the top left corner, and lays each out at the
    // window's size divided by it. qt sizes both to the window itself as it
    // resizes, so each is set back whenever it changes
    readonly property real zoomStep: 0.1
    function setZoom(value: real) {
        Settings.zoom = Math.round(Math.min(2, Math.max(0.5, value)) * 10) / 10
        toast.show(qsTr("Zoom ") + Math.round(Settings.zoom * 100) + "%")
    }
    property list<Item> zoomedItems
    function fitZoomed(item: Item) {
        // worked out here rather than read off layoutWidth, whose binding
        // has not caught up yet while the window's own size changes
        const scale = Settings.zoom
        item.transformOrigin = Item.TopLeft
        item.scale = scale
        item.smooth = true
        if (Math.abs(item.width - width / scale) > 0.01) {
            item.width = width / scale
        }
        if (Math.abs(item.height - height / scale) > 0.01) {
            item.height = height / scale
        }
    }
    function fitAllZoomed() {
        for (const item of zoomedItems) {
            fitZoomed(item)
        }
    }
    onZoomChanged: fitAllZoomed()
    onWidthChanged: fitAllZoomed()
    onHeightChanged: fitAllZoomed()
    Instantiator {
        model: window.zoomedItems
        delegate: Connections {
            required property Item modelData
            target: modelData
            function onWidthChanged() { window.fitZoomed(modelData) }
            function onHeightChanged() { window.fitZoomed(modelData) }
        }
    }
    // the content's control and the overlay, the root item's children
    function findZoomedItems() {
        const root = contentItem.parent.parent
        zoomedItems = root.children.filter(item => item === contentItem.parent || String(item).startsWith("QQuickOverlay"))
        fitAllZoomed()
    }

    Item {
        id: shell
        anchors.fill: parent
        // nothing under the opaque immersive player is drawn, or animates
        visible: window.onScreen && !Session.evaluatingStoredLogin && window.immersiveReveal < 1

        // the top edge moves the window, as a title bar would: the gutter
        // above the page and, on macos, the traffic lights' strip. it sits
        // under everything else, so it only gets what nothing above takes.
        Item {
            width: parent.width
            height: Math.max(window.titleBarInset, window.inAppWindowControls ? 36 : 0) + 8

            DragHandler {
                target: null
                onActiveChanged: if (active) window.startSystemMove()
            }
            TapHandler {
                onDoubleTapped: window.visibility === Window.Maximized ? window.showNormal() : window.showMaximized()
            }
        }

        Sidebar {
            id: sidebar
            objectName: "mainSidebar"
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.topMargin: 8 + window.titleBarInset
            anchors.leftMargin: 8
            width: window.effectiveSidebarWidth
        }

        // drag the drawer's edge to resize it, within the mac app's 205-270
        MouseArea {
            anchors.left: sidebar.right
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: 8
            cursorShape: Qt.SizeHorCursor
            property real startX
            property real startWidth
            onPressed: (mouse) => {
                startX = mouse.x + x
                startWidth = window.sidebarWidth
            }
            onPositionChanged: (mouse) => {
                window.sidebarWidth = Math.max(132, Math.min(270, startWidth + mouse.x + x - startX))
            }
        }

        Rectangle {
            id: sheet
            objectName: "mainSheet"
            anchors.left: sidebar.right
            anchors.leftMargin: 8
            anchors.right: inspector.left
            anchors.rightMargin: 8
            anchors.top: parent.top
            anchors.topMargin: 8
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 8
            radius: Appearance.rounding.large
            color: Appearance.colors.layer1

            // the pages fill the sheet: they scroll up under the top bar,
            // which has no background, only its buttons on their shapes
            PageHost {
                id: pageContent
                anchors.fill: parent
                clip: true
            }

            // a cover on its way between a card and a page's hero, over the
            // pages but under the top bar and the player
            Item {
                anchors.fill: parent
                clip: true

                HeroFlight {
                    anchors.fill: parent
                }
            }

            // the pages clip square: what reaches into a corner is cut to
            // the sheet's rounding
            CornerCaps {
                anchors.fill: parent
                radius: sheet.radius
                color: window.color
            }

            TopBar {
                id: topBar
                objectName: "mainTopBar"
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                window: window
                backdropSource: pageContent
                windowControlsOnRight: window.inAppWindowControls && Settings.windowControlsOnRight
                inspectorShown: inspector.shown
            }

            Snackbar {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.top: topBar.bottom
                anchors.topMargin: 4
                message: qsTranslate("Errors", Errors.message)
                serial: Errors.serial
                onDismissed: Errors.clear()
            }

            PlayerBar {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 12
                width: Math.max(0, Math.min(720, sheet.width - 36))
                Reveal.shown: !UiState.needsSetup
                onImmersiveRequested: UiState.immersiveOpen = true
            }
        }

        // the inspector: lyrics or the queue beside the page, outside the
        // navigation, hidden while the immersive player is open.
        Item {
            id: inspector
            objectName: "mainInspector"
            readonly property bool shown: (Player.showingLyrics || Player.showingQueue) && !UiState.immersiveOpen
                                          && !UiState.needsSetup && window.rightPanelFits
            anchors.right: parent.right
            anchors.rightMargin: shown ? 8 : 0
            anchors.top: parent.top
            anchors.topMargin: 8 + (window.inAppWindowControls && Settings.windowControlsOnRight ? 36 : 0)
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 8
            width: shown ? Appearance.sizes.sideSheetWidth : 0
            clip: true
            // shut, it is hidden rather than only cut to nothing: the lyrics
            // in it went on following the song, unseen
            visible: width > 0.5

            // opens and shuts on a spring, so the page beside it bounces too
            Behavior on width {
                enabled: !Appearance.performanceMode && window.rightPanelFits
                SpringAnimation { spring: 4; damping: 0.32; epsilon: 0.5 }
            }

            Rectangle {
                width: Appearance.sizes.sideSheetWidth
                height: parent.height
                radius: Appearance.rounding.large
                color: Appearance.colors.layer1

                LyricsPanel {
                    objectName: "mainLyricsPanel"
                    anchors.fill: parent
                    Reveal.shown: Player.showingLyrics
                    suspended: UiState.immersiveOpen
                }
                QueuePanel {
                    anchors.fill: parent
                    Reveal.shown: Player.showingQueue
                }
            }
        }

        // music files and folders dropped onto the window go into Local Files
        DropArea {
            id: musicDrop
            anchors.fill: parent
            enabled: Session.libraryConnected
            onEntered: (drag) => drag.accepted = drag.hasUrls
            onDropped: (drop) => {
                if (!drop.hasUrls) {
                    return
                }
                drop.accept(Qt.CopyAction)
                // onto a playlist made here, they are copied into it
                if (Library.page === Library.Page.Playlist && Library.selectedPlaylist.isLocal) {
                    LocalFiles.addFiles(Library.selectedPlaylistId, drop.urls)
                    toast.show(qsTr("Adding to “%1”…").arg(Library.selectedPlaylist.name))
                } else if (LocalFiles.addSources(drop.urls) > 0) {
                    Library.showLocalTracks()
                    toast.show(qsTr("Looking for music…"))
                } else {
                    toast.show(qsTr("No new music there"))
                }
            }

            Rectangle {
                anchors.fill: parent
                anchors.margins: 8
                radius: Appearance.rounding.large
                color: Qt.alpha(Appearance.colors.primaryContainer, 0.88)
                border.width: 2
                border.color: Appearance.colors.primary
                opacity: musicDrop.containsDrag ? 1 : 0
                visible: opacity > 0

                Behavior on opacity {
                    NumberAnimation { duration: Appearance.duration(160); easing.type: Easing.OutCubic }
                }

                Column {
                    anchors.centerIn: parent
                    spacing: 14

                    ShapeIcon {
                        anchors.horizontalCenter: parent.horizontalCenter
                        size: 72
                        iconSize: 36
                        shape: "clamShell"
                        symbol: "folder_open"
                        color: Appearance.colors.primary
                        iconColor: Appearance.colors.onPrimary
                        hovered: musicDrop.containsDrag
                    }
                    TitleText {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: qsTr("Add to Local Files")
                        font.pixelSize: Appearance.fontSize.headline
                        color: Appearance.colors.onPrimaryContainer
                    }
                }
            }
        }
    }

    // built for the start, and let go of once it has faded away. whether it
    // still shows is passed on a moment later: read straight off the view,
    // letting it go changed what decided to let it go, a binding loop
    Loader {
        id: startup
        property bool fading: false
        anchors.fill: parent
        active: Session.evaluatingStoredLogin || fading
        sourceComponent: StartupView {
            Reveal.shown: Session.evaluatingStoredLogin
            // held from when it is built until it has faded away
            Component.onCompleted: startup.fading = true
            onVisibleChanged: {
                if (!visible) {
                    Qt.callLater(() => startup.fading = Session.evaluatingStoredLogin)
                }
            }
        }
    }

    /// how far the immersive player has opened, 0 to 1: what decides
    /// whether it is built and whether the window under it is drawn
    property real immersiveReveal: 0
    /// the shape it shows through: its size, as a share of the size that
    /// covers the window, and its turn in radians
    property real revealReach: 0
    property real revealSpin: 0
    /// with reduced motion the player fades in and out over the window,
    /// rather than opening through a shape; performance mode just shows it
    readonly property bool immersiveFades: Appearance.reduceMotion && !Appearance.performanceMode
    property real immersiveFade: 1

    /// a duration of the opening or closing shape's, none while it fades
    function shapeMotion(milliseconds: int): int {
        return immersiveFades ? 0 : Appearance.motion(milliseconds)
    }
    /// the shape has opened: the player shows, or, fading, fades in once it
    /// is built (the loader starts it otherwise), so it never fades in empty
    function finishOpening() {
        if (!immersiveFades) {
            immersiveReveal = 1
        } else if (immersive.status === Loader.Ready) {
            fadeInMotion.restart()
        }
    }
    /// where the turn starts from as it opens, so a reopening mid-close
    /// carries on from where the shape is rather than jumping
    property real revealSpinStart: 0
    /// the shape it opens through, picked afresh each time it opens
    property string revealShape: "cookie9"
    readonly property list<string> revealShapes: ["cookie9", "cookie6", "clover4", "clover8", "sunny", "verySunny",
                                                  "softBurst", "burst", "flower", "puffy", "pentagon", "gem"]

    Connections {
        target: UiState
        function onImmersiveOpenChanged() {
            if (UiState.immersiveOpen) {
                // with no shapes it opens through a plain circle
                window.revealShape = Settings.noShapes ? "circle"
                    : window.revealShapes[Math.floor(Math.random() * window.revealShapes.length)]
            }
            openMotion.stop()
            fadeInMotion.stop()
            closeMotion.stop()
            window.revealSpinStart = window.revealSpin % (2 * Math.PI)
            window.revealSpin = window.revealSpinStart
            if (UiState.immersiveOpen) {
                openMotion.start()
            } else {
                closeMotion.start()
            }
        }
    }

    // opening goes in two stages, 0.6 s in all: a small shape bounces out
    // of the middle, overshooting and settling, holds a moment, then winds
    // back a touch and bursts out over the whole window. all the while it
    // turns 0.8 of a turn, evenly. with reduced motion the shape is the
    // whole window at once and the player fades in over it instead; in
    // performance mode it is simply there
    SequentialAnimation {
        id: openMotion
        PropertyAction { target: window; property: "immersiveFade"; value: window.immersiveFades ? 0 : 1 }
        PropertyAction { target: window; property: "immersiveReveal"; value: 0.01 }
        ParallelAnimation {
            NumberAnimation {
                target: window
                property: "revealSpin"
                to: window.revealSpinStart + 1.6 * Math.PI
                duration: window.shapeMotion(600)
                easing.type: Easing.Linear
            }
            SequentialAnimation {
                NumberAnimation {
                    target: window
                    property: "revealReach"
                    to: 0.2
                    duration: window.shapeMotion(260)
                    easing.type: Easing.OutBack
                    easing.overshoot: 2.4
                }
                PauseAnimation { duration: window.shapeMotion(60) }
                NumberAnimation {
                    target: window
                    property: "revealReach"
                    to: 1
                    duration: window.shapeMotion(280)
                    easing.type: Easing.InBack
                    easing.overshoot: 1.2
                }
            }
        }
        ScriptAction { script: window.finishOpening() }
    }

    // with reduced motion the built player fades in over the window, which
    // is let go of from drawing once it has
    SequentialAnimation {
        id: fadeInMotion
        NumberAnimation {
            target: window
            property: "immersiveFade"
            to: 1
            duration: 240
            easing.type: Easing.OutCubic
        }
        PropertyAction { target: window; property: "immersiveReveal"; value: 1 }
    }

    // closing shrinks the shape away into the middle as it turns, quickly
    // at first and settling as it goes; the player is let go of once it has
    // gone, and what it held handed back. with reduced motion it fades away
    // where it stands, over the window drawn again under it
    SequentialAnimation {
        id: closeMotion
        onFinished: App.trimMemory()
        PropertyAction { target: window; property: "immersiveReveal"; value: Math.min(window.immersiveReveal, 0.99) }
        NumberAnimation {
            target: window
            property: "immersiveFade"
            to: window.immersiveFades ? 0 : 1
            duration: window.immersiveFades ? 200 : 0
            easing.type: Easing.InCubic
        }
        ParallelAnimation {
            NumberAnimation {
                target: window
                property: "immersiveReveal"
                to: 0
                duration: window.shapeMotion(520)
                easing.type: Easing.OutCubic
            }
            NumberAnimation {
                target: window
                property: "revealReach"
                to: 0
                duration: window.shapeMotion(520)
                easing.type: Easing.OutCubic
            }
            NumberAnimation {
                target: window
                property: "revealSpin"
                to: window.revealSpinStart + 1.4
                duration: window.shapeMotion(520)
                easing.type: Easing.OutCubic
            }
        }
        PropertyAction { target: window; property: "immersiveFade"; value: 1 }
    }

    // the immersive player opens through a shape that grows from the middle
    // of the window and closes through the same shape shrinking away. it is
    // only built while it shows, and only drawn through the shape while it
    // opens or closes.
    Loader {
        id: immersive
        anchors.fill: parent
        visible: window.onScreen && window.immersiveReveal > 0
        active: window.immersiveReveal > 0
        // built a little at a time between frames while the shape grows,
        // rather than all at once in the frame the click lands: that held
        // up the window for most of a tenth of a second
        asynchronous: true
        focus: UiState.immersiveOpen
        opacity: window.immersiveFade
        onLoaded: {
            if (window.immersiveFades && UiState.immersiveOpen && !openMotion.running && window.immersiveReveal < 1) {
                fadeInMotion.restart()
            }
        }

        // only while the shape moves: a layer stays enabled on a hidden
        // item, and this one is the window's size, 16 MB on a retina screen
        // held for as long as the app ran once the player had first opened
        layer.enabled: window.immersiveReveal > 0 && window.immersiveReveal < 1
        layer.effect: ShaderEffect {
            property variant source
            property size itemSize: Qt.size(width, height)
            // whichever shape is picked, it has five corners at most
            property vector4d shape: {
                const picked = Shapes.params(window.revealShape)
                return Qt.vector4d(Math.min(picked.x, 5), picked.y, picked.z, picked.w)
            }
            // grown far enough that even its valleys clear the corners
            property real reach: Math.max(0, window.revealReach) * Math.hypot(width, height) / 2
                                 / (shape.w > 0.5 ? 1 : 1 - shape.y) + 2
            property real spin: window.revealSpin
            fragmentShader: "qrc:/shaders/shape_mask.frag.qsb"
        }

        sourceComponent: ImmersivePlayer {
            windowControlsTopInset: window.inAppWindowControls ? appWindowControls.y + appWindowControls.height : 0
            focus: true
            onCloseRequested: UiState.immersiveOpen = false
        }
    }

    WindowControls {
        id: appWindowControls
        objectName: "appWindowControls"
        x: Settings.windowControlsOnRight ? parent.width - width - 12 : 12
        y: 8
        visible: window.inAppWindowControls && window.visibility !== Window.FullScreen
        compact: true
        scheme: window.immersiveReveal > 0 ? PlayerStyle.darkScheme : Appearance.colors
        window: window
    }

    Toast {
        id: toast
        anchors.centerIn: parent
    }

    // over everything, the immersive player included
    CornerCameo {
        anchors.fill: parent
    }

    // little bursts of shapes from what was just done, over everything in
    // this window (UiState.burst)
    Item {
        id: bursts
        anchors.fill: parent

        function sceneOf(item: Item): Item {
            let top = item
            while (top.parent !== null) {
                top = top.parent
            }
            return top
        }

        Connections {
            target: UiState
            function onBurstRequested(item: Item, kind: string) {
                // only from this window's own items
                if (bursts.sceneOf(item) !== bursts.sceneOf(bursts)) {
                    return
                }
                const at = item.mapToItem(bursts, item.width / 2, item.height / 2)
                burstComponent.createObject(bursts, { x: at.x, y: at.y, kind: kind })
            }
        }

        Component {
            id: burstComponent
            ShapeBurst {}
        }
    }

    // the settings, a sheet over the window, built only while open, and
    // between frames: all at once it held up the window for half a second
    Loader {
        id: settings
        active: false
        asynchronous: true
        // opened once it is in the window, which it is not yet when built
        onLoaded: {
            const dialog = item as SettingsDialog
            dialog.open()
        }
        sourceComponent: SettingsDialog {
            // let go once it has gone, unless it was asked for again on the way
            onClosed: Qt.callLater(() => {
                const dialog = settings.item as SettingsDialog
                if (dialog && !dialog.visible) {
                    settings.active = false
                    App.trimMemory()
                }
            })
        }
    }

    // the squiggles and shapes that turn do so while music plays somewhere seen
    Binding {
        target: UiState
        property: "spinning"
        value: Player.playing && window.visible
    }

    // the analysis runs only while the window could show it
    Binding {
        target: Player
        property: "visualizerPresentationActive"
        value: window.visible && (UiState.immersiveOpen
               ? Settings.immersiveVisualizer || Settings.livingBackdrop
               : Settings.playerBarVisualizer || Settings.trackListVisualizer)
    }

    // closing the window leaves the music playing; the tray brings it back.
    // built only where something shows tray icons, since qt complains loudly
    // about a tray with nowhere to go
    Loader {
        active: App.trayAvailable && (Settings.trayIcon || (App.windows && !window.visible))
        sourceComponent: Platform.SystemTrayIcon {
            objectName: "swiftifyTrayIcon"
            visible: true
            icon.source: "qrc:/qt/qml/Swiftify/resources/swiftify-icon.png"
            tooltip: Player.hasSong ? Player.currentSong.name + " — " + Player.currentSong.artists : "Swiftify"
            onActivated: (reason) => {
                if (reason === Platform.SystemTrayIcon.Trigger || reason === Platform.SystemTrayIcon.DoubleClick) {
                    window.reveal()
                }
            }

            menu: Platform.Menu {
                Platform.MenuItem {
                    text: qsTr("Show Swiftify")
                    onTriggered: window.reveal()
                }
                Platform.MenuItem {
                    text: Player.playing ? qsTr("Pause") : qsTr("Play")
                    enabled: Player.hasSong
                    onTriggered: Player.togglePlayback()
                }
                Platform.MenuSeparator {}
                Platform.MenuItem {
                    text: qsTr("Quit Swiftify")
                    onTriggered: App.quit()
                }
            }
        }
    }

    LoginDialog {}

    // dialogs seldom opened are built when asked for, and let go of once
    // they have closed
    Loader {
        id: deleteDialog
        active: false
        sourceComponent: DeletePlaylistDialog {
            onClosed: Qt.callLater(() => {
                deleteDialog.active = false
                App.trimMemory()
            })
        }
    }

    Loader {
        id: playlistEditor
        active: false
        sourceComponent: PlaylistEditor {
            onClosed: Qt.callLater(() => {
                playlistEditor.active = false
                App.trimMemory()
            })
        }
    }

    Loader {
        id: trackEditor
        active: false
        sourceComponent: LocalTrackEditor {
            onClosed: Qt.callLater(() => {
                trackEditor.active = false
                App.trimMemory()
            })
        }
    }

    SongOptionsMenu {
        id: songMenu
    }

    Shortcut { sequence: "Space"; enabled: Player.hasSong; onActivated: Player.togglePlayback() }
    // on macos the menu bar carries these (MacMenuBar), and two owners of a
    // key would leave neither acting
    Shortcut { sequence: "Ctrl+Right"; enabled: !App.macOS; onActivated: Player.skipForward() }
    Shortcut { sequence: "Ctrl+Left"; enabled: !App.macOS; onActivated: Player.skipBackward() }
    Shortcut { sequence: "Ctrl+Alt+Right"; enabled: !App.macOS; onActivated: Player.seekBy(10000) }
    Shortcut { sequence: "Ctrl+Alt+Left"; enabled: !App.macOS; onActivated: Player.seekBy(-10000) }
    Shortcut { sequence: "Ctrl+Up"; enabled: !App.macOS && Session.connected; onActivated: Player.adjustVolume(0.05) }
    Shortcut { sequence: "Ctrl+Down"; enabled: !App.macOS && Session.connected; onActivated: Player.adjustVolume(-0.05) }
    Shortcut { sequence: "Ctrl+Alt+M"; enabled: !App.macOS && Session.connected; onActivated: Player.toggleMute() }
    Shortcut { sequence: "Ctrl+Alt+S"; enabled: !App.macOS && Player.hasSong; onActivated: Player.toggleShuffle() }
    Shortcut { sequence: "Ctrl+Alt+R"; enabled: !App.macOS && Player.hasSong; onActivated: Player.cycleRepeatMode() }
    Shortcut { sequence: "Ctrl+Alt+G"; onActivated: UiState.showCatchphrase("test") }
    Shortcut { sequence: "Ctrl+,"; enabled: !App.macOS; onActivated: UiState.settingsRequested() }

    // StandardKey.Back is "Alt+Left" both on Windows and Linux. Both Shortcuts thus have the
    // same key, which results in neither acting (see "Ctrl+..." Shortcuts)
    Shortcut { sequence: "Alt+Left"; enabled: App.macOS; onActivated: Library.goBack() }
    Shortcut { sequences: [StandardKey.Back]; enabled: !App.macOS; onActivated: Library.goBack() }
    // escape closes the page and goes back to the one before. an open menu
    // or dialog takes it first, the search field clears itself with it, and
    // the immersive player closes on its own
    Shortcut {
        sequence: "Escape"
        enabled: Library.canGoBack && !UiState.immersiveOpen
        onActivated: Library.goBack()
    }
    TapHandler {
        acceptedButtons: Qt.BackButton
        enabled: Library.canGoBack && !UiState.immersiveOpen
        onTapped: Library.goBack()
    }

    Shortcut { sequences: [StandardKey.Find]; enabled: !App.macOS; onActivated: topBar.focusSearch() }

    Shortcut { sequences: ["Ctrl+K", "Ctrl+L"]; enabled: !App.macOS; onActivated: topBar.openCatalogueSearch()}

    // spelled out rather than StandardKey.ZoomIn, which holds ctrl + too: the
    // same key twice in one shortcut is ambiguous, and neither acts
    Shortcut { sequences: ["Ctrl++", "Ctrl+="]; onActivated: window.setZoom(window.zoom + window.zoomStep) }
    Shortcut { sequences: ["Ctrl+-"]; onActivated: window.setZoom(window.zoom - window.zoomStep) }
    Shortcut { sequence: "Ctrl+0"; onActivated: window.setZoom(1) }

    Loader {
        active: App.nativeMenuBar
        sourceComponent: MacMenuBar {
            window: window
            onFindRequested: topBar.focusSearch()
            onCatalogueSearchRequested: topBar.openCatalogueSearch()
            // the settings first, if they are open over the window
            onCloseWindowRequested: {
                const open = settings.item as SettingsDialog
                if (open && open.opened) {
                    open.close()
                } else {
                    window.close()
                }
            }
            onMinimizeRequested: window.showMinimized()
            onZoomRequested: window.visibility === Window.Maximized ? window.showNormal() : window.showMaximized()
            onShowWindowRequested: window.reveal()
        }
    }
}
