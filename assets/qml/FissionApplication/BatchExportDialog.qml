// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import "."

// File > Batch Export: several bodies, several formats, one folder; one file per body or one for all
Popup {
    id: dlg
    property var app
    readonly property var doc: app.doc
    property var picked: []          // body indices
    property var formats: ["step", "stl"]
    property bool separate: true
    property string folder: ""
    property string report: ""
    modal: true
    focus: true
    anchors.centerIn: Overlay.overlay
    width: 460
    height: col.implicitHeight + 50
    padding: 0
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    readonly property var allFormats: [["step", "STEP"], ["iges", "IGES"], ["stl", "STL"], ["obj", "OBJ"], ["3mf", "3MF"]]

    onOpened: {
        const vis = []
        if (doc) for (let i = 0; i < doc.bodies.length; ++i) if (doc.bodies[i].visible) vis.push(i)
        picked = vis
        report = ""
        if (!folder && doc && doc.filePath) folder = doc.filePath.substring(0, Math.max(doc.filePath.lastIndexOf("/"), doc.filePath.lastIndexOf("\\")))
    }
    function toggle(list, v, on) {
        const out = list.filter(x => x !== v)
        if (on) out.push(v)
        return out
    }
    function run() {
        if (!doc) return []
        const files = doc.exportBatch(picked.slice().sort((a, b) => a - b), folder, formats, separate, doc.name)
        const err = doc.lastError()
        report = files.length + " file(s) written" + (err ? "\n" + err : "")
        fissionApp.showMessage("Batch export: " + files.length + " file(s) written to " + folder)
        return files
    }

    background: Rectangle { color: Theme.panel; border.color: Theme.border; radius: 4 }

    FolderDialog {
        id: folderDialog
        title: "Export to folder"
        onAccepted: dlg.folder = fissionApp.toLocalPath(selectedFolder.toString())
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 34
            color: Theme.header
            radius: 4
            Text { anchors.verticalCenter: parent.verticalCenter; x: 12; text: "Batch Export"; color: Theme.headerText; font.bold: true; font.pixelSize: 13 }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                anchors.right: parent.right
                anchors.rightMargin: 12
                text: "\u2715"
                color: Theme.headerText
                MouseArea { anchors.fill: parent; anchors.margins: -6; onClicked: dlg.close() }
            }
        }

        ColumnLayout {
            id: col
            Layout.fillWidth: true
            Layout.margins: 12
            spacing: 8

            Text { text: "BODIES"; font.bold: true; font.pixelSize: Theme.smallFont; color: Theme.textDim }
            ScrollView {
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(160, bodyList.implicitHeight)
                clip: true
                Column {
                    id: bodyList
                    width: parent.width
                    Repeater {
                        model: dlg.doc ? dlg.doc.bodies : []
                        CheckBox {
                            required property var modelData
                            required property int index
                            text: modelData.name + (modelData.isMesh ? "  (mesh)" : "")
                            checked: dlg.picked.indexOf(index) >= 0
                            onToggled: dlg.picked = dlg.toggle(dlg.picked, index, checked)
                        }
                    }
                }
            }
            RowLayout {
                Button { text: "All"; onClicked: { const a = []; for (let i = 0; i < dlg.doc.bodies.length; ++i) a.push(i); dlg.picked = a } }
                Button { text: "None"; onClicked: dlg.picked = [] }
            }

            Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: Theme.border }
            Text { text: "FORMATS"; font.bold: true; font.pixelSize: Theme.smallFont; color: Theme.textDim }
            Flow {
                Layout.fillWidth: true
                spacing: 6
                Repeater {
                    model: dlg.allFormats
                    CheckBox {
                        required property var modelData
                        text: modelData[1]
                        checked: dlg.formats.indexOf(modelData[0]) >= 0
                        onToggled: dlg.formats = dlg.toggle(dlg.formats, modelData[0], checked)
                    }
                }
            }
            CheckBox {
                text: "One file per body (named after the body)"
                checked: dlg.separate
                onToggled: dlg.separate = checked
            }
            Text {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                font.pixelSize: Theme.smallFont
                color: Theme.textDim
                text: "STEP and IGES take solid bodies only; mesh bodies go to STL, OBJ and 3MF. "
                      + (dlg.separate ? "" : "All bodies go into one file named after the design.")
            }

            Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: Theme.border }
            Text { text: "FOLDER"; font.bold: true; font.pixelSize: Theme.smallFont; color: Theme.textDim }
            RowLayout {
                Layout.fillWidth: true
                TextField {
                    id: folderField
                    Layout.fillWidth: true
                    selectByMouse: true
                    text: dlg.folder
                    placeholderText: "Choose a folder"
                    onEditingFinished: dlg.folder = text
                    background: Rectangle { color: "white"; radius: 2; border.color: folderField.activeFocus ? Theme.accent : Theme.border }
                }
                Button { text: "Browse..."; onClicked: folderDialog.open() }
            }
            Text {
                visible: dlg.report.length > 0
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: Theme.text
                text: dlg.report
            }
            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                Button { text: "Close"; onClicked: dlg.close() }
                Button {
                    text: "Export"
                    highlighted: true
                    enabled: dlg.picked.length > 0 && dlg.formats.length > 0 && dlg.folder.length > 0
                    onClicked: dlg.run()
                }
            }
        }
    }
}
