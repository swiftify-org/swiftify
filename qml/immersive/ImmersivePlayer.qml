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

// the immersive player (FullscreenPlayerView): the cover's colours poured over
// the window, what is playing, one of lyrics, the queue, the sleep timer or
// Sound beside it, and the spectrum along the bottom. it is always dark, and
// while it is open the keys drive playback: space, ←/→ to seek, ↑/↓ for the
// volume, and Esc to leave.
FocusScope {
    id: root

    signal closeRequested()

    property MaterialScheme scheme: PlayerStyle.darkScheme
    property real windowControlsTopInset: 0

    readonly property bool lyricsAvailable: Player.loadingLyrics || (Player.hasLyrics && Player.lyrics.count > 0)
    /// the switch's segments, in order; "" is the artwork alone
    readonly property list<string> panels: ["", "lyrics", "queue", "devices", "sleep", "audio"]

    /// what the layout makes room for beside the artwork
    property string panel
    /// what the panel area shows: the panel, or the one still sliding away
    property string shownPanel
    property real panelOpacity: 1
    /// how far the panel sits off to the right of its place, 0 to 1
    property real panelShift: 0
    /// true while a panel arrives or leaves, so the layout eases rather than
    /// snapping; resizing the window still snaps
    property bool layoutMoving: false

    /// distraction-free fullscreen mode: hides controls after 5 seconds of mouse inactivity
    property bool distractionFree: false
    property bool controlsHidden: false
    property int savedVisibility: Window.Windowed
    property real lastMouseX: -1
    property real lastMouseY: -1
    property real controlsOpacity: 1
    readonly property bool panelRetainedInDistractionFree: shownPanel === "lyrics" || shownPanel === "queue"
    readonly property real panelDistractionFreeOpacity: panelRetainedInDistractionFree ? 1 : controlsOpacity

    NumberAnimation {
        id: hideControlsAnimation
        objectName: "hideControlsAnimation"
        target: root
        property: "controlsOpacity"
        to: 0
        duration: Appearance.duration(400)
        easing.type: Easing.OutCubic
    }

    NumberAnimation {
        id: showControlsAnimation
        objectName: "showControlsAnimation"
        target: root
        property: "controlsOpacity"
        to: 1
        duration: Appearance.duration(200)
        easing.type: Easing.OutCubic
    }

    onControlsHiddenChanged: {
        if (distractionFree && controlsHidden) {
            showControlsAnimation.stop()
            hideControlsAnimation.restart()
            if (!panelRetainedInDistractionFree && usesPanelLayout) {
                layoutMoving = true
                settle.restart()
            }
        } else {
            hideControlsAnimation.stop()
            showControlsAnimation.restart()
            if (!panelRetainedInDistractionFree && usesPanelLayout) {
                layoutMoving = true
                settle.restart()
            }
        }
    }

    Timer {
        id: inactivityTimer
        objectName: "inactivityTimer"
        interval: 5000
        running: root.distractionFree && !root.controlsHidden
        onTriggered: root.controlsHidden = true
    }

    function registerActivity() {
        if (!distractionFree) {
            return
        }
        controlsHidden = false
        inactivityTimer.restart()
    }

    function toggleDistractionFree() {
        setDistractionFree(!distractionFree)
    }

    function setDistractionFree(enable: bool) {
        if (distractionFree === enable) {
            return
        }
        distractionFree = enable
        const win = root.Window.window
        if (enable) {
            if (win) {
                if (win.visibility !== Window.FullScreen) {
                    savedVisibility = win.visibility
                }
                win.showFullScreen()
            }
            lastMouseX = -1
            lastMouseY = -1
            controlsHidden = false
            hideControlsAnimation.stop()
            showControlsAnimation.stop()
            controlsOpacity = 1
            inactivityTimer.restart()
        } else {
            inactivityTimer.stop()
            controlsHidden = false
            hideControlsAnimation.stop()
            showControlsAnimation.stop()
            controlsOpacity = 1
            if (win && win.visibility === Window.FullScreen) {
                if (savedVisibility === Window.Maximized) {
                    win.showMaximized()
                } else {
                    win.showNormal()
                }
            }
        }
    }

    Component.onDestruction: {
        if (root.distractionFree) {
            root.setDistractionFree(false)
        }
    }

    readonly property bool usesPanelLayout: panel.length > 0
    readonly property bool showsSpectrum: Settings.visualizersEnabled && Settings.immersiveVisualizer && !Settings.circularVisualizer
    readonly property bool effectiveUsesPanelLayout: distractionFree && controlsHidden ? panelRetainedInDistractionFree : usesPanelLayout

    readonly property real metadataWidth: Math.max(300, Math.min(400, width * 0.32))
    readonly property real horizontalPadding: Math.max(28, Math.min(64, width * 0.05))
    readonly property real rowSpace: Math.min(1250, width - 2 * horizontalPadding)
    readonly property real gap: Math.max(32, Math.min(72, width * 0.055))
    readonly property bool sideBySide: effectiveUsesPanelLayout && rowSpace >= 900
    readonly property bool panelOnly: effectiveUsesPanelLayout && !sideBySide
    readonly property real panelWidth: sideBySide ? Math.min(540, (rowSpace - gap) / 2) : rowSpace
    readonly property real nowPlayingWidth: sideBySide ? Math.min(500, rowSpace - gap - panelWidth) : Math.min(620, rowSpace)
    readonly property real rowWidth: sideBySide ? nowPlayingWidth + gap + panelWidth : nowPlayingWidth
    readonly property real panelHeight: Math.min(520, immersiveLayout.contentHeight)
    // where the panel stands when shown, worked out from the panel layout
    // whether or not it is in use, so a panel sliding away keeps its track
    readonly property real panelX: sideBySide ? (width - rowWidth) / 2 + nowPlayingWidth + gap
                                              : (width - panelWidth) / 2
    readonly property real panelSlide: Math.min(140, panelWidth * 0.3)
    /// the mac's traffic lights stay over the top-left corner
    readonly property real titleBarInset: App.macOS ? Math.max(28, SafeArea.margins.top) : windowControlsTopInset

    component LayoutMotion: NumberAnimation {
        duration: Appearance.motion(500)
        easing.type: Easing.BezierSpline
        easing.bezierCurve: Appearance.curves.expressiveDefaultSpatial
    }

    function resolvedPanel(preference: string): string {
        if (preference === "artwork" || panels.indexOf(preference) < 0) {
            return ""
        }
        return preference === "lyrics" && !lyricsAvailable ? "" : preference
    }

    // a panel opening slides in from the right, fading in, while the cover
    // and its controls move aside for it; closing, it slides back out,
    // fading, as they return to the middle. one panel for another fades
    // across where it stands. a change the player makes on its own, such as
    // lyrics going missing, is not remembered as the person's choice.
    property string pendingPanel
    property bool pendingRemembers: true

    function swapPanel(next: string, remembers: bool) {
        if (next === panel && !swap.running && !leavePanel.running) {
            return
        }
        pendingPanel = next
        pendingRemembers = remembers
        swap.stop()
        enterPanel.stop()
        leavePanel.stop()
        if (next.length > 0 && panel.length === 0) {
            commitPanel()
            if (shownPanel !== next) {
                shownPanel = next
                panelOpacity = 0
                panelShift = 1
            }
            enterPanel.start()
        } else if (next.length === 0) {
            commitPanel()
            leavePanel.start()
        } else {
            swap.start()
        }
    }

    function commitPanel() {
        if (pendingRemembers) {
            Settings.immersivePlayerPanel = pendingPanel.length > 0 ? pendingPanel : "artwork"
        }
        if (pendingPanel.length > 0 !== usesPanelLayout) {
            layoutMoving = true
            settle.restart()
        }
        panel = pendingPanel
    }

    ParallelAnimation {
        id: enterPanel
        LayoutMotion { target: root; property: "panelShift"; to: 0 }
        NumberAnimation { target: root; property: "panelOpacity"; to: 1; duration: Appearance.duration(320); easing.type: Easing.OutCubic }
    }

    SequentialAnimation {
        id: leavePanel
        ParallelAnimation {
            LayoutMotion { target: root; property: "panelShift"; to: 1 }
            NumberAnimation { target: root; property: "panelOpacity"; to: 0; duration: Appearance.duration(260); easing.type: Easing.OutQuad }
        }
        ScriptAction {
            script: if (root.panel.length === 0) root.shownPanel = ""
        }
    }

    SequentialAnimation {
        id: swap
        NumberAnimation {
            target: root
            property: "panelOpacity"
            to: 0
            duration: Appearance.duration(160)
            easing.type: Easing.OutQuad
        }
        ScriptAction {
            script: {
                root.commitPanel()
                root.shownPanel = root.pendingPanel
                root.panelShift = 0
            }
        }
        NumberAnimation {
            target: root
            property: "panelOpacity"
            to: 1
            duration: Appearance.duration(380)
            easing.type: Easing.BezierSpline
            easing.bezierCurve: Appearance.curves.emphasizedDecel
        }
    }

    Timer {
        id: settle
        interval: 520
        onTriggered: root.layoutMoving = false
    }

    onLyricsAvailableChanged: {
        if (!lyricsAvailable && panel === "lyrics") {
            swapPanel("", false)
        } else if (lyricsAvailable && panel.length === 0 && Settings.immersivePlayerPanel === "lyrics") {
            swapPanel("lyrics", false)
        }
    }

    Component.onCompleted: {
        panel = resolvedPanel(Settings.immersivePlayerPanel)
        shownPanel = panel
        forceActiveFocus()
    }

    Keys.onPressed: (event) => {
        root.registerActivity()
        switch (event.key) {
        case Qt.Key_Space:
            Player.togglePlayback()
            break
        case Qt.Key_Escape:
            if (root.distractionFree) {
                root.setDistractionFree(false)
            } else {
                root.closeRequested()
            }
            break
        case Qt.Key_Left:
            Player.seekBy(-10000)
            break
        case Qt.Key_Right:
            Player.seekBy(10000)
            break
        case Qt.Key_Up:
            Player.setVolume(Math.min(Player.volume + 0.05, 1))
            break
        case Qt.Key_Down:
            Player.setVolume(Math.max(Player.volume - 0.05, 0))
            break
        default:
            return
        }
        event.accepted = true
    }

    ImmersiveLayout {
        id: immersiveLayout
        windowWidth: root.width
        windowHeight: root.height - (topBar.height - immersiveLayout.topBarHeight)
        showsSpectrum: root.showsSpectrum
        showsPanel: root.sideBySide
        circular: Settings.visualizersEnabled && Settings.immersiveVisualizer && Settings.circularVisualizer
    }

    LiquidBackdrop {
        id: backdrop
        anchors.fill: parent
        shown: root.visible
    }

    HoverHandler {
        id: activityTracker
        objectName: "activityTracker"
        cursorShape: (root.distractionFree && root.controlsHidden) ? Qt.BlankCursor : Qt.ArrowCursor
        onPointChanged: {
            const px = point.position.x
            const py = point.position.y
            if (root.lastMouseX < 0 && root.lastMouseY < 0) {
                root.lastMouseX = px
                root.lastMouseY = py
                return
            }
            if (Math.abs(px - root.lastMouseX) > 2 || Math.abs(py - root.lastMouseY) > 2) {
                root.lastMouseX = px
                root.lastMouseY = py
                root.registerActivity()
            }
        }
    }

    Item {
        id: blankCursorOverlay
        objectName: "blankCursorOverlay"
        anchors.fill: parent
        z: 9999
        visible: root.distractionFree && root.controlsHidden
        HoverHandler {
            cursorShape: Qt.BlankCursor
        }
    }

    // a click anywhere empty takes the keys back from whatever had them
    TapHandler {
        onTapped: {
            root.forceActiveFocus()
            root.registerActivity()
        }
    }

    // --- top bar -------------------------------------------------------------------

    Item {
        id: topBar
        width: parent.width
        height: Math.max(immersiveLayout.topBarHeight, immersiveClose.y + immersiveClose.height + 2)
        opacity: root.controlsOpacity
        visible: opacity > 0
        enabled: opacity > 0

        DragHandler {
            target: null
            grabPermissions: PointerHandler.CanTakeOverFromAnything
            onActiveChanged: if (active) root.Window.window.startSystemMove()
        }
        TapHandler {
            acceptedButtons: Qt.LeftButton
            onDoubleTapped: {
                if (root.distractionFree) {
                    root.setDistractionFree(false)
                } else {
                    const window = root.Window.window
                    window.visibility === Window.Maximized ? window.showNormal() : window.showMaximized()
                }
            }
        }

        IconButton {
            id: immersiveClose
            objectName: "immersiveCloseButton"
            anchors.left: parent.left
            anchors.leftMargin: 24
            y: Math.max(18, (Settings.windowControlsOnRight ? 0 : root.titleBarInset) + 6)
            scheme: root.scheme
            symbol: "close"
            size: 34
            iconSize: 18
            iconColor: "white"
            colBackground: Qt.rgba(1, 1, 1, 0.12)
            colBackgroundHover: Qt.rgba(1, 1, 1, 0.2)
            colRipple: Qt.rgba(1, 1, 1, 0.24)
            backgroundItem.border.width: 1
            backgroundItem.border.color: Qt.rgba(1, 1, 1, 0.14)
            tooltip: qsTr("Close Immersive Player")
            onClicked: {
                if (root.distractionFree) {
                    root.setDistractionFree(false)
                }
                root.closeRequested()
            }
        }

        IconButton {
            id: immersiveDistractionFree
            objectName: "immersiveDistractionFreeButton"
            anchors.left: immersiveClose.right
            anchors.leftMargin: 8
            y: immersiveClose.y
            scheme: root.scheme
            symbol: root.distractionFree ? "fullscreen_exit" : "fullscreen"
            size: 34
            iconSize: 18
            iconColor: "white"
            colBackground: root.distractionFree ? Qt.rgba(1, 1, 1, 0.24) : Qt.rgba(1, 1, 1, 0.12)
            colBackgroundHover: Qt.rgba(1, 1, 1, 0.2)
            colRipple: Qt.rgba(1, 1, 1, 0.24)
            backgroundItem.border.width: 1
            backgroundItem.border.color: Qt.rgba(1, 1, 1, 0.14)
            tooltip: root.distractionFree ? qsTr("Exit Fullscreen") : qsTr("Fullscreen")
            onClicked: root.toggleDistractionFree()
        }

        SegmentedControl {
            id: switcher
            objectName: "immersiveSwitcher"
            anchors.right: parent.right
            anchors.rightMargin: 24
            y: Math.max(18, (Settings.windowControlsOnRight ? root.titleBarInset : 0) + 6)
            scheme: root.scheme
            segmentHeight: 30
            horizontalPadding: 12
            iconSize: 19
            trackColor: Qt.rgba(1, 1, 1, 0.12)
            indicatorColor: Qt.rgba(1, 1, 1, 0.92)
            contentColor: Qt.rgba(1, 1, 1, 0.86)
            selectedContentColor: "#141414"
            currentIndex: root.panels.indexOf(root.panel)
            segments: [
                Segment { symbol: "image"; tooltip: qsTr("Artwork") },
                Segment { symbol: "lyrics"; tooltip: qsTr("Lyrics"); enabled: root.lyricsAvailable },
                Segment { symbol: "queue_music"; tooltip: qsTr("Playing Next") },
                Segment { symbol: Player.remoteDeviceActive ? "cast_connected" : "devices"; tooltip: qsTr("Devices") },
                Segment { symbol: "bedtime"; tooltip: qsTr("Sleep Timer") },
                Segment { symbol: "tune"; tooltip: qsTr("Sound") }
            ]
            onActivated: (index) => {
                const next = root.panels[index]
                if (next !== "lyrics" || root.lyricsAvailable) {
                    root.swapPanel(next, true)
                }
            }
        }

        LoadingSpinner {
            anchors.horizontalCenter: switcher.horizontalCenter
            anchors.bottom: switcher.top
            anchors.bottomMargin: 2
            width: 12
            height: 12
            Reveal.shown: Player.loadingLyrics
            running: visible
            color: "white"
            trackColor: Qt.rgba(1, 1, 1, 0.16)
        }
    }

    // --- content -------------------------------------------------------------------

    Item {
        id: content
        y: topBar.height
        width: parent.width
        height: immersiveLayout.contentHeight

        ImmersiveNowPlaying {
            id: nowPlaying
            objectName: "immersiveNowPlaying"
            Reveal.shown: Player.hasSong && !root.panelOnly
            layout: immersiveLayout
            metadataWidth: Math.min(root.metadataWidth, root.nowPlayingWidth)
            animateLayout: root.layoutMoving
            scheme: root.scheme
            controlsOpacity: root.controlsOpacity
            width: root.nowPlayingWidth
            x: (content.width - root.rowWidth) / 2
            anchors.verticalCenter: parent.verticalCenter
            anchors.verticalCenterOffset: root.distractionFree ? (1.0 - root.controlsOpacity) * nowPlaying.controlsShift : 0

            Behavior on x {
                enabled: root.layoutMoving || root.distractionFree
                LayoutMotion {}
            }
            Behavior on width {
                enabled: root.layoutMoving || root.distractionFree
                LayoutMotion {}
            }
        }

        Loader {
            id: panelHost
            objectName: "immersivePanel"
            visible: Player.hasSong && root.shownPanel.length > 0 && opacity > 0
            x: root.panelX + root.panelShift * root.panelSlide
            width: root.panelWidth
            height: root.panelHeight
            anchors.verticalCenter: parent.verticalCenter
            opacity: root.panelOpacity * root.panelDistractionFreeOpacity
            active: Player.hasSong && root.shownPanel.length > 0
            sourceComponent: {
                switch (root.shownPanel) {
                case "lyrics": return lyrics
                case "queue": return queue
                case "devices": return devices
                case "sleep": return sleep
                case "audio": return sound
                default: return null
                }
            }
        }

        EmptyState {
            anchors.centerIn: parent
            Reveal.shown: !Player.hasSong
            scheme: root.scheme
            symbol: "music_note"
            title: qsTr("Nothing Playing")
            description: qsTr("Choose a song to open the immersive player.")
        }
    }

    Component {
        id: lyrics
        ImmersiveLyrics {
            scheme: root.scheme
        }
    }
    Component {
        id: queue
        ImmersiveQueue {
            scheme: root.scheme
        }
    }
    Component {
        id: devices
        DevicesPanel {
            scheme: root.scheme
            Component.onCompleted: Player.refreshDevices()
        }
    }
    Component {
        id: sleep
        SleepTimerPanel {
            scheme: root.scheme
        }
    }
    Component {
        id: sound
        // Sound is taller than the panel on short windows, so it scrolls
        Flickable {
            id: soundScroll
            contentHeight: soundPanel.implicitHeight + 24
            boundsBehavior: Flickable.StopAtBounds
            clip: true

            SoundPanel {
                id: soundPanel
                y: 12
                width: soundScroll.width
                scheme: root.scheme
            }
        }
    }

    // --- spectrum ------------------------------------------------------------------

    SpectrumView {
        // fades in and out with the setting, and dims beside a panel
        visible: opacity > 0
        x: 42
        y: content.y + content.height + immersiveLayout.spectrumSpacing
        width: parent.width - 84
        height: immersiveLayout.spectrumHeight
        kind: SpectrumView.Immersive
        wave: Settings.visualizerShape === "wave"
        continuous: Settings.continuousVisualizer
        sensitivity: Settings.visualizerSensitivity
        active: Player.playing && root.showsSpectrum
        opacity: !root.showsSpectrum ? 0 : (root.usesPanelLayout ? 0.74 : 1)

        Behavior on opacity {
            NumberAnimation { duration: Appearance.duration(240); easing.type: Easing.OutQuad }
        }
    }
}
