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

// cover art: decoded at about the size shown, clipped by a shader rather
// than a layer, over a procedural record sleeve until it arrives. with
// `crossfade` a new cover blends over the old one, as the swift player's
// artwork does. resized, it keeps showing what it has while the new size
// decodes, rather than dropping back to the placeholder.
Item {
    id: root

    property url source
    property real radius: Appearance.sizes.artworkRadius
    property bool circular: false
    property real borderWidth: 0
    property color borderColor: Qt.rgba(1, 1, 1, 0.12)
    property bool crossfade: false
    /// a cover that arrives fades in over the placeholder; without, it is
    /// simply there, as a cover already decoded should be
    property bool fades: true
    /// with `crossfade`, a new cover sweeps in from the side over this long,
    /// counted from when its source changed
    property int revealDuration: 0
    property double sourceChangedAt: 0
    property string placeholderSymbol: "music_note"
    /// Shared across a card, its hero and navigation thumbnail. Genuine art
    /// always wins; this identity only chooses the missing/loading sleeve.
    property string fallbackKey: source.toString().length > 0 ? source.toString() : placeholderSymbol
    /// a cover is shared with everything else showing it at the same size,
    /// and its decoded pixels are let go of once it is on the gpu
    /// (QSG_TRANSIENT_IMAGES). another window cannot draw from what is gone,
    /// so a cover in a window of its own loads its own copy
    property bool cache: true
    readonly property bool ready: front.status === Image.Ready
    /// the most pixels it is decoded at, for a cover only ever seen blurred
    property int maximumPixelSize: 2048
    /// the size it decodes at, in points, as sourceSize takes it: the size
    /// shown in pixels, rounded up to 32 of them. qt multiplies a sourceSize
    /// by the screen's pixels a point itself, so working in pixels here
    /// counted them twice, and a retina screen decoded every cover at twice
    /// its width (up to the 640 a spotify cover comes at): over three times
    /// the memory for each one shown. a coarser step, half as big again each
    /// time, decoded a card shown at 352 pixels at 384, a fifth more again
    readonly property int pixelSize: {
        const zoom = Settings.zoom > 0 ? Settings.zoom : 1
        const screenRatio = Screen.devicePixelRatio > 0 ? Screen.devicePixelRatio : 1
        const totalRatio = screenRatio * zoom
        const pixels = Math.min(maximumPixelSize, Math.max(16, Math.ceil(Math.max(width, height) * totalRatio / 32) * 32))
        return Math.ceil(pixels / screenRatio)
    }
    /// what back holds is the cover front is loading, at another size
    readonly property bool sameCover: back.status === Image.Ready && back.source.toString() === front.source.toString()
    /// how deep the edge's scallops cut, in pixels; 0 is a plain edge. while
    /// it is not 0 the scallops run slowly round the edge
    property real waviness: 0
    property real wavelength: 22
    readonly property real wavePhase: waveDrift.value
    /// a round cover's edge is a squiggle: a wavy line the cover is cut along
    property bool squiggly: true
    /// with no shapes (Settings.noShapes) a round cover's edge is a plain
    /// circle, and no edge waves
    readonly property bool showsSquiggle: circular && squiggly && !Settings.noShapes && Math.min(width, height) >= 40
    property color squiggleColor: Appearance.colors.primary
    /// the players' covers turn their squiggle with the music; anywhere else
    /// it turns while the pointer is on it, or on what holds the cover
    property bool squiggleFollowsPlayback: false
    property bool squiggleTurning: hover.hovered
    property real squiggleSpin: 0
    // eases the hovered spin in and out rather than starting it dead
    property real squiggleSpeed: squiggleTurning && !Appearance.reduceMotion && !App.pointerResting ? 1 : 0
    Behavior on squiggleSpeed {
        NumberAnimation { duration: 700; easing.type: Easing.InOutSine }
    }
    // the squiggle keeps its proportions at every size, as material's wavy
    // indicators do: a wave eight lines long and seven tenths of a line
    // high either side. a fixed wavelength packed a big cover's edge so
    // tight that the sine bunched into loops
    readonly property real squiggleThickness: Math.max(2, Math.min(width, height) * 0.02)
    readonly property real squiggleAmplitude: Math.max(1.4, squiggleThickness * 0.7)
    readonly property real squiggleWaves: {
        const middle = Math.min(width, height) / 2 - squiggleAmplitude - squiggleThickness / 2 - 0.75
        return Math.max(8, Math.round(2 * Math.PI * middle / (8 * squiggleThickness)))
    }
    // either way it goes round at much the same pace, about a turn a
    // minute and a bit, whatever the cover's size
    readonly property real squigglePhase: squiggleFollowsPlayback && visible ? (squiggleWaves * 0.1 * UiState.spin) % (2 * Math.PI)
                                                                  : squiggleSpin
    /// the circle the squiggle line runs along, from the cover's centre, as
    /// rounded_image.frag works it out; a ring visualizer is cut along it
    readonly property real squiggleMiddle: Math.min(width, height) / 2 - squiggleAmplitude - squiggleThickness / 2 - 0.75
    readonly property real effectiveRadius: circular ? Math.min(face.width, face.height) / 2 : radius

    onSourceChanged: {
        sourceChangedAt = Date.now()
        if (source.toString().length === 0) {
            back.source = ""
        }
    }

    function artworkUrl(url: url): string {
        const text = url.toString()
        return text.length > 0 ? "image://art/" + encodeURIComponent(text) : ""
    }

    component Layer: ShaderEffect {
        required property Item image
        property real imageAspect: 1
        readonly property real itemAspect: height > 0 ? width / height : 1
        property variant source: image
        property size itemSize: Qt.size(width, height)
        property real radius
        property real borderWidth
        property color borderColor
        property size uvScale: imageAspect > itemAspect ? Qt.size(itemAspect / imageAspect, 1) : Qt.size(1, imageAspect / itemAspect)
        property point uvOffset: Qt.point((1 - uvScale.width) / 2, (1 - uvScale.height) / 2)
        property real waviness: Settings.noShapes ? 0 : root.waviness
        property real wavelength: root.wavelength
        property real phase: root.wavePhase
        property real ringWaves: root.showsSquiggle ? root.squiggleWaves : 0
        property real ringAmplitude: root.squiggleAmplitude
        property real ringThickness: root.squiggleThickness
        property real ringPhase: root.squigglePhase
        property color ringColor: root.squiggleColor
        property real reveal: 1

        anchors.fill: parent
        fragmentShader: "qrc:/shaders/rounded_image.frag.qsb"
    }

    // the scallops run round the edge while it is wavy, easing in and out
    Drift {
        id: waveDrift
        active: Math.abs(root.waviness) > 0.01 && root.visible && !Settings.noShapes
        speed: 2 * Math.PI / 2.4
    }

    HoverHandler {
        id: hover
        enabled: root.showsSquiggle
    }

    // a hovered round cover's squiggle runs round its edge; it slows to a
    // stop where it is when the pointer leaves
    FrameAnimation {
        running: root.showsSquiggle && !root.squiggleFollowsPlayback && root.squiggleSpeed > 0 && root.visible
        onTriggered: root.squiggleSpin = (root.squiggleSpin + frameTime * root.squiggleSpeed * root.squiggleWaves * 0.075)
                                         % (2 * Math.PI)
    }

    Item {
        id: face
        anchors.fill: parent

        // under a squiggly cover the placeholder goes through the same cut,
        // so the squiggle is its edge too
        Image {
            id: placeholder
            anchors.fill: parent
            visible: false
            asynchronous: true
            source: root.artworkUrl("swiftify-cover:/" + (Theme.dark ? "dark" : "light")
                                    + "?key=" + encodeURIComponent(root.fallbackKey))
            sourceSize: Qt.size(root.pixelSize, root.pixelSize)
        }

        Layer {
            image: placeholder
            radius: root.effectiveRadius
            borderWidth: root.borderWidth
            borderColor: root.borderColor
            visible: (!root.ready || frontLayer.opacity < 1) && !backLayer.visible
        }

        // the cover last shown, at the size it was shown: what stands in
        // while the same cover decodes at a new size, and what a new cover
        // crossfades over. it takes front's cover once front is shown.
        Image {
            id: back
            visible: false
            asynchronous: true
            cache: root.cache
        }

        Image {
            id: front
            visible: false
            asynchronous: true
            cache: root.cache
            source: root.artworkUrl(root.source)
            sourceSize: Qt.size(root.pixelSize, root.pixelSize)
            onStatusChanged: {
                if (status === Image.Error) {
                    back.source = ""
                    return
                }
                if (status !== Image.Ready) {
                    return
                }

                sweep.stop()
                frontLayer.reveal = 1
                // the same cover at a new size is simply there
                if (root.fades && !root.sameCover) {
                    const sweepLeft = root.revealDuration - (Date.now() - root.sourceChangedAt)
                    if (root.crossfade && back.status === Image.Ready && sweepLeft > 400 && !Appearance.reduceMotion) {
                        fadeIn.stop()
                        frontLayer.opacity = 1
                        frontLayer.reveal = 0
                        sweep.duration = sweepLeft
                        sweep.restart()
                    } else {
                        frontLayer.opacity = 0
                        fadeIn.restart()
                    }
                } else {
                    fadeIn.stop()
                    frontLayer.opacity = 1
                    root.keepShown()
                }
            }
        }

        Layer {
            id: backLayer
            image: back
            imageAspect: back.implicitHeight > 0 ? back.implicitWidth / back.implicitHeight : 1
            radius: root.effectiveRadius
            borderWidth: root.borderWidth
            borderColor: root.borderColor
            // a new cover crossfades over it; the same one, at another size,
            // stands in for it while it decodes
            visible: back.status === Image.Ready && (root.sameCover || root.crossfade)
                     && (!root.ready || frontLayer.opacity < 1 || frontLayer.reveal < 1)
        }

        Layer {
            id: frontLayer
            image: front
            imageAspect: front.implicitHeight > 0 ? front.implicitWidth / front.implicitHeight : 1
            radius: root.effectiveRadius
            borderWidth: root.borderWidth
            borderColor: root.borderColor
            visible: root.ready
        }
    }

    NumberAnimation {
        id: fadeIn
        target: frontLayer
        property: "opacity"
        from: 0
        to: 1
        duration: Appearance.duration(280)
        easing.type: Easing.OutCubic
        onFinished: root.keepShown()
    }

    NumberAnimation {
        id: sweep
        target: frontLayer
        property: "reveal"
        from: 0
        to: 1
        easing.type: Easing.InOutSine
        onFinished: root.keepShown()
    }

    /// back takes the cover front shows, already decoded, so the cache hands
    /// it over at once
    function keepShown() {
        // emptied first, so changing the size alone does not decode the old
        // cover again
        back.source = ""
        back.sourceSize = front.sourceSize
        back.source = front.source
    }
}
