// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "."

// SHEET METAL > Sheet Metal Rules: the active design's rule (thickness, inner bend radius, K-factor, kept as the
// parameters SheetThickness, BendRadius and KFactor) and a library of named rules shared by every design.
Popup {
    id: dlg
    property var app
    readonly property var doc: app ? app.doc : null
    property var library: []
    property int picked: -1
    modal: true
    focus: true
    anchors.centerIn: Overlay.overlay
    width: 520
    height: 420
    padding: 0
    closePolicy: Popup.CloseOnEscape
    background: Rectangle { color: Theme.panel; border.color: Theme.border; radius: 4 }

    readonly property var builtIn: [
        { name: "Steel 1 mm", thickness: "1 mm", radius: "1 mm", k: "0.44" },
        { name: "Steel 2 mm", thickness: "2 mm", radius: "2 mm", k: "0.44" },
        { name: "Aluminium 1.5 mm", thickness: "1.5 mm", radius: "1.5 mm", k: "0.42" },
        { name: "Stainless 0.8 mm", thickness: "0.8 mm", radius: "0.8 mm", k: "0.45" },
        { name: "Copper 0.5 mm", thickness: "0.5 mm", radius: "0.5 mm", k: "0.40" }
    ]

    onOpened: {
        if (doc) doc.ensureSheetMetalRule()
        loadLibrary()
        refreshFields()
    }

    function loadLibrary() {
        let saved = []
        try { saved = JSON.parse(fissionApp.setting("sheetMetal/library", "[]")) } catch (e) { saved = [] }
        library = builtIn.map(r => Object.assign({ builtIn: true }, r)).concat(saved)
        picked = -1
    }
    function saveLibrary() {
        fissionApp.setSetting("sheetMetal/library", JSON.stringify(library.filter(r => !r.builtIn)))
    }
    function expr(name) {
        const p = doc ? doc.parameter(name) : null
        return p ? p.expression : ""
    }
    function refreshFields() {
        thicknessField.text = expr("SheetThickness")
        radiusField.text = expr("BendRadius")
        kField.text = expr("KFactor")
    }
    function applyRule(r) {
        if (!doc) return
        doc.ensureSheetMetalRule()
        doc.checkpoint()
        doc.parameter("SheetThickness").expression = r.thickness
        doc.parameter("BendRadius").expression = r.radius
        doc.parameter("KFactor").expression = r.k
        refreshFields()
        fissionApp.showMessage("Sheet metal rule: " + (r.name || "custom"))
    }
    function current() {
        return { thickness: thicknessField.text.trim(), radius: radiusField.text.trim(), k: kField.text.trim() }
    }
    function saveAsNew(name) {
        const n = name.trim()
        if (!n) return
        const r = Object.assign({ name: n }, current())
        const i = library.findIndex(x => x.name === n && !x.builtIn)
        const lib = library.slice()
        if (i >= 0) lib[i] = r
        else lib.push(r)
        library = lib
        saveLibrary()
        picked = library.findIndex(x => x.name === n && !x.builtIn)
    }
    function removePicked() {
        if (picked < 0 || library[picked].builtIn) return
        const lib = library.slice()
        lib.splice(picked, 1)
        library = lib
        saveLibrary()
        picked = -1
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 34
            color: Theme.header
            radius: 4
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                Glyph { name: "rulelibrary"; color: Theme.headerText; Layout.preferredWidth: 18; Layout.preferredHeight: 18 }
                Text { text: "Sheet Metal Rules"; color: Theme.headerText; font.bold: true; font.pixelSize: 13; Layout.fillWidth: true }
                Text {
                    text: "\u2715"
                    color: Theme.headerText
                    MouseArea { anchors.fill: parent; anchors.margins: -6; onClicked: dlg.close() }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.margins: 10
            spacing: 10

            ColumnLayout {
                Layout.preferredWidth: 220
                Layout.fillHeight: true
                spacing: 4
                Text { text: "Rule library"; font.bold: true; color: Theme.text }
                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    color: Theme.panelAlt
                    border.color: Theme.border
                    ListView {
                        objectName: "ruleList"
                        anchors.fill: parent
                        anchors.margins: 1
                        clip: true
                        model: dlg.library
                        delegate: Rectangle {
                            required property var modelData
                            required property int index
                            width: ListView.view.width
                            height: 36
                            color: index === dlg.picked ? Theme.accentLight : area.containsMouse ? Theme.hover : "transparent"
                            Column {
                                anchors.verticalCenter: parent.verticalCenter
                                x: 6
                                width: parent.width - 12
                                Text { text: modelData.name + (modelData.builtIn ? "" : "  \u2605"); color: Theme.text; elide: Text.ElideRight; width: parent.width }
                                Text {
                                    text: "T " + modelData.thickness + "   R " + modelData.radius + "   K " + modelData.k
                                    color: Theme.textDim
                                    font.pixelSize: Theme.smallFont
                                }
                            }
                            MouseArea {
                                id: area
                                anchors.fill: parent
                                hoverEnabled: true
                                onClicked: dlg.picked = index
                                onDoubleClicked: { dlg.picked = index; dlg.applyRule(modelData) }
                            }
                        }
                    }
                }
                RowLayout {
                    spacing: 4
                    Button {
                        objectName: "applyRule"
                        text: "Apply"
                        highlighted: true
                        enabled: dlg.picked >= 0
                        onClicked: dlg.applyRule(dlg.library[dlg.picked])
                    }
                    Button {
                        text: "Delete"
                        enabled: dlg.picked >= 0 && !dlg.library[dlg.picked].builtIn
                        onClicked: dlg.removePicked()
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 6
                Text { text: "This design"; font.bold: true; color: Theme.text }
                GridLayout {
                    columns: 2
                    columnSpacing: 8
                    rowSpacing: 6
                    Text { text: "Thickness"; color: Theme.text }
                    TextField { id: thicknessField; objectName: "ruleThickness"; Layout.fillWidth: true }
                    Text { text: "Bend radius"; color: Theme.text }
                    TextField { id: radiusField; Layout.fillWidth: true }
                    Text { text: "K-factor"; color: Theme.text }
                    TextField { id: kField; Layout.fillWidth: true }
                }
                RowLayout {
                    Button {
                        text: "Apply to design"
                        onClicked: dlg.applyRule(Object.assign({ name: "custom" }, dlg.current()))
                    }
                    Button { text: "Parameters\u2026"; onClicked: { dlg.close(); dlg.app.openParameters() } }
                }
                Item { Layout.preferredHeight: 6 }
                Text { text: "Save as a new rule"; font.bold: true; color: Theme.text }
                RowLayout {
                    TextField { id: newName; Layout.fillWidth: true; placeholderText: "Rule name" }
                    Button { text: "Save"; enabled: newName.text.trim().length > 0; onClicked: { dlg.saveAsNew(newName.text); newName.text = "" } }
                }
                Item { Layout.fillHeight: true }
                Text {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    font.pixelSize: Theme.smallFont
                    color: Theme.textDim
                    text: "Sheet metal features use these values by default: thickness for Flange, Tab and Contour Flange, the "
                          + "inner radius for every bend, and the K-factor (neutral axis position) for flat patterns."
                }
            }
        }
    }
}
