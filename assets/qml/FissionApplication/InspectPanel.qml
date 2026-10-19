// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "."

// INSPECT panel: Measure, Interference, Section Analysis and the surface analyses (zebra, draft, curvature)
Item {
    id: ip
    property var app
    readonly property string mode: app ? app.inspectMode : ""
    implicitHeight: col.implicitHeight

    readonly property var titles: ({ measure: "MEASURE", interference: "INTERFERENCE", section: "SECTION ANALYSIS",
                                     zebra: "ZEBRA ANALYSIS", draft: "DRAFT ANALYSIS", curvature: "CURVATURE MAP" })
    readonly property var glyphs: ({ measure: "measure", interference: "interference", section: "section",
                                     zebra: "zebra", draft: "draftanalysis", curvature: "curvature" })

    readonly property var measured: (app.rev, app.lengthUnit, app.selection.count, app.selection.items,
                                     mode === "measure" && app.doc)
                                    ? app.doc.measure(app.selection.items) : ({})
    property var clashes: null

    function runInterference() {
        const b = app.bodyIndices(app.selection.items.filter(x => x.kind === "body" || x.kind === "face"))
        clashes = app.doc.interference(b)
        fissionApp.showMessage(clashes.length ? clashes.length + " interference(s) found" : "No interference")
    }
    onModeChanged: clashes = null

    // Section offset limits along the plane normal (from the plane), from the scene bounds
    readonly property var range: {
        app.rev
        const f = app.sectionFrame
        const b = app.viewport.renderer.bounds()
        if (!f.ok || !b.length) return [-100, 100]
        let lo = Infinity, hi = -Infinity
        for (let k = 0; k < 8; ++k) {
            const p = [b[(k & 1) ? 3 : 0], b[(k & 2) ? 4 : 1], b[(k & 4) ? 5 : 2]]
            const d = (p[0] - f.origin[0]) * f.normal[0] + (p[1] - f.origin[1]) * f.normal[1] + (p[2] - f.origin[2]) * f.normal[2]
            lo = Math.min(lo, d); hi = Math.max(hi, d)
        }
        return [lo - 1, hi + 1]
    }
    readonly property string planeName: {
        const r = app.sectionRef
        if (typeof r === "string") return r + " plane"
        if (!r) return ""
        if (r.kind === "originplane") return r.id + " plane"
        if (r.kind === "face") return "Face of " + ((app.doc.body(r.body) || {}).name || "body")
        const f = app.doc.featureById(r.id)
        return f ? f.name : "Plane"
    }

    ColumnLayout {
        id: col
        width: parent.width
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 38
            color: Theme.header
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                Glyph {
                    name: ip.glyphs[ip.mode] || "measure"
                    color: Theme.headerText
                    Layout.preferredWidth: 20
                    Layout.preferredHeight: 20
                }
                Text {
                    Layout.fillWidth: true
                    text: ip.titles[ip.mode] || ""
                    color: Theme.headerText
                    font.bold: true
                    font.pixelSize: 13
                }
                ToolbarButton {
                    compact: true
                    glyph: "close"
                    glyphColor: Theme.headerText
                    tip: "Close (Esc)"
                    Layout.preferredWidth: 22
                    Layout.preferredHeight: 22
                    onClicked: ip.app.closeInspect()
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.margins: 12
            spacing: 8

            // ---------------------------------------------------------------- measure
            Text {
                visible: ip.mode === "measure"
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: Theme.textDim
                text: !ip.measured.ok ? (ip.measured.error || "Pick a face, edge, body or point.")
                    : (app.selection.count < 2 ? "Pick a second face or edge for distance and angle."
                                               : "Distance and angle between the two selections.")
            }
            GridLayout {
                visible: ip.mode === "measure" && !!ip.measured.ok
                Layout.fillWidth: true
                columns: 2
                columnSpacing: 10
                rowSpacing: 3
                Repeater {
                    model: ip.measured.lines || []
                    delegate: Text {
                        required property var modelData
                        required property int index
                        Layout.row: index
                        Layout.column: 0
                        Layout.columnSpan: modelData[1] === "" ? 2 : 1
                        Layout.topMargin: modelData[1] === "" && index > 0 ? 6 : 0
                        text: modelData[0]
                        font.bold: modelData[1] === ""
                        color: modelData[1] === "" ? Theme.text : Theme.textDim
                    }
                }
                Repeater {
                    model: ip.measured.lines || []
                    delegate: Text {
                        required property var modelData
                        required property int index
                        visible: modelData[1] !== ""
                        Layout.row: index
                        Layout.column: 1
                        Layout.fillWidth: true
                        text: modelData[1]
                        wrapMode: Text.WrapAnywhere
                    }
                }
            }

            // ---------------------------------------------------------------- interference
            Text {
                visible: ip.mode === "interference"
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: Theme.textDim
                text: "Checks the selected bodies (none selected = all solid bodies) for overlapping volume."
            }
            Button {
                visible: ip.mode === "interference"
                text: "Compute"
                highlighted: true
                onClicked: ip.runInterference()
            }
            Repeater {
                model: ip.mode === "interference" && ip.clashes ? ip.clashes : []
                delegate: RowLayout {
                    required property var modelData
                    Layout.fillWidth: true
                    Text { text: modelData.names; Layout.fillWidth: true; elide: Text.ElideRight }
                    Text { text: ip.app.doc.formatLength(modelData.volume, 3, 3); color: Theme.error }
                }
            }
            Text {
                visible: ip.mode === "interference" && !!ip.clashes && ip.clashes.length === 0
                text: "No interference"
                color: Theme.textDim
            }

            // ---------------------------------------------------------------- section
            RowLayout {
                visible: ip.mode === "section"
                spacing: 4
                Text { text: "Plane"; color: Theme.textDim; Layout.preferredWidth: 60 }
                Repeater {
                    model: ["XY", "XZ", "YZ"]
                    delegate: Button {
                        required property string modelData
                        text: modelData
                        checkable: true
                        checked: !ip.app.sectionAwaitingPlane && ip.app.sectionRef === modelData
                        implicitWidth: 44
                        onClicked: ip.app.setSectionRef(modelData)
                    }
                }
            }
            Text {
                visible: ip.mode === "section"
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: Theme.textDim
                text: ip.app.sectionAwaitingPlane ? "Click an origin plane, a construction plane or a planar face to cut with."
                                                  : "Cut: " + ip.planeName + ". Click a planar face or a plane in the canvas to cut there instead."
            }
            RowLayout {
                visible: ip.mode === "section"
                Layout.fillWidth: true
                Text { text: "Offset"; color: Theme.textDim; Layout.preferredWidth: 60 }
                Slider {
                    Layout.fillWidth: true
                    from: ip.range[0]
                    to: ip.range[1]
                    value: ip.app.sectionOffset
                    onMoved: ip.app.sectionOffset = value
                }
                Text { text: ip.app.lenText(ip.app.sectionOffset, 1); Layout.preferredWidth: 60 }
            }
            CheckBox {
                visible: ip.mode === "section"
                text: "Flip side"
                checked: ip.app.sectionFlip
                onToggled: ip.app.sectionFlip = checked
            }
            CheckBox {
                visible: ip.mode === "section"
                text: "Show cut"
                enabled: !ip.app.sectionAwaitingPlane
                checked: ip.app.sectionEnabled
                onToggled: ip.app.sectionEnabled = checked
            }
            Text {
                visible: ip.mode === "section"
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: Theme.textDim
                text: ip.app.sectionId ? "OK updates " + (ip.app.doc.analysis(ip.app.sectionId).name || "the section") + "."
                                       : "OK saves this section in the browser's Analysis folder."
            }

            // ---------------------------------------------------------------- surface analyses
            RowLayout {
                visible: ip.mode === "draft"
                spacing: 4
                Text { text: "Pull"; color: Theme.textDim; Layout.preferredWidth: 60 }
                Repeater {
                    model: ["X", "Y", "Z"]
                    delegate: Button {
                        required property string modelData
                        text: modelData
                        checkable: true
                        checked: ip.app.pullAxis === modelData
                        implicitWidth: 40
                        onClicked: ip.app.pullAxis = modelData
                    }
                }
            }
            RowLayout {
                visible: ip.mode === "draft"
                Layout.fillWidth: true
                Text { text: "Min. draft"; color: Theme.textDim; Layout.preferredWidth: 60 }
                Slider {
                    Layout.fillWidth: true
                    from: 0
                    to: 15
                    stepSize: 0.5
                    value: ip.app.draftAngle
                    onMoved: ip.app.draftAngle = value
                }
                Text { text: ip.app.draftAngle.toFixed(1) + "\u00b0"; Layout.preferredWidth: 40 }
            }
            Text {
                visible: ip.mode === "zebra" || ip.mode === "draft" || ip.mode === "curvature"
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: Theme.textDim
                text: ip.mode === "zebra" ? "Stripes that run on smoothly across an edge mean the faces are tangent or curvature continuous."
                    : ip.mode === "draft" ? "Green: draft at least the minimum along the pull. Red: the same on the opposite side (undercut). Yellow: less than the minimum."
                    : "Blue = flat, green to red = tighter curvature (relative to the model size)."
            }
            CheckBox {
                visible: ip.mode === "zebra" || ip.mode === "draft" || ip.mode === "curvature"
                text: "Show analysis"
                checked: ip.app.analysis === ip.mode
                onToggled: ip.app.analysis = checked ? ip.mode : ""
            }
        }

        Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: Theme.border }
        RowLayout {
            Layout.fillWidth: true
            Layout.margins: 10
            Item { Layout.fillWidth: true }
            Button {
                visible: ip.mode === "section"
                text: "OK"
                highlighted: true
                onClicked: ip.app.commitSection()
            }
            Button {
                text: ip.mode === "section" ? "Cancel" : "Close"
                onClicked: ip.app.closeInspect()
            }
        }
    }
}
