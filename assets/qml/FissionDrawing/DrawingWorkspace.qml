// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import "../FissionApplication"

// Drawing workspace — Fusion-like sheet: place base with orientation, Shift+drag to project,
// pan/zoom the sheet, floating view inspector, clean floating views (no cards).
Item {
    id: drawingWS
    anchors.fill: parent

    property var app
    property var drawingDoc: null
    property var activeSheet: null
    property string activeTool: "select"   // select | baseView | projectedView | sectionView | detailView | text | dimension | centerMark
    property string placeOrientation: "front"
    property var selectedView: null
    property real dimStartX: 0
    property real dimStartY: 0
    property bool dimDragging: false
    property real ghostX: 0
    property real ghostY: 0
    property bool ghostVisible: false

    // Sheet camera (screen space over the paper host)
    property real sheetZoom: 1.0
    property real sheetPanX: 0
    property real sheetPanY: 0

    readonly property var orientations: [
        { id: "front", label: "Front" },
        { id: "top", label: "Top" },
        { id: "right", label: "Right" },
        { id: "left", label: "Left" },
        { id: "back", label: "Back" },
        { id: "bottom", label: "Bottom" },
        { id: "isometric", label: "Iso" }
    ]

    function syncSheet() {
        if (drawingDoc && drawingDoc.sheetCount > 0) {
            const idx = Math.max(0, Math.min(drawingDoc.activeSheetIndex, drawingDoc.sheetCount - 1))
            activeSheet = drawingDoc.sheet(idx)
        } else {
            activeSheet = null
        }
    }

    onDrawingDocChanged: {
        syncSheet()
        sheetZoom = 1; sheetPanX = 0; sheetPanY = 0
        selectedView = null
        cancelPlace()
        Qt.callLater(maybeAutoPlaceBase)
    }
    Connections {
        target: drawingDoc
        function onSheetsChanged() { syncSheet() }
        function onActiveSheetChanged() { syncSheet(); selectedView = null }
    }

    function designDoc() { return app ? app.doc : null }

    function cancelPlace() {
        activeTool = "select"
        ghostVisible = false
    }

    function beginBaseView(orient) {
        if (orient) placeOrientation = orient
        activeTool = "baseView"
        ghostVisible = true
        fissionApp.showMessage("Base View — pick orientation, click the sheet to place (Esc cancels)")
    }

    function beginProjectMode() {
        if (!selectedView) {
            fissionApp.showMessage("Select a parent view, then Shift+drag it — or click Projected View")
            return
        }
        activeTool = "projectedView"
        fissionApp.showMessage("Projected View — Shift+drag from the parent toward the projection direction")
    }

    function projectView(view) {
        if (!view || !fissionApp.drawingManager) return false
        const ok = fissionApp.drawingManager.projectView(view, designDoc(), drawingDoc)
        if (!ok)
            fissionApp.showMessage(fissionApp.drawingManager.lastError || "Projection failed")
        return ok
    }

    function paperToMm(px, py) {
        const s = paper.fitScale * sheetZoom
        return Qt.point(px / Math.max(0.01, s), py / Math.max(0.01, s))
    }

    function orientationForProject(parentOrient, dir) {
        // Third-angle orthographic graph (screen: +x right, +y down)
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

    function placeProjected(parent, dir) {
        if (!activeSheet || !parent) return
        const gap = 25
        const pw = Math.max(20, parent.width * parent.scale)
        const ph = Math.max(16, parent.height * parent.scale)
        let px = parent.position.x, py = parent.position.y
        if (dir === "right") px += pw + gap
        else if (dir === "left") px -= pw + gap
        else if (dir === "up") py -= ph + gap
        else if (dir === "down") py += ph + gap

        const childOrient = orientationForProject(parent.orientation || "front", dir)
        const view = activeSheet.addView("projected", {
            position: Qt.point(px, py),
            scale: parent.scale,
            orientation: childOrient,
            parentViewId: parent.id,
            projectDirection: dir,
            showHiddenLines: parent.showHiddenLines
        })
        projectView(view)
        selectedView = view
        cancelPlace()
        fissionApp.showMessage("Projected " + childOrient + " view")
    }

    function placeBaseAt(mmX, mmY) {
        if (!activeSheet) return
        const view = activeSheet.addView("base", {
            position: Qt.point(mmX, mmY),
            scale: suggestScale(),
            orientation: placeOrientation,
            showHiddenLines: false
        })
        projectView(view)
        selectedView = view
        cancelPlace()
        fissionApp.showMessage("Base view (" + placeOrientation + ")")
    }

    function placeSectionOrDetail(type, mmX, mmY) {
        if (!activeSheet || !selectedView) {
            fissionApp.showMessage("Select a parent view first")
            return
        }
        const opts = {
            position: Qt.point(mmX, mmY),
            scale: type === "detail" ? selectedView.scale * 2 : selectedView.scale,
            orientation: selectedView.orientation,
            parentViewId: selectedView.id,
            projectDirection: "right",
            showHiddenLines: false
        }
        if (type === "detail") {
            opts.detailRadius = 20
            opts.detailCenter = Qt.point(0, 0)
        }
        const view = activeSheet.addView(type, opts)
        projectView(view)
        selectedView = view
        cancelPlace()
        fissionApp.showMessage((type === "section" ? "Section" : "Detail") + " view placed")
    }

    function suggestScale() {
        if (!activeSheet || !designDoc()) return 1.0
        // Fit roughly a third of the sheet width
        return 1.0
    }

    function maybeAutoPlaceBase() {
        if (!activeSheet || activeSheet.viewCount > 0) return
        // Center a front base view so a new drawing isn't an empty sheet
        const cx = Math.max(20, activeSheet.width * 0.35 - 30)
        const cy = Math.max(20, activeSheet.height * 0.4 - 20)
        placeOrientation = "front"
        placeBaseAt(cx, cy)
        fissionApp.showMessage("Drawing opened — Front base view placed. Shift+drag to project.")
    }

    function regenerateAll() {
        if (!activeSheet) return
        const n = fissionApp.drawingManager.regenerateSheet(activeSheet, designDoc(), drawingDoc)
        if (drawingDoc && designDoc()) {
            drawingDoc.setSourceDesignRevision(designDoc().contentRevision)
            designDoc().linkAsset("drawing", drawingDoc.id, drawingDoc.name)
        }
        fissionApp.showMessage("Regenerated " + n + " view(s)")
    }
    function updateFromDesign() {
        const design = designDoc()
        if (!drawingDoc || !design) {
            fissionApp.showMessage("No design linked to this drawing")
            return
        }
        const n = fissionApp.drawingManager.updateDrawingFromDesign(drawingDoc, design)
        fissionApp.showMessage(n
            ? ("Drawing updated from design — " + n + " view(s), now current")
            : (fissionApp.drawingManager.lastError || "Nothing to update"))
    }
    readonly property bool drawingOutOfDate: {
        const d = drawingDoc, design = designDoc()
        if (!d || !design) return false
        d.outOfDate
        design.contentRevision
        return d.sourceDesignRevision !== design.contentRevision
    }

    function addTextAt(mx, my, text) {
        if (!activeSheet) return
        const p = paperToMm(mx, my)
        activeSheet.addAnnotation({ kind: "text", x: p.x, y: p.y, text: text || "Note", fontSize: 3.5 })
    }

    function addDimension(x1, y1, x2, y2) {
        if (!activeSheet) return
        const a = paperToMm(x1, y1), b = paperToMm(x2, y2)
        const len = Math.hypot(b.x - a.x, b.y - a.y)
        activeSheet.addAnnotation({
            kind: "dimension", x: a.x, y: a.y, x2: b.x, y2: b.y,
            text: len.toFixed(2) + " mm"
        })
    }

    function exportPdf() { exportDialog.kind = "pdf"; exportDialog.open() }
    function exportDxf() { exportDialog.kind = "dxf"; exportDialog.open() }
    function exportPng() { exportDialog.kind = "png"; exportDialog.open() }
    function exportTiff() { exportDialog.kind = "tiff"; exportDialog.open() }
    function addPartsList() {
        if (!activeSheet) return
        fissionApp.drawingManager.addPartsList(activeSheet, designDoc(), Qt.point(20, 40))
        fissionApp.showMessage("Parts list placed")
    }
    function addRevisionTable() {
        if (!activeSheet) return
        fissionApp.drawingManager.addRevisionTable(activeSheet, Qt.point(20, 40))
        fissionApp.showMessage("Revision table placed")
    }
    function addBendTable() {
        if (!activeSheet) return
        fissionApp.drawingManager.addBendTable(activeSheet, designDoc(), Qt.point(20, 40))
        fissionApp.showMessage("Bend table placed")
    }
    function insertImage() { imageDialog.open() }

    function zoomAt(factor, sx, sy) {
        const old = sheetZoom
        sheetZoom = Math.max(0.25, Math.min(6, sheetZoom * factor))
        // Keep the point under the cursor stable
        sheetPanX = sx - (sx - sheetPanX) * (sheetZoom / old)
        sheetPanY = sy - (sy - sheetPanY) * (sheetZoom / old)
    }

    function fitSheet() {
        sheetZoom = 1
        sheetPanX = 0
        sheetPanY = 0
    }

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

    Keys.onPressed: (e) => {
        if (e.key === Qt.Key_Escape) { cancelPlace(); e.accepted = true }
        else if (e.key === Qt.Key_F && !(e.modifiers & Qt.ControlModifier)) { fitSheet(); e.accepted = true }
    }
    Component.onCompleted: forceActiveFocus()

    RowLayout {
        anchors.fill: parent
        spacing: 0

        // -------- Browser --------
        Rectangle {
            Layout.preferredWidth: 200
            Layout.fillHeight: true
            color: Theme.panel
            border.color: Theme.border
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 8
                spacing: 6
                Text { text: "DRAWING"; font.pixelSize: 10; font.bold: true; color: Theme.textDim }
                Text {
                    text: drawingDoc ? drawingDoc.name : "—"
                    font.pixelSize: 13; font.bold: true; color: Theme.text
                    elide: Text.ElideRight; Layout.fillWidth: true
                }
                Text {
                    text: drawingDoc ? (drawingDoc.standard + " · " + drawingDoc.projectionAngle + "-angle") : ""
                    font.pixelSize: 10; color: Theme.textDim
                }
                Rectangle { Layout.fillWidth: true; height: 1; color: Theme.border }

                Text { text: "Sheets"; font.pixelSize: 10; font.bold: true; color: Theme.textDim }
                Repeater {
                    model: drawingDoc ? drawingDoc.sheetCount : 0
                    delegate: Rectangle {
                        required property int index
                        Layout.fillWidth: true
                        height: 22
                        radius: 3
                        color: index === drawingDoc.activeSheetIndex ? Theme.accentLight : "transparent"
                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.left: parent.left
                            anchors.leftMargin: 6
                            text: {
                                const s = drawingDoc.sheet(index)
                                return (s ? s.name : "") + (s ? "  (" + s.size + ")" : "")
                            }
                            color: Theme.text; font.pixelSize: 11
                        }
                        MouseArea { anchors.fill: parent; onClicked: drawingDoc.activeSheetIndex = index }
                    }
                }
                Button {
                    text: "+ Sheet"
                    flat: true
                    onClicked: {
                        if (!drawingDoc) return
                        drawingDoc.addSheet("Sheet" + (drawingDoc.sheetCount + 1), "A3", "landscape")
                        drawingDoc.activeSheetIndex = drawingDoc.sheetCount - 1
                    }
                }

                Rectangle { Layout.fillWidth: true; height: 1; color: Theme.border }
                Text { text: "Views"; font.pixelSize: 10; font.bold: true; color: Theme.textDim }
                Flickable {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    contentHeight: viewCol.height
                    clip: true
                    Column {
                        id: viewCol
                        width: parent.width
                        spacing: 2
                        Repeater {
                            model: activeSheet ? activeSheet.viewCount : 0
                            delegate: Rectangle {
                                required property int index
                                width: viewCol.width
                                height: 24
                                radius: 3
                                color: {
                                    const v = activeSheet.view(index)
                                    return (selectedView && v && selectedView.id === v.id) ? Theme.accentLight : "transparent"
                                }
                                Text {
                                    anchors.fill: parent
                                    anchors.leftMargin: 6
                                    verticalAlignment: Text.AlignVCenter
                                    text: {
                                        const v = activeSheet.view(index)
                                        return v ? (v.name + " · " + v.orientation) : ""
                                    }
                                    color: Theme.text; font.pixelSize: 11; elide: Text.ElideRight
                                }
                                MouseArea {
                                    anchors.fill: parent
                                    onClicked: {
                                        selectedView = activeSheet.view(index)
                                        if (selectedView && app.selection)
                                            app.selection.select({ kind: "drawingView", id: selectedView.id }, false)
                                    }
                                }
                            }
                        }
                    }
                }

                Button {
                    text: drawingOutOfDate ? "Update from Design ★" : "Update from Design"
                    flat: true
                    enabled: !!designDoc()
                    onClicked: updateFromDesign()
                }
                Button { text: "Regenerate"; flat: true; onClicked: regenerateAll() }
                Button { text: "Fit sheet (F)"; flat: true; onClicked: fitSheet() }
                Text {
                    visible: drawingOutOfDate
                    text: "Out of date with design"
                    color: Theme.error
                    font.pixelSize: 11
                    font.bold: true
                }
            }
        }

        // -------- Sheet viewport --------
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            Rectangle { anchors.fill: parent; color: "#d4d8dc" }

            // Orientation bar while placing base view
            Rectangle {
                id: orientBar
                visible: activeTool === "baseView"
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.top: parent.top
                anchors.topMargin: 10
                width: orientRow.width + 16
                height: 36
                radius: 6
                color: Theme.panel
                border.color: Theme.border
                z: 30
                Row {
                    id: orientRow
                    anchors.centerIn: parent
                    spacing: 4
                    Repeater {
                        model: drawingWS.orientations
                        delegate: Button {
                            text: modelData.label
                            flat: true
                            highlighted: placeOrientation === modelData.id
                            onClicked: placeOrientation = modelData.id
                            implicitHeight: 26
                            implicitWidth: 48
                        }
                    }
                }
            }

            // View inspector
            Rectangle {
                id: inspector
                visible: !!selectedView && activeTool === "select"
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: 10
                width: 200
                height: inspCol.implicitHeight + 16
                radius: 6
                color: Theme.panel
                border.color: Theme.border
                z: 30
                ColumnLayout {
                    id: inspCol
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 6
                    Text {
                        text: selectedView ? selectedView.name : ""
                        font.bold: true; font.pixelSize: 12; color: Theme.text
                        Layout.fillWidth: true; elide: Text.ElideRight
                    }
                    Text {
                        text: selectedView ? (selectedView.type + " · " + selectedView.orientation) : ""
                        font.pixelSize: 10; color: Theme.textDim
                    }
                    RowLayout {
                        Label { text: "Scale"; color: Theme.text; Layout.preferredWidth: 48 }
                        SpinBox {
                            from: 1; to: 1000
                            value: selectedView ? Math.round(selectedView.scale * 100) : 100
                            editable: true
                            textFromValue: (v) => (v / 100).toFixed(2)
                            valueFromText: (t) => Math.round(parseFloat(t) * 100)
                            onValueModified: if (selectedView) selectedView.scale = value / 100
                            Layout.fillWidth: true
                        }
                    }
                    CheckBox {
                        text: "Hidden lines"
                        checked: selectedView ? selectedView.showHiddenLines : false
                        onToggled: if (selectedView) selectedView.showHiddenLines = checked
                    }
                    Button {
                        text: "Shift+drag to project"
                        flat: true
                        Layout.fillWidth: true
                        onClicked: beginProjectMode()
                    }
                    Button {
                        text: "Regenerate view"
                        flat: true
                        Layout.fillWidth: true
                        onClicked: if (selectedView) projectView(selectedView)
                    }
                }
            }

            // HUD
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 12
                z: 30
                color: Theme.header
                font.pixelSize: 12
                font.bold: true
                style: Text.Outline
                styleColor: "#ffffff"
                text: {
                    if (activeTool === "baseView")
                        return "Click sheet to place " + placeOrientation + " base view · Esc cancel"
                    if (activeTool === "projectedView")
                        return "Shift+drag from parent view toward the projection · Esc cancel"
                    if (activeTool === "sectionView")
                        return "Click to place section view from selected parent"
                    if (activeTool === "detailView")
                        return "Click to place detail view from selected parent"
                    if (activeTool === "dimension")
                        return "Drag on the sheet to place a dimension"
                    if (activeTool === "text")
                        return "Click to place a note"
                    if (activeTool === "centerMark")
                        return "Click to place a center mark"
                    return "Wheel zoom · Mid-drag pan · Shift+drag view to project · F fit"
                }
            }

            // Transformed paper host (pan + zoom around view center)
            Item {
                id: paperHost
                width: parent.width
                height: parent.height
                x: sheetPanX
                y: sheetPanY
                scale: sheetZoom
                transformOrigin: Item.Center

                Rectangle {
                    id: paper
                    anchors.centerIn: parent
                    width: (activeSheet ? activeSheet.width : 420) * fitScale
                    height: (activeSheet ? activeSheet.height : 297) * fitScale
                    readonly property real fitScale: {
                        if (!activeSheet) return 1
                        const sx = (paperHost.width - 48) / Math.max(1, activeSheet.width)
                        const sy = (paperHost.height - 48) / Math.max(1, activeSheet.height)
                        return Math.min(sx, sy)
                    }
                    color: "white"
                    border.color: "#9aa0a6"
                    border.width: 1
                    layer.enabled: true
                    layer.smooth: true

                    // Soft sheet shadow
                    Rectangle {
                        anchors.fill: parent
                        anchors.margins: -1
                        z: -1
                        color: "transparent"
                        border.color: "#00000022"
                        border.width: 6
                        radius: 1
                    }

                    // Light border frame (drawing format)
                    Rectangle {
                        anchors.fill: parent
                        anchors.margins: 10 * paper.fitScale
                        color: "transparent"
                        border.color: "#333"
                        border.width: 1
                    }

                    Repeater {
                        model: activeSheet ? activeSheet.viewCount : 0
                        delegate: DrawingViewItem {
                            required property int index
                            view: activeSheet ? activeSheet.view(index) : null
                            selection: app ? app.selection : null
                            paperScale: paper.fitScale
                            x: view ? view.position.x * paper.fitScale : 0
                            y: view ? view.position.y * paper.fitScale : 0
                            selected: selectedView && view && selectedView.id === view.id
                            interactive: activeTool === "select" || activeTool === "projectedView"
                            onClicked: selectedView = view
                            onDragFinished: (nx, ny) => {
                                if (view) view.position = Qt.point(nx / paper.fitScale, ny / paper.fitScale)
                            }
                            onProjectDrag: (dir) => placeProjected(view, dir)
                        }
                    }

                    // Annotations
                    Repeater {
                        model: activeSheet ? activeSheet.annotationCount : 0
                        delegate: Item {
                            required property int index
                            readonly property var ann: activeSheet ? activeSheet.annotation(index) : ({})
                            x: (ann.x || 0) * paper.fitScale
                            y: (ann.y || 0) * paper.fitScale
                            Canvas {
                                width: 240; height: 48
                                anchors.centerIn: parent
                                visible: ann.kind === "dimension" || ann.kind === "leader"
                                onPaint: {
                                    const ctx = getContext("2d")
                                    ctx.reset()
                                    ctx.strokeStyle = "#222"
                                    ctx.lineWidth = 1
                                    const x2 = ((ann.x2 || ann.x) - ann.x) * paper.fitScale
                                    const y2 = ((ann.y2 || ann.y) - ann.y) * paper.fitScale
                                    ctx.beginPath(); ctx.moveTo(0, 0); ctx.lineTo(x2, y2); ctx.stroke()
                                    // arrows
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
                                visible: ann.kind === "text" || ann.kind === "holeNote" || ann.kind === "dimension"
                                text: ann.text || ""
                                font.pixelSize: Math.max(9, (ann.fontSize || 3.5) * paper.fitScale)
                                color: "#222"
                            }
                            Image {
                                visible: ann.kind === "image"
                                source: ann.path ? ("file:///" + ann.path.replace(/\\/g, "/")) : ""
                                width: (ann.width || 40) * paper.fitScale
                                height: (ann.height || 30) * paper.fitScale
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
                                    ctx.beginPath(); ctx.arc(12, 12, 5, 0, Math.PI * 2); ctx.stroke()
                                }
                            }
                        }
                    }

                    Repeater {
                        model: activeSheet ? activeSheet.tableCount : 0
                        delegate: Rectangle {
                            required property int index
                            readonly property var tbl: activeSheet ? activeSheet.table(index) : ({})
                            x: (tbl.x || 0) * paper.fitScale
                            y: (tbl.y || 0) * paper.fitScale
                            width: {
                                const cols = tbl.colWidths || [20, 20, 40]
                                let w = 0
                                for (let i = 0; i < cols.length; ++i) w += cols[i]
                                return w * paper.fitScale
                            }
                            height: ((tbl.rows ? tbl.rows.length : 1) * 5 + 4) * paper.fitScale
                            color: "#fafafa"
                            border.color: "#666"
                            Column {
                                anchors.fill: parent
                                anchors.margins: 1
                                Text {
                                    text: tbl.title || "Table"
                                    font.pixelSize: Math.max(8, 2.5 * paper.fitScale)
                                    font.bold: true
                                }
                                Repeater {
                                    model: tbl.rows || []
                                    delegate: Row {
                                        required property var modelData
                                        spacing: 0
                                        Repeater {
                                            model: modelData
                                            delegate: Rectangle {
                                                required property var modelData
                                                required property int index
                                                width: ((tbl.colWidths && tbl.colWidths[index]) || 20) * paper.fitScale
                                                height: 5 * paper.fitScale
                                                border.color: "#888"
                                                border.width: 0.5
                                                color: "white"
                                                Text {
                                                    anchors.fill: parent
                                                    anchors.margins: 1
                                                    text: String(modelData)
                                                    font.pixelSize: Math.max(7, 2.2 * paper.fitScale)
                                                    elide: Text.ElideRight
                                                    verticalAlignment: Text.AlignVCenter
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }

                    // Ghost for base view placement
                    Rectangle {
                        visible: ghostVisible && activeTool === "baseView"
                        x: ghostX - width / 2
                        y: ghostY - height / 2
                        width: 90 * paper.fitScale
                        height: 70 * paper.fitScale
                        color: "#2d7ff928"
                        border.color: "#2d7ff9"
                        border.width: 1.5
                        radius: 2
                        z: 40
                        Text {
                            anchors.centerIn: parent
                            text: placeOrientation
                            color: "#2d7ff9"
                            font.bold: true
                            font.pixelSize: 12
                        }
                    }

                    // Dimension rubber band
                    Canvas {
                        id: dimPreview
                        anchors.fill: parent
                        visible: dimDragging
                        z: 45
                        property real x2: 0
                        property real y2: 0
                        onPaint: {
                            if (!dimDragging) return
                            const ctx = getContext("2d")
                            ctx.reset()
                            ctx.strokeStyle = "#2d7ff9"
                            ctx.setLineDash([4, 3])
                            ctx.beginPath()
                            ctx.moveTo(dimStartX, dimStartY)
                            ctx.lineTo(x2, y2)
                            ctx.stroke()
                        }
                    }

                    // Title block
                    Rectangle {
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        anchors.margins: 14 * paper.fitScale
                        width: 190
                        height: 70
                        color: "#f7f7f7"
                        border.color: "#444"
                        Column {
                            anchors.fill: parent
                            anchors.margins: 5
                            spacing: 1
                            Text {
                                text: activeSheet ? (activeSheet.partName || activeSheet.title || activeSheet.name) : "Sheet"
                                font.pixelSize: 11; font.bold: true; color: "#222"
                                width: parent.width; elide: Text.ElideRight
                            }
                            Text {
                                text: (activeSheet && activeSheet.drawnBy ? "By " + activeSheet.drawnBy + "  " : "")
                                      + (activeSheet ? "Rev " + activeSheet.revision : "")
                                font.pixelSize: 9; color: "#555"
                            }
                            Text {
                                text: (activeSheet ? activeSheet.date : "")
                                      + (activeSheet && activeSheet.material ? " · " + activeSheet.material : "")
                                font.pixelSize: 9; color: "#555"
                            }
                            Text {
                                text: (drawingDoc ? drawingDoc.standard : "ISO")
                                      + " · " + (activeSheet ? activeSheet.size : "A3")
                                      + " · " + (drawingDoc ? drawingDoc.projectionAngle + "-angle" : "")
                                font.pixelSize: 8; color: "#888"
                            }
                        }
                    }

                    MouseArea {
                        id: paperMouse
                        anchors.fill: parent
                        acceptedButtons: Qt.LeftButton
                        hoverEnabled: true
                        propagateComposedEvents: true
                        z: 1
                        cursorShape: (activeTool === "baseView" || activeTool === "dimension"
                                      || activeTool === "text" || activeTool === "centerMark"
                                      || activeTool === "sectionView" || activeTool === "detailView")
                                     ? Qt.CrossCursor : Qt.ArrowCursor

                        onPositionChanged: (m) => {
                            if (activeTool === "baseView") {
                                ghostX = m.x; ghostY = m.y; ghostVisible = true
                            }
                            if (dimDragging) {
                                dimPreview.x2 = m.x; dimPreview.y2 = m.y; dimPreview.requestPaint()
                            }
                        }
                        onPressed: (m) => {
                            drawingWS.forceActiveFocus()
                            if (activeTool === "dimension") {
                                dimStartX = m.x; dimStartY = m.y; dimDragging = true
                                dimPreview.x2 = m.x; dimPreview.y2 = m.y; dimPreview.requestPaint()
                                m.accepted = true
                                return
                            }
                            if (activeTool === "baseView") {
                                const p = paperToMm(m.x, m.y)
                                // Center the view on the click
                                placeBaseAt(p.x - 40, p.y - 30)
                                m.accepted = true
                                return
                            }
                            if (activeTool === "sectionView" || activeTool === "detailView") {
                                const p = paperToMm(m.x, m.y)
                                placeSectionOrDetail(activeTool === "sectionView" ? "section" : "detail", p.x, p.y)
                                m.accepted = true
                                return
                            }
                            if (activeTool === "text") {
                                addTextAt(m.x, m.y, "Note")
                                cancelPlace()
                                m.accepted = true
                                return
                            }
                            if (activeTool === "centerMark") {
                                const p = paperToMm(m.x, m.y)
                                activeSheet.addAnnotation({ kind: "centerMark", x: p.x, y: p.y, radius: 3 })
                                cancelPlace()
                                m.accepted = true
                                return
                            }
                            // Deselect when clicking empty paper
                            selectedView = null
                            m.accepted = false
                        }
                        onReleased: (m) => {
                            if (dimDragging && activeTool === "dimension") {
                                addDimension(dimStartX, dimStartY, m.x, m.y)
                                dimDragging = false
                                cancelPlace()
                            }
                        }
                    }
                }
            }

            // Pan / zoom over the whole sheet area (middle button or space+left)
            MouseArea {
                id: navMouse
                anchors.fill: parent
                acceptedButtons: Qt.MiddleButton | Qt.RightButton
                propagateComposedEvents: true
                property real lastX: 0
                property real lastY: 0
                z: 5
                onPressed: (m) => { lastX = m.x; lastY = m.y }
                onPositionChanged: (m) => {
                    if (!pressed) return
                    sheetPanX += m.x - lastX
                    sheetPanY += m.y - lastY
                    lastX = m.x; lastY = m.y
                }
                onWheel: (w) => {
                    const f = w.angleDelta.y > 0 ? 1.12 : 1 / 1.12
                    zoomAt(f, w.x, w.y)
                }
            }
        }
    }
}
