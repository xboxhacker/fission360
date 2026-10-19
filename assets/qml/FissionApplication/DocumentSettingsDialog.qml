// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "."

// Browser > Document Settings: units, default density and document properties. Every change is one undo step.
Popup {
    id: dlg
    property var app
    readonly property var doc: app.doc
    modal: true
    focus: true
    anchors.centerIn: Overlay.overlay
    width: 420
    height: col.implicitHeight + 50
    padding: 0
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    background: Rectangle { color: Theme.panel; border.color: Theme.border; radius: 4 }

    function setInfo(key, text) {
        if (!doc || (doc.info[key] || "") === text) return
        doc.checkpoint()
        doc.setInfo(key, text)
    }

    component Label_: Text {
        color: Theme.textDim
        Layout.preferredWidth: 110
    }
    component InfoField: TextField {
        id: field
        property string key
        Layout.fillWidth: true
        selectByMouse: true
        text: dlg.doc ? (dlg.doc.info[key] || "") : ""
        onEditingFinished: dlg.setInfo(key, text)
        background: Rectangle { color: "white"; radius: 2; border.color: field.activeFocus ? Theme.accent : Theme.border }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 34
            color: Theme.header
            radius: 4
            Text { anchors.verticalCenter: parent.verticalCenter; x: 12; text: "Document Settings"; color: Theme.headerText; font.bold: true; font.pixelSize: 13 }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                anchors.right: parent.right
                anchors.rightMargin: 12
                text: "\u2715"
                color: Theme.headerText
                MouseArea { anchors.fill: parent; anchors.margins: -6; onClicked: dlg.close() }
            }
        }

        GridLayout {
            id: col
            columns: 2
            columnSpacing: 10
            rowSpacing: 8
            Layout.fillWidth: true
            Layout.margins: 12

            Text { text: "UNITS"; font.bold: true; font.pixelSize: Theme.smallFont; color: Theme.textDim; Layout.columnSpan: 2 }
            Label_ { text: "Length" }
            ComboBox {
                id: lengthBox
                objectName: "lengthUnitBox"
                Layout.fillWidth: true
                model: dlg.doc ? dlg.doc.lengthUnits() : []
                currentIndex: dlg.doc ? model.indexOf(dlg.doc.lengthUnit) : 0
                onActivated: (i) => dlg.app.setLengthUnit(model[i])
            }
            Label_ { text: "Mass" }
            ComboBox {
                Layout.fillWidth: true
                model: dlg.doc ? dlg.doc.massUnits() : []
                currentIndex: dlg.doc ? model.indexOf(dlg.doc.massUnit) : 0
                onActivated: (i) => { dlg.doc.checkpoint(); dlg.doc.massUnit = model[i] }
            }
            Text {
                Layout.columnSpan: 2
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                font.pixelSize: Theme.smallFont
                color: Theme.textDim
                text: "Lengths are shown in this unit and a number typed without a unit uses it (\"10\" becomes \"10 "
                      + dlg.app.lengthUnit + "\"). Existing dimensions keep their own units, so the model does not change size."
            }
            Label_ { text: "Density (g/cm\u00b3)" }
            TextField {
                id: densityField
                Layout.fillWidth: true
                selectByMouse: true
                text: dlg.doc ? String(dlg.doc.density) : ""
                validator: DoubleValidator { bottom: 0.000001 }
                onEditingFinished: {
                    const v = parseFloat(text)
                    if (dlg.doc && v > 0 && v !== dlg.doc.density) { dlg.doc.checkpoint(); dlg.doc.density = v }
                }
                background: Rectangle { color: "white"; radius: 2; border.color: densityField.activeFocus ? Theme.accent : Theme.border }
            }

            Rectangle { Layout.columnSpan: 2; Layout.fillWidth: true; Layout.preferredHeight: 1; color: Theme.border }
            Text { text: "DESIGN"; font.bold: true; font.pixelSize: Theme.smallFont; color: Theme.textDim; Layout.columnSpan: 2 }
            Label_ { text: "Name" }
            TextField {
                id: nameField
                Layout.fillWidth: true
                selectByMouse: true
                text: dlg.doc ? dlg.doc.name : ""
                onEditingFinished: if (dlg.doc && text.trim().length && text !== dlg.doc.name) dlg.doc.name = text.trim()
                background: Rectangle { color: "white"; radius: 2; border.color: nameField.activeFocus ? Theme.accent : Theme.border }
            }
            Label_ { text: "Design type" }
            Text { text: "Part Design"; color: Theme.text }
            Label_ { text: "Part number" }
            InfoField { key: "partNumber" }
            Label_ { text: "Description" }
            InfoField { key: "description" }
            Label_ { text: "Designer" }
            InfoField { key: "designer" }
            Label_ { text: "File" }
            Text {
                Layout.fillWidth: true
                elide: Text.ElideMiddle
                color: Theme.text
                text: dlg.doc && dlg.doc.filePath ? dlg.doc.filePath : "(not saved)"
            }
        }
    }
}
