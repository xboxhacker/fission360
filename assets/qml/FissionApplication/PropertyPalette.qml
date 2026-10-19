// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "."

// Inspect > Properties: physical properties of the selected (or first) body
Rectangle {
    id: pal
    property var app
    readonly property int bodyIndex: {
        const items = app.selection.items
        for (const it of items) if (it.body !== undefined) return it.body
        return app.doc && app.doc.bodies.length ? 0 : -1
    }
    readonly property var props: (app.rev, app.doc && bodyIndex >= 0) ? app.doc.bodyProperties(bodyIndex) : ({})
    width: 250
    height: col.implicitHeight + 16
    radius: 5
    color: "#f2fafbfc"
    border.color: Theme.border

    function fmt(v, digits) { return v === undefined ? "-" : Number(v).toFixed(digits) }

    ColumnLayout {
        id: col
        anchors.fill: parent
        anchors.margins: 8
        spacing: 4

        RowLayout {
            Layout.fillWidth: true
            Text { text: "PROPERTIES"; font.bold: true; color: Theme.textDim; font.pixelSize: 11; Layout.fillWidth: true }
            Text {
                text: "\u2715"
                color: Theme.textDim
                MouseArea { anchors.fill: parent; onClicked: pal.app.showProperties = false }
            }
        }
        Text {
            visible: pal.bodyIndex < 0
            text: "No bodies"
            color: Theme.textDim
        }
        GridLayout {
            visible: pal.bodyIndex >= 0
            columns: 2
            columnSpacing: 10
            rowSpacing: 3
            Layout.fillWidth: true

            Text { text: "Body"; color: Theme.textDim }
            Text { text: pal.props.name || "" ; font.bold: true }
            Text { text: "Volume"; color: Theme.textDim }
            Text { text: pal.props.volume === undefined ? "-" : (pal.app.lengthUnit, pal.app.doc.formatLength(pal.props.volume, 3, 3)) }
            Text { text: "Area"; color: Theme.textDim }
            Text { text: pal.props.area === undefined ? "-" : (pal.app.lengthUnit, pal.app.doc.formatLength(pal.props.area, 3, 2)) }
            Text { text: "Density"; color: Theme.textDim }
            Text { text: pal.fmt(pal.props.density, 3) + " g/cm\u00b3" }
            Text { text: "Mass"; color: Theme.textDim }
            Text { text: pal.props.mass === undefined ? "-" : (pal.app.doc.massUnit, pal.app.doc.formatMass(pal.props.mass, 3)) }
            Text { text: "Center of mass"; color: Theme.textDim }
            Text { text: pal.props.centroid ? pal.props.centroid.map(v => pal.app.lenValue(v)).join(", ") + " " + pal.app.lengthUnit : "-" }
            Text { text: "Bounding box"; color: Theme.textDim }
            Text { text: pal.props.size ? pal.props.size.map(v => pal.app.lenValue(v)).join(" \u00d7 ") + " " + pal.app.lengthUnit : "-" }
            Text { text: "Faces / edges"; color: Theme.textDim; visible: !pal.props.mesh }
            Text { text: (pal.props.faces || 0) + " / " + (pal.props.edges || 0); visible: !pal.props.mesh }
        }
        // Mesh statistics
        GridLayout {
            readonly property var st: pal.props.meshStats || ({})
            visible: !!pal.props.mesh
            columns: 2
            columnSpacing: 10
            rowSpacing: 3
            Layout.fillWidth: true
            Text { text: "MESH"; font.bold: true; color: Theme.textDim; font.pixelSize: 11; Layout.columnSpan: 2; Layout.topMargin: 4 }
            Text { text: "Triangles"; color: Theme.textDim }
            Text { text: String(parent.st.triangles || 0) }
            Text { text: "Vertices"; color: Theme.textDim }
            Text { text: String(parent.st.nodes || 0) }
            Text { text: "Face groups"; color: Theme.textDim }
            Text { text: String(parent.st.groups || 0) }
            Text { text: "Pieces"; color: Theme.textDim }
            Text { text: String(parent.st.pieces || 0) }
            Text { text: "Closed"; color: Theme.textDim }
            Text {
                text: parent.st.closed ? "Yes (watertight)" : "No - " + (parent.st.openEdges || 0) + " open edges"
                color: parent.st.closed ? Theme.text : "#c0392b"
            }
            Text { text: "Manifold"; color: Theme.textDim }
            Text {
                text: parent.st.nonManifoldEdges ? "No - " + parent.st.nonManifoldEdges + " edges" : "Yes"
                color: parent.st.nonManifoldEdges ? "#c0392b" : Theme.text
            }
            Text { text: "Normals"; color: Theme.textDim }
            Text {
                text: parent.st.oriented ? "Consistent" : "Mixed - run Repair"
                color: parent.st.oriented ? Theme.text : "#c0392b"
            }
            Text { text: "Edge length"; color: Theme.textDim }
            Text {
                text: parent.st.triangles ? pal.app.lenValue(parent.st.minEdge) + " - " + pal.app.lenValue(parent.st.maxEdge) + " " + pal.app.lengthUnit : "-"
            }
        }
    }
}
