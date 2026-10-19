// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
import QtQuick
import QtQuick.Layouts
import "."

Rectangle {
    id: bar
    property var app
    property alias progressBar: progress
    implicitHeight: 24
    color: Theme.header

    function errorCount() {
        if (!app.doc) return 0
        let n = 0
        const fs = app.doc.featureList("")
        for (const f of fs) if (!f.healthy) ++n
        return n
    }
    function describe(ref) {
        if (!ref || ref.kind === undefined) return ""
        switch (ref.kind) {
        case "face": return "Face " + (ref.index + 1) + " of Body" + (ref.body + 1)
        case "edge": return "Edge " + (ref.index + 1) + " of Body" + (ref.body + 1)
        case "body": return "Body" + (ref.body + 1)
        case "profile": return "Profile " + (ref.index + 1)
        case "sketchcurve": return "Sketch curve " + (ref.entity + 1)
        default: return ref.kind
        }
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 10
        anchors.rightMargin: 10
        spacing: 16

        Text {
            text: fissionApp.statusMessage
            color: Theme.headerText
            elide: Text.ElideRight
            Layout.fillWidth: true
        }
        // Long operations (mesh conversion, face groups, scripts): what is running and how far along
        Row {
            id: progress
            objectName: "progressBar"
            visible: fissionApp.busy
            spacing: 8
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: fissionApp.progressText
                color: "#9fd4f0"
                elide: Text.ElideRight
                width: Math.min(implicitWidth, 320)
            }
            Rectangle {
                id: track
                anchors.verticalCenter: parent.verticalCenter
                width: 140
                height: 6
                radius: 3
                color: Qt.darker(Theme.header, 1.6)
                border.color: Qt.lighter(Theme.header, 1.4)
                border.width: 1
                clip: true
                readonly property bool indeterminate: fissionApp.progressValue < 0
                Rectangle {
                    id: fill
                    y: 1
                    height: parent.height - 2
                    radius: 2
                    color: Theme.accent
                    x: track.indeterminate ? 0 : 1
                    width: track.indeterminate ? track.width * 0.3
                                               : Math.max(0, (track.width - 2) * Math.min(1, fissionApp.progressValue))
                    Behavior on width { enabled: !track.indeterminate; NumberAnimation { duration: 120 } }
                    // Unknown length: a segment sweeping back and forth
                    SequentialAnimation on x {
                        running: progress.visible && track.indeterminate
                        loops: Animation.Infinite
                        NumberAnimation { from: 1; to: track.width * 0.7 - 1; duration: 900; easing.type: Easing.InOutSine }
                        NumberAnimation { from: track.width * 0.7 - 1; to: 1; duration: 900; easing.type: Easing.InOutSine }
                    }
                }
            }
        }
        Text {
            text: bar.describe(bar.app.viewport.renderer.hovered)
            color: "#9fd4f0"
        }
        Text {
            text: bar.app.selection.count ? bar.app.selection.summary : ""
            color: Theme.headerText
        }
        Text {
            readonly property int errors: (bar.app.rev, bar.errorCount())
            visible: errors > 0
            text: "\u26a0 " + errors + " feature error" + (errors > 1 ? "s" : "")
            color: "#ff9b8f"
        }
        Text {
            // Linked drawings / renders behind the design contentRevision
            readonly property bool stale: {
                const d = bar.app.doc
                if (!d) return false
                d.contentRevision
                d.linkedAssets
                return d.anyLinkedAssetOutOfDate()
            }
            visible: stale
            text: "Linked outputs out of date"
            color: "#ffcc80"
            font.pixelSize: 11
        }
        Text {
            text: bar.app.doc ? bar.app.doc.bodies.length + " bodies" : ""
            color: "#aab2bb"
        }
    }
}
