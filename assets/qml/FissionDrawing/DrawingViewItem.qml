// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
import QtQuick

// Projected 2D view on a drawing sheet — curves float on the paper (Fusion-like), not in a card.
Item {
    id: viewItem
    width: view ? Math.max(40, view.width * view.scale * paperScale) : 80
    height: view ? Math.max(32, view.height * view.scale * paperScale) : 60
    z: selected ? 20 : 10

    property var view
    property var selection
    property bool selected: false
    property real paperScale: 1
    property bool interactive: true

    signal dragFinished(real x, real y)
    signal clicked()
    signal projectDrag(string direction)

    readonly property real pad: 10

    Canvas {
        id: canvas
        anchors.fill: parent
        onPaint: {
            const ctx = getContext("2d")
            ctx.reset()
            ctx.clearRect(0, 0, width, height)
            if (!view) return
            const cx = width * 0.5
            const cy = height * 0.5
            const s = view.scale * paperScale
            function strokeSegs(segs, color, dash, lw) {
                if (!segs || !segs.length) return
                ctx.strokeStyle = color
                ctx.lineWidth = lw || 1.1
                ctx.setLineDash(dash || [])
                ctx.beginPath()
                for (let i = 0; i < segs.length; ++i) {
                    const m = segs[i]
                    ctx.moveTo(cx + m.x1 * s, cy - m.y1 * s)
                    ctx.lineTo(cx + m.x2 * s, cy - m.y2 * s)
                }
                ctx.stroke()
                ctx.setLineDash([])
            }
            if (view.showHiddenLines)
                strokeSegs(view.hiddenCurves, "#9aa3ab", [3.5, 2.5], 0.9)
            strokeSegs(view.visibleCurves, "#1a1a1a", [], 1.25)
            strokeSegs(view.sectionCurves, "#111", [], 1.6)
            if (view.type === "section" && view.sectionCurves && view.sectionCurves.length) {
                ctx.save()
                ctx.strokeStyle = "rgba(60,60,60,0.45)"
                ctx.lineWidth = 0.65
                ctx.beginPath()
                const hatchSpan = Math.max(view.width, view.height) * s * 0.55
                for (let h = -hatchSpan; h <= hatchSpan; h += 5.5) {
                    ctx.moveTo(cx - hatchSpan + h, cy - hatchSpan)
                    ctx.lineTo(cx - hatchSpan + h + hatchSpan * 0.35, cy + hatchSpan)
                }
                ctx.stroke()
                ctx.restore()
            }
            if (view.type === "detail" && view.detailRadius > 0) {
                ctx.strokeStyle = "#333"
                ctx.lineWidth = 1
                ctx.setLineDash([4, 3])
                ctx.beginPath()
                ctx.arc(cx, cy, view.detailRadius * s, 0, Math.PI * 2)
                ctx.stroke()
                ctx.setLineDash([])
            }
            if (!view.projected) {
                ctx.fillStyle = "rgba(240,240,240,0.85)"
                ctx.fillRect(4, 4, width - 8, height - 8)
                ctx.fillStyle = "#888"
                ctx.font = "11px 'Segoe UI', sans-serif"
                ctx.fillText("Projecting…", 12, height * 0.5)
            }
        }
        Connections {
            target: viewItem.view
            function onCurvesChanged() { canvas.requestPaint() }
            function onScaleChanged() { canvas.requestPaint() }
            function onGeometryChanged() { canvas.requestPaint() }
            function onHiddenLinesChanged() { canvas.requestPaint() }
        }
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
    }

    Rectangle {
        anchors.fill: parent
        anchors.margins: -3
        color: "transparent"
        border.color: selected ? "#2d7ff9" : "transparent"
        border.width: 1.5
        radius: 2
        z: 5
    }

    Text {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.bottom
        anchors.topMargin: 3
        text: view ? (view.name + "  " + Number(view.scale).toFixed(view.scale < 1 ? 3 : 2)) : ""
        font.pixelSize: 10
        color: selected ? "#2d7ff9" : "#666"
        visible: !!view
        z: 6
    }

    MouseArea {
        anchors.fill: parent
        anchors.margins: -viewItem.pad
        enabled: viewItem.interactive
        hoverEnabled: true
        acceptedButtons: Qt.LeftButton
        property real originX: 0
        property real originY: 0
        property real startItemX: 0
        property real startItemY: 0
        property bool moved: false
        property bool projecting: false
        cursorShape: projecting ? Qt.CrossCursor : Qt.SizeAllCursor

        onPressed: (m) => {
            originX = m.x; originY = m.y
            startItemX = viewItem.x; startItemY = viewItem.y
            moved = false
            projecting = !!(m.modifiers & Qt.ShiftModifier)
            viewItem.clicked()
            if (view && selection)
                selection.select({ kind: "drawingView", id: view.id }, false)
        }
        onPositionChanged: (m) => {
            if (!pressed) return
            const dx = m.x - originX, dy = m.y - originY
            if (Math.hypot(dx, dy) > 5) moved = true
            if (projecting || (m.modifiers & Qt.ShiftModifier)) {
                projecting = true
                return
            }
            viewItem.x = startItemX + dx
            viewItem.y = startItemY + dy
        }
        onReleased: (m) => {
            const dx = m.x - originX, dy = m.y - originY
            if (projecting && moved) {
                let dir = "right"
                if (Math.abs(dx) >= Math.abs(dy))
                    dir = dx >= 0 ? "right" : "left"
                else
                    dir = dy >= 0 ? "down" : "up"
                viewItem.projectDrag(dir)
            } else if (moved) {
                viewItem.dragFinished(viewItem.x, viewItem.y)
            }
            projecting = false
            moved = false
        }
    }
}
