// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
import QtQuick

// Projected 2D view on a drawing sheet — curves float on the paper (Fusion-like), not in a card.
Item {
    id: viewItem
    // Clear space inside the selection frame so the border does not run through the outline.
    readonly property real framePad: 16
    width: (view ? Math.max(40, view.width * view.scale * paperScale) : 80) + framePad * 2
    height: (view ? Math.max(32, view.height * view.scale * paperScale) : 60) + framePad * 2
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

    // Sheet scale as a drawing ratio (1:1, 2:1, 1:2). A decimal is only used when it is not a
    // simple ratio, and it always uses a dot so 1 never renders like a clock ("1:00").
    function drawingScaleLabel(scale) {
        const s = Number(scale)
        if (!isFinite(s) || s <= 0) return "1:1"
        const nice = [1, 2, 4, 5, 8, 10, 20, 25, 50, 100]
        for (let i = 0; i < nice.length; ++i) {
            const n = nice[i]
            if (Math.abs(s - n) < 0.001) return n + ":1"
            if (Math.abs(s * n - 1) < 0.001) return "1:" + n
        }
        return s.toFixed(s < 1 ? 3 : 2)
    }

    readonly property bool showRender: view && view.displayStyle === "rendered" && (view.renderPng || "").length > 8

    Image {
        anchors.centerIn: parent
        width: view ? Math.max(8, view.width * view.scale * paperScale) : 0
        height: view ? Math.max(8, view.height * view.scale * paperScale) : 0
        visible: viewItem.showRender
        fillMode: Image.PreserveAspectFit
        smooth: true
        mipmap: true
        source: viewItem.showRender ? ("data:image/png;base64," + view.renderPng) : ""
        z: 2
    }

    Canvas {
        id: canvas
        anchors.fill: parent
        visible: !viewItem.showRender
        antialiasing: true
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
                ctx.lineJoin = "round"
                ctx.lineCap = "round"
                ctx.setLineDash(dash || [])
                ctx.beginPath()
                let px = NaN, py = NaN
                for (let i = 0; i < segs.length; ++i) {
                    const m = segs[i]
                    const x1 = cx + m.x1 * s, y1 = cy - m.y1 * s
                    const x2 = cx + m.x2 * s, y2 = cy - m.y2 * s
                    if (!(Math.hypot(x1 - px, y1 - py) < 0.05))
                        ctx.moveTo(x1, y1)
                    ctx.lineTo(x2, y2)
                    px = x2
                    py = y2
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
            function onStyleChanged() { canvas.requestPaint() }
            function onRenderChanged() { canvas.requestPaint() }
        }
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
    }

    Rectangle {
        anchors.fill: parent
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
        text: view ? (view.name + "  " + viewItem.drawingScaleLabel(view.scale)) : ""
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
