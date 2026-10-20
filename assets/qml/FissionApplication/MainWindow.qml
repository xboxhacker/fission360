// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import Fission.Render
import Fission.UI
import "."
import "../FissionFeatures"
import "../FissionSketch"
import "../FissionDrawing"
import "../FissionRender"

ApplicationWindow {
    id: win
    width: 1600
    height: 960
    minimumWidth: 1000
    minimumHeight: 640
    visible: true
    color: Theme.window
    title: (doc ? doc.name + (doc.modified ? " *" : "") : "") + " \u2014 Fission 360"
    font.family: Theme.fontFamily
    font.pixelSize: Theme.fontSize

    property var doc: fissionApp.activeDocument
    property int rev: 0                      // bumped after every rebuild so id lookups re-evaluate

    // Document units (geometry stays in mm; these only change display and bare-number inputs)
    readonly property string lengthUnit: doc ? doc.lengthUnit : "mm"
    readonly property real lengthScale: doc ? (doc.lengthUnit, doc.lengthScale()) : 1
    function lenValue(mm, decimals) {
        const k = Math.pow(10, decimals === undefined ? 2 : decimals)
        return Math.round(mm / lengthScale * k) / k
    }
    function lenText(mm, decimals) { return lenValue(mm, decimals) + " " + lengthUnit }
    // "length" for distances, "angle" for angles, "" for plain numbers (counts)
    function inputKind(key) {
        if (/angle|taper/i.test(key)) return "angle"
        if (/^count/.test(key)) return ""
        return "length"
    }
    function withUnit(text, kind) { return doc ? doc.withUnit(text, kind) : text }
    function setLengthUnit(u) {
        if (!doc || u === doc.lengthUnit) return
        doc.checkpoint()
        doc.lengthUnit = u
        fissionApp.showMessage("Document units: " + u)
    }

    // Feature command (create or edit) shown in the right-hand panel
    property string commandFeatureId: ""
    property bool commandIsNew: false
    readonly property var commandFeature: (rev, doc && commandFeatureId) ? doc.featureById(commandFeatureId) : null
    property string lastCommand: ""
    property var lastCommandProps: ({})

    // Sketch editing
    property string activeSketchId: ""
    readonly property var activeSketch: (rev, doc && activeSketchId) ? doc.featureById(activeSketchId) : null
    property int sketchMarkerRestore: -1
    property bool sketchWasHidden: false     // a hidden (consumed) sketch is shown while it is edited
    property string sketchTool: ""
    property bool sketchConstruction: false
    property int polygonSides: 6
    property bool pickingSketchPlane: false
    property string sketchFilletRadius: "2 mm"
    property string sketchOffsetDistance: "2 mm"

    // Origin geometry shown in the view (browser eyes). Planes, axes and the point also appear on their own
    // while a plane / axis / point input is waiting for a pick.
    property var originVisible: ({ O: false, X: false, Y: false, Z: false, XY: false, XZ: false, YZ: false })
    function setOriginVisible(id, on) {
        const v = Object.assign({}, originVisible)
        v[id] = on
        originVisible = v
    }
    function setAllOriginVisible(on) {
        const v = {}
        for (const k of ["O", "X", "Y", "Z", "XY", "XZ", "YZ"]) v[k] = on
        originVisible = v
    }

    property bool showProperties: false
    property bool showBrowser: true
    property bool showViewCube: true
    property bool showNavBar: true
    // Appearance ▸ Zoom to cursor: wheel zooms toward the mouse pointer (persisted)
    property bool zoomToCursor: false
    property string pendingSketchTool: ""    // sketch tool chosen by key before a sketch plane was picked
    property string selectionTool: "window"  // left-drag in empty space: "window" | "freeform" | "paint"
    property var panelPos: null              // floating command panel position, shared by all commands
    property alias camera: cameraController
    property alias selection: selectionManager
    property alias viewport: viewportItem
    property alias toolbar: toolbarItem
    property alias commandSearchBox: commandSearch
    property alias documentSettingsDialog: documentSettings

    // Workspace: "design" (SOLID/MESH/…) | "render" (under DESIGN ▾) | drawing via activeDrawingId
    property string workspace: "design"
    readonly property bool inRenderWorkspace: workspace === "render" && !inDrawing
    property bool showAppearance: false
    property bool showRender: false
    property bool showManufacture: false
    property int manufactureTab: 0
    property bool liveRender: false
    // Sketch: free clicks snap to grid when true (endpoint snaps always win). Persisted.
    property bool sketchSnapToGrid: !!fissionApp.setting("sketch/snapToGrid", true)

    // Phase 9 Drawing workspace (null = design / mesh / etc.)
    property string activeDrawingId: ""
    readonly property var activeDrawing: activeDrawingId.length
        ? fissionApp.drawingManager.drawingById(activeDrawingId) : null
    readonly property bool inDrawing: !!activeDrawing

    function createDrawingFromDesign() {
        if (!doc) return
        createDrawingDialog.partName = doc.name
        createDrawingDialog.titleText = doc.name
        createDrawingDialog.open()
    }
    function finishCreateDrawing(opts) {
        if (!doc) return
        leaveDesign()
        const input = fissionApp.drawingManager.createDrawingInput(doc.documentId)
        Object.keys(opts || {}).forEach(k => { input[k] = opts[k] })
        input.contentRevision = doc.contentRevision
        const id = fissionApp.drawingManager.createDrawingDocument(input)
        if (!id) { fissionApp.showMessage("Could not create a drawing"); return }
        const drawing = fissionApp.drawingManager.drawingById(id)
        if (drawing) drawing.setSourceDesignRevision(doc.contentRevision)
        doc.linkAsset("drawing", id, (drawing && drawing.name) || "Drawing")
        activeDrawingId = id
        workspace = "design"
        toolbar.tab = "DRAWING"
        fissionApp.showMessage("Drawing created from " + doc.name + " (linked)")
    }
    function updateActiveDrawingFromDesign() {
        if (!doc || !activeDrawing) return
        const n = fissionApp.drawingManager.updateDrawingFromDesign(activeDrawing, doc)
        fissionApp.showMessage(n ? ("Drawing updated from design (" + n + " view(s))") : "Drawing update: no views regenerated")
    }
    function revertDesignToVersion(versionNumber) {
        if (!doc) return
        if (!doc.revertToVersion(versionNumber))
            fissionApp.showMessage(doc.lastError || "Revert failed")
        else
            fissionApp.showMessage("Reverted to version " + versionNumber + " — linked drawings may be out of date")
    }
    function leaveDrawing() {
        if (!inDrawing) return
        activeDrawingId = ""
        if (toolbar.tab === "DRAWING") toolbar.tab = "SOLID"
    }
    function enterDesignWorkspace() {
        leaveDrawing()
        workspace = "design"
        if (liveRender) applyLiveRenderLook(false)
        liveRender = false
        showRender = false
        if (toolbar.tab === "RENDER" || toolbar.tab === "MANUFACTURE") toolbar.tab = "SOLID"
    }
    function enterManufactureWorkspace() {
        leaveDrawing()
        if (liveRender) applyLiveRenderLook(false)
        liveRender = false
        showRender = false
        workspace = "manufacture"
        toolbar.tab = "MANUFACTURE"
        showManufacture = true
    }
    function openManufacture(tab) {
        enterManufactureWorkspace()
        if (tab !== undefined && tab !== null) manufactureTab = tab
        showManufacture = true
    }
    function mfgNewSetup() {
        openManufacture(0)
        if (!doc) return
        const id = doc.addMfgItem("setups", {})
        fissionApp.showMessage(id ? "Setup created — stock from body bounds" : (doc.lastError || "Setup failed"))
    }
    function mfgRefreshStock() {
        openManufacture(0)
        if (!doc) return
        const ok = doc.refreshMfgStock()
        fissionApp.showMessage(ok ? "Stock refreshed from body bounds" : (doc.lastError || "Refresh failed"))
    }
    function mfgPickTool(type) {
        const tools = (doc && doc.manufacture) ? (doc.manufacture.tools || []) : []
        const prefer = (type.indexOf("drill") >= 0 || type.indexOf("bore") >= 0) ? "drill"
                     : (type.indexOf("tap") >= 0) ? "tap"
                     : (type.indexOf("turn") >= 0) ? "turning"
                     : (type.indexOf("probe") >= 0) ? "probe"
                     : (type === "face" || type === "flat") ? "facemill"
                     : "endmill"
        for (const t of tools) if (t.type === prefer) return t.id
        return tools.length ? tools[0].id : ""
    }
    function mfgAddOp(type, name) {
        openManufacture(2)
        if (!doc) return
        const mfg = doc.manufacture || {}
        if (!mfg.activeSetupId) {
            fissionApp.showMessage("Create a New Setup first")
            manufactureTab = 0
            return
        }
        const id = doc.addMfgItem("operations", {
            type: type, name: name || type, setupId: mfg.activeSetupId,
            toolId: mfgPickTool(type), enabled: true, folder: ""
        })
        fissionApp.showMessage(id ? (name + " added") : (doc.lastError || "Operation failed"))
    }
    function mfgSimulate() {
        openManufacture(3)
        if (!doc) return
        const report = doc.simulateMfgReport()
        fissionApp.showMessage(report.split("\n")[0] || report)
        if (manufactureLoader.item)
            manufactureLoader.item.simulateText = report
    }
    function mfgPostProcess() {
        openManufacture(3)
        mfgPostDialog.open()
    }
    function enterRenderWorkspace() {
        leaveDrawing()
        showManufacture = false
        workspace = "render"
        liveRender = true
        toolbar.tab = "RENDER"
        applyLiveRenderLook(true)
        fissionApp.showMessage("Live Render — sun / HDRI shading in the design viewport (no file save).")
    }
    // Stronger sun + HDRI (when cached) so Live Render looks different from the CAD view
    function applyLiveRenderLook(on) {
        if (!doc || !doc.renderSettings) return
        const rs = doc.renderSettings
        if (on) {
            rs.useSunLight = true
            rs.lightIntensity = Math.max(rs.lightIntensity, 1.55)
            rs.lightAltitude = Math.max(rs.lightAltitude, 42)
            let usedHdri = false
            if (renderAssets) {
                const hdris = renderEnvironments ? renderEnvironments.polyHavenHdris() : []
                for (let i = 0; i < hdris.length; ++i) {
                    const st = renderAssets.statusForHdri(hdris[i].id)
                    if (st && st.bestPath) {
                        rs.sceneBackgroundType = "environment"
                        rs.environmentPath = st.bestPath
                        rs.environmentIntensity = Math.max(rs.environmentIntensity, 1.0)
                        usedHdri = true
                        break
                    }
                }
            }
            if (!usedHdri) {
                rs.sceneBackgroundType = "gradient"
                rs.backgroundColor = "#d8dde3"
                rs.environmentPath = ""
            }
        } else {
            // Back to a flat CAD look (keep any downloaded HDRI path for Render Settings)
            rs.sceneBackgroundType = "solid"
            rs.backgroundColor = "#ffffff"
            rs.lightIntensity = 1.0
            rs.lightAltitude = 30
        }
        if (viewportItem && viewportItem.renderer)
            Qt.callLater(() => viewportItem.renderer.update())
    }
    function ensureLiveRenderLighting() { applyLiveRenderLook(true) }
    function setSketchSnapToGrid(on) {
        sketchSnapToGrid = !!on
        fissionApp.setSetting("sketch/snapToGrid", sketchSnapToGrid)
        if (viewport && viewport.renderer)
            viewport.renderer.snapToGrid = sketchSnapToGrid
    }

    ViewportController { id: cameraController }
    SelectionManager { id: selectionManager }

    Connections {
        target: win.doc
        function onRebuilt() { win.rev++ }
        function onContentRevisionChanged() {
            if (win.doc && fissionApp.drawingManager)
                fissionApp.drawingManager.refreshDrawingSync(win.doc)
        }
    }
    onDocChanged: {
        Qt.callLater(checkLinkedDesigns)
        commandFeatureId = ""
        activeSketchId = ""
        sketchTool = ""
        pickingSketchPlane = false
        selection.filter = ""
        selection.clear()
        rev++
        inspectMode = ""
        syncSections()
        const view = doc ? designViews[doc.objectName] : undefined
        if (view) {
            camera.yaw = view.yaw; camera.pitch = view.pitch; camera.distance = view.distance
            camera.target = view.target; camera.orthographic = view.ortho
        } else {
            Qt.callLater(() => viewportItem.renderer.fitAll(false))
        }
    }

    // ------------------------------------------------------------------ design tabs

    // Camera of each open design (by document objectName), restored when its tab comes back
    property var designViews: ({})
    // Finishes what is being edited in the current design before another tab takes over
    function leaveDesign() {
        if (!doc) return
        if (pointPick) pointPick = null
        if (sketchMove) finishSketchMove(true)
        if (commandFeatureId) finishCommand(true)
        if (activeSketchId) finishSketch()
        if (inspectMode) closeInspect()
        showRender = false
        showAppearance = false
        designViews[doc.objectName] = { yaw: camera.yaw, pitch: camera.pitch, distance: camera.distance,
                                        target: camera.target, ortho: camera.orthographic }
    }
    function newDesign() { leaveDesign(); fissionApp.newDocument() }
    function switchDesign(index) {
        if (index === fissionApp.activeIndex) return
        leaveDesign()
        fissionApp.activeIndex = index
    }
    function closeDesign(index) {
        const d = fissionApp.documents[index]
        if (!d) return
        if (d.modified) { closeConfirm.index = index; closeConfirm.open(); return }
        reallyCloseDesign(index)
    }
    function reallyCloseDesign(index) {
        if (index === fissionApp.activeIndex) leaveDesign()
        const d = fissionApp.documents[index]
        if (d) delete designViews[d.objectName]
        fissionApp.closeDocument(index)
    }
    Dialog {
        id: closeConfirm
        property int index: -1
        readonly property var target: fissionApp.documents[index]
        title: "Close design"
        modal: true
        anchors.centerIn: parent
        standardButtons: Dialog.Discard | Dialog.Cancel
        Label { text: (closeConfirm.target ? closeConfirm.target.name : "") + " has unsaved changes. Close it anyway?" }
        onDiscarded: { close(); win.reallyCloseDesign(index) }
    }
    // While Section Analysis is open a picked face or plane becomes the cut plane
    Connections {
        target: selectionManager
        function onSelectionChanged() {
            if (win.inspectMode !== "section" || win.commandFeatureId) return
            const it = selectionManager.items.find(x => ["face", "plane", "originplane"].indexOf(x.kind) >= 0)
            if (!it) return
            if (!win.doc.planeFrame(it).ok) { fissionApp.showMessage("Pick a planar face or a plane"); return }
            Qt.callLater(() => { selectionManager.clear(); win.setSectionRef(it) })
        }
    }

    // ------------------------------------------------------------------ commands

    function lastSketchId(excluding) {
        const list = doc.featureList("Sketch")
        for (let i = list.length - 1; i >= 0; --i)
            if (list[i].id !== excluding) return list[i].id
        return ""
    }

    function hasSolidBefore(featureId) {
        for (const e of doc.featureList("")) {
            if (e.id === featureId) return false
            if (["Sketch", "ConstructionPlane", "ConstructionAxis", "ConstructionPoint"].indexOf(e.type) < 0) return true
        }
        return false
    }

    // Latest sketch no solid feature has used yet ("" if every sketch is used)
    function unusedSketchId() {
        const used = []
        for (const t of ["Extrude", "Revolve", "Sweep", "Loft", "BaseFlange", "Tab", "ContourFlange"])
            for (const entry of doc.featureList(t)) {
                const f = doc.featureById(entry.id)
                if (!f) continue
                if (f.get("sketch")) used.push(f.get("sketch"))
                if (f.get("path")) used.push(f.get("path"))
                if (f.get("rail")) used.push(f.get("rail"))
                if (f.get("centerline")) used.push(f.get("centerline"))
                for (const s of (f.get("sections") || [])) if (s.sketch) used.push(s.sketch)
            }
        const list = doc.featureList("Sketch")
        for (let i = list.length - 1; i >= 0; --i)
            if (used.indexOf(list[i].id) < 0) return list[i].id
        return ""
    }

    function selectedOf(kind) { return selection.ofKind(kind) }

    function bodyIndices(items) {
        const out = []
        for (const it of items)
            if (it.body !== undefined && out.indexOf(it.body) < 0) out.push(it.body)
        return out
    }

    function planeRef(it) {
        if (!it) return undefined
        if (it.kind === "face") return it
        if (it.kind === "originplane") return it.id
        if (it.kind === "plane") return "feature:" + it.id
        if (it.kind === "profile") return "feature:" + it.sketch
        return undefined
    }

    function axisRef(it) {
        if (!it) return undefined
        if (it.kind === "edge") return it
        if (it.kind === "originaxis") return it.id
        if (it.kind === "axis") return "feature:" + it.id
        if (it.kind === "sketchcurve") return "sketchline:" + it.sketch + ":" + it.entity
        return undefined
    }

    // Seeds a new feature from the current selection, falling back to the latest sketch
    function initialProps(type) {
        const p = {}
        const profiles = selectedOf("profile")
        const items = selection.items
        switch (type) {
        case "Extrude": case "Revolve": case "Sweep":
            if (profiles.length) {
                p.sketch = profiles[0].sketch
                p.profiles = profiles.filter(x => x.sketch === p.sketch).map(x => x.index)
            } else if (selectedOf("face").length) {
                p.sketch = ""
                p.profiles = selectedOf("face")
            } else {
                p.sketch = type === "Sweep" ? lastSketchId(lastSketchId("")) : unusedSketchId()
            }
            if (type === "Revolve") {
                const ax = axisRef(selectedOf("sketchcurve")[0] || selectedOf("axis")[0] || selectedOf("originaxis")[0]
                                   || selectedOf("edge")[0])
                if (ax) p.axis = ax
            }
            if (type === "Sweep") {
                const curves = selectedOf("sketchcurve")
                p.path = curves.length ? curves[0].sketch : lastSketchId(p.sketch)
            }
            break
        case "Loft": {
            const sections = []
            for (const it of items) {
                if (it.kind === "profile") sections.push({ kind: "profile", sketch: it.sketch, profile: it.index })
                else if (it.kind === "face") sections.push(it)
            }
            p.sections = sections
            break
        }
        case "Fillet": case "Chamfer": {
            const edges = selectedOf("edge")
            for (const f of selectedOf("face"))
                for (const e of doc.faceEdges(f))
                    if (!edges.some(x => x.body === e.body && x.index === e.index)) edges.push(e)
            p.edges = edges
            if (edges.length) p.body = edges[0].body
            break
        }
        case "Shell": {
            const faces = selectedOf("face")
            p.faces = faces
            if (faces.length) p.body = faces[0].body
            break
        }
        case "Mirror": case "RectangularPattern": case "CircularPattern": {
            const faces = selectedOf("face")
            const bodiesOnly = selectedOf("body")
            // Face selection (e.g. a hole wall) → pattern as Cut tools; whole-body selection → copy bodies
            if (faces.length && !bodiesOnly.length && type !== "Mirror") {
                p.faces = faces
                p.bodies = []
                p.operation = "cut"
            } else {
                p.bodies = bodyIndices(items.filter(x => x.kind === "body" || x.kind === "face"))
            }
            if (type === "Mirror") {
                const pl = planeRef(selectedOf("plane")[0] || selectedOf("originplane")[0])
                if (pl) p.plane = pl
            }
            if (type === "CircularPattern") {
                const ax = axisRef(selectedOf("axis")[0] || selectedOf("originaxis")[0])
                if (ax) p.axis = ax
            }
            break
        }
        case "ConstructionPlane": {
            const pl = planeRef(selectedOf("face")[0] || selectedOf("plane")[0] || selectedOf("originplane")[0])
            if (pl) p.base = pl
            break
        }
        case "ConstructionAxis": {
            const e = selectedOf("edge")[0]
            if (e) { p.mode = "edge"; p.edge = e }
            break
        }
        case "Hole": case "Thread": {
            const f = selectedOf("face")[0]
            const c = selectedOf("sketchcurve")[0]
            if (f) {
                p.face = f
                if (type === "Hole" && f.hit) p.center = f.hit.map(n => lenText(n, 3))
            } else if (c && type === "Hole") {
                p.sketch = c.sketch
            }
            break
        }
        case "Combine": {
            const b = bodyIndices(items.filter(x => x.kind === "body" || x.kind === "face"))
            if (b.length) { p.target = b[0]; p.tools = b.slice(1) }
            break
        }
        case "Move":
            p.bodies = bodyIndices(items.filter(x => x.kind === "body" || x.kind === "face"))
            p.moveType = "free"
            p.pivot = bodiesCenter(p.bodies).map(v => lenText(v, 4))
            break
        case "Joint":
            p.bodies = bodyIndices(items.filter(x => x.kind === "body"))
            break
        case "Scale": case "SplitBody": case "PathPattern":
            p.bodies = bodyIndices(items.filter(x => x.kind === "body" || x.kind === "face"))
            if (type === "PathPattern" && selectedOf("sketchcurve").length) p.path = selectedOf("sketchcurve")[0].sketch
            break
        case "SplitFace": case "OffsetFace": case "ReplaceFace": case "Offset":
            p.faces = selectedOf("face")
            break
        case "Draft": {
            // Like Fusion: the first picked face (or a plane) sets the pull direction, the rest get the draft
            const faces = selectedOf("face")
            const pl = planeRef(selectedOf("plane")[0] || selectedOf("originplane")[0])
                       || axisRef(selectedOf("edge")[0] || selectedOf("axis")[0] || selectedOf("originaxis")[0])
            if (pl) { p.plane = pl; p.faces = faces }
            else if (faces.length) { p.plane = faces[0]; p.faces = faces.slice(1) }
            break
        }
        case "Rib": case "Web": {
            const c = selectedOf("sketchcurve")[0]
            p.sketch = c ? c.sketch : unusedSketchId()
            break
        }
        case "MeshConvert":
            p.bodies = []   // the mesh is picked once the dialog is open
            break
        case "MeshReverse": case "MeshSmooth": case "MeshReduce": case "MeshPlaneCut": case "Remove":
        case "MeshRepair": case "MeshShell": case "MeshCombine": case "MeshFaceGroups": case "MeshRemesh":
            p.bodies = bodyIndices(items.filter(x => x.kind === "body" || x.kind === "face"))
            if (type === "MeshPlaneCut") {
                const pl = planeRef(selectedOf("plane")[0] || selectedOf("originplane")[0])
                if (pl) p.plane = pl
            }
            break
        case "BaseFlange": case "Tab":
            if (profiles.length) {
                p.sketch = profiles[0].sketch
                p.profiles = profiles.filter(x => x.sketch === p.sketch).map(x => x.index)
            } else {
                p.sketch = unusedSketchId()
            }
            break
        case "EdgeFlange": case "Hem": {
            const e = selectedOf("edge")[0]
            if (e) p.edge = e
            break
        }
        case "Bend": {
            const c = selectedOf("sketchcurve")[0]
            if (c) p.line = "sketchline:" + c.sketch + ":" + c.entity
            break
        }
        case "ContourFlange": {
            const c = selectedOf("sketchcurve")[0]
            p.sketch = c ? c.sketch : unusedSketchId()
            break
        }
        case "FlatPattern": case "Unfold": {
            const b = bodyIndices(items.filter(x => x.kind === "body" || x.kind === "face"))
            if (b.length) p.body = b[0]
            break
        }
        }
        return p
    }

    function startCommand(type, extra) {
        if (!doc) return
        if (type === "Move" && activeSketchId) { startSketchMove(); return }
        if (type === "Move" && !commandFeatureId) {
            // Sketch curves picked outside the sketch: move them inside it, as Fusion does
            const curves = selectedOf("sketchcurve")
            if (curves.length && !Array.from(selection.items).some(x => x.kind === "body" || x.kind === "face")) {
                const id = curves[0].sketch
                editSketch(id, false)
                selection.setItems(curves.filter(x => x.sketch === id))
                startSketchMove()
                return
            }
        }
        if (sketchMove) finishSketchMove(true)
        if (commandFeatureId) finishCommand(true)
        if (type === "Sketch") { pendingSketchTool = ""; beginSketchPlanePick(); return }
        pendingSketchTool = ""
        if (activeSketchId) finishSketch()
        pickingSketchPlane = false
        const props = initialProps(type)
        if (extra) for (const k in extra) props[k] = extra[k]
        const f = doc.addFeature(type, props)
        if (!f) { fissionApp.showMessage("Could not start " + type); return }
        lastCommand = type
        lastCommandProps = extra || {}
        if (type === "MeshFaceGroups") viewportItem.renderer.meshDisplay = "faceGroups"
        selection.clear()
        commandIsNew = true
        commandFeatureId = f.id
        fissionApp.showMessage(f.name + ": adjust the inputs, then press OK")
    }

    function repeatLastCommand() {
        if (lastCommand) startCommand(lastCommand, lastCommandProps)
    }

    function editFeature(f) {
        if (!f || !doc) return
        if (f.type === "Sketch") { editSketch(f.id, false); return }
        if (commandFeatureId) finishCommand(true)
        if (activeSketchId) finishSketch()
        doc.checkpoint()
        selection.clear()
        commandIsNew = false
        commandFeatureId = f.id
    }

    function finishCommand(ok) {
        if (!commandFeatureId) return
        if (ok) viewportItem.flushManip()
        const f = commandFeature
        commandFeatureId = ""
        doc.inputFeatureId = ""
        selection.filter = ""
        selection.clear()
        if (!ok) {
            doc.undo()   // reverts the add (or the edit, which checkpointed on entry)
            fissionApp.showMessage("Cancelled")
        } else if (f) {
            if (commandIsNew && f.healthy) hideConsumedSketches(f)
            fissionApp.showMessage(f.healthy ? f.name + " done" : f.name + ": " + f.error)
        }
    }

    // Like Fusion, sketches used by a new solid feature are hidden once it is committed
    function hideConsumedSketches(f) {
        const ids = []
        if (f.get("sketch")) ids.push(f.get("sketch"))
        if (f.type === "Sweep" && f.get("path")) ids.push(f.get("path"))
        if (f.type === "Loft") for (const s of (f.get("sections") || [])) ids.push(s.sketch)
        for (const id of ids) {
            const s = doc.featureById(id)
            if (s) s.visible = false
        }
    }

    function deleteFeature(f) {
        if (!f) return
        if (f.id === commandFeatureId) commandFeatureId = ""
        if (f.id === activeSketchId) finishSketch()
        doc.removeFeature(f)
    }

    // ------------------------------------------------------------------ sketches

    function beginSketchPlanePick() {
        if (activeSketchId) finishSketch()
        for (const it of selection.items) {
            const ref = planeRef(it)
            if (ref) { createSketch(ref); return }
        }
        selection.clear()
        selection.filter = "face,plane"
        pickingSketchPlane = true
        if (doc.bodies.length === 0) {
            camera.setView("home", false)
            viewportItem.renderer.fitAll(false)
        }
        fissionApp.showMessage("Select an origin plane, a construction plane or a planar face")
    }

    function createSketch(ref) {
        pickingSketchPlane = false
        selection.filter = ""
        selection.clear()
        const s = doc.addFeature("Sketch", { plane: ref })
        if (!s) return
        lastCommand = "Sketch"
        lastCommandProps = {}
        if (!s.healthy) fissionApp.showMessage(s.name + ": " + s.error)
        editSketch(s.id, true)
        if (pendingSketchTool) { sketchTool = pendingSketchTool; pendingSketchTool = "" }
    }

    function editSketch(id, isNew) {
        if (commandFeatureId) finishCommand(true)
        const f = doc.featureById(id)
        if (!f) return
        sketchMarkerRestore = -1
        if (!isNew) {
            const idx = doc.timeline.indexOf(f)
            if (idx + 1 < doc.timeline.marker) {
                sketchMarkerRestore = doc.timeline.marker
                doc.timeline.marker = idx + 1
            }
        }
        sketchWasHidden = !f.visible
        if (sketchWasHidden) f.visible = true
        activeSketchId = id
        sketchTool = isNew ? "line" : ""
        selection.clear()
        selection.filter = "sketchcurve"
        viewportItem.renderer.lookAtSketch(id)
        fissionApp.showMessage("Sketching on " + f.name + " \u2014 pick a tool, click in the view; Finish Sketch when done")
    }

    function finishSketch() {
        if (!activeSketchId) return
        viewportItem.cancelSketchInput()
        const s = activeSketch
        activeSketchId = ""
        sketchTool = ""
        selection.filter = ""
        selection.clear()
        if (sketchWasHidden && s) s.visible = false
        sketchWasHidden = false
        // Roll the timeline forward again and rebuild now so Extrude (etc.) come back immediately
        if (sketchMarkerRestore >= 0) {
            doc.timeline.marker = sketchMarkerRestore
            sketchMarkerRestore = -1
            doc.rebuild()
        }
        camera.setView("home", true)
        viewportItem.renderer.fitAll(true)
        if (s) {
            let tip = ""
            for (const info of doc.featureList()) {
                if (info.healthy) continue
                const bad = doc.featureById(info.id)
                if (bad) { tip = " \u2014 " + bad.name + ": " + bad.error; break }
            }
            fissionApp.showMessage(s.name + ": " + (s.healthy ? s.profileCount + " profile(s)" : s.error) + tip)
        }
    }

    function saveBodyAsMesh(index) {
        meshDialog.body = index
        meshDialog.open()
    }
    function exportBodyMesh(index, url) {
        const path = url.startsWith("file:///") ? decodeURIComponent(url.substring(8)) : url
        const out = doc.exportBodies(path, [index])
        const b = doc.bodies[index]
        fissionApp.showMessage(out ? "Saved " + (b ? b.name : "body") + " as " + path : "Mesh export failed: " + doc.lastError())
        return !!out
    }

    function deleteSelectedSketchEntities() {
        if (!activeSketch) return
        const ents = selection.ofKind("sketchcurve").map(x => x.entity).sort((a, b) => b - a)
        if (!ents.length) return
        doc.checkpoint()
        for (const e of ents) activeSketch.removeEntity(e)
        selection.clear()
    }

    // ------------------------------------------------------------------ files

    // Every save carries a picture of the view for the projects panel
    function saveTo(pathOrUrl) {
        fissionApp.captureThumbnail(viewportItem)
        return fissionApp.saveDocument(pathOrUrl)
    }
    function saveOrSaveAs() {
        if (doc.filePath.length) saveTo("")
        else saveAs()
    }
    // Saves into a projects folder under the design's name; a different design of that name asks first
    function saveInto(folder) {
        const path = folder + "/" + doc.name + ".f360"
        const mine = doc.filePath.replace(/\\/g, "/").toLowerCase() === path.toLowerCase()
        if (!mine && projects.exists(path)) { saveDialog.currentFolder = projects.urlOf(folder); saveDialog.open(); return false }
        return saveTo(path)
    }

    FileDialog {
        id: openDialog
        title: "Open design"
        currentFolder: projects.rootUrl
        nameFilters: ["Fission 360 designs (*.f360)", "All files (*)"]
        onAccepted: {
            win.leaveDesign()
            if (!fissionApp.openDocument(selectedFile.toString())) return
            // Fusion imports often lack a preview — grab the viewport once geometry is on screen
            Qt.callLater(() => {
                if (!win.doc || win.doc.hasThumbnail) return
                fissionApp.captureThumbnail(viewportItem)
                if (win.doc.hasThumbnail && win.doc.filePath.length)
                    fissionApp.saveDocument("")
                projectsDrawer.revision++
            })
        }
    }
    FileDialog {
        id: saveDialog
        title: "Save design"
        fileMode: FileDialog.SaveFile
        defaultSuffix: "f360"
        nameFilters: ["Fission 360 designs (*.f360)"]
        onAccepted: win.saveTo(selectedFile.toString())
    }
    FileDialog {
        id: exportDialog
        title: "Export"
        fileMode: FileDialog.SaveFile
        nameFilters: ["STEP (*.step *.stp)", "IGES (*.iges *.igs)", "STL mesh (*.stl)", "Wavefront OBJ (*.obj)",
                      "3D Manufacturing Format (*.3mf)"]
        onAccepted: fissionApp.exportDocument(selectedFile.toString())
    }

    FileDialog {
        id: meshDialog
        property int body: -1
        title: "Save body as mesh"
        fileMode: FileDialog.SaveFile
        defaultSuffix: "stl"
        nameFilters: ["STL mesh (*.stl)", "Wavefront OBJ (*.obj)", "3D Manufacturing Format (*.3mf)"]
        onAccepted: exportBodyMesh(body, selectedFile.toString())
    }

    FileDialog {
        id: dxfDialog
        property var face: null   // a planar face ref, or null for the active sketch
        title: face ? "Export face as DXF" : "Export sketch as DXF"
        fileMode: FileDialog.SaveFile
        defaultSuffix: "dxf"
        nameFilters: ["DXF drawing (*.dxf)"]
        onAccepted: face ? exportFaceDxf(face, selectedFile.toString()) : exportSketchDxf(selectedFile.toString())
    }
    FileDialog {
        id: flatDxfDialog
        title: "Export flat pattern as DXF"
        fileMode: FileDialog.SaveFile
        defaultSuffix: "dxf"
        nameFilters: ["DXF drawing (*.dxf)"]
        onAccepted: writeFlatPatternDxf(selectedFile.toString())
    }
    function exportSketchDxf(url) {
        const s = activeSketch
        if (!s) return false
        const path = url.startsWith("file:///") ? decodeURIComponent(url.substring(8)) : url
        const ok = s.exportDxfFile(path)
        fissionApp.showMessage(ok ? "Exported " + s.name + " to " + path : "DXF export failed: " + path)
        return ok
    }
    function exportFaceDxf(face, url) {
        const path = url.startsWith("file:///") ? decodeURIComponent(url.substring(8)) : url
        const out = doc.exportFaceDxf(face, path)
        fissionApp.showMessage(out ? "Exported the face to " + path : "DXF export failed: " + doc.lastError())
        return !!out
    }

    FileDialog {
        id: insertMeshDialog
        title: "Insert mesh"
        nameFilters: ["Mesh files (*.stl *.obj)", "STL mesh (*.stl)", "Wavefront OBJ (*.obj)"]
        onAccepted: insertMeshFile(selectedFile.toString())
    }
    function insertMesh() { insertMeshDialog.open() }
    function insertMeshFile(url) {
        if (!doc) return false
        const path = url.startsWith("file:///") ? decodeURIComponent(url.substring(8)) : url
        if (commandFeatureId) finishCommand(true)
        if (activeSketchId) finishSketch()
        const id = doc.insertMesh(path, lengthUnit)
        if (!id) { fissionApp.showMessage("Insert Mesh failed: " + doc.lastError()); return false }
        fissionApp.showMessage("Inserted " + path + " (units: " + lengthUnit + ")")
        Qt.callLater(() => viewportItem.renderer.fitAll(true))
        return true
    }

    FileDialog {
        id: importCadDialog
        title: "Insert STEP / IGES"
        nameFilters: ["CAD files (*.step *.stp *.iges *.igs)", "STEP (*.step *.stp)", "IGES (*.iges *.igs)"]
        onAccepted: importCADFile(selectedFile.toString())
    }
    function importCAD() { importCadDialog.open() }
    function importCADFile(url) {
        if (!doc) return false
        const path = url.startsWith("file:///") ? decodeURIComponent(url.substring(8)) : url
        if (commandFeatureId) finishCommand(true)
        if (activeSketchId) finishSketch()
        const id = doc.importCAD(path)
        if (!id) { fissionApp.showMessage("Insert failed: " + doc.lastError()); return false }
        const f = doc.featureById(id)
        fissionApp.showMessage(f && !f.healthy ? f.name + ": " + f.error : "Inserted " + path)
        Qt.callLater(() => viewportItem.renderer.fitAll(true))
        return true
    }
    function insertMcMaster() {
        // Lazy-load: WebEngine/Chromium stays out of cold start until first Insert McMaster
        if (mcmasterLoader.item) {
            mcmasterLoader.item.open()
            return
        }
        mcmasterLoader.active = true
    }

    // ------------------------------------------------------------------ linked designs
    // A saved design dropped (or inserted from Projects) into this one stays linked to its file: when the
    // source is saved with changes, the user is asked to update. Unlink keeps the geometry and stops asking.
    function insertLinkedDesign(path) {
        if (!doc) return false
        if (commandFeatureId) finishCommand(true)
        if (activeSketchId) finishSketch()
        const before = doc.bodyCount()
        const id = doc.insertLinkedDesign(path)
        if (!id) { fissionApp.showMessage("Insert failed: " + doc.lastError()); return false }
        const f = doc.featureById(id)
        if (f && !f.healthy) { fissionApp.showMessage(f.name + ": " + f.error); return true }
        fissionApp.showMessage("Inserted " + f.name + " (linked — updates when the original is saved)")
        // Place it like a McMaster part: select the new bodies and start Move
        const items = []
        for (let i = before; i < doc.bodyCount(); ++i) items.push({ kind: "body", body: i })
        if (items.length) {
            selection.setItems(items)
            Qt.callLater(() => startCommand("Move"))
        }
        return true
    }
    function linkedSourceName(featureId) {
        const l = doc ? doc.linkedDesigns().find(x => x.featureId === featureId) : null
        return l ? l.sourceName : ""
    }
    function updateLinkedDesign(featureId) {
        if (!doc) return
        if (commandFeatureId) finishCommand(true)
        if (!doc.updateLinkedDesign(featureId)) fissionApp.showMessage("Update failed: " + doc.lastError())
        else fissionApp.showMessage("Updated " + linkedSourceName(featureId) + " from its original design")
        checkLinkedDesigns()
    }
    function unlinkDesign(featureId) {
        if (!doc) return
        const name = linkedSourceName(featureId)
        doc.unlinkDesign(featureId)
        fissionApp.showMessage(name + " unlinked — it stays in this design and is no longer updated")
        checkLinkedDesigns()
    }
    function openLinkedOriginal(path) {
        if (path && fissionApp.openDocument(path)) return
        fissionApp.showMessage("The original design could not be opened: " + path)
    }
    // Inserts files dropped on the viewport: .f360 as linked designs, CAD and mesh files as imports
    function dropFiles(urls) {
        let n = 0
        for (const u of urls) {
            const url = u.toString()
            const path = url.startsWith("file:///") ? decodeURIComponent(url.substring(8)) : url
            const ext = path.substring(path.lastIndexOf(".") + 1).toLowerCase()
            if (ext === "f360") { if (insertLinkedDesign(path)) ++n }
            else if (["step", "stp", "iges", "igs"].includes(ext)) { if (importCADFile(url)) ++n }
            else if (["stl", "obj"].includes(ext)) { if (insertMeshFile(url)) ++n }
            else fissionApp.showMessage("Cannot insert " + path.substring(path.lastIndexOf("/") + 1))
        }
        return n
    }
    // featureId → source revision the user answered "Not Now" for (asked again once the source changes again)
    property var linkedDismissed: ({})
    property var linkedStale: []
    property int linkedRevision: 0
    function checkLinkedDesigns() {
        ++linkedRevision
        if (!doc || linkUpdateDialog.visible || inDrawing) return
        const stale = doc.linkedDesigns().filter(l => l.outOfDate && linkedDismissed[l.featureId] !== l.sourceRevision)
        if (!stale.length) return
        linkedStale = stale
        linkUpdateDialog.open()
    }
    Timer {
        interval: 4000
        repeat: true
        running: !!win.doc && win.active
        onTriggered: win.checkLinkedDesigns()
    }
    Dialog {
        id: linkUpdateDialog
        anchors.centerIn: parent
        modal: true
        width: 420
        title: "Linked designs changed"
        standardButtons: Dialog.Ok | Dialog.Cancel
        Component.onCompleted: {
            standardButton(Dialog.Ok).text = "Update"
            standardButton(Dialog.Cancel).text = "Not Now"
        }
        onAccepted: {
            for (const l of win.linkedStale) win.doc.updateLinkedDesign(l.featureId)
            fissionApp.showMessage("Updated " + win.linkedStale.length + " linked design(s)")
            win.linkedStale = []
            ++win.linkedRevision
        }
        onRejected: {
            const d = Object.assign({}, win.linkedDismissed)
            for (const l of win.linkedStale) d[l.featureId] = l.sourceRevision
            win.linkedDismissed = d
            win.linkedStale = []
        }
        Column {
            width: parent.width
            spacing: 6
            Text {
                width: parent.width
                wrapMode: Text.WordWrap
                color: Theme.text
                text: "The original of these inserted designs was saved with changes. Update them to the newest version?"
            }
            Repeater {
                model: win.linkedStale
                Text {
                    required property var modelData
                    width: parent.width
                    elide: Text.ElideMiddle
                    color: Theme.text
                    font.bold: true
                    text: "\u2022 " + modelData.name + "  (" + modelData.path.substring(modelData.path.lastIndexOf("/") + 1) + ")"
                }
            }
            Text {
                width: parent.width
                wrapMode: Text.WordWrap
                color: Theme.textDim
                font.pixelSize: Theme.smallFont
                text: "Right-click an insert under Linked Designs in the browser to unlink it and stop these updates."
            }
        }
    }

    // ------------------------------------------------------------------ sheet metal and scripts

    function openSheetMetalRules() {
        if (!doc) return
        doc.ensureSheetMetalRule()
        sheetRulesDialog.open()
    }
    function exportFlatPatternDxf() {
        if (!doc) return
        if (!doc.featureList("FlatPattern").length && !doc.featureList("Unfold").length) {
            fissionApp.showMessage("Export DXF: create a flat pattern first")
            return
        }
        flatDxfDialog.open()
    }
    function writeFlatPatternDxf(url) {
        const path = url.startsWith("file:///") ? decodeURIComponent(url.substring(8)) : url
        const out = doc.exportFlatPatternDxf("", path)
        fissionApp.showMessage(out ? "Exported the flat pattern to " + path : "DXF export failed: " + doc.lastError())
        return !!out
    }
    function openScripts() { scriptsDialog.open() }
    function runAddInCommand(id) {
        if (commandFeatureId) finishCommand(true)
        const r = fissionScripts.runCommand(id)
        if (!r.ok) fissionApp.showMessage(id + ": " + (r.line ? "line " + r.line + ": " : "") + r.error)
        return r
    }
    // Add-ins run once the window is up
    Component.onCompleted: {
        zoomToCursor = !!fissionApp.setting("viewport/zoomToCursor", false)
        sketchSnapToGrid = fissionApp.setting("sketch/snapToGrid", true) !== false
            && fissionApp.setting("sketch/snapToGrid", true) !== "false"
        Qt.callLater(() => {
            if (viewportItem && viewportItem.renderer)
                viewportItem.renderer.snapToGrid = sketchSnapToGrid
            fissionScripts.startAddIns(win)
            // Recover Autosave is File-menu only (no startup prompt)
        })
    }
    function setZoomToCursor(on) {
        zoomToCursor = !!on
        fissionApp.setSetting("viewport/zoomToCursor", zoomToCursor)
    }

    // ------------------------------------------------------------------ move / copy

    // Point input: {prompt, sketch, done(point, label)}. The viewport takes the next click: a snapped 3D point
    // ([x, y, z]), or with sketch set a sketch point ([u, v]) of the active sketch.
    property var pointPick: null
    function requestPoint(prompt, done, sketch) {
        pointPick = { prompt: prompt, done: done, sketch: !!sketch }
        fissionApp.showMessage(prompt + " (Esc cancels)")
    }

    function num(expr) {
        const r = doc.evaluateExpression(String(expr))
        return r.error ? 0 : r.value
    }
    function bodiesCenter(list) {
        let lo = null, hi = null
        const ids = list && list.length ? list : [...Array(doc.bodyCount()).keys()]
        for (const i of ids) {
            const b = doc.bodyBox(i)
            if (!b.length) continue
            lo = lo ? lo.map((v, k) => Math.min(v, b[k])) : b.slice(0, 3)
            hi = hi ? hi.map((v, k) => Math.max(v, b[k + 3])) : b.slice(3, 6)
        }
        return lo ? lo.map((v, k) => (v + hi[k]) / 2) : [0, 0, 0]
    }

    // Rotation R = Rz * Ry * Rx (as the Move feature applies it); turning it further about world axis k
    function rotMat(k, deg) {
        const a = deg * Math.PI / 180, c = Math.cos(a), s = Math.sin(a)
        if (k === 0) return [[1, 0, 0], [0, c, -s], [0, s, c]]
        if (k === 1) return [[c, 0, s], [0, 1, 0], [-s, 0, c]]
        return [[c, -s, 0], [s, c, 0], [0, 0, 1]]
    }
    function matMul(a, b) {
        return a.map((row, i) => [0, 1, 2].map(j => row[0] * b[0][j] + row[1] * b[1][j] + row[2] * b[2][j]))
    }
    function composeRotation(xyz, k, deg) {
        const R = matMul(rotMat(k, deg), matMul(rotMat(2, xyz[2] || 0), matMul(rotMat(1, xyz[1] || 0), rotMat(0, xyz[0] || 0))))
        const d = 180 / Math.PI
        const sy = Math.max(-1, Math.min(1, -R[2][0]))
        let rx, ry = Math.asin(sy) * d, rz
        if (Math.abs(sy) > 0.99999) { rx = Math.atan2(-R[1][2], R[1][1]) * d; rz = 0 }
        else { rx = Math.atan2(R[2][1], R[2][2]) * d; rz = Math.atan2(R[1][0], R[0][0]) * d }
        return [rx, ry, rz].map(v => Math.round(v * 1000) / 1000 + 0)
    }

    // Triad of the Move feature being edited (free move and translate), or of the sketch move
    readonly property var gizmoCache: ({})
    readonly property var moveGizmo: sketchMove ? sketchMove.gizmo
                                   : (commandFeature && commandFeature.type === "Move") ? bodyMoveGizmo(commandFeature) : null
    function bodyMoveGizmo(f) {
        if (gizmoCache.id === f.id) return gizmoCache.g
        let start = null
        const vec = key => [0, 1, 2].map(i => num((f.get(key) || [])[i] || 0))
        const g = {
            get visible() { const t = f.get("moveType"); return t === "free" || t === "translate" },
            origin: () => { const p = vec("pivot"), t = vec("translate"); return [p[0] + t[0], p[1] + t[1], p[2] + t[2]] },
            axes: () => [[1, 0, 0], [0, 1, 0], [0, 0, 1]],
            arrows: [true, true, true],
            planes: [true, true, true],
            get rings() { const on = f.get("moveType") === "free"; return [on, on, on] },
            begin: () => { start = { t: vec("translate"), r: vec("rotation") } },
            translate: d => f.set("translate", [0, 1, 2].map(i => lenText(start.t[i] + d[i], 4))),
            rotate: (k, deg) => f.set("rotation", composeRotation(start.r, k, deg).map(a => a + " deg")),
            end: () => {},
            format: v => lenText(v, 3)
        }
        gizmoCache.id = f.id
        gizmoCache.g = g
        return g
    }

    // Move/Copy inside a sketch: the selected curves move on their own (links to other curves are dropped)
    property var sketchMove: null
    property int sketchMoveRev: 0
    function sketchMoveCenter(s, ents) {
        let pu = 0, pv = 0, n = 0
        for (const h of s.handles()) if (ents.indexOf(h.entity) >= 0) { pu += h.u; pv += h.v; ++n }
        return n ? [pu / n, pv / n] : [0, 0]
    }
    function startSketchMove() {
        const s = activeSketch
        if (!s || sketchMove) return
        const ents = selectedOf("sketchcurve").filter(x => x.sketch === activeSketchId).map(x => x.entity)
        sketchTool = ""
        doc.checkpoint()
        const frame = doc.planeFrame({ kind: "plane", id: activeSketchId })
        const c = sketchMoveCenter(s, ents)
        const st = {
            sketch: activeSketchId, entities: ents, frame: frame,
            saved: JSON.parse(JSON.stringify({ e: s.get("entities"), c: s.get("constraints") })),
            type: "free", du: 0, dv: 0, angle: 0, pu: c[0], pv: c[1], pivotSet: false, copy: false, error: ""
        }
        const w = (u, v) => [0, 1, 2].map(k => frame.origin[k] + frame.x[k] * u + frame.y[k] * v)
        let start = null
        st.gizmo = {
            get visible() { return st.entities.length > 0 && (st.type === "free" || st.type === "translate") },
            origin: () => w(st.pu + st.du, st.pv + st.dv),
            axes: () => [frame.x, frame.y, frame.normal],
            arrows: [true, true, false],
            planes: [false, false, true],
            get rings() { return [false, false, st.type === "free"] },
            begin: () => { start = { du: st.du, dv: st.dv, angle: st.angle } },
            translate: d => {
                st.du = Math.round((start.du + d[0] * frame.x[0] + d[1] * frame.x[1] + d[2] * frame.x[2]) * 1e4) / 1e4
                st.dv = Math.round((start.dv + d[0] * frame.y[0] + d[1] * frame.y[1] + d[2] * frame.y[2]) * 1e4) / 1e4
                applySketchMove()
            },
            rotate: (k, deg) => { st.angle = start.angle + deg; applySketchMove() },
            end: () => {},
            format: v => lenText(v, 3)
        }
        sketchMove = st
        sketchMoveRev++
        fissionApp.showMessage(ents.length ? "Move/Copy: drag the arrows, square or ring, or type the values; Enter = OK"
                                           : "Move/Copy: click the sketch curves to move")
    }
    // While Move/Copy is open, clicking curves adds them to (or takes them out of) what moves
    function syncSketchMoveSelection() {
        const st = sketchMove
        const s = st ? doc.featureById(st.sketch) : null
        if (!s) return
        const count = st.saved.e.length
        const ents = selectedOf("sketchcurve").filter(x => x.sketch === st.sketch && x.entity < count).map(x => x.entity)
        if (JSON.stringify(ents) === JSON.stringify(st.entities)) return
        st.entities = ents
        if (!st.pivotSet) {
            const p = s.props
            p.entities = st.saved.e
            p.constraints = st.saved.c
            s.props = p
            const c = sketchMoveCenter(s, ents)
            st.pu = c[0]; st.pv = c[1]
        }
        applySketchMove()
    }
    Connections {
        target: selection
        function onSelectionChanged() { if (win.sketchMove) Qt.callLater(win.syncSketchMoveSelection) }
    }
    function applySketchMove() {
        const st = sketchMove
        const s = st ? doc.featureById(st.sketch) : null
        if (!s) return
        const p = s.props
        p.entities = st.saved.e
        p.constraints = st.saved.c
        s.props = p
        st.error = ""
        if (st.entities.length && (st.du !== 0 || st.dv !== 0 || st.angle !== 0 || st.copy)) {
            const r = s.moveEntities(st.entities, st.du, st.dv, st.angle, st.pu, st.pv, st.copy)
            if (!r.ok) {
                st.error = r.error || "The curves cannot move there"
                s.props = p
            }
        }
        sketchMoveRev++
    }
    function setSketchMove(changes) {
        if (!sketchMove) return
        for (const k in changes) sketchMove[k] = changes[k]
        applySketchMove()
    }
    function finishSketchMove(ok) {
        const st = sketchMove
        if (!st) return
        pointPick = null
        sketchMove = null
        if (!ok) { doc.undo(); fissionApp.showMessage("Move cancelled"); return }
        fissionApp.showMessage(st.error ? "Move: " + st.error : (st.copy ? "Copied " : "Moved ") + st.entities.length + " curve(s)")
    }
    onActiveSketchIdChanged: if (sketchMove && sketchMove.sketch !== activeSketchId) { sketchMove = null; pointPick = null }
    onCommandFeatureIdChanged: pointPick = null

    // ------------------------------------------------------------------ inspect

    // "" | "measure" | "interference" | "section" | "zebra" | "draft" | "curvature": panel shown
    property string inspectMode: ""
    property string analysis: ""             // surface shading: "" | "zebra" | "draft" | "curvature"
    property bool sectionEnabled: false
    // The cut plane: "XY" / "XZ" / "YZ" or a plane / planar face ref; its normal points at the removed side
    property var sectionRef: "XY"
    property bool sectionFlip: false
    property real sectionOffset: 0           // mm along the plane normal, from the plane
    property string sectionId: ""            // saved analysis being edited ("" = a new one)
    readonly property var sectionFrame: (rev, doc) ? doc.planeFrame(sectionRef) : ({ ok: false })
    property string pullAxis: "Z"
    property real draftAngle: 1              // degrees

    // Offset putting the cut through the middle of the model along the plane normal
    function sectionCenterOffset(frame) {
        const b = viewportItem.renderer.bounds()
        if (!frame.ok || !b.length) return 0
        const n = frame.normal, o = frame.origin
        const mid = ((b[0] + b[3]) / 2 - o[0]) * n[0] + ((b[1] + b[4]) / 2 - o[1]) * n[1] + ((b[2] + b[5]) / 2 - o[2]) * n[2]
        return Math.round(mid * 100) / 100
    }
    function setSectionRef(ref) {
        sectionRef = ref
        sectionOffset = sectionCenterOffset(doc.planeFrame(ref))
        sectionAwaitingPlane = false
        sectionEnabled = true
    }
    // A new section shows nothing cut until a plane is picked; the origin planes are shown to pick from
    property bool sectionAwaitingPlane: false
    // The cut shown outside the panel follows the visible saved section
    function syncSections() {
        if (!doc || inspectMode === "section") return
        const shown = (doc.analyses || []).filter(a => a.type === "section" && a.visible)
        if (!shown.length) { sectionEnabled = false; return }
        const a = shown[shown.length - 1]
        sectionRef = a.plane
        sectionOffset = a.offset || 0
        sectionFlip = !!a.flip
        sectionEnabled = true
    }
    function editSection(id) {
        const a = doc.analysis(id)
        if (!a.id) return
        if (commandFeatureId) finishCommand(true)
        sectionId = id
        sectionRef = a.plane
        sectionOffset = a.offset || 0
        sectionFlip = !!a.flip
        sectionEnabled = true
        inspectMode = "section"
        fissionApp.showMessage("Edit " + a.name + ": pick a face or plane, drag the offset, then OK")
    }
    // OK in the section panel: saves the cut as a browser analysis (or updates the one being edited)
    function commitSection() {
        if (sectionAwaitingPlane) { fissionApp.showMessage("Pick a plane or planar face to cut with first"); return }
        const props = { type: "section", plane: sectionRef, offset: sectionOffset, flip: sectionFlip, visible: true }
        if (sectionId) doc.updateAnalysis(sectionId, props)
        else sectionId = doc.addAnalysis(props)
        const name = doc.analysis(sectionId).name
        inspectMode = ""
        sectionId = ""
        syncSections()
        fissionApp.showMessage(name + " saved in the browser")
    }
    Connections {
        target: win.doc
        function onAnalysesChanged() { win.syncSections() }
    }

    function openInspect(mode) {
        if (!doc) return
        if (commandFeatureId) finishCommand(true)
        if (mode === "section") {
            if (inspectMode === "section") { closeInspect(); return }
            sectionId = ""
            sectionFlip = false
            sectionEnabled = false
            sectionAwaitingPlane = true
        } else if (mode === "zebra" || mode === "draft" || mode === "curvature") {
            if (analysis === mode && inspectMode === mode) { analysis = ""; inspectMode = ""; return }
            analysis = mode
        } else if (mode === "measure") {
            selection.clear()
            selection.filter = "face,edge,body,point,originpoint"
        }
        inspectMode = mode
        if (mode !== "section") { sectionId = ""; syncSections() }
        const names = { measure: "Measure: pick one item, then a second for distance and angle",
                        interference: "Interference: checks every pair of solid bodies",
                        section: "Section Analysis: pick an origin plane, a plane or a planar face to cut with",
                        zebra: "Zebra Analysis: stripes show surface continuity",
                        draft: "Draft Analysis: green = enough draft, red = undercut, yellow = too little",
                        curvature: "Curvature Map: blue = flat, red = tight curvature" }
        fissionApp.showMessage(names[mode] || mode)
    }
    function closeInspect() {
        if (inspectMode === "measure") { selection.clear(); selection.filter = "" }
        inspectMode = ""
        sectionAwaitingPlane = false
        sectionId = ""
        syncSections()
    }

    function openDocumentSettings() { documentSettings.open() }
    function openAppearance() {
        showAppearance = !showAppearance
        if (showAppearance) {
            inspectMode = ""
            showRender = false
        }
    }
    function openDisplaySettings() { appearanceSettings.open() }    // viewport background / zoom
    // Live Render = viewport shading only (never opens a save-PNG dialog).
    // Stays in the RENDER workspace when toggled off — DESIGN ▾ → Design leaves Render.
    function toggleLiveRender() {
        if (workspace === "render" && liveRender) {
            liveRender = false
            applyLiveRenderLook(false)
            fissionApp.showMessage("Live Render off")
            return
        }
        if (workspace !== "render")
            enterRenderWorkspace()
        else {
            liveRender = true
            applyLiveRenderLook(true)
            fissionApp.showMessage("Live Render — sun / HDRI shading in the design viewport (no file save).")
        }
    }
    function openRender() { toggleLiveRender() }
    function openRenderSettings() {
        showRender = !showRender
        if (showRender) {
            showAppearance = false
            inspectMode = ""
            // Ensure lighting is live while tweaking settings
            if (workspace !== "render") enterRenderWorkspace()
            else {
                liveRender = true
                applyLiveRenderLook(true)
            }
        }
    }
    function goHome() { camera.setView("home", true); viewportItem.renderer.fitAll(true) }

    // ------------------------------------------------------------------ keyboard commands (Fusion defaults)

    // A sketch tool key: switches tool inside a sketch; outside one it asks for a plane first, like Fusion
    function sketchToolKey(tool) {
        if (activeSketchId) { sketchTool = sketchTool === tool ? "" : tool; return }
        if (commandFeatureId) return
        pendingSketchTool = tool
        beginSketchPlanePick()
    }

    // Q (Press Pull): a picked edge is filleted, a picked body face is offset (its neighbours follow),
    // otherwise the profile (or latest sketch) is extruded
    function pressPull() {
        if (selectedOf("edge").length) startCommand("Fillet")
        else if (selectedOf("face").length) startCommand("OffsetFace")
        else startCommand("Extrude")
    }

    // V: toggles the visibility of the selected bodies, sketches and construction geometry
    function toggleSelectedVisibility() {
        const targets = []
        const add = o => { if (o && targets.indexOf(o) < 0) targets.push(o) }
        for (const it of selection.items) {
            if (it.body !== undefined) add(doc.bodies[it.body])
            else if (it.sketch) add(doc.featureById(it.sketch))
            else if (it.kind === "plane" || it.kind === "axis" || it.kind === "point") add(doc.featureById(it.id))
            else if (it.kind && it.kind.indexOf("origin") === 0) setOriginVisible(it.id, !originVisible[it.id])
        }
        if (!targets.length) { if (!selection.items.length) fissionApp.showMessage("Select bodies, sketches or construction geometry to hide or show (V)"); return }
        const show = !targets[0].visible
        for (const t of targets) t.visible = show
        selection.clear()
    }

    // Ctrl+A: every body (or, inside a sketch, every curve of the active sketch)
    function selectAll() {
        if (!doc || commandFeatureId) return
        if (activeSketch) {
            const items = []
            for (let i = 0; i < activeSketch.entityCount; ++i) items.push({ kind: "sketchcurve", sketch: activeSketchId, entity: i })
            selection.setItems(items)
            return
        }
        const items = []
        for (let i = 0; i < doc.bodyCount(); ++i) if (doc.bodies[i].visible) items.push({ kind: "body", body: i })
        selection.setItems(items)
        fissionApp.showMessage(items.length + " bodies selected (Delete removes them)")
    }

    // Like Fusion's Delete on bodies: a Remove feature in the timeline, so it can be undone or rolled back
    function removeBodies(indices) {
        if (!doc || !indices.length) return
        if (commandFeatureId) finishCommand(true)
        const f = doc.addFeature("Remove", { bodies: indices })
        fissionApp.showMessage(f && f.healthy ? "Removed " + indices.length + (indices.length > 1 ? " bodies" : " body")
                                              : "Remove failed" + (f ? ": " + f.error : ""))
    }

    function setSelectionTool(tool) {
        selectionTool = tool
        const names = { window: "Window selection: drag a box (left to right = fully inside, right to left = touching)",
                        freeform: "Freeform selection: drag a loop around the items",
                        paint: "Paint selection: drag over the items" }
        fissionApp.showMessage(names[tool])
    }

    // Shift+N: each body in its own colour (Fission's bodies are its components)
    function toggleComponentColors() {
        const r = viewportItem.renderer
        r.componentColors = !r.componentColors
        fissionApp.showMessage("Component color cycling " + (r.componentColors ? "on" : "off"))
    }

    function notAvailable(name, key) {
        fissionApp.showMessage(name + " (" + key + ") is not in Fission 360 yet")
    }

    // Every command reachable from the S toolbox; `keys` is the Fusion default shortcut ("" = none).
    // mode: "model" (no sketch open), "sketch" (inside a sketch) or "any".
    function commandCatalog() {
        const cmd = (name, keys, section, mode, run) => ({ name: name, keys: keys, section: section, mode: mode, run: run })
        const st = (name, tool, keys) => cmd(name, keys || "", "Sketch", "sketch", () => { sketchTool = tool })
        return [
            cmd("Create Sketch", "", "Create", "model", () => startCommand("Sketch")),
            cmd("Extrude", "E", "Create", "any", () => startCommand("Extrude")),
            cmd("Press Pull", "Q", "Modify", "any", () => pressPull()),
            cmd("Revolve", "", "Create", "any", () => startCommand("Revolve")),
            cmd("Sweep", "", "Create", "any", () => startCommand("Sweep")),
            cmd("Loft", "", "Create", "any", () => startCommand("Loft")),
            cmd("Fillet", "F", "Modify", "any", () => startCommand("Fillet")),
            cmd("Chamfer", "", "Modify", "any", () => startCommand("Chamfer")),
            cmd("Shell", "", "Modify", "any", () => startCommand("Shell")),
            cmd("Hole", "H", "Create", "any", () => startCommand("Hole")),
            cmd("Thread", "", "Create", "any", () => startCommand("Thread")),
            cmd("Rib", "", "Create", "any", () => startCommand("Rib")),
            cmd("Web", "", "Create", "any", () => startCommand("Web")),
            cmd("Draft", "", "Modify", "any", () => startCommand("Draft")),
            cmd("Scale", "", "Modify", "any", () => startCommand("Scale")),
            cmd("Combine", "", "Modify", "any", () => startCommand("Combine")),
            cmd("Replace Face", "", "Modify", "any", () => startCommand("ReplaceFace")),
            cmd("Split Face", "", "Modify", "any", () => startCommand("SplitFace")),
            cmd("Split Body", "", "Modify", "any", () => startCommand("SplitBody")),
            cmd("Offset Face", "", "Modify", "any", () => startCommand("OffsetFace")),
            cmd("Offset (copy faces / bodies)", "", "Modify", "any", () => startCommand("Offset")),
            cmd("Move/Copy", "M", "Modify", "any", () => startCommand("Move")),
            cmd("Joint", "J", "Assemble", "model", () => startCommand("Joint", { asBuilt: false })),
            cmd("As-built Joint", "Shift+J", "Assemble", "model", () => startCommand("Joint", { asBuilt: true })),
            cmd("Component Color Cycling", "Shift+N", "View", "any", () => toggleComponentColors()),
            cmd("Change Parameters", "", "Modify", "any", () => openParameters()),
            cmd("Mirror", "", "Pattern", "any", () => startCommand("Mirror")),
            cmd("Rectangular Pattern", "", "Pattern", "any", () => startCommand("RectangularPattern")),
            cmd("Circular Pattern", "", "Pattern", "any", () => startCommand("CircularPattern")),
            cmd("Pattern on Path", "", "Pattern", "any", () => startCommand("PathPattern")),
            cmd("Offset Plane", "", "Construct", "any", () => startCommand("ConstructionPlane", { mode: "offset" })),
            cmd("Plane at Angle", "", "Construct", "any", () => startCommand("ConstructionPlane", { mode: "angle" })),
            cmd("Midplane", "", "Construct", "any", () => startCommand("ConstructionPlane", { mode: "midplane" })),
            cmd("Plane Through Three Points", "", "Construct", "any", () => startCommand("ConstructionPlane", { mode: "threePoints" })),
            cmd("Axis", "", "Construct", "any", () => startCommand("ConstructionAxis")),
            cmd("Point", "", "Construct", "any", () => startCommand("ConstructionPoint")),
            cmd("Properties", "", "Inspect", "any", () => { showProperties = !showProperties }),
            cmd("Measure", "I", "Inspect", "model", () => openInspect("measure")),
            cmd("Interference", "", "Inspect", "model", () => openInspect("interference")),
            cmd("Section Analysis", "", "Inspect", "model", () => openInspect("section")),
            cmd("Zebra Analysis", "", "Inspect", "model", () => openInspect("zebra")),
            cmd("Draft Analysis", "", "Inspect", "model", () => openInspect("draft")),
            cmd("Curvature Map Analysis", "", "Inspect", "model", () => openInspect("curvature")),
            cmd("Insert Mesh", "", "Mesh", "model", () => insertMesh()),
            cmd("Convert Mesh to Solid", "", "Mesh", "model", () => startCommand("MeshConvert", { direction: "toBRep" })),
            cmd("Convert Solid to Mesh", "", "Mesh", "model", () => startCommand("MeshConvert", { direction: "toMesh" })),
            cmd("Reverse Normals", "", "Mesh", "model", () => startCommand("MeshReverse")),
            cmd("Mesh Plane Cut", "", "Mesh", "model", () => startCommand("MeshPlaneCut")),
            cmd("Mesh Smooth", "", "Mesh", "model", () => startCommand("MeshSmooth")),
            cmd("Mesh Reduce", "", "Mesh", "model", () => startCommand("MeshReduce")),
            cmd("Generate Face Groups", "", "Mesh", "model", () => startCommand("MeshFaceGroups")),
            cmd("Remesh", "", "Mesh", "model", () => startCommand("MeshRemesh")),
            cmd("Insert STEP / IGES", "", "Insert", "model", () => importCAD()),
            cmd("Insert McMaster-Carr", "", "Insert", "model", () => insertMcMaster()),
            cmd("Flange", "", "Sheet Metal", "model", () => startCommand("BaseFlange")),
            cmd("Edge Flange", "", "Sheet Metal", "model", () => startCommand("EdgeFlange")),
            cmd("Create Flat Pattern", "", "Sheet Metal", "model", () => startCommand("FlatPattern")),
            cmd("Sheet Metal Rules", "", "Sheet Metal", "model", () => openSheetMetalRules()),
            cmd("Hem", "", "Sheet Metal", "model", () => startCommand("Hem")),
            cmd("Tab", "", "Sheet Metal", "model", () => startCommand("Tab")),
            cmd("Bend", "", "Sheet Metal", "model", () => startCommand("Bend")),
            cmd("Contour Flange", "", "Sheet Metal", "model", () => startCommand("ContourFlange")),
            cmd("Unfold", "", "Sheet Metal", "model", () => startCommand("Unfold")),
            cmd("Refold", "", "Sheet Metal", "model", () => startCommand("Refold")),
            cmd("Export Flat Pattern as DXF", "", "Sheet Metal", "model", () => exportFlatPatternDxf()),
            cmd("Scripts and Add-Ins", "Shift+S", "Utilities", "any", () => openScripts()),
            cmd("Appearance", "A", "Render", "any", () => openAppearance()),
            cmd("Live Render", "", "Render", "any", () => toggleLiveRender()),
            cmd("Render Settings", "", "Render", "any", () => openRenderSettings()),
            cmd("Display Settings", "", "Utilities", "any", () => openDisplaySettings()),
            cmd("Window Selection", "1", "Select", "any", () => setSelectionTool("window")),
            cmd("Freeform Selection", "2", "Select", "any", () => setSelectionTool("freeform")),
            cmd("Paint Selection", "3", "Select", "any", () => setSelectionTool("paint")),
            cmd("Toggle Visibility", "V", "View", "any", () => toggleSelectedVisibility()),
            cmd("Select All", "Ctrl+A", "Select", "any", () => selectAll()),
            cmd("Remove Selected Bodies", "Del", "Modify", "model", () =>
                removeBodies(bodyIndices(selection.items.filter(x => x.kind === "body" || x.kind === "face")))),
            cmd("Fit", "F6", "View", "any", () => viewportItem.renderer.fitAll(true)),
            cmd("Home View", "", "View", "any", () => goHome()),
            cmd("Compute All", "Ctrl+B", "Design", "any", () => { doc.rebuild(); fissionApp.showMessage("Recomputed the timeline") }),
            cmd("Document Settings", "", "Design", "any", () => openDocumentSettings()),
            st("Line", "line", "L"), st("Center Line", "centerline"), st("2-Point Rectangle", "rectangle", "R"),
            st("Center Rectangle", "centerrectangle"), st("Center Diameter Circle", "circle", "C"),
            st("3-Point Arc", "arc"), st("Polygon", "polygon"), st("Slot", "slot"), st("Ellipse", "ellipse"),
            st("Fit Point Spline", "spline"), st("Point", "point"), st("Trim", "trim", "T"), st("Extend", "extend"),
            st("Offset", "offset", "O"), st("Sketch Fillet", "fillet"), st("Project", "project", "P"),
            st("Sketch Dimension", "dimension", "D"),
            cmd("Normal / Construction", "X", "Sketch", "sketch", () => { sketchConstruction = !sketchConstruction }),
            cmd("Finish Sketch", "", "Sketch", "sketch", () => finishSketch()),
            cmd("Export Sketch as DXF", "", "Sketch", "sketch", () => exportDxf()),
            cmd("Export Face as DXF", "", "File", "model", () => exportDxf()),
            cmd("Batch Export", "", "File", "any", () => openBatchExport()),
            cmd("New Design", "Ctrl+N", "File", "any", () => newDesign()),
            cmd("Close Design", "Ctrl+W", "File", "any", () => closeDesign(fissionApp.activeIndex)),
            cmd("Projects", "Ctrl+Alt+P", "File", "any", () => openProjects()),
            cmd("Open", "Ctrl+O", "File", "any", () => openFile()),
            cmd("Save", "Ctrl+S", "File", "any", () => saveOrSaveAs()),
            cmd("Save As", "Ctrl+Shift+S", "File", "any", () => saveAs()),
            cmd("Version History", "", "File", "any", () => openVersionHistory()),
            cmd("Recover Autosave", "", "File", "any", () => openRecoverAutosave()),
            cmd("Send to Slicer", "", "Manufacture", "model", () => sendToOrca()),
            cmd("New Setup", "", "Manufacture", "model", () => mfgNewSetup()),
            cmd("Refresh Stock", "", "Manufacture", "model", () => mfgRefreshStock()),
            cmd("Tool Library", "", "Manufacture", "any", () => openManufacture(1)),
            cmd("Face", "", "Manufacture", "model", () => mfgAddOp("face", "Face")),
            cmd("2D Adaptive", "", "Manufacture", "model", () => mfgAddOp("2d-adaptive", "2D Adaptive")),
            cmd("2D Pocket", "", "Manufacture", "model", () => mfgAddOp("2d-pocket", "2D Pocket")),
            cmd("2D Contour", "", "Manufacture", "model", () => mfgAddOp("2d-contour", "2D Contour")),
            cmd("Slot", "", "Manufacture", "model", () => mfgAddOp("slot", "Slot")),
            cmd("Trace", "", "Manufacture", "model", () => mfgAddOp("trace", "Trace")),
            cmd("Bore", "", "Manufacture", "model", () => mfgAddOp("bore", "Bore")),
            cmd("Circular", "", "Manufacture", "model", () => mfgAddOp("circular", "Circular")),
            cmd("Engrave", "", "Manufacture", "model", () => mfgAddOp("engrave", "Engrave")),
            cmd("Chamfer", "", "Manufacture", "model", () => mfgAddOp("chamfer", "Chamfer")),
            cmd("Thread", "", "Manufacture", "model", () => mfgAddOp("thread", "Thread")),
            cmd("Drill", "", "Manufacture", "model", () => mfgAddOp("drill", "Drill")),
            cmd("Tap", "", "Manufacture", "model", () => mfgAddOp("tap", "Tap")),
            cmd("Thread Mill", "", "Manufacture", "model", () => mfgAddOp("thread-mill", "Thread Mill")),
            cmd("Adaptive Clearing", "", "Manufacture", "model", () => mfgAddOp("3d-adaptive", "Adaptive Clearing")),
            cmd("Pocket Clearing", "", "Manufacture", "model", () => mfgAddOp("3d-pocket", "Pocket Clearing")),
            cmd("Parallel", "", "Manufacture", "model", () => mfgAddOp("parallel", "Parallel")),
            cmd("3D Contour", "", "Manufacture", "model", () => mfgAddOp("3d-contour", "3D Contour")),
            cmd("Flat", "", "Manufacture", "model", () => mfgAddOp("flat", "Flat")),
            cmd("Scallop", "", "Manufacture", "model", () => mfgAddOp("scallop", "Scallop")),
            cmd("Radial", "", "Manufacture", "model", () => mfgAddOp("radial", "Radial")),
            cmd("Spiral", "", "Manufacture", "model", () => mfgAddOp("spiral", "Spiral")),
            cmd("Morphed Spiral", "", "Manufacture", "model", () => mfgAddOp("morphed-spiral", "Morphed Spiral")),
            cmd("Pencil", "", "Manufacture", "model", () => mfgAddOp("pencil", "Pencil")),
            cmd("Ramp", "", "Manufacture", "model", () => mfgAddOp("ramp", "Ramp")),
            cmd("Steep & Shallow", "", "Manufacture", "model", () => mfgAddOp("steep-shallow", "Steep & Shallow")),
            cmd("Project", "", "Manufacture", "model", () => mfgAddOp("project", "Project")),
            cmd("Morph", "", "Manufacture", "model", () => mfgAddOp("morph", "Morph")),
            cmd("Flow", "", "Manufacture", "model", () => mfgAddOp("flow", "Flow")),
            cmd("Blend", "", "Manufacture", "model", () => mfgAddOp("blend", "Blend")),
            cmd("Swarf", "", "Manufacture", "model", () => mfgAddOp("swarf", "Swarf")),
            cmd("Multi-Axis Contour", "", "Manufacture", "model", () => mfgAddOp("multi-contour", "Multi-Axis Contour")),
            cmd("Rotary / Wrap", "", "Manufacture", "model", () => mfgAddOp("rotary", "Rotary / Wrap")),
            cmd("Facing", "", "Manufacture", "model", () => mfgAddOp("turning-face", "Facing")),
            cmd("Profiling", "", "Manufacture", "model", () => mfgAddOp("turning-profile", "Profiling")),
            cmd("Grooving", "", "Manufacture", "model", () => mfgAddOp("turning-groove", "Grooving")),
            cmd("Parting", "", "Manufacture", "model", () => mfgAddOp("turning-part", "Parting")),
            cmd("Threading", "", "Manufacture", "model", () => mfgAddOp("turning-thread", "Threading")),
            cmd("Adaptive Roughing", "", "Manufacture", "model", () => mfgAddOp("turning-adaptive", "Adaptive Roughing")),
            cmd("Probe WCS", "", "Manufacture", "model", () => mfgAddOp("probe-wcs", "Probe WCS")),
            cmd("Probe Geometry", "", "Manufacture", "model", () => mfgAddOp("probe-geometry", "Probe Geometry")),
            cmd("Simulate", "", "Manufacture", "model", () => mfgSimulate()),
            cmd("Post Process", "", "Manufacture", "model", () => mfgPostProcess()),
            cmd("Manufacture Panel", "", "Manufacture", "any", () => openManufacture()),
            cmd("Export", "", "File", "any", () => exportFile()),
            cmd("Undo", "Ctrl+Z", "Edit", "any", () => doc.undo()),
            cmd("Redo", "Ctrl+Y", "Edit", "any", () => doc.redo()),
            cmd("Show/Hide Browser", "Ctrl+Alt+B", "Window", "any", () => { showBrowser = !showBrowser }),
            cmd("Show/Hide ViewCube", "Ctrl+Alt+V", "Window", "any", () => { showViewCube = !showViewCube }),
            cmd("Show/Hide Navigation Bar", "Ctrl+Alt+N", "Window", "any", () => { showNavBar = !showNavBar }),
            cmd("Reset to Default Layout", "Ctrl+Alt+R", "Window", "any", () => resetLayout()),
            cmd("Keyboard Shortcuts", "", "Help", "any", () => shortcutsDialog.open()),
            cmd("About Fission 360", "", "Help", "any", () => aboutDialog.open())
        ]
    }
    function resetLayout() {
        showBrowser = true; showViewCube = true; showNavBar = true; showProperties = false; panelPos = null
    }
    function openCommandSearch() { commandSearch.openSearch() }
    function openShortcuts() { shortcutsDialog.open() }
    function openAbout() { aboutDialog.open() }

    function openFile() { openDialog.open() }
    // DXF of the active sketch, or else of the picked planar face
    function exportDxf() {
        if (activeSketch) { dxfDialog.face = null; dxfDialog.open(); return }
        const face = selectedOf("face")[0]
        if (!face) { fissionApp.showMessage("Export DXF: open a sketch or pick a planar face first"); return }
        dxfDialog.face = face
        dxfDialog.open()
    }
    function openBatchExport() { batchExport.open() }
    function saveAs() {
        const p = doc.filePath.replace(/\\/g, "/")
        saveDialog.currentFolder = projects.urlOf(p.length ? p.substring(0, p.lastIndexOf("/")) : projects.root)
        saveDialog.selectedFile = projects.urlOf((p.length ? p.substring(0, p.lastIndexOf("/")) : projects.root) + "/" + doc.name + ".f360")
        saveDialog.open()
    }
    function openProjects() {
        const p = doc.filePath.replace(/\\/g, "/")
        projectsDrawer.show(p.length ? p.substring(0, p.lastIndexOf("/")) : projects.root)
    }
    function exportFile() { exportDialog.open() }
    function openParameters() { parametersDialog.open() }
    function closeParameters() { parametersDialog.close() }
    function openVersionHistory() { versionHistoryDialog.open() }
    function openRecoverAutosave() { recoverAutosaveDialog.open() }
    function sendToOrca() {
        const bodies = selection.ofKind("body").map(x => x.body)
        // The kernel drops hidden bodies (also explicitly selected ones): hidden means "do not print"
        fissionApp.sendToSlicer(bodies)
    }

    // ------------------------------------------------------------------ shortcuts

    // Fusion's default keyboard shortcuts. Single letters are ignored while a text field has focus.
    Shortcut {
        sequences: [StandardKey.Undo]
        context: Qt.ApplicationShortcut
        onActivated: { viewportItem.cancelSketchInput(); win.doc.undo() }
    }
    // Ctrl+Shift+Z redo (Ctrl+Y is also handled in C++ FissionApplication::eventFilter — more reliable)
    Shortcut {
        sequence: "Ctrl+Shift+Z"
        context: Qt.ApplicationShortcut
        onActivated: {
            viewportItem.cancelSketchInput()
            if (!win.doc || !win.doc.canRedo) {
                fissionApp.showMessage("Nothing to redo")
                return
            }
            win.doc.redo()
        }
    }
    Shortcut { sequences: [StandardKey.Save]; onActivated: win.saveOrSaveAs() }
    Shortcut { sequence: "Ctrl+Shift+S"; onActivated: win.saveAs() }
    Shortcut { sequences: [StandardKey.Open]; onActivated: win.openFile() }
    Shortcut { sequence: "Ctrl+Alt+P"; onActivated: win.openProjects() }
    Shortcut { sequences: [StandardKey.New]; onActivated: win.newDesign() }
    Shortcut { sequence: "Ctrl+W"; onActivated: win.closeDesign(fissionApp.activeIndex) }
    Shortcut { sequence: "Ctrl+Tab"; onActivated: win.switchDesign((fissionApp.activeIndex + 1) % fissionApp.documents.length) }
    Shortcut {
        sequence: "Ctrl+Shift+Tab"
        onActivated: win.switchDesign((fissionApp.activeIndex + fissionApp.documents.length - 1) % fissionApp.documents.length)
    }
    Shortcut { sequences: [StandardKey.SelectAll]; onActivated: win.selectAll() }
    Shortcut { sequence: "Ctrl+B"; onActivated: { win.doc.rebuild(); fissionApp.showMessage("Recomputed the timeline") } }
    Shortcut { sequence: "F6"; onActivated: viewportItem.renderer.fitAll(true) }
    Shortcut { sequence: "Ctrl+Alt+B"; onActivated: win.showBrowser = !win.showBrowser }
    Shortcut { sequence: "Ctrl+Alt+V"; onActivated: win.showViewCube = !win.showViewCube }
    Shortcut { sequence: "Ctrl+Alt+N"; onActivated: win.showNavBar = !win.showNavBar }
    Shortcut { sequence: "Ctrl+Alt+R"; onActivated: win.resetLayout() }
    Shortcut {
        sequence: "Esc"
        onActivated: {
            if (win.pointPick) { win.pointPick = null; fissionApp.showMessage("Point pick cancelled") }
            else if (win.sketchMove) win.finishSketchMove(false)
            else if (viewportItem.editingConstraint >= 0) viewportItem.editingConstraint = -1
            else if (viewportItem.sketchInputActive) viewportItem.cancelSketchInput()
            else if (win.sketchTool) win.sketchTool = ""
            else if (win.commandFeatureId) win.finishCommand(false)
            else if (win.pickingSketchPlane) { win.pickingSketchPlane = false; win.pendingSketchTool = ""; win.selection.filter = "" }
            else if (win.showRender) win.showRender = false
            else if (win.showManufacture) win.showManufacture = false
            else if (win.showAppearance) win.showAppearance = false
            else if (win.inspectMode) win.closeInspect()
            else win.selection.clear()
        }
    }
    Shortcut {
        sequences: ["Return", "Enter"]
        enabled: win.commandFeatureId.length > 0 || !!win.sketchMove
        onActivated: win.sketchMove ? win.finishSketchMove(true) : win.finishCommand(true)
    }
    Shortcut {
        sequences: [StandardKey.Delete]
        onActivated: {
            if (win.activeSketchId) {
                if (!viewportItem.deleteSelectedConstraint()) win.deleteSelectedSketchEntities()
                return
            }
            if (win.commandFeatureId) return
            const ids = []
            for (const it of win.selection.items)
                if (it.kind === "plane" || it.kind === "axis" || it.kind === "point") ids.push(it.id)
            const bodies = win.bodyIndices(win.selection.items.filter(x => x.kind === "body" || x.kind === "face"))
            if (!ids.length && !bodies.length) return
            win.selection.clear()
            if (bodies.length) win.removeBodies(bodies)
            for (const id of ids) win.deleteFeature(win.doc.featureById(id))
        }
    }
    // Single keys (Fusion defaults)
    Shortcut { sequence: "S"; onActivated: win.openCommandSearch() }
    Shortcut { sequence: "E"; onActivated: win.startCommand("Extrude") }
    Shortcut { sequence: "Q"; onActivated: win.pressPull() }
    Shortcut { sequence: "F"; onActivated: win.startCommand("Fillet") }
    Shortcut { sequence: "V"; onActivated: win.toggleSelectedVisibility() }
    Shortcut { sequence: "L"; onActivated: win.sketchToolKey("line") }
    Shortcut { sequence: "R"; onActivated: win.sketchToolKey("rectangle") }
    Shortcut { sequence: "C"; onActivated: win.sketchToolKey("circle") }
    Shortcut { sequence: "T"; onActivated: win.sketchToolKey("trim") }
    Shortcut { sequence: "O"; onActivated: win.sketchToolKey("offset") }
    Shortcut { sequence: "P"; onActivated: win.sketchToolKey("project") }
    Shortcut { sequence: "D"; onActivated: win.sketchToolKey("dimension") }
    Shortcut { sequence: "X"; enabled: !!win.activeSketchId; onActivated: win.sketchConstruction = !win.sketchConstruction }
    // Fusion tools that Fission 360 does not have yet: say so instead of doing nothing
    Shortcut { sequence: "H"; onActivated: win.startCommand("Hole") }
    Shortcut { sequence: "M"; onActivated: win.startCommand("Move") }
    Shortcut { sequence: "J"; onActivated: win.startCommand("Joint", { asBuilt: false }) }
    Shortcut { sequence: "Shift+J"; onActivated: win.startCommand("Joint", { asBuilt: true }) }
    Shortcut { sequence: "A"; onActivated: win.openAppearance() }
    Shortcut { sequence: "I"; onActivated: win.openInspect("measure") }
    Shortcut { sequence: "Shift+S"; onActivated: win.openScripts() }
    Shortcut { sequence: "Shift+N"; onActivated: win.toggleComponentColors() }
    // Off while a sketch tool is drawing or a feature shows a size handle, so digits go to the value boxes
    readonly property bool digitsToManip: !!viewportItem.manip && !activeSketchId
    readonly property bool selectionDigits: !(activeSketchId && sketchTool) && !digitsToManip
    Shortcut { sequence: "1"; enabled: win.selectionDigits; onActivated: win.setSelectionTool("window") }
    Shortcut { sequence: "2"; enabled: win.selectionDigits; onActivated: win.setSelectionTool("freeform") }
    Shortcut { sequence: "3"; enabled: win.selectionDigits; onActivated: win.setSelectionTool("paint") }
    // Like Fusion: with Fillet, Chamfer, Extrude ... active, typing a number replaces the size in the
    // handle's value box without clicking it first
    Instantiator {
        model: ["0", "1", "2", "3", "4", "5", "6", "7", "8", "9", ".", "-"]
        delegate: Shortcut {
            required property string modelData
            sequence: modelData
            enabled: win.digitsToManip
            onActivated: viewportItem.typeIntoManip(modelData)
        }
    }

    // ------------------------------------------------------------------ layout

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        CommandBar { app: win; Layout.fillWidth: true }
        Toolbar { id: toolbarItem; app: win; Layout.fillWidth: true }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            BrowserPanel {
                app: win
                visible: win.showBrowser
                Layout.preferredWidth: 250
                Layout.fillHeight: true
            }

            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true

                DropArea {
                    id: designDrop
                    anchors.fill: parent
                    z: 50
                    enabled: !win.inDrawing && !!win.doc
                    onEntered: (drag) => drag.accept(Qt.CopyAction)
                    onDropped: (drop) => {
                        if (!drop.hasUrls) return
                        drop.accept(Qt.CopyAction)
                        const urls = drop.urls
                        Qt.callLater(() => win.dropFiles(urls))
                    }
                    Rectangle {
                        anchors.fill: parent
                        visible: designDrop.containsDrag
                        color: Qt.rgba(0.2, 0.5, 0.9, 0.08)
                        border.color: Theme.accent
                        border.width: 2
                        Text {
                            anchors.centerIn: parent
                            text: "Drop to insert into " + (win.doc ? win.doc.name : "the design")
                            color: Theme.accent
                            font.bold: true
                            font.pixelSize: 15
                        }
                    }
                }

                Viewport {
                    id: viewportItem
                    app: win
                    anchors.fill: parent
                    visible: !win.inDrawing

                    FloatingPanel {
                        id: featureFloat
                        app: win
                        visible: win.commandFeatureId.length > 0
                        contentHeight: featureLoader.item ? featureLoader.item.implicitHeight + 2 : 200
                        Loader {
                            id: featureLoader
                            anchors.fill: parent
                            active: featureFloat.visible
                            sourceComponent: FeaturePanel { app: win; feature: win.commandFeature }
                        }
                    }
                    FloatingPanel {
                        id: sketchFloat
                        app: win
                        width: 300
                        visible: !win.commandFeatureId && win.activeSketchId.length > 0 && !win.sketchMove
                        contentHeight: sketchLoader.item ? sketchLoader.item.implicitHeight + 2 : 200
                        Loader {
                            id: sketchLoader
                            anchors.fill: parent
                            active: sketchFloat.visible
                            sourceComponent: SketchPanel { app: win }
                        }
                    }
                    FloatingPanel {
                        id: sketchMoveFloat
                        app: win
                        width: 300
                        visible: !!win.sketchMove
                        contentHeight: sketchMoveLoader.item ? sketchMoveLoader.item.implicitHeight + 2 : 200
                        Loader {
                            id: sketchMoveLoader
                            anchors.fill: parent
                            active: sketchMoveFloat.visible
                            sourceComponent: SketchMovePanel { app: win }
                        }
                    }
                    FloatingPanel {
                        id: inspectFloat
                        app: win
                        width: 300
                        visible: !win.commandFeatureId && !win.activeSketchId && !win.showAppearance && !win.showRender && !win.showManufacture
                                 && win.inspectMode.length > 0
                        contentHeight: inspectLoader.item ? inspectLoader.item.implicitHeight + 2 : 200
                        Loader {
                            id: inspectLoader
                            anchors.fill: parent
                            active: inspectFloat.visible
                            sourceComponent: InspectPanel { app: win }
                        }
                    }
                    FloatingPanel {
                        id: appearanceFloat
                        app: win
                        width: 340
                        visible: win.showAppearance && !win.commandFeatureId && !win.activeSketchId && !win.inDrawing
                        contentHeight: appearanceLoader.item ? appearanceLoader.item.implicitHeight + 2 : 480
                        Loader {
                            id: appearanceLoader
                            anchors.fill: parent
                            active: appearanceFloat.visible
                            sourceComponent: AppearanceDialog { app: win }
                        }
                    }
                    FloatingPanel {
                        id: renderFloat
                        app: win
                        width: 380
                        visible: win.showRender && !win.commandFeatureId && !win.activeSketchId && !win.inDrawing
                        contentHeight: renderLoader.item ? renderLoader.item.implicitHeight + 2 : 520
                        Loader {
                            id: renderLoader
                            anchors.fill: parent
                            active: renderFloat.visible
                            sourceComponent: RenderDialog { app: win }
                        }
                    }
                    FloatingPanel {
                        id: manufactureFloat
                        app: win
                        width: 400
                        visible: win.showManufacture && !win.commandFeatureId && !win.activeSketchId && !win.inDrawing
                        contentHeight: manufactureLoader.item ? manufactureLoader.item.implicitHeight + 2 : 480
                        Loader {
                            id: manufactureLoader
                            anchors.fill: parent
                            active: manufactureFloat.visible
                            sourceComponent: ManufactureDialog {
                                app: win
                                tabIndex: win.manufactureTab
                                onTabIndexChanged: win.manufactureTab = tabIndex
                            }
                        }
                    }
                }

                DrawingWorkspace {
                    id: drawingWorkspace
                    app: win
                    anchors.fill: parent
                    visible: win.inDrawing
                    drawingDoc: win.activeDrawing
                }
            }
        }

        TimelinePanel { app: win; Layout.fillWidth: true; visible: !win.inDrawing }
        StatusBar { id: statusBarItem; app: win; Layout.fillWidth: true }
    }
    property alias statusBar: statusBarItem
    property alias drawingWorkspace: drawingWorkspace

    ParametersDialog { id: parametersDialog; app: win }
    DocumentSettingsDialog { id: documentSettings; app: win }
    AppearanceSettingsDialog { id: appearanceSettings; app: win }
    VersionHistoryDialog { id: versionHistoryDialog; app: win }
    RecoverAutosaveDialog { id: recoverAutosaveDialog }
    readonly property alias displaySettingsPopup: appearanceSettings
    readonly property alias renderPopup: renderFloat
    CommandSearch { id: commandSearch; app: win }
    ShortcutsDialog { id: shortcutsDialog; app: win }
    AboutDialog { id: aboutDialog; app: win }
    ScriptsDialog { id: scriptsDialog; app: win }
    SheetMetalRulesDialog { id: sheetRulesDialog; app: win }
    property alias scriptsPopup: scriptsDialog
    property alias sheetRulesPopup: sheetRulesDialog
    BatchExportDialog { id: batchExport; app: win }
    readonly property alias batchExportDialog: batchExport
    FileDialog {
        id: mfgPostDialog
        title: "Export NC program"
        fileMode: FileDialog.SaveFile
        nameFilters: ["NC / G-code (*.nc *.tap *.gcode)", "All files (*)"]
        onAccepted: {
            let path = fissionApp.toLocalPath(selectedFile.toString())
            if (!path.toLowerCase().match(/\.(nc|tap|gcode)$/)) path += ".nc"
            const out = doc.exportMfgGCode(path)
            fissionApp.showMessage(out ? ("Posted → " + out) : (doc.lastError || "Post failed"))
        }
    }
    Loader {
        id: mcmasterLoader
        active: false
        // source (not sourceComponent) so McMasterInsertDialog.qml / QtWebEngine are not parsed at startup
        source: active ? "McMasterInsertDialog.qml" : ""
        onLoaded: {
            if (!item) return
            item.app = win
            item.open()
        }
    }
    readonly property alias mcmasterInsertDialog: mcmasterLoader
    ProjectsPanel { id: projectsDrawer; app: win }
    readonly property alias projectsPanel: projectsDrawer
    CreateDrawingDialog {
        id: createDrawingDialog
        app: win
        onAccepted: (input) => win.finishCreateDrawing(input)
    }
    readonly property alias createDrawingPopup: createDrawingDialog

    Loader {
        active: uiTest.enabled && !uiTest.perfFile
        sourceComponent: UiTest { app: win }
    }
    Loader {
        active: !!uiTest.perfFile
        sourceComponent: UiPerf { app: win }
    }
}
