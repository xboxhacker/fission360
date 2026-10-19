// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "."

// Modify > Change Parameters: user parameters and every feature dimension
Popup {
    id: dlg
    property var app
    readonly property var doc: app.doc
    modal: true
    focus: true
    anchors.centerIn: Overlay.overlay
    width: Math.min(860, app.width - 80)
    height: Math.min(640, app.height - 80)
    padding: 0
    closePolicy: Popup.CloseOnEscape

    background: Rectangle { color: Theme.panel; border.color: Theme.border; radius: 4 }

    function fmt(v) { return Number(v).toFixed(Math.abs(v) < 1000 ? 3 : 1).replace(/\.?0+$/, "") }
    function kindOf(unit) { return unit === "deg" ? "angle" : unit === "" ? "" : "length" }
    function fmtKind(v, kind) { return kind === "length" ? app.lenText(v, 3) : kind === "angle" ? fmt(v) + "\u00b0" : fmt(v) }

    component Header: Text {
        font.bold: true
        font.pixelSize: Theme.smallFont
        color: Theme.textDim
    }
    component ExprField: TextField {
        id: field
        property string expression
        property bool invalid: false
        signal committed(string text)
        text: expression
        onExpressionChanged: text = expression
        selectByMouse: true
        color: invalid ? Theme.error : Theme.text
        onEditingFinished: if (text !== expression) committed(text)
        background: Rectangle {
            color: "white"
            border.color: field.activeFocus ? Theme.accent : field.invalid ? Theme.error : Theme.border
            radius: 2
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 34
            color: Theme.header
            radius: 4
            Text { anchors.verticalCenter: parent.verticalCenter; x: 12; text: "Parameters"; color: Theme.headerText; font.bold: true; font.pixelSize: 13 }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                anchors.right: parent.right
                anchors.rightMargin: 12
                text: "\u2715"
                color: Theme.headerText
                MouseArea { anchors.fill: parent; anchors.margins: -6; onClicked: dlg.close() }
            }
        }

        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentWidth: availableWidth

            ColumnLayout {
                width: dlg.width - 24
                x: 12
                spacing: 6

                Item { Layout.preferredHeight: 6 }
                Text { text: "User Parameters"; font.bold: true; font.pixelSize: 13; color: Theme.text }

                RowLayout {
                    spacing: 8
                    Layout.fillWidth: true
                    Header { text: "NAME"; Layout.preferredWidth: 120 }
                    Header { text: "UNIT"; Layout.preferredWidth: 50 }
                    Header { text: "EXPRESSION"; Layout.fillWidth: true }
                    Header { text: "VALUE"; Layout.preferredWidth: 90 }
                    Header { text: "COMMENT"; Layout.preferredWidth: 180 }
                    Item { Layout.preferredWidth: 24 }
                }

                Repeater {
                    model: dlg.doc ? dlg.doc.parameters : []
                    RowLayout {
                        required property var modelData
                        spacing: 8
                        Layout.fillWidth: true
                        Text { text: modelData.name; font.bold: true; Layout.preferredWidth: 120; elide: Text.ElideRight }
                        Text { text: modelData.unit; color: Theme.textDim; Layout.preferredWidth: 50 }
                        ExprField {
                            Layout.fillWidth: true
                            expression: modelData.expression
                            invalid: modelData.error.length > 0
                            onCommitted: (t) => { dlg.doc.checkpoint(); modelData.expression = dlg.app.withUnit(t, dlg.kindOf(modelData.unit)) }
                        }
                        Text {
                            Layout.preferredWidth: 90
                            text: modelData.error.length ? modelData.error : dlg.fmtKind(modelData.value, dlg.kindOf(modelData.unit))
                            color: modelData.error.length ? Theme.error : Theme.text
                            elide: Text.ElideRight
                            ToolTip.visible: modelData.error.length > 0 && valueHover.containsMouse
                            ToolTip.text: modelData.error
                            MouseArea { id: valueHover; anchors.fill: parent; hoverEnabled: true }
                        }
                        ExprField {
                            Layout.preferredWidth: 180
                            expression: modelData.comment
                            onCommitted: (t) => modelData.comment = t
                        }
                        ToolbarButton {
                            compact: true
                            glyph: "delete"
                            tip: "Delete parameter"
                            Layout.preferredWidth: 24
                            Layout.preferredHeight: 24
                            onClicked: dlg.doc.removeParameter(modelData.name)
                        }
                    }
                }

                // Add row
                RowLayout {
                    spacing: 8
                    Layout.fillWidth: true
                    Layout.topMargin: 4
                    TextField { id: newName; placeholderText: "name"; Layout.preferredWidth: 120; selectByMouse: true }
                    ComboBox { id: newUnit; model: [dlg.app.lengthUnit, "deg", ""]; Layout.preferredWidth: 70 }
                    TextField {
                        id: newExpr
                        placeholderText: "expression, e.g. 25 " + dlg.app.lengthUnit + " or width / 2"
                        Layout.fillWidth: true
                        selectByMouse: true
                        onAccepted: addButton.clicked()
                    }
                    TextField { id: newComment; placeholderText: "comment"; Layout.preferredWidth: 180; selectByMouse: true }
                    Button {
                        id: addButton
                        text: "Add"
                        onClicked: {
                            const p = dlg.doc.addParameter(newName.text.trim(),
                                                           dlg.app.withUnit(newExpr.text.trim() || "0", dlg.kindOf(newUnit.currentText)),
                                                           newUnit.currentText, newComment.text)
                            if (!p) {
                                addError.text = "Use a unique name made of letters, digits and _ (not a unit or function)"
                                return
                            }
                            addError.text = ""
                            newName.text = ""; newExpr.text = ""; newComment.text = ""
                        }
                    }
                }
                Text { id: addError; color: Theme.error; visible: text.length > 0 }

                Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: Theme.border; Layout.topMargin: 8 }
                Text { text: "Model Parameters"; font.bold: true; font.pixelSize: 13; color: Theme.text; Layout.topMargin: 4 }
                Text {
                    text: "Feature dimensions accept expressions that reference user parameters."
                    color: Theme.textDim
                    font.pixelSize: Theme.smallFont
                }

                Repeater {
                    model: dlg.doc ? dlg.doc.timeline : null
                    ColumnLayout {
                        id: featureRows
                        required property var feature
                        required property string name
                        readonly property var keys: feature ? feature.expressionKeys() : []
                        visible: keys.length > 0
                        spacing: 2
                        Layout.fillWidth: true

                        Text { text: featureRows.name; font.bold: true; color: Theme.featureColor(featureRows.feature.type); Layout.topMargin: 4 }
                        Repeater {
                            model: featureRows.keys
                            RowLayout {
                                id: keyRow
                                required property string modelData
                                readonly property string expr: (dlg.app.rev, String(featureRows.feature.get(modelData)))
                                readonly property var result: (dlg.app.rev, dlg.doc.evaluateExpression(expr))
                                spacing: 8
                                Layout.fillWidth: true
                                Layout.leftMargin: 14
                                Text { text: keyRow.modelData; color: Theme.textDim; Layout.preferredWidth: 156 }
                                ExprField {
                                    Layout.fillWidth: true
                                    expression: keyRow.expr
                                    invalid: keyRow.result.error.length > 0
                                    onCommitted: (t) => {
                                        dlg.doc.checkpoint()
                                        featureRows.feature.set(keyRow.modelData, dlg.app.withUnit(t, dlg.app.inputKind(keyRow.modelData)))
                                    }
                                }
                                Text {
                                    Layout.preferredWidth: 90
                                    text: keyRow.result.error.length ? keyRow.result.error
                                                                     : dlg.fmtKind(keyRow.result.value, dlg.app.inputKind(keyRow.modelData))
                                    color: keyRow.result.error.length ? Theme.error : Theme.text
                                    elide: Text.ElideRight
                                }
                                Item { Layout.preferredWidth: 212 }
                            }
                        }
                    }
                }
                Item { Layout.preferredHeight: 12 }
            }
        }

        Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: Theme.border }
        RowLayout {
            Layout.fillWidth: true
            Layout.margins: 10
            Item { Layout.fillWidth: true }
            Button { text: "Close"; onClicked: dlg.close() }
        }
    }
}
