// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
import QtQuick
import "."

// Radial right-click menu centred on the cursor
Item {
    id: menu
    property var app
    property point center
    visible: false

    readonly property var solidItems: [
        { label: "Repeat " + (app.lastCommand || "..."), glyph: "redo", run: () => app.repeatLastCommand() },
        { label: "Extrude", glyph: "extrude", run: () => app.startCommand("Extrude") },
        { label: "Fillet", glyph: "fillet", run: () => app.startCommand("Fillet") },
        { label: "Sketch", glyph: "sketch", run: () => app.startCommand("Sketch") },
        { label: "Undo", glyph: "undo", run: () => app.doc.undo() },
        { label: "Fit", glyph: "fit", run: () => app.viewport.renderer.fitAll(true) },
        { label: "Revolve", glyph: "revolve", run: () => app.startCommand("Revolve") },
        { label: "Parameters", glyph: "parameters", run: () => app.openParameters() }
    ]
    readonly property var sketchItems: [
        { label: "Finish Sketch", glyph: "finish", run: () => app.finishSketch() },
        { label: "Line", glyph: "line", run: () => app.sketchTool = "line" },
        { label: "Rectangle", glyph: "rectangle", run: () => app.sketchTool = "rectangle" },
        { label: "Circle", glyph: "circle", run: () => app.sketchTool = "circle" },
        { label: "Undo", glyph: "undo", run: () => app.doc.undo() },
        { label: "Arc", glyph: "arc", run: () => app.sketchTool = "arc" },
        { label: "Delete", glyph: "delete", run: () => app.deleteSelectedSketchEntities() },
        { label: "Look At", glyph: "home", run: () => app.viewport.renderer.lookAtSketch(app.activeSketchId, true) }
    ]
    readonly property var items: app.activeSketchId ? sketchItems : solidItems

    function openAt(x, y) {
        center = Qt.point(x, y)
        visible = true
    }

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.AllButtons
        onPressed: menu.visible = false
        onWheel: menu.visible = false
    }

    Rectangle {
        x: menu.center.x - 16
        y: menu.center.y - 16
        width: 32
        height: 32
        radius: 16
        color: "#cc3c4249"
    }

    Repeater {
        model: menu.items
        Rectangle {
            id: item
            required property var modelData
            required property int index
            readonly property real angle: -Math.PI / 2 + index * Math.PI * 2 / menu.items.length
            width: 112
            height: 28
            radius: 4
            x: menu.center.x + Math.cos(angle) * 110 - width / 2
            y: menu.center.y + Math.sin(angle) * 80 - height / 2
            color: area.containsMouse ? Theme.accentLight : "#f7f8f9"
            border.color: area.containsMouse ? Theme.accent : Theme.border
            Row {
                anchors.verticalCenter: parent.verticalCenter
                x: 6
                spacing: 6
                Glyph { width: 18; height: 18; name: item.modelData.glyph }
                Text { text: item.modelData.label; anchors.verticalCenter: parent.verticalCenter; color: Theme.text }
            }
            MouseArea {
                id: area
                anchors.fill: parent
                hoverEnabled: true
                onClicked: { menu.visible = false; item.modelData.run() }
            }
        }
    }
}
