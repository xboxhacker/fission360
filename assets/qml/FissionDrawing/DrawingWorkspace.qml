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
    property real dimStartX: 0
    property real dimStartY: 0
    property bool dimDragging: false
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
        try { fn() } finally { if (drawingDoc) drawingDoc.endEdit() }
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
        dimDragging = false
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

    function beginDimensionMode() {
        activeTool = "dimension"
        fissionApp.showMessage("Dimension — click two points to measure")
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
                x: parent.width - 180 * sheetCanvas.fitScale
                y: parent.height - 60 * sheetCanvas.fitScale
                width: 180 * sheetCanvas.fitScale
                height: 60 * sheetCanvas.fitScale
                color: "white"
                border.color: "#333"
                border.width: 1

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
                    readonly property var ann: ws.activeSheet ? ws.activeSheet.annotation(index) : ({})
                    x: (ann.x || 0) * sheetCanvas.fitScale
                    y: (ann.y || 0) * sheetCanvas.fitScale
                    z: 8
                    width: ann.kind === "text" ? Math.max(12, noteLabel.implicitWidth, noteInput.visible ? Math.max(80, noteInput.implicitWidth) : 0) : 1
                    height: ann.kind === "text" ? Math.max(14, noteLabel.implicitHeight) : 1
                    Canvas {
                        width: 240; height: 80
                        anchors.centerIn: parent
                        visible: ann.kind === "dimension" || ann.kind === "leader"
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
                        visible: (ann.kind === "holeNote" || ann.kind === "dimension" || ann.kind === "text")
                                 && ws.editingNoteId !== ann.id
                        text: ann.text || (ann.kind === "holeNote" ? "Hole" : "")
                        font.pixelSize: Math.max(9, (ann.fontSize || 3.5) * sheetCanvas.fitScale)
                        color: ws.selectedNoteId === ann.id && ann.kind === "text" ? "#1a4f9c" : "#222"
                    }
                    TextInput {
                        id: noteInput
                        z: 2
                        visible: ann.kind === "text" && ws.editingNoteId === ann.id
                        font.pixelSize: noteLabel.font.pixelSize
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
                        enabled: ann.kind === "text" && ws.editingNoteId !== ann.id
                        acceptedButtons: Qt.LeftButton
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

            Rectangle {
                id: dimPreview
                visible: ws.dimDragging
                x: Math.min(ws.dimStartX, x2)
                y: Math.min(ws.dimStartY, y2)
                width: Math.abs(x2 - ws.dimStartX)
                height: Math.abs(y2 - ws.dimStartY)
                color: Qt.rgba(204 / 255, 0, 0, 0.1)
                border.color: "#CC0000"
                border.width: 1
                property real x2: 0
                property real y2: 0
                z: 40
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
                        ws.dimStartX = m.x; ws.dimStartY = m.y
                        ws.dimDragging = true
                        dimPreview.x2 = m.x; dimPreview.y2 = m.y
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
                        ws.selectedNoteId = ""
                    }
                }
                onPositionChanged: (m) => {
                    if (ws.placing) { ws.ghostX = m.x; ws.ghostY = m.y }
                    if (ws.dimDragging) { dimPreview.x2 = m.x; dimPreview.y2 = m.y }
                    if (ws.sectionDragging) { sectionLine.x2 = m.x; sectionLine.y2 = m.y }
                }
                onReleased: (m) => {
                    if (ws.dimDragging) {
                        const a = ws.paperToMm(ws.dimStartX, ws.dimStartY)
                        const b = ws.paperToMm(m.x, m.y)
                        const len = Math.hypot(b.x - a.x, b.y - a.y)
                        ws.edit(() => ws.activeSheet.addAnnotation({
                            kind: "dimension", x: a.x, y: a.y, x2: b.x, y2: b.y,
                            text: len.toFixed(2) + " mm"
                        }))
                        ws.finishPlace("Dimension placed")
                    } else if (ws.sectionDragging && ws.selectedView) {
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
                    text: ws.selectedView ? ws.selectedView.type : "Note"
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

            Label {
                text: "Text"
                color: "#b7bec6"
                font.pixelSize: 11
                visible: !ws.selectedView && ws.selectedNoteId !== ""
            }
            TextField {
                id: noteField
                Layout.fillWidth: true
                visible: !ws.selectedView && ws.selectedNoteId !== ""
                selectByMouse: true
                placeholderText: "Note"
                onAccepted: {
                    const id = ws.selectedNoteId
                    if (!id || text === ws.noteTextOf(id)) return
                    ws.editingNoteId = ""
                    ws.editingDraft = text
                    ws.edit(() => ws.activeSheet.updateAnnotation(id, { text: text }))
                }
                Keys.onEscapePressed: (e) => { ws.cancelNoteEdit(); e.accepted = true }
            }

            Label { text: "Orientation"; color: "#b7bec6"; font.pixelSize: 11; visible: !!ws.selectedView }
            ComboBox {
                id: orientBox
                Layout.fillWidth: true
                implicitHeight: 30
                visible: !!ws.selectedView
                model: ws.orientationLabels
                currentIndex: ws.orientationIndex(ws.selectedView ? ws.selectedView.orientation : "front")
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
    onSelectedViewChanged: if (selectedView) { if (editingNoteId) commitNoteEdit(); selectedNoteId = "" }

    Keys.onPressed: (e) => {
        if (e.key === Qt.Key_Escape) {
            if (editingNoteId) cancelNoteEdit()
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