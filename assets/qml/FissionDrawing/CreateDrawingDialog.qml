// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../FissionApplication"

Popup {
    id: dlg
    property var app
    modal: true
    focus: true
    anchors.centerIn: Overlay.overlay
    width: 360
    height: 460
    padding: 0
    closePolicy: Popup.CloseOnEscape

    property string standard: "ISO"
    property string projectionAngle: "third"
    property string sheetSize: "A3"
    property string orientation: "landscape"
    property int customWidth: 420
    property int customHeight: 297
    property string drawnBy: ""
    property string titleText: ""
    property string partName: ""
    property string material: ""

    signal accepted(var input)

    background: Rectangle { color: Theme.panel; border.color: Theme.border; radius: 4 }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 8

        Text { text: "Create Drawing"; font.pixelSize: 14; font.bold: true; color: Theme.text }
        Text { text: "Creates a 2D drawing from the active design."; color: Theme.textDim; font.pixelSize: 11; wrapMode: Text.WordWrap; Layout.fillWidth: true }

        GridLayout {
            columns: 2
            columnSpacing: 8
            rowSpacing: 6
            Layout.fillWidth: true

            Label { text: "Standard"; color: Theme.text }
            ComboBox {
                model: ["ISO", "ASME"]
                currentIndex: standard === "ASME" ? 1 : 0
                onActivated: (i) => standard = model[i]
                Layout.fillWidth: true
            }
            Label { text: "Projection"; color: Theme.text }
            ComboBox {
                model: ["third", "first"]
                currentIndex: projectionAngle === "first" ? 1 : 0
                onActivated: (i) => projectionAngle = model[i]
                Layout.fillWidth: true
            }
            Label { text: "Sheet size"; color: Theme.text }
            ComboBox {
                model: ["A4", "A3", "A2", "A1", "A0", "A", "B", "C", "D", "E", "custom"]
                Component.onCompleted: currentIndex = Math.max(0, model.indexOf(sheetSize))
                onActivated: (i) => sheetSize = model[i]
                Layout.fillWidth: true
            }
            Label { text: "Orientation"; color: Theme.text }
            ComboBox {
                model: ["landscape", "portrait"]
                currentIndex: orientation === "portrait" ? 1 : 0
                onActivated: (i) => orientation = model[i]
                Layout.fillWidth: true
            }
            Label { text: "Width (mm)"; color: Theme.text; visible: sheetSize === "custom" }
            SpinBox {
                from: 50; to: 5000; value: customWidth; visible: sheetSize === "custom"
                onValueModified: customWidth = value
                Layout.fillWidth: true
            }
            Label { text: "Height (mm)"; color: Theme.text; visible: sheetSize === "custom" }
            SpinBox {
                from: 50; to: 5000; value: customHeight; visible: sheetSize === "custom"
                onValueModified: customHeight = value
                Layout.fillWidth: true
            }
            Label { text: "Title"; color: Theme.text }
            TextField { text: titleText; onTextChanged: titleText = text; Layout.fillWidth: true }
            Label { text: "Part name"; color: Theme.text }
            TextField { text: partName; onTextChanged: partName = text; Layout.fillWidth: true }
            Label { text: "Drawn by"; color: Theme.text }
            TextField { text: drawnBy; onTextChanged: drawnBy = text; Layout.fillWidth: true }
            Label { text: "Material"; color: Theme.text }
            TextField { text: material; onTextChanged: material = text; Layout.fillWidth: true }
        }

        Item { Layout.fillHeight: true }

        RowLayout {
            Layout.alignment: Qt.AlignRight
            Button { text: "Cancel"; onClicked: dlg.close() }
            Button {
                text: "Create Drawing"
                highlighted: true
                onClicked: {
                    accepted({
                        standard: standard,
                        projectionAngle: projectionAngle,
                        sheetSize: sheetSize,
                        orientation: orientation,
                        customWidth: customWidth,
                        customHeight: customHeight,
                        title: titleText,
                        partName: partName,
                        drawnBy: drawnBy,
                        material: material
                    })
                    dlg.close()
                }
            }
        }
    }
}
