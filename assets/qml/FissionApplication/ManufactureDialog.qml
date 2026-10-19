// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import "."

// Manufacture workspace panel: setups, tool library, operations, post / simulate.
// Light scaffolding — stores ops on the design; G-code is a placeholder contour around stock.
Item {
    id: panel
    property var app
    property int tabIndex: 0
    implicitHeight: 480

    readonly property var mfg: (app && app.doc) ? app.doc.manufacture : ({})
    readonly property var setups: mfg.setups || []
    readonly property var tools: mfg.tools || []
    readonly property var operations: mfg.operations || []
    readonly property string activeSetupId: mfg.activeSetupId || ""

    function close() { if (app) app.showManufacture = false }

    function activeSetup() {
        for (const s of setups) if (s.id === activeSetupId) return s
        return null
    }

    function opsForActive() {
        return operations.filter(o => !activeSetupId || o.setupId === activeSetupId)
    }

    function newSetup() {
        if (!app || !app.doc) return
        const id = app.doc.addMfgItem("setups", {})
        if (id) {
            tabIndex = 0
            fissionApp.showMessage("Setup created — stock from body bounds + offset")
        } else fissionApp.showMessage(app.doc.lastError || "Setup failed")
    }

    function addTool() {
        if (!app || !app.doc) return
        const id = app.doc.addMfgItem("tools", { name: "New Tool", type: "endmill", diameter: 6, flutes: 4 })
        if (id) { tabIndex = 1; fissionApp.showMessage("Tool added") }
    }

    function addOp(type, name) {
        if (!app || !app.doc) return
        if (!activeSetupId) {
            fissionApp.showMessage("Create a New Setup first")
            tabIndex = 0
            return
        }
        const toolId = tools.length ? tools[0].id : ""
        const id = app.doc.addMfgItem("operations", {
            type: type,
            name: name || type,
            setupId: activeSetupId,
            toolId: toolId
        })
        if (id) {
            tabIndex = 2
            fissionApp.showMessage(name + " added to " + (activeSetup() ? activeSetup().name : "setup"))
        } else fissionApp.showMessage(app.doc.lastError || "Operation failed")
    }

    function simulate() {
        const ops = opsForActive()
        if (!activeSetupId) { fissionApp.showMessage("No active setup to simulate"); return }
        if (!ops.length) { fissionApp.showMessage("No operations on the active setup"); return }
        let len = 0
        const stock = (activeSetup() || {}).stock || []
        if (stock.length >= 6)
            len = 2 * ((stock[3] - stock[0]) + (stock[4] - stock[1]))
        fissionApp.showMessage("Simulate: " + ops.length + " op(s), ~" + Math.round(len) + " mm stock contour (placeholder)")
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
            text: "Setups, tools and operations are saved with the design. Toolpaths are scaffolding — Post writes a stock-contour NC stub. Use Send to Slicer for additive."
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
            Layout.margins: 10
            currentIndex: panel.tabIndex

            ColumnLayout {
                spacing: 6
                Button { text: "New Setup"; highlighted: true; onClicked: panel.newSetup() }
                Label {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    color: Theme.textDim
                    text: "Stock box = body bounds + offset. Active setup drives Ops / Post."
                }
                ListView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: panel.setups
                    spacing: 2
                    delegate: Rectangle {
                        width: ListView.view.width
                        height: 36
                        radius: 3
                        color: modelData.id === panel.activeSetupId ? Theme.accentLight : Theme.panelAlt
                        border.color: Theme.border
                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 6
                            Text {
                                text: modelData.name + (modelData.stock ? "  (stock)" : "")
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
                                onClicked: app.doc.removeMfgItem("setups", modelData.id)
                            }
                        }
                        MouseArea {
                            anchors.fill: parent
                            z: -1
                            onClicked: app.doc.setActiveMfgSetup(modelData.id)
                        }
                    }
                }
            }

            ColumnLayout {
                spacing: 6
                Button { text: "Add Tool"; onClicked: panel.addTool() }
                ListView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: panel.tools
                    spacing: 2
                    delegate: Rectangle {
                        width: ListView.view.width
                        height: 40
                        radius: 3
                        color: Theme.panelAlt
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
                                onClicked: app.doc.removeMfgItem("tools", modelData.id)
                            }
                        }
                    }
                }
            }

            ColumnLayout {
                spacing: 6
                Label {
                    text: activeSetup() ? ("Ops on " + activeSetup().name) : "No active setup"
                    color: Theme.text
                    font.bold: true
                }
                Flow {
                    Layout.fillWidth: true
                    spacing: 4
                    Button { text: "2D Adaptive"; onClicked: panel.addOp("2d-adaptive", "2D Adaptive") }
                    Button { text: "2D Pocket"; onClicked: panel.addOp("2d-pocket", "2D Pocket") }
                    Button { text: "2D Contour"; onClicked: panel.addOp("2d-contour", "2D Contour") }
                    Button { text: "Face"; onClicked: panel.addOp("face", "Face") }
                    Button { text: "Drill"; onClicked: panel.addOp("drill", "Drill") }
                    Button { text: "3D Adaptive"; onClicked: panel.addOp("3d-adaptive", "3D Adaptive") }
                    Button { text: "Parallel"; onClicked: panel.addOp("parallel", "Parallel") }
                    Button { text: "Scallop"; onClicked: panel.addOp("scallop", "Scallop") }
                    Button { text: "Facing"; onClicked: panel.addOp("turning-face", "Facing") }
                    Button { text: "Profiling"; onClicked: panel.addOp("turning-profile", "Profiling") }
                    Button { text: "Grooving"; onClicked: panel.addOp("turning-groove", "Grooving") }
                    Button { text: "Parting"; onClicked: panel.addOp("turning-part", "Parting") }
                }
                ListView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: panel.opsForActive()
                    spacing: 2
                    delegate: Rectangle {
                        width: ListView.view.width
                        height: 36
                        radius: 3
                        color: Theme.panelAlt
                        border.color: Theme.border
                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 6
                            Text {
                                text: modelData.name + "  (" + modelData.type + ")"
                                color: Theme.text
                                Layout.fillWidth: true
                            }
                            Button {
                                text: "✕"
                                flat: true
                                onClicked: app.doc.removeMfgItem("operations", modelData.id)
                            }
                        }
                    }
                }
            }

            ColumnLayout {
                spacing: 8
                Label {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    color: Theme.textDim
                    text: "Simulate reports op counts and stock contour length. Post Process writes a placeholder G-code box path around stock."
                }
                Button {
                    text: "Simulate"
                    Layout.fillWidth: true
                    onClicked: panel.simulate()
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
                Item { Layout.fillHeight: true }
            }
        }
    }
}
