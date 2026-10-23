// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import "../FissionApplication"

Item {
    id: ws
    anchors.fill: parent

    property var app
    property var drawingDoc: null
    property var activeSheet: null
    property string activeTool: "select"
    property string placeOrientation: "front"
    property var selectedView: null
    property string selectedNoteId: ""
    property string editingNoteId: ""
    property string editingDraft: ""
    property bool noteCancel: false
    // Set while the inspector closes so the press cannot land on the sheet underneath.
    property bool suppressSheetPress: false
    readonly property var orientationIds: [
        "front", "top", "right", "left", "back", "bottom",
        "isoFrontTopRight", "isoFrontTopLeft", "isoFrontBottomRight", "isoFrontBottomLeft",
        "isoBackTopRight", "isoBackTopLeft", "isoBackBottomRight", "isoBackBottomLeft"
    ]
    readonly property var orientationLabels: [
        "Front", "Top", "Right", "Left", "Back", "Bottom",
        "Front top right", "Front top left", "Front bottom right", "Front bottom left",
        "Back top right", "Back top left", "Back bottom right", "Back bottom left"
    ]
    property real sheetZoom: 1.0
    property real sheetPanX: 0
    property real sheetPanY: 0
    property string sheetSize: "A3"
    property int projectionAngle: 1   // 0=first, 1=third
    property int standard: 0          // 0=ISO, 1=ASME
    property bool showHidden: false

    // Paper size follows the sheet (A0–A4, ANSI A–E, or a custom size). The fallback is A3.
    readonly property real mmW: activeSheet && activeSheet.width > 0 ? activeSheet.width
        : sheetSize === "A0" ? 1189 : sheetSize === "A1" ? 841 : sheetSize === "A2" ? 594
        : sheetSize === "A4" ? 297 : sheetSize === "A" ? 279 : sheetSize === "B" ? 432
        : sheetSize === "C" ? 559 : sheetSize === "D" ? 864 : sheetSize === "E" ? 1118 : 420
    readonly property real mmH: activeSheet && activeSheet.height > 0 ? activeSheet.height
        : sheetSize === "A0" ? 841 : sheetSize === "A1" ? 594 : sheetSize === "A2" ? 420
        : sheetSize === "A4" ? 210 : sheetSize === "A" ? 216 : sheetSize === "B" ? 279
        : sheetSize === "C" ? 432 : sheetSize === "D" ? 559 : sheetSize === "E" ? 864 : 297

    // Interaction state
    property bool placing: false
    property real ghostX: 0
    property real ghostY: 0
    property bool dragView: false
    property real dragOffX: 0
    property real dragOffY: 0
    // Dimension tool: dimension (the picks decide), linear, aligned, angular, radius, diameter. Picks are
    // drawingManager.pickSheetEdge results (a line, circle or point of a view); the next click places it.
    property string dimTool: "dimension"
    property var dimPicks: []
    property var dimHover: ({})
    property var dimPreview: null
    // Bumped on every annotation change: an edit keeps annotationCount, so delegates re-read through this
    property int annRev: 0
    Connections {
        target: ws.activeSheet
        function onAnnotationsChanged() { ws.annRev++ }
    }
    property real sectionStartX: 0
    property real sectionStartY: 0
    property bool sectionDragging: false
    property real detailStartX: 0
    property real detailStartY: 0
    property bool detailDragging: false

    function syncSheet() {
        if (drawingDoc && drawingDoc.sheetCount > 0) {
            const idx = Math.max(0, Math.min(drawingDoc.activeSheetIndex, drawingDoc.sheetCount - 1))
            activeSheet = drawingDoc.sheet(idx)
            if (activeSheet && activeSheet.size)
                sheetSize = activeSheet.size
        } else {
            activeSheet = null
        }
        if (drawingDoc) {
            standard = drawingDoc.standard === "ASME" ? 1 : 0
            projectionAngle = drawingDoc.projectionAngle === "first" ? 0 : 1
        }
    }

    onDrawingDocChanged: {
        syncSheet()
        sheetZoom = 1; sheetPanX = 0; sheetPanY = 0
        selectedView = null
        placing = false
        activeTool = "select"
        Qt.callLater(maybeAutoPlaceBase)
    }
    Connections {
        target: drawingDoc
        function onSheetsChanged() {
            const keep = selectedView ? selectedView.id : ""
            syncSheet()
            selectedView = null
            if (!keep || !activeSheet) return
            for (let i = 0; i < activeSheet.viewCount; ++i) {
                const v = activeSheet.view(i)
                if (v && v.id === keep) { selectedView = v; break }
            }
        }
        function onActiveSheetChanged() { syncSheet(); selectedView = null }
    }

    function designDoc() { return app ? app.doc : null }

    // One drawing undo step. Nested calls share the snapshot taken by the outermost edit.
    function edit(fn) {
        if (drawingDoc) drawingDoc.beginEdit()
        try { fn() } finally {
            if (drawingDoc) drawingDoc.endEdit()
            // The drawing is saved in the design's .f360, so a drawing edit leaves the design unsaved
            if (app && app.doc) app.doc.markModified()
        }
    }

    // The title block is picked like a note ("__title"); the inspector edits its fields or deletes it
    readonly property bool titleSelected: selectedNoteId === "__title"
    function toggleTitleBlock() {
        if (!activeSheet) return
        const on = !activeSheet.titleBlockVisible
        edit(() => { activeSheet.titleBlockVisible = on })
        if (!on && titleSelected) selectedNoteId = ""
        fissionApp.showMessage(on ? "Title block shown" : "Title block deleted (DRAWING ▸ Title Block brings it back)")
    }

    readonly property var scaleSteps: [0.1, 0.2, 0.25, 0.5, 1, 2, 4, 5, 10]
    function stepScale(dir) {
        if (!selectedView) return
        const s = Number(selectedView.scale)
        let best = 0
        for (let i = 0; i < scaleSteps.length; ++i)
            if (scaleSteps[i] <= s + 0.001) best = i
        const n = Math.max(0, Math.min(scaleSteps.length - 1, best + dir))
        const view = selectedView
        edit(() => { view.scale = scaleSteps[n] })
    }

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

    // Pick a sheet scale so the outline is large enough to read when the sheet is fit.
    function niceScale(viewW, viewH) {
        const w = Math.max(1, viewW), h = Math.max(1, viewH)
        const fit = Math.min(mmW * 0.34 / w, mmH * 0.34 / h)
        const nice = [0.05, 0.1, 0.2, 0.25, 0.5, 1, 2, 4, 5, 10, 20, 50, 100]
        let best = nice[0]
        for (let i = 0; i < nice.length; ++i) if (nice[i] <= fit + 1e-9) best = nice[i]
        if (best * Math.min(w, h) < 25) {
            const i = nice.indexOf(best)
            if (i >= 0 && i + 1 < nice.length) best = nice[i + 1]
        }
        return best
    }

    function orientationIndex(id) {
        let key = id || "front"
        if (key === "isometric" || key === "home") key = "isoBackTopRight"
        const i = orientationIds.indexOf(key)
        return i < 0 ? 0 : i
    }

    function noteTextOf(id) {
        if (!activeSheet || !id) return ""
        const n = activeSheet.annotationCount
        for (let i = 0; i < n; ++i) {
            const a = activeSheet.annotation(i)
            if (a && a.id === id) return a.text || ""
        }
        return ""
    }

    function beginNoteEdit(id) {
        if (!id) return
        selectedView = null
        selectedNoteId = id
        noteCancel = false
        editingDraft = noteTextOf(id)
        if (!editingDraft) editingDraft = "Note"
        editingNoteId = id
        if (noteField) noteField.text = editingDraft
    }

    function commitNoteEdit() {
        if (noteCancel) { noteCancel = false; return }
        const id = editingNoteId
        const text = editingDraft
        editingNoteId = ""
        if (!id || !activeSheet) return
        if (text === noteTextOf(id)) return
        edit(() => activeSheet.updateAnnotation(id, { text: text }))
        if (noteField && selectedNoteId === id) noteField.text = text
    }

    function cancelNoteEdit() {
        noteCancel = true
        const id = editingNoteId || selectedNoteId
        editingNoteId = ""
        editingDraft = noteTextOf(id)
        if (noteField && selectedNoteId === id) noteField.text = editingDraft
        Qt.callLater(() => { noteCancel = false })
    }

    function dismissInspector() {
        suppressSheetPress = true
        if (editingNoteId) cancelNoteEdit()
        selectedView = null
        selectedNoteId = ""
        Qt.callLater(() => { suppressSheetPress = false })
    }

    function idleStatus() {
        fissionApp.showMessage("Wheel zoom · middle-drag pan · Shift+drag a view to project · F fit")
    }

    function finishPlace(msg) {
        activeTool = "select"
        placing = false
        dimPicks = []
        dimHover = ({})
        dimPreview = null
        sectionDragging = false
        detailDragging = false
        fissionApp.showMessage(msg || "Wheel zoom · middle-drag pan · Shift+drag a view to project · F fit")
    }

    function cancelPlace() { finishPlace() }

    function beginBaseView(orient) {
        if (orient) placeOrientation = orient
        activeTool = "baseView"
        placing = true
        fissionApp.showMessage("Base View — click the sheet to place (Esc cancels)")
    }

    function beginProjectMode() {
        if (!selectedView) {
            fissionApp.showMessage("Select a parent view first, then Shift+drag to project")
            return
        }
        activeTool = "projectedView"
        fissionApp.showMessage("Projected View — Shift+drag from the parent view")
    }

    function beginSectionMode() {
        if (!selectedView) {
            fissionApp.showMessage("Select a parent view first")
            return
        }
        activeTool = "sectionView"
        placing = true
        fissionApp.showMessage("Section View — drag a cut line across the parent view")
    }

    function beginDetailMode() {
        if (!selectedView) {
            fissionApp.showMessage("Select a parent view first")
            return
        }
        activeTool = "detailView"
        placing = true
        fissionApp.showMessage("Detail View — click and drag a circle on the parent view")
    }

    function beginDimensionMode(tool) {
        dimTool = tool || "dimension"
        dimPicks = []
        dimHover = ({})
        dimPreview = null
        selectedView = null
        selectedNoteId = ""
        activeTool = "dimension"
        fissionApp.showMessage(dimHint())
    }

    function dimHint() {
        const names = { dimension: "Dimension", linear: "Linear Dimension", aligned: "Aligned Dimension",
                        angular: "Angular Dimension", radius: "Radius Dimension", diameter: "Diameter Dimension" }
        const name = names[dimTool] || "Dimension"
        if (!dimPicks.length) {
            if (dimTool === "angular") return name + " — click the first line (Esc finishes)"
            if (dimTool === "radius" || dimTool === "diameter") return name + " — click an arc or circle (Esc finishes)"
            return name + " — click a line, circle or corner (Esc finishes)"
        }
        if (dimTool === "angular" && dimPicks.length === 1) return name + " — click the second line"
        if (dimPicks.length === 1 && dimPicks[0].kind === "point") return name + " — click a second corner, line or circle"
        return name + " — click where the dimension goes" + (dimPicks.length === 1 && dimTool !== "radius"
                && dimTool !== "diameter" ? ", or a second line, circle or corner" : "")
    }

    function dimTolerance() { return 6 / Math.max(0.01, sheetCanvas.fitScale * sheetZoom) }

    function samePick(a, b) {
        return !!a && !!b && a.viewId === b.viewId && a.kind === b.kind
            && (a.kind === "point" ? Math.hypot(a.x - b.x, a.y - b.y) < 1e-6 : a.edge === b.edge)
    }

    // A click with the dimension tool: a pick, or (once the picks make a dimension) its place
    function dimensionClick(p) {
        if (!activeSheet) return
        const mgr = fissionApp.drawingManager
        const hit = mgr.pickSheetEdge(activeSheet, p.x, p.y, dimTolerance())
        const has = hit && !!hit.kind
        const picks = dimPicks
        const oneOnly = dimTool === "radius" || dimTool === "diameter"
        if (!picks.length) {
            if (!has) { fissionApp.showMessage("Nothing to measure there — " + dimHint()); return }
            dimPicks = [hit]
            dimPreview = null
            fissionApp.showMessage(dimHint())
            return
        }
        const needSecond = dimTool === "angular" || picks[0].kind === "point"
        if (picks.length === 1 && has && !oneOnly && !samePick(hit, picks[0]) && hit.viewId !== picks[0].viewId) {
            fissionApp.showMessage("Both picks must be on the same view")
            return
        }
        if (picks.length === 1 && has && !oneOnly && !samePick(hit, picks[0])) {
            dimPicks = [picks[0], hit]
            dimPreview = null
            fissionApp.showMessage(dimHint())
            return
        }
        if (picks.length === 1 && needSecond) { fissionApp.showMessage(dimHint()); return }
        const ann = mgr.buildDimension(dimTool, picks[0], picks[1] || ({}), p.x, p.y, picks[0].viewScale || 1)
        if (ann.error) { fissionApp.showMessage(ann.error); return }
        // Stored on its view: it moves with the view and is measured again when the design changes
        const attached = mgr.attachDimension(activeSheet, ann, picks)
        edit(() => activeSheet.addAnnotation(attached))
        dimPicks = []
        dimPreview = null
        fissionApp.showMessage(mgr.dimensionLabel(ann) + " placed — " + dimHint())
    }

    function dimensionMove(p) {
        if (!activeSheet) return
        const mgr = fissionApp.drawingManager
        dimHover = mgr.pickSheetEdge(activeSheet, p.x, p.y, dimTolerance())
        const picks = dimPicks
        if (!picks.length || ((dimTool === "angular" || picks[0].kind === "point") && picks.length < 2)) {
            dimPreview = null
            return
        }
        const ann = mgr.buildDimension(dimTool, picks[0], picks[1] || ({}), p.x, p.y, picks[0].viewScale || 1)
        dimPreview = ann.error ? null : ann
    }

    // Delete: the selected dimension or note, else the selected view
    function deleteSelection() {
        if (!activeSheet || editingNoteId) return
        if (titleSelected) { toggleTitleBlock(); return }
        if (selectedNoteId) {
            const id = selectedNoteId
            selectedNoteId = ""
            edit(() => activeSheet.removeAnnotation(id))
            fissionApp.showMessage("Deleted")
        } else if (selectedView) {
            const id = selectedView.id
            selectedView = null
            edit(() => activeSheet.removeView(id))
            fissionApp.showMessage("View deleted")
        }
    }

    function annotationById(id) {
        if (!activeSheet || !id) return null
        for (let i = 0; i < activeSheet.annotationCount; ++i) {
            const a = activeSheet.annotation(i)
            if (a && a.id === id) return a
        }
        return null
    }
    readonly property var selectedAnn: { annRev; return annotationById(selectedNoteId) }
    function setAnn(fields) {
        const id = selectedNoteId
        if (id && activeSheet) edit(() => activeSheet.updateAnnotation(id, fields))
    }

    function beginTextMode() {
        activeTool = "text"
        placing = true
        fissionApp.showMessage("Text — click to place a note")
    }

    function beginHoleNoteMode() {
        activeTool = "holeNote"
        placing = true
        fissionApp.showMessage("Hole Note — click the sheet to place (Esc cancels)")
    }

    function beginCenterMarkMode() {
        activeTool = "centerMark"
        placing = true
        fissionApp.showMessage("Center Mark — click the sheet to place (Esc cancels)")
    }

    function beginAuxMode() {
        if (!selectedView) {
            fissionApp.showMessage("Select a parent view first")
            return
        }
        activeTool = "auxiliaryView"
        placing = true
        fissionApp.showMessage("Auxiliary View — click the sheet to place (Esc cancels)")
    }

    function toggleHiddenLines() {
        showHidden = !showHidden
        if (selectedView) {
            selectedView.showHiddenLines = showHidden
            fissionApp.drawingManager.projectView(selectedView, designDoc(), drawingDoc)
        }
        fissionApp.showMessage(showHidden ? "Hidden lines on" : "Hidden lines off")
    }

    function beginPartsListMode() {
        activeTool = "partsList"
        placing = true
        fissionApp.showMessage("Parts List — click to place the table")
    }

    function beginRevisionMode() {
        activeTool = "revision"
        placing = true
        fissionApp.showMessage("Revision Table — click to place the table")
    }

    function tidyUp() {
        if (!activeSheet) return
        // Auto-arrange: place base view top-left, projected views around it
        const views = activeSheet.views
        if (views.length === 0) return
        const base = views.find(v => v.type === "base")
        if (!base) return
        edit(() => {
        base.position = Qt.point(30, 30)
        let i = 0
        for (const v of views) {
            if (v === base) continue
            i++
            const gap = 10
            const w = Math.max(20, v.width * v.scale)
            const h = Math.max(16, v.height * v.scale)
                if (i % 2 === 1)
                v.position = Qt.point(base.position.x + w + gap, base.position.y + (i / 2) * (h + gap))
                else
                v.position = Qt.point(base.position.x, base.position.y + h + gap + (i / 2) * (h + gap))
            }
        })
        fissionApp.showMessage("Tidied up")
    }

    // Zoom is a transform on the sheet (origin at the canvas centre), so curves are not rebuilt
    // on each wheel tick. The pan update keeps the point under the cursor fixed.
    function zoomAt(factor, sx, sy) {
        const old = sheetZoom
        const next = Math.max(0.25, Math.min(6, sheetZoom * factor))
        if (old > 0 && sheetXform) {
            const k = next / old
            const ox = sheetXform.width * 0.5
            const oy = sheetXform.height * 0.5
            sheetPanX = (sx - ox) * (1 - k) + sheetPanX * k
            sheetPanY = (sy - oy) * (1 - k) + sheetPanY * k
        }
        sheetZoom = next
    }

    function fitSheet() {
        sheetZoom = 1
        sheetPanX = 0
        sheetPanY = 0
    }

    // px, py are in the sheet item's own pixels (already inside the zoom transform).
    function paperToMm(px, py) {
        const s = Math.max(0.01, sheetCanvas.fitScale)
        return Qt.point(px / s, py / s)
    }

    function projectOnto(view) {
        if (!view) return false
        const ok = fissionApp.drawingManager.projectView(view, designDoc(), drawingDoc)
        if (!ok) fissionApp.showMessage(fissionApp.drawingManager.lastError || "Projection failed")
        return ok
    }

    // One projection, on this UI thread, then a scale that fills about a third of the sheet.
    function placeBaseAt(mmX, mmY) {
        if (!activeSheet) return
        let view = null
        edit(() => {
        view = activeSheet.addView("base", {
            position: Qt.point(mmX, mmY),
            scale: 1.0,
            orientation: placeOrientation,
            showHiddenLines: showHidden
        })
        projectOnto(view)
        const sc = niceScale(view.width, view.height)
        view.scale = sc
        const vw = Math.max(20, view.width * sc)
        const vh = Math.max(16, view.height * sc)
        view.position = Qt.point(mmX - vw * 0.5, mmY - vh * 0.5)
        selectedView = view
        })
        finishPlace("Base view placed (" + drawingScaleLabel(view ? view.scale : 1) + ")")
    }

    function regenerateAll() {
        if (!activeSheet) return
        let n = 0
        edit(() => { n = fissionApp.drawingManager.regenerateSheet(activeSheet, designDoc(), drawingDoc) })
        if (drawingDoc && designDoc()) {
            drawingDoc.setSourceDesignRevision(designDoc().contentRevision)
            designDoc().linkAsset("drawing", drawingDoc.id, drawingDoc.name)
        }
        fissionApp.showMessage("Regenerated " + n + " view(s)")
    }
    function exportPdf() { exportDialog.kind = "pdf"; exportDialog.open() }
    function exportDxf() { exportDialog.kind = "dxf"; exportDialog.open() }
    function exportPng() { exportDialog.kind = "png"; exportDialog.open() }
    function exportTiff() { exportDialog.kind = "tiff"; exportDialog.open() }
    function addPartsList() {
        if (!activeSheet) return
        edit(() => fissionApp.drawingManager.addPartsList(activeSheet, designDoc(), Qt.point(20, 40)))
        fissionApp.showMessage("Parts list placed")
    }
    function addRevisionTable() {
        if (!activeSheet) return
        edit(() => fissionApp.drawingManager.addRevisionTable(activeSheet, Qt.point(20, 40)))
        fissionApp.showMessage("Revision table placed")
    }
    function addBendTable() {
        if (!activeSheet) return
        edit(() => fissionApp.drawingManager.addBendTable(activeSheet, designDoc(), Qt.point(20, 40)))
        fissionApp.showMessage("Bend table placed")
    }
    function insertImage() { imageDialog.open() }

    function orientationForProject(parentOrient, dir) {
        const first = drawingDoc && drawingDoc.projectionAngle === "first"
        const map = {
            front:  { right: "right", left: "left", up: "top", down: "bottom" },
            back:   { right: "left", left: "right", up: "top", down: "bottom" },
            top:    { right: "right", left: "left", up: "back", down: "front" },
            bottom: { right: "right", left: "left", up: "front", down: "back" },
            right:  { right: "back", left: "front", up: "top", down: "bottom" },
            left:   { right: "front", left: "back", up: "top", down: "bottom" },
            isometric: { right: "right", left: "left", up: "top", down: "front" }
        }
        let d = dir
        if (first) {
            if (d === "right") d = "left"
            else if (d === "left") d = "right"
            else if (d === "up") d = "down"
            else if (d === "down") d = "up"
        }
        const row = map[parentOrient] || map.front
        return row[d] || "right"
    }

    function projectFrom(view, dir) {
        if (!view || !activeSheet) return
        edit(() => { projectFromNow(view, dir) })
    }
    function projectFromNow(view, dir) {
        if (!view || !activeSheet) return
        const orient = orientationForProject(view.orientation || "front", dir)
        const gap = 20
        let x = view.position.x, y = view.position.y
        if (dir === "right") x += view.width * view.scale + gap
        else if (dir === "left") x -= view.width * view.scale + gap
        else if (dir === "down") y += view.height * view.scale + gap
        else y -= view.height * view.scale + gap
        const child = activeSheet.addView("projected", {
            position: Qt.point(x, y),
            scale: view.scale,
            orientation: orient,
            parentViewId: view.id,
            projectDirection: dir,
            showHiddenLines: showHidden
        })
        projectOnto(child)
        selectedView = child
        finishPlace("Projected " + orient + " view")
    }

    function maybeAutoPlaceBase() {
        if (!activeSheet) return
        if (activeSheet.viewCount > 0) {
            // Saved curves stay on the sheet until they are projected again. Opening the
            // drawing rebuilds them so a round edge is not left as the old coarse polyline.
            if (designDoc() && designDoc().bodyCount > 0)
                fissionApp.drawingManager.regenerateSheet(activeSheet, designDoc(), drawingDoc)
            return
        }
        if (!designDoc() || designDoc().bodyCount < 1) {
            finishPlace("Drawing opened")
            return
        }
        // Project the current design once, here on the UI thread, then leave placement mode.
        placeOrientation = "front"
        placeBaseAt(mmW * 0.38, mmH * 0.40)
    }

    // The stock combo list is positioned under the control. Once it is reparented to the
    // window overlay, that position is the top-left of the window, which lands on the file browser.
    function pinComboPopup(box) {
        if (!box || !box.popup) return
        box.popup.parent = Overlay.overlay
        box.popup.z = 2000
        box.popup.aboutToShow.connect(function() {
            const p = box.mapToItem(Overlay.overlay, 0, box.height)
            box.popup.x = p.x
            box.popup.y = p.y
            box.popup.width = box.width
        })
    }

    // Commands live on the DRAWING ribbon. Fit stays on the sheet because it is the
    // zoom reset for the paper itself.
    Rectangle {
        id: sheetCanvas
        anchors.fill: parent
        color: "#1A1A1A"
        clip: true

        readonly property real fitScale: Math.min(
            (sheetCanvas.width - 40) / ws.mmW,
            (sheetCanvas.height - 40) / ws.mmH
        )

        // Pan and zoom are one transform, so wheel and drag do not rebuild the sheet.
        Item {
            id: sheetXform
            width: parent.width
            height: parent.height
            x: ws.sheetPanX
            y: ws.sheetPanY
            scale: ws.sheetZoom
            transformOrigin: Item.Center

        Rectangle {
            id: sheet
            anchors.centerIn: parent
            width: ws.mmW * sheetCanvas.fitScale
            height: ws.mmH * sheetCanvas.fitScale
            color: "white"
            border.color: "#333"
            border.width: 1

            // ── TITLE BLOCK ───────────────────────────────────────────────
            Rectangle {
                id: titleBlock
                visible: !ws.activeSheet || ws.activeSheet.titleBlockVisible
                x: parent.width - 180 * sheetCanvas.fitScale
                y: parent.height - 60 * sheetCanvas.fitScale
                z: 5
                width: 180 * sheetCanvas.fitScale
                height: 60 * sheetCanvas.fitScale
                color: "white"
                border.color: ws.titleSelected ? "#1a6fd1" : "#333"
                border.width: ws.titleSelected ? 2 : 1

                // Click to edit its fields in the inspector; Delete removes it
                MouseArea {
                    anchors.fill: parent
                    enabled: ws.activeTool === "select"
                    onClicked: {
                        if (ws.editingNoteId) ws.commitNoteEdit()
                        ws.selectedView = null
                        ws.selectedNoteId = "__title"
                        ws.forceActiveFocus()
                    }
                }

                Text {
                    anchors.top: parent.top
                    anchors.left: parent.left
                    anchors.margins: 6 * sheetCanvas.fitScale
                    text: (ws.activeSheet && ws.activeSheet.partName) ? ws.activeSheet.partName : "Fission 360"
                    font.pixelSize: 14 * sheetCanvas.fitScale
                    font.bold: true
                }

                Text {
                    anchors.top: parent.top
                    anchors.right: parent.right
                    anchors.margins: 6 * sheetCanvas.fitScale
                    text: (ws.activeSheet ? ws.activeSheet.size : ws.sheetSize)
                          + " · " + (ws.drawingDoc ? ws.drawingDoc.standard : (ws.standard === 0 ? "ISO" : "ASME"))
                          + " · " + ((ws.drawingDoc ? ws.drawingDoc.projectionAngle === "first" : ws.projectionAngle === 0) ? "1st" : "3rd")
                          + " Angle"
                    font.pixelSize: 10 * sheetCanvas.fitScale
                    color: "#666"
                }

                Text {
                    anchors.bottom: parent.bottom
                    anchors.left: parent.left
                    anchors.margins: 6 * sheetCanvas.fitScale
                    text: {
                        const rev = ws.activeSheet ? ws.activeSheet.revision : "A"
                        const who = ws.activeSheet && ws.activeSheet.drawnBy ? ws.activeSheet.drawnBy : ""
                        const when = ws.activeSheet && ws.activeSheet.date ? ws.activeSheet.date : Qt.formatDate(new Date(), "yyyy-MM-dd")
                        return (who ? who + "  " : "") + "Rev " + rev + "  |  " + when
                    }
                    font.pixelSize: 10 * sheetCanvas.fitScale
                    color: "#666"
                }
            }

            // ── VIEWS ─────────────────────────────────────────────────────
            Repeater {
                model: ws.activeSheet ? ws.activeSheet.viewCount : 0
                delegate: DrawingViewItem {
                    required property int index
                    readonly property var viewObj: ws.activeSheet ? ws.activeSheet.view(index) : null
                    view: viewObj
                    selection: ws.app ? ws.app.selection : null
                    paperScale: sheetCanvas.fitScale
                    interactive: ws.activeTool === "select" || ws.activeTool === "projectedView"
                    selected: viewObj && ws.selectedView && viewObj.id === ws.selectedView.id
                    x: viewObj ? viewObj.position.x * paperScale - framePad : 0
                    y: viewObj ? viewObj.position.y * paperScale - framePad : 0
                    onClicked: ws.selectedView = viewObj
                    onDragFinished: (nx, ny) => {
                        if (!viewObj) return
                        const v = viewObj
                        ws.edit(() => { v.position = Qt.point((nx + framePad) / paperScale, (ny + framePad) / paperScale) })
                    }
                    onProjectDrag: (dir) => ws.projectFrom(viewObj, dir)
                }
            }

            Repeater {
                model: ws.activeSheet ? ws.activeSheet.annotationCount : 0
                delegate: Item {
                    id: annItem
                    required property int index
                    readonly property var ann: { ws.annRev; return ws.activeSheet ? ws.activeSheet.annotation(index) : ({}) }
                    readonly property bool movable: ann.kind === "text" || ann.kind === "holeNote"
                    x: (ann.x || 0) * sheetCanvas.fitScale
                    y: (ann.y || 0) * sheetCanvas.fitScale
                    // Above the views, so a note placed on a view can still be picked and dragged
                    z: 30
                    width: movable ? Math.max(12, noteLabel.implicitWidth, noteInput.visible ? Math.max(80, noteInput.implicitWidth) : 0) : 1
                    height: movable ? Math.max(14, noteLabel.implicitHeight) : 1
                    Canvas {
                        width: 240; height: 80
                        anchors.centerIn: parent
                        visible: ann.kind === "leader"   // dimensions are drawn by dimLayer
                        onPaint: {
                            const ctx = getContext("2d")
                            ctx.reset()
                            ctx.strokeStyle = "#222"
                            ctx.lineWidth = 1
                            const x2 = ((ann.x2 || ann.x) - ann.x) * sheetCanvas.fitScale
                            const y2 = ((ann.y2 || ann.y) - ann.y) * sheetCanvas.fitScale
                            ctx.beginPath(); ctx.moveTo(0, 0); ctx.lineTo(x2, y2); ctx.stroke()
                            const len = Math.hypot(x2, y2) || 1
                            const ux = x2 / len, uy = y2 / len
                            function arrow(atx, aty, sx, sy) {
                                ctx.beginPath()
                                ctx.moveTo(atx, aty)
                                ctx.lineTo(atx - sx * 7 + sy * 3, aty - sy * 7 - sx * 3)
                                ctx.lineTo(atx - sx * 7 - sy * 3, aty - sy * 7 + sx * 3)
                                ctx.closePath(); ctx.fillStyle = "#222"; ctx.fill()
                            }
                            arrow(0, 0, -ux, -uy)
                            arrow(x2, y2, ux, uy)
                        }
                    }
                    Text {
                        id: noteLabel
                        visible: (ann.kind === "holeNote" || ann.kind === "text") && ws.editingNoteId !== ann.id
                        text: ann.text || (ann.kind === "holeNote" ? "Hole" : "")
                        font.pixelSize: Math.max(4, (ann.fontSize || 3.5) * sheetCanvas.fitScale)
                        font.bold: !!ann.bold
                        font.italic: !!ann.italic
                        color: ws.selectedNoteId === ann.id && ann.kind === "text" ? "#1a4f9c" : "#222"
                    }
                    TextInput {
                        id: noteInput
                        z: 2
                        visible: ann.kind === "text" && ws.editingNoteId === ann.id
                        font.pixelSize: noteLabel.font.pixelSize
                        font.bold: noteLabel.font.bold
                        font.italic: noteLabel.font.italic
                        color: "#111"
                        selectByMouse: true
                        width: Math.max(80, implicitWidth + 8)
                        onTextEdited: ws.editingDraft = text
                        onAccepted: ws.commitNoteEdit()
                        onActiveFocusChanged: if (!activeFocus && ws.editingNoteId === ann.id) ws.commitNoteEdit()
                        Keys.onEscapePressed: (e) => { ws.cancelNoteEdit(); e.accepted = true }
                        onVisibleChanged: if (visible) { text = ws.editingDraft; forceActiveFocus(); selectAll() }
                    }
                    MouseArea {
                        anchors.fill: parent
                        z: 1
                        enabled: annItem.movable && ws.editingNoteId !== ann.id && ws.activeTool === "select"
                        acceptedButtons: Qt.LeftButton
                        cursorShape: drag.active ? Qt.ClosedHandCursor : Qt.OpenHandCursor
                        // Drag a note to move it; a click selects it, a double-click edits it on the sheet
                        drag.target: annItem
                        drag.threshold: 4
                        onReleased: {
                            if (annItem.x === (ann.x || 0) * sheetCanvas.fitScale && annItem.y === (ann.y || 0) * sheetCanvas.fitScale)
                                return
                            const id = ann.id
                            const nx = annItem.x / sheetCanvas.fitScale, ny = annItem.y / sheetCanvas.fitScale
                            ws.edit(() => ws.activeSheet.updateAnnotation(id, { x: nx, y: ny }))
                            annItem.x = Qt.binding(() => (annItem.ann.x || 0) * sheetCanvas.fitScale)
                            annItem.y = Qt.binding(() => (annItem.ann.y || 0) * sheetCanvas.fitScale)
                        }
                        onClicked: {
                            ws.selectedView = null
                            ws.selectedNoteId = ann.id
                            ws.editingDraft = ann.text || "Note"
                            if (noteField) {
                                noteField.text = ws.editingDraft
                                noteField.forceActiveFocus()
                                noteField.selectAll()
                            }
                        }
                        onDoubleClicked: ws.beginNoteEdit(ann.id)
                    }
                    Image {
                        visible: ann.kind === "image" && !!ann.path
                        source: ann.path ? ("file:///" + String(ann.path).replace(/\\/g, "/")) : ""
                        width: (ann.width || 40) * sheetCanvas.fitScale
                        height: (ann.height || 30) * sheetCanvas.fitScale
                        fillMode: Image.PreserveAspectFit
                    }
                    Canvas {
                        visible: ann.kind === "centerMark"
                        width: 24; height: 24
                        anchors.centerIn: parent
                        onPaint: {
                            const ctx = getContext("2d")
                            ctx.reset()
                            ctx.strokeStyle = "#333"
                            ctx.beginPath()
                            ctx.moveTo(12, 2); ctx.lineTo(12, 22)
                            ctx.moveTo(2, 12); ctx.lineTo(22, 12)
                            ctx.stroke()
                        }
                    }
                }
            }

            Repeater {
                model: ws.activeSheet ? ws.activeSheet.tableCount : 0
                delegate: Rectangle {
                    required property int index
                    readonly property var tbl: ws.activeSheet ? ws.activeSheet.table(index) : ({})
                    x: (tbl.x || 0) * sheetCanvas.fitScale
                    y: (tbl.y || 0) * sheetCanvas.fitScale
                    z: 8
                    width: {
                        const cols = tbl.colWidths || [20, 20, 40]
                        let w = 0
                        for (let i = 0; i < cols.length; ++i) w += cols[i]
                        return Math.max(40, w) * sheetCanvas.fitScale
                    }
                    height: ((tbl.rows ? tbl.rows.length : 1) * 5 + 6) * sheetCanvas.fitScale
                    color: "#fafafa"
                    border.color: "#666"
                    Column {
                        anchors.fill: parent
                        anchors.margins: 2
                        Text {
                            text: tbl.title || "Table"
                            font.pixelSize: Math.max(8, 2.6 * sheetCanvas.fitScale)
                            font.bold: true
                            color: "#222"
                        }
                        Repeater {
                            model: tbl.rows || []
                            delegate: Text {
                                required property var modelData
                                text: Array.isArray(modelData) ? modelData.join("  ") : String(modelData)
                                font.pixelSize: Math.max(7, 2.2 * sheetCanvas.fitScale)
                                color: "#333"
                            }
                        }
                    }
                }
            }

        Rectangle {
            id: ghost
                visible: ws.placing && (ws.activeTool === "baseView" || ws.activeTool === "text"
                         || ws.activeTool === "partsList" || ws.activeTool === "revision"
                         || ws.activeTool === "auxiliaryView")
                x: ws.ghostX - width / 2
                y: ws.ghostY - height / 2
                width: 90
                height: 70
                color: Qt.rgba(0, 102 / 255, 204 / 255, 0.15)
            border.color: "#0066CC"
            border.width: 2
                z: 40
        }

        // Every dimension on the sheet, plus (with the dimension tool) the edge under the cursor, the picks and
        // the dimension that follows the cursor. Geometry comes from DrawingDimension, like PDF and DXF export.
        Canvas {
            id: dimLayer
            anchors.fill: parent
            z: 25
            enabled: false
            // Dimensions on a view follow it, so a view's position and scale are watched too
            readonly property var viewPlaces: ws.activeSheet
                ? ws.activeSheet.views.map(v => v.position.x + "," + v.position.y + "," + v.scale + "," + v.width).join(";") : ""
            readonly property var watch: [ws.annRev, sheetCanvas.fitScale, ws.dimHover, ws.dimPicks, ws.dimPreview,
                                          ws.selectedNoteId, ws.activeSheet, ws.activeTool, viewPlaces]
            onWatchChanged: requestPaint()
            onWidthChanged: requestPaint()
            onHeightChanged: requestPaint()
            onPaint: {
                const ctx = getContext("2d")
                ctx.reset()
                if (!ws.activeSheet) return
                const k = sheetCanvas.fitScale
                const mgr = fissionApp.drawingManager
                function edge(e, color, w) {
                    ctx.strokeStyle = color
                    ctx.fillStyle = color
                    ctx.lineWidth = w
                    ctx.beginPath()
                    if (e.kind === "point") {
                        ctx.arc(e.x * k, e.y * k, 3.5, 0, 2 * Math.PI)
                        ctx.fill()
                        return
                    }
                    if (e.kind === "circle") ctx.arc(e.cx * k, e.cy * k, e.r * k, 0, 2 * Math.PI)
                    else { ctx.moveTo(e.x1 * k, e.y1 * k); ctx.lineTo(e.x2 * k, e.y2 * k) }
                    ctx.stroke()
                }
                function dim(a, color) {
                    const g = mgr.dimensionGeometry(ws.activeSheet, a)
                    if (g.detached) color = "#c83a32"   // its edge is gone from the view: shown red, not moved
                    ctx.strokeStyle = color
                    ctx.fillStyle = color
                    ctx.lineWidth = 1
                    ctx.beginPath()
                    for (const s of g.lines) { ctx.moveTo(s[0] * k, s[1] * k); ctx.lineTo(s[2] * k, s[3] * k) }
                    ctx.stroke()
                    for (const s of g.arcs) {
                        const a0 = s[3] * Math.PI / 180, sw = s[4] * Math.PI / 180
                        ctx.beginPath()
                        ctx.arc(s[0] * k, s[1] * k, s[2] * k, a0, a0 + sw, sw < 0)
                        ctx.stroke()
                    }
                    const len = (g.arrowSize || 3) * k
                    for (const s of g.arrows) {
                        const tx = s[0] * k, ty = s[1] * k, dx = s[2], dy = s[3]
                        const bx = tx - dx * len, by = ty - dy * len, sx = -dy * len * 0.3, sy = dx * len * 0.3
                        ctx.beginPath()
                        ctx.moveTo(tx, ty); ctx.lineTo(bx + sx, by + sy); ctx.lineTo(bx - sx, by - sy)
                        ctx.closePath()
                        ctx.fill()
                    }
                    const t = g.text
                    ctx.save()
                    ctx.translate(t.x * k, t.y * k)
                    ctx.rotate(t.angle * Math.PI / 180)
                    ctx.font = Math.max(6, t.size * k).toFixed(1) + "px sans-serif"
                    ctx.textAlign = "center"
                    ctx.textBaseline = "middle"
                    ctx.fillText(t.str, 0, 0)
                    ctx.restore()
                }
                for (let i = 0; i < ws.activeSheet.annotationCount; ++i) {
                    const a = ws.activeSheet.annotation(i)
                    if (a.kind === "dimension") dim(a, a.id === ws.selectedNoteId ? "#1a6fd1" : "#1e1e1e")
                }
                if (ws.activeTool !== "dimension") return
                for (const p of ws.dimPicks) edge(p, "#e07a1f", 2.5)
                if (ws.dimHover && ws.dimHover.kind) edge(ws.dimHover, "#1a6fd1", 2)
                if (ws.dimPreview) dim(ws.dimPreview, "#1a6fd1")
            }
        }

        // Above the views: a press on a dimension selects it and dragging moves its line / text (one undo
        // step). Anywhere else the press is passed on to the view or the paper underneath.
        MouseArea {
            id: dimMouse
            anchors.fill: parent
            z: 26
            enabled: ws.activeTool === "select"
            property string dragId: ""
            property bool moved: false
            onPressed: (m) => {
                const p = ws.paperToMm(m.x, m.y)
                const id = ws.activeSheet ? fissionApp.drawingManager.dimensionAt(ws.activeSheet, p.x, p.y, ws.dimTolerance()) : ""
                if (!id) { m.accepted = false; return }
                if (ws.editingNoteId) ws.commitNoteEdit()
                ws.selectedView = null
                ws.selectedNoteId = id
                ws.forceActiveFocus()
                dragId = id
                moved = false
            }
            onPositionChanged: (m) => {
                if (!dragId) return
                const p = ws.paperToMm(m.x, m.y)
                if (!moved) { if (ws.drawingDoc) ws.drawingDoc.beginEdit(); moved = true }
                fissionApp.drawingManager.moveDimension(ws.activeSheet, dragId, p.x, p.y)
            }
            onReleased: {
                if (moved) {
                    if (ws.drawingDoc) ws.drawingDoc.endEdit()
                    if (ws.app && ws.app.doc) ws.app.doc.markModified()
                }
                dragId = ""
                moved = false
            }
        }

        Rectangle {
            id: sectionLine
            visible: ws.sectionDragging
                x: Math.min(ws.sectionStartX, x2)
                y: Math.min(ws.sectionStartY, y2)
                width: Math.abs(x2 - ws.sectionStartX) || 2
                height: Math.abs(y2 - ws.sectionStartY) || 2
                color: Qt.rgba(204 / 255, 0, 0, 0.3)
            border.color: "#CC0000"
            border.width: 2
            property real x2: 0
            property real y2: 0
                z: 40
        }

            // Under the views. Left clicks on a view hit the view; empty paper hits this.
        MouseArea {
                id: sheetMouse
            anchors.fill: parent
                z: 1
                hoverEnabled: true
                acceptedButtons: Qt.LeftButton
                cursorShape: (ws.placing || ws.activeTool === "dimension" || ws.activeTool === "sectionView"
                              || ws.activeTool === "detailView")
                         ? Qt.CrossCursor : Qt.ArrowCursor
            onPressed: (m) => {
                    ws.forceActiveFocus()
                if (ws.activeTool === "dimension") {
                    ws.dimensionClick(ws.paperToMm(m.x, m.y))
                    return
                }
                if (ws.activeTool === "sectionView") {
                        ws.sectionStartX = m.x; ws.sectionStartY = m.y
                    ws.sectionDragging = true
                        sectionLine.x2 = m.x; sectionLine.y2 = m.y
                    return
                }
                if (ws.activeTool === "detailView") {
                        ws.detailStartX = m.x; ws.detailStartY = m.y
                    ws.detailDragging = true
                    return
                }
                    const p = ws.paperToMm(m.x, m.y)
                if (ws.activeTool === "baseView") {
                        ws.placeBaseAt(p.x, p.y)
                    } else if (ws.activeTool === "auxiliaryView" && ws.selectedView) {
                        ws.edit(() => {
                            const view = ws.activeSheet.addView("auxiliary", {
                        position: Qt.point(p.x, p.y),
                                scale: ws.selectedView.scale,
                                orientation: "isometric",
                                parentViewId: ws.selectedView.id,
                        showHiddenLines: ws.showHidden
                    })
                            ws.projectOnto(view)
                            ws.selectedView = view
                        })
                        ws.finishPlace("Auxiliary view placed")
                } else if (ws.activeTool === "text") {
                        let id = ""
                        ws.edit(() => {
                            id = ws.activeSheet.addAnnotation({ kind: "text", x: p.x, y: p.y, text: "Note", fontSize: 3.5 })
                        })
                        ws.beginNoteEdit(id)
                        ws.finishPlace("Note placed — type to edit, Enter commits, Esc cancels")
                } else if (ws.activeTool === "holeNote") {
                        ws.edit(() => ws.activeSheet.addAnnotation({ kind: "holeNote", x: p.x, y: p.y, text: "Hole", radius: 5 }))
                        ws.finishPlace("Hole note placed")
                } else if (ws.activeTool === "partsList") {
                        ws.edit(() => fissionApp.drawingManager.addPartsList(ws.activeSheet, ws.designDoc(), p))
                        ws.finishPlace("Parts list placed")
                } else if (ws.activeTool === "revision") {
                        ws.edit(() => fissionApp.drawingManager.addRevisionTable(ws.activeSheet, p))
                        ws.finishPlace("Revision table placed")
                } else if (ws.activeTool === "centerMark") {
                        ws.edit(() => ws.activeSheet.addAnnotation({ kind: "centerMark", x: p.x, y: p.y }))
                        ws.finishPlace("Center mark placed")
                } else {
                        if (ws.suppressSheetPress) return
                    ws.selectedView = null
                        if (ws.editingNoteId) ws.cancelNoteEdit()
                        // A click on a dimension selects it (Delete removes it; the inspector edits it)
                        ws.selectedNoteId = fissionApp.drawingManager.dimensionAt(ws.activeSheet, p.x, p.y, ws.dimTolerance())
                }
            }
            onPositionChanged: (m) => {
                    if (ws.placing) { ws.ghostX = m.x; ws.ghostY = m.y }
                    if (ws.activeTool === "dimension") ws.dimensionMove(ws.paperToMm(m.x, m.y))
                    if (ws.sectionDragging) { sectionLine.x2 = m.x; sectionLine.y2 = m.y }
                }
            onReleased: (m) => {
                if (ws.sectionDragging && ws.selectedView) {
                        const a = ws.paperToMm(ws.sectionStartX, ws.sectionStartY)
                        const b = ws.paperToMm(m.x, m.y)
                        let dir = "right"
                        if (Math.abs(b.x - a.x) >= Math.abs(b.y - a.y))
                            dir = b.x >= a.x ? "right" : "left"
                        else
                            dir = b.y >= a.y ? "down" : "up"
                        ws.edit(() => {
                            const view = ws.activeSheet.addView("section", {
                        position: Qt.point((a.x + b.x) / 2, (a.y + b.y) / 2),
                        scale: ws.selectedView.scale,
                        orientation: ws.selectedView.orientation,
                                parentViewId: ws.selectedView.id,
                                projectDirection: dir
                            })
                            ws.projectOnto(view)
                            ws.selectedView = view
                        })
                        ws.finishPlace("Section view placed")
                    } else if (ws.detailDragging && ws.selectedView) {
                        const a = ws.paperToMm(ws.detailStartX, ws.detailStartY)
                        const b = ws.paperToMm(m.x, m.y)
                        ws.edit(() => {
                            const view = ws.activeSheet.addView("detail", {
                        position: Qt.point((a.x + b.x) / 2, (a.y + b.y) / 2),
                        scale: ws.selectedView.scale * 2,
                        orientation: ws.selectedView.orientation,
                        parentViewId: ws.selectedView.id,
                                detailRadius: Math.max(5, Math.hypot(b.x - a.x, b.y - a.y)),
                                detailCenter: Qt.point(0, 0)
                            })
                            ws.projectOnto(view)
                            ws.selectedView = view
                        })
                        ws.finishPlace("Detail view placed")
                    }
                }
            }
        }
        }

        // Middle-drag and wheel sit above the sheet. Left clicks fall through to the views
        // and to the paper, so this area does not rebuild the sheet and does not eat picks.
        MouseArea {
            id: panZoom
            anchors.fill: parent
            z: 30
            acceptedButtons: Qt.MiddleButton | Qt.RightButton
            property real lastX: 0
            property real lastY: 0
            property bool panning: false
            onPressed: (m) => {
                panning = true
                lastX = m.x
                lastY = m.y
            }
            onPositionChanged: (m) => {
                if (!panning) return
                ws.sheetPanX += m.x - lastX
                ws.sheetPanY += m.y - lastY
                lastX = m.x
                lastY = m.y
            }
            onReleased: panning = false
            onWheel: (w) => ws.zoomAt(w.angleDelta.y > 0 ? 1.12 : 1 / 1.12, w.x, w.y)
        }

        ToolButton {
            text: "Fit"
            z: 40
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 8
            onClicked: ws.fitSheet()
        }

    }

    // Outside the clipped sheet, so the panel and its dropdown stay on screen.
        Rectangle {
            id: inspector
        visible: ws.selectedView !== null || ws.selectedNoteId !== ""
        width: 236
        height: inspectorCol.implicitHeight + 20
        x: Math.max(8, parent.width - width - 12)
        y: 8
        color: "#2c3136"
        border.color: "#1b1f23"
            radius: 6
        z: 60

        // Eats presses on the panel so closing it cannot reach the sheet.
        MouseArea {
                anchors.fill: parent
            z: 0
            onPressed: (m) => { m.accepted = true }
        }

        ColumnLayout {
            id: inspectorCol
            z: 1
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 10
            spacing: 6

                RowLayout {
                Layout.fillWidth: true
                Label {
                    text: ws.selectedView ? ws.selectedView.type : ws.titleSelected ? "Title block"
                          : ws.selectedAnn && ws.selectedAnn.kind === "dimension" ? "Dimension" : "Note"
                    color: "#f2f4f6"
                    font.bold: true
                    font.pixelSize: 13
                    font.capitalization: Font.Capitalize
                }
                    Item { Layout.fillWidth: true }
                MouseArea {
                    implicitWidth: 22
                    implicitHeight: 22
                    Layout.preferredWidth: 22
                    Layout.preferredHeight: 22
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onPressed: (m) => { m.accepted = true; ws.dismissInspector() }
                    ToolTip.visible: containsMouse
                    ToolTip.text: "Close"
                    Text {
                        anchors.centerIn: parent
                        text: "×"
                        color: parent.containsMouse ? "#ffffff" : "#d0d4d8"
                        font.pixelSize: 16
                    }
                }
            }

            // Title block: its fields, and Delete (DRAWING ▸ Title Block brings it back)
            Repeater {
                model: ws.titleSelected ? [["Title", "partName"], ["Drawn by", "drawnBy"], ["Revision", "revision"],
                                           ["Material", "material"], ["Date", "date"]] : []
                delegate: ColumnLayout {
                    required property var modelData
                    Layout.fillWidth: true
                    spacing: 2
                    Label { text: modelData[0]; color: "#b7bec6"; font.pixelSize: 11 }
                    TextField {
                        Layout.fillWidth: true
                        selectByMouse: true
                        text: ws.activeSheet ? (ws.activeSheet[modelData[1]] || "") : ""
                        onEditingFinished: {
                            const key = modelData[1], value = text
                            if (!ws.activeSheet || ws.activeSheet[key] === value) return
                            ws.edit(() => { ws.activeSheet[key] = value })
                        }
                    }
                }
            }
            Button {
                text: "Delete title block"
                Layout.fillWidth: true
                implicitHeight: 30
                visible: ws.titleSelected
                onClicked: ws.toggleTitleBlock()
            }

            Label {
                text: "Text"
                color: "#b7bec6"
                font.pixelSize: 11
                visible: !ws.selectedView && ws.selectedNoteId !== "" && !ws.titleSelected
            }
            TextField {
                id: noteField
                readonly property bool isDim: !!ws.selectedAnn && ws.selectedAnn.kind === "dimension"
                Layout.fillWidth: true
                visible: !ws.selectedView && ws.selectedNoteId !== "" && !ws.titleSelected
                selectByMouse: true
                // A dimension shows its measured value until text is typed over it; clear the field to measure again
                placeholderText: isDim ? fissionApp.drawingManager.dimensionLabel(Object.assign({}, ws.selectedAnn, { text: "" }))
                                       : "Note"
                // Enter or leaving the field saves (it used to save on Enter only, so a click away lost the edit)
                onEditingFinished: {
                    const id = ws.selectedNoteId
                    if (!id || !ws.activeSheet || text === ws.noteTextOf(id)) return
                    ws.editingNoteId = ""
                    ws.editingDraft = text
                    ws.edit(() => ws.activeSheet.updateAnnotation(id, { text: text }))
                }
                Keys.onEscapePressed: (e) => { ws.cancelNoteEdit(); e.accepted = true }
            }

            // Text height (ISO 3098 sizes, mm on the paper), weight and slant; decimals for a dimension
            Label { text: "Size · style"; color: "#b7bec6"; font.pixelSize: 11; visible: !ws.selectedView && !!ws.selectedAnn }
            RowLayout {
                Layout.fillWidth: true
                visible: !ws.selectedView && !!ws.selectedAnn
                spacing: 4
                ComboBox {
                    id: sizeBox
                    readonly property var sizes: [1.8, 2.5, 3.5, 5, 7, 10, 14, 20]
                    Layout.fillWidth: true
                    implicitHeight: 30
                    model: sizes.map(s => s + " mm")
                    currentIndex: {
                        const f = ws.selectedAnn ? Number(ws.selectedAnn.fontSize || 3.5) : 3.5
                        let best = 0
                        for (let i = 0; i < sizes.length; ++i)
                            if (Math.abs(sizes[i] - f) < Math.abs(sizes[best] - f)) best = i
                        return best
                    }
                    onActivated: (i) => ws.setAnn({ fontSize: sizes[i] })
                    Component.onCompleted: ws.pinComboPopup(sizeBox)
                }
                Button {
                    text: "B"
                    font.bold: true
                    implicitWidth: 30
                    implicitHeight: 30
                    checkable: true
                    visible: !noteField.isDim
                    checked: !!ws.selectedAnn && !!ws.selectedAnn.bold
                    onClicked: ws.setAnn({ bold: checked })
                    ToolTip.visible: hovered
                    ToolTip.text: "Bold"
                }
                Button {
                    text: "I"
                    font.italic: true
                    implicitWidth: 30
                    implicitHeight: 30
                    checkable: true
                    visible: !noteField.isDim
                    checked: !!ws.selectedAnn && !!ws.selectedAnn.italic
                    onClicked: ws.setAnn({ italic: checked })
                    ToolTip.visible: hovered
                    ToolTip.text: "Italic"
                }
                ComboBox {
                    id: decimalsBox
                    visible: noteField.isDim
                    implicitWidth: 64
                    implicitHeight: 30
                    model: ["0", "0.0", "0.00", "0.000"]
                    currentIndex: ws.selectedAnn && ws.selectedAnn.decimals !== undefined ? Number(ws.selectedAnn.decimals) : 2
                    onActivated: (i) => ws.setAnn({ decimals: i })
                    ToolTip.visible: hovered
                    ToolTip.text: "Decimal places"
                    Component.onCompleted: ws.pinComboPopup(decimalsBox)
                }
            }
            Button {
                text: "Delete"
                Layout.fillWidth: true
                implicitHeight: 30
                visible: !ws.selectedView && !!ws.selectedAnn
                onClicked: ws.deleteSelection()
            }

            Label { text: "Orientation"; color: "#b7bec6"; font.pixelSize: 11; visible: !!ws.selectedView }
                ComboBox {
                id: orientBox
                Layout.fillWidth: true
                implicitHeight: 30
                visible: !!ws.selectedView
                model: ws.orientationLabels
                currentIndex: ws.orientationIndex(ws.selectedView ? ws.selectedView.orientation : "front")
                // A click on the name only focuses the box; then Up / Down or the wheel step through the
                // orientations and the view follows each step. The arrows on the right open the list.
                wheelEnabled: true
                MouseArea {
                    anchors { left: parent.left; top: parent.top; bottom: parent.bottom }
                    width: parent.width - (parent.indicator ? parent.indicator.width + 6 : 32)
                    cursorShape: Qt.IBeamCursor
                    onPressed: (m) => { orientBox.forceActiveFocus(); m.accepted = true }
                }
                Rectangle {   // focus ring: shows the keys / wheel act on this box
                    anchors.fill: parent
                    color: "transparent"
                    radius: 3
                    border.color: Theme.accent
                    border.width: 2
                    visible: orientBox.activeFocus && !orientBox.popup.visible
                }
                onActivated: (i) => {
                    const view = ws.selectedView
                    const id = ws.orientationIds[i]
                    const same = view && (view.orientation === id
                        || ((view.orientation === "isometric" || view.orientation === "home") && id === "isoBackTopRight"))
                    if (!view || !id || same) return
                    ws.edit(() => { view.orientation = id })
                    ws.projectOnto(view)
                }
                Component.onCompleted: ws.pinComboPopup(orientBox)
            }

            Label { text: "Display"; color: "#b7bec6"; font.pixelSize: 11; visible: !!ws.selectedView }
            ComboBox {
                id: styleBox
                Layout.fillWidth: true
                implicitHeight: 30
                visible: !!ws.selectedView
                model: ["Wireframe", "Rendered"]
                currentIndex: ws.selectedView && ws.selectedView.displayStyle === "rendered" ? 1 : 0
                onActivated: (i) => {
                    const view = ws.selectedView
                    const style = i === 1 ? "rendered" : "wireframe"
                    if (!view || view.displayStyle === style) return
                    ws.edit(() => { view.displayStyle = style })
                    ws.projectOnto(view)
                }
                Component.onCompleted: ws.pinComboPopup(styleBox)
            }

            Label { text: "Scale"; color: "#b7bec6"; font.pixelSize: 11; visible: !!ws.selectedView }
                RowLayout {
                Layout.fillWidth: true
                visible: !!ws.selectedView
                spacing: 6
                Button {
                    text: "−"
                    implicitWidth: 32
                    implicitHeight: 30
                    onClicked: ws.stepScale(-1)
                }
                Label {
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    text: ws.selectedView ? ws.drawingScaleLabel(ws.selectedView.scale) : "1:1"
                    color: "#f2f4f6"
                    font.pixelSize: 13
                }
                Button {
                    text: "+"
                    implicitWidth: 32
                    implicitHeight: 30
                    onClicked: ws.stepScale(1)
                }
                }

                CheckBox {
                id: hiddenBox
                Layout.fillWidth: true
                visible: !!ws.selectedView
                enabled: !ws.selectedView || ws.selectedView.displayStyle !== "rendered"
                    text: "Hidden lines"
                spacing: 8
                    checked: ws.selectedView ? ws.selectedView.showHiddenLines : false
                indicator: Rectangle {
                    implicitWidth: 16
                    implicitHeight: 16
                    x: hiddenBox.leftPadding
                    y: hiddenBox.height / 2 - height / 2
                    radius: 2
                    border.width: 1
                    border.color: "#d0d4d8"
                    color: hiddenBox.checked ? Theme.accent : "transparent"
                    Text {
                        anchors.centerIn: parent
                        text: "✓"
                        color: "white"
                        font.pixelSize: 11
                        visible: hiddenBox.checked
                    }
                }
                contentItem: Text {
                    text: hiddenBox.text
                    color: "#f2f4f6"
                    font.pixelSize: 12
                    leftPadding: hiddenBox.indicator.width + hiddenBox.spacing
                    verticalAlignment: Text.AlignVCenter
                }
                onToggled: {
                    const view = ws.selectedView
                    if (!view || view.showHiddenLines === checked) return
                    ws.edit(() => { view.showHiddenLines = checked })
                    ws.projectOnto(view)
                }
            }

            RowLayout {
                Layout.fillWidth: true
                visible: !!ws.selectedView
                spacing: 6
                Button {
                    text: "Regenerate"
                    Layout.fillWidth: true
                    implicitHeight: 30
                    onClicked: {
                        const view = ws.selectedView
                        if (!view) return
                        ws.edit(() => { ws.projectOnto(view) })
                    }
                }
                Button {
                    text: "Delete"
                    implicitHeight: 30
                    onClicked: {
                        const view = ws.selectedView
                        if (!view || !ws.activeSheet) return
                        ws.edit(() => { ws.activeSheet.removeView(view.id) })
                        ws.selectedView = null
                    }
                }
            }
        }
    }

    onSelectedNoteIdChanged: if (noteField) noteField.text = noteTextOf(selectedNoteId)
    // An edit from the sheet, undo or another control refreshes the field, unless it is being typed in
    onSelectedAnnChanged: if (noteField && !noteField.activeFocus) noteField.text = selectedAnn ? (selectedAnn.text || "") : ""
    onSelectedViewChanged: if (selectedView) { if (editingNoteId) commitNoteEdit(); selectedNoteId = "" }

    Keys.onPressed: (e) => {
        if (e.key === Qt.Key_Escape) {
            if (editingNoteId) cancelNoteEdit()
            else if (activeTool === "dimension" && dimPicks.length) { dimPicks = []; dimPreview = null; fissionApp.showMessage(dimHint()) }
            else cancelPlace()
            e.accepted = true
        }
        else if (e.key === Qt.Key_F && !(e.modifiers & Qt.ControlModifier)) { fitSheet(); e.accepted = true }
    }
    Component.onCompleted: forceActiveFocus()

    FileDialog {
        id: exportDialog
        property string kind: "pdf"
        fileMode: FileDialog.SaveFile
        nameFilters: kind === "pdf" ? ["PDF (*.pdf)"]
                     : kind === "dxf" ? ["DXF (*.dxf)"]
                     : kind === "tiff" ? ["TIFF (*.tif *.tiff)"]
                     : ["PNG (*.png)"]
        defaultSuffix: kind === "tiff" ? "tif" : kind
        onAccepted: {
            const path = fissionApp.toLocalPath(selectedFile.toString())
            let ok = false
            if (kind === "pdf") ok = fissionApp.drawingManager.exportPdf(activeSheet, path)
            else if (kind === "dxf") ok = fissionApp.drawingManager.exportDxf(activeSheet, path)
            else if (kind === "tiff") ok = fissionApp.drawingManager.exportTiff(activeSheet, path)
            else ok = fissionApp.drawingManager.exportPng(activeSheet, path)
            fissionApp.showMessage(ok ? ("Exported " + kind.toUpperCase()) : (fissionApp.drawingManager.lastError || "Export failed"))
        }
    }
    FileDialog {
        id: imageDialog
        fileMode: FileDialog.OpenFile
        nameFilters: ["Images (*.png *.jpg *.jpeg *.bmp *.tif *.tiff)"]
        onAccepted: {
            const path = fissionApp.toLocalPath(selectedFile.toString())
            const id = fissionApp.drawingManager.addImage(activeSheet, path, Qt.point(30, 30), 40)
            fissionApp.showMessage(id ? "Image placed" : (fissionApp.drawingManager.lastError || "Could not place image"))
        }
    }
}