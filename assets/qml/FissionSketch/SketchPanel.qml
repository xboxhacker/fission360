// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../FissionApplication"

// Sketch palette: plane picking, tool options, the entity list and an editor for entity dimensions
Rectangle {
    id: panel
    property var app
    readonly property var sketch: app.activeSketch
    readonly property var entities: (app.rev, sketch ? (sketch.props.entities || []) : [])
    readonly property var selectedEntity: {
        const curves = app.selection.items.filter(i => i.kind === "sketchcurve" && i.sketch === app.activeSketchId)
        return curves.length === 1 ? curves[0].entity : -1
    }
    color: Theme.panel
    implicitHeight: 510 + (constraints.length ? 24 + Math.min(constraints.length, 6) * 24 : 0)
                    + (toolOption.visible ? 30 : 0)

    readonly property var fieldOrder: ({
        line: ["x1", "y1", "x2", "y2"],
        circle: ["cx", "cy", "r"],
        arc: ["cx", "cy", "r", "a1", "a2"],
        arc3: ["x1", "y1", "xm", "ym", "x2", "y2"],
        rectangle: ["x", "y", "w", "h"],
        polygon: ["cx", "cy", "r", "sides"],
        slot: ["x1", "y1", "x2", "y2", "w"],
        ellipse: ["cx", "cy", "rx", "ry"],
        point: ["x", "y"]
    })
    readonly property var fieldNames: ({
        x1: "Start X", y1: "Start Y", x2: "End X", y2: "End Y", xm: "Mid X", ym: "Mid Y",
        cx: "Center X", cy: "Center Y", r: "Radius", a1: "Start angle", a2: "End angle",
        x: "X", y: "Y", w: "Width", h: "Height", sides: "Sides", rx: "Radius X", ry: "Radius Y"
    })

    readonly property var constraints: (app.rev, sketch ? (sketch.props.constraints || []) : [])
    function constraintLabel(c) {
        const names = {
            distance: "Distance", pointline: "Distance", radius: "Radius", diameter: "Diameter", angle: "Angle",
            coincident: "Coincident", horizontal: "Horizontal", vertical: "Vertical", parallel: "Parallel",
            perpendicular: "Perpendicular", collinear: "Collinear", tangent: "Tangent", equal: "Equal",
            concentric: "Concentric", midpoint: "Midpoint", symmetric: "Symmetry", fix: "Fix"
        }
        const refs = [c.a, c.b, c.c].filter(r => r !== undefined)
                                     .map(r => r.e < 0 ? "origin" : String(r.e + 1)).join(", ")
        return (names[c.type] || c.type) + (refs ? "  (" + refs + ")" : "")
    }

    function entityLabel(e, i) {
        const t = e.type || "?"
        return (i + 1) + ". " + t.charAt(0).toUpperCase() + t.slice(1) + (e.construction ? "  (construction)" : "")
               + (e.projected ? "  (projected)" : "")
    }

    function setEntityField(index, key, value) {
        const e = Object.assign({}, entities[index])
        if (String(e[key]) === value) return
        e[key] = key === "sides" ? parseInt(value) : value
        app.doc.checkpoint()
        sketch.setEntity(index, e)
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 38
            color: Theme.sketch
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                Glyph { name: "sketch"; color: "white"; Layout.preferredWidth: 20; Layout.preferredHeight: 20 }
                TextInput {
                    Layout.fillWidth: true
                    text: panel.sketch ? panel.sketch.name : ""
                    readOnly: !panel.sketch
                    color: "white"
                    font.bold: true
                    font.pixelSize: 13
                    selectByMouse: true
                    onEditingFinished: if (panel.sketch && text.length) panel.sketch.name = text
                }
                ToolbarButton {
                    compact: true
                    glyph: "close"
                    glyphColor: "white"
                    tip: "Finish Sketch"
                    Layout.preferredWidth: 22
                    Layout.preferredHeight: 22
                    onClicked: panel.app.finishSketch()
                }
            }
        }

        // ---------------- editing
        ColumnLayout {
            visible: !!panel.sketch
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.margins: 12
            spacing: 8

            Rectangle {
                visible: panel.sketch && !panel.sketch.healthy
                Layout.fillWidth: true
                Layout.preferredHeight: errText.implicitHeight + 12
                radius: 3
                color: "#fdecea"
                Text {
                    id: errText
                    anchors.fill: parent
                    anchors.margins: 6
                    wrapMode: Text.WordWrap
                    color: Theme.error
                    text: panel.sketch ? panel.sketch.error : ""
                }
            }

            Text {
                text: panel.sketch ? (panel.app.rev, panel.sketch.profileCount + " closed profile(s), "
                                       + panel.entities.length + " curve(s)") : ""
                color: Theme.textDim
            }
            Text {
                readonly property int dof: (panel.app.rev, panel.sketch ? panel.sketch.degreesOfFreedom() : 0)
                visible: panel.entities.length > 0
                text: dof === 0 ? "Fully constrained" : dof + " degree(s) of freedom left"
                color: dof === 0 ? Theme.text : Theme.textDim
                font.bold: dof === 0
            }

            RowLayout {
                visible: panel.app.sketchTool === "polygon"
                Text { text: "Polygon sides"; Layout.preferredWidth: 96 }
                SpinBox {
                    from: 3; to: 64
                    value: panel.app.polygonSides
                    onValueModified: panel.app.polygonSides = value
                }
            }

            // Fillet radius / offset distance for those tools (expressions allowed)
            RowLayout {
                id: toolOption
                readonly property bool fillet: panel.app.sketchTool === "fillet"
                visible: fillet || panel.app.sketchTool === "offset"
                Text { text: toolOption.fillet ? "Fillet radius" : "Offset distance"; Layout.preferredWidth: 96; color: Theme.text }
                TextField {
                    Layout.fillWidth: true
                    selectByMouse: true
                    text: toolOption.fillet ? panel.app.sketchFilletRadius : panel.app.sketchOffsetDistance
                    onEditingFinished: {
                        const t = panel.app.withUnit(text, "length")
                        if (toolOption.fillet) panel.app.sketchFilletRadius = t
                        else panel.app.sketchOffsetDistance = t
                    }
                }
            }

            CheckBox {
                text: "Construction (X)"
                checked: panel.app.sketchConstruction
                onToggled: panel.app.sketchConstruction = checked
            }
            CheckBox {
                text: "Snap to grid"
                checked: panel.app.sketchSnapToGrid
                onToggled: panel.app.setSketchSnapToGrid(checked)
            }

            Text { text: "CURVES"; font.bold: true; font.pixelSize: Theme.smallFont; color: Theme.textDim; Layout.topMargin: 4 }
            ListView {
                id: list
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumHeight: 120
                clip: true
                model: panel.entities.length
                delegate: Rectangle {
                    id: entRow
                    required property int index
                    readonly property var entity: panel.entities[index] || ({})
                    readonly property bool selected: (panel.app.selection.count,
                        panel.app.selection.contains({ kind: "sketchcurve", sketch: panel.app.activeSketchId, entity: index }))
                    width: list.width
                    height: 24
                    color: selected ? Theme.accentLight : rowArea.containsMouse ? Theme.hover : "transparent"
                    MouseArea {
                        id: rowArea
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: (m) => {
                            const ref = { kind: "sketchcurve", sketch: panel.app.activeSketchId, entity: entRow.index }
                            if (m.modifiers & (Qt.ControlModifier | Qt.ShiftModifier)) panel.app.selection.toggle(ref)
                            else panel.app.selection.select(ref, false)
                        }
                    }
                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 4
                        anchors.rightMargin: 4
                        Glyph {
                            name: entRow.entity.type === "arc3" ? "arc" : (entRow.entity.type || "line")
                            color: entRow.entity.construction ? Theme.construction : Theme.sketch
                            Layout.preferredWidth: 16
                            Layout.preferredHeight: 16
                        }
                        Text {
                            text: panel.entityLabel(entRow.entity, entRow.index)
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                            color: Theme.text
                        }
                        ToolbarButton {
                            compact: true
                            glyph: "construction"
                            tip: "Toggle construction"
                            Layout.preferredWidth: 22
                            Layout.preferredHeight: 22
                            onClicked: {
                                panel.app.doc.checkpoint()
                                panel.sketch.setConstruction(entRow.index, !entRow.entity.construction)
                            }
                        }
                        ToolbarButton {
                            compact: true
                            glyph: "delete"
                            tip: "Delete curve"
                            Layout.preferredWidth: 22
                            Layout.preferredHeight: 22
                            onClicked: {
                                panel.app.doc.checkpoint()
                                panel.app.selection.clear()
                                panel.sketch.removeEntity(entRow.index)
                            }
                        }
                    }
                }
            }

            Text {
                text: "CONSTRAINTS"
                visible: panel.constraints.length > 0
                font.bold: true
                font.pixelSize: Theme.smallFont
                color: Theme.textDim
            }
            ListView {
                id: conList
                visible: panel.constraints.length > 0
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(panel.constraints.length, 6) * 24
                clip: true
                model: panel.constraints.length
                delegate: Rectangle {
                    id: conRow
                    required property int index
                    readonly property var con: panel.constraints[index] || ({})
                    readonly property bool dim: ["distance", "pointline", "radius", "diameter", "angle"].indexOf(con.type) >= 0
                    readonly property bool selected: panel.app.viewport.selectedConstraint === index
                    width: conList.width
                    height: 24
                    color: selected ? Theme.accentLight : conArea.containsMouse ? Theme.hover : "transparent"
                    MouseArea {
                        id: conArea
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: panel.app.viewport.selectedConstraint = conRow.index
                    }
                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 4
                        anchors.rightMargin: 4
                        Glyph {
                            name: conRow.dim ? "dimension" : "constraint:" + conRow.con.type
                            color: Theme.text
                            Layout.preferredWidth: 16
                            Layout.preferredHeight: 16
                        }
                        Text {
                            text: panel.constraintLabel(conRow.con)
                            Layout.fillWidth: !conRow.dim
                            Layout.preferredWidth: conRow.dim ? 110 : -1
                            elide: Text.ElideRight
                            color: Theme.text
                        }
                        TextField {
                            visible: conRow.dim
                            Layout.fillWidth: true
                            Layout.preferredHeight: 22
                            topPadding: 0
                            bottomPadding: 0
                            text: String(conRow.con.value === undefined ? "" : conRow.con.value)
                            selectByMouse: true
                            onEditingFinished: {
                                if (text === String(conRow.con.value)) return
                                panel.app.doc.checkpoint()
                                const res = panel.sketch.setConstraintValue(conRow.index,
                                    panel.app.withUnit(text, conRow.con.type === "angle" ? "angle" : "length"))
                                if (!res.ok) { panel.app.doc.undo(); fissionApp.showMessage(res.error); text = String(conRow.con.value) }
                            }
                        }
                        ToolbarButton {
                            compact: true
                            glyph: "delete"
                            tip: "Delete constraint"
                            Layout.preferredWidth: 22
                            Layout.preferredHeight: 22
                            onClicked: {
                                panel.app.viewport.selectedConstraint = conRow.index
                                panel.app.viewport.deleteSelectedConstraint()
                            }
                        }
                    }
                }
            }

            // Dimensions of the selected curve (accept expressions such as "width / 2")
            ColumnLayout {
                visible: panel.selectedEntity >= 0 && !!panel.fieldOrder[(panel.entities[panel.selectedEntity] || {}).type]
                Layout.fillWidth: true
                spacing: 4
                Text { text: "DIMENSIONS"; font.bold: true; font.pixelSize: Theme.smallFont; color: Theme.textDim }
                Repeater {
                    model: panel.selectedEntity >= 0 ? (panel.fieldOrder[(panel.entities[panel.selectedEntity] || {}).type] || []) : []
                    RowLayout {
                        id: dimRow
                        required property string modelData
                        readonly property string value: String((panel.entities[panel.selectedEntity] || {})[modelData])
                        Layout.fillWidth: true
                        Text { text: panel.fieldNames[dimRow.modelData] || dimRow.modelData; Layout.preferredWidth: 80; color: Theme.text }
                        TextField {
                            Layout.fillWidth: true
                            text: dimRow.value
                            selectByMouse: true
                            onEditingFinished: panel.setEntityField(panel.selectedEntity, dimRow.modelData, text)
                        }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 6
                Button {
                    text: "Export DXF..."
                    Layout.fillWidth: true
                    onClicked: panel.app.exportDxf()
                }
                Button {
                    text: "Finish Sketch"
                    highlighted: true
                    Layout.fillWidth: true
                    onClicked: panel.app.finishSketch()
                }
            }
        }
    }
}
