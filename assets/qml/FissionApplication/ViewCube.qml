// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
import QtQuick
import "."

// Orientation cube following the camera; click a face to look at it
Item {
    id: cube
    property var app
    width: 110
    height: 124

    // Face definitions in world space (Z up). u/v span the label so text reads correctly from outside.
    readonly property var faces: [
        { name: "top",    label: "TOP",    n: [0, 0, 1],  u: [1, 0, 0],  v: [0, 1, 0] },
        { name: "bottom", label: "BOTTOM", n: [0, 0, -1], u: [1, 0, 0],  v: [0, -1, 0] },
        { name: "front",  label: "FRONT",  n: [0, -1, 0], u: [1, 0, 0],  v: [0, 0, 1] },
        { name: "back",   label: "BACK",   n: [0, 1, 0],  u: [-1, 0, 0], v: [0, 0, 1] },
        { name: "right",  label: "RIGHT",  n: [1, 0, 0],  u: [0, 1, 0],  v: [0, 0, 1] },
        { name: "left",   label: "LEFT",   n: [-1, 0, 0], u: [0, -1, 0], v: [0, 0, 1] }
    ]
    property string hoveredFace: ""
    property var projected: []     // [{name, pts:[[x,y]...], depth}] front faces, back to front

    readonly property real cx: width / 2
    readonly property real cy: 18 + (height - 18) / 2
    readonly property real scale: 30

    function rot(v) {
        const m = app.camera.rotationMatrix
        const r = m.times(Qt.vector4d(v[0], v[1], v[2], 0))
        return [r.x, r.y, r.z]
    }

    function recompute() {
        const out = []
        for (const f of faces) {
            const n = rot(f.n)
            if (n[2] <= 0.02) continue
            const pts = []
            for (const [a, b] of [[-1, -1], [1, -1], [1, 1], [-1, 1]]) {
                const p = rot([f.n[0] + a * f.u[0] + b * f.v[0], f.n[1] + a * f.u[1] + b * f.v[1], f.n[2] + a * f.u[2] + b * f.v[2]])
                pts.push([cx + p[0] * scale, cy - p[1] * scale])
            }
            const u = rot(f.u), v = rot(f.v), c = rot(f.n)
            out.push({ name: f.name, label: f.label, pts: pts, depth: n[2],
                       origin: [cx + c[0] * scale, cy - c[1] * scale],
                       ux: [u[0] * scale, -u[1] * scale], vx: [v[0] * scale, -v[1] * scale] })
        }
        out.sort((a, b) => a.depth - b.depth)
        projected = out
        canvas.requestPaint()
    }

    Connections {
        target: cube.app.camera
        function onChanged() { cube.recompute() }
    }
    Component.onCompleted: recompute()

    function inside(pt, poly) {
        let c = false
        for (let i = 0, j = poly.length - 1; i < poly.length; j = i++) {
            if (((poly[i][1] > pt[1]) !== (poly[j][1] > pt[1]))
                    && (pt[0] < (poly[j][0] - poly[i][0]) * (pt[1] - poly[i][1]) / (poly[j][1] - poly[i][1]) + poly[i][0]))
                c = !c
        }
        return c
    }
    function faceAt(x, y) {
        for (let i = projected.length - 1; i >= 0; --i)
            if (inside([x, y], projected[i].pts)) return projected[i].name
        return ""
    }

    Canvas {
        id: canvas
        anchors.fill: parent
        antialiasing: true
        onPaint: {
            const c = getContext("2d")
            c.reset()
            for (const f of cube.projected) {
                c.beginPath()
                c.moveTo(f.pts[0][0], f.pts[0][1])
                for (let i = 1; i < 4; ++i) c.lineTo(f.pts[i][0], f.pts[i][1])
                c.closePath()
                const shade = 0.82 + 0.18 * f.depth
                c.fillStyle = f.name === cube.hoveredFace ? "#bfe3f6"
                            : Qt.rgba(0.93 * shade, 0.94 * shade, 0.96 * shade, 0.96)
                c.fill()
                c.strokeStyle = "#7d8792"
                c.lineWidth = 1
                c.stroke()
                if (f.depth < 0.25) continue
                c.save()
                // Map label space (x right, y down) onto the face
                c.setTransform(f.ux[0] / 30, f.ux[1] / 30, -f.vx[0] / 30, -f.vx[1] / 30, f.origin[0], f.origin[1])
                c.font = "bold 9px 'Segoe UI'"
                c.fillStyle = "#3c4249"
                c.textAlign = "center"
                c.textBaseline = "middle"
                c.fillText(f.label, 0, 0)
                c.restore()
            }
            // Axis triad
            const o = [12, cube.height - 12]
            const axes = [[[1, 0, 0], "#d64541"], [[0, 1, 0], "#2e9e5b"], [[0, 0, 1], "#2f6fd6"]]
            for (const [v, col] of axes) {
                const p = cube.rot(v)
                c.strokeStyle = col
                c.lineWidth = 2
                c.beginPath()
                c.moveTo(o[0], o[1])
                c.lineTo(o[0] + p[0] * 14, o[1] - p[1] * 14)
                c.stroke()
            }
        }
    }

    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        onPositionChanged: (m) => {
            const f = cube.faceAt(m.x, m.y)
            if (f !== cube.hoveredFace) { cube.hoveredFace = f; canvas.requestPaint() }
        }
        onExited: { cube.hoveredFace = ""; canvas.requestPaint() }
        onClicked: (m) => {
            const f = cube.faceAt(m.x, m.y)
            if (f) cube.app.camera.setView(f, true)
        }
    }

    // Home button
    Rectangle {
        x: 2
        y: 0
        width: 20
        height: 18
        radius: 3
        color: homeArea.containsMouse ? Theme.hover : "transparent"
        Glyph { anchors.centerIn: parent; width: 15; height: 15; name: "home" }
        MouseArea {
            id: homeArea
            anchors.fill: parent
            hoverEnabled: true
            onClicked: { cube.app.camera.setView("home", true); cube.app.viewport.renderer.fitAll(true) }
        }
    }
}
