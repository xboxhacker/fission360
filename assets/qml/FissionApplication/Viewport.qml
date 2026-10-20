// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Fission.Render
import "."

// 3D view: navigation, picking, sketch drawing and the viewport overlays
Item {
    id: vp
    property var app
    property alias renderer: r
    property alias moveTriad: triad
    property bool orbitMode: false
    property bool panMode: false

    // Sketch tool input: committed clicks ([u, v]) and the snapped cursor ({u, v, snap})
    property var pts: []
    property var cursor: null
    property point cursorPx: Qt.point(0, 0)
    property var ptSnaps: []       // sketch point ({e, p}) each committed click snapped to, or null
    property int chainFirst: -1    // first and last line of the current line chain
    property int chainPrev: -1
    // Constraint / dimension tools: picked items ({e} curve, {e, p} point, with u, v)
    property var cpicks: []
    property int selectedConstraint: -1
    property int editingConstraint: -1
    property var annoHits: []      // painted labels and badges: [{index, x, y, w, h, dim}]
    readonly property bool constraintTool: app.sketchTool === "dimension" || app.sketchTool.startsWith("constraint:")
    // Sketch modify tools: picked curve indices (offset chain, first fillet line)
    readonly property bool modifyTool: ["trim", "extend", "offset", "fillet", "project"].indexOf(app.sketchTool) >= 0
    property var mpicks: []
    readonly property bool sketchInputActive: pts.length > 0 || cpicks.length > 0 || mpicks.length > 0
    // Drag of sketch geometry or a dimension label (no tool active)
    property var sdrag: null

    clip: true

    BRepRenderer {
        id: r
            anchors.fill: parent
        document: vp.app.doc
        camera: vp.app.camera
        selection: vp.app.selection
        analysis: vp.app.analysis
        pullDirection: ({ X: Qt.vector3d(1, 0, 0), Y: Qt.vector3d(0, 1, 0), Z: Qt.vector3d(0, 0, 1) })[vp.app.pullAxis]
        draftAngle: vp.app.draftAngle
        sectionEnabled: vp.app.sectionEnabled && !!vp.app.sectionFrame.ok
        sectionNormal: {
            const f = vp.app.sectionFrame
            const n = f.ok ? Qt.vector3d(f.normal[0], f.normal[1], f.normal[2]) : Qt.vector3d(0, 0, 1)
            return vp.app.sectionFlip ? n.times(-1) : n
        }
        sectionOffset: {
            const f = vp.app.sectionFrame
            const d = (f.ok ? f.origin[0] * f.normal[0] + f.origin[1] * f.normal[1] + f.origin[2] * f.normal[2] : 0)
                      + vp.app.sectionOffset
            return vp.app.sectionFlip ? -d : d
        }
        backgroundTop: fissionApp.setting("viewport/backgroundTop", "#3c4148")
        backgroundBottom: fissionApp.setting("viewport/backgroundBottom", "#1e2125")
        snapToGrid: vp.app ? vp.app.sketchSnapToGrid : true
        // Origin items shown: the ones switched on in the browser, plus whatever the current pick accepts
        originItems: {
            const a = vp.app, vis = a.originVisible, f = a.selection.filter
            const out = Object.keys(vis).filter(k => vis[k])
            const accepts = k => f.split(",").indexOf(k) >= 0
            const add = ks => { for (const k of ks) if (out.indexOf(k) < 0) out.push(k) }
            if (a.pickingSketchPlane || a.sectionAwaitingPlane || (!a.activeSketchId && (accepts("plane") || accepts("originplane")))) add(["XY", "XZ", "YZ"])
            if (accepts("axis") || accepts("originaxis")) add(["X", "Y", "Z"])
            if (accepts("point") || accepts("originpoint")) add(["O"])
            return out
        }
        Component.onCompleted: {
            backgroundTop = fissionApp.setting("viewport/backgroundTop", "#3c4148")
            backgroundBottom = fissionApp.setting("viewport/backgroundBottom", "#1e2125")
            snapToGrid = vp.app ? vp.app.sketchSnapToGrid : true
            const g = fissionApp.setting("viewport/showGrid", true)
            showGrid = g !== false && g !== "false"
        }
    }

    // ------------------------------------------------------------------ sketch tools

    function round4(v) { return Math.round(v * 10000) / 10000 }
    function same(a, b) { return Math.abs(a[0] - b[0]) < 1e-7 && Math.abs(a[1] - b[1]) < 1e-7 }
    function dist(a, b) { return Math.hypot(a[0] - b[0], a[1] - b[1]) }

    function sketchUV(x, y) {
        const p = r.sketchPoint(x, y, app.activeSketchId, true)
        return p.u === undefined ? null : p
    }

    function sketchMove(x, y) {
        cursor = sketchUV(x, y)
        cursorPx = Qt.point(x, y)
        preview.requestPaint()
    }

    function cancelSketchInput() {
        pts = []
        ptSnaps = []
        chainFirst = chainPrev = -1
        cpicks = []
        if (mpicks.length) { mpicks = []; app.selection.clear() }
        dimLocks = []
        dimTexts = []
        preview.requestPaint()
    }

    // ------------------------------------------------------------------ typed sketch dimensions

    // Dimensions the active tool asks for at the current step. A typed value locks that dimension;
    // the cursor still chooses the direction.
    property var dimLocks: []      // evaluated value (mm) or null per dimension
    property var dimTexts: []      // the typed expressions; they become driving dimensions
    function dimSpecs() {
        const n = pts.length
        switch (app.sketchTool) {
        case "line": case "centerline": return n ? ["Length", "Angle"] : []
        case "rectangle": case "centerrectangle": return n ? ["Width", "Height"] : []
        case "ellipse": return n ? ["Width", "Height"] : []
        case "circle": return n ? ["Diameter"] : []
        case "polygon": return n ? ["Radius"] : []
        case "slot": return n === 1 ? ["Length"] : n === 2 ? ["Width"] : []
        }
        return []
    }
    function dimLock(i) {
        const v = dimLocks[i]
        return v === undefined || v === null ? null : v
    }
    // Returns "" when accepted, otherwise the expression error
    function setDimLock(i, text) {
        const locks = dimLocks.slice(), texts = dimTexts.slice()
        let err = ""
        texts[i] = null
        if (text.trim().length === 0) {
            locks[i] = null
        } else {
            const isAngle = dimSpecs()[i] === "Angle"
            if (!isAngle) text = app.withUnit(text, "length")
            const r = app.doc.evaluateExpression(text)
            if (r.error) { locks[i] = null; err = r.error }
            else if (isAngle) {
                // Angles keep their sign (counter-clockwise from +X); a bare number means degrees
                const deg = /^[-+]?[0-9.]+$/.test(text.trim()) ? parseFloat(text) : r.value
                locks[i] = deg
                texts[i] = null
            }
            else { locks[i] = Math.abs(r.value); texts[i] = r.value < 0 ? "abs(" + text.trim() + ")" : text.trim() }
        }
        dimLocks = locks
        dimTexts = texts
        preview.requestPaint()
        return err
    }
    function hasDimLock() { return dimLocks.some(v => v !== null && v !== undefined) }

    function unitDir(a, b) {
        const d = dist(a, b)
        return d > 1e-9 ? [(b[0] - a[0]) / d, (b[1] - a[1]) / d] : [1, 0]
    }
    // Line direction snaps to horizontal / vertical within this many degrees
    readonly property real axisSnapDeg: 4
    // Line from a to the cursor c: typed angle, else snapped to the nearest axis when close, else free.
    // Returns {dir, len, snapped}
    function lineDirection(a, c, endpointSnap) {
        const dx = c[0] - a[0], dy = c[1] - a[1], len = Math.hypot(dx, dy)
        const A = dimLock(1)
        if (A !== null) {
            const t = A * Math.PI / 180, d = [Math.cos(t), Math.sin(t)]
            return { dir: d, len: Math.max(0, dx * d[0] + dy * d[1]), snapped: false }
        }
        if (len < 1e-9 || endpointSnap) return { dir: len < 1e-9 ? [1, 0] : [dx / len, dy / len], len: len, snapped: false }
        const deg = Math.atan2(dy, dx) * 180 / Math.PI
        const nearest = Math.round(deg / 90) * 90
        if (Math.abs(deg - nearest) <= axisSnapDeg) {
            const t = nearest * Math.PI / 180, d = [Math.round(Math.cos(t)), Math.round(Math.sin(t))]
            return { dir: d, len: Math.abs(dx * d[0] + dy * d[1]), snapped: true }
        }
        return { dir: [dx / len, dy / len], len: len, snapped: false }
    }
    function lineEnd(a, c, endpointSnap) {
        const ld = lineDirection(a, c, endpointSnap)
        const L = dimLock(0)
        const len = L === null ? ld.len : L
        return [a[0] + ld.dir[0] * len, a[1] + ld.dir[1] * len]
    }
    readonly property bool cursorOnPoint: !!cursor && cursor.snap === "endpoint"

    // Cursor position (sketch coordinates) with the locked dimensions applied
    function constrained(c, endpointSnap) {
        const n = pts.length
        if (!n) return c
        const sgn = x => x < 0 ? -1 : 1
        const along = (a, len) => { const u = unitDir(a, c); return [a[0] + u[0] * len, a[1] + u[1] * len] }
        switch (app.sketchTool) {
        case "line": {
            if (endpointSnap === undefined) endpointSnap = cursorOnPoint
            return lineEnd(pts[n - 1], c, endpointSnap && dimLock(0) === null && dimLock(1) === null)
        }
        case "centerline": {
            // The typed length is the whole line; the cursor sets one end
            if (endpointSnap === undefined) endpointSnap = cursorOnPoint
            const a = pts[0], ld = lineDirection(a, c, endpointSnap && dimLock(0) === null && dimLock(1) === null)
            const L = dimLock(0), half = L === null ? ld.len : L / 2
            return [a[0] + ld.dir[0] * half, a[1] + ld.dir[1] * half]
        }
        case "circle": { const D = dimLock(0); return D === null ? c : along(pts[0], D / 2) }
        case "polygon": { const R = dimLock(0); return R === null ? c : along(pts[0], R) }
        case "rectangle": case "ellipse": case "centerrectangle": {
            const a = pts[0], k = app.sketchTool === "rectangle" ? 1 : 0.5
            const w = dimLock(0), h = dimLock(1)
            return [w === null ? c[0] : a[0] + sgn(c[0] - a[0]) * w * k,
                    h === null ? c[1] : a[1] + sgn(c[1] - a[1]) * h * k]
        }
        case "slot": {
            if (n === 1) { const L = dimLock(0); return L === null ? c : along(pts[0], L) }
            const W = dimLock(0)
            if (W === null) return c
            const a = pts[0], u = unitDir(a, pts[1]), nrm = [-u[1], u[0]]
            const side = sgn((c[0] - a[0]) * nrm[0] + (c[1] - a[1]) * nrm[1])
            return [a[0] + nrm[0] * side * W / 2, a[1] + nrm[1] * side * W / 2]
        }
        }
        return c
    }
    // Current value of each dimension at the (constrained) cursor
    function liveDims() {
        if (!cursor || !pts.length) return []
        const c = constrained([cursor.u, cursor.v]), n = pts.length, a = pts[0]
        switch (app.sketchTool) {
        case "line": {
            const a = pts[n - 1]
            return [dist(a, c), Math.round(Math.atan2(c[1] - a[1], c[0] - a[0]) * 1800 / Math.PI) / 10]
        }
        case "centerline": return [2 * dist(a, c), Math.round(Math.atan2(c[1] - a[1], c[0] - a[0]) * 1800 / Math.PI) / 10]
        case "rectangle": return [Math.abs(c[0] - a[0]), Math.abs(c[1] - a[1])]
        case "ellipse": case "centerrectangle": return [2 * Math.abs(c[0] - a[0]), 2 * Math.abs(c[1] - a[1])]
        case "circle": return [2 * dist(a, c)]
        case "polygon": return [dist(a, c)]
        case "slot":
            if (n === 1) return [dist(a, c)]
            const b = pts[1], L = dist(a, b)
            return L > 1e-9 ? [2 * Math.abs((b[0] - a[0]) * (a[1] - c[1]) - (a[0] - c[0]) * (b[1] - a[1])) / L] : [0]
        }
        return []
    }

    // Enter: commits the typed dimensions, otherwise ends a line chain / spline
    property bool enterGuard: false
    function enterPressed() {
        if (enterGuard) return
        enterGuard = true
        Qt.callLater(() => vp.enterGuard = false)
        if (hasDimLock()) {
            const n = pts.length
            const base = cursor ? [cursor.u, cursor.v] : [pts[n - 1][0] + 1, pts[n - 1][1]]
            const c = constrained(base)
            commitPoint([round4(c[0]), round4(c[1])])
        } else {
            finishSketchInput()
        }
        vp.forceActiveFocus()
    }

    // Runs fn(sketch) as one undo step; returns the index of the first new entity (or -1)
    function addEntity(fn) {
        const s = app.activeSketch
        if (!s) return -1
        app.doc.checkpoint()
        const before = s.entityCount
        const idx = fn(s)
        if (app.sketchConstruction)
            for (let i = before; i < s.entityCount; ++i) s.setConstruction(i, true)
        return idx === undefined || idx < 0 ? -1 : idx
    }

    // Constraints the drawing implies; they always hold for the new geometry, so failures are ignored
    function autoConstrain(c) {
        const s = app.activeSketch
        if (s) s.addConstraint(c)
    }
    // Ties a new entity point to the sketch point the click snapped to
    function snapConstraint(e, role, snap) {
        if (!snap || snap.e === e) return
        if (snap.p === "mid") autoConstrain({ type: "midpoint", a: { e: e, p: role }, b: { e: snap.e } })
        else autoConstrain({ type: "coincident", a: { e: e, p: role }, b: { e: snap.e, p: snap.p } })
    }
    function dimText(i) { const t = dimTexts[i]; return t === undefined || t === null ? "" : t }

    function finishSketchInput() {
        if (app.sketchTool === "spline" && pts.length >= 2) {
            const list = pts.map(p => [p[0], p[1]])
            addEntity(s => s.addSpline(list))
        }
        cancelSketchInput()
    }

    function sketchClick(x, y) {
        if (constraintTool) { constraintClick(x, y); return }
        if (modifyTool) { modifyClick(x, y); return }
        const p = sketchUV(x, y)
        if (!p) return
        const c = constrained([p.u, p.v], p.snap === "endpoint")
        const snap = p.entity !== undefined && same(c, [p.u, p.v]) ? { e: p.entity, p: p.role } : null
        commitPoint([round4(c[0]), round4(c[1])], snap)
    }

    function commitPoint(q, snap) {
        const n = pts.length
        const t = app.sketchTool
        if (n && same(q, pts[n - 1])) return
        snap = snap || null
        const push = () => { pts = pts.concat([q]); ptSnaps = ptSnaps.concat([snap]) }
        switch (t) {
        case "point": {
            const idx = addEntity(s => s.addPoint(q[0], q[1]))
            if (idx >= 0) snapConstraint(idx, "pos", snap)
            break
        }
        case "line": {
            if (n === 0) { push(); break }
            const a = pts[n - 1], closes = n > 1 && same(q, pts[0])
            const idx = addEntity(s => s.addLine(a[0], a[1], q[0], q[1]))
            if (idx < 0) break
            if (chainPrev >= 0) autoConstrain({ type: "coincident", a: { e: idx, p: "start" }, b: { e: chainPrev, p: "end" } })
            else snapConstraint(idx, "start", ptSnaps[n - 1])
            if (closes && chainFirst >= 0)
                autoConstrain({ type: "coincident", a: { e: idx, p: "end" }, b: { e: chainFirst, p: "start" } })
            else snapConstraint(idx, "end", snap)
            if (Math.abs(a[1] - q[1]) < 1e-9) autoConstrain({ type: "horizontal", a: { e: idx } })
            else if (Math.abs(a[0] - q[0]) < 1e-9) autoConstrain({ type: "vertical", a: { e: idx } })
            if (dimText(0)) autoConstrain({ type: "distance", a: { e: idx, p: "start" }, b: { e: idx, p: "end" }, value: dimText(0) })
            if (chainFirst < 0) chainFirst = idx
            chainPrev = idx
            if (closes) { pts = []; ptSnaps = []; chainFirst = chainPrev = -1 }   // closing the loop ends the chain
            else { pts = [pts[0], q]; ptSnaps = [ptSnaps[0], snap] }
            break
        }
        case "rectangle": {
            if (n === 0) { push(); break }
            const a = pts[0]
            if (Math.abs(q[0] - a[0]) < 1e-9 || Math.abs(q[1] - a[1]) < 1e-9) return
            const idx = addEntity(s => s.addRectangleLines(a[0], a[1], round4(q[0] - a[0]), round4(q[1] - a[1])))
            if (idx >= 0) {
                snapConstraint(idx, "start", ptSnaps[0])
                snapConstraint(idx + 1, "end", snap)
                if (dimText(0)) autoConstrain({ type: "distance", a: { e: idx, p: "start" }, b: { e: idx, p: "end" }, value: dimText(0) })
                if (dimText(1)) autoConstrain({ type: "distance", a: { e: idx + 1, p: "start" }, b: { e: idx + 1, p: "end" }, value: dimText(1) })
            }
            pts = []; ptSnaps = []
            break
        }
        case "centerrectangle": {
            // Rectangle about a center point, which sits at the middle of a construction diagonal
            if (n === 0) { push(); break }
            const m = pts[0], a = [round4(2 * m[0] - q[0]), round4(2 * m[1] - q[1])]
            if (Math.abs(q[0] - m[0]) < 1e-9 || Math.abs(q[1] - m[1]) < 1e-9) return
            let diag = -1, centre = -1
            const idx = addEntity(s => {
                const first = s.addRectangleLines(a[0], a[1], round4(q[0] - a[0]), round4(q[1] - a[1]))
                diag = s.addLine(a[0], a[1], q[0], q[1])
                centre = s.addPoint(m[0], m[1])
                return first
            })
            if (idx >= 0) {
                app.activeSketch.setConstruction(diag, true)
                autoConstrain({ type: "coincident", a: { e: diag, p: "start" }, b: { e: idx, p: "start" } })
                autoConstrain({ type: "coincident", a: { e: diag, p: "end" }, b: { e: idx + 1, p: "end" } })
                autoConstrain({ type: "midpoint", a: { e: centre, p: "pos" }, b: { e: diag } })
                snapConstraint(centre, "pos", ptSnaps[0])
                snapConstraint(idx + 1, "end", snap)
                if (dimText(0)) autoConstrain({ type: "distance", a: { e: idx, p: "start" }, b: { e: idx, p: "end" }, value: dimText(0) })
                if (dimText(1)) autoConstrain({ type: "distance", a: { e: idx + 1, p: "start" }, b: { e: idx + 1, p: "end" }, value: dimText(1) })
            }
            pts = []; ptSnaps = []
            break
        }
        case "centerline": {
            // A line through its midpoint: the clicked center stays in the middle
            if (n === 0) { push(); break }
            const m = pts[0], a = [round4(2 * m[0] - q[0]), round4(2 * m[1] - q[1])]
            let centre = -1
            const idx = addEntity(s => {
                const line = s.addLine(a[0], a[1], q[0], q[1])
                centre = s.addPoint(m[0], m[1])
                return line
            })
            if (idx >= 0) {
                autoConstrain({ type: "midpoint", a: { e: centre, p: "pos" }, b: { e: idx } })
                snapConstraint(centre, "pos", ptSnaps[0])
                snapConstraint(idx, "end", snap)
                if (Math.abs(a[1] - q[1]) < 1e-9) autoConstrain({ type: "horizontal", a: { e: idx } })
                else if (Math.abs(a[0] - q[0]) < 1e-9) autoConstrain({ type: "vertical", a: { e: idx } })
                if (dimText(0)) autoConstrain({ type: "distance", a: { e: idx, p: "start" }, b: { e: idx, p: "end" }, value: dimText(0) })
            }
            pts = []; ptSnaps = []
            break
        }
        case "circle": case "polygon": {
            if (n === 0) { push(); break }
            const rad = round4(dist(pts[0], q))
            const c = pts[0]
            const idx = t === "circle" ? addEntity(s => s.addCircle(c[0], c[1], rad))
                                       : addEntity(s => s.addPolygon(c[0], c[1], rad, app.polygonSides))
            if (idx >= 0) {
                snapConstraint(idx, "center", ptSnaps[0])
                if (t === "circle" && dimText(0)) autoConstrain({ type: "diameter", a: { e: idx }, value: dimText(0) })
            }
            pts = []; ptSnaps = []
            break
        }
        case "ellipse": {
            if (n === 0) { push(); break }
            const c = pts[0]
            const rx = round4(Math.abs(q[0] - c[0])), ry = round4(Math.abs(q[1] - c[1]))
            if (rx < 1e-6 || ry < 1e-6) return
            const idx = addEntity(s => s.addEllipse(c[0], c[1], rx, ry))
            if (idx >= 0) snapConstraint(idx, "center", ptSnaps[0])
            pts = []; ptSnaps = []
            break
        }
        case "arc": {
            if (n < 2) { push(); break }
            const a = pts[0], b = pts[1], sa = ptSnaps[0], sb = ptSnaps[1]
            const idx = addEntity(s => s.addArcThroughPoints(a[0], a[1], q[0], q[1], b[0], b[1]))
            if (idx >= 0) {
                const s = app.activeSketch
                const ra = s.roleAt(idx, a[0], a[1]), rb = s.roleAt(idx, b[0], b[1])
                if (ra) snapConstraint(idx, ra, sa)
                if (rb) snapConstraint(idx, rb, sb)
            }
            pts = []; ptSnaps = []
            break
        }
        case "slot": {
            if (n < 2) { push(); break }
            const a = pts[0], b = pts[1]
            const L = dist(a, b)
            if (L < 1e-9) { pts = []; ptSnaps = []; break }
            const d = Math.abs((b[0] - a[0]) * (a[1] - q[1]) - (a[0] - q[0]) * (b[1] - a[1])) / L
            const w = round4(2 * d)
            if (w < 1e-6) return
            const idx = addEntity(s => s.addSlot(a[0], a[1], b[0], b[1], w))
            if (idx >= 0) { snapConstraint(idx, "start", ptSnaps[0]); snapConstraint(idx, "end", ptSnaps[1]) }
            pts = []; ptSnaps = []
            break
        }
        case "spline":
            push()
            break
        }
        dimLocks = []
        dimTexts = []
        preview.requestPaint()
    }

    // ------------------------------------------------------------------ sketch modify tools

    function curveAt(x, y) {
        const ref = r.pick(x, y, "sketchcurve")
        return ref.kind === "sketchcurve" && ref.sketch === app.activeSketchId ? ref.entity : -1
    }
    function highlightCurves(list) {
        app.selection.setItems(list.map(e => ({ kind: "sketchcurve", sketch: app.activeSketchId, entity: e })))
    }
    // Runs fn() as one undo step; a false result rolls it back and shows the message
    function sketchEdit(fn, failure) {
        app.doc.checkpoint()
        const res = fn()
        if (res === false || res === 0 || (res && res.ok === false) || (Array.isArray(res) && !res.length)) {
            app.doc.undo()
            fissionApp.showMessage(res && res.error ? res.error : failure)
            return false
        }
        if (res && res.warning) fissionApp.showMessage(res.warning)
        return true
    }

    function modifyClick(x, y) {
        const s = app.activeSketch
        if (!s) return
        const tool = app.sketchTool
        if (tool === "project") {
            const ref = r.pick(x, y, "edge,face")
            if (ref.kind !== "edge" && ref.kind !== "face") { fissionApp.showMessage("Select a body edge or face to project"); return }
            sketchEdit(() => s.projectRef(ref), "That geometry can't be projected onto the sketch")
            return
        }
        const p = r.sketchPoint(x, y, app.activeSketchId, false)
        if (p.u === undefined) return
        const e = curveAt(x, y)
        switch (tool) {
        case "trim": case "extend":
            if (e < 0) { fissionApp.showMessage("Select the part of a curve to " + tool); return }
            sketchEdit(() => tool === "trim" ? s.trimAt(e, p.u, p.v) : s.extendAt(e, p.u, p.v),
                       tool === "trim" ? "That curve can't be trimmed" : "Nothing to extend to")
            return
        case "offset": {
            if (!mpicks.length) {
                if (e < 0) { fissionApp.showMessage("Select a curve or loop to offset"); return }
                const chain = s.chainOf(e)
                mpicks = chain.length ? chain : [e]
                highlightCurves(mpicks)
                return
            }
            const d = app.doc.evaluateExpression(app.sketchOffsetDistance)
            if (d.error || Math.abs(d.value) < 1e-9) { fissionApp.showMessage("Offset distance: " + (d.error || "must not be zero")); return }
            const chain = mpicks
            mpicks = []
            app.selection.clear()
            sketchEdit(() => s.offsetEntities(chain, Math.abs(d.value), p.u, p.v), "That geometry can't be offset by " + app.sketchOffsetDistance)
            return
        }
        case "fillet": {
            if (e < 0 || s.entity(e).type !== "line") { fissionApp.showMessage("Select two lines that meet at a corner"); return }
            if (!mpicks.length) { mpicks = [e]; highlightCurves(mpicks); return }
            if (mpicks[0] === e) return
            const first = mpicks[0]
            mpicks = []
            app.selection.clear()
            sketchEdit(() => s.filletCorner(first, e, app.sketchFilletRadius), "Those lines can't be filleted")
            return
        }
        }
    }

    property bool trimPaintStroke: false

    function paintTrimDelete(x, y) {
        const s = app.activeSketch
        if (!s) return
        const e = curveAt(x, y)
        if (e < 0) return
        if (!trimPaintStroke) {
            app.doc.checkpoint()
            trimPaintStroke = true
        }
        s.removeEntity(e)
        app.rev++
        preview.requestPaint()
    }

    function modifyHint() {
        switch (app.sketchTool) {
        case "trim": return "Click a curve piece to trim, or drag to paint-delete curves"
        case "extend": return "Click near the end of a line or arc to extend it"
        case "offset": return mpicks.length ? "Click the side to offset to (" + app.sketchOffsetDistance + ")" : "Select a curve or loop to offset"
        case "fillet": return mpicks.length ? "Select the second line" : "Select the first line of the corner (R " + app.sketchFilletRadius + ")"
        case "project": return "Select body edges or faces to project onto the sketch"
        }
        return ""
    }

    // ------------------------------------------------------------------ drag sketch geometry / dimension labels

    function beginSketchPress(x, y) {
        if (!app.activeSketchId || app.sketchTool || sketchInputActive) return false
        const s = app.activeSketch
        if (!s) return false
        const hit = annotationAt(x, y)
        if (hit) {
            if (!hit.dim || !hit.anno) return false
            sdrag = { kind: "dim", index: hit.index, a: hit.anno, moved: false }
            return true
        }
        const fit = splinePointAt(x, y)
        if (fit) {
            sdrag = { kind: "spline", e: fit.e, k: fit.k, moved: false }
            return true
        }
        const sp = r.sketchPoint(x, y, app.activeSketchId, true)
        if (sp.u === undefined) return false
        if (sp.entity >= 0 && sp.role && sp.role !== "mid") {
            sdrag = { kind: "point", e: sp.entity, p: sp.role, moved: false }
            return true
        }
        const e = curveAt(x, y)
        if (e < 0) return false
        const free = r.sketchPoint(x, y, app.activeSketchId, false)
        sdrag = { kind: "curve", e: e, ent: s.entity(e), start: [free.u, free.v],
                  handles: s.handles().filter(h => h.entity === e && h.role !== "mid"), moved: false }
        return true
    }

    function moveSketchDrag(x, y) {
        const s = app.activeSketch
        const p = r.sketchPoint(x, y, app.activeSketchId, false)
        if (!s || !sdrag || p.u === undefined) return
        if (!sdrag.moved) { app.doc.checkpoint(); sdrag.moved = true }
        const d = sdrag
        if (d.kind === "dim") { s.setDimensionOffset(d.index, dimOffsetAt(d.a, [p.u, p.v])); return }
        if (d.kind === "spline") {
            const ent = s.entity(d.e)
            const list = (ent.points || []).slice()
            if (d.k >= list.length) return
            const sp = r.sketchPoint(x, y, app.activeSketchId, true)
            list[d.k] = [round4(sp.u), round4(sp.v)]
            ent.points = list
            s.setEntity(d.e, ent)
            return
        }
        if (d.kind === "point") { s.dragTo([{ e: d.e, p: d.p, u: p.u, v: p.v }]); return }
        const du = p.u - d.start[0], dv = p.v - d.start[1]
        const shifted = d.handles.map(h => ({ e: d.e, p: h.role, u: h.u + du, v: h.v + dv }))
        const t = d.ent.type
        if (t === "circle" || t === "arc") {
            const c = d.handles.find(h => h.role === "center")
            const ring = { e: d.e, u: p.u, v: p.v }
            if (c && s.dragTo([ring, { e: d.e, p: "center", u: c.u, v: c.v }])) return
            s.dragTo([ring])
            return
        }
        if (shifted.length && s.dragTo(shifted)) return
        if (t === "line" && shifted.length === 2) {
            // Translation is blocked: move the end nearest to where the line was grabbed
            const near = Math.hypot(d.start[0] - d.handles[0].u, d.start[1] - d.handles[0].v)
                       <= Math.hypot(d.start[0] - d.handles[1].u, d.start[1] - d.handles[1].v) ? 0 : 1
            s.dragTo([shifted[near]])
        }
    }

    function endSketchDrag() { sdrag = null }

    // Fit points of the active sketch's splines: [{e, k, u, v}]
    function splineFitPoints() {
        const s = app.activeSketch
        const out = []
        if (!s) return out
        for (let e = 0; e < s.entityCount; ++e) {
            const ent = s.entity(e)
            if (ent.type !== "spline" || ent.projected) continue
            const list = ent.points || []
            for (let k = 0; k < list.length; ++k) {
                const u = parseFloat(list[k][0]), v = parseFloat(list[k][1])
                if (isFinite(u) && isFinite(v)) out.push({ e: e, k: k, u: u, v: v })
            }
        }
        return out
    }
    function splinePointAt(x, y) {
        const fits = splineFitPoints()
        if (!fits.length) return null
        const sp = r.projectSketch(app.activeSketchId, fits.map(f => [f.u, f.v]))
        let best = null, bestD = 8
        for (let i = 0; i < sp.length && i < fits.length; ++i) {
            const d = Math.hypot(sp[i][0] - x, sp[i][1] - y)
            if (d < bestD) { bestD = d; best = fits[i] }
        }
        return best
    }

    // Dimension offset for a label dragged to sketch point c: linear = perpendicular distance,
    // radial = label angle (degrees), angular = arc radius
    function dimOffsetAt(a, c) {
        if (a.geom === "radial") return Math.atan2(c[1] - a.center[1], c[0] - a.center[0]) * 180 / Math.PI
        if (a.geom === "angular") return Math.max(1e-3, Math.hypot(c[0] - a.vertex[0], c[1] - a.vertex[1]))
        const p1 = a.p1, p2 = a.p2
        if (a.type !== "pointline" && a.orient === "horizontal") return c[1] - Math.max(p1[1], p2[1])
        if (a.type !== "pointline" && a.orient === "vertical") return c[0] - Math.max(p1[0], p2[0])
        const L = Math.hypot(p2[0] - p1[0], p2[1] - p1[1])
        const n = L > 1e-9 ? [-(p2[1] - p1[1]) / L, (p2[0] - p1[0]) / L] : [0, 1]
        return (c[0] - p1[0]) * n[0] + (c[1] - p1[1]) * n[1]
    }

    // ------------------------------------------------------------------ constraint and dimension tools

    // Item sequences each tool accepts. "place" is a click on empty space (dimension placement).
    readonly property var constraintSignatures: ({
        horizontal: [["line"], ["point", "point"]],
        vertical: [["line"], ["point", "point"]],
        coincident: [["point", "point"], ["point", "line"], ["point", "circle"], ["line", "point"], ["circle", "point"]],
        parallel: [["line", "line"]],
        perpendicular: [["line", "line"]],
        collinear: [["line", "line"]],
        tangent: [["line", "circle"], ["circle", "line"], ["circle", "circle"]],
        equal: [["line", "line"], ["circle", "circle"]],
        concentric: [["circle", "circle"]],
        midpoint: [["point", "line"], ["line", "point"]],
        symmetric: [["point", "point", "line"]],
        fix: [["point"], ["line"], ["circle"], ["other"]],
        dimension: [["circle"], ["line", "place"], ["line", "line"], ["line", "point"], ["point", "line"],
                    ["point", "point", "place"]]
    })
    readonly property string constraintType: app.sketchTool === "dimension" ? "dimension"
                                           : app.sketchTool.startsWith("constraint:") ? app.sketchTool.slice(11) : ""

    function entityKind(e) {
        const t = ((app.activeSketch && app.activeSketch.entity(e)) || {}).type
        return t === "line" ? "line" : (t === "circle" || t === "arc") ? "circle" : "other"
    }
    function pickKind(p) { return p.place ? "place" : p.p !== undefined ? "point" : entityKind(p.e) }

    // Candidate readings of a click: the snapped point first, then the curve under the cursor
    function constraintCandidates(x, y) {
        const out = []
        const sp = r.sketchPoint(x, y, app.activeSketchId, true)
        if (sp.u === undefined) return out
        if (sp.entity !== undefined && sp.role !== "mid") out.push({ e: sp.entity, p: sp.role, u: sp.u, v: sp.v })
        const ref = r.pick(x, y, "sketchcurve")
        if (ref.kind === "sketchcurve" && ref.sketch === app.activeSketchId) out.push({ e: ref.entity, u: sp.u, v: sp.v })
        else if (sp.entity >= 0) out.push({ e: sp.entity, u: sp.u, v: sp.v })
        if (!out.length) out.push({ place: true, u: sp.u, v: sp.v })
        return out
    }

    function signatureState(type, kinds) {
        let prefix = false, exact = false
        for (const sig of constraintSignatures[type] || []) {
            if (kinds.length > sig.length) continue
            let ok = true
            for (let i = 0; i < kinds.length; ++i) if (sig[i] !== kinds[i]) { ok = false; break }
            if (!ok) continue
            prefix = true
            if (sig.length === kinds.length) exact = true
        }
        return { prefix: prefix, exact: exact }
    }

    function constraintClick(x, y) {
        const type = constraintType
        const kinds = cpicks.map(pickKind)
        for (const cand of constraintCandidates(x, y)) {
            const k = pickKind(cand)
            if (!cand.place && cpicks.some(p => !p.place && p.e === cand.e && p.p === cand.p)) continue
            const st = signatureState(type, kinds.concat([k]))
            if (!st.prefix) continue
            const picks = cpicks.concat([cand])
            if (st.exact) { cpicks = []; createConstraint(type, picks) }
            else cpicks = picks
            preview.requestPaint()
            return
        }
        if (cpicks.length === 0) fissionApp.showMessage("That can't be used here \u2014 " + constraintHint())
    }

    function lineDir(e) {
        const m = app.activeSketch.entity(e)
        return [parseFloat(m.x2) - parseFloat(m.x1), parseFloat(m.y2) - parseFloat(m.y1)]
    }

    function buildConstraint(type, picks) {
        const ref = p => p.p !== undefined ? { e: p.e, p: p.p } : { e: p.e }
        const kinds = picks.map(pickKind).join(",")
        if (type === "fix") return { type: "fix", a: { e: picks[0].e } }
        if (type !== "dimension") {
            const c = { type: type, a: ref(picks[0]) }
            if (picks.length > 1) c.b = ref(picks[1])
            if (picks.length > 2) c.c = ref(picks[2])
            return c
        }
        switch (kinds) {
        case "circle":
            return { type: app.activeSketch.entity(picks[0].e).type === "arc" ? "radius" : "diameter", a: ref(picks[0]) }
        case "line,place":
            return { type: "distance", a: { e: picks[0].e, p: "start" }, b: { e: picks[0].e, p: "end" } }
        case "line,point": case "point,line":
            return { type: "pointline", a: ref(picks[kinds === "line,point" ? 1 : 0]), b: ref(picks[kinds === "line,point" ? 0 : 1]) }
        case "line,line": {
            const d1 = lineDir(picks[0].e), d2 = lineDir(picks[1].e)
            const cross = d1[0] * d2[1] - d1[1] * d2[0], n = Math.hypot(d1[0], d1[1]) * Math.hypot(d2[0], d2[1])
            if (n > 0 && Math.abs(cross) / n < 1e-6)   // parallel lines: their distance
                return { type: "pointline", a: { e: picks[1].e, p: "start" }, b: { e: picks[0].e } }
            return { type: "angle", a: ref(picks[0]), b: ref(picks[1]) }
        }
        case "point,point,place": {
            // Placing beside the points measures vertically, above / below horizontally
            const p = picks[0], q = picks[1], pl = picks[2]
            const inU = pl.u >= Math.min(p.u, q.u) && pl.u <= Math.max(p.u, q.u)
            const inV = pl.v >= Math.min(p.v, q.v) && pl.v <= Math.max(p.v, q.v)
            const orient = !inU && inV ? "vertical" : inU && !inV ? "horizontal" : "aligned"
            return { type: "distance", orient: orient, a: ref(p), b: ref(q) }
        }
        }
        return null
    }

    function createConstraint(type, picks) {
        const s = app.activeSketch
        const c = buildConstraint(type, picks)
        if (!s || !c) return
        if (type === "fix") {
            // Fix toggles: picking fixed geometry again releases it
            const list = s.props.constraints || []
            for (let i = 0; i < list.length; ++i)
                if (list[i].type === "fix" && list[i].a.e === c.a.e) {
                    app.doc.checkpoint()
                    s.removeConstraint(i)
                    return
                }
        }
        app.doc.checkpoint()
        const res = s.addConstraint(c)
        if (!res.ok) { app.doc.undo(); fissionApp.showMessage(res.error); return }
        if (type === "dimension") editingConstraint = res.index
    }

    function constraintHint() {
        const type = constraintType, n = cpicks.length
        const kinds = cpicks.map(pickKind).join(",")
        if (type === "dimension") {
            if (n === 0) return "Select a line, circle, arc, or two points to dimension"
            if (kinds === "line") return "Click to place the length, or select a second line (angle) or a point (distance)"
            if (kinds === "point") return "Select a second point or a line"
            return "Click to place: above / below = horizontal, beside = vertical"
        }
        const what = {
            horizontal: "a line or two points", vertical: "a line or two points",
            coincident: "two points, or a point and a curve", parallel: "two lines", perpendicular: "two lines",
            collinear: "two lines", tangent: "a line and a circle / arc, or two circles / arcs",
            equal: "two lines or two circles / arcs", concentric: "two circles or arcs",
            midpoint: "a point and a line", symmetric: "two points, then the symmetry line",
            fix: "geometry to fix or release"
        }[type] || ""
        return n === 0 ? "Select " + what : "Select the next item (" + what + ")"
    }

    // Annotation (dimension label or constraint badge) at a view position
    function annotationAt(x, y) {
        for (let i = annoHits.length - 1; i >= 0; --i) {
            const h = annoHits[i]
            if (x >= h.x && x <= h.x + h.w && y >= h.y && y <= h.y + h.h) return h
        }
        return null
    }
    function deleteSelectedConstraint() {
        const s = app.activeSketch
        if (!s || selectedConstraint < 0) return false
        app.doc.checkpoint()
        s.removeConstraint(selectedConstraint)
        selectedConstraint = -1
        editingConstraint = -1
        return true
    }

    // Rubber-band geometry in sketch coordinates
    function circlePts(c, rad, nseg) {
        const out = []
        for (let i = 0; i <= nseg; ++i) {
            const a = i / nseg * Math.PI * 2
            out.push([c[0] + rad * Math.cos(a), c[1] + rad * Math.sin(a)])
        }
        return out
    }
    function circumcircle(a, b, c) {
        const d = 2 * (a[0] * (b[1] - c[1]) + b[0] * (c[1] - a[1]) + c[0] * (a[1] - b[1]))
        if (Math.abs(d) < 1e-12) return null
        const a2 = a[0] * a[0] + a[1] * a[1], b2 = b[0] * b[0] + b[1] * b[1], c2 = c[0] * c[0] + c[1] * c[1]
        const ux = (a2 * (b[1] - c[1]) + b2 * (c[1] - a[1]) + c2 * (a[1] - b[1])) / d
        const uy = (a2 * (c[0] - b[0]) + b2 * (a[0] - c[0]) + c2 * (b[0] - a[0])) / d
        return { c: [ux, uy], r: Math.hypot(a[0] - ux, a[1] - uy) }
    }
    function arcPts(a, m, b) {
        const cc = circumcircle(a, m, b)
        if (!cc) return [a, b]
        const ang = p => Math.atan2(p[1] - cc.c[1], p[0] - cc.c[0])
        const a0 = ang(a), am = ang(m), a1 = ang(b)
        const norm = x => { while (x < 0) x += Math.PI * 2; return x }
        let sweep = norm(a1 - a0)
        if (norm(am - a0) > sweep) sweep -= Math.PI * 2
        const out = []
        for (let i = 0; i <= 48; ++i) {
            const t = a0 + sweep * i / 48
            out.push([cc.c[0] + cc.r * Math.cos(t), cc.c[1] + cc.r * Math.sin(t)])
        }
        return out
    }

    function previewCurves() {
        if (!cursor) return { curves: [], label: "" }
        const c = constrained([cursor.u, cursor.v])
        const n = pts.length
        const out = []
        let label = ""
        const f = v => app.lenValue(v).toString()
        const u = " " + app.lengthUnit
        switch (app.sketchTool) {
        case "line":
            if (n) {
                const a = pts[n - 1]
                out.push([a, c])
                const ang = Math.round(Math.atan2(c[1] - a[1], c[0] - a[0]) * 1800 / Math.PI) / 10
                const snapped = vp.lineDirection(a, [cursor.u, cursor.v], vp.cursorOnPoint).snapped
                label = f(dist(a, c)) + u + "  " + ang + "\u00b0"
                        + (snapped ? (Math.abs(c[1] - a[1]) < 1e-9 ? "  \u2014 horizontal" : "  \u2014 vertical") : "")
            }
            break
        case "rectangle":
            if (n) {
                const a = pts[0]
                out.push([a, [c[0], a[1]], c, [a[0], c[1]], a])
                label = f(Math.abs(c[0] - a[0])) + " \u00d7 " + f(Math.abs(c[1] - a[1])) + u
            }
            break
        case "centerrectangle":
            if (n) {
                const m = pts[0], a = [2 * m[0] - c[0], 2 * m[1] - c[1]]
                out.push([a, [c[0], a[1]], c, [a[0], c[1]], a])
                out.push([a, c])
                label = f(2 * Math.abs(c[0] - m[0])) + " \u00d7 " + f(2 * Math.abs(c[1] - m[1])) + u
            }
            break
        case "centerline":
            if (n) {
                const m = pts[0]
                out.push([[2 * m[0] - c[0], 2 * m[1] - c[1]], c])
                const ang = Math.round(Math.atan2(c[1] - m[1], c[0] - m[0]) * 1800 / Math.PI) / 10
                label = f(2 * dist(m, c)) + u + "  " + ang + "\u00b0"
            }
            break
        case "circle":
            if (n) { out.push(circlePts(pts[0], dist(pts[0], c), 72)); label = "\u2300 " + f(2 * dist(pts[0], c)) }
            break
        case "polygon":
            if (n) {
                const rad = dist(pts[0], c), poly = []
                for (let i = 0; i <= app.polygonSides; ++i) {
                    const a = i / app.polygonSides * Math.PI * 2
                    poly.push([pts[0][0] + rad * Math.cos(a), pts[0][1] + rad * Math.sin(a)])
                }
                out.push(poly)
                label = "R " + f(rad)
            }
            break
        case "ellipse":
            if (n) {
                const rx = Math.abs(c[0] - pts[0][0]), ry = Math.abs(c[1] - pts[0][1]), e = []
                for (let i = 0; i <= 72; ++i) {
                    const a = i / 72 * Math.PI * 2
                    e.push([pts[0][0] + rx * Math.cos(a), pts[0][1] + ry * Math.sin(a)])
                }
                out.push(e)
                label = f(2 * rx) + " \u00d7 " + f(2 * ry)
            }
            break
        case "arc":
            if (n === 1) out.push([pts[0], c])
            else if (n === 2) out.push(arcPts(pts[0], c, pts[1]))
            break
        case "slot":
            if (n === 1) { out.push([pts[0], c]); label = f(dist(pts[0], c)) + u }
            else if (n === 2) {
                const a = pts[0], b = pts[1], L = dist(a, b)
                if (L > 1e-9) {
                    const nx = -(b[1] - a[1]) / L, ny = (b[0] - a[0]) / L
                    const d = Math.abs((b[0] - a[0]) * (a[1] - c[1]) - (a[0] - c[0]) * (b[1] - a[1])) / L
                    out.push([[a[0] + nx * d, a[1] + ny * d], [b[0] + nx * d, b[1] + ny * d]])
                    out.push([[a[0] - nx * d, a[1] - ny * d], [b[0] - nx * d, b[1] - ny * d]])
                    out.push(circlePts(a, d, 48))
                    out.push(circlePts(b, d, 48))
                    label = "W " + f(2 * d)
                }
            }
            break
        case "spline":
            if (n) out.push(pts.concat([c]))
            break
        }
        return { curves: out, label: label }
    }

    function sketchHint() {
        const n = pts.length
        if (constraintTool) return constraintHint()
        if (modifyTool) return modifyHint()
        switch (app.sketchTool) {
        case "line": return n ? "Click the next point \u2014 double-click or Esc to stop" : "Click the start point"
        case "rectangle": return n ? "Click the opposite corner" : "Click the first corner"
        case "centerrectangle": return n ? "Click a corner" : "Click the center"
        case "centerline": return n ? "Click an end (the line runs the same length the other way)" : "Click the midpoint"
        case "circle": return n ? "Click to set the radius" : "Click the center"
        case "polygon": return n ? "Click a vertex (" + app.polygonSides + " sides)" : "Click the center"
        case "ellipse": return n ? "Click a corner of the bounding box" : "Click the center"
        case "arc": return n === 0 ? "Click the start point" : n === 1 ? "Click the end point" : "Click a point on the arc"
        case "slot": return n === 0 ? "Click the first center" : n === 1 ? "Click the second center" : "Click to set the width"
        case "spline": return n ? "Click fit points \u2014 double-click or Enter to finish" : "Click the first fit point"
        case "point": return "Click to place a point"
        }
        return ""
    }

    Shortcut {
        sequences: ["Return", "Enter"]
        enabled: vp.sketchInputActive
        onActivated: vp.enterPressed()
    }
    // Typing a number while drawing goes into the dimension box
    Keys.onPressed: (e) => {
        if (!dimBox.visible) return
        if (e.key === Qt.Key_Tab) { dimBox.focusField(0, ""); e.accepted = true; return }
        if (e.text.length === 1 && "0123456789.(".indexOf(e.text) >= 0) {
            dimBox.focusField(0, e.text)
            e.accepted = true
        }
    }
    Connections {
        target: vp.app
        function onSketchToolChanged() { vp.cancelSketchInput() }
        function onLengthUnitChanged() { annoCanvas.requestPaint(); preview.requestPaint() }
        function onActiveSketchIdChanged() {
            vp.cancelSketchInput()
            vp.selectedConstraint = -1
            vp.editingConstraint = -1
            annoCanvas.requestPaint()
        }
    }
    Connections {
        target: vp.app.camera
        function onChanged() {
            vp.camRev++
            if (vp.app.activeSketchId) { preview.requestPaint(); annoCanvas.requestPaint() }
            if ((vp.manip || vp.sectionManip) && manipCanvas) manipCanvas.requestPaint()
            if (triad.gizmo) triad.requestPaint()
            if (vp.app.pointPick) pointCanvas.requestPaint()
        }
    }
    Connections {
        target: vp.app
        function onRevChanged() { if (triad.gizmo) triad.requestPaint() }
    }
    property int camRev: 0
    onWidthChanged: { manipCanvas.requestPaint(); annoCanvas.requestPaint() }
    onHeightChanged: { manipCanvas.requestPaint(); annoCanvas.requestPaint() }
    onManipHoverChanged: manipCanvas.requestPaint()
    onManipDragChanged: manipCanvas.requestPaint()

    // ------------------------------------------------------------------ feature drag handle

    // {origin, direction, key, value} reported by the feature being edited (Extrude, Fillet, Chamfer)
    readonly property var manip: (app.rev, app.commandFeature && !app.activeSketchId
                                  && app.commandFeature.manipulator.key !== undefined)
                                 ? app.commandFeature.manipulator : null
    property var manipDrag: null   // {origin, direction, key, offset} while dragging
    property bool manipHover: false
    onManipChanged: {
        // A focused value box would keep swallowing the single-key shortcuts after the command ends
        if (!manip && manipField.activeFocus) vp.forceActiveFocus()
        if (manipCanvas) manipCanvas.requestPaint()
    }
    onSectionManipChanged: if (manipCanvas) manipCanvas.requestPaint()
    // manipCanvas is a child id, not vp.manipCanvas — guard until the Canvas exists
    function repaintManip() { if (manipCanvas) manipCanvas.requestPaint() }
    Connections {
        target: app
        function onSectionOffsetChanged() { vp.repaintManip() }
        function onSectionFlipChanged() { vp.repaintManip() }
        function onSectionEnabledChanged() { vp.repaintManip() }
        function onInspectModeChanged() { vp.repaintManip() }
    }

    // Screen geometry of the handle: base, tip and a unit screen direction for the arrow head
    function manipScreen() {
        if (!manip) return null
        const tipW = manip.origin.plus(manip.direction.times(manip.value))
        const o = r.projectPoint(manip.origin), t = r.projectPoint(tipW)
        const ahead = r.projectPoint(tipW.plus(manip.direction.times(r.snapStep() * 4)))
        if (!o.length || !t.length || !ahead.length) return null
        let dx = ahead[0] - t[0], dy = ahead[1] - t[1]
        const len = Math.hypot(dx, dy)
        if (len < 1e-6) { dx = 0; dy = -1 } else { dx /= len; dy /= len }
        if (manip.value < 0) { dx = -dx; dy = -dy }
        return { o: o, t: t, d: [dx, dy], handle: [t[0] + dx * 10, t[1] + dy * 10] }
    }
    // Place the distance value box so it never covers the arrow head or the taper knob
    function manipBoxPos(bw, bh) {
        const s = manipScreen()
        if (!s) return { x: 4, y: 4 }
        const as = angleScreen()
        function circleHits(cx, cy, rad, bx, by) {
            const nx = Math.max(bx, Math.min(cx, bx + bw))
            const ny = Math.max(by, Math.min(cy, by + bh))
            return Math.hypot(cx - nx, cy - ny) < rad
        }
        function rectHits(ax, ay, aw, ah, bx, by) {
            return bx < ax + aw && bx + bw > ax && by < ay + ah && by + bh > ay
        }
        function ok(bx, by) {
            bx = Math.max(4, Math.min(bx, vp.width - bw - 4))
            by = Math.max(4, Math.min(by, vp.height - bh - 4))
            if (circleHits(s.handle[0], s.handle[1], 18, bx, by)) return null
            if (circleHits(s.t[0], s.t[1], 16, bx, by)) return null
            for (let u = 0; u <= 1.001; u += 0.2) {
                const sx = s.o[0] + (s.t[0] - s.o[0]) * u
                const sy = s.o[1] + (s.t[1] - s.o[1]) * u
                if (circleHits(sx, sy, 10, bx, by)) return null
            }
            if (as) {
                if (circleHits(as.q[0], as.q[1], 16, bx, by)) return null
                if (rectHits(as.q[0] + 10, as.q[1] - 10, 56, 20, bx, by)) return null
            }
            return { x: bx, y: by }
        }
        const dx = s.d[0], dy = s.d[1], px = -dy, py = dx
        const hx = s.handle[0], hy = s.handle[1]
        const dirs = [
            [px, py], [-px, -py], [dx, dy], [-dx, -dy],
            [(px + dx) * 0.707, (py + dy) * 0.707], [(-px + dx) * 0.707, (-py + dy) * 0.707],
            [(px - dx) * 0.707, (py - dy) * 0.707], [(-px - dx) * 0.707, (-py - dy) * 0.707]
        ]
        for (let g = 34; g <= 90; g += 14) {
            for (let i = 0; i < dirs.length; ++i) {
                const r = ok(hx + dirs[i][0] * g - bw / 2, hy + dirs[i][1] * g - bh / 2)
                if (r) return r
            }
        }
        return ok(hx + 48, hy - bh / 2) || { x: 4, y: 4 }
    }
    function nearManip(x, y) {
        // Draft: only the angle knob is interactive (the pull arrow is a guide)
        if (app.commandFeature && app.commandFeature.type === "Draft") return false
        const s = manipScreen()
        return !!s && Math.hypot(x - s.handle[0], y - s.handle[1]) < 14
    }
    function beginManipDrag(x, y) {
        const t = r.axisParameter(x, y, manip.origin, manip.direction)
        if (t === undefined) return false
        manipDrag = { origin: manip.origin, direction: manip.direction, key: manip.key, offset: manip.value - t }
        return true
    }

    // Section Analysis offset handle (independent of commandFeature)
    readonly property var sectionManip: {
        const a = app
        if (!a || a.inspectMode !== "section" || !a.sectionEnabled || a.sectionAwaitingPlane) return null
        const f = a.sectionFrame
        if (!f || !f.ok) return null
        let n = Qt.vector3d(f.normal[0], f.normal[1], f.normal[2])
        if (a.sectionFlip) n = n.times(-1)
        return {
            origin: Qt.vector3d(f.origin[0], f.origin[1], f.origin[2]),
            direction: n,
            value: a.sectionFlip ? -a.sectionOffset : a.sectionOffset
        }
    }
    function sectionScreen() {
        const m = sectionManip
        if (!m) return null
        const tipW = m.origin.plus(m.direction.times(m.value))
        const o = r.projectPoint(m.origin), t = r.projectPoint(tipW)
        const ahead = r.projectPoint(tipW.plus(m.direction.times(r.snapStep() * 4)))
        if (!o.length || !t.length || !ahead.length) return null
        let dx = ahead[0] - t[0], dy = ahead[1] - t[1]
        const len = Math.hypot(dx, dy)
        if (len < 1e-6) { dx = 0; dy = -1 } else { dx /= len; dy /= len }
        if (m.value < 0) { dx = -dx; dy = -dy }
        return { o: o, t: t, d: [dx, dy], handle: [t[0] + dx * 10, t[1] + dy * 10] }
    }
    function nearSection(x, y) {
        const s = sectionScreen()
        return !!s && Math.hypot(x - s.handle[0], y - s.handle[1]) < 14
    }
    function beginSectionDrag(x, y) {
        const m = sectionManip
        if (!m) return false
        const t = r.axisParameter(x, y, m.origin, m.direction)
        if (t === undefined) return false
        manipDrag = { section: true, origin: m.origin, direction: m.direction, offset: m.value - t }
        return true
    }
    // ------------------------------------------------------------------ region selection
    // Left-drag from empty space, with the tool picked by 1 / 2 / 3 (app.selectionTool):
    // window = box (left to right: items fully inside; right to left: items it touches),
    // freeform = lasso (items fully inside), paint = everything the cursor passes over.
    property var regionPath: []     // [[x, y], ...] screen points of the drag
    property bool regionAdditive: false

    function canRegionSelect() {
        return !app.pickingSketchPlane && !(app.activeSketchId && app.sketchTool)
    }
    function regionFilter() {
        if (app.activeSketchId) return "sketchcurve"
        if (app.commandFeatureId) return ""
        return "body,plane,axis,point"
    }
    function regionCrossing() {
        return app.selectionTool === "window" && regionPath.length > 1
               && regionPath[regionPath.length - 1][0] < regionPath[0][0]
    }
    function inActiveSketch(refs) {
        return app.activeSketchId ? refs.filter(x => x.kind !== "sketchcurve" || x.sketch === app.activeSketchId) : refs
    }
    function beginRegion(x0, y0, mods) {
        regionAdditive = !!(mods & (Qt.ControlModifier | Qt.ShiftModifier)) || app.commandFeatureId.length > 0
        regionPath = [[x0, y0]]
        r.clearHover()
        if (app.selectionTool === "paint" && !regionAdditive) app.selection.clear()
        paintAt(x0, y0)
    }
    function paintAt(x, y) {
        if (app.selectionTool !== "paint") return
        const hit = inActiveSketch([r.pick(x, y, regionFilter())]).filter(h => h.kind !== undefined)
        if (hit.length && !app.selection.contains(hit[0])) app.selection.select(hit[0], true)
    }
    function moveRegion(x, y) {
        const p = regionPath.slice()
        const lastP = p[p.length - 1]
        if (Math.hypot(x - lastP[0], y - lastP[1]) < 3) return
        if (app.selectionTool === "window") p.splice(1, p.length - 1, [x, y])
        else p.push([x, y])
        regionPath = p
        paintAt(x, y)
        regionCanvas.requestPaint()
    }
    function endRegion() {
        const path = regionPath
        regionPath = []
        regionCanvas.requestPaint()
        if (app.selectionTool === "paint" || path.length < 2) return
        let poly = path.map(p => [Number(p[0]), Number(p[1])])
        if (app.selectionTool === "window") {
            const a = path[0], b = path[path.length - 1]
            poly = [[a[0], a[1]], [b[0], a[1]], [b[0], b[1]], [a[0], b[1]]]
        }
        const crossing = app.selectionTool === "window" && path[path.length - 1][0] < path[0][0]
        const refs = inActiveSketch(r.pickRegion(poly, crossing, regionFilter()))
        if (!regionAdditive) app.selection.setItems(refs)
        else app.selection.setItems(Array.from(app.selection.items).concat(refs.filter(x => !app.selection.contains(x))))
        fissionApp.showMessage(refs.length + " item(s) selected")
    }

    // Construction plane resize: press within 6 px of a visible plane's border and drag that side
    property var planeDrag: null
    function planeEdgeAt(x, y) {
        if (app.activeSketchId || app.commandFeature) return null
        let best = null, bestD = 6
        for (const e of app.doc.featureList("ConstructionPlane")) {
            const f = app.doc.featureById(e.id)
            if (!f || !f.visible) continue
            const info = f.displayInfo()
            if (!info.bounds) continue
            const b = info.bounds
            const at = (u, v) => r.projectPoint(info.origin.plus(info.x.times(u)).plus(info.y.times(v)))
            const c = [at(b[0], b[2]), at(b[1], b[2]), at(b[1], b[3]), at(b[0], b[3])]
            if (c.some(p => !p.length)) continue
            // side index into bounds: bottom edge = vmin(2), right = umax(1), top = vmax(3), left = umin(0)
            const sides = [[0, 1, 2], [1, 2, 1], [2, 3, 3], [3, 0, 0]]
            for (const [i, j, side] of sides) {
                const ax = c[i][0], ay = c[i][1], bx = c[j][0], by = c[j][1]
                const L2 = (bx - ax) * (bx - ax) + (by - ay) * (by - ay)
                const t = L2 > 0 ? Math.max(0, Math.min(1, ((x - ax) * (bx - ax) + (y - ay) * (by - ay)) / L2)) : 0
                const d = Math.hypot(x - ax - t * (bx - ax), y - ay - t * (by - ay))
                if (d < bestD) { bestD = d; best = { feature: f, info: info, side: side } }
            }
        }
        return best
    }
    function beginPlaneResize(x, y) {
        const hit = planeEdgeAt(x, y)
        if (!hit) return false
        planeDrag = { feature: hit.feature, info: hit.info, side: hit.side, bounds: hit.info.bounds.slice(), moved: false }
        return true
    }
    function movePlaneResize(x, y) {
        const d = planeDrag
        if (!d) return
        const dir = d.side < 2 ? d.info.x : d.info.y
        const t = r.axisParameter(x, y, d.info.origin, dir)
        if (t === undefined) return
        const b = d.bounds.slice()
        const lo = d.side % 2 === 0
        const other = b[lo ? d.side + 1 : d.side - 1]
        b[d.side] = lo ? Math.min(t, other - 1) : Math.max(t, other + 1)
        if (!d.moved) { app.doc.checkpoint(); d.moved = true }
        d.feature.set("display", b.map(v => Math.round(v * 100) / 100))
    }

    // Angle knob (Extrude taper): the side line from the profile edge to the knob at the far end
    function angleScreen() {
        if (!manip || !manip.angle) return null
        const a = manip.angle
        const t = Math.tan(a.value * Math.PI / 180)
        const top = a.pivot.plus(a.up.times(a.height)).minus(a.out.times(a.height * t))
        const p = r.projectPoint(a.pivot), q = r.projectPoint(top)
        if (!p.length || !q.length) return null
        return { p: p, q: q, value: a.value }
    }
    function nearAngle(x, y) {
        const s = angleScreen()
        return !!s && Math.hypot(x - s.q[0], y - s.q[1]) < 12
    }
    function beginAngleDrag() {
        if (!manip || !manip.angle || manip.angle.height < 1e-9) return false
        manipDrag = { angle: manip.angle }
        return true
    }

    // Point handle (Hole center): dragged across its face plane, snapping to the points the feature lists
    property var moveSnap: null      // {p: [x, y, z], label, s: [sx, sy]} while a drag sits on a snap point
    function moveScreen() {
        if (!manip || !manip.move) return null
        const p = r.projectPoint(manip.move.origin)
        return p.length ? p : null
    }
    function nearMove(x, y) {
        const p = moveScreen()
        return !!p && Math.hypot(x - p[0], y - p[1]) < 11
    }
    function beginMoveDrag() {
        if (!manip || !manip.move) return false
        manipDrag = { move: manip.move }
        return true
    }
    function moveTo(x, y) {
        const m = manipDrag.move
        moveSnap = null
        let best = 12
        for (const s of m.snaps || []) {
            const q = r.projectPoint(Qt.vector3d(s[0], s[1], s[2]))
            if (!q.length) continue
            const d = Math.hypot(x - q[0], y - q[1])
            if (d < best) { best = d; moveSnap = { p: [s[0], s[1], s[2]], label: s[3], s: q } }
        }
        let p = moveSnap ? moveSnap.p : null
        if (!p) {
            const w = r.planePoint(x, y, m.origin, m.normal)
            if (w === undefined) return
            const step = Math.max(0.01, r.snapStep() / 4)
            p = [w.x, w.y, w.z].map(v => Math.round(v / step) * step)
        }
        const unitDigits = app.lengthUnit === "mm" ? 3 : 4
        app.commandFeature.set(m.key, p.map(v => app.lenText(v, unitDigits)))
        manipCanvas.requestPaint()
    }

    function moveManipDrag(x, y) {
        if (!manipDrag) return
        // Section Analysis has no commandFeature — handle it before the feature-only path
        if (manipDrag.section) {
            const t = r.axisParameter(x, y, manipDrag.origin, manipDrag.direction)
            if (t === undefined) return
            const step = Math.max(0.01, r.snapStep() / 2)
            let v = Math.round((t + manipDrag.offset) / step) * step
            app.sectionOffset = app.sectionFlip ? -v : v
            if (manipCanvas) manipCanvas.requestPaint()
            return
        }
        const f = app.commandFeature
        if (!f) return
        if (manipDrag.move) { moveTo(x, y); return }
        if (manipDrag.angle) {
            const a = manipDrag.angle
            const t = r.axisParameter(x, y, a.pivot.plus(a.up.times(a.height)), a.out)
            if (t === undefined) return
            let deg = Math.round(Math.atan2(-t, a.height) * 180 / Math.PI * 2) / 2
            deg = Math.max(-60, Math.min(60, deg))
            f.set(a.key, deg + " deg")
            return
        }
        const t = r.axisParameter(x, y, manipDrag.origin, manipDrag.direction)
        if (t === undefined) return
        const step = Math.max(0.01, r.snapStep() / 2)
        let v = Math.round((t + manipDrag.offset) / step) * step
        if (manipDrag.key !== "distance" || f.type !== "Extrude") v = Math.max(step, v)
        // Like Fusion: dragging an extrude into the body turns Join into Cut, and back out again
        if (f.type === "Extrude" && manipDrag.key === "distance" && app.hasSolidBefore(f.id)) {
            const op = f.get("operation")
            if (v < 0 && op === "join") f.set("operation", "cut")
            else if (v > 0 && op === "cut") f.set("operation", "join")
        }
        f.set(manipDrag.key, app.lenText(v, app.lengthUnit === "mm" ? 3 : 4))
    }
    function endManipDrag() { manipDrag = null; moveSnap = null; manipCanvas.requestPaint() }

    // Move (free / translate): a press on one of the moved bodies; dragging it moves the bodies freely
    property var grabHit: null
    function bodyGrabAt(x, y) {
        const f = app.commandFeature
        if (!f || f.type !== "Move" || !triad.gizmo || triad.gizmo.visible === false) return null
        const ref = r.pick(x, y, "face")
        if (ref.kind !== "face" || !ref.hit) return null
        const bodies = (f.get("bodies") || []).map(Number)
        return bodies.indexOf(Number(ref.body)) >= 0 ? [ref.hit[0], ref.hit[1], ref.hit[2]] : null
    }

    // ------------------------------------------------------------------ mouse

    function click(x, y, mods) {
        if (app.activeSketchId && !pts.length && !cpicks.length) {
            const hit = annotationAt(x, y)
            selectedConstraint = hit ? hit.index : -1
            annoCanvas.requestPaint()
            if (hit) { app.selection.clear(); return }
        }
        if (app.activeSketchId && app.sketchTool) { sketchClick(x, y); return }
        const ref = r.pick(x, y)
        if (app.pickingSketchPlane) {
            const pl = app.planeRef(ref)
            if (pl) app.createSketch(pl)
            return
        }
        const measuring = app.inspectMode === "measure"
        const additive = (mods & (Qt.ControlModifier | Qt.ShiftModifier)) || app.commandFeatureId.length > 0
        if (ref.kind === undefined) {
            if (!additive) app.selection.clear()
            return
        }
        // Measure (I): first click shows the item; second adds distance / angle between them
        if (measuring && !additive) {
            const n = app.selection.count
            if (n === 0) app.selection.select(ref, false)
            else if (n === 1 && !app.selection.contains(ref)) app.selection.select(ref, true)
            else app.selection.select(ref, false)   // third pick starts a new pair
            return
        }
        if (additive) app.selection.toggle(ref)
        else app.selection.select(ref, false)
    }

    function doubleClick(x, y) {
        if (app.activeSketchId) {
            const hit = annotationAt(x, y)
            if (hit && hit.dim) { editingConstraint = hit.index; return }
        }
        if (app.activeSketchId && app.sketchTool) { finishSketchInput(); return }
        if (app.activeSketchId || app.commandFeatureId) return
        const ref = r.pick(x, y, "sketchcurve,profile")
        if (ref.sketch) app.editSketch(ref.sketch, false)
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton | Qt.MiddleButton | Qt.RightButton
        hoverEnabled: true
        property point last
        property point pressPos
        property int mode: 0          // 1 orbit, 2 pan, 3 click candidate, 4 drag handle, 5 drag sketch item, 6 resize plane,
                                      // 7 region selection, 8 move triad, 9 point pick, 10 trim paint-delete,
                                      // 11 press on a moved body (becomes 8 once dragged)
        property int pressMods: 0
        property bool dragged: false

        onPressed: (m) => {
            vp.forceActiveFocus()
            last = Qt.point(m.x, m.y)
            pressPos = last
            pressMods = m.modifiers
            dragged = false
            vp.trimPaintStroke = false
            if (m.button === Qt.MiddleButton) mode = (m.modifiers & Qt.ShiftModifier) ? 1 : 2
            else if (m.button === Qt.RightButton) mode = 1
            else if (vp.orbitMode) mode = 1
            else if (vp.panMode) mode = 2
            else if (m.button === Qt.LeftButton && vp.app.pointPick) mode = 9
            else if (m.button === Qt.LeftButton && triad.begin(m.x, m.y)) mode = 8
            else if (m.button === Qt.LeftButton && (vp.grabHit = vp.bodyGrabAt(m.x, m.y))) mode = 11
            else if (vp.nearSection(m.x, m.y) && vp.beginSectionDrag(m.x, m.y)) mode = 4
            else if (vp.nearMove(m.x, m.y) && vp.beginMoveDrag()) mode = 4
            else if (vp.nearAngle(m.x, m.y) && vp.beginAngleDrag()) mode = 4
            else if (vp.nearManip(m.x, m.y) && vp.beginManipDrag(m.x, m.y)) mode = 4
            else if (vp.beginSketchPress(m.x, m.y)) mode = 5
            else if (vp.beginPlaneResize(m.x, m.y)) mode = 6
            else mode = 3
        }
        onPositionChanged: (m) => {
            const dx = m.x - last.x, dy = m.y - last.y
            last = Qt.point(m.x, m.y)
            if (pressed && Math.abs(m.x - pressPos.x) + Math.abs(m.y - pressPos.y) > 4) dragged = true
            if (pressed && mode === 1 && dragged) vp.app.camera.orbit(dx, dy)
            else if (pressed && mode === 2) vp.app.camera.pan(dx, dy, vp.height)
            else if (pressed && mode === 4) { vp.moveManipDrag(m.x, m.y); return }
            else if (pressed && mode === 8) { triad.move(m.x, m.y, m.modifiers); return }
            else if (pressed && mode === 11) {
                if (dragged && triad.beginGrab(pressPos.x, pressPos.y, vp.grabHit)) {
                    mode = 8
                    triad.move(m.x, m.y, m.modifiers)
                }
                return
            }
            else if (pressed && mode === 5) { if (dragged) vp.moveSketchDrag(m.x, m.y); return }
            else if (pressed && mode === 6) { if (dragged) vp.movePlaneResize(m.x, m.y); return }
            else if (pressed && mode === 7) { vp.moveRegion(m.x, m.y); return }
            else if (pressed && mode === 10) { vp.paintTrimDelete(m.x, m.y); return }
            else if (pressed && mode === 3 && dragged && (m.buttons & Qt.LeftButton)
                     && vp.app.activeSketchId && vp.app.sketchTool === "trim") {
                mode = 10
                vp.paintTrimDelete(m.x, m.y)
                return
            }
            else if (pressed && mode === 3 && dragged && (m.buttons & Qt.LeftButton) && vp.canRegionSelect()) {
                mode = 7
                vp.beginRegion(pressPos.x, pressPos.y, pressMods)
                vp.moveRegion(m.x, m.y)
                return
            }

            if (vp.app.pointPick && !pressed) {
                vp.pointHover = vp.pointUnder(m.x, m.y)
                pointCanvas.requestPaint()
            } else if (vp.app.activeSketchId && vp.app.sketchTool) {
                vp.sketchMove(m.x, m.y)
            } else if (!pressed) {
                vp.manipHover = triad.hover(m.x, m.y) || vp.nearManip(m.x, m.y) || vp.nearAngle(m.x, m.y)
                                || vp.nearSection(m.x, m.y)
                                || vp.nearMove(m.x, m.y) || !!vp.planeEdgeAt(m.x, m.y)
                if (vp.manipHover) r.clearHover()
                else r.hover(m.x, m.y)
            }
        }
        onReleased: (m) => {
            if (m.button === Qt.RightButton && !dragged) markMenu.openAt(m.x, m.y)
            else if (m.button === Qt.LeftButton && (mode === 3 || mode === 5 || mode === 6 || mode === 11) && !dragged) vp.click(m.x, m.y, m.modifiers)
            if (mode === 4) vp.endManipDrag()
            if (mode === 8) triad.end()
            if (mode === 9 && m.button === Qt.LeftButton && !dragged) vp.pickPoint(m.x, m.y)
            if (mode === 5) vp.endSketchDrag()
            if (mode === 6) vp.planeDrag = null
            if (mode === 7) vp.endRegion()
            if (mode === 10) vp.trimPaintStroke = false
            mode = 0
        }
        onDoubleClicked: (m) => { if (m.button === Qt.LeftButton) vp.doubleClick(m.x, m.y) }
        onWheel: (w) => {
            const f = w.angleDelta.y > 0 ? 0.87 : 1 / 0.87
            if (vp.app.zoomToCursor)
                vp.app.camera.zoomAt(f, w.x, w.y, vp.width, vp.height)
            else
                vp.app.camera.zoom(f)
        }
        onExited: { r.clearHover(); vp.cursor = null; preview.requestPaint() }
        cursorShape: (vp.app.activeSketchId && vp.app.sketchTool) || vp.app.pointPick ? Qt.CrossCursor
                   : mode === 1 ? Qt.ClosedHandCursor : mode === 2 ? Qt.SizeAllCursor
                   : mode === 7 ? (vp.app.selectionTool === "paint" ? Qt.PointingHandCursor : Qt.CrossCursor)
                   : (mode >= 4 || vp.manipHover) ? Qt.SizeAllCursor : Qt.ArrowCursor
    }

    // Window box / lasso / paint trail while region-selecting
    Canvas {
        id: regionCanvas
        anchors.fill: parent
        z: 5
        onPaint: {
            const c = getContext("2d")
            c.reset()
            const p = vp.regionPath
            if (p.length < 2) return
            const tool = vp.app.selectionTool
            const crossing = vp.regionCrossing()
            const ink = crossing ? "#2fa84f" : "#1f6fd1"
            c.lineWidth = tool === "paint" ? 6 : 1.5
            c.strokeStyle = tool === "paint" ? "rgba(31,111,209,0.45)" : ink
            c.fillStyle = crossing ? "rgba(47,168,79,0.12)" : "rgba(31,111,209,0.12)"
            if (crossing) c.setLineDash([6, 4])
            c.beginPath()
            if (tool === "window") {
                const a = p[0], b = p[p.length - 1]
                c.rect(Math.min(a[0], b[0]), Math.min(a[1], b[1]), Math.abs(b[0] - a[0]), Math.abs(b[1] - a[1]))
            } else {
                c.moveTo(p[0][0], p[0][1])
                for (let i = 1; i < p.length; ++i) c.lineTo(p[i][0], p[i][1])
                if (tool === "freeform") c.closePath()
            }
            if (tool !== "paint") c.fill()
            c.stroke()
        }
    }

    // Rubber band for the active sketch tool
    Canvas {
        id: preview
        anchors.fill: parent
        visible: vp.app.activeSketchId.length > 0
        onPaint: {
            const c = getContext("2d")
            c.reset()
            if (!vp.app.activeSketchId || !vp.app.sketchTool) return
            if (vp.constraintTool) {
                // Items picked so far, and the point the cursor would pick
                c.fillStyle = Theme.manipulator
                for (const p of r.projectSketch(vp.app.activeSketchId, vp.cpicks.map(q => [q.u, q.v]))) {
                    c.beginPath(); c.arc(p[0], p[1], 5, 0, Math.PI * 2); c.fill()
                }
                if (vp.cursor && vp.cursor.snap === "endpoint") {
                    const cp = r.projectSketch(vp.app.activeSketchId, [[vp.cursor.u, vp.cursor.v]])
                    if (cp.length) { c.strokeStyle = Theme.manipulator; c.lineWidth = 2; c.strokeRect(cp[0][0] - 5, cp[0][1] - 5, 10, 10) }
                }
                return
            }
            const pv = vp.previewCurves()
            c.lineWidth = 1.5
            c.strokeStyle = vp.app.sketchConstruction ? Theme.construction : "#1f6fd1"
            c.setLineDash([5, 3])
            for (const curve of pv.curves) {
                const sp = r.projectSketch(vp.app.activeSketchId, curve)
                if (sp.length < 2) continue
                c.beginPath()
                c.moveTo(sp[0][0], sp[0][1])
                for (let i = 1; i < sp.length; ++i) c.lineTo(sp[i][0], sp[i][1])
                c.stroke()
            }
            c.setLineDash([])
            // committed points
            const placed = r.projectSketch(vp.app.activeSketchId, vp.pts)
            c.fillStyle = "#1f6fd1"
            for (const p of placed) c.fillRect(p[0] - 3, p[1] - 3, 6, 6)
            if (!vp.cursor) return
            const cp = r.projectSketch(vp.app.activeSketchId, [[vp.cursor.u, vp.cursor.v]])
            if (!cp.length) return
            const x = cp[0][0], y = cp[0][1]
            if (vp.cursor.snap === "endpoint") {
                c.fillStyle = "#f28c28"
                c.fillRect(x - 4, y - 4, 8, 8)
            } else {
                c.strokeStyle = "#1f6fd1"
                c.lineWidth = 1
                c.beginPath(); c.moveTo(x - 6, y); c.lineTo(x + 6, y); c.moveTo(x, y - 6); c.lineTo(x, y + 6); c.stroke()
            }
            const coord = "(" + vp.app.lenValue(vp.cursor.u) + ", " + vp.app.lenValue(vp.cursor.v) + ")"
            const text = pv.label ? pv.label + "   " + coord : coord
            c.font = "12px 'Segoe UI'"
            const w = c.measureText(text).width + 10
            c.fillStyle = "rgba(255,255,255,0.9)"
            c.fillRect(x + 14, y + 12, w, 20)
            c.fillStyle = "#24292e"
            c.fillText(text, x + 19, y + 26)
        }
    }

    // Dimensions and constraint badges of the active sketch; also records their hit boxes
    Canvas {
        id: annoCanvas
        anchors.fill: parent
        visible: vp.app.activeSketchId.length > 0
        onVisibleChanged: requestPaint()

        readonly property color ink: "#1d2a38"        // text on the white labels and badges
        readonly property color lineInk: "#dde4ec"    // dimension lines over the dark view
        readonly property var badgeText: ({
            horizontal: "H", vertical: "V", parallel: "//", perpendicular: "\u22a5", collinear: "\u2261",
            tangent: "T", equal: "=", concentric: "\u25ce", midpoint: "\u25b3", symmetric: "][", fix: ""
        })

        function valueText(a) {
            const f = v => vp.app.lenValue(v).toString()
            const t = a.type === "angle" ? (Math.round(a.measured * 10) / 10) + "\u00b0"
                    : a.type === "radius" ? "R" + f(a.measured)
                    : a.type === "diameter" ? "\u2300" + f(a.measured) : f(a.measured)
            return a.expression ? "fx: " + t : t
        }

        onPaint: {
            const c = getContext("2d")
            c.reset()
            const hits = []
            const s = vp.app.activeSketch
            const id = vp.app.activeSketchId
            if (!s || !visible) { vp.annoHits = hits; return }
            const P = uv => { const q = r.projectSketch(id, [uv]); return q.length ? q[0] : null }
            const o = P([0, 0]), px = P([1, 0]), py = P([0, 1])
            if (!o || !px || !py) { vp.annoHits = hits; return }
            const ppm = Math.max(1e-6, (Math.hypot(px[0] - o[0], px[1] - o[1]) + Math.hypot(py[0] - o[0], py[1] - o[1])) / 2)
            const off = 24 / ppm      // dimension line offset: 24 px in sketch units
            c.font = "12px 'Segoe UI'"
            c.lineWidth = 1

            const seg = (a, b) => { c.beginPath(); c.moveTo(a[0], a[1]); c.lineTo(b[0], b[1]); c.stroke() }
            const arrow = (tip, from) => {
                const dx = tip[0] - from[0], dy = tip[1] - from[1], L = Math.hypot(dx, dy)
                if (L < 1e-6) return
                const ux = dx / L, uy = dy / L
                c.beginPath()
                c.moveTo(tip[0], tip[1])
                c.lineTo(tip[0] - ux * 8 - uy * 3, tip[1] - uy * 8 + ux * 3)
                c.lineTo(tip[0] - ux * 8 + uy * 3, tip[1] - uy * 8 - ux * 3)
                c.closePath()
                c.fill()
            }
            const label = (a, x, y) => {
                const text = valueText(a)
                const w = c.measureText(text).width + 10, h = 18, bx = x - w / 2, by = y - h / 2
                const sel = a.index === vp.selectedConstraint
                c.fillStyle = sel ? "#fff3e6" : "rgba(255,255,255,0.92)"
                c.fillRect(bx, by, w, h)
                c.strokeStyle = sel ? Theme.manipulator : "rgba(29,42,56,0.35)"
                c.strokeRect(bx + 0.5, by + 0.5, w - 1, h - 1)
                c.fillStyle = a.expression ? "#1f6fd1" : ink
                c.fillText(text, bx + 5, by + 13)
                hits.push({ index: a.index, x: bx, y: by, w: w, h: h, dim: true, anno: a })
            }

            const annos = (vp.app.rev, s.annotations())
            for (const a of annos) {
                if (!a.geom) continue
                const col = a.index === vp.selectedConstraint ? Theme.manipulator : lineInk
                c.strokeStyle = col
                c.fillStyle = col
                const placed = a.offset !== undefined
                if (a.geom === "linear") {
                    const p1 = a.p1, p2 = a.p2
                    const dOff = placed ? a.offset : a.type === "pointline" ? 0 : off
                    let d1, d2
                    if (a.type !== "pointline" && a.orient === "horizontal") { const v = Math.max(p1[1], p2[1]) + dOff; d1 = [p1[0], v]; d2 = [p2[0], v] }
                    else if (a.type !== "pointline" && a.orient === "vertical") { const u = Math.max(p1[0], p2[0]) + dOff; d1 = [u, p1[1]]; d2 = [u, p2[1]] }
                    else {
                        const L = Math.hypot(p2[0] - p1[0], p2[1] - p1[1])
                        const n = L > 1e-9 ? [-(p2[1] - p1[1]) / L, (p2[0] - p1[0]) / L] : [0, 1]
                        d1 = [p1[0] + n[0] * dOff, p1[1] + n[1] * dOff]
                        d2 = [p2[0] + n[0] * dOff, p2[1] + n[1] * dOff]
                    }
                    const s1 = P(p1), s2 = P(p2), e1 = P(d1), e2 = P(d2)
                    if (!s1 || !s2 || !e1 || !e2) continue
                    c.globalAlpha = 0.55
                    if (Math.abs(dOff) > 1e-9) { seg(s1, e1); seg(s2, e2) }
                    c.globalAlpha = 1
                    seg(e1, e2)
                    const m = [(e1[0] + e2[0]) / 2, (e1[1] + e2[1]) / 2]
                    arrow(e1, m)
                    arrow(e2, m)
                    label(a, m[0], m[1])
                } else if (a.geom === "radial") {
                    const cen = a.center, ang = placed ? a.offset * Math.PI / 180 : Math.PI / 4
                    const sc = P(cen), se = P([cen[0] + a.r * Math.cos(ang), cen[1] + a.r * Math.sin(ang)])
                    if (!sc || !se) continue
                    const dx = se[0] - sc[0], dy = se[1] - sc[1], L = Math.hypot(dx, dy) || 1
                    const out = [se[0] + dx / L * 28, se[1] + dy / L * 28]
                    const start = a.type === "diameter" ? [sc[0] - dx, sc[1] - dy] : sc
                    seg(start, out)
                    arrow(se, sc)
                    if (a.type === "diameter") arrow(start, sc)
                    label(a, out[0] + dx / L * 18, out[1] + dy / L * 10)
                } else if (a.geom === "angular") {
                    const v = a.vertex, d1 = a.d1, d2 = a.d2
                    const R = placed ? Math.max(a.offset, 1e-6)
                                     : Math.max(0.45 * Math.min(Math.hypot(d1[0], d1[1]), Math.hypot(d2[0], d2[1])), 1e-6)
                    const a1 = Math.atan2(d1[1], d1[0])
                    let sweep = Math.atan2(d2[1], d2[0]) - a1
                    while (sweep > Math.PI) sweep -= 2 * Math.PI
                    while (sweep < -Math.PI) sweep += 2 * Math.PI
                    const arcUV = []
                    for (let i = 0; i <= 24; ++i) {
                        const t = a1 + sweep * i / 24
                        arcUV.push([v[0] + R * Math.cos(t), v[1] + R * Math.sin(t)])
                    }
                    const sp = r.projectSketch(id, arcUV)
                    if (sp.length < 25) continue
                    c.beginPath()
                    c.moveTo(sp[0][0], sp[0][1])
                    for (let i = 1; i < sp.length; ++i) c.lineTo(sp[i][0], sp[i][1])
                    c.stroke()
                    arrow(sp[0], sp[2])
                    arrow(sp[24], sp[22])
                    label(a, sp[12][0], sp[12][1])
                }
            }

            // Geometric constraints: a small badge beside each anchor (coincidence is a dot)
            const stack = {}
            for (const a of annos) {
                if (!a.anchors) continue
                const sel = a.index === vp.selectedConstraint
                for (const an of a.anchors) {
                    const sp = P(an)
                    if (!sp) continue
                    if (a.type === "coincident") {
                        c.fillStyle = sel ? Theme.manipulator : lineInk
                        c.fillRect(sp[0] - 3, sp[1] - 3, 6, 6)
                        hits.push({ index: a.index, x: sp[0] - 5, y: sp[1] - 5, w: 10, h: 10, dim: false })
                        continue
                    }
                    const key = Math.round(sp[0] / 8) + "," + Math.round(sp[1] / 8)
                    const k = stack[key] || 0
                    stack[key] = k + 1
                    const bx = sp[0] + 6 + k * 19, by = sp[1] - 24
                    c.fillStyle = sel ? "#fff3e6" : "rgba(255,255,255,0.95)"
                    c.fillRect(bx, by, 17, 17)
                    c.strokeStyle = sel ? Theme.manipulator : "rgba(29,42,56,0.45)"
                    c.strokeRect(bx + 0.5, by + 0.5, 16, 16)
                    c.fillStyle = sel ? Theme.manipulator : ink
                    c.strokeStyle = c.fillStyle
                    if (a.type === "fix") {
                        c.fillRect(bx + 4, by + 8, 9, 6)
                        c.beginPath(); c.arc(bx + 8.5, by + 7.5, 3, Math.PI, 0, false); c.stroke()
                    } else {
                        const t = badgeText[a.type] || "?"
                        c.font = "bold 11px 'Segoe UI'"
                        c.fillText(t, bx + 8.5 - c.measureText(t).width / 2, by + 13)
                        c.font = "12px 'Segoe UI'"
                    }
                    hits.push({ index: a.index, x: bx, y: by, w: 17, h: 17, dim: false })
                }
            }

            // Spline fit points: drag handles
            const fits = vp.splineFitPoints()
            if (fits.length) {
                const sp = r.projectSketch(id, fits.map(f => [f.u, f.v]))
                c.strokeStyle = "#1f6fd1"
                c.fillStyle = "white"
                c.lineWidth = 1.5
                for (const q of sp) {
                    c.fillRect(q[0] - 3.5, q[1] - 3.5, 7, 7)
                    c.strokeRect(q[0] - 3.5, q[1] - 3.5, 7, 7)
                }
            }
            vp.annoHits = hits
        }
    }

    // Value editor over a dimension label (Enter applies, Esc closes); accepts expressions
    Rectangle {
        id: dimEditor
        readonly property var hit: (vp.annoHits, vp.editingConstraint >= 0)
                                   ? vp.annoHits.find(h => h.dim && h.index === vp.editingConstraint) || null : null
        property string original: ""
        visible: !!hit && !!vp.app.activeSketch
        x: hit ? Math.max(4, Math.min(hit.x + hit.w / 2 - width / 2, vp.width - width - 4)) : 0
        y: hit ? hit.y + hit.h / 2 - height / 2 : 0
        width: Math.max(96, editField.contentWidth + 18)
        height: 24
        radius: 3
        color: "white"
        border.color: Theme.accent
        border.width: 2

        function init() {
            if (!visible) return
            const c = vp.app.activeSketch.constraint(vp.editingConstraint)
            original = String(c.value === undefined ? "" : c.value)
            editField.text = original
            editField.forceActiveFocus()
            editField.selectAll()
        }
        function commit() {
            const idx = vp.editingConstraint, s = vp.app.activeSketch
            if (idx < 0 || !s) return
            let text = editField.text.trim()
            if (!text.length || text === original) { close(); return }
            text = vp.app.withUnit(text, vp.app.activeSketch.constraint(idx).type === "angle" ? "angle" : "length")
            vp.app.doc.checkpoint()
            const res = s.setConstraintValue(idx, text)
            if (!res.ok) {
                vp.app.doc.undo()
                fissionApp.showMessage(res.error)
                editField.forceActiveFocus()
                return
            }
            close()
        }
        function close() {
            vp.editingConstraint = -1
            vp.forceActiveFocus()
        }
        onVisibleChanged: init()

        TextInput {
            id: editField
            anchors.fill: parent
            anchors.leftMargin: 8
            anchors.rightMargin: 8
            verticalAlignment: TextInput.AlignVCenter
            font.pixelSize: 13
            color: Theme.text
            selectByMouse: true
            clip: true
            onAccepted: dimEditor.commit()
            Keys.onEscapePressed: dimEditor.close()
        }
    }
    onEditingConstraintChanged: Qt.callLater(dimEditor.init)
    onSelectedConstraintChanged: annoCanvas.requestPaint()
    Connections {
        target: vp.app
        function onRevChanged() { if (vp.app.activeSketchId) annoCanvas.requestPaint() }
    }

    // Move/Copy triad (bodies or sketch curves)
    MoveTriad {
        id: triad
        anchors.fill: parent
        z: 6
        renderer: r
        camera: vp.app.camera
        gizmo: vp.app.moveGizmo
    }

    // Point input (Move pivot, point to point): app.pointPick = {prompt, done(p)}; clicks take the snapped point
    property var pointHover: ({})
    onPointHoverChanged: pointCanvas.requestPaint()
    // {x, y, z, label} under the cursor; for sketch picks also {u, v}
    function pointUnder(x, y) {
        if (!app.pointPick || !app.pointPick.sketch) return r.snapPoint(x, y)
        const sp = r.sketchPoint(x, y, app.activeSketchId, true)
        if (sp.u === undefined) return ({})
        const f = app.doc.planeFrame({ kind: "plane", id: app.activeSketchId })
        const w = [0, 1, 2].map(k => f.origin[k] + f.x[k] * sp.u + f.y[k] * sp.v)
        return { x: w[0], y: w[1], z: w[2], u: sp.u, v: sp.v,
                 label: sp.snap === "endpoint" ? "Point" : sp.snap === "grid" ? "Grid" : "Sketch" }
    }
    function pickPoint(x, y) {
        const p = pointUnder(x, y)
        if (p.x === undefined) { fissionApp.showMessage("Click a vertex, edge, circle or face"); return }
        const req = app.pointPick
        app.pointPick = null
        pointHover = ({})
        req.done(req.sketch ? [p.u, p.v] : [p.x, p.y, p.z], p.label)
    }
        Canvas {
        id: pointCanvas
            anchors.fill: parent
        z: 6
        visible: !!vp.app.pointPick
        onVisibleChanged: requestPaint()
            onPaint: {
            const c = getContext("2d")
            c.reset()
            const p = vp.pointHover
            if (!vp.app.pointPick || p.x === undefined) return
            const q = r.projectPoint(Qt.vector3d(p.x, p.y, p.z))
            if (!q.length) return
            c.strokeStyle = "#f28c28"
            c.fillStyle = "white"
            c.lineWidth = 2
            c.beginPath(); c.arc(q[0], q[1], 6, 0, Math.PI * 2); c.fill(); c.stroke()
            c.font = "11px 'Segoe UI'"
            const w = c.measureText(p.label).width + 10
            c.fillStyle = "rgba(29,42,56,0.9)"
            c.fillRect(q[0] + 10, q[1] + 8, w, 17)
            c.fillStyle = "white"
            c.fillText(p.label, q[0] + 15, q[1] + 20)
        }
    }

    // Drag handle of the active feature
    Canvas {
        id: manipCanvas
        anchors.fill: parent
        visible: !!vp.manip || !!vp.sectionManip
        onPaint: {
            const c = getContext("2d")
            c.reset()
            function paintArrow(s, hot) {
                if (!s) return
                c.strokeStyle = Theme.manipulator
                c.fillStyle = Theme.manipulator
                c.lineWidth = hot ? 3 : 2
                c.beginPath(); c.moveTo(s.o[0], s.o[1]); c.lineTo(s.t[0], s.t[1]); c.stroke()
                c.beginPath(); c.arc(s.o[0], s.o[1], 3, 0, Math.PI * 2); c.fill()
                const len = hot ? 22 : 18, half = hot ? 8 : 6.5
                const bx = s.t[0], by = s.t[1], dx = s.d[0], dy = s.d[1]
                c.beginPath()
                c.moveTo(bx + dx * len, by + dy * len)
                c.lineTo(bx - dy * half, by + dx * half)
                c.lineTo(bx + dy * half, by - dx * half)
                c.closePath()
                c.fill()
            }
            const hot = vp.manipHover || vp.manipDrag
            const draftOnly = vp.app.commandFeature && vp.app.commandFeature.type === "Draft"
            if (!draftOnly) paintArrow(vp.manipScreen(), hot && !(vp.manipDrag && vp.manipDrag.section))
            paintArrow(vp.sectionScreen(), hot && (!vp.manipDrag || vp.manipDrag.section))

            const as = vp.angleScreen()
            if (as) {
                c.lineWidth = 1.5
                c.setLineDash([5, 3])
                c.beginPath(); c.moveTo(as.p[0], as.p[1]); c.lineTo(as.q[0], as.q[1]); c.stroke()
                c.setLineDash([])
                c.fillStyle = "white"
                c.lineWidth = 2
                c.beginPath(); c.arc(as.q[0], as.q[1], 7, 0, Math.PI * 2); c.fill(); c.stroke()
                c.fillStyle = Theme.manipulator
                c.beginPath(); c.arc(as.q[0], as.q[1], 2.5, 0, Math.PI * 2); c.fill()
                const text = (Math.round(as.value * 10) / 10) + "\u00b0"
                c.font = "12px 'Segoe UI'"
                const w = c.measureText(text).width + 10
                c.fillStyle = "rgba(255,255,255,0.92)"
                c.fillRect(as.q[0] + 12, as.q[1] - 9, w, 18)
                c.fillStyle = "#1d2a38"
                c.fillText(text, as.q[0] + 17, as.q[1] + 4)
            }

            const mp = vp.moveScreen()
            if (mp) {
                // Snap targets while dragging, then the point handle itself
                if (vp.manipDrag && vp.manipDrag.move) {
                    c.fillStyle = "rgba(255,255,255,0.85)"
                    c.strokeStyle = Theme.manipulator
                    c.lineWidth = 1.2
                    for (const sn of vp.manip.move.snaps || []) {
                        const q = r.projectPoint(Qt.vector3d(sn[0], sn[1], sn[2]))
                        if (!q.length) continue
                        c.beginPath()
                        c.moveTo(q[0], q[1] - 4); c.lineTo(q[0] + 4, q[1]); c.lineTo(q[0], q[1] + 4); c.lineTo(q[0] - 4, q[1])
                        c.closePath(); c.fill(); c.stroke()
                    }
                    if (vp.moveSnap) {
                        c.font = "11px 'Segoe UI'"
                        const w = c.measureText(vp.moveSnap.label).width + 10
                        c.fillStyle = "rgba(29,42,56,0.9)"
                        c.fillRect(mp[0] + 12, mp[1] + 10, w, 17)
                        c.fillStyle = "white"
                        c.fillText(vp.moveSnap.label, mp[0] + 17, mp[1] + 22)
                    }
                }
                c.strokeStyle = Theme.manipulator
                c.fillStyle = "white"
                c.lineWidth = 2
                c.beginPath(); c.arc(mp[0], mp[1], hot ? 8 : 7, 0, Math.PI * 2); c.fill(); c.stroke()
                c.beginPath()
                c.moveTo(mp[0] - 4, mp[1]); c.lineTo(mp[0] + 4, mp[1])
                c.moveTo(mp[0], mp[1] - 4); c.lineTo(mp[0], mp[1] + 4)
                c.stroke()
            }
        }
    }

    // Value of the handle; edits go straight into the feature (expressions allowed)
    Rectangle {
        id: manipBox
        readonly property var s: (vp.camRev, vp.width, vp.height, vp.manip, vp.manipScreen())
        readonly property var pos: (s, vp.angleScreen(), width, height, vp.manipBoxPos(width, height))
        readonly property string expr: (vp.app.rev, vp.manip && vp.app.commandFeature) ? String(vp.app.commandFeature.get(vp.manip.key)) : ""
        visible: !!vp.manip && !!s && !(vp.app.commandFeature && vp.app.commandFeature.type === "Draft")
        x: pos ? pos.x : 0
        y: pos ? pos.y : 0
        width: 120
        height: 26
        radius: 3
        color: "white"
        border.color: Theme.manipulator
        onExprChanged: if (!manipField.activeFocus) manipField.text = expr
        TextInput {
            id: manipField
            anchors.fill: parent
            anchors.leftMargin: 6
            anchors.rightMargin: 6
            verticalAlignment: TextInput.AlignVCenter
            font.pixelSize: 12
            color: Theme.text
            selectByMouse: true
            clip: true
            onTextEdited: manipCommit.restart()
            onActiveFocusChanged: if (!activeFocus) text = manipBox.expr
            Keys.onEscapePressed: (e) => { manipCommit.stop(); text = manipBox.expr; vp.forceActiveFocus(); e.accepted = true }
        }
        Timer {
            id: manipCommit
            interval: 350
            onTriggered: manipBox.commit()
        }
        function commit() {
            const f = vp.app.commandFeature
            if (!f || !vp.manip) return
            const kind = vp.app.inputKind(vp.manip.key)
            const text = vp.app.withUnit(manipField.text, kind)
            const r = vp.app.doc.evaluateExpression(text)
            if (!r.error) f.set(vp.manip.key, text)
        }
    }
    // First keystroke replaces the value; further keys go to the focused box as usual
    function typeIntoManip(ch) {
        if (!manip || !manipBox.visible) return false
        manipField.forceActiveFocus()
        manipField.text = ch
        manipCommit.restart()
        return true
    }
    function flushManip() {
        if (!manipCommit.running) return
        manipCommit.stop()
        manipBox.commit()
    }

    // Dimension input next to the cursor while drawing; Tab moves between fields, Enter commits
    Rectangle {
        id: dimBox
        readonly property var specs: (vp.pts.length, vp.app.sketchTool, vp.dimSpecs())
        readonly property var live: (vp.cursor, vp.dimLocks, vp.liveDims())
        visible: vp.app.activeSketchId.length > 0 && specs.length > 0 && !!vp.cursor
        x: Math.max(4, Math.min(vp.cursorPx.x + 22, vp.width - width - 4))
        y: Math.max(4, Math.min(vp.cursorPx.y + 38, vp.height - height - 4))
        width: dimRow.implicitWidth + 10
        height: dimRow.implicitHeight + 10
        radius: 3
        color: "#f7ffffff"
        border.color: Theme.accent

        function focusField(i, initial) {
            const f = dimRepeater.itemAt(i)
            if (!f) return
            f.field.forceActiveFocus()
            if (initial.length) { f.field.text = initial; f.field.edited() }
            else f.field.selectAll()
        }

        Row {
            id: dimRow
            x: 5
            y: 5
            spacing: 6
            Repeater {
                id: dimRepeater
                model: dimBox.specs
                Column {
                    id: dimCol
                    required property int index
                    required property string modelData
                    property alias field: input
                    property string error: ""
                    spacing: 1
                    Text { text: dimCol.modelData; font.pixelSize: 10; color: Theme.textDim }
                    Rectangle {
                        width: 74
                        height: 22
                        radius: 2
                        color: input.activeFocus ? "white" : "#f0f3f6"
                        border.color: dimCol.error ? Theme.error : input.activeFocus ? Theme.accent : Theme.border
                        TextInput {
                            id: input
                            anchors.fill: parent
                            anchors.leftMargin: 4
                            anchors.rightMargin: 4
                            verticalAlignment: TextInput.AlignVCenter
                            font.pixelSize: 12
                            font.bold: text.length > 0
                            color: Theme.text
                            clip: true
                            selectByMouse: true
                            function edited() { dimCol.error = vp.setDimLock(dimCol.index, text) }
                            onTextEdited: edited()
                            onAccepted: vp.enterPressed()
                            Keys.onTabPressed: (e) => {
                                dimBox.focusField((dimCol.index + 1) % dimBox.specs.length, "")
                                e.accepted = true
                            }
                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                visible: input.text.length === 0
                                text: dimBox.live.length <= dimCol.index ? ""
                                    : dimCol.modelData === "Angle" ? (Math.round(dimBox.live[dimCol.index] * 100) / 100).toString()
                                    : vp.app.lenValue(dimBox.live[dimCol.index]).toString()
                                color: Theme.textDim
                                font.pixelSize: 12
                            }
                        }
                    }
                }
            }
        }
    }

    // Hint banner
    Rectangle {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: 10
        visible: hint.text.length > 0
        width: hint.implicitWidth + 24
        height: 28
        radius: 14
        color: "#e6ffffff"
        border.color: Theme.border
        Text {
            id: hint
            anchors.centerIn: parent
            color: Theme.text
            text: vp.app.pickingSketchPlane ? "Select a plane or planar face to sketch on"
                : (vp.app.activeSketchId && vp.app.sketchTool) ? (vp.pts.length, vp.cpicks.length, vp.mpicks.length, vp.sketchHint())
                : ""
        }
    }

    // ------------------------------------------------------------------ overlays

    ViewCube {
        app: vp.app
        visible: vp.app.showViewCube
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.margins: 10
    }

    PropertyPalette {
        app: vp.app
        visible: vp.app.showProperties
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.margins: 10
    }

    // Navigation bar
    Rectangle {
        visible: vp.app.showNavBar
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 10
        width: navRow.implicitWidth + 12
        height: 38
        radius: 5
        color: "#eefafbfc"
        border.color: Theme.border
        RowLayout {
            id: navRow
            anchors.centerIn: parent
            spacing: 2
            ToolbarButton { compact: true; glyph: "orbit"; tip: "Orbit (drag with left button; or Shift+middle / right drag)"
                checked: vp.orbitMode; onClicked: { vp.orbitMode = !vp.orbitMode; vp.panMode = false } }
            ToolbarButton { compact: true; glyph: "pan"; tip: "Pan (or middle drag)"
                checked: vp.panMode; onClicked: { vp.panMode = !vp.panMode; vp.orbitMode = false } }
            ToolbarButton { compact: true; glyph: "fit"; tip: "Fit (F6)"; onClicked: r.fitAll(true) }
            ToolbarButton { compact: true; glyph: "home"; tip: "Home view"; onClicked: vp.app.goHome() }
            Rectangle { Layout.preferredWidth: 1; Layout.preferredHeight: 20; color: Theme.border }
            ToolbarButton { compact: true; glyph: "ortho"; tip: "Orthographic projection"
                checked: vp.app.camera.orthographic; onClicked: vp.app.camera.orthographic = !vp.app.camera.orthographic }
            ToolbarButton {
                compact: true; glyph: "grid"; tip: "Grid"
                checked: r.showGrid
                onClicked: {
                    r.showGrid = !r.showGrid
                    fissionApp.setSetting("viewport/showGrid", r.showGrid)
                }
            }
            ToolbarButton { compact: true; glyph: "edges"; tip: "Visible edges"; checked: r.showEdges; onClicked: r.showEdges = !r.showEdges }
            ToolbarButton {
                id: meshDisplayButton
                compact: true
                glyph: "meshdisplay"
                tip: "Mesh display"
                checked: r.meshDisplay !== "shadedEdges"
                onClicked: meshDisplayMenu.popup(meshDisplayButton, 0, -meshDisplayMenu.implicitHeight)
            }
        }
    }
    Menu {
        id: meshDisplayMenu
        Repeater {
            model: [["shadedEdges", "Shaded with mesh edges"], ["shaded", "Shaded"], ["wireframe", "Wireframe"],
                    ["faceGroups", "Face groups"]]
            MenuItem {
                required property var modelData
                text: modelData[1]
                checkable: true
                checked: r.meshDisplay === modelData[0]
                onTriggered: r.meshDisplay = modelData[0]
            }
        }
    }

    MarkMenu {
        id: markMenu
        app: vp.app
        anchors.fill: parent
    }
}
