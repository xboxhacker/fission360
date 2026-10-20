// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import "."

// Manufacture workspace panel: setups, tool library, full op catalog, post / simulate.
Item {
    id: panel
    property var app
    property int tabIndex: 0
    property string selectedSetupId: ""
    property string selectedToolId: ""
    property string selectedOpId: ""
    property string simulateText: ""
    implicitHeight: 520

    readonly property var mfg: (app && app.doc) ? app.doc.manufacture : ({})
    readonly property var setups: mfg.setups || []
    readonly property var tools: mfg.tools || []
    readonly property var operations: mfg.operations || []
    readonly property string activeSetupId: mfg.activeSetupId || ""

    readonly property var opCatalog: [
        { group: "2D Milling", items: [
            { type: "face", name: "Face" },
            { type: "2d-adaptive", name: "2D Adaptive" },
            { type: "2d-pocket", name: "2D Pocket" },
            { type: "2d-contour", name: "2D Contour" },
            { type: "slot", name: "Slot" },
            { type: "trace", name: "Trace" },
            { type: "bore", name: "Bore" },
            { type: "circular", name: "Circular" },
            { type: "engrave", name: "Engrave" },
            { type: "chamfer", name: "Chamfer" },
            { type: "thread", name: "Thread" }
        ]},
        { group: "Drilling", items: [
            { type: "drill", name: "Drill" },
            { type: "tap", name: "Tap" },
            { type: "thread-mill", name: "Thread Mill" }
        ]},
        { group: "3D Milling", items: [
            { type: "3d-adaptive", name: "Adaptive Clearing" },
            { type: "3d-pocket", name: "Pocket Clearing" },
            { type: "parallel", name: "Parallel" },
            { type: "3d-contour", name: "3D Contour" },
            { type: "flat", name: "Flat" },
            { type: "scallop", name: "Scallop" },
            { type: "radial", name: "Radial" },
            { type: "spiral", name: "Spiral" },
            { type: "morphed-spiral", name: "Morphed Spiral" },
            { type: "pencil", name: "Pencil" },
            { type: "ramp", name: "Ramp" },
            { type: "steep-shallow", name: "Steep & Shallow" },
            { type: "project", name: "Project" },
            { type: "morph", name: "Morph" },
            { type: "flow", name: "Flow" },
            { type: "blend", name: "Blend" }
        ]},
        { group: "Multi-axis", items: [
            { type: "swarf", name: "Swarf" },
            { type: "multi-contour", name: "Multi-Axis Contour" },
            { type: "rotary", name: "Rotary / Wrap" }
        ]},
        { group: "Turning", items: [
            { type: "turning-face", name: "Facing" },
            { type: "turning-profile", name: "Profiling" },
            { type: "turning-groove", name: "Grooving" },
            { type: "turning-part", name: "Parting" },
            { type: "turning-thread", name: "Threading" },
            { type: "turning-adaptive", name: "Adaptive Roughing" }
        ]},
        { group: "Probing", items: [
            { type: "probe-wcs", name: "Probe WCS" },
            { type: "probe-geometry", name: "Probe Geometry" }
        ]}
    ]

    function close() { if (app) app.showManufacture = false }

    function activeSetup() {
        for (const s of setups) if (s.id === activeSetupId) return s
        return null
    }

    function setupById(id) {
        for (const s of setups) if (s.id === id) return s
        return null
    }

    function toolById(id) {
        for (const t of tools) if (t.id === id) return t
        return null
    }

    function opById(id) {
        for (const o of operations) if (o.id === id) return o
        return null
    }

    function opsForActive() {
        return operations.filter(o => !activeSetupId || o.setupId === activeSetupId)
    }

    function toolLabel(id) {
        const t = toolById(id)
        return t ? t.name : "(no tool)"
    }

    function pickToolForType(type) {
        const prefer = (type.indexOf("drill") >= 0 || type.indexOf("bore") >= 0) ? "drill"
                     : (type.indexOf("tap") >= 0) ? "tap"
                     : (type.indexOf("turn") >= 0) ? "turning"
                     : (type.indexOf("probe") >= 0) ? "probe"
                     : (type === "face" || type === "flat") ? "facemill"
                     : "endmill"
        for (const t of tools) if (t.type === prefer) return t.id
        return tools.length ? tools[0].id : ""
    }

    function newSetup() {
        if (!app || !app.doc) return
        const id = app.doc.addMfgItem("setups", {})
        if (id) {
            selectedSetupId = id
            tabIndex = 0
            fissionApp.showMessage("Setup created — stock from body bounds + offset")
        } else fissionApp.showMessage(app.doc.lastError || "Setup failed")
    }

    function addTool(preset) {
        if (!app || !app.doc) return
        const props = preset || { name: "New Tool", type: "endmill", diameter: 6, flutes: 4 }
        const id = app.doc.addMfgItem("tools", props)
        if (id) { selectedToolId = id; tabIndex = 1; fissionApp.showMessage("Tool added") }
    }

    function addOp(type, name) {
        if (!app || !app.doc) return
        if (!activeSetupId) {
            fissionApp.showMessage("Create a New Setup first")
            tabIndex = 0
            return
        }
        const id = app.doc.addMfgItem("operations", {
            type: type,
            name: name || type,
            setupId: activeSetupId,
            toolId: pickToolForType(type),
            folder: ""
        })
        if (id) {
            selectedOpId = id
            tabIndex = 2
            fissionApp.showMessage(name + " added to " + (activeSetup() ? activeSetup().name : "setup"))
        } else fissionApp.showMessage(app.doc.lastError || "Operation failed")
    }

    function patchSetup(id, patch) {
        if (app && app.doc) app.doc.updateMfgItem("setups", id, patch)
    }

    function patchTool(id, patch) {
        if (app && app.doc) app.doc.updateMfgItem("tools", id, patch)
    }

    function patchOp(id, patch) {
        if (app && app.doc) app.doc.updateMfgItem("operations", id, patch)
    }

    function refreshStock() {
        if (!app || !app.doc) return
        const ok = app.doc.refreshMfgStock(selectedSetupId || activeSetupId)
        fissionApp.showMessage(ok ? "Stock refreshed from body bounds" : (app.doc.lastError || "Refresh failed"))
    }

    function simulate() {
        if (!app || !app.doc) return
        simulateText = app.doc.simulateMfgReport()
        fissionApp.showMessage(simulateText.split("\n")[0] || "Simulate")
        tabIndex = 3
    }

    FileDialog {
        id: postDlg
        title: "Export NC program"
        fileMode: FileDialog.SaveFile
        nameFilters: ["NC / G-code (*.nc *.tap *.gcode)", "All files (*)"]
        onAccepted: {
            const local = fissionApp.toLocalPath(selectedFile.toString())
            let path = local
            if (!path.toLowerCase().match(/\.(nc|tap|gcode)$/)) path += ".nc"
            const out = app.doc.exportMfgGCode(path)
            fissionApp.showMessage(out ? ("Posted → " + out) : (app.doc.lastError || "Post failed"))
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 38
            color: Theme.header
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 8
                Text {
                    text: "Manufacture"
                    color: Theme.headerText
                    font.bold: true
                    font.pixelSize: 13
                    Layout.fillWidth: true
                }
                ToolbarButton {
                    compact: true; glyph: "close"; glyphColor: "white"; tip: "Close"
                    Layout.preferredWidth: 22; Layout.preferredHeight: 22; z: 2
                    onClicked: panel.close()
                }
            }
        }

        Text {
            Layout.fillWidth: true
            Layout.leftMargin: 10
            Layout.rightMargin: 10
            Layout.topMargin: 8
            wrapMode: Text.WordWrap
            color: Theme.textDim
            font.pixelSize: 11
            text: "Full MFG catalog on the design. Post writes op-type stock NC stubs (not a CAM kernel). Send to Slicer for additive."
        }

        TabBar {
            id: tabs
            Layout.fillWidth: true
            Layout.leftMargin: 8
            Layout.rightMargin: 8
            Layout.topMargin: 6
            currentIndex: panel.tabIndex
            onCurrentIndexChanged: panel.tabIndex = currentIndex
            TabButton { text: "Setup" }
            TabButton { text: "Tools" }
            TabButton { text: "Ops" }
            TabButton { text: "Finish" }
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.margins: 8
            currentIndex: panel.tabIndex

            // ── Setup ──────────────────────────────────────────────
            ColumnLayout {
                spacing: 6
                RowLayout {
                    Button { text: "New Setup"; highlighted: true; onClicked: panel.newSetup() }
                    Button {
                        text: "Refresh Stock"
                        enabled: !!(panel.selectedSetupId || panel.activeSetupId)
                        onClicked: panel.refreshStock()
                    }
                }
                ListView {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 120
                    clip: true
                    model: panel.setups
                    spacing: 2
                    delegate: Rectangle {
                        width: ListView.view.width
                        height: 36
                        radius: 3
                        color: modelData.id === (panel.selectedSetupId || panel.activeSetupId) ? Theme.accentLight : Theme.panelAlt
                        border.color: Theme.border
                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 6
                            Text {
                                text: modelData.name + (modelData.id === panel.activeSetupId ? " ●" : "")
                                color: Theme.text
                                Layout.fillWidth: true
                            }
                            Button {
                                text: "Active"
                                flat: true
                                enabled: modelData.id !== panel.activeSetupId
                                onClicked: app.doc.setActiveMfgSetup(modelData.id)
                            }
                            Button {
                                text: "✕"
                                flat: true
                                onClicked: {
                                    if (panel.selectedSetupId === modelData.id) panel.selectedSetupId = ""
                                    app.doc.removeMfgItem("setups", modelData.id)
                                }
                            }
                        }
                        MouseArea {
                            anchors.fill: parent
                            z: -1
                            onClicked: {
                                panel.selectedSetupId = modelData.id
                                app.doc.setActiveMfgSetup(modelData.id)
                            }
                        }
                    }
                }
                // Setup inspector
                Rectangle {
                    id: setupInsp
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    color: Theme.panelAlt
                    radius: 4
                    border.color: Theme.border
                    visible: !!(panel.setupById(panel.selectedSetupId || panel.activeSetupId))
                    readonly property var s: panel.setupById(panel.selectedSetupId || panel.activeSetupId)
                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 8
                        spacing: 4
                        Label { text: "Setup properties"; font.bold: true; color: Theme.text }
                        RowLayout {
                            Label { text: "Name"; color: Theme.textDim; Layout.preferredWidth: 72 }
                            TextField {
                                Layout.fillWidth: true
                                text: setupInsp.s ? setupInsp.s.name : ""
                                onEditingFinished: if (setupInsp.s) panel.patchSetup(setupInsp.s.id, { name: text })
                            }
                        }
                        RowLayout {
                            Label { text: "Machine"; color: Theme.textDim; Layout.preferredWidth: 72 }
                            ComboBox {
                                id: machineBox
                                Layout.fillWidth: true
                                model: ["Generic 3-axis", "Generic 4-axis", "Generic 5-axis", "Generic Lathe", "Generic Mill-Turn"]
                                currentIndex: {
                                    const m = setupInsp.s ? (setupInsp.s.machine || "Generic 3-axis") : "Generic 3-axis"
                                    const i = machineBox.model.indexOf(m)
                                    return i >= 0 ? i : 0
                                }
                                onActivated: if (setupInsp.s) panel.patchSetup(setupInsp.s.id, { machine: currentText })
                            }
                        }
                        RowLayout {
                            Label { text: "WCS"; color: Theme.textDim; Layout.preferredWidth: 72 }
                            ComboBox {
                                id: wcsBox
                                Layout.fillWidth: true
                                model: ["model-origin", "stock-box", "selected-point", "fixture"]
                                currentIndex: {
                                    const m = setupInsp.s ? (setupInsp.s.wcs || "model-origin") : "model-origin"
                                    const i = wcsBox.model.indexOf(m)
                                    return i >= 0 ? i : 0
                                }
                                onActivated: if (setupInsp.s) panel.patchSetup(setupInsp.s.id, { wcs: currentText })
                            }
                        }
                        RowLayout {
                            Label { text: "Stock +mm"; color: Theme.textDim; Layout.preferredWidth: 72 }
                            TextField {
                                Layout.fillWidth: true
                                text: setupInsp.s ? String(setupInsp.s.stockOffset !== undefined ? setupInsp.s.stockOffset : 2) : "2"
                                onEditingFinished: {
                                    const v = parseFloat(text)
                                    if (!isNaN(v) && setupInsp.s) {
                                        panel.patchSetup(setupInsp.s.id, { stockOffset: v })
                                        panel.refreshStock()
                                    }
                                }
                            }
                        }
                        Label {
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            color: Theme.textDim
                            font.pixelSize: 10
                            text: {
                                const s = setupInsp.s
                                if (!s || !s.stock || s.stock.length < 6) return "No stock box yet — add bodies, then Refresh Stock."
                                const dx = (s.stock[3] - s.stock[0]).toFixed(1)
                                const dy = (s.stock[4] - s.stock[1]).toFixed(1)
                                const dz = (s.stock[5] - s.stock[2]).toFixed(1)
                                return "Stock " + dx + " × " + dy + " × " + dz + " mm"
                            }
                        }
                    }
                }
            }

            // ── Tools ──────────────────────────────────────────────
            ColumnLayout {
                spacing: 6
                RowLayout {
                    Button { text: "End Mill"; onClicked: panel.addTool({ name: "End Mill", type: "endmill", diameter: 6, flutes: 4 }) }
                    Button { text: "Drill"; onClicked: panel.addTool({ name: "Drill", type: "drill", diameter: 3.3, flutes: 2 }) }
                    Button { text: "Tap"; onClicked: panel.addTool({ name: "Tap", type: "tap", diameter: 4, pitch: 0.7, flutes: 3 }) }
                    Button { text: "Turning"; onClicked: panel.addTool({ name: "Turning Insert", type: "turning", diameter: 0.8, flutes: 1 }) }
                    Button { text: "Probe"; onClicked: panel.addTool({ name: "Touch Probe", type: "probe", diameter: 6, flutes: 0 }) }
                }
                ListView {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 140
                    clip: true
                    model: panel.tools
                    spacing: 2
                    delegate: Rectangle {
                        width: ListView.view.width
                        height: 40
                        radius: 3
                        color: modelData.id === panel.selectedToolId ? Theme.accentLight : Theme.panelAlt
                        border.color: Theme.border
                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 6
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 0
                                Text { text: modelData.name; color: Theme.text; font.pixelSize: 12 }
                                Text {
                                    text: (modelData.type || "") + "  Ø" + (modelData.diameter || "?") + " mm"
                                    color: Theme.textDim; font.pixelSize: 10
                                }
                            }
                            Button {
                                text: "✕"
                                flat: true
                                onClicked: {
                                    if (panel.selectedToolId === modelData.id) panel.selectedToolId = ""
                                    app.doc.removeMfgItem("tools", modelData.id)
                                }
                            }
                        }
                        MouseArea {
                            anchors.fill: parent
                            z: -1
                            onClicked: panel.selectedToolId = modelData.id
                        }
                    }
                }
                Rectangle {
                    id: toolInsp
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    color: Theme.panelAlt
                    radius: 4
                    border.color: Theme.border
                    visible: !!panel.toolById(panel.selectedToolId)
                    readonly property var t: panel.toolById(panel.selectedToolId)
                    GridLayout {
                        anchors.fill: parent
                        anchors.margins: 8
                        columns: 2
                        columnSpacing: 8
                        rowSpacing: 4
                        Label { text: "Name"; color: Theme.textDim }
                        TextField {
                            Layout.fillWidth: true
                            text: toolInsp.t ? toolInsp.t.name : ""
                            onEditingFinished: if (toolInsp.t) panel.patchTool(toolInsp.t.id, { name: text })
                        }
                        Label { text: "Type"; color: Theme.textDim }
                        ComboBox {
                            id: toolTypeBox
                            Layout.fillWidth: true
                            model: ["endmill", "facemill", "ballmill", "drill", "tap", "turning", "probe", "chamfer", "threadmill"]
                            currentIndex: {
                                const m = toolInsp.t ? toolInsp.t.type : "endmill"
                                const i = toolTypeBox.model.indexOf(m)
                                return i >= 0 ? i : 0
                            }
                            onActivated: if (toolInsp.t) panel.patchTool(toolInsp.t.id, { type: currentText })
                        }
                        Label { text: "Ø mm"; color: Theme.textDim }
                        TextField {
                            Layout.fillWidth: true
                            text: toolInsp.t ? String(toolInsp.t.diameter || 6) : "6"
                            onEditingFinished: {
                                const v = parseFloat(text)
                                if (!isNaN(v) && toolInsp.t) panel.patchTool(toolInsp.t.id, { diameter: v })
                            }
                        }
                        Label { text: "Flutes"; color: Theme.textDim }
                        TextField {
                            Layout.fillWidth: true
                            text: toolInsp.t ? String(toolInsp.t.flutes !== undefined ? toolInsp.t.flutes : 4) : "4"
                            onEditingFinished: {
                                const v = parseInt(text, 10)
                                if (!isNaN(v) && toolInsp.t) panel.patchTool(toolInsp.t.id, { flutes: v })
                            }
                        }
                        Label { text: "Feed"; color: Theme.textDim }
                        TextField {
                            Layout.fillWidth: true
                            text: toolInsp.t ? String(toolInsp.t.feed || 500) : "500"
                            onEditingFinished: {
                                const v = parseFloat(text)
                                if (!isNaN(v) && toolInsp.t) panel.patchTool(toolInsp.t.id, { feed: v })
                            }
                        }
                        Label { text: "Speed"; color: Theme.textDim }
                        TextField {
                            Layout.fillWidth: true
                            text: toolInsp.t ? String(toolInsp.t.speed || 8000) : "8000"
                            onEditingFinished: {
                                const v = parseFloat(text)
                                if (!isNaN(v) && toolInsp.t) panel.patchTool(toolInsp.t.id, { speed: v })
                            }
                        }
                    }
                }
            }

            // ── Ops ────────────────────────────────────────────────
            ColumnLayout {
                spacing: 4
                Label {
                    text: activeSetup() ? ("Ops on " + activeSetup().name) : "No active setup — create one on Setup"
                    color: Theme.text
                    font.bold: true
                }
                ScrollView {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 110
                    clip: true
                    ColumnLayout {
                        width: parent.width
                        spacing: 4
                        Repeater {
                            model: panel.opCatalog
                            delegate: ColumnLayout {
                                id: catRow
                                required property var modelData
                                spacing: 2
                                Label { text: catRow.modelData.group; color: Theme.textDim; font.pixelSize: 10; font.bold: true }
                                Flow {
                                    Layout.fillWidth: true
                                    spacing: 3
                                    Repeater {
                                        model: catRow.modelData.items
                                        delegate: Button {
                                            required property var modelData
                                            text: modelData.name
                                            flat: true
                                            font.pixelSize: 11
                                            onClicked: panel.addOp(modelData.type, modelData.name)
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
                ListView {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 120
                    clip: true
                    model: panel.opsForActive()
                    spacing: 2
                    delegate: Rectangle {
                        width: ListView.view.width
                        height: 36
                        radius: 3
                        color: modelData.id === panel.selectedOpId ? Theme.accentLight : Theme.panelAlt
                        border.color: Theme.border
                        opacity: modelData.enabled === false ? 0.45 : 1
                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 6
                            Text {
                                text: (modelData.folder ? ("[" + modelData.folder + "] ") : "")
                                       + modelData.name + "  ·  " + panel.toolLabel(modelData.toolId)
                                color: Theme.text
                                Layout.fillWidth: true
                                elide: Text.ElideRight
                            }
                            Button {
                                text: modelData.enabled === false ? "On" : "Off"
                                flat: true
                                onClicked: panel.patchOp(modelData.id, { enabled: modelData.enabled === false })
                            }
                            Button {
                                text: "✕"
                                flat: true
                                onClicked: {
                                    if (panel.selectedOpId === modelData.id) panel.selectedOpId = ""
                                    app.doc.removeMfgItem("operations", modelData.id)
                                }
                            }
                        }
                        MouseArea {
                            anchors.fill: parent
                            z: -1
                            onClicked: panel.selectedOpId = modelData.id
                        }
                    }
                }
                Rectangle {
                    id: opInsp
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    color: Theme.panelAlt
                    radius: 4
                    border.color: Theme.border
                    visible: !!panel.opById(panel.selectedOpId)
                    readonly property var o: panel.opById(panel.selectedOpId)
                    GridLayout {
                        anchors.fill: parent
                        anchors.margins: 8
                        columns: 2
                        columnSpacing: 8
                        rowSpacing: 4
                        Label { text: "Name"; color: Theme.textDim }
                        TextField {
                            Layout.fillWidth: true
                            text: opInsp.o ? opInsp.o.name : ""
                            onEditingFinished: if (opInsp.o) panel.patchOp(opInsp.o.id, { name: text })
                        }
                        Label { text: "Folder"; color: Theme.textDim }
                        TextField {
                            Layout.fillWidth: true
                            placeholderText: "e.g. Roughing"
                            text: opInsp.o ? (opInsp.o.folder || "") : ""
                            onEditingFinished: if (opInsp.o) panel.patchOp(opInsp.o.id, { folder: text })
                        }
                        Label { text: "Tool"; color: Theme.textDim }
                        ComboBox {
                            Layout.fillWidth: true
                            model: panel.tools.map(t => t.name)
                            currentIndex: {
                                if (!opInsp.o) return 0
                                const i = panel.tools.findIndex(t => t.id === opInsp.o.toolId)
                                return i >= 0 ? i : 0
                            }
                            onActivated: {
                                if (opInsp.o && index >= 0 && index < panel.tools.length)
                                    panel.patchOp(opInsp.o.id, { toolId: panel.tools[index].id })
                            }
                        }
                        Label { text: "Feed"; color: Theme.textDim }
                        TextField {
                            Layout.fillWidth: true
                            text: opInsp.o ? String(opInsp.o.feed || 500) : "500"
                            onEditingFinished: {
                                const v = parseFloat(text)
                                if (!isNaN(v) && opInsp.o) panel.patchOp(opInsp.o.id, { feed: v })
                            }
                        }
                        Label { text: "Speed"; color: Theme.textDim }
                        TextField {
                            Layout.fillWidth: true
                            text: opInsp.o ? String(opInsp.o.speed || 8000) : "8000"
                            onEditingFinished: {
                                const v = parseFloat(text)
                                if (!isNaN(v) && opInsp.o) panel.patchOp(opInsp.o.id, { speed: v })
                            }
                        }
                        Label { text: "Stepdown"; color: Theme.textDim }
                        TextField {
                            Layout.fillWidth: true
                            text: opInsp.o ? String(opInsp.o.stepdown || 1) : "1"
                            onEditingFinished: {
                                const v = parseFloat(text)
                                if (!isNaN(v) && opInsp.o) panel.patchOp(opInsp.o.id, { stepdown: v })
                            }
                        }
                    }
                }
            }

            // ── Finish ─────────────────────────────────────────────
            ColumnLayout {
                spacing: 8
                Label {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    color: Theme.textDim
                    text: "Simulate estimates path length from stock + op type. Post Process writes op-aware NC stubs (face zigzag, pocket insets, drill points, turning XZ, probe touches)."
                }
                Button {
                    text: "Simulate"
                    Layout.fillWidth: true
                    onClicked: panel.simulate()
                }
                ScrollView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    visible: panel.simulateText.length > 0
                    TextArea {
                        readOnly: true
                        wrapMode: Text.Wrap
                        text: panel.simulateText
                        color: Theme.text
                        background: Rectangle { color: Theme.panelAlt; radius: 3 }
                    }
                }
                Button {
                    text: "Post Process…"
                    highlighted: true
                    Layout.fillWidth: true
                    onClicked: postDlg.open()
                }
                Button {
                    text: "Send to Slicer"
                    Layout.fillWidth: true
                    onClicked: app.sendToOrca()
                }
            }
        }
    }
}
