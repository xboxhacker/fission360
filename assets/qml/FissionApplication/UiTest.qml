// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
import QtQuick

// Scripted walkthrough driven by --uitest: exercises the same functions the toolbar, viewport and dialogs call.
Item {
    id: t
    property var app
    property int step: 0
    property real extrudeVolume: 0

    function vol() {
        let v = 0
        for (const b of app.doc.bodies) v += b.volume
        return v
    }
    function sketchPx(u, v) {
        return app.viewport.renderer.projectSketch(app.activeSketchId, [[u, v]])[0]
    }
    function clickSketch(u, v) { const p = sketchPx(u, v); app.viewport.sketchClick(p[0], p[1]) }
    function moveSketch(u, v) { const p = sketchPx(u, v); app.viewport.sketchMove(p[0], p[1]) }
    // First viewport pixel (scanning from the centre outwards) whose pick is of `kind`
    property var pickSpot: null
    function findPick(kind, accept) {
        const vp = app.viewport
        for (let rad = 0; rad < 0.35; rad += 0.01)
            for (let a = 0; a < 360; a += 15) {
                const x = vp.width * (0.5 + rad * Math.cos(a * Math.PI / 180))
                const y = vp.height * (0.5 + rad * Math.sin(a * Math.PI / 180))
                const ref = vp.renderer.pick(x, y)
                if (ref.kind === kind && (!accept || accept(ref))) return { x: x, y: y, ref: ref }
            }
        return null
    }
    function feature(type) {
        const list = app.doc.featureList(type)
        return list.length ? app.doc.featureById(list[0].id) : null
    }

    readonly property var steps: [
        () => { fissionApp.newDocument(); return 800 },
        () => {
            app.startCommand("Sketch")
            uiTest.check(app.pickingSketchPlane, "Create Sketch asks for a plane")
            const items = app.viewport.renderer.originItems
            uiTest.check(["XY", "XZ", "YZ"].every(k => items.indexOf(k) >= 0), "origin planes shown for the plane pick",
                         JSON.stringify(items))
            return 700
        },
        () => {
            uiTest.grab("01_pick_plane")
            pickSpot = findPick("originplane", r => r.id === "XY")
            uiTest.check(!!pickSpot, "XY origin plane can be picked in the view")
            if (pickSpot) app.viewport.click(pickSpot.x, pickSpot.y, 0)
            else app.createSketch("XY")
            return 900
        },
        () => {
            uiTest.check(!!app.activeSketch, "sketch on XY is active")
            app.sketchTool = "rectangle"
            clickSketch(-20, -15)
            moveSketch(20, 15)
            return 400
        },
        () => {
            uiTest.grab("02_rectangle_rubberband")
            clickSketch(20, 15)
            app.sketchTool = "circle"
            clickSketch(0, 0)
            moveSketch(0, 5)
            return 400
        },
        () => { uiTest.grab("03_circle_rubberband"); clickSketch(0, 5); return 700 },
        () => {
            const s = app.activeSketch
            uiTest.check(s && s.entityCount === 5 && s.profileCount === 2, "sketch has a rectangle and a circle (2 profiles)",
                         s ? s.entityCount + " curves, " + s.profileCount + " profiles" : "no sketch")
            app.sketchTool = ""
            uiTest.grab("04_sketch")
            app.finishSketch()
            app.camera.setView("home", false)
            app.viewport.renderer.fitAll(false)
            return 600
        },
        () => { app.startCommand("Extrude"); return 600 },
        () => {
            // Picking a profile in the view feeds the active selection input of the dialog
            const f = app.commandFeature
            app.selection.select({ kind: "profile", sketch: f.get("sketch"), index: 1 }, true)
            const picked = f.get("profiles")
            uiTest.check(picked.length === 1 && picked[0] === 1, "profile pick drives the Extrude profile input",
                         JSON.stringify(picked) + ", filter '" + app.selection.filter + "'")
            app.selection.clear()
            uiTest.check(f.get("profiles").length === 0, "clearing the pick restores the default profiles")
            f.set("distance", "15 mm")
            return 800
        },
        () => {
            // Drag the arrow handle further along its direction
            const vp = app.viewport
            const s = vp.manipScreen()
            uiTest.check(!!vp.manip && !!s && vp.manip.key === "distance", "extrude shows a drag arrow",
                         vp.manip ? JSON.stringify(vp.manip) : "no manipulator")
            if (!s) return 300
            const h = s.handle
            vp.beginManipDrag(h[0], h[1])
            vp.moveManipDrag(h[0] + s.d[0] * 80, h[1] + s.d[1] * 80)
            vp.endManipDrag()
            return 800
        },
        () => {
            const d = app.doc.evaluateExpression(app.commandFeature.get("distance")).value
            uiTest.check(d > 16, "dragging the arrow lengthens the extrude", "distance " + app.commandFeature.get("distance"))
            uiTest.grab("05b_extrude_drag")
            app.commandFeature.set("distance", "15 mm")
            return 800
        },
        () => { uiTest.grab("05_extrude_dialog"); app.finishCommand(true); return 700 },
        () => {
            extrudeVolume = vol()
            uiTest.check(Math.abs(extrudeVolume - (1200 - Math.PI * 25) * 15) < 0.5,
                         "extrude from the sketch (default profile keeps the hole)", "volume " + extrudeVolume.toFixed(3))
            app.viewport.renderer.fitAll(false)
            return 700
        },
        () => { uiTest.grab("06_extruded"); app.startCommand("Shell"); return 900 },
        () => {
            pickSpot = findPick("face")
            uiTest.check(app.doc.inputFeatureId === app.commandFeatureId && !!pickSpot,
                         "Shell's face input shows the pre-shell body", pickSpot ? "" : "no face under the cursor")
            if (pickSpot) app.viewport.click(pickSpot.x, pickSpot.y, 0)
            return 1000
        },
        () => {
            const f = app.commandFeature
            const n = (f.get("faces") || []).length
            const again = pickSpot ? app.viewport.renderer.pick(pickSpot.x, pickSpot.y) : ({})
            uiTest.check(n === 1 && again.kind === "face" && again.index === pickSpot.ref.index,
                         "picked shell face stays clickable after the preview", n + " faces, re-pick " + JSON.stringify(again))
            uiTest.grab("06b_shell_face_selected")
            if (pickSpot) app.viewport.click(pickSpot.x, pickSpot.y, 0)
            return 900
        },
        () => {
            uiTest.check((app.commandFeature.get("faces") || []).length === 0, "clicking the face again deselects it")
            app.finishCommand(false)
            return 700
        },
        () => {
            uiTest.check(app.doc.inputFeatureId === "" && Math.abs(vol() - extrudeVolume) < 0.5,
                         "cancelling Shell restores the normal view", "volume " + vol().toFixed(3))
            app.startCommand("Fillet")
            return 900
        },
        () => {
            pickSpot = findPick("edge")
            // The camera looks down from the front: the back-bottom edge (y = 15, z = 0) must not be pickable
            uiTest.check(!!pickSpot && !(Math.abs(pickSpot.ref.y - 15) < 1e-6 && Math.abs(pickSpot.ref.z) < 1e-6),
                         "edges behind the body cannot be picked", pickSpot ? JSON.stringify(pickSpot.ref) : "no edge")
            if (pickSpot) app.viewport.click(pickSpot.x, pickSpot.y, 0)
            app.commandFeature.set("radius", "1 mm")
            return 1000
        },
        () => {
            const n = (app.commandFeature.get("edges") || []).length
            uiTest.check(!!pickSpot && n === 1 && app.commandFeature.healthy, "fillet one picked edge",
                         n + " edges " + app.commandFeature.error)
            uiTest.grab("06c_fillet_edge_selected")
            if (pickSpot) app.viewport.click(pickSpot.x, pickSpot.y, 0)
            return 900
        },
        () => {
            uiTest.check((app.commandFeature.get("edges") || []).length === 0, "clicking the edge again deselects it")
            app.commandFeature.set("allEdges", true)
            return 1000
        },
        () => {
            uiTest.grab("07_fillet_dialog")
            const f = app.commandFeature
            uiTest.check(f && f.healthy && vol() < extrudeVolume, "fillet all edges", f ? f.error : "")
            app.finishCommand(true)
            return 600
        },
        () => {
            app.doc.addParameter("depth", "25 mm")
            feature("Extrude").set("distance", "depth")
            return 1000
        },
        () => {
            uiTest.check(vol() > extrudeVolume * 1.5, "extrude driven by the 'depth' parameter", "volume " + vol().toFixed(3))
            app.viewport.renderer.fitAll(false)
            app.openParameters()
            return 900
        },
        () => { uiTest.grab("08_parameters"); app.closeParameters(); return 300 },
        () => { app.doc.timeline.marker = 2; return 800 },
        () => {
            uiTest.grab("09_rolled_back")
            uiTest.check(Math.abs(vol() - (1200 - Math.PI * 25) * 25) < 0.5, "rolling the marker back skips the fillet",
                         "volume " + vol().toFixed(3))
            app.doc.timeline.marker = app.doc.timeline.count
            return 800
        },
        () => {
            extrudeVolume = vol()
            app.editFeature(feature("Extrude"))
            app.commandFeature.set("distance", "5 mm")
            return 900
        },
        () => {
            uiTest.grab("10_edit_extrude")
            app.finishCommand(false)
            return 900
        },
        () => {
            uiTest.check(Math.abs(vol() - extrudeVolume) < 1e-6, "cancelling an edit restores the model",
                         vol().toFixed(3) + " vs " + extrudeVolume.toFixed(3))
            app.showProperties = true
            app.selection.select({ kind: "body", body: 0 }, false)
            return 600
        },
        () => { uiTest.grab("11_properties"); app.showProperties = false; return 100 },

        // ---- Phase 2 tools through their dialogs
        () => { fissionApp.newDocument(); app.createSketch("XZ"); return 900 },
        () => {
            app.sketchTool = "rectangle"
            clickSketch(10, 0)
            clickSketch(20, 10)
            app.finishSketch()
            app.startCommand("Revolve")
            app.commandFeature.set("axis", "Z")
            return 900
        },
        () => {
            app.viewport.renderer.fitAll(false)
            const f = app.commandFeature
            uiTest.check(f && f.healthy && Math.abs(vol() - Math.PI * 300 * 10) < 1, "revolve dialog: profile about Z",
                         f ? f.error + " volume " + vol().toFixed(3) : "")
            return 700
        },
        () => { uiTest.grab("12_revolve_dialog"); app.finishCommand(true); return 400 },

        () => { fissionApp.newDocument(); app.createSketch("XY"); return 900 },
        () => {
            app.sketchTool = "rectangle"
            clickSketch(10, 0)
            clickSketch(20, 10)
            app.finishSketch()
            app.startCommand("Extrude")
            app.commandFeature.set("distance", "5 mm")
            app.finishCommand(true)
            app.startCommand("CircularPattern")
            return 900
        },
        () => {
            app.camera.setView("home", false)
            app.viewport.renderer.fitAll(false)
            const f = app.commandFeature
            uiTest.check(f && f.healthy && app.doc.bodies.length === 6, "circular pattern dialog: 6 instances",
                         f ? f.error + " bodies " + app.doc.bodies.length : "")
            return 700
        },
        () => { uiTest.grab("13_circular_pattern"); app.finishCommand(true); return 400 },
        () => {
            app.selection.select({ kind: "body", body: 0 }, false)
            app.startCommand("Mirror")
            app.commandFeature.set("plane", "XZ")
            app.commandFeature.set("operation", "newBody")
            return 900
        },
        () => {
            const f = app.commandFeature
            uiTest.check(f && f.healthy && app.doc.bodies.length === 7, "mirror dialog: one body across XZ",
                         f ? f.error + " bodies " + app.doc.bodies.length : "")
            uiTest.grab("14_mirror")
            app.finishCommand(true)
            return 400
        },

        () => {
            fissionApp.newDocument()
            app.createSketch("XY")
            return 900
        },
        () => {
            app.sketchTool = "rectangle"
            clickSketch(-10, -10)
            clickSketch(10, 10)
            app.finishSketch()
            app.startCommand("ConstructionPlane", { mode: "offset" })
            app.commandFeature.set("offset", "30 mm")
            return 900
        },
        () => {
            const f = app.commandFeature
            uiTest.check(f && f.healthy, "offset plane dialog", f ? f.error : "")
            uiTest.grab("15_offset_plane")
            const id = f.id
            app.finishCommand(true)
            app.createSketch("feature:" + id)
            return 900
        },
        () => {
            app.sketchTool = "circle"
            clickSketch(0, 0)
            clickSketch(0, 5)
            app.finishSketch()
            app.startCommand("Loft")
            uiTest.check((app.commandFeature.get("sections") || []).length === 0, "loft starts with nothing selected")
            return 900
        },
        () => {
            app.camera.setView("home", false)
            app.viewport.renderer.fitAll(false)
            return 800
        },
        () => {
            let picked = 0
            for (const sk of app.doc.featureList("Sketch")) {
                const spot = findPick("profile", r => r.sketch === sk.id)
                if (spot) { app.viewport.click(spot.x, spot.y, 0); picked++ }
            }
            const n = (app.commandFeature.get("sections") || []).length
            uiTest.check(picked === 2 && n === 2, "loft selection takes both profiles", picked + " picked, " + n + " sections")
            return 900
        },
        () => {
            const f = app.commandFeature
            uiTest.check(f && f.healthy && vol() > 1000, "loft dialog: square to circle on the offset plane",
                         f ? f.error + " volume " + vol().toFixed(3) : "")
            uiTest.grab("16_loft")
            app.selection.clear()
            return 500
        },
        () => {
            uiTest.check((app.commandFeature.get("sections") || []).length === 0,
                         "clearing the loft selection removes every section")
            app.finishCommand(false)
            return 400
        },

        () => { fissionApp.newDocument(); app.createSketch("XZ"); return 900 },
        () => {
            app.sketchTool = "circle"
            clickSketch(0, 0)
            clickSketch(0, 5)
            app.finishSketch()
            app.createSketch("XY")
            return 900
        },
        () => {
            app.sketchTool = "line"
            clickSketch(0, 0)
            clickSketch(0, 40)
            app.activeSketch.addArc(30, 40, 30, 90, 180)   // tangent continuation (off-grid midpoint)
            app.finishSketch()
            app.startCommand("Sweep")
            return 1000
        },
        () => {
            app.camera.setView("home", false)
            app.viewport.renderer.fitAll(false)
            const f = app.commandFeature
            uiTest.check(f && f.healthy && vol() > Math.PI * 25 * 40, "sweep dialog: circle along line + arc",
                         f ? f.error + " volume " + vol().toFixed(3) : "")
            return 700
        },
        () => { uiTest.grab("17_sweep"); app.finishCommand(true); return 400 },

        // ---- Typed dimensions while sketching
        () => { fissionApp.newDocument(); app.createSketch("XY"); return 900 },
        () => {
            const vp = app.viewport
            app.sketchTool = "line"
            clickSketch(0, 0)
            moveSketch(40, 30)
            vp.setDimLock(0, "25")
            vp.enterPressed()
            const e = app.activeSketch.props.entities[0] || {}
            uiTest.check(Math.abs(parseFloat(e.x2) - 20) < 1e-3 && Math.abs(parseFloat(e.y2) - 15) < 1e-3,
                         "typed line length 25 along the cursor direction", JSON.stringify(e))
            app.sketchTool = "rectangle"
            clickSketch(-40, -40)
            moveSketch(-20, -30)
            vp.setDimLock(0, "30")
            vp.setDimLock(1, "2 * 6")
            moveSketch(-25, -31)
            return 500
        },
        () => {
            uiTest.grab("18_typed_dimensions")
            app.viewport.enterPressed()
            const s = app.activeSketch
            const ents = s.props.entities
            const w = ents[ents.length - 4] || {}, h = ents[ents.length - 3] || {}
            const cons = s.props.constraints || []
            uiTest.check(Math.abs(Math.abs(w.x2 - w.x1) - 30) < 1e-3 && Math.abs(Math.abs(h.y2 - h.y1) - 12) < 1e-3
                         && cons.some(c => c.type === "distance" && c.value === "(2 * 6) mm"),
                         "typed rectangle 30 x (2 * 6) becomes driving dimensions", JSON.stringify([w, h]))
            app.finishSketch()
            return 400
        },

        // ---- Constraints and driving dimensions
        () => { fissionApp.newDocument(); app.createSketch("XY"); return 900 },
        () => {
            app.sketchTool = "rectangle"
            clickSketch(0, 0)
            clickSketch(30, 20)
            return 700
        },
        () => {
            const s = app.activeSketch
            uiTest.check(s.entityCount === 4 && s.constraintCount === 9 && s.degreesOfFreedom() === 2,
                         "rectangle is four constrained lines tied to the origin",
                         s.entityCount + " curves, " + s.constraintCount + " constraints, dof " + s.degreesOfFreedom())
            app.sketchTool = "dimension"
            clickSketch(15, 0)     // bottom edge — opens the length text box
            return 700
        },
        () => {
            const s = app.activeSketch, vp = app.viewport
            uiTest.check(vp.editingConstraint === 9 && s.constraint(9).type === "distance",
                         "dimension tool adds the length and opens its editor", JSON.stringify(s.constraint(9)))
            const res = s.setConstraintValue(9, "40")
            vp.editingConstraint = -1
            uiTest.check(res.ok, "editing the dimension value", res.error || "")
            return 800
        },
        () => {
            clickSketch(40, 10)    // right edge, now at x = 40
            app.activeSketch.setConstraintValue(10, "25")
            app.viewport.editingConstraint = -1
            return 800
        },
        () => {
            const s = app.activeSketch
            const e0 = s.entity(0), e1 = s.entity(1)
            uiTest.check(s.degreesOfFreedom() === 0 && Math.abs(e0.x2 - 40) < 1e-6 && Math.abs(e1.y2 - 25) < 1e-6,
                         "driving dimensions resize and fully constrain the rectangle",
                         "dof " + s.degreesOfFreedom() + " " + JSON.stringify([e0, e1]))
            app.sketchTool = "line"
            clickSketch(55, 0)
            clickSketch(75, 6)
            app.viewport.cancelSketchInput()
            return 700
        },
        () => {
            app.sketchTool = "constraint:perpendicular"
            clickSketch(65, 3)       // the free line
            clickSketch(40, 12.5)    // right edge
            return 800
        },
        () => {
            const s = app.activeSketch
            const e = s.entity(4)
            uiTest.check(Math.abs(e.y2 - e.y1) < 1e-6 && s.constraintCount === 12,
                         "perpendicular constraint picked in the view", s.constraintCount + " " + JSON.stringify(e))
            app.sketchTool = ""
            app.viewport.renderer.fitAll(false)
            return 700
        },
        () => { uiTest.grab("19_constraints"); app.finishSketch(); return 400 },

        // ---- Sketch modify tools, dragging and the origin
        () => { fissionApp.newDocument(); app.createSketch("XY"); return 900 },
        () => {
            const s = app.activeSketch
            s.addLine(0, 0, 20, 0)
            s.addLine(10, -5, 10, 5)
            app.sketchTool = "trim"
            return 600
        },
        () => { clickSketch(16, 0); return 700 },
        () => {
            const e = app.activeSketch.entity(0)
            uiTest.check(Math.abs(e.x2 - 10) < 1e-6 && Math.abs(e.x1) < 1e-6, "trim tool cuts the line at the crossing",
                         JSON.stringify(e))
            app.sketchTool = ""
            const p = sketchPx(10, 5)
            const vp = app.viewport
            uiTest.check(vp.beginSketchPress(p[0], p[1]) && vp.sdrag.kind === "point", "pressing a line end starts a drag",
                         JSON.stringify(vp.sdrag))
            const q = sketchPx(14, 8)
            vp.moveSketchDrag(q[0], q[1])
            vp.endSketchDrag()
            return 700
        },
        () => {
            const e = app.activeSketch.entity(1)
            uiTest.check(Math.abs(e.x2 - 14) < 0.05 && Math.abs(e.y2 - 8) < 0.05, "dragging moves the line end",
                         JSON.stringify(e))
            app.setOriginVisible("X", true)
            app.setOriginVisible("O", true)
            return 600
        },
        () => {
            const items = app.viewport.renderer.originItems
            uiTest.check(items.indexOf("X") >= 0 && items.indexOf("O") >= 0 && items.indexOf("XY") < 0,
                         "browser eye toggles the origin axis and point", JSON.stringify(items))
            uiTest.grab("20_trim_drag_origin")
            app.setOriginVisible("X", false)
            app.setOriginVisible("O", false)
            app.finishSketch()
            return 400
        },
        () => {
            app.setAllOriginVisible(true)
            const all = Array.from(app.viewport.renderer.originItems)
            app.setAllOriginVisible(false)
            const none = Array.from(app.viewport.renderer.originItems)
            uiTest.check(["XY", "XZ", "YZ", "X", "Y", "Z", "O"].every(k => all.indexOf(k) >= 0) && none.length === 0,
                         "Origin folder eye shows and hides every origin item", JSON.stringify([all, none]))
            return 300
        },

        // ---- Line axis snap and typed angle, spline handles, Finish Sketch view
        () => { fissionApp.newDocument(); app.createSketch("XY"); return 900 },
        () => {
            const vp = app.viewport
            app.sketchTool = "line"
            clickSketch(0, 0)
            moveSketch(30, 1)
            clickSketch(30, 1)
            const e = app.activeSketch.entity(0)
            uiTest.check(Math.abs(e.y2) < 1e-6 && Math.abs(e.x2 - 30) < 1e-3, "a nearly horizontal line snaps to the axis",
                         JSON.stringify(e))
            vp.setDimLock(1, "30")
            vp.setDimLock(0, "10")
            moveSketch(45, 6)
            vp.enterPressed()
            const f = app.activeSketch.entity(1)
            uiTest.check(Math.abs(f.x2 - 30 - 10 * Math.cos(Math.PI / 6)) < 1e-3 && Math.abs(f.y2 - 5) < 1e-3,
                         "typed angle 30 and length 10 place the next line", JSON.stringify(f))
            vp.cancelSketchInput()
            app.sketchTool = ""
            app.activeSketch.addSpline([[0, 20], [10, 30], [20, 20]])
            return 700
        },
        () => {
            const vp = app.viewport
            const p = sketchPx(10, 30), q = sketchPx(10, 40)
            uiTest.check(vp.beginSketchPress(p[0], p[1]) && vp.sdrag.kind === "spline", "pressing a spline fit point grabs it",
                         JSON.stringify(vp.sdrag))
            vp.moveSketchDrag(q[0], q[1])
            vp.endSketchDrag()
            return 700
        },
        () => {
            const ents = app.activeSketch.props.entities
            const sp = ents[ents.length - 1]
            const mid = sp.points[1]
            uiTest.check(Math.abs(parseFloat(mid[1]) - 40) < 0.1, "dragging the handle reshapes the spline", JSON.stringify(sp))
            uiTest.grab("21_line_angle_spline")
            app.camera.setView("home", false)
            t.homeYaw = app.camera.yaw
            t.homePitch = app.camera.pitch
            app.camera.setView("top", false)
            app.finishSketch()
            return 1200
        },
        () => {
            uiTest.check(Math.abs(app.camera.yaw - homeYaw) < 0.5 && Math.abs(app.camera.pitch - homePitch) < 0.5,
                         "Finish Sketch returns to the home view",
                         "yaw " + app.camera.yaw + " pitch " + app.camera.pitch + " home " + homeYaw + " " + homePitch)
            return 300
        },

        // ---- Re-editing a consumed sketch, taper knob, full preview, Mirror order, plane resize
        () => {
            fissionApp.newDocument()
            app.createSketch("XY")
            app.activeSketch.addRectangleLines(0, 0, 40, 20)
            app.finishSketch()
            app.startCommand("Extrude")
            return 900
        },
        () => {
            const vp = app.viewport
            app.commandFeature.set("distance", "10 mm")
            const s = vp.angleScreen()
            uiTest.check(!!s && vp.manip.angle.key === "taper", "extrude shows a taper knob",
                         vp.manip ? JSON.stringify(vp.manip.angle) : "no manipulator")
            if (!s) return 300
            const a = vp.manip.angle
            const target = vp.renderer.projectPoint(a.pivot.plus(a.up.times(a.height)).plus(a.out.times(a.height * 0.3)))
            uiTest.check(vp.nearAngle(s.q[0], s.q[1]) && vp.beginAngleDrag(), "the knob can be grabbed")
            vp.moveManipDrag(target[0], target[1])
            vp.endManipDrag()
            return 800
        },
        () => {
            const tp = app.doc.evaluateExpression(app.commandFeature.get("taper")).value
            uiTest.check(Math.abs(tp + 16.7) < 1, "dragging the knob outward sets a negative taper",
                         "taper " + app.commandFeature.get("taper"))
            uiTest.grab("22_extrude_taper_knob")
            app.commandFeature.set("taper", "0 deg")
            app.finishCommand(true)
            return 700
        },
        () => {
            const sk = feature("Sketch")
            uiTest.check(sk && !sk.visible, "the extruded sketch is hidden")
            app.editSketch(sk.id, false)
            uiTest.check(sk.visible && app.activeSketchId === sk.id, "re-editing shows the sketch's curves")
            return 700
        },
        () => {
            uiTest.grab("23_reedit_sketch")
            const sk = feature("Sketch")
            app.finishSketch()
            uiTest.check(!sk.visible, "finishing the re-edit hides the consumed sketch again")
            app.startCommand("Fillet")
            return 900
        },
        () => {
            const f = app.commandFeature
            const sp = findPick("edge")
            if (sp) app.viewport.click(sp.x, sp.y, 0)
            f.set("radius", "4 mm")
            return 900
        },
        () => {
            const f = app.commandFeature
            uiTest.check(f.healthy && (f.get("edges") || []).length >= 1 && app.doc.inputFeatureId === f.id,
                         "fillet previews on the picked edge", f.error)
            uiTest.grab("24_fillet_full_preview")
            app.finishCommand(false)
            app.startCommand("Mirror")
            return 900
        },
        () => {
            app.selection.select({ kind: "body", body: 0 }, false)
            return 700
        },
        () => {
            uiTest.check(app.selection.filter === "face,plane", "Mirror moves on to the plane after the body pick",
                         "filter '" + app.selection.filter + "'")
            app.finishCommand(false)
            app.doc.addFeature("ConstructionPlane", { mode: "offset", base: "XY", offset: "30 mm" })
            app.viewport.renderer.fitAll(false)
            return 900
        },
        () => {
            const vp = app.viewport
            const pl = feature("ConstructionPlane")
            const info = pl.displayInfo()
            const b = info.bounds
            const at = (u, v) => vp.renderer.projectPoint(info.origin.plus(info.x.times(u)).plus(info.y.times(v)))
            const p = at(b[1], (b[2] + b[3]) / 2), q = at(b[1] + 20, (b[2] + b[3]) / 2)
            uiTest.check(vp.beginPlaneResize(p[0], p[1]) && vp.planeDrag.side === 1, "pressing a plane's edge grabs that side",
                         JSON.stringify(vp.planeDrag ? vp.planeDrag.side : null))
            vp.movePlaneResize(q[0], q[1])
            vp.planeDrag = null
            t.planeWidth = b[1]
            return 800
        },
        () => {
            const b = feature("ConstructionPlane").displayInfo().bounds
            uiTest.check(Math.abs(b[1] - planeWidth - 20) < 0.5, "dragging the edge resizes the plane",
                         JSON.stringify(b) + " was " + planeWidth)
            uiTest.grab("25_plane_resize")
            return 400
        },

        // ---- Keyboard shortcuts (real key presses through the shortcut map) and document units
        () => { fissionApp.newDocument(); return 900 },
        () => {
            uiTest.key("L")
            uiTest.check(app.pickingSketchPlane && app.pendingSketchTool === "line", "L outside a sketch asks for a plane",
                         app.pickingSketchPlane + " " + app.pendingSketchTool)
            app.createSketch("XY")
            uiTest.check(app.activeSketchId && app.sketchTool === "line", "after the plane pick the Line tool is active",
                         "tool '" + app.sketchTool + "'")
            return 700
        },
        () => {
            const seen = []
            for (const k of ["R", "C", "T", "O", "P", "D"]) { uiTest.key(k); seen.push(app.sketchTool) }
            uiTest.check(JSON.stringify(seen) === JSON.stringify(["rectangle", "circle", "trim", "offset", "project", "dimension"]),
                         "sketch keys R C T O P D pick their tools", JSON.stringify(seen))
            uiTest.key("X")
            const on = app.sketchConstruction
            uiTest.key("X")
            uiTest.check(on && !app.sketchConstruction, "X toggles construction geometry")
            uiTest.key("Esc")
            uiTest.check(app.sketchTool === "", "Esc drops the tool")
            return 400
        },
        () => {
            app.setLengthUnit("in")
            app.sketchTool = "line"
            clickSketch(0, 0)
            moveSketch(30, 20)
            app.viewport.setDimLock(0, "2")
            app.viewport.enterPressed()
            const e = app.activeSketch.entity(0)
            const len = Math.hypot(e.x2 - e.x1, e.y2 - e.y1)
            const label = app.viewport.previewCurves().label
            uiTest.check(Math.abs(len - 50.8) < 1e-3, "in an inch design a typed 2 is 2 in", "length " + len)
            uiTest.check(label.indexOf(" in") > 0, "the sketch label shows inches", label)
            uiTest.grab("26_inch_line")
            app.viewport.cancelSketchInput()
            app.sketchTool = ""
            return 500
        },
        () => {
            app.finishSketch()
            app.doc.undo()
            uiTest.check(app.doc.lengthUnit === "in", "undo after the line keeps the unit change as its own step",
                         app.doc.lengthUnit)
            app.setLengthUnit("mm")
            return 600
        },
        () => {
            fissionApp.newDocument()
            app.createSketch("XY")
            app.activeSketch.addRectangleLines(0, 0, 30, 20)
            app.finishSketch()
            app.startCommand("Extrude")
            return 900
        },
        () => { app.finishCommand(true); return 700 },
        () => {
            const b = app.doc.bodies[0]
            app.selection.select({ kind: "body", body: 0 }, false)
            uiTest.key("V")
            const hidden = !b.visible
            app.selection.select({ kind: "body", body: 0 }, false)
            uiTest.key("V")
            uiTest.check(hidden && b.visible, "V hides and shows the selected body")
            uiTest.key("Ctrl+Alt+B")
            const browserOff = !app.showBrowser
            uiTest.key("Ctrl+Alt+B")
            uiTest.key("Ctrl+Alt+V")
            const cubeOff = !app.showViewCube
            uiTest.key("Ctrl+Alt+R")
            uiTest.check(browserOff && cubeOff && app.showBrowser && app.showViewCube,
                         "Ctrl+Alt+B / Ctrl+Alt+V toggle the browser and ViewCube; Ctrl+Alt+R resets")
            app.camera.setView("home", false)
            app.viewport.renderer.fitAll(false)
            return 700
        },
        () => {
            const sp = findPick("edge")
            uiTest.check(!!sp, "an edge can be picked for Press Pull")
            if (sp) app.selection.select(sp.ref, false)
            uiTest.key("Q")
            const f = app.commandFeature
            uiTest.check(f && f.type === "Fillet" && (f.get("edges") || []).length >= 1, "Q on a picked edge starts Fillet",
                         f ? f.type : "no command")
            uiTest.key("Esc")
            return 700
        },
        () => {
            uiTest.key("S")
            const box = app.commandSearchBox
            uiTest.check(box.opened && box.matches.length > 10, "S opens the command search", box.matches.length + " commands")
            box.search("chamf")
            uiTest.grab("27_command_search")
            uiTest.check(box.matches.length === 1 && box.matches[0].name === "Chamfer", "typing filters the commands",
                         JSON.stringify(box.matches.map(m => m.name)))
            box.run(0)
            uiTest.check(app.commandFeature && app.commandFeature.type === "Chamfer", "Enter runs the command")
            return 700
        },
        () => {
            uiTest.key("Esc")
            uiTest.key("E")
            const f = app.commandFeature
            uiTest.check(f && f.type === "Extrude", "E starts Extrude")
            uiTest.key("Esc")
            app.openDocumentSettings()
            return 600
        },
        () => {
            uiTest.check(app.documentSettingsDialog.opened, "Document Settings opens from the browser")
            uiTest.grab("28_document_settings")
            app.documentSettingsDialog.close()
            return 300
        },

        // ---- Extrude a face of a tapered body; region selection
        () => {
            fissionApp.newDocument()
            app.createSketch("XY")
            app.activeSketch.addRectangleLines(0, 0, 30, 20)
            app.finishSketch()
            app.startCommand("Extrude")
            app.commandFeature.set("taper", "40 deg")
            return 900
        },
        () => {
            app.finishCommand(true)
            t.extrudeVolume = vol()
            app.camera.setView("home", false)
            app.viewport.renderer.fitAll(false)
            return 700
        },
        () => {
            const sp = findPick("face", r => Math.abs(r.z - 10) < 0.01)
            uiTest.check(!!sp, "the top face of the tapered body can be picked")
            if (sp) app.selection.select(sp.ref, false)
            uiTest.key("E")
            const f = app.commandFeature
            const prof = f ? Array.from(f.get("profiles") || []) : []
            uiTest.check(f && f.type === "Extrude" && f.get("sketch") === "" && prof.length === 1 && prof[0].kind === "face",
                         "E on a picked face extrudes that face, with no new sketch",
                         f ? JSON.stringify(prof) : "no command")
            uiTest.check(app.doc.featureList("Sketch").length === 1, "no sketch was created for the face")
            return 900
        },
        () => {
            const f = app.commandFeature
            uiTest.grab("29_extrude_face")
            app.finishCommand(true)
            const v = vol()
            uiTest.check(f && f.healthy && app.doc.bodies.length === 1 && v > t.extrudeVolume + 100 && v < 5000,
                         "the face extrude joins onto the tapered body and keeps the taper",
                         "before " + t.extrudeVolume.toFixed(1) + ", after " + v.toFixed(1))
            app.selection.clear()
            uiTest.key("E")
            const g = app.commandFeature
            uiTest.check(g && g.get("sketch") === "", "with every sketch used, Extrude waits for a pick instead of reusing one",
                         g ? "sketch '" + g.get("sketch") + "'" : "no command")
            uiTest.key("Esc")
            return 700
        },
        () => {
            const vp = app.viewport
            const c = vp.renderer.projectPoint(Qt.vector3d(15, 10, 5))
            app.selection.clear()
            vp.beginRegion(4, 4, 0); vp.moveRegion(vp.width - 4, vp.height - 4); vp.endRegion()
            const all = Array.from(app.selection.items)
            uiTest.check(all.length === 1 && all[0].kind === "body", "a window around everything selects the body",
                         JSON.stringify(all.map(x => x.kind)))
            vp.beginRegion(c[0] - 10, c[1] - 10, 0); vp.moveRegion(c[0] + 10, c[1] + 10); vp.endRegion()
            uiTest.check(app.selection.count === 0, "a small left-to-right window inside the body selects nothing",
                         app.selection.count + " selected")
            uiTest.key("1")
            vp.beginRegion(c[0] + 10, c[1] - 10, 0)
            vp.moveRegion(c[0] - 10, c[1] + 10)
            uiTest.check(vp.regionCrossing(), "right to left is a crossing window")
            uiTest.grab("30_crossing_window")
            vp.endRegion()
            uiTest.check(app.selection.count === 1, "a right-to-left window touching the body selects it")
            return 400
        },
        () => {
            const vp = app.viewport
            uiTest.key("2")
            uiTest.check(app.selectionTool === "freeform", "2 picks freeform selection")
            app.selection.clear()
            app.camera.zoom(1.6)
            const cx = vp.width / 2, cy = vp.height / 2, rx = vp.width * 0.49, ry = vp.height * 0.49
            vp.beginRegion(cx + rx, cy, 0)
            for (let k = 1; k <= 24; ++k) {
                // A rounded square loop hugging the viewport border
                const a = k * Math.PI / 12, c = Math.cos(a), s = Math.sin(a)
                vp.moveRegion(cx + rx * Math.sign(c) * Math.pow(Math.abs(c), 0.3), cy + ry * Math.sign(s) * Math.pow(Math.abs(s), 0.3))
            }
            uiTest.grab("31_freeform")
            vp.endRegion()
            uiTest.check(app.selection.count === 1, "a freeform loop around the body selects it", app.selection.count + " selected")
            uiTest.key("3")
            app.selection.clear()
            const c = vp.renderer.projectPoint(Qt.vector3d(15, 10, 5))
            vp.beginRegion(4, c[1], 0)
            vp.moveRegion(c[0], c[1])
            vp.moveRegion(vp.width - 4, c[1])
            vp.endRegion()
            uiTest.check(app.selectionTool === "paint" && app.selection.count === 1, "painting across the body selects it",
                         app.selection.count + " selected")
            uiTest.key("1")
            return 400
        },
        () => {
            // Sketch curves by window while a sketch is open
            app.createSketch("XY")
            app.activeSketch.addRectangleLines(50, 0, 10, 10)
            app.activeSketch.addCircle(80, 5, 3)
            app.camera.setView("top", false)
            app.viewport.renderer.fitAll(false)
            return 600
        },
        () => {
            const vp = app.viewport
            const a = sketchPx(45, -5), b = sketchPx(65, 15)
            app.selection.clear()
            vp.beginRegion(a[0], b[1], 0); vp.moveRegion(b[0], a[1]); vp.endRegion()
            const items = Array.from(app.selection.items)
            uiTest.check(items.length === 4 && items.every(x => x.kind === "sketchcurve"),
                         "in a sketch, a window selects the curves fully inside (the rectangle, not the circle)",
                         JSON.stringify(items.map(x => x.kind + ":" + x.entity)))
            app.finishSketch()
            return 400
        },
        () => {
            // Ribbon tabs: every Fusion workspace tab has groups; MESH has PREPARE and EXPORT
            const tb = app.toolbar
            uiTest.check(tb.tabNames.length === 7 && tb.tabNames.every(n => (tb.tabs[n] || []).length > 0),
                         "toolbar: seven workspace tabs, each with tool groups")
            uiTest.check(tb.tabs.SOLID.map(g => g.title).join(",") === "CREATE,MODIFY,CONFIGURE,CONSTRUCT,INSPECT,INSERT,ASSEMBLE,SELECT",
                         "toolbar: SOLID groups match Fusion", tb.tabs.SOLID.map(g => g.title).join(","))
            tb.tab = "MESH"
            const mesh = tb.tabs.MESH.map(g => g.title)
            uiTest.check(mesh.indexOf("PREPARE") >= 0 && mesh.indexOf("EXPORT") >= 0, "toolbar: MESH tab has PREPARE and EXPORT",
                         mesh.join(","))
            return 500
        },
        () => {
            uiTest.grab("32_mesh_tab")
            app.toolbar.tab = "SOLID"
            const create = app.toolbar.tabs.SOLID[0].items
            const hole = create.find(i => i !== "-" && i.type === "Hole")
            uiTest.check(!!hole, "toolbar: Hole is in SOLID > CREATE")
            app.toolbar.runItem(hole)
            uiTest.check(app.commandFeature && app.commandFeature.type === "Hole", "toolbar: running Hole starts the command")
            return 600
        },
        () => {
            uiTest.grab("33_hole_dialog")
            uiTest.key("Esc")
            app.camera.setView("home", false)
            app.viewport.renderer.fitAll(false)
            const sp = findPick("edge")
            uiTest.check(!!sp, "an edge can be picked for Fillet")
            if (sp) app.selection.select(sp.ref, false)
            uiTest.key("F")
            return 600
        },
        () => {
            // Number keys go straight into the handle's value box
            uiTest.check(!!app.viewport.manip && app.digitsToManip, "fillet shows a size handle that takes typed digits")
            uiTest.key("3")
            uiTest.key("Return")
            return 600
        },
        () => {
            const f = feature("Fillet")
            uiTest.check(!app.commandFeatureId && f && /^3( mm)?$/.test(String(f.get("radius")).trim()),
                         "typing 3 then Enter sets the fillet radius without clicking the box",
                         f ? "radius '" + f.get("radius") + "'" : "no fillet")
            uiTest.key("I")
            return 600
        },
        () => {
            uiTest.check(app.inspectMode === "measure", "I opens Measure")
            app.selection.select({ kind: "body", body: 0 }, false)
            return 500
        },
        () => {
            uiTest.grab("34_measure")
            app.openInspect("section")
            app.setSectionRef("XZ")
            uiTest.check(app.sectionEnabled && app.viewport.renderer.sectionEnabled, "section analysis cuts the view")
            return 500
        },
        () => {
            uiTest.grab("35_section")
            app.openInspect("section")
            app.openInspect("zebra")
            uiTest.check(!app.sectionEnabled && app.viewport.renderer.analysis === "zebra", "zebra analysis replaces the section")
            return 500
        },
        () => {
            uiTest.grab("36_zebra")
            app.openInspect("curvature")
            return 500
        },
        () => {
            uiTest.grab("37_curvature")
            app.openInspect("draft")
            return 500
        },
        () => {
            uiTest.grab("38_draft_analysis")
            uiTest.key("Esc")
            app.analysis = ""
            uiTest.check(app.inspectMode === "", "Esc closes the inspect panel")
            app.startCommand("MeshConvert", { direction: "toMesh", bodies: [0] })
            return 600
        },
        () => {
            app.finishCommand(true)
            uiTest.check(app.doc.bodyProperties(0).mesh === true, "tessellate turns the body into a mesh body")
            app.startCommand("MeshPlaneCut", { bodies: [0], plane: "XY" })
            return 600
        },
        () => {
            uiTest.grab("39_mesh_plane_cut")
            app.finishCommand(true)
            fissionApp.newDocument()
            return 600
        },
        () => {
            // Two boxes for the section, select-all and face-fillet steps
            for (const x of [0, 30]) {
                app.createSketch("XY")
                app.activeSketch.addRectangle(x, 0, 20, 20)
                app.finishSketch()
                app.doc.addFeature("Extrude", { sketch: app.doc.featureList("Sketch").slice(-1)[0].id, distance: "15 mm",
                                                operation: "newBody" })
            }
            app.camera.setView("home", false)
            app.viewport.renderer.fitAll(false)
            return 600
        },
        () => {
            app.openInspect("section")
            const items = app.viewport.renderer.originItems
            uiTest.check(!app.sectionEnabled && ["XY", "XZ", "YZ"].every(k => items.indexOf(k) >= 0),
                         "section: opening it cuts nothing yet and shows the origin planes to pick",
                         JSON.stringify(items))
            app.setSectionRef("XZ")
            // XZ faces -Y; the boxes span y 0..20
            uiTest.check(Math.abs(app.sectionOffset + 10) < 1e-6, "section: a new cut starts through the middle of the model",
                         String(app.sectionOffset))
            return 600
        },
        () => {
            uiTest.check(app.sectionEnabled && app.sectionFrame.ok && Math.abs(app.sectionFrame.normal[1] + 1) < 1e-9,
                         "section: XZ button cuts across Y")
            app.commitSection()
            const list = app.doc.analyses
            uiTest.check(list.length === 1 && list[0].name === "Section1" && list[0].plane === "XZ" && app.sectionEnabled
                             && app.inspectMode === "",
                         "section: OK saves the cut to the browser and keeps it shown", JSON.stringify(list))
            return 900
        },
        () => {
            uiTest.grab("40_section_saved")
            uiTest.check(app.viewport.renderer.sectionCapCount() > 0, "section: the cut faces are capped",
                         String(app.viewport.renderer.sectionCapCount()))
            app.doc.updateAnalysis(app.doc.analyses[0].id, { visible: false })
            uiTest.check(!app.sectionEnabled, "section: hiding the analysis removes the cut")
            const sp = findPick("face", r => r.body === 0)
            uiTest.check(!!sp, "a face can be picked for Fillet")
            app.selection.clear()
            if (sp) app.selection.select(sp.ref, false)
            app.startCommand("Fillet")
            return 600
        },
        () => {
            const f = app.commandFeature
            const edges = f ? f.get("edges") : []
            uiTest.check(f && f.type === "Fillet" && edges.length === 4, "fillet: a picked face fillets all its edges",
                         JSON.stringify(edges))
            uiTest.grab("41_fillet_face")
            app.finishCommand(false)
            app.selection.clear()
            uiTest.key("Ctrl+A")
            return 400
        },
        () => {
            uiTest.check(app.selection.items.length === 2 && Array.from(app.selection.items).every(x => x.kind === "body"),
                         "Ctrl+A selects every body", JSON.stringify(app.selection.items))
            uiTest.key("Delete")
            return 600
        },
        () => {
            uiTest.check(app.doc.bodyCount() === 0 && app.doc.featureList("Remove").length === 1,
                         "Delete removes the selected bodies with a Remove feature", app.doc.bodyCount() + " bodies")
            app.doc.undo()
            uiTest.check(app.doc.bodyCount() === 2, "undo brings the removed bodies back")
            const r = fissionScripts.run("print(doc.bodyCount())\ndoc.addParameter('fromScript', '7 mm')\nreturn fission.version", app)
            uiTest.check(r.ok && r.output[0] === "2" && !!app.doc.parameter("fromScript") && r.result === fissionApp.version,
                         "scripts: a script drives the document", JSON.stringify(r))
            app.openScripts()
            return 700
        },
        () => {
            uiTest.grab("42_scripts")
            uiTest.key("Esc")
            app.toolbar.tab = "SHEET METAL"
            const groups = (app.toolbar.tabs["SHEET METAL"] || []).map(g => g.title)
            uiTest.check(groups.indexOf("CREATE") >= 0 && groups.indexOf("FLAT PATTERN") >= 0,
                         "toolbar: SHEET METAL has CREATE and FLAT PATTERN", groups.join(","))
            return 500
        },
        () => {
            uiTest.grab("43_sheet_metal_tab")
            app.toolbar.tab = "SOLID"
            app.selection.clear()
            app.selection.select({ kind: "body", body: 0 }, false)
            uiTest.key("M")
            return 700
        },
        () => {
            const f = app.commandFeature
            const g = app.moveGizmo
            const b = app.doc.bodyBox(0)
            const piv = f ? f.get("pivot").map(v => app.num(v)) : []
            uiTest.check(f && f.type === "Move" && f.get("moveType") === "free" && g && g.visible
                         && Math.abs(piv[0] - (b[0] + b[3]) / 2) < 1e-6 && Math.abs(piv[2] - (b[2] + b[5]) / 2) < 1e-6,
                         "move: M opens a free move with the triad at the body center", JSON.stringify(piv))
            g.begin(); g.translate([0, 0, 15]); g.end()
            g.begin(); g.rotate(2, 90); g.end()
            return 800
        },
        () => {
            const f = app.commandFeature
            const b = app.doc.bodyBox(0)
            uiTest.check(f && app.num(f.get("translate")[2]) === 15 && app.num(f.get("rotation")[2]) === 90 && b.length
                         && Math.abs(b[2] - 15) < 1e-4,
                         "move: dragging the Z arrow and the Z ring moves and turns the body",
                         JSON.stringify([f.get("translate"), f.get("rotation"), b]))
            uiTest.grab("44_move_free")
            f.set("moveType", "pointToPoint")
            return 500
        },
        () => {
            uiTest.check(!app.moveGizmo.visible, "move: point to point hides the triad")
            uiTest.grab("45_move_point_to_point")
            app.finishCommand(false)
            const s = app.doc.addFeature("Sketch", { plane: "XY" })
            app.editSketch(s.id, false)
            s.addLine(0, -30, 20, -30)
            s.addLine(20, -30, 20, -40)
            return 700
        },
        () => {
            // Like a user: click the line with the mouse, press M
            app.sketchTool = ""
            app.selection.clear()
            app.viewport.renderer.fitAll(false)
            return 500
        },
        () => {
            // Fusion order: M first, then pick the curves
            uiTest.key("M")
            return 400
        },
        () => {
            uiTest.check(app.sketchMove && app.sketchMove.entities.length === 0 && !app.moveGizmo.visible,
                         "sketch move: M with nothing selected opens Move/Copy and waits for curves")
            const p = sketchPx(10, -30)
            const w = app.viewport.mapToItem(null, p[0], p[1])
            uiTest.mouse("press", w.x, w.y)
            uiTest.mouse("release", w.x, w.y)
            return 500
        },
        () => {
            const picked = Array.from(app.selection.items).filter(x => x.kind === "sketchcurve")
            uiTest.check(picked.length === 1 && picked[0].entity === 0, "sketch move: clicking a line selects it",
                         JSON.stringify(app.selection.items))
            return 300
        },
        () => {
            const st = app.sketchMove
            uiTest.check(!!st && st.entities.length === 1 && app.moveGizmo === st.gizmo,
                         "sketch move: M in a sketch moves the selected curves", st ? JSON.stringify(st.entities) : "none")
            // Drag the X arrow of the triad with the mouse, 60 px to the right
            const tr = app.viewport.moveTriad
            const f = tr.frame()
            if (!f) { uiTest.check(false, "sketch move: the triad is shown"); return 300 }
            const tip = tr.sp(f.at(0, f.L * 0.7))
            const dir = [tip[0] - f.so[0], tip[1] - f.so[1]]
            const len = Math.hypot(dir[0], dir[1])
            const a = app.viewport.mapToItem(null, tip[0], tip[1])
            sketchDrag = { x: a.x, y: a.y, dx: dir[0] / len * 60, dy: dir[1] / len * 60 }
            uiTest.mouse("press", a.x, a.y)
            uiTest.check(tr.drag && tr.drag.handle === "arrow0", "sketch move: pressing the X arrow grabs it",
                         tr.drag ? tr.drag.handle : "nothing")
            for (let i = 1; i <= 6; ++i) uiTest.mouse("move", a.x + sketchDrag.dx * i / 6, a.y + sketchDrag.dy * i / 6)
            uiTest.mouse("release", a.x + sketchDrag.dx, a.y + sketchDrag.dy)
            return 600
        },
        () => {
            const st = app.sketchMove
            const e = app.activeSketch.entity(0)
            uiTest.check(st && st.du > 1 && Math.abs(st.dv) < 1e-6 && Math.abs(app.num(e.x1) - st.du) < 1e-6,
                         "sketch move: dragging the X arrow moves the line", st ? JSON.stringify([st.du, st.dv, e]) : "none")
            app.setSketchMove({ du: 0, dv: -5 })
            return 400
        },
        () => {
            uiTest.grab("46_sketch_move")
            uiTest.key("Return")
            const s = app.activeSketch
            const moved = s.entity(0), other = s.entity(1)
            uiTest.check(!app.sketchMove && Math.abs(app.num(moved.y1) + 35) < 1e-6 && Math.abs(app.num(other.y1) + 30) < 1e-6,
                         "sketch move: only the picked line moved", JSON.stringify([moved, other]))
            app.finishSketch()
            return 400
        },
        // --- design tabs: New opens a second design instead of replacing the first
        () => {
            tabs = { count: fissionApp.documents.length, first: app.doc, firstIndex: fissionApp.activeIndex }
            app.camera.yaw = 33
            app.newDesign()
            return 800
        },
        () => {
            uiTest.check(fissionApp.documents.length === tabs.count + 1 && app.doc !== tabs.first
                         && fissionApp.activeIndex === fissionApp.documents.length - 1 && app.doc.bodies.length === 0,
                         "tabs: New Design opens a new tab and keeps the old one",
                         fissionApp.documents.length + " / " + fissionApp.activeIndex)
            uiTest.grab("47_tabs")
            tabs.second = app.doc
            app.switchDesign(tabs.firstIndex)
            return 800
        },
        () => {
            uiTest.check(app.doc === tabs.first && app.doc.featureList("Sketch").length > 0 && Math.abs(app.camera.yaw - 33) < 1e-6,
                         "tabs: switching back restores the design and its camera", app.camera.yaw)
            app.closeDesign(fissionApp.documents.indexOf(tabs.second))
            return 600
        },
        () => {
            uiTest.check(fissionApp.documents.length === tabs.count && app.doc === tabs.first,
                         "tabs: closing an unmodified tab removes it", fissionApp.documents.length)
            return 300
        },
        // --- mesh workspace and export
        () => { fissionApp.newDocument(); return 600 },
        () => {
            for (const x of [0, 40]) {
                app.createSketch("XY")
                app.activeSketch.addRectangle(x, 0, 20, 20)
                app.finishSketch()
                app.doc.addFeature("Extrude", { sketch: app.doc.featureList("Sketch").slice(-1)[0].id, distance: "10 mm",
                                                operation: "newBody" })
            }
            app.camera.setView("home", false)
            app.viewport.renderer.fitAll(false)
            app.startCommand("MeshConvert", { direction: "toMesh", bodies: [1] })
            return 700
        },
        () => {
            app.finishCommand(true)
            const b = app.doc.bodies
            uiTest.check(b.length === 2 && !b[0].isMesh && b[1].isMesh,
                         "mesh: tessellate makes a mesh body; the browser shows the mesh icon for it only")
            app.startCommand("MeshFaceGroups", { bodies: [1] })
            return 900
        },
        () => {
            uiTest.check(app.viewport.renderer.meshDisplay === "faceGroups", "mesh: Generate Face Groups shows the face groups",
                         app.viewport.renderer.meshDisplay)
            uiTest.grab("48_face_groups")
            app.finishCommand(true)
            app.viewport.renderer.meshDisplay = "shadedEdges"
            const path = uiTest.dir + "/ui_export.3mf"
            uiTest.check(fissionApp.exportDocument(path), "export: the design exports as 3MF", app.doc.lastError())
            return 500
        },
        () => {
            app.selection.clear()
            const hit = findPick("face", r => r.body === 0)
            uiTest.check(!!hit, "export: a planar face of the solid can be picked")
            if (hit) {
                app.selection.select(hit.ref, false)
                uiTest.check(app.selectedOf("face").length === 1 && app.exportFaceDxf(hit.ref, uiTest.dir + "/ui_face.dxf"),
                             "export: the picked face saves as DXF", app.doc.lastError())
            }
            app.openBatchExport()
            return 600
        },
        () => {
            const d = app.batchExportDialog
            uiTest.check(d.opened && d.picked.length === 2, "batch export: the dialog opens with the visible bodies ticked",
                         JSON.stringify(d.picked))
            uiTest.grab("49_batch_export")
            d.folder = uiTest.dir + "/batch"
            d.formats = ["stl", "3mf"]
            d.separate = true
            const files = d.run()
            uiTest.check(files.length === 4, "batch export: two bodies in two formats write four files", JSON.stringify(files))
            d.close()
            return 400
        },

        // ---- Center rectangle and center line
        () => { fissionApp.newDocument(); app.createSketch("XY"); return 900 },
        () => {
            app.sketchTool = "centerrectangle"
            clickSketch(0, 0)
            clickSketch(15, 10)
            app.sketchTool = "centerline"
            clickSketch(40, 0)
            clickSketch(50, 5)
            return 700
        },
        () => {
            const s = app.activeSketch
            const bottom = s.entity(0), diag = s.entity(4), centre = s.entity(5)
            uiTest.check(s.entityCount === 8 && Math.abs(Number(bottom.x1) + 15) < 1e-6 && Math.abs(Number(bottom.y1) + 10) < 1e-6
                         && Math.abs(bottom.x2 - 15) < 1e-6 && diag.construction && Math.abs(centre.x) < 1e-6,
                         "center rectangle: corners mirror about the clicked center, with a construction diagonal",
                         JSON.stringify([bottom, diag, centre]))
            const line = s.entity(6), mid = s.entity(7)
            uiTest.check(Math.abs(line.x1 - 30) < 1e-6 && Math.abs(Number(line.y1) + 5) < 1e-6 && Math.abs(line.x2 - 50) < 1e-6
                         && Math.abs(mid.x - 40) < 1e-6 && Math.abs(mid.y) < 1e-6,
                         "center line: runs the same length either side of its midpoint", JSON.stringify([line, mid]))
            uiTest.check(s.degreesOfFreedom() === 6 && s.profileCount === 1,
                         "center tools: rectangle keeps only width and height free; the line keeps center, length and angle",
                         "dof " + s.degreesOfFreedom() + ", profiles " + s.profileCount)
            uiTest.grab("50_center_tools")
            app.finishSketch()
            return 400
        },

        // ---- Projects folder and panel
        () => { app.leaveDesign(); fissionApp.openDemo(); return 1500 },
        () => {
            app.doc.name = "Center Tools"
            uiTest.check(projects.root.indexOf("f360ui") >= 0, "projects: the walkthrough uses its own projects folder", projects.root)
            uiTest.check(app.saveInto(projects.root) && projects.thumbnail(projects.root + "/Center Tools.f360").indexOf("data:image/png;base64,") === 0,
                         "projects: saving into the projects folder stores a thumbnail", app.doc.lastError())
            const sub = projects.makeFolder(projects.root, "Fixtures")
            uiTest.check(sub.length > 0 && projects.folders().length === 2, "projects: a sub folder is created",
                         JSON.stringify(projects.folders()))
            app.openProjects()
            return 900
        },
        () => {
            const p = app.projectsPanel
            uiTest.check(p.opened && p.items.length === 2 && p.items[0].folder && p.items[1].name === "Center Tools",
                         "projects: the panel lists folders first, then designs", JSON.stringify(p.items.map(i => i.name)))
            uiTest.grab("51_projects_panel")
            const moved = p.moveTo(p.items[1], projects.root + "/Fixtures")
            uiTest.check(moved.length > 0 && app.doc.filePath.replace(/\\/g, "/") === moved,
                         "projects: moving an open design into a sub folder follows it in the open tab", app.doc.filePath + " vs " + moved)
            const dup = projects.duplicate(moved)
            uiTest.check(dup.length > 0 && projects.entries(projects.root + "/Fixtures").length === 2,
                         "projects: duplicate makes a copy next to it", dup)
            uiTest.check(projects.exportDesign(dup, uiTest.dir + "/from_projects.step"), "projects: a design exports straight from the panel",
                         projects.lastError())
            const renamed = projects.rename(dup, "Center Tools v2")
            uiTest.check(renamed.endsWith("/Center Tools v2.f360"), "projects: rename keeps the .f360 suffix", renamed)
            p.folder = projects.root + "/Fixtures"
            p.reload()
            return 700
        },
        () => {
            const p = app.projectsPanel
            uiTest.check(p.items.length === 2 && p.crumbs().length === 2, "projects: browsing into the sub folder shows its designs and a breadcrumb",
                         JSON.stringify(p.items.map(i => i.name)))
            uiTest.grab("52_projects_subfolder")
            uiTest.check(projects.remove(projects.root + "/Fixtures/Center Tools v2.f360") && p.items.length === 1,
                         "projects: delete removes the design from the list", projects.lastError())
            p.close()
            return 400
        },

        // ---- Sheet metal (phase 7)
        () => { app.leaveDesign(); fissionApp.newDocument(); return 1200 },
        () => {
            const sm = app.toolbar.tabs["SHEET METAL"] || []
            const items = [].concat(...sm.map(g => g.items)).filter(i => i && i !== "-")
            uiTest.check(!items.some(i => i.planned) && ["Contour Flange", "Hem", "Tab", "Bend", "Unfold", "Refold", "Export DXF"]
                             .every(n => items.some(i => i.text === n)),
                         "toolbar: every SHEET METAL command is available", items.map(i => i.text + (i.planned ? "*" : "")).join(","))
            const s = app.doc.addFeature("Sketch", { plane: "XZ" })
            s.addLine(0, 0, 0, 25)
            s.addLine(0, 0, 40, 0)
            s.addLine(40, 0, 40, 25)
            app.doc.rebuild()
            const cf = app.doc.addFeature("ContourFlange", { sketch: s.id, distance: "30 mm" })
            const fp = app.doc.addFeature("FlatPattern", {})
            uiTest.check(cf.healthy && fp.healthy && app.doc.bodyCount() === 2, "sheet metal: contour flange and its flat pattern",
                         cf.error + " " + fp.error)
            app.toolbar.tab = "SHEET METAL"
            app.viewport.renderer.fitAll(false)
            return 900
        },
        () => {
            uiTest.grab("53_sheet_metal")
            app.openSheetMetalRules()
            return 600
        },
        () => {
            const d = app.sheetRulesPopup
            uiTest.check(d.opened && d.library.length >= 4, "sheet metal: the rule library lists presets", d.library.length)
            uiTest.grab("54_sheet_rules")
            d.applyRule(d.library.find(r => r.name === "Steel 2 mm"))
            const cf = app.doc.featureList("ContourFlange")[0]
            uiTest.check(app.doc.parameter("SheetThickness").expression === "2 mm" && app.doc.featureById(cf.id).healthy,
                         "sheet metal: applying a library rule rebuilds the part", app.doc.parameter("SheetThickness").expression)
            d.close()
            uiTest.check(app.writeFlatPatternDxf(uiTest.dir + "/flat_pattern.dxf"), "sheet metal: flat pattern exports as DXF",
                         app.doc.lastError())
            return 500
        },

        // ---- Add-ins (phase 8)
        () => {
            const ids = fissionScripts.commands.map(c => c.id)
            uiTest.check(ids.indexOf("sample.bodyCount") >= 0, "add-ins: the sample add-in ran at startup", JSON.stringify(ids))
            const addIns = (app.toolbar.tabs["UTILITIES"] || []).find(g => g.title === "ADD-INS")
            uiTest.check(addIns && addIns.items.some(i => i.text === "Body Count"), "add-ins: its command is a toolbar button",
                         addIns ? addIns.items.map(i => i.text).join(",") : "no group")
            const r = app.runAddInCommand("sample.bodyCount")
            uiTest.check(r.ok && fissionApp.statusMessage.indexOf("2 bodies") === 0, "add-ins: clicking the button runs it on the active design",
                         JSON.stringify(r) + " " + fissionApp.statusMessage)
            if (fissionScripts.pythonStatus.indexOf("Python 3") === 0) {
                uiTest.check(ids.indexOf("sample.volume") >= 0, "add-ins: the Python sample add-in ran at startup", JSON.stringify(ids))
                const p = app.runAddInCommand("sample.volume")
                uiTest.check(p.ok && fissionApp.statusMessage.indexOf("Total volume") === 0 && fissionApp.statusMessage.indexOf("volume: 0 ") < 0,
                             "add-ins: a Python command runs on the active design",
                             JSON.stringify(p) + " " + fissionApp.statusMessage)
            }
            app.toolbar.tab = "UTILITIES"
            app.openScripts()
            app.scriptsPopup.mode = "addins"
            return 700
        },
        () => {
            const d = app.scriptsPopup
            uiTest.check(d.addInMode && d.current.length > 0 && d.names.indexOf(d.current) >= 0,
                         "add-ins: the Add-Ins tab lists and opens the add-ins", d.current)
            uiTest.grab("55_addins")
            d.close()
            app.toolbar.tab = "SOLID"
            return 400
        },

        // ---- Progress bar in the status bar
        () => {
            uiTest.check(!fissionApp.busy, "progress: idle before any task", String(fissionApp.taskCount))
            fissionApp.beginTask("uitest", "Testing progress")
            return 300
        },
        () => {
            const bar = app.statusBar.progressBar
            uiTest.check(fissionApp.busy && bar.visible && fissionApp.progressValue < 0,
                         "progress: a task without a known length shows a sweeping bar",
                         fissionApp.progressText + " " + fissionApp.progressValue)
            fissionApp.reportProgress("uitest", 0.5, "Halfway there")
            return 300
        },
        () => {
            uiTest.check(fissionApp.progressText.indexOf("Halfway there 50%") === 0 && Math.abs(fissionApp.progressValue - 0.5) < 1e-9,
                         "progress: the bar and text follow the reported fraction", fissionApp.progressText)
            uiTest.grab("56_progress")
            fissionApp.beginTask("uitest2", "Second task")
            return 200
        },
        () => {
            uiTest.check(fissionApp.taskCount === 2 && fissionApp.progressText.indexOf("(+1 more)") > 0,
                         "progress: several tasks at once are counted", fissionApp.progressText)
            fissionApp.endTask("uitest2")
            fissionApp.endTask("uitest")
            return 300
        },
        () => {
            uiTest.check(!fissionApp.busy && !app.statusBar.progressBar.visible, "progress: the bar goes away when the tasks end",
                         String(fissionApp.taskCount))
            return 200
        },
        () => {
            app.finishCreateDrawing({
                standard: "ISO", sheetSize: "A3", orientation: "landscape",
                projectionAngle: "third", partName: "UiTest Part", drawnBy: "UiTest"
            })
            return 400
        },
        () => {
            uiTest.check(app.inDrawing && !!app.activeDrawing && app.activeDrawing.sheetCount === 1,
                         "drawing: Create Drawing opens a sheet", String(!!app.activeDrawing))
            uiTest.check(app.toolbar.tab === "DRAWING" && app.drawingWorkspace.visible,
                         "drawing: the DRAWING tab and workspace are shown", app.toolbar.tab)
            return 300
        },
        () => {
            const sheet = app.activeDrawing.sheet(0)
            const v = sheet.addView("base", { position: Qt.point(80, 60), orientation: "front", scale: 1 })
            fissionApp.drawingManager.projectView(v, app.doc, app.activeDrawing)
            uiTest.check(sheet.viewCount === 1 && v.projected, "drawing: a base view can be placed on the sheet",
                         String(sheet.viewCount) + " projected=" + v.projected)
            app.drawingWorkspace.selectedView = v
            const p = sheet.addView("projected", {
                position: Qt.point(220, 60), parentViewId: v.id, projectDirection: "right"
            })
            fissionApp.drawingManager.projectView(p, app.doc, app.activeDrawing)
            sheet.addAnnotation({ kind: "text", x: 20, y: 20, text: "UiTest", fontSize: 4 })
            uiTest.check(sheet.viewCount === 2 && sheet.annotationCount >= 1,
                         "drawing: projected view and a note", String(sheet.viewCount))
            uiTest.grab("57_drawing")
            return 400
        },
        () => {
            app.leaveDrawing()
            uiTest.check(!app.inDrawing && app.toolbar.tab === "SOLID", "drawing: Back to Design returns to the model",
                         app.toolbar.tab)
            return 200
        }
    ]
    // FISSION_UITEST_ONLY=<name> runs one of these short scripts instead of the whole walkthrough
    readonly property string only: uiTest.env("FISSION_UITEST_ONLY")
    readonly property var quickSteps: ({
        panels: [
            () => {
                fissionApp.newDocument()
                const s = app.doc.addFeature("Sketch", { plane: "XY" })
                s.addRectangle(0, 0, 40, 20)
                s.addCircle(10, 10, 4)
                app.doc.rebuild()
                app.doc.addFeature("Extrude", { sketch: s.id, distance: "10 mm", operation: "newBody" })
                return 800
            },
            () => {
                app.finishCreateDrawing({ standard: "ISO", sheetSize: "A3", orientation: "landscape",
                                          projectionAngle: "third", partName: "Dims", drawnBy: "UiTest" })
                return 500
            },
            () => {
                // Top view of a 40 x 20 plate with a 8 mm hole at (10, 10), scale 2, its top left at (60, 60):
                // left edge x 60, right edge x 140, top edge y 60, hole centre (80, 80), radius 8 on the sheet
                const mgr = fissionApp.drawingManager, ws = app.drawingWorkspace
                const sheet = app.activeDrawing.sheet(0)
                for (let i = sheet.viewCount - 1; i >= 0; --i) sheet.removeView(sheet.view(i).id)
                const v = sheet.addView("base", { position: Qt.point(60, 60), orientation: "top", scale: 2 })
                mgr.projectView(v, app.doc, app.activeDrawing)
                const left = mgr.pickSheetEdge(sheet, 60, 85, 1), right = mgr.pickSheetEdge(sheet, 140, 85, 1)
                const top = mgr.pickSheetEdge(sheet, 110, 60, 1)
                const hole = mgr.pickSheetEdge(sheet, 80 + 8 * Math.SQRT1_2, 80 - 8 * Math.SQRT1_2, 1)
                uiTest.check(left.kind === "line" && right.kind === "line" && top.kind === "line" && hole.kind === "circle",
                             "dims: view edges can be picked as lines and a circle",
                             [left.kind, right.kind, top.kind, hole.kind].join(","))
                const made = [mgr.buildDimension("dimension", left, right, 100, 45, 2),
                              mgr.buildDimension("dimension", hole, {}, 100, 100, 2),
                              mgr.buildDimension("radius", hole, {}, 60, 110, 2),
                              mgr.buildDimension("angular", left, top, 70, 70, 2),
                              mgr.buildDimension("dimension", top, {}, 110, 40, 2)]
                const labels = made.map(a => a.error ? "error: " + a.error : mgr.dimensionLabel(a))
                uiTest.check(labels.join(" | ") === "40 | Ø8 | R4 | 90° | 40",
                             "dims: distance between lines, diameter, radius, angle, line length", labels.join(" | "))
                const picksOf = [[left, right], [hole], [hole], [left, top], [top]]
                const ids = made.map((a, i) => a.error ? "" : sheet.addAnnotation(mgr.attachDimension(sheet, a, picksOf[i])))
                const first = () => { for (let i = 0; i < sheet.annotationCount; ++i) if (sheet.annotation(i).id === ids[0]) return sheet.annotation(i) }
                uiTest.check(first().space === "view" && first().view === v.id, "dims: dimensions are stored on their view",
                             JSON.stringify(first().space))
                const before = mgr.dimensionGeometry(sheet, first()).text.x
                v.position = Qt.point(80, 60)
                const after = mgr.dimensionGeometry(sheet, first()).text.x
                uiTest.check(Math.abs(after - before - 20) < 1e-6, "dims: moving the view moves its dimensions",
                             before + " -> " + after)
                mgr.projectView(v, app.doc, app.activeDrawing)
                const again = ids.map(id => { for (let i = 0; i < sheet.annotationCount; ++i) if (sheet.annotation(i).id === id) return sheet.annotation(i) })
                uiTest.check(again.every(a => !a.detached) && mgr.dimensionLabel(again[0]) === "40" && mgr.dimensionLabel(again[1]) === "Ø8",
                             "dims: projecting the view again re-attaches them with the same values",
                             again.map(a => mgr.dimensionLabel(a) + (a.detached ? "!" : "")).join(" "))
                ws.toggleTitleBlock()
                uiTest.check(!sheet.titleBlockVisible && app.doc.modified, "drawing: the title block can be deleted (and the design is unsaved)")
                ws.toggleTitleBlock()
                const note = sheet.addAnnotation({ kind: "text", x: 30, y: 30, text: "Note", fontSize: 3.5 })
                ws.selectedNoteId = note
                ws.setAnn({ text: "Edited note", fontSize: 7, bold: true })
                uiTest.check(ws.selectedAnn && ws.selectedAnn.text === "Edited note" && ws.selectedAnn.bold
                             && ws.selectedAnn.fontSize === 7, "drawing note: text, size and bold are saved and re-read",
                             JSON.stringify(ws.selectedAnn))
                const tmp = uiTest.env("TEMP")
                uiTest.check(mgr.exportPdf(sheet, tmp + "/fission_uitest_dims.pdf")
                             && mgr.exportDxf(sheet, tmp + "/fission_uitest_dims.dxf"),
                             "dims: the sheet exports to PDF and DXF", mgr.lastError)
                return 700
            },
            () => { uiTest.grab("dims"); return 300 },
            () => { app.leaveDrawing(); app.openManufacture(0); return 800 },
            () => { uiTest.grab("manufacture"); return 300 },
            () => {
                app.enterDesignWorkspace()
                uiTest.check(!app.showManufacture, "manufacture: leaving for Design closes its panel")
                return 300
            }
        ]
    })
    // WebEngine is not loaded at startup; the McMaster dialog must still bring it up on first use
    readonly property var mcmasterSteps: [
        () => { fissionApp.newDocument(); app.insertMcMaster(); return 6000 },
        () => {
            const l = app.mcmasterInsertDialog
            uiTest.check(l.status === Loader.Ready && !!l.item, "mcmaster: the browser dialog (WebEngine) loads on demand",
                         "status " + l.status)
            uiTest.grab("mcmaster")
            return 300
        }
    ]
    // Shell started with nothing selected (it crashed in 1.13.39), on FISSION_UITEST_FILE or a plain box
    readonly property var shellSteps: [
        () => {
            const f = uiTest.env("FISSION_UITEST_FILE")
            if (f.length) fissionApp.openDocument(f)
            else {
                fissionApp.newDocument()
                const s = app.doc.addFeature("Sketch", { plane: "XY" })
                s.addRectangle(0, 0, 40, 20)
                app.doc.rebuild()
                app.doc.addFeature("Extrude", { sketch: s.id, distance: "10 mm", operation: "newBody" })
            }
            return 1500
        },
        () => { app.selection.clear(); app.startCommand("Shell"); return 2500 },
        () => {
            const f = app.doc.featureById(app.commandFeatureId)
            uiTest.check(!!f, "shell: the command starts with nothing picked", f ? f.error : "no feature")
            uiTest.grab("shell")
            app.finishCommand(false)
            return 500
        }
    ]
    // Open / save timing on FISSION_UITEST_FILE: opens it, saves a copy to FISSION_UITEST_SAVE, opens the copy
    readonly property var openSteps: [
        () => {
            let t = uiTest.ms()
            const ok = fissionApp.openDocument(uiTest.env("FISSION_UITEST_FILE"))
            uiTest.perf("open", uiTest.ms() - t, String(ok))
            t = uiTest.ms()
            const saved = app.doc.save(uiTest.env("FISSION_UITEST_SAVE"))
            uiTest.perf("save copy", uiTest.ms() - t, String(saved))
            t = uiTest.ms()
            const again = fissionApp.openDocument(uiTest.env("FISSION_UITEST_SAVE"))
            uiTest.perf("open copy", uiTest.ms() - t, String(again))
            uiTest.check(ok && saved && again, "open: the file opens, saves and opens again")
            return 300
        }
    ]
    readonly property var script: only === "open" ? openSteps : only === "shell" ? shellSteps : only === "mcmaster" ? mcmasterSteps
                                  : only.length && quickSteps[only] ? quickSteps[only] : steps

    property var tabs: null
    property var sketchDrag: null
    property real homeYaw: 0
    property real homePitch: 0
    property real planeWidth: 0

    function next() {
        if (step >= script.length) {
            // Animated GIF of the walkthrough screenshots (optional; keep maxWidth modest for low-end PCs)
            if (script === steps) uiTest.exportGif("walkthrough.gif", [], 40, 720)
            uiTest.finish()
            return
        }
        let delay = 600
        try {
            delay = script[step]() || 600
        } catch (e) {
            uiTest.check(false, "step " + step + " threw", String(e))
        }
        ++step
        timer.interval = delay
        timer.start()
    }

    Timer { id: timer; interval: 1500; onTriggered: t.next() }
    Component.onCompleted: timer.start()
}
