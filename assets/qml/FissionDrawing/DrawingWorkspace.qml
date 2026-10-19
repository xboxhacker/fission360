// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import "../FissionApplication"

// Drawing workspace: sheet with 2D projected views, annotations, title block.
Item {
    id: drawingWS
    anchors.fill: parent

    property var app
    property var drawingDoc: null
    property var activeSheet: null
    property string activeTool: "select"
    property string pendingViewType: ""
    property string placeOrientation: "front"
    property string placeDirection: "right"
    property var selectedView: null
    property real dimStartX: 0
    property real dimStartY: 0
    property bool dimDragging: false

    function syncSheet() {
        if (drawingDoc && drawingDoc.sheetCount > 0) {
            const idx = Math.max(0, Math.min(drawingDoc.activeSheetIndex, drawingDoc.sheetCount - 1))
            activeSheet = drawingDoc.sheet(idx)
        } else {
            activeSheet = null
        }
    }

    onDrawingDocChanged: syncSheet()
    Connections {
        target: drawingDoc
        function onSheetsChanged() { syncSheet() }
        function onActiveSheetChanged() { syncSheet() }
    }

    function designDoc() { return app ? app.doc : null }

    function projectView(view) {
        if (!view || !fissionApp.drawingManager) return
        const ok = fissionApp.drawingManager.projectView(view, designDoc(), drawingDoc)
        if (!ok)
            fissionApp.showMessage(fissionApp.drawingManager.lastError || "Projection failed")
        return ok
    }

    function placeViewAt(mx, my) {
        if (!pendingViewType || !activeSheet) return
        const opts = {
            "position": Qt.point(mx / Math.max(0.01, paper.paperScale), my / Math.max(0.01, paper.paperScale)),
            "scale": 1.0,
            "orientation": placeOrientation,
            "showHiddenLines": false
        }
        if (pendingViewType === "projected" || pendingViewType === "section" || pendingViewType === "detail"
                || pendingViewType === "auxiliary") {
            if (selectedView)
                opts["parentViewId"] = selectedView.id
            opts["projectDirection"] = placeDirection
            if (selectedView)
                opts["orientation"] = selectedView.orientation
        }
        if (pendingViewType === "detail") {
            opts["detailRadius"] = 20
            opts["detailCenter"] = Qt.point(0, 0)
            opts["scale"] = 2.0
        }
        const view = activeSheet.addView(pendingViewType === "base" ? "base" : pendingViewType, opts)
        projectView(view)
        selectedView = view
        pendingViewType = ""
        activeTool = "select"
        fissionApp.showMessage((view.type.charAt(0).toUpperCase() + view.type.slice(1)) + " view placed")
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
        d.outOfDate  // dependency
        design.contentRevision
        return d.sourceDesignRevision !== design.contentRevision
    }

    function addTextAt(mx, my, text) {
        if (!activeSheet) return
        activeSheet.addAnnotation({
            kind: "text",
            x: mx / paper.paperScale,
            y: my / paper.paperScale,
            text: text || "Note",
            fontSize: 3.5
        })
    }

    function addDimension(x1, y1, x2, y2) {
        if (!activeSheet) return
        const dx = (x2 - x1) / paper.paperScale
        const dy = (y2 - y1) / paper.paperScale
        const len = Math.sqrt(dx * dx + dy * dy)
        activeSheet.addAnnotation({
            kind: "dimension",
            x: x1 / paper.paperScale,
            y: y1 / paper.paperScale,
            x2: x2 / paper.paperScale,
            y2: y2 / paper.paperScale,
            text: len.toFixed(2) + " mm"
        })
    }

    function exportPdf() {
        exportDialog.kind = "pdf"
        exportDialog.open()
    }
    function exportDxf() {
        exportDialog.kind = "dxf"
        exportDialog.open()
    }
    function exportPng() {
        exportDialog.kind = "png"
        exportDialog.open()
    }
    function exportTiff() {
        exportDialog.kind = "tiff"
        exportDialog.open()
    }
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
            const path = selectedFile.toString().replace(/^file:\/\//, "").replace(/^\/([A-Za-z]:)/, "$1")
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
            const path = selectedFile.toString()
            const id = fissionApp.drawingManager.addImage(activeSheet, path, Qt.point(30, 30), 40)
            fissionApp.showMessage(id ? "Image placed" : (fissionApp.drawingManager.lastError || "Could not place image"))
        }
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        // Drawing browser
        Rectangle {
            Layout.preferredWidth: 180
            Layout.fillHeight: true
            color: Theme.panel
            border.color: Theme.border
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 6
                spacing: 4
                Text { text: "DRAWING"; font.pixelSize: 10; font.bold: true; color: Theme.textDim }
                Text {
                    text: drawingDoc ? drawingDoc.name : "—"
                    font.pixelSize: 12; font.bold: true; color: Theme.text
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
                    delegate: Text {
                        required property int index
                        text: {
                            const s = drawingDoc.sheet(index)
                            return (index === drawingDoc.activeSheetIndex ? "▸ " : "  ") + (s ? s.name : "")
                        }
                        color: Theme.text
                        font.pixelSize: 11
                        MouseArea {
                            anchors.fill: parent
                            onClicked: drawingDoc.activeSheetIndex = index
                        }
                    }
                }
                Button {
                    text: "Add Sheet"
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
                            delegate: Text {
                                required property int index
                                width: viewCol.width
                                text: {
                                    const v = activeSheet.view(index)
                                    return (selectedView && v && selectedView.id === v.id ? "▸ " : "  ")
                                           + (v ? v.name : "")
                                }
                                color: Theme.text
                                font.pixelSize: 11
                                elide: Text.ElideRight
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
                Button {
                    text: "Regenerate sheet"
                    flat: true
                    onClicked: regenerateAll()
                }
                Text {
                    visible: drawingOutOfDate
                    text: "Out of date"
                    color: Theme.error
                    font.pixelSize: 11
                    font.bold: true
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }

        // Paper area
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            Rectangle {
                anchors.fill: parent
                anchors.margins: 12
                color: "#e8e8e8"
                border.color: Theme.border

                Rectangle {
                    id: paper
                    anchors.centerIn: parent
                    width: Math.min(parent.width - 24, (activeSheet ? activeSheet.width : 420) * paperScale)
                    height: Math.min(parent.height - 24, (activeSheet ? activeSheet.height : 297) * paperScale)
                    readonly property real paperScale: {
                        if (!activeSheet) return 1
                        const sx = (parent.width - 24) / Math.max(1, activeSheet.width)
                        const sy = (parent.height - 24) / Math.max(1, activeSheet.height)
                        return Math.min(sx, sy)
                    }
                    color: "white"
                    border.color: "#bbb"

                    Canvas {
                        anchors.fill: parent
                        onPaint: {
                            const ctx = getContext("2d")
                            ctx.clearRect(0, 0, width, height)
                            ctx.strokeStyle = "#eee"
                            ctx.lineWidth = 1
                            const step = 10 * paper.paperScale
                            for (let x = 0; x < width; x += step) {
                                ctx.beginPath(); ctx.moveTo(x, 0); ctx.lineTo(x, height); ctx.stroke()
                            }
                            for (let y = 0; y < height; y += step) {
                                ctx.beginPath(); ctx.moveTo(0, y); ctx.lineTo(width, y); ctx.stroke()
                            }
                        }
                        onWidthChanged: requestPaint()
                        onHeightChanged: requestPaint()
                    }

                    Repeater {
                        model: activeSheet ? activeSheet.viewCount : 0
                        delegate: DrawingViewItem {
                            required property int index
                            view: activeSheet ? activeSheet.view(index) : null
                            selection: app ? app.selection : null
                            paperScale: paper.paperScale
                            x: view ? view.position.x * paper.paperScale : 0
                            y: view ? view.position.y * paper.paperScale : 0
                            selected: selectedView && view && selectedView.id === view.id
                            onClicked: selectedView = view
                            onDragFinished: (nx, ny) => {
                                if (view) view.position = Qt.point(nx / paper.paperScale, ny / paper.paperScale)
                            }
                        }
                    }

                    // Annotations overlay
                    Repeater {
                        model: activeSheet ? activeSheet.annotationCount : 0
                        delegate: Item {
                            required property int index
                            readonly property var ann: activeSheet ? activeSheet.annotation(index) : ({})
                            x: (ann.x || 0) * paper.paperScale
                            y: (ann.y || 0) * paper.paperScale
                            Canvas {
                                width: 200; height: 40
                                anchors.centerIn: parent
                                visible: ann.kind === "dimension" || ann.kind === "leader"
                                onPaint: {
                                    const ctx = getContext("2d")
                                    ctx.reset()
                                    ctx.strokeStyle = "#222"
                                    ctx.beginPath()
                                    const x2 = ((ann.x2 || ann.x) - ann.x) * paper.paperScale
                                    const y2 = ((ann.y2 || ann.y) - ann.y) * paper.paperScale
                                    ctx.moveTo(0, 0); ctx.lineTo(x2, y2); ctx.stroke()
                                }
                            }
                            Text {
                                visible: ann.kind === "text" || ann.kind === "holeNote" || ann.kind === "dimension"
                                text: ann.text || ""
                                font.pixelSize: Math.max(9, (ann.fontSize || 3.5) * paper.paperScale)
                                color: "#222"
                            }
                            Image {
                                visible: ann.kind === "image"
                                source: ann.path ? ("file:///" + ann.path.replace(/\\/g, "/")) : ""
                                width: (ann.width || 40) * paper.paperScale
                                height: (ann.height || 30) * paper.paperScale
                                fillMode: Image.PreserveAspectFit
                            }
                        }
                    }

                    // Tables
                    Repeater {
                        model: activeSheet ? activeSheet.tableCount : 0
                        delegate: Rectangle {
                            required property int index
                            readonly property var tbl: activeSheet ? activeSheet.table(index) : ({})
                            x: (tbl.x || 0) * paper.paperScale
                            y: (tbl.y || 0) * paper.paperScale
                            width: {
                                const cols = tbl.colWidths || [20, 20, 40]
                                let w = 0
                                for (let i = 0; i < cols.length; ++i) w += cols[i]
                                return w * paper.paperScale
                            }
                            height: ((tbl.rows ? tbl.rows.length : 1) * 5 + 4) * paper.paperScale
                            color: "#fafafa"
                            border.color: "#666"
                            Column {
                                anchors.fill: parent
                                anchors.margins: 1
                                Text {
                                    text: tbl.title || "Table"
                                    font.pixelSize: Math.max(8, 2.5 * paper.paperScale)
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
                                                width: ((tbl.colWidths && tbl.colWidths[index]) || 20) * paper.paperScale
                                                height: 5 * paper.paperScale
                                                border.color: "#888"
                                                border.width: 0.5
                                                color: "white"
                                                Text {
                                                    anchors.fill: parent
                                                    anchors.margins: 1
                                                    text: String(modelData)
                                                    font.pixelSize: Math.max(7, 2.2 * paper.paperScale)
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

                    MouseArea {
                        anchors.fill: parent
                        acceptedButtons: Qt.LeftButton
                        propagateComposedEvents: true
                        cursorShape: pendingViewType || activeTool === "text" || activeTool === "dimension"
                                     ? Qt.CrossCursor : Qt.ArrowCursor
                        onPressed: (mouse) => {
                            if (activeTool === "dimension") {
                                dimStartX = mouse.x; dimStartY = mouse.y; dimDragging = true
                                mouse.accepted = true
                                return
                            }
                            if (pendingViewType) {
                                placeViewAt(mouse.x, mouse.y)
                                mouse.accepted = true
                                return
                            }
                            if (activeTool === "text") {
                                addTextAt(mouse.x, mouse.y, "Note")
                                activeTool = "select"
                                mouse.accepted = true
                                return
                            }
                            if (activeTool === "centerMark") {
                                activeSheet.addAnnotation({
                                    kind: "centerMark",
                                    x: mouse.x / paper.paperScale,
                                    y: mouse.y / paper.paperScale,
                                    radius: 3
                                })
                                activeTool = "select"
                                mouse.accepted = true
                                return
                            }
                            mouse.accepted = false
                        }
                        onReleased: (mouse) => {
                            if (dimDragging && activeTool === "dimension") {
                                addDimension(dimStartX, dimStartY, mouse.x, mouse.y)
                                dimDragging = false
                                activeTool = "select"
                            }
                        }
                    }

                    // Title block
                    Rectangle {
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        anchors.margins: 8
                        width: 180
                        height: 64
                        color: "#f8f8f8"
                        border.color: "#999"
                        Column {
                            anchors.fill: parent
                            anchors.margins: 4
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
                                      + " · " + (activeSheet ? Math.round(activeSheet.width) + "×" + Math.round(activeSheet.height) + " mm" : "")
                                font.pixelSize: 8; color: "#888"
                            }
                        }
                    }
                }
            }

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.top: parent.top
                anchors.topMargin: 4
                visible: pendingViewType !== "" || activeTool === "text" || activeTool === "dimension"
                text: pendingViewType ? ("Click to place " + pendingViewType + " view (" + placeOrientation + ")")
                      : activeTool === "text" ? "Click to place a note"
                      : activeTool === "dimension" ? "Drag to place a dimension"
                      : ""
                color: Theme.accent
                font.pixelSize: 12
                font.bold: true
                z: 20
            }
        }
    }
}
