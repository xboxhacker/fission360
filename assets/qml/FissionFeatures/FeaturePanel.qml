// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../FissionApplication"

// Command panel hosting the dialog for the feature being created or edited
Rectangle {
    id: panel
    property var app
    property var feature
    color: Theme.panel
    implicitHeight: 38 + (loader.item ? loader.item.implicitHeight : 0) + 24
                    + (errorBanner.visible ? errorBanner.Layout.preferredHeight + 20 : 0)
                    + 1 + footer.implicitHeight + 20

    function dialogFor(type) {
        switch (type) {
        case "Extrude": return "ExtrudeDialog.qml"
        case "Revolve": return "RevolveDialog.qml"
        case "Fillet": return "FilletDialog.qml"
        case "Chamfer": return "ChamferDialog.qml"
        case "Shell": return "ShellDialog.qml"
        case "Loft": return "LoftDialog.qml"
        case "Sweep": return "SweepDialog.qml"
        case "Mirror": return "MirrorDialog.qml"
        case "RectangularPattern": case "CircularPattern": return "PatternDialog.qml"
        case "Hole": case "Thread": case "Combine": case "Move": case "Scale": case "SplitBody": case "SplitFace":
        case "OffsetFace": case "Offset": case "ReplaceFace": case "Draft": case "PathPattern": case "Rib": case "Web": case "Joint":
        case "InsertMesh": case "MeshConvert": case "MeshReverse": case "MeshSmooth": case "MeshReduce": case "MeshPlaneCut":
        case "MeshRepair": case "MeshShell": case "MeshCombine": case "MeshFaceGroups": case "MeshRemesh":
        case "Remove": case "ImportCAD": case "BaseFlange": case "EdgeFlange": case "FlatPattern":
        case "Hem": case "Tab": case "Bend": case "ContourFlange": case "Unfold": case "Refold":
            return "SpecDialog.qml"
        default: return "ConstructionDialog.qml"
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
                anchors.rightMargin: 10
                Glyph {
                    name: panel.feature ? Theme.featureGlyph(panel.feature.type) : "box"
                    color: Theme.headerText
                    Layout.preferredWidth: 20
                    Layout.preferredHeight: 20
                }
                TextInput {
                    Layout.fillWidth: true
                    text: panel.feature ? panel.feature.name : ""
                    color: Theme.headerText
                    font.bold: true
                    font.pixelSize: 13
                    selectByMouse: true
                    onEditingFinished: if (panel.feature && text.length) panel.feature.name = text
                }
                Text {
                    text: panel.app.commandIsNew ? "NEW" : "EDIT"
                    color: "#9fd4f0"
                    font.pixelSize: 10
                    font.bold: true
                }
                ToolbarButton {
                    compact: true
                    glyph: "close"
                    glyphColor: Theme.headerText
                    tip: "Cancel (Esc)"
                    Layout.preferredWidth: 22
                    Layout.preferredHeight: 22
                    onClicked: panel.app.finishCommand(false)
                }
            }
        }

        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentWidth: availableWidth

            Loader {
                id: loader
                width: panel.width - 24
                x: 12
                y: 12
                source: panel.feature ? panel.dialogFor(panel.feature.type) : ""
                onLoaded: {
                    item.app = Qt.binding(() => panel.app)
                    item.feature = Qt.binding(() => panel.feature)
                }
            }
        }

        // Error / status banner
        Rectangle {
            id: errorBanner
            Layout.fillWidth: true
            Layout.margins: 10
            Layout.preferredHeight: errorText.implicitHeight + 14
            visible: panel.feature && !panel.feature.healthy
            radius: 3
            color: "#fdecea"
            border.color: "#f3b4ae"
            Text {
                id: errorText
                anchors.fill: parent
                anchors.margins: 7
                text: panel.feature ? "\u26a0  " + panel.feature.error : ""
                color: Theme.error
                wrapMode: Text.WordWrap
            }
        }

        Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: Theme.border }
        RowLayout {
            id: footer
            Layout.fillWidth: true
            Layout.margins: 10
            spacing: 8
            Item { Layout.fillWidth: true }
            Button {
                text: "OK"
                highlighted: true
                enabled: !!panel.feature
                onClicked: panel.app.finishCommand(true)
            }
            Button {
                text: "Cancel"
                onClicked: panel.app.finishCommand(false)
            }
        }
    }
}
