// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
import QtQuick

// Vector icons drawn in a 24 x 24 design grid. Brand icons (orca) use a coloured image instead.
Item {
    id: glyph
    property string name: "box"
    property color color: "#3c4249"
    property color fill: Qt.rgba(color.r, color.g, color.b, 0.18)
    readonly property bool brand: name === "orca"
    implicitWidth: 24
    implicitHeight: 24

    Image {
        anchors.fill: parent
        visible: glyph.brand
        source: glyph.brand ? "qrc:/icons/orca.svg" : ""
        fillMode: Image.PreserveAspectFit
        smooth: true
        mipmap: true
        opacity: glyph.enabled ? 1 : 0.45
    }

    Canvas {
        id: canvas
        anchors.fill: parent
        visible: !glyph.brand
        antialiasing: true

        onPaint: {
            if (glyph.brand) return
            const c = getContext("2d")
            c.reset()
            const s = Math.min(width, height) / 24
            c.translate((width - 24 * s) / 2, (height - 24 * s) / 2)
            c.scale(s, s)
            c.lineWidth = 1.6
            c.lineJoin = "round"
            c.lineCap = "round"
            c.strokeStyle = glyph.color
            c.fillStyle = glyph.fill
            glyph.draw(c, glyph.name)
        }

        Connections {
            target: glyph
            function onNameChanged() { canvas.requestPaint() }
            function onColorChanged() { canvas.requestPaint() }
        }
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
    }

    function poly(c, pts, close, doFill) {
        c.beginPath()
        c.moveTo(pts[0][0], pts[0][1])
        for (let i = 1; i < pts.length; ++i) c.lineTo(pts[i][0], pts[i][1])
        if (close) c.closePath()
        if (doFill) c.fill()
        c.stroke()
    }
    function circle(c, x, y, r, doFill) {
        c.beginPath()
        c.arc(x, y, r, 0, Math.PI * 2)
        if (doFill) c.fill()
        c.stroke()
    }
    function dot(c, x, y, r) {
        c.beginPath()
        c.arc(x, y, r, 0, Math.PI * 2)
        c.fillStyle = glyph.color
        c.fill()
        c.fillStyle = glyph.fill
    }
    function box(c, x, y, w, h, d) {
        poly(c, [[x, y + d], [x + w, y + d], [x + w, y + d + h], [x, y + d + h]], true, true)
        poly(c, [[x, y + d], [x + d, y], [x + w + d, y], [x + w, y + d]], true, true)
        poly(c, [[x + w, y + d], [x + w + d, y], [x + w + d, y + h], [x + w, y + d + h]], true, true)
    }

    function draw(c, n) {
        switch (n) {
        case "sketch":
            poly(c, [[4, 20], [4, 4], [20, 4], [20, 20]], true, false)
            c.setLineDash([2, 2]); poly(c, [[4, 12], [20, 12]], false, false); c.setLineDash([])
            poly(c, [[14, 18], [20, 6], [22, 7], [16, 19], [13, 20]], true, true)
            break
        case "extrude":
            poly(c, [[4, 20], [14, 20], [20, 16], [10, 16]], true, true)
            poly(c, [[4, 20], [4, 10], [10, 6], [10, 16]], false, false)
            poly(c, [[4, 10], [14, 10], [20, 6], [10, 6]], true, true)
            poly(c, [[14, 10], [14, 20]], false, false); poly(c, [[20, 6], [20, 16]], false, false)
            poly(c, [[17, 2], [17, 7]], false, false); poly(c, [[15, 4], [17, 2], [19, 4]], false, false)
            break
        case "revolve":
            c.beginPath(); c.ellipse(4, 14, 16, 7); c.fill(); c.stroke()
            poly(c, [[12, 2], [12, 22]], false, false)
            c.beginPath(); c.ellipse(5, 5, 14, 5); c.stroke()
            poly(c, [[16, 3], [19, 5], [16, 8]], false, false)
            break
        case "fillet":
            poly(c, [[4, 20], [4, 4]], false, false); poly(c, [[20, 20], [8, 20]], false, false)
            c.beginPath(); c.moveTo(4, 8); c.quadraticCurveTo(4, 20, 16, 20); c.stroke()
            poly(c, [[4, 20], [20, 20]], false, false)
            c.beginPath(); c.moveTo(4, 8); c.lineTo(4, 20); c.lineTo(16, 20); c.quadraticCurveTo(4, 20, 4, 8); c.fill()
            break
        case "chamfer":
            poly(c, [[4, 20], [4, 9], [9, 4], [20, 4], [20, 20]], true, true)
            c.lineWidth = 2.4; poly(c, [[4, 9], [9, 4]], false, false)
            break
        case "loft":
            c.beginPath(); c.ellipse(3, 16, 18, 6); c.fill(); c.stroke()
            poly(c, [[8, 3], [16, 3], [16, 6], [8, 6]], true, false)
            poly(c, [[3, 19], [8, 4.5]], false, false); poly(c, [[21, 19], [16, 4.5]], false, false)
            break
        case "sweep":
            c.beginPath(); c.moveTo(4, 20); c.bezierCurveTo(4, 8, 14, 14, 20, 4); c.stroke()
            c.beginPath(); c.ellipse(1, 16, 6, 8); c.fill(); c.stroke()
            c.setLineDash([2, 2]); c.beginPath(); c.moveTo(8, 21); c.bezierCurveTo(8, 10, 17, 16, 22, 6); c.stroke(); c.setLineDash([])
            break
        case "shell":
            poly(c, [[3, 7], [3, 21], [21, 21], [21, 7]], false, true)
            poly(c, [[6, 7], [6, 18], [18, 18], [18, 7]], false, false)
            poly(c, [[3, 7], [6, 7]], false, false); poly(c, [[18, 7], [21, 7]], false, false)
            break
        case "mirror":
            c.setLineDash([2, 2]); poly(c, [[12, 2], [12, 22]], false, false); c.setLineDash([])
            poly(c, [[3, 18], [9, 18], [9, 8]], true, true)
            poly(c, [[21, 18], [15, 18], [15, 8]], true, false)
            break
        case "rectpattern":
            for (let i = 0; i < 3; ++i) for (let j = 0; j < 2; ++j)
                poly(c, [[3 + i * 7, 5 + j * 8], [8 + i * 7, 5 + j * 8], [8 + i * 7, 10 + j * 8], [3 + i * 7, 10 + j * 8]], true, i + j === 0)
            break
        case "circpattern":
            c.setLineDash([2, 2]); circle(c, 12, 12, 7, false); c.setLineDash([])
            for (let k = 0; k < 6; ++k) {
                const a = k * Math.PI / 3
                circle(c, 12 + 7 * Math.cos(a), 12 + 7 * Math.sin(a), 2.4, k === 0)
            }
            break
        case "plane":
            poly(c, [[2, 17], [8, 7], [22, 7], [16, 17]], true, true)
            break
        case "planeoffset":
            poly(c, [[2, 21], [7, 14], [21, 14], [16, 21]], true, false)
            poly(c, [[2, 11], [7, 4], [21, 4], [16, 11]], true, true)
            poly(c, [[12, 12], [12, 18]], false, false)
            break
        case "planeangle":
            poly(c, [[2, 20], [8, 14], [22, 14], [16, 20]], true, false)
            poly(c, [[6, 17], [10, 3], [20, 5], [16, 17]], true, true)
            break
        case "midplane":
            poly(c, [[2, 21], [7, 17], [21, 17], [16, 21]], true, false)
            poly(c, [[2, 7], [7, 3], [21, 3], [16, 7]], true, false)
            poly(c, [[2, 14], [7, 10], [21, 10], [16, 14]], true, true)
            break
        case "plane3":
            poly(c, [[3, 18], [8, 6], [21, 10]], true, true)
            dot(c, 3, 18, 2); dot(c, 8, 6, 2); dot(c, 21, 10, 2)
            break
        case "axis":
            poly(c, [[4, 20], [20, 4]], false, false)
            dot(c, 4, 20, 2); dot(c, 20, 4, 2)
            break
        case "joint": case "asbuiltjoint":
            // Two blocks meeting on a shared origin
            poly(c, [[3, 14], [10, 14], [10, 21], [3, 21]], true, false)
            poly(c, [[14, 3], [21, 3], [21, 10], [14, 10]], true, false)
            circle(c, 12, 12, 3.2, false); dot(c, 12, 12, 1.4)
            if (n === "asbuiltjoint") { c.setLineDash([2, 2]); poly(c, [[10, 14], [14, 10]], false, false); c.setLineDash([]) }
            break
        case "point":
            circle(c, 12, 12, 5, true); dot(c, 12, 12, 2)
            break
        case "trim":
            poly(c, [[3, 12], [9, 12]], false, false)
            c.setLineDash([2, 2]); poly(c, [[9, 12], [21, 12]], false, false); c.setLineDash([])
            poly(c, [[9, 4], [9, 20]], false, false)
            poly(c, [[14, 6], [20, 18]], false, false); poly(c, [[20, 6], [14, 18]], false, false)
            break
        case "extend":
            poly(c, [[3, 12], [11, 12]], false, false)
            c.setLineDash([2, 2]); poly(c, [[11, 12], [19, 12]], false, false); c.setLineDash([])
            poly(c, [[19, 4], [19, 20]], false, false)
            poly(c, [[14, 9], [17, 12], [14, 15]], false, false)
            break
        case "offset":
            poly(c, [[3, 20], [3, 9], [9, 3], [21, 3]], false, false)
            c.setLineDash([2, 2]); poly(c, [[8, 20], [8, 11], [11, 8], [21, 8]], false, false); c.setLineDash([])
            break
        case "sketchfillet":
            poly(c, [[4, 4], [4, 12]], false, false); poly(c, [[12, 20], [20, 20]], false, false)
            c.beginPath(); c.arc(12, 12, 8, Math.PI / 2, Math.PI); c.stroke()
            c.setLineDash([2, 2]); poly(c, [[4, 12], [4, 20], [12, 20]], false, false); c.setLineDash([])
            break
        case "project":
            box(c, 4, 4, 11, 7, 4)
            c.setLineDash([2, 2]); poly(c, [[9, 17], [9, 21]], false, false); c.setLineDash([])
            poly(c, [[2, 21], [20, 21]], false, false)
            break
        case "dxf":
            poly(c, [[5, 3], [15, 3], [19, 7], [19, 21], [5, 21]], true, true)
            c.font = "bold 7px sans-serif"; c.fillStyle = glyph.color
            c.fillText("DXF", 6.5, 16)
            break
        case "originpoint":
            circle(c, 12, 12, 4, true); dot(c, 12, 12, 1.5)
            break
        case "originaxis":
            poly(c, [[3, 21], [21, 3]], false, false)
            poly(c, [[15, 3], [21, 3], [21, 9]], false, false)
            break
        case "parameters":
            c.font = "bold 13px sans-serif"; c.fillStyle = glyph.color
            c.fillText("f(x)", 1, 16)
            break
        case "settings": {
            const teeth = []
            for (let k = 0; k < 16; ++k) {
                const a = k / 16 * Math.PI * 2, r = k % 2 ? 7 : 10
                teeth.push([12 + r * Math.cos(a), 12 + r * Math.sin(a)])
            }
            poly(c, teeth, true, true)
            circle(c, 12, 12, 3, false)
            break
        }
        case "keyboard":
            poly(c, [[2, 6], [22, 6], [22, 18], [2, 18]], true, true)
            for (let r = 0; r < 2; ++r)
                for (let k = 0; k < 5; ++k) dot(c, 5 + k * 3.5, 9.5 + r * 3, 0.9)
            poly(c, [[7, 15.5], [17, 15.5]], false, false)
            break
        case "info":
            circle(c, 12, 12, 9, true)
            circle(c, 12, 7.5, 1.2, true)
            poly(c, [[12, 10.5], [12, 17.5]], false, false)
            break
        case "bug":
            // Simple bug: oval body, head, legs, antennae — for Report Bug
            circle(c, 12, 8, 3.2, true)
            poly(c, [[7, 11], [17, 11], [16, 18], [8, 18]], true, true)
            poly(c, [[9, 5], [6, 2]], false, false)
            poly(c, [[15, 5], [18, 2]], false, false)
            poly(c, [[7, 12], [3, 11]], false, false)
            poly(c, [[7, 15], [3, 16]], false, false)
            poly(c, [[17, 12], [21, 11]], false, false)
            poly(c, [[17, 15], [21, 16]], false, false)
            poly(c, [[12, 11], [12, 18]], false, false)
            break
        case "selwindow":
            c.setLineDash([3, 2]); poly(c, [[3, 4], [19, 4], [19, 15], [3, 15]], true, false); c.setLineDash([])
            poly(c, [[13, 11], [13, 22], [16, 19], [19, 23], [21, 22], [18, 18], [22, 17]], true, true)
            break
        case "selfreeform": {
            const loop = []
            for (let k = 0; k < 14; ++k) {
                const a = k / 14 * Math.PI * 2
                loop.push([11 + 8.5 * Math.cos(a) + (k % 3 === 0 ? 1.5 : 0), 10 + 6.5 * Math.sin(a)])
            }
            c.setLineDash([3, 2]); poly(c, loop, true, false); c.setLineDash([])
            poly(c, [[13, 11], [13, 22], [16, 19], [19, 23], [21, 22], [18, 18], [22, 17]], true, true)
            break
        }
        case "selpaint":
            poly(c, [[3, 15], [7, 9], [11, 13], [15, 6], [19, 10]], false, false)
            box(c, 14, 14, 6, 4, 3)
            poly(c, [[17, 18], [17, 22]], false, false)
            break
        case "units":
            poly(c, [[2, 9], [22, 9], [22, 16], [2, 16]], true, true)
            for (let k = 0; k < 5; ++k) poly(c, [[5 + k * 3.5, 9], [5 + k * 3.5, k % 2 ? 12 : 13.5]], false, false)
            break
        case "design":
            box(c, 4, 7, 11, 11, 5)
            break
        case "view":
            poly(c, [[3, 8], [15, 8], [15, 18], [3, 18]], true, true)
            poly(c, [[15, 11], [21, 7], [21, 19], [15, 15]], true, false)
            break
        case "properties":
            poly(c, [[5, 3], [19, 3], [19, 21], [5, 21]], true, true)
            for (let r = 0; r < 4; ++r) poly(c, [[8, 7 + r * 4], [16, 7 + r * 4]], false, false)
            break
        case "line":
            poly(c, [[4, 20], [20, 4]], false, false); dot(c, 4, 20, 2); dot(c, 20, 4, 2)
            break
        case "rectangle":
            poly(c, [[3, 6], [21, 6], [21, 18], [3, 18]], true, true); dot(c, 3, 18, 2); dot(c, 21, 6, 2)
            break
        case "centerrectangle":
            poly(c, [[3, 6], [21, 6], [21, 18], [3, 18]], true, true)
            c.setLineDash([2, 2]); poly(c, [[12, 12], [21, 6]], false, false); c.setLineDash([])
            dot(c, 12, 12, 2); dot(c, 21, 6, 2)
            break
        case "centerline":
            poly(c, [[3, 19], [21, 5]], false, false); dot(c, 12, 12, 2.2); dot(c, 21, 5, 2)
            c.lineWidth = 1.0
            poly(c, [[6, 10], [9, 14]], false, false); poly(c, [[15, 10], [18, 14]], false, false)
            break
        case "circle":
            circle(c, 12, 12, 8, true); dot(c, 12, 12, 1.6)
            break
        case "arc":
            c.beginPath(); c.arc(12, 16, 9, Math.PI * 1.05, Math.PI * 1.95); c.stroke()
            dot(c, 3.1, 14.5, 2); dot(c, 20.9, 14.5, 2); dot(c, 12, 7, 2)
            break
        case "polygon": {
            const pts = []
            for (let k = 0; k < 6; ++k) pts.push([12 + 9 * Math.cos(k * Math.PI / 3), 12 + 9 * Math.sin(k * Math.PI / 3)])
            poly(c, pts, true, true)
            break
        }
        case "slot":
            c.beginPath(); c.arc(7, 12, 5, Math.PI / 2, Math.PI * 1.5); c.lineTo(17, 7)
            c.arc(17, 12, 5, -Math.PI / 2, Math.PI / 2); c.closePath(); c.fill(); c.stroke()
            break
        case "ellipse":
            c.beginPath(); c.ellipse(2, 6, 20, 12); c.fill(); c.stroke()
            break
        case "spline":
            c.beginPath(); c.moveTo(3, 18); c.bezierCurveTo(8, 2, 14, 22, 21, 6); c.stroke()
            dot(c, 3, 18, 2); dot(c, 12, 12, 2); dot(c, 21, 6, 2)
            break
        case "construction":
            c.strokeStyle = "#e09232"; c.setLineDash([3, 2]); poly(c, [[4, 20], [20, 4]], false, false)
            break
        case "finish":
            c.strokeStyle = "#2e9e5b"; c.lineWidth = 2.6; poly(c, [[5, 13], [10, 18], [19, 6]], false, false)
            break
        case "undo":
            c.beginPath(); c.arc(13, 13, 7, Math.PI * 1.1, Math.PI * 0.6, false); c.stroke()
            poly(c, [[3, 7], [6, 12], [11, 9]], false, false)
            break
        case "redo":
            c.beginPath(); c.arc(11, 13, 7, -Math.PI * 0.1, Math.PI * 0.4, true); c.stroke()
            poly(c, [[21, 7], [18, 12], [13, 9]], false, false)
            break
        case "save":
            poly(c, [[4, 4], [17, 4], [20, 7], [20, 20], [4, 20]], true, true)
            poly(c, [[8, 4], [8, 9], [15, 9], [15, 4]], false, false)
            poly(c, [[7, 20], [7, 14], [17, 14], [17, 20]], false, false)
            break
        case "open":
            poly(c, [[3, 19], [3, 5], [9, 5], [11, 7], [19, 7], [19, 10]], false, false)
            poly(c, [[3, 19], [7, 10], [22, 10], [18, 19]], true, true)
            break
        case "plus":
            poly(c, [[12, 6], [12, 18]], false, false); poly(c, [[6, 12], [18, 12]], false, false)
            break
        case "new":
            poly(c, [[5, 3], [14, 3], [19, 8], [19, 21], [5, 21]], true, true)
            poly(c, [[14, 3], [14, 8], [19, 8]], false, false)
            break
        case "folder":
            poly(c, [[3, 19], [3, 6], [9, 6], [11, 8], [21, 8], [21, 19]], true, true)
            poly(c, [[3, 11], [21, 11]], false, false)
            break
        case "folderup":
            poly(c, [[3, 19], [3, 6], [9, 6], [11, 8], [21, 8], [21, 19]], true, true)
            poly(c, [[12, 17], [12, 11]], false, false)
            poly(c, [[9, 14], [12, 11], [15, 14]], false, false)
            break
        case "newfolder":
            poly(c, [[3, 19], [3, 6], [9, 6], [11, 8], [21, 8], [21, 19]], true, true)
            poly(c, [[12, 11], [12, 17]], false, false); poly(c, [[9, 14], [15, 14]], false, false)
            break
        case "refresh":
            c.beginPath(); c.arc(12, 12, 7, -Math.PI * 0.35, Math.PI * 1.35, false); c.stroke()
            poly(c, [[19, 3], [17.5, 7.3], [13, 6.5]], false, false)
            break
        case "projects":
            for (let gx = 0; gx < 3; ++gx)
                for (let gy = 0; gy < 3; ++gy) poly(c, [[3 + gx * 6.5, 3 + gy * 6.5], [8 + gx * 6.5, 3 + gy * 6.5],
                                                        [8 + gx * 6.5, 8 + gy * 6.5], [3 + gx * 6.5, 8 + gy * 6.5]], true, true)
            break
        case "home":
            poly(c, [[3, 12], [12, 4], [21, 12]], false, false)
            poly(c, [[6, 10], [6, 20], [18, 20], [18, 10]], false, true)
            break
        case "fit":
            poly(c, [[3, 8], [3, 3], [8, 3]], false, false); poly(c, [[16, 3], [21, 3], [21, 8]], false, false)
            poly(c, [[21, 16], [21, 21], [16, 21]], false, false); poly(c, [[8, 21], [3, 21], [3, 16]], false, false)
            circle(c, 12, 12, 4, true)
            break
        case "orbit":
            c.beginPath(); c.ellipse(2, 8, 20, 8); c.stroke(); circle(c, 12, 12, 3, true)
            poly(c, [[17, 6], [20, 8.5], [17, 11]], false, false)
            break
        case "pan":
            poly(c, [[12, 2], [12, 22]], false, false); poly(c, [[2, 12], [22, 12]], false, false)
            poly(c, [[9, 5], [12, 2], [15, 5]], false, false); poly(c, [[9, 19], [12, 22], [15, 19]], false, false)
            poly(c, [[5, 9], [2, 12], [5, 15]], false, false); poly(c, [[19, 9], [22, 12], [19, 15]], false, false)
            break
        case "grid":
            for (let g = 0; g < 4; ++g) {
                poly(c, [[3 + g * 6, 3], [3 + g * 6, 21]], false, false)
                poly(c, [[3, 3 + g * 6], [21, 3 + g * 6]], false, false)
            }
            break
        case "edges":
            box(c, 3, 6, 12, 12, 5)
            break
        case "ortho":
            poly(c, [[4, 4], [20, 4], [20, 20], [4, 20]], true, true)
            poly(c, [[4, 12], [20, 12]], false, false); poly(c, [[12, 4], [12, 20]], false, false)
            break
        case "eye":
            c.beginPath(); c.moveTo(2, 12); c.quadraticCurveTo(12, 2, 22, 12); c.quadraticCurveTo(12, 22, 2, 12); c.fill(); c.stroke()
            circle(c, 12, 12, 3, false)
            break
        case "eyeoff":
            c.globalAlpha = 0.45
            c.beginPath(); c.moveTo(2, 12); c.quadraticCurveTo(12, 2, 22, 12); c.quadraticCurveTo(12, 22, 2, 12); c.stroke()
            c.globalAlpha = 1; poly(c, [[4, 20], [20, 4]], false, false)
            break
        case "delete":
            poly(c, [[5, 7], [19, 7]], false, false)
            poly(c, [[7, 7], [8, 21], [16, 21], [17, 7]], true, true)
            poly(c, [[10, 7], [10, 4], [14, 4], [14, 7]], false, false)
            break
        case "close":
            poly(c, [[7, 7], [17, 17]], false, false); poly(c, [[17, 7], [7, 17]], false, false)
            break
        case "export":
            poly(c, [[12, 15], [12, 3]], false, false); poly(c, [[8, 7], [12, 3], [16, 7]], false, false)
            poly(c, [[4, 13], [4, 21], [20, 21], [20, 13]], false, false)
            break
        // ---- sketch constraints
        case "dimension":
            poly(c, [[4, 6], [4, 18]], false, false); poly(c, [[20, 6], [20, 18]], false, false)
            poly(c, [[4, 12], [20, 12]], false, false)
            poly(c, [[7, 10], [4, 12], [7, 14]], false, false); poly(c, [[17, 10], [20, 12], [17, 14]], false, false)
            break
        case "constraint:horizontal": poly(c, [[3, 15], [21, 15]], false, false); dot(c, 3, 15, 2); dot(c, 21, 15, 2); break
        case "constraint:vertical": poly(c, [[12, 3], [12, 21]], false, false); dot(c, 12, 3, 2); dot(c, 12, 21, 2); break
        case "constraint:coincident":
            poly(c, [[3, 19], [12, 12]], false, false); poly(c, [[12, 12], [21, 17]], false, false); dot(c, 12, 12, 3)
            break
        case "constraint:parallel": poly(c, [[5, 19], [13, 4]], false, false); poly(c, [[11, 20], [19, 5]], false, false); break
        case "constraint:perpendicular": poly(c, [[4, 20], [20, 20]], false, false); poly(c, [[12, 20], [12, 4]], false, false); break
        case "constraint:collinear":
            poly(c, [[3, 19], [10, 13]], false, false); poly(c, [[14, 10], [21, 4]], false, false)
            c.globalAlpha = 0.4; poly(c, [[10, 13], [14, 10]], false, false); c.globalAlpha = 1
            break
        case "constraint:tangent": circle(c, 12, 14, 6, true); poly(c, [[3, 8], [21, 8]], false, false); break
        case "constraint:equal": poly(c, [[5, 9], [19, 9]], false, false); poly(c, [[5, 15], [19, 15]], false, false); break
        case "constraint:concentric": circle(c, 12, 12, 8, false); circle(c, 12, 12, 4, true); dot(c, 12, 12, 1.2); break
        case "constraint:midpoint":
            poly(c, [[3, 16], [21, 16]], false, false); poly(c, [[8, 9], [12, 16], [16, 9]], true, true)
            break
        case "constraint:symmetric":
            c.globalAlpha = 0.5; poly(c, [[12, 3], [12, 21]], false, false); c.globalAlpha = 1
            dot(c, 6, 12, 2.2); dot(c, 18, 12, 2.2); poly(c, [[8, 12], [10, 12]], false, false); poly(c, [[14, 12], [16, 12]], false, false)
            break
        case "constraint:fix":
            poly(c, [[6, 11], [18, 11], [18, 21], [6, 21]], true, true)
            c.beginPath(); c.moveTo(8.5, 11); c.lineTo(8.5, 7.5); c.arc(12, 7.5, 3.5, Math.PI, 0, false); c.lineTo(15.5, 11); c.stroke()
            break
        case "first": poly(c, [[6, 5], [6, 19]], false, false); poly(c, [[18, 5], [9, 12], [18, 19]], true, true); break
        case "prev": poly(c, [[16, 5], [7, 12], [16, 19]], true, true); break
        case "next": poly(c, [[8, 5], [17, 12], [8, 19]], true, true); break
        case "last": poly(c, [[18, 5], [18, 19]], false, false); poly(c, [[6, 5], [15, 12], [6, 19]], true, true); break
        // ---- solid tools
        case "hole":
            poly(c, [[2, 9], [12, 4], [22, 9], [12, 14]], true, true)
            c.beginPath(); c.ellipse(8, 7, 8, 4); c.stroke()
            poly(c, [[8, 9], [8, 18]], false, false); poly(c, [[16, 9], [16, 18]], false, false)
            poly(c, [[8, 18], [12, 21], [16, 18]], false, false)
            break
        case "thread":
            poly(c, [[7, 3], [7, 21]], false, false); poly(c, [[17, 3], [17, 21]], false, false)
            for (let k = 0; k < 5; ++k) poly(c, [[7, 5 + k * 3.5], [17, 3 + k * 3.5]], false, false)
            break
        case "combine":
            circle(c, 9, 12, 6, true); circle(c, 15, 12, 6, true)
            break
        case "move":
            box(c, 3, 9, 9, 9, 4)
            poly(c, [[14, 6], [21, 6]], false, false); poly(c, [[18, 3], [21, 6], [18, 9]], false, false)
            poly(c, [[17, 10], [17, 18]], false, false); poly(c, [[14, 15], [17, 18], [20, 15]], false, false)
            break
        case "movefree":
            // three arrows from one corner plus a turning arc
            poly(c, [[6, 18], [19, 18]], false, false); poly(c, [[16, 15], [19, 18], [16, 21]], false, false)
            poly(c, [[6, 18], [6, 5]], false, false); poly(c, [[3, 8], [6, 5], [9, 8]], false, false)
            poly(c, [[6, 18], [13, 11]], false, false)
            c.beginPath(); c.arc(6, 18, 9, -Math.PI * 0.42, -Math.PI * 0.08); c.stroke()
            dot(c, 6, 18, 1.8)
            break
        case "movetranslate":
            poly(c, [[12, 3], [12, 21]], false, false); poly(c, [[3, 12], [21, 12]], false, false)
            poly(c, [[9, 6], [12, 3], [15, 6]], false, false); poly(c, [[9, 18], [12, 21], [15, 18]], false, false)
            poly(c, [[6, 9], [3, 12], [6, 15]], false, false); poly(c, [[18, 9], [21, 12], [18, 15]], false, false)
            break
        case "moverotate":
            c.beginPath(); c.arc(12, 12, 8, Math.PI * 0.15, Math.PI * 1.75); c.stroke()
            poly(c, [[16, 2], [18.5, 6.4], [13.5, 7.5]], false, false)
            dot(c, 12, 12, 1.8)
            break
        case "movepointtopoint":
            circle(c, 5, 18, 2.4, false); dot(c, 19, 6, 2.6)
            c.setLineDash([2, 2]); poly(c, [[7, 16], [16, 8.5]], false, false); c.setLineDash([])
            poly(c, [[12, 8.5], [16.5, 8], [16, 12.5]], false, false)
            break
        case "movepointtoposition":
            circle(c, 5, 18, 2.4, false)
            poly(c, [[14, 3], [14, 9]], false, false); poly(c, [[11, 6], [17, 6]], false, false)
            c.setLineDash([2, 2]); poly(c, [[7, 16], [12.5, 8.5]], false, false); c.setLineDash([])
            poly(c, [[18, 14], [22, 14]], false, false); poly(c, [[18, 18], [22, 18]], false, false)
            poly(c, [[18, 22], [22, 22]], false, false)
            break
        case "scale":
            poly(c, [[3, 21], [3, 13], [11, 13], [11, 21]], true, true)
            c.setLineDash([2, 2]); poly(c, [[3, 13], [3, 3], [21, 3], [21, 21], [11, 21]], false, false); c.setLineDash([])
            poly(c, [[12, 12], [19, 5]], false, false); poly(c, [[14, 5], [19, 5], [19, 10]], false, false)
            break
        case "splitbody":
            box(c, 3, 8, 11, 11, 5)
            c.strokeStyle = "#d9534f"; poly(c, [[2, 14], [10, 9], [22, 9], [14, 14]], true, false)
            break
        case "splitface":
            poly(c, [[3, 4], [21, 4], [21, 20], [3, 20]], true, true)
            c.strokeStyle = "#d9534f"; poly(c, [[12, 4], [12, 20]], false, false)
            break
        case "offsetface":
            box(c, 3, 10, 11, 9, 4)
            c.setLineDash([2, 2]); poly(c, [[3, 7], [7, 3], [18, 3], [14, 7]], true, false); c.setLineDash([])
            poly(c, [[19, 9], [19, 3]], false, false); poly(c, [[17, 5], [19, 3], [21, 5]], false, false)
            break
        case "offsetbody":
            poly(c, [[7, 7], [17, 7], [17, 17], [7, 17]], true, true)
            c.setLineDash([2, 2]); poly(c, [[3, 3], [21, 3], [21, 21], [3, 21]], true, false); c.setLineDash([])
            break
        case "replaceface":
            box(c, 3, 11, 11, 8, 4)
            c.setLineDash([2, 2]); poly(c, [[3, 6], [7, 2], [18, 2], [14, 6]], true, false); c.setLineDash([])
            poly(c, [[3, 11], [3, 6]], false, false); poly(c, [[14, 11], [14, 6]], false, false)
            break
        case "draft":
            poly(c, [[4, 21], [20, 21], [17, 5], [7, 5]], true, true)
            c.setLineDash([2, 2]); poly(c, [[4, 21], [4, 5]], false, false); c.setLineDash([])
            break
        case "pathpattern":
            c.setLineDash([2, 2]); c.beginPath(); c.moveTo(3, 19); c.bezierCurveTo(8, 4, 16, 22, 21, 6); c.stroke(); c.setLineDash([])
            for (const q of [[3, 19], [9, 12], [15, 14], [21, 6]]) poly(c, [[q[0] - 2, q[1] - 2], [q[0] + 2, q[1] - 2], [q[0] + 2, q[1] + 2], [q[0] - 2, q[1] + 2]], true, true)
            break
        case "rib":
            poly(c, [[2, 21], [22, 21]], false, false); poly(c, [[2, 21], [2, 4]], false, false)
            poly(c, [[2, 8], [16, 21], [12, 21], [2, 12]], true, true)
            break
        case "web":
            poly(c, [[2, 21], [22, 21], [22, 4], [2, 4]], true, false)
            poly(c, [[11, 4], [13, 4], [13, 21], [11, 21]], true, true)
            poly(c, [[2, 11], [22, 11], [22, 13], [2, 13]], true, true)
            break
        // ---- inspect / mesh
        case "measure":
            poly(c, [[2, 16], [16, 2], [22, 8], [8, 22]], true, true)
            for (let k = 0; k < 5; ++k) poly(c, [[5 + k * 2.6, 13 - k * 2.6], [7 + k * 2.6, 15 - k * 2.6]], false, false)
            break
        case "interference":
            box(c, 2, 9, 9, 9, 4); box(c, 9, 5, 9, 9, 4)
            c.strokeStyle = "#d9534f"; poly(c, [[9, 9], [15, 9], [15, 18], [9, 18]], true, false)
            break
        case "section":
            box(c, 3, 8, 11, 11, 5)
            c.fillStyle = Qt.rgba(0.85, 0.33, 0.31, 0.35); c.strokeStyle = "#d9534f"
            poly(c, [[3, 13], [8, 8], [19, 8], [14, 13]], true, true)
            break
        case "zebra":
            poly(c, [[3, 4], [21, 4], [21, 20], [3, 20]], true, false)
            for (let k = 0; k < 4; ++k) poly(c, [[3, 6 + k * 4], [21, 4 + k * 4]], false, false)
            break
        case "curvature":
            c.beginPath(); c.moveTo(3, 19); c.bezierCurveTo(9, 3, 15, 3, 21, 19); c.stroke()
            for (let k = 0; k < 5; ++k) { const x = 6 + k * 3; poly(c, [[x, 13 - Math.sin(k / 4 * Math.PI) * 4], [x, 7 - Math.sin(k / 4 * Math.PI) * 4]], false, false) }
            break
        case "draftanalysis":
            poly(c, [[4, 21], [20, 21], [17, 5], [7, 5]], true, false)
            c.fillStyle = Qt.rgba(0.2, 0.7, 0.3, 0.45); poly(c, [[4, 21], [12, 21], [12, 5], [7, 5]], true, true)
            c.fillStyle = Qt.rgba(0.85, 0.3, 0.3, 0.45); poly(c, [[12, 21], [20, 21], [17, 5], [12, 5]], true, true)
            break
        case "mesh": {
            const p = [[3, 18], [9, 5], [15, 19], [21, 6]]
            poly(c, [[3, 18], [9, 5], [21, 6], [15, 19]], true, true)
            poly(c, [[9, 5], [15, 19]], false, false); poly(c, [[3, 18], [15, 19]], false, false)
            for (const q of p) dot(c, q[0], q[1], 1.6)
            break
        }
        case "insertmesh":
            poly(c, [[3, 20], [9, 9], [17, 20]], true, true); poly(c, [[9, 9], [21, 13], [17, 20]], false, false)
            poly(c, [[17, 9], [17, 2]], false, false); poly(c, [[14, 5], [17, 2], [20, 5]], false, false)
            break
        case "tomesh":
            box(c, 2, 9, 8, 8, 3)
            poly(c, [[13, 13], [16, 13]], false, false); poly(c, [[15, 11], [17, 13], [15, 15]], false, false)
            poly(c, [[18, 18], [20, 8], [23, 18]], true, true)
            break
        case "tobrep":
            poly(c, [[1, 18], [3, 8], [6, 18]], true, true)
            poly(c, [[8, 13], [11, 13]], false, false); poly(c, [[10, 11], [12, 13], [10, 15]], false, false)
            box(c, 13, 9, 7, 7, 3)
            break
        case "reversenormals":
            poly(c, [[3, 14], [21, 14]], false, false)
            poly(c, [[8, 14], [8, 5]], false, false); poly(c, [[6, 7], [8, 5], [10, 7]], false, false)
            poly(c, [[16, 14], [16, 21]], false, false); poly(c, [[14, 19], [16, 21], [18, 19]], false, false)
            break
        case "planecut":
            poly(c, [[3, 20], [9, 6], [21, 12], [15, 21]], true, true)
            c.strokeStyle = "#d9534f"; poly(c, [[1, 13], [23, 13]], false, false)
            break
        case "smooth":
            c.beginPath(); c.moveTo(3, 16); c.bezierCurveTo(8, 6, 16, 6, 21, 16); c.stroke()
            c.setLineDash([2, 2]); poly(c, [[3, 19], [7, 11], [10, 14], [14, 8], [17, 13], [21, 18]], false, false); c.setLineDash([])
            break
        case "reduce":
            poly(c, [[3, 20], [12, 4], [21, 20]], true, true); poly(c, [[7.5, 12], [16.5, 12]], false, false)
            break
        case "meshrepair":
            poly(c, [[3, 19], [10, 5], [17, 19]], true, true); poly(c, [[6.5, 12], [13.5, 12], [10, 19]], true, false)
            c.strokeStyle = "#5cb85c"
            poly(c, [[15, 15], [18, 19], [23, 10]], false, false)
            break
        case "meshbody":
            // The Insert Mesh pyramid (a triangulated tetrahedron), without the arrow
            poly(c, [[3, 20], [10, 4], [16, 20]], true, true)
            poly(c, [[10, 4], [21, 15], [16, 20]], true, true)
            break
        case "facegroups": {
            const tints = ["#e8a05a", "#6fb3d9", "#8fcf7a"]
            const patches = [[[3, 5], [13, 3], [12, 12], [4, 13]], [[13, 3], [21, 6], [20, 14], [12, 12]],
                             [[4, 13], [12, 12], [20, 14], [17, 21], [6, 20]]]
            for (let i = 0; i < 3; ++i) {
                c.fillStyle = tints[i]
                poly(c, patches[i], true, true)
            }
            c.fillStyle = glyph.fill
            break
        }
        case "remesh":
            poly(c, [[3, 20], [12, 4], [21, 20]], true, true)
            c.lineWidth = 1.1
            poly(c, [[7.5, 12], [16.5, 12]], false, false)
            poly(c, [[7.5, 12], [12, 20], [16.5, 12]], false, false)
            poly(c, [[5.25, 16], [9.75, 16]], false, false); poly(c, [[14.25, 16], [18.75, 16]], false, false)
            break
        case "meshdisplay":
            poly(c, [[3, 19], [8, 5], [16, 7], [21, 18]], true, false)
            poly(c, [[8, 5], [11, 13], [16, 7]], false, false)
            poly(c, [[3, 19], [11, 13], [21, 18]], false, false)
            break
        case "remove":
            box(c, 3, 9, 10, 10, 4)
            c.strokeStyle = "#d9534f"
            poly(c, [[15, 3], [22, 10]], false, false); poly(c, [[22, 3], [15, 10]], false, false)
            break
        case "importcad":
            box(c, 3, 11, 9, 8, 3)
            poly(c, [[19, 2], [19, 12]], false, false); poly(c, [[15.5, 8.5], [19, 12], [22.5, 8.5]], false, false)
            poly(c, [[14, 21], [22, 21]], false, false)
            break
        case "mcmaster":
            // Catalog / part insert: document with search mark (distinct from STEP importcad)
            poly(c, [[4, 3], [14, 3], [16, 5], [16, 21], [4, 21]], true, true)
            poly(c, [[7, 8], [13, 8]], false, false)
            poly(c, [[7, 11], [13, 11]], false, false)
            poly(c, [[7, 14], [11, 14]], false, false)
            circle(c, 17.5, 16.5, 3.5, false)
            poly(c, [[20, 19], [23, 22]], false, false)
            break
        case "script":
            poly(c, [[5, 3], [17, 3], [19, 5], [19, 21], [5, 21]], true, true)
            poly(c, [[9, 9], [7, 11.5], [9, 14]], false, false); poly(c, [[15, 9], [17, 11.5], [15, 14]], false, false)
            poly(c, [[13, 8], [11, 15]], false, false)
            break
        case "sheetrule":
            poly(c, [[3, 16], [13, 16], [13, 19], [3, 19]], true, true)
            c.beginPath(); c.arc(13, 10, 6, Math.PI / 2, 0, true); c.stroke()
            poly(c, [[19, 10], [19, 3]], false, false)
            poly(c, [[16, 10], [16, 3]], false, false)
            break
        case "flange":
            poly(c, [[3, 15], [15, 15], [21, 9], [9, 9]], true, true)
            poly(c, [[3, 15], [3, 18], [15, 18], [21, 12], [21, 9]], false, false)
            poly(c, [[15, 15], [15, 18]], false, false)
            break
        case "edgeflange":
            poly(c, [[2, 18], [12, 18], [17, 13], [7, 13]], true, true)
            poly(c, [[12, 18], [12, 6], [17, 1], [17, 13]], true, true)
            break
        case "flatpattern":
            poly(c, [[2, 9], [9, 9], [9, 15], [2, 15]], true, true)
            poly(c, [[9, 9], [22, 9], [22, 15], [9, 15]], true, true)
            c.setLineDash([2, 2]); poly(c, [[9, 7], [9, 17]], false, false); c.setLineDash([])
            break
        case "hem":
            // a sheet in side view folded back over itself
            poly(c, [[2, 17], [16, 17]], false, false); poly(c, [[2, 14], [16, 14]], false, false)
            c.beginPath(); c.arc(16, 12.5, 4.5, Math.PI / 2, -Math.PI / 2, true); c.stroke()
            c.beginPath(); c.arc(16, 12.5, 1.5, Math.PI / 2, -Math.PI / 2, true); c.stroke()
            poly(c, [[16, 8], [7, 8]], false, false); poly(c, [[16, 11], [7, 11]], false, false); poly(c, [[7, 8], [7, 11]], false, false)
            break
        case "tab":
            poly(c, [[2, 17], [14, 17], [20, 11], [8, 11]], true, true)
            c.strokeStyle = "#e07a1f"
            poly(c, [[11, 14], [15, 6], [21, 6], [17, 14]], true, false)
            break
        case "bend":
            poly(c, [[2, 18], [10, 18]], false, false); poly(c, [[2, 15], [10, 15]], false, false)
            c.beginPath(); c.arc(10, 10, 8, Math.PI / 2, 0, true); c.stroke()
            c.beginPath(); c.arc(10, 10, 5, Math.PI / 2, 0, true); c.stroke()
            poly(c, [[18, 10], [18, 3]], false, false); poly(c, [[15, 10], [15, 3]], false, false)
            c.setLineDash([2, 2]); poly(c, [[6, 21], [21, 6]], false, false); c.setLineDash([])
            break
        case "contourflange":
            poly(c, [[3, 6], [3, 18], [12, 18], [12, 9], [21, 9]], false, false)
            poly(c, [[5.5, 6], [5.5, 15.5], [9.5, 15.5], [9.5, 6.5], [21, 6.5]], false, false)
            dot(c, 3, 6, 1.4); dot(c, 21, 9, 1.4)
            break
        case "unfold":
            poly(c, [[2, 19], [12, 19], [12, 16], [2, 16]], true, true)
            c.setLineDash([2, 2]); poly(c, [[12, 16], [12, 5], [15, 5], [15, 16]], false, false); c.setLineDash([])
            poly(c, [[12, 19], [22, 19], [22, 16], [12, 16]], true, true)
            c.beginPath(); c.arc(13.5, 16, 6, -Math.PI / 2, -Math.PI / 6); c.stroke()
            poly(c, [[17, 12.5], [18.7, 13], [19.2, 11.2]], false, false)
            break
        case "refold":
            poly(c, [[2, 19], [12, 19], [12, 16], [2, 16]], true, true)
            poly(c, [[12, 16], [12, 5], [15, 5], [15, 16]], true, true)
            c.setLineDash([2, 2]); poly(c, [[15, 19], [22, 19], [22, 16]], false, false); c.setLineDash([])
            c.beginPath(); c.arc(15, 16, 6, -Math.PI / 6, -Math.PI / 2, true); c.stroke()
            poly(c, [[13.3, 10.3], [15, 10], [14.8, 11.8]], false, false)
            break
        case "flatdxf":
            poly(c, [[2, 5], [16, 5], [16, 14], [2, 14]], true, true)
            c.setLineDash([2, 2]); poly(c, [[7, 3], [7, 16]], false, false); c.setLineDash([])
            poly(c, [[19, 11], [19, 21]], false, false); poly(c, [[15.5, 17.5], [19, 21], [22.5, 17.5]], false, false)
            break
        case "addin":
            // a plug: body with two prongs and a lead
            poly(c, [[6, 9], [16, 9], [16, 14], [13, 18], [9, 18], [6, 14]], true, true)
            poly(c, [[9, 9], [9, 4]], false, false); poly(c, [[13, 9], [13, 4]], false, false)
            poly(c, [[11, 18], [11, 21], [19, 21]], false, false)
            break
        case "rulelibrary":
            poly(c, [[3, 4], [8, 4], [8, 20], [3, 20]], true, true)
            poly(c, [[9.5, 4], [14.5, 4], [14.5, 20], [9.5, 20]], true, true)
            poly(c, [[16, 5.5], [20.5, 4.3], [22, 19], [17.5, 20.2]], true, true)
            poly(c, [[3, 8], [8, 8]], false, false); poly(c, [[9.5, 8], [14.5, 8]], false, false)
            break
        case "comingsoon":
            circle(c, 12, 12, 9, true); poly(c, [[12, 7], [12, 12], [16, 14]], false, false)
            break
        case "body":
        case "box":
        default:
            box(c, 3, 7, 12, 12, 5)
            break
        }
    }
}
