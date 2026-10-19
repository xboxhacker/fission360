// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
import QtQuick

// Move/Copy manipulator: axis arrows translate along one axis, the squares between two arrows translate in
// that plane, the rings rotate about an axis and the center ball (or a moved body itself, beginGrab) drags
// in the view plane.
// The owner hands in `gizmo`:
//   { origin(): [x, y, z], axes(): [[x, y, z] x3], arrows / planes / rings: [bool x3], center: bool,
//     begin(), translate([dx, dy, dz]) (total since begin), rotate(axisIndex, degrees) (total), end(),
//     snapLength (optional) }
Canvas {
    id: triad
    property var renderer
    property var camera
    property var gizmo: null
    property string hot: ""          // "arrow0" | "plane1" | "ring2" | "center" | ""
    property var drag: null
    property string readout: ""
    property point readoutAt
    readonly property var colors: ["#e0473c", "#3cae4a", "#3f7fe0"]
    visible: !!gizmo

    // Screen-space size of the manipulator: arrows about 95 px long
    function frame() {
        if (!gizmo || gizmo.visible === false) return null
        const o = gizmo.origin(), ax = gizmo.axes()
        const ov = Qt.vector3d(o[0], o[1], o[2])
        const so = renderer.projectPoint(ov)
        if (!so.length) return null
        let px = 0
        for (const a of ax) {
            const q = renderer.projectPoint(ov.plus(Qt.vector3d(a[0], a[1], a[2])))
            if (q.length) px = Math.max(px, Math.hypot(q[0] - so[0], q[1] - so[1]))
        }
        if (px < 1e-9) return null
        const L = 95 / px
        const at = (k, s) => ov.plus(Qt.vector3d(ax[k][0], ax[k][1], ax[k][2]).times(s))
        return { o: ov, so: so, L: L, ax: ax, at: at }
    }
    function sp(v) { const q = renderer.projectPoint(v); return q.length ? q : null }
    function segDist(x, y, a, b) {
        const dx = b[0] - a[0], dy = b[1] - a[1], L2 = dx * dx + dy * dy
        const t = L2 > 0 ? Math.max(0, Math.min(1, ((x - a[0]) * dx + (y - a[1]) * dy) / L2)) : 0
        return Math.hypot(x - a[0] - t * dx, y - a[1] - t * dy)
    }
    function inQuad(x, y, q) {
        let inside = false
        for (let i = 0, j = q.length - 1; i < q.length; j = i++)
            if ((q[i][1] > y) !== (q[j][1] > y) && x < (q[j][0] - q[i][0]) * (y - q[i][1]) / (q[j][1] - q[i][1]) + q[i][0])
                inside = !inside
        return inside
    }
    function others(k) { return [[1, 2], [0, 2], [0, 1]][k] }
    function ringPts(f, k) {
        const [i, j] = others(k)
        const out = []
        for (let s = 0; s <= 48; ++s) {
            const t = s / 48 * Math.PI * 2
            const v = f.o.plus(Qt.vector3d(f.ax[i][0], f.ax[i][1], f.ax[i][2]).times(Math.cos(t) * f.L * 0.72))
                         .plus(Qt.vector3d(f.ax[j][0], f.ax[j][1], f.ax[j][2]).times(Math.sin(t) * f.L * 0.72))
            const q = sp(v)
            if (q) out.push(q)
        }
        return out
    }
    function planeQuad(f, k) {
        const [i, j] = others(k)
        const a = 0.22 * f.L, b = 0.4 * f.L
        const ai = Qt.vector3d(f.ax[i][0], f.ax[i][1], f.ax[i][2]), aj = Qt.vector3d(f.ax[j][0], f.ax[j][1], f.ax[j][2])
        const pts = [f.o.plus(ai.times(a)).plus(aj.times(a)), f.o.plus(ai.times(b)).plus(aj.times(a)),
                     f.o.plus(ai.times(b)).plus(aj.times(b)), f.o.plus(ai.times(a)).plus(aj.times(b))].map(sp)
        return pts.every(p => p) ? pts : null
    }

    function handleAt(x, y) {
        const f = frame()
        if (!f) return ""
        if (gizmo.center !== false && Math.hypot(x - f.so[0], y - f.so[1]) < 9) return "center"
        for (let k = 0; k < 3; ++k) {
            if (!gizmo.arrows[k]) continue
            const a = sp(f.at(k, f.L * 0.25)), b = sp(f.at(k, f.L * 1.12))
            if (a && b && segDist(x, y, a, b) < 7) return "arrow" + k
        }
        for (let k = 0; k < 3; ++k) {
            if (!gizmo.planes[k]) continue
            const q = planeQuad(f, k)
            if (q && inQuad(x, y, q)) return "plane" + k
        }
        for (let k = 0; k < 3; ++k) {
            if (!gizmo.rings[k]) continue
            const pts = ringPts(f, k)
            for (let s = 1; s < pts.length; ++s)
                if (segDist(x, y, pts[s - 1], pts[s]) < 6) return "ring" + k
        }
        return ""
    }
    function hover(x, y) {
        const h = gizmo ? handleAt(x, y) : ""
        if (h !== hot) { hot = h; requestPaint() }
        return h !== ""
    }

    function snapLen(v) {
        const step = gizmo.snapLength ? gizmo.snapLength : Math.max(0.01, renderer.snapStep() / 2)
        return Math.round(v / step) * step
    }
    function begin(x, y) {
        const h = gizmo ? handleAt(x, y) : ""
        if (!h) return false
        const f = frame()
        const k = Number(h.slice(-1))
        const d = { handle: h, k: k, o: f.o, ax: f.ax }
        if (h.startsWith("arrow")) {
            d.t0 = renderer.axisParameter(x, y, f.o, Qt.vector3d(f.ax[k][0], f.ax[k][1], f.ax[k][2]))
            if (d.t0 === undefined) return false
        } else if (h.startsWith("plane") || h.startsWith("ring")) {
            d.w0 = renderer.planePoint(x, y, f.o, Qt.vector3d(f.ax[k][0], f.ax[k][1], f.ax[k][2]))
            if (d.w0 === undefined) return false
            if (h.startsWith("ring")) { d.a0 = ringAngle(d, d.w0); d.last = 0; d.turns = 0 }
        } else {
            d.n = camera.eye.minus(f.o).normalized()
            d.w0 = renderer.planePoint(x, y, f.o, d.n)
            if (d.w0 === undefined) return false
        }
        drag = d
        hot = h
        gizmo.begin()
        return true
    }
    // Free drag of a body itself: like the center ball, in the view plane through the grabbed point p [x, y, z]
    function beginGrab(x, y, p) {
        if (!gizmo || gizmo.visible === false) return false
        const o = Qt.vector3d(p[0], p[1], p[2])
        const n = camera.eye.minus(o).normalized()
        const w0 = renderer.planePoint(x, y, o, n)
        if (w0 === undefined) return false
        drag = { handle: "center", k: 0, o: o, ax: gizmo.axes(), n: n, w0: w0 }
        hot = "center"
        gizmo.begin()
        return true
    }
    function ringAngle(d, w) {
        const [i, j] = others(d.k)
        const v = w.minus(d.o)
        const ai = Qt.vector3d(d.ax[i][0], d.ax[i][1], d.ax[i][2]), aj = Qt.vector3d(d.ax[j][0], d.ax[j][1], d.ax[j][2])
        return Math.atan2(v.dotProduct(aj), v.dotProduct(ai)) * 180 / Math.PI
    }
    function move(x, y, mods) {
        const d = drag
        if (!d) return
        const axis = Qt.vector3d(d.ax[d.k][0], d.ax[d.k][1], d.ax[d.k][2])
        const lt = v => gizmo.format ? gizmo.format(v) : (Math.round(v * 1000) / 1000) + " mm"
        if (d.handle.startsWith("arrow")) {
            const t = renderer.axisParameter(x, y, d.o, axis)
            if (t === undefined) return
            const s = snapLen(t - d.t0)
            gizmo.translate([axis.x * s, axis.y * s, axis.z * s])
            readout = "XYZ"[d.k] + "  " + lt(s)
        } else if (d.handle.startsWith("ring")) {
            const w = renderer.planePoint(x, y, d.o, axis)
            if (w === undefined) return
            let a = ringAngle(d, w) - d.a0
            while (a - d.last > 180) a -= 360
            while (a - d.last < -180) a += 360
            d.last = a
            const step = (mods & Qt.ShiftModifier) ? 15 : 1
            const deg = Math.round(a / step) * step
            gizmo.rotate(d.k, deg)
            readout = "XYZ"[d.k] + " angle  " + deg + "\u00b0"
        } else {
            const n = d.handle === "center" ? d.n : axis
            const w = renderer.planePoint(x, y, d.o, n)
            if (w === undefined) return
            const v = w.minus(d.w0)
            const out = [snapLen(v.x), snapLen(v.y), snapLen(v.z)]
            if (d.handle !== "center") out[d.k] = 0
            gizmo.translate(out)
            readout = out.map((c, i) => "XYZ"[i] + " " + lt(c)).join("   ")
        }
        readoutAt = Qt.point(x, y)
        requestPaint()
    }
    function end() {
        if (!drag) return
        drag = null
        readout = ""
        gizmo.end()
        requestPaint()
    }

    // The owner's binding may hand back the same gizmo after every rebuild; only a new one resets the drag
    property var shownGizmo: null
    onGizmoChanged: {
        if (gizmo !== shownGizmo) { shownGizmo = gizmo; hot = ""; drag = null; readout = "" }
        requestPaint()
    }
    onPaint: {
        const c = getContext("2d")
        c.reset()
        const f = frame()
        if (!f) return
        const hl = "#f5c518"
        // rings first so the arrows stay on top
        for (let k = 0; k < 3; ++k) {
            if (!gizmo.rings[k]) continue
            const pts = ringPts(f, k)
            if (pts.length < 2) continue
            c.strokeStyle = hot === "ring" + k ? hl : colors[k]
            c.lineWidth = hot === "ring" + k ? 3.5 : 2
            c.globalAlpha = 0.9
            c.beginPath()
            c.moveTo(pts[0][0], pts[0][1])
            for (let s = 1; s < pts.length; ++s) c.lineTo(pts[s][0], pts[s][1])
            c.stroke()
        }
        c.globalAlpha = 1
        for (let k = 0; k < 3; ++k) {
            if (!gizmo.planes[k]) continue
            const q = planeQuad(f, k)
            if (!q) continue
            c.fillStyle = hot === "plane" + k ? "rgba(245,197,24,0.75)" : colors[k]
            c.globalAlpha = hot === "plane" + k ? 1 : 0.45
            c.beginPath()
            c.moveTo(q[0][0], q[0][1])
            for (let s = 1; s < 4; ++s) c.lineTo(q[s][0], q[s][1])
            c.closePath()
            c.fill()
        }
        c.globalAlpha = 1
        for (let k = 0; k < 3; ++k) {
            if (!gizmo.arrows[k]) continue
            const a = sp(f.o), b = sp(f.at(k, f.L))
            const tip = sp(f.at(k, f.L * 1.22))
            if (!a || !b || !tip) continue
            const col = hot === "arrow" + k ? hl : colors[k]
            c.strokeStyle = col
            c.fillStyle = col
            c.lineWidth = hot === "arrow" + k ? 4 : 2.5
            c.beginPath(); c.moveTo(a[0], a[1]); c.lineTo(b[0], b[1]); c.stroke()
            let dx = tip[0] - b[0], dy = tip[1] - b[1]
            const len = Math.hypot(dx, dy)
            if (len > 1e-6) {
                dx /= len; dy /= len
                const head = Math.max(10, len), half = 6
                c.beginPath()
                c.moveTo(b[0] + dx * head, b[1] + dy * head)
                c.lineTo(b[0] - dy * half, b[1] + dx * half)
                c.lineTo(b[0] + dy * half, b[1] - dx * half)
                c.closePath()
                c.fill()
            }
        }
        if (gizmo.center !== false) {
            c.fillStyle = hot === "center" ? hl : "white"
            c.strokeStyle = "#1d2a38"
            c.lineWidth = 1.5
            c.beginPath(); c.arc(f.so[0], f.so[1], 6.5, 0, Math.PI * 2); c.fill(); c.stroke()
        }
        if (readout) {
            c.font = "12px 'Segoe UI'"
            const w = c.measureText(readout).width + 12
            const x = Math.min(readoutAt.x + 16, width - w - 4), y = Math.max(4, readoutAt.y - 30)
            c.fillStyle = "rgba(29,42,56,0.92)"
            c.fillRect(x, y, w, 20)
            c.fillStyle = "white"
            c.fillText(readout, x + 6, y + 14)
        }
    }
}
