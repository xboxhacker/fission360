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
    width: 380
    height: Math.min((Overlay.overlay ? Overlay.overlay.height : 640) - 48, formCol.implicitHeight + 28)
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

    function pinComboPopup(box) {
        if (!box || !box.popup) return
        box.popup.parent = Overlay.overlay
        box.popup.z = 2000
        box.popup.aboutToShow.connect(function() {
            const p = box.mapToItem(Overlay.overlay, 0, box.height)
            box.popup.x = p.x
            box.popup.y = p.y
            box.popup.width = box.width
        })
    }

    background: Rectangle { color: Theme.panel; border.color: Theme.border; radius: 4 }

    ColumnLayout {
        id: formCol
        anchors.fill: parent
        anchors.margins: 14
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
                id: stdBox
                model: ["ISO", "ASME"]
                currentIndex: standard === "ASME" ? 1 : 0
                onActivated: (i) => standard = model[i]
                Layout.fillWidth: true
                implicitHeight: 30
                Component.onCompleted: dlg.pinComboPopup(stdBox)
            }
            Label { text: "Projection"; color: Theme.text }
            ComboBox {
                id: projBox
                model: ["third", "first"]
                currentIndex: projectionAngle === "first" ? 1 : 0
                onActivated: (i) => projectionAngle = model[i]
                Layout.fillWidth: true
                implicitHeight: 30
                Component.onCompleted: dlg.pinComboPopup(projBox)
            }
            Label { text: "Sheet size"; color: Theme.text }
            ComboBox {
                id: sizeBox
                model: ["A4", "A3", "A2", "A1", "A0", "A", "B", "C", "D", "E", "custom"]
                Component.onCompleted: {
                    currentIndex = Math.max(0, model.indexOf(sheetSize))
                    dlg.pinComboPopup(sizeBox)
                }
                onActivated: (i) => sheetSize = model[i]
                Layout.fillWidth: true
                implicitHeight: 30
            }
            Label { text: "Orientation"; color: Theme.text }
            ComboBox {
                id: landBox
                model: ["landscape", "portrait"]
                currentIndex: orientation === "portrait" ? 1 : 0
                onActivated: (i) => orientation = model[i]
                Layout.fillWidth: true
                implicitHeight: 30
                Component.onCompleted: dlg.pinComboPopup(landBox)
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
