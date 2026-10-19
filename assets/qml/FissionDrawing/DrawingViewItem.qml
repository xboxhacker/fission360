// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
import QtQuick

// A projected 2D view on a drawing sheet — draws HLR curves when projected.
Item {
    id: viewItem
    width: view ? Math.max(60, view.width * view.scale * paperScale) : 120
    height: view ? Math.max(48, view.height * view.scale * paperScale) : 90
    z: 10

    property var view
    property var selection
    property bool selected: false
    property real paperScale: 1

    signal dragFinished(real x, real y)
    signal clicked()

    Rectangle {
        anchors.fill: parent
        color: "white"
        border.color: selected ? "#4d90fe" : "#999"
        border.width: selected ? 2 : 1

        Text {
            anchors.top: parent.top
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.topMargin: 2
            text: view ? (view.name + " · " + view.orientation + " · " + Number(view.scale).toFixed(2)) : "View"
            font.pixelSize: 9
            color: "#555"
            z: 2
        }

        Canvas {
            id: canvas
            anchors.fill: parent
            anchors.margins: 4
            anchors.topMargin: 16
            onPaint: {
                const ctx = getContext("2d")
                ctx.reset()
                ctx.clearRect(0, 0, width, height)
                if (!view) return
                const cx = width * 0.5
                const cy = height * 0.5
                const s = view.scale * paperScale
                function strokeSegs(segs, color, dash) {
                    if (!segs || !segs.length) return
                    ctx.strokeStyle = color
                    ctx.lineWidth = 1
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
                strokeSegs(view.hiddenCurves, "#aaa", [4, 3])
                strokeSegs(view.visibleCurves, "#222", [])
                strokeSegs(view.sectionCurves, "#000", [])
                // ANSI31-style hatch beside section cutting lines
                if (view.type === "section" && view.sectionCurves && view.sectionCurves.length) {
                    ctx.save()
                    ctx.strokeStyle = "#444"
                    ctx.lineWidth = 0.7
                    ctx.beginPath()
                    const hatchSpan = Math.max(view.width, view.height) * s * 0.55
                    for (let h = -hatchSpan; h <= hatchSpan; h += 6) {
                        ctx.moveTo(cx - hatchSpan + h, cy - hatchSpan)
                        ctx.lineTo(cx - hatchSpan + h + hatchSpan * 0.4, cy + hatchSpan)
                    }
                    ctx.stroke()
                    ctx.restore()
                }
                if (view.type === "detail" && view.detailRadius > 0) {
                    ctx.strokeStyle = "#222"
                    ctx.beginPath()
                    ctx.arc(cx, cy, view.detailRadius * s, 0, Math.PI * 2)
                    ctx.stroke()
                }
                if (!view.projected) {
                    ctx.fillStyle = "#f0f0f0"
                    ctx.fillRect(8, 8, width - 16, height - 16)
                    ctx.fillStyle = "#888"
                    ctx.font = "11px sans-serif"
                    ctx.fillText("Empty — regenerate", 14, height * 0.5)
                }
            }
            Connections {
                target: viewItem.view
                function onCurvesChanged() { canvas.requestPaint() }
                function onScaleChanged() { canvas.requestPaint() }
                function onGeometryChanged() { canvas.requestPaint() }
            }
            onWidthChanged: requestPaint()
            onHeightChanged: requestPaint()
        }
    }

    MouseArea {
        anchors.fill: parent
        drag.target: parent
        drag.minimumX: 0
        drag.minimumY: 0
        drag.maximumX: parent.parent ? parent.parent.width - parent.width : 2000
        drag.maximumY: parent.parent ? parent.parent.height - parent.height : 2000
        cursorShape: Qt.SizeAllCursor
        onPressed: {
            viewItem.clicked()
            if (view && selection)
                selection.select({ kind: "drawingView", id: view.id }, false)
        }
        onReleased: viewItem.dragFinished(viewItem.x, viewItem.y)
    }
}
