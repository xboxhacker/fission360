// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import "."

// VIEW / Preferences: viewport background gradient — changes apply immediately and are saved.
Popup {
    id: dlg
    property var app
    modal: true
    focus: true
    anchors.centerIn: Overlay.overlay
    width: 480
    height: col.implicitHeight + 50
    padding: 0
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    property color topColor: "#3c4148"
    property color bottomColor: "#1e2125"
    property string editing: "top"   // which swatch the color dialog edits

    background: Rectangle { color: Theme.panel; border.color: Theme.border; radius: 4 }

    property bool searching: false
    property string searchWhere: ""
    property string searchNote: ""
    property var slicerResults: []
    property int slicerRev: 0

    function nativePath(p) { return p.replace(/\//g, "\\") }
    function startSearch() {
        slicerResults = []
        searchNote = ""
        searching = true
        fissionApp.searchSlicers()
    }
    function refreshSlicer() {
        const orca = fissionApp.findSlicerExecutable("orca")
        const bambu = fissionApp.findSlicerExecutable("bambu")
        const active = fissionApp.activeSlicer()
        slicerRev++
        slicerBox.currentIndex = Math.max(0, slicerBox.keys.indexOf(fissionApp.setting("slicer/choice", "auto")))
        slicerStatus.text = "OrcaSlicer: " + (orca ? nativePath(orca) : "not found")
            + "\nBambu Studio: " + (bambu ? nativePath(bambu) : "not found")
            + "\n" + (active === "" ? "No slicer available." : "Send to Slicer will open " + fissionApp.slicerName(active) + ".")
            + " Only visible bodies are sent; hidden bodies never are."
    }

    Connections {
        target: fissionApp
        function onSlicerSearchProgress(where) { dlg.searchWhere = where }
        function onSlicerSearchFinished(results, cancelled) {
            dlg.searching = false
            dlg.slicerResults = results
            dlg.searchNote = results.length === 0
                ? (cancelled ? "Search stopped \u2014 nothing found yet." : "No OrcaSlicer or Bambu Studio found. Use Browse\u2026 to pick the exe.")
                : (results.length + " found" + (cancelled ? " (search stopped)" : "") + " \u2014 newest first. Click Use on the one you slice with.")
        }
    }

    FileDialog {
        id: slicerFileDlg
        title: "Select your slicer (orca-slicer.exe, bambu-studio.exe or a custom build)"
        nameFilters: ["Programs (*.exe)", "All files (*)"]
        onAccepted: {
            fissionApp.setSlicerExecutable(selectedFile.toString())
            dlg.load()
        }
    }

    function load() {
        slicerBox.currentIndex = Math.max(0, slicerBox.keys.indexOf(fissionApp.setting("slicer/choice", "auto")))
        refreshSlicer()
        topColor = fissionApp.setting("viewport/backgroundTop", "#3c4148")
        bottomColor = fissionApp.setting("viewport/backgroundBottom", "#1e2125")
        zoomCursorBox.checked = !!(app && app.zoomToCursor)
        showGridBox.checked = fissionApp.setting("viewport/showGrid", true) !== false
            && fissionApp.setting("viewport/showGrid", true) !== "false"
        apply()
    }
    function apply() {
        if (app && app.viewport && app.viewport.renderer) {
            app.viewport.renderer.backgroundTop = topColor
            app.viewport.renderer.backgroundBottom = bottomColor
            app.viewport.renderer.showGrid = showGridBox.checked
        }
        fissionApp.setSetting("viewport/backgroundTop", topColor.toString())
        fissionApp.setSetting("viewport/backgroundBottom", bottomColor.toString())
        fissionApp.setSetting("viewport/showGrid", showGridBox.checked)
    }
    function preset(top, bottom) {
        topColor = top
        bottomColor = bottom
        apply()
    }

    onOpened: load()

    ColorDialog {
        id: colorDlg
        title: editing === "top" ? "Background top" : "Background bottom"
        selectedColor: editing === "top" ? topColor : bottomColor
        onAccepted: {
            if (editing === "top") topColor = selectedColor
            else bottomColor = selectedColor
            apply()
        }
    }

    ColumnLayout {
        id: col
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 34
            color: Theme.header
            radius: 4
            Text { anchors.verticalCenter: parent.verticalCenter; x: 12; text: "Display Settings"; color: Theme.headerText; font.bold: true; font.pixelSize: 13 }
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
            Layout.fillWidth: true
            Layout.margins: 12
            spacing: 10

            Text { text: "Viewport background"; font.bold: true; font.pixelSize: Theme.smallFont; color: Theme.textDim }

            // Live preview strip
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 56
                radius: 3
                gradient: Gradient {
                    GradientStop { position: 0; color: topColor }
                    GradientStop { position: 1; color: bottomColor }
                }
                border.color: Theme.border
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 10
                ColumnLayout {
                    Layout.fillWidth: true
                    Text { text: "Top"; color: Theme.text; font.pixelSize: 11 }
                    Rectangle {
                        Layout.fillWidth: true
                        height: 28
                        radius: 3
                        color: topColor
                        border.color: Theme.border
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: { editing = "top"; colorDlg.open() }
                        }
                    }
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    Text { text: "Bottom"; color: Theme.text; font.pixelSize: 11 }
                    Rectangle {
                        Layout.fillWidth: true
                        height: 28
                        radius: 3
                        color: bottomColor
                        border.color: Theme.border
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: { editing = "bottom"; colorDlg.open() }
                        }
                    }
                }
            }

            CheckBox {
                id: zoomCursorBox
                text: "Zoom to cursor"
                checked: app ? app.zoomToCursor : false
                onToggled: if (app) app.setZoomToCursor(checked)
            }
            Text {
                text: "When on, the mouse wheel zooms toward the pointer instead of the view centre."
                color: Theme.textDim
                font.pixelSize: 10
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }

            CheckBox {
                id: showGridBox
                text: "Show viewport grid"
                checked: true
                onToggled: {
                    fissionApp.setSetting("viewport/showGrid", checked)
                    if (app && app.viewport && app.viewport.renderer)
                        app.viewport.renderer.showGrid = checked
                }
            }
            Text {
                text: "Ground grid and origin axis in the 3D view. The viewport Grid button toggles the same setting."
                color: Theme.textDim
                font.pixelSize: 10
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }

            Text { text: "Slicer"; font.bold: true; font.pixelSize: Theme.smallFont; color: Theme.textDim }
            RowLayout {
                Layout.fillWidth: true
                Text { text: "Send to Slicer opens"; color: Theme.text; Layout.fillWidth: true }
                ComboBox {
                    id: slicerBox
                    readonly property var keys: ["auto", "orca", "bambu"]
                    model: ["Auto-detect", "OrcaSlicer", "Bambu Studio"]
                    Layout.preferredWidth: 160
                    onActivated: {
                        fissionApp.setSetting("slicer/choice", keys[currentIndex])
                        dlg.refreshSlicer()
                    }
                }
            }
            Text {
                id: slicerStatus
                color: Theme.textDim
                font.pixelSize: 10
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }

            // Custom builds: browse for the exe, or search the disk for it
            RowLayout {
                Layout.fillWidth: true
                spacing: 6
                Button { text: "Browse\u2026"; onClicked: slicerFileDlg.open() }
                Button {
                    text: dlg.searching ? "Stop search" : "Search for slicer"
                    highlighted: !dlg.searching
                    onClicked: {
                        if (dlg.searching) fissionApp.cancelSlicerSearch()
                        else dlg.startSearch()
                    }
                }
                Button {
                    text: "Reset"
                    ToolTip.visible: hovered
                    ToolTip.text: "Forget custom paths and go back to auto-detect"
                    onClicked: { fissionApp.clearSlicerExecutables(); dlg.slicerResults = []; dlg.load() }
                }
                Item { Layout.fillWidth: true }
            }
            Text {
                visible: dlg.searching || dlg.searchNote !== ""
                text: dlg.searching ? ("Searching\u2026 " + dlg.searchWhere) : dlg.searchNote
                color: Theme.textDim
                font.pixelSize: 10
                elide: Text.ElideMiddle
                Layout.fillWidth: true
            }
            Rectangle {
                visible: dlg.slicerResults.length > 0
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(132, dlg.slicerResults.length * 40 + 2)
                color: Theme.header
                border.color: Theme.border
                radius: 3
                clip: true
                ListView {
                    anchors.fill: parent
                    anchors.margins: 1
                    model: dlg.slicerResults
                    boundsBehavior: Flickable.StopAtBounds
                    ScrollBar.vertical: ScrollBar {}
                    delegate: Rectangle {
                        required property var modelData
                        width: ListView.view.width
                        height: 40
                        color: hit.containsMouse ? Theme.panel : "transparent"
                        readonly property bool current: {
                            dlg.slicerRev   // re-evaluate after the choice changes
                            return fissionApp.findSlicerExecutable(modelData.slicer).toLowerCase().replace(/\\/g, "/")
                                   === modelData.path.toLowerCase().replace(/\\/g, "/")
                        }
                        Column {
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.left: parent.left
                            anchors.right: useBtn.left
                            anchors.margins: 6
                            Text {
                                width: parent.width
                                text: fissionApp.slicerName(modelData.slicer) + "   " + modelData.modified
                                      + (parent.parent.current ? "   (in use)" : "")
                                color: Theme.text
                                font.pixelSize: 11
                                font.bold: parent.parent.current
                            }
                            Text { width: parent.width; text: modelData.path; elide: Text.ElideMiddle; color: Theme.textDim; font.pixelSize: 10 }
                        }
                        Button {
                            id: useBtn
                            anchors.right: parent.right
                            anchors.rightMargin: 6
                            anchors.verticalCenter: parent.verticalCenter
                            text: "Use"
                            onClicked: { fissionApp.setSlicerExecutable(modelData.path); dlg.load() }
                        }
                        MouseArea { id: hit; anchors.fill: parent; hoverEnabled: true; acceptedButtons: Qt.NoButton }
                    }
                }
            }

            Text { text: "Autosave (crash / power loss)"; font.bold: true; font.pixelSize: Theme.smallFont; color: Theme.textDim }
            RowLayout {
                Layout.fillWidth: true
                Text { text: "Interval (seconds)"; color: Theme.text; Layout.fillWidth: true }
                SpinBox {
                    id: autosaveSec
                    from: 0
                    to: 3600
                    stepSize: 30
                    value: fissionApp.autosaveIntervalSec
                    onValueModified: fissionApp.autosaveIntervalSec = value
                }
            }
            Text {
                text: "0 disables autosave. Dirty designs are written under AppData; File ▸ Recover Autosave restores them."
                color: Theme.textDim
                font.pixelSize: 10
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }

            Text { text: "Presets"; font.bold: true; font.pixelSize: Theme.smallFont; color: Theme.textDim }
            Flow {
                Layout.fillWidth: true
                spacing: 6
                Repeater {
                    model: [
                        { name: "Dark", top: "#3c4148", bottom: "#1e2125" },
                        { name: "Graphite", top: "#2a2d32", bottom: "#121417" },
                        { name: "Blue steel", top: "#3a4555", bottom: "#1a222e" },
                        { name: "Light", top: "#d8dce2", bottom: "#aeb6c0" },
                        { name: "Studio", top: "#6a7380", bottom: "#3e4650" },
                        { name: "White", top: "#f4f4f4", bottom: "#c8c8c8" }
                    ]
                    delegate: Button {
                        required property var modelData
                        text: modelData.name
                        flat: true
                        onClicked: dlg.preset(modelData.top, modelData.bottom)
                    }
                }
            }

            Text {
                text: "Changes apply immediately and are kept between sessions."
                color: Theme.textDim
                font.pixelSize: 10
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }

            RowLayout {
                Layout.alignment: Qt.AlignRight
                Button { text: "Close"; onClicked: dlg.close() }
            }
        }
    }
}
