// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import QtQuick.Window
import "."

// File ▸ Export Viewport PNG — size picker, then grab the 3D view without the grid.
Popup {
    id: dlg
    property var app
    property int imgWidth: 1920
    property int imgHeight: 1080

    modal: true
    focus: true
    anchors.centerIn: Overlay.overlay
    width: 380
    padding: 0
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    background: Rectangle { color: Theme.panel; border.color: Theme.border; radius: 4 }

    function openForExport() {
        if (app && app.viewport) {
            const dpr = Screen.devicePixelRatio || 1
            imgWidth = Math.max(256, Math.round(app.viewport.width * dpr))
            imgHeight = Math.max(256, Math.round(app.viewport.height * dpr))
            wField.text = String(imgWidth)
            hField.text = String(imgHeight)
        }
        presetBox.currentIndex = 3 // Custom / current view
        open()
    }

    FileDialog {
        id: saveDlg
        title: "Export viewport PNG"
        fileMode: FileDialog.SaveFile
        nameFilters: ["PNG image (*.png)", "All files (*)"]
        onAccepted: {
            let path = fissionApp.toLocalPath(selectedFile.toString())
            if (!path.toLowerCase().endsWith(".png")) path += ".png"
            dlg.doGrab(path)
        }
    }

    function doGrab(path) {
        if (!app || !app.viewport || !app.viewport.renderer) {
            fissionApp.showMessage("No viewport to capture")
            return
        }
        const r = app.viewport.renderer
        const w = Math.max(64, parseInt(wField.text, 10) || imgWidth)
        const h = Math.max(64, parseInt(hField.text, 10) || imgHeight)
        imgWidth = w
        imgHeight = h
        const hadGrid = r.showGrid
        r.showGrid = false
        // Let the renderer rebuild without grid, then grab at the requested size
        Qt.callLater(function() {
            r.grabToImage(function(result) {
                r.showGrid = hadGrid
                const ok = result && result.saveToFile(path)
                fissionApp.showMessage(ok ? ("Viewport PNG → " + path) : "PNG export failed")
                if (ok) dlg.close()
            }, Qt.size(w, h))
        })
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0
        width: dlg.width

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 34
            color: Theme.header
            radius: 4
            Text {
                anchors.verticalCenter: parent.verticalCenter
                x: 12
                text: "Export Viewport PNG"
                color: Theme.headerText
                font.bold: true
                font.pixelSize: 13
            }
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
            Layout.margins: 14
            spacing: 10

            Text {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: Theme.textDim
                font.pixelSize: 11
                text: "Fast screen grab of the 3D view (no grid). Overlays and browser chrome are not included."
            }
            Text { text: "Size"; color: Theme.textDim; font.bold: true; font.pixelSize: 11 }
            ComboBox {
                id: presetBox
                Layout.fillWidth: true
                model: ["1280 × 720 (HD)", "1920 × 1080 (Full HD)", "2560 × 1440 (QHD)", "Custom / current view", "3840 × 2160 (4K)"]
                currentIndex: 3
                onActivated: {
                    if (index === 0) { wField.text = "1280"; hField.text = "720" }
                    else if (index === 1) { wField.text = "1920"; hField.text = "1080" }
                    else if (index === 2) { wField.text = "2560"; hField.text = "1440" }
                    else if (index === 4) { wField.text = "3840"; hField.text = "2160" }
                    else {
                        if (app && app.viewport) {
                            const dpr = (typeof Screen !== "undefined" && Screen.devicePixelRatio) ? Screen.devicePixelRatio : 1
                            wField.text = String(Math.max(256, Math.round(app.viewport.width * dpr)))
                            hField.text = String(Math.max(256, Math.round(app.viewport.height * dpr)))
                        }
                    }
                }
            }
            RowLayout {
                Layout.fillWidth: true
                Label { text: "Width"; color: Theme.textDim; Layout.preferredWidth: 48 }
                TextField {
                    id: wField
                    Layout.fillWidth: true
                    text: String(dlg.imgWidth)
                    validator: IntValidator { bottom: 64; top: 8192 }
                }
                Label { text: "Height"; color: Theme.textDim; Layout.preferredWidth: 48 }
                TextField {
                    id: hField
                    Layout.fillWidth: true
                    text: String(dlg.imgHeight)
                    validator: IntValidator { bottom: 64; top: 8192 }
                }
            }
            RowLayout {
                Layout.fillWidth: true
                Button {
                    text: "Export…"
                    highlighted: true
                    onClicked: {
                        dlg.imgWidth = Math.max(64, parseInt(wField.text, 10) || 1920)
                        dlg.imgHeight = Math.max(64, parseInt(hField.text, 10) || 1080)
                        saveDlg.open()
                    }
                }
                Button { text: "Cancel"; onClicked: dlg.close() }
                Item { Layout.fillWidth: true }
            }
        }
    }
}
