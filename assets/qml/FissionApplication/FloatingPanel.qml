// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
import QtQuick

// Command panel floating over the viewport. Dragged by its top strip (excluding the close control),
// kept inside the parent and remembers its position through `app.panelPos`.
Item {
    id: fp
    property var app
    default property alias content: holder.data
    property real contentHeight: 400
    // Right edge reserved so the hosted panel's ✕ / close button receives clicks (not the DragHandler)
    property real closeReserve: 40
    readonly property real margin: 10

    width: 320
    height: Math.max(120, Math.min(contentHeight, parent ? parent.height - 2 * margin : contentHeight))

    function clamp() {
        if (!parent) return
        x = Math.max(margin, Math.min(x, parent.width - width - margin))
        y = Math.max(margin, Math.min(y, parent.height - height - margin))
    }
    function place() {
        if (!parent) return
        const p = app.panelPos
        x = p ? p.x : parent.width - width - margin
        y = p ? p.y : 120
        clamp()
    }

    onVisibleChanged: if (visible) place()
    onHeightChanged: clamp()
    Component.onCompleted: place()
    Connections {
        target: fp.parent
        function onWidthChanged() { fp.clamp() }
        function onHeightChanged() { fp.clamp() }
    }

    // Shadow
    Rectangle {
        anchors.fill: parent
        anchors.margins: -1
        anchors.topMargin: 1
        anchors.bottomMargin: -3
        radius: 5
        color: "#30000000"
    }

    Rectangle {
        anchors.fill: parent
        radius: 4
        color: Theme.panel
        border.color: Theme.border
        clip: true

        // Keeps clicks and wheel events on the panel from reaching the viewport
        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.AllButtons
            onWheel: (w) => w.accepted = true
        }

        Item {
            id: holder
            anchors.fill: parent
            anchors.margins: 1
        }
    }

    // Drag only the title strip, leaving the close control free (previous full-width DragHandler stole ✕ clicks)
    Item {
        id: dragStrip
        width: Math.max(0, parent.width - fp.closeReserve)
        height: 38
        z: 1
        DragHandler {
            target: fp
            xAxis.minimum: fp.margin
            xAxis.maximum: fp.parent ? fp.parent.width - fp.width - fp.margin : 0
            yAxis.minimum: fp.margin
            yAxis.maximum: fp.parent ? fp.parent.height - fp.height - fp.margin : 0
            cursorShape: Qt.SizeAllCursor
            // Require a short move so a press on chrome doesn't feel like a drag
            dragThreshold: 4
            onActiveChanged: if (!active) fp.app.panelPos = Qt.point(fp.x, fp.y)
        }
        HoverHandler { cursorShape: Qt.SizeAllCursor }
    }
}
