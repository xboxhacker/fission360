// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "."

// Application bar: file menu, quick actions and the document title
Rectangle {
    id: bar
    property var app
    implicitHeight: 34
    color: Theme.header

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 8
        anchors.rightMargin: 8
        spacing: 2

        Rectangle {
            Layout.preferredWidth: 26
            Layout.preferredHeight: 22
            radius: 3
            color: "#f28c28"
            Text {
                anchors.centerIn: parent
                text: "F"
                color: "white"
                font.bold: true
                font.pixelSize: 15
            }
        }

        ToolbarButton {
            compact: true; glyph: "projects"; glyphColor: Theme.headerText; tip: "Projects (Ctrl+Alt+P)"
            onClicked: bar.app.openProjects()
        }
        ToolbarButton {
            id: fileButton
            compact: true
            glyph: "open"
            glyphColor: Theme.headerText
            tip: "File"
            onClicked: fileMenu.popup(fileButton, 0, fileButton.height)
            Menu {
                id: fileMenu
                MenuItem { text: "New Design\tCtrl+N"; onTriggered: bar.app.newDesign() }
                MenuItem { text: "Open Demo Bracket"; onTriggered: { bar.app.leaveDesign(); fissionApp.openDemo() } }
                MenuItem { text: "Projects...\tCtrl+Alt+P"; onTriggered: bar.app.openProjects() }
                MenuItem { text: "Open...\tCtrl+O"; onTriggered: bar.app.openFile() }
                MenuSeparator {}
                MenuItem { text: "Save\tCtrl+S"; onTriggered: bar.app.saveOrSaveAs() }
                MenuItem { text: "Save As...\tCtrl+Shift+S"; onTriggered: bar.app.saveAs() }
                MenuItem { text: "Version History..."; onTriggered: bar.app.openVersionHistory() }
                MenuItem { text: "Recover Autosave..."; onTriggered: bar.app.openRecoverAutosave() }
                MenuItem { text: "Send to Slicer..."; onTriggered: bar.app.sendToOrca() }
                MenuItem { text: "Export (STEP / IGES / STL / OBJ / 3MF)..."; onTriggered: bar.app.exportFile() }
                MenuItem { text: "Export Viewport PNG..."; onTriggered: bar.app.exportViewportPng() }
                MenuItem { text: "Batch Export..."; onTriggered: bar.app.openBatchExport() }
                MenuItem { text: "Export Face / Sketch as DXF..."; onTriggered: bar.app.exportDxf() }
                MenuSeparator {}
                MenuItem { text: "Close Design\tCtrl+W"; onTriggered: bar.app.closeDesign(fissionApp.activeIndex) }
                MenuSeparator {}
                MenuItem { text: "Exit"; onTriggered: Qt.quit() }
                MenuSeparator {}
                MenuItem { text: "About Fission 360..."; onTriggered: bar.app.openAbout() }
                MenuItem { text: "Report Bug..."; onTriggered: fissionApp.reportBug() }
            }
        }
        ToolbarButton {
            compact: true; glyph: "save"; glyphColor: Theme.headerText; tip: "Save (Ctrl+S)"
            onClicked: bar.app.saveOrSaveAs()
        }
        Rectangle { Layout.preferredWidth: 1; Layout.preferredHeight: 18; color: "#5a626b" }
        ToolbarButton {
            compact: true; glyph: "undo"; glyphColor: Theme.headerText; tip: "Undo (Ctrl+Z)"
            enabled: bar.app.inDrawing
                     ? (bar.app.activeDrawing && bar.app.activeDrawing.canUndo)
                     : (bar.app.doc && bar.app.doc.canUndo)
            onClicked: bar.app.editUndo()
        }
        ToolbarButton {
            compact: true; glyph: "redo"; glyphColor: Theme.headerText; tip: "Redo (Ctrl+Y)"
            enabled: bar.app.inDrawing
                     ? (bar.app.activeDrawing && bar.app.activeDrawing.canRedo)
                     : (bar.app.doc && bar.app.doc.canRedo)
            onClicked: bar.app.editRedo()
        }

        Item { Layout.preferredWidth: 10 }

        // One tab per open design
        Flickable {
            id: tabStrip
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: tabRow.width
            clip: true
            flickableDirection: Flickable.HorizontalFlick
            boundsBehavior: Flickable.StopAtBounds
            Row {
                id: tabRow
                height: parent.height
                spacing: 1
                Repeater {
                    model: fissionApp.documents
                    delegate: Rectangle {
                        id: tab
                        required property var modelData
                        required property int index
                        readonly property bool active: modelData === fissionApp.activeDocument
                        objectName: "designTab_" + index
                        function reveal() {
                            if (!active) return
                            if (x < tabStrip.contentX) tabStrip.contentX = x
                            else if (x + width > tabStrip.contentX + tabStrip.width)
                                tabStrip.contentX = x + width - tabStrip.width
                        }
                        onActiveChanged: Qt.callLater(reveal)
                        onXChanged: Qt.callLater(reveal)
                        Component.onCompleted: Qt.callLater(reveal)
                        Connections {
                            target: tabStrip
                            function onWidthChanged() { Qt.callLater(tab.reveal) }
                        }
                        width: Math.min(220, tabLabel.implicitWidth + 44)
                        height: tabStrip.height
                        color: active ? Theme.window : tabArea.containsMouse ? "#4a535d" : "transparent"
                        Rectangle {
                            visible: tab.active
                            anchors { left: parent.left; right: parent.right; top: parent.top }
                            height: 2
                            color: Theme.accent
                        }
                        MouseArea {
                            id: tabArea
                            anchors.fill: parent
                            hoverEnabled: true
                            acceptedButtons: Qt.LeftButton | Qt.MiddleButton
                            onClicked: (m) => m.button === Qt.MiddleButton ? bar.app.closeDesign(tab.index)
                                                                           : bar.app.switchDesign(tab.index)
                        }
                        Text {
                            id: tabLabel
                            anchors { left: parent.left; leftMargin: 10; verticalCenter: parent.verticalCenter }
                            width: Math.min(implicitWidth, tab.width - 40)
                            elide: Text.ElideRight
                            text: tab.modelData.name + (tab.modelData.modified ? "  \u2022" : "")
                            color: tab.active ? Theme.text : Theme.headerText
                            font.pixelSize: 12
                            font.bold: tab.active
                        }
                        Rectangle {
                            anchors { right: parent.right; rightMargin: 6; verticalCenter: parent.verticalCenter }
                            width: 18; height: 18; radius: 3
                            color: closeArea.containsMouse ? (tab.active ? "#d5d9de" : "#5d6772") : "transparent"
                            Text {
                                anchors.centerIn: parent
                                text: "\u2715"
                                font.pixelSize: 10
                                color: tab.active ? Theme.text : Theme.headerText
                            }
                            MouseArea {
                                id: closeArea
                                anchors.fill: parent
                                hoverEnabled: true
                                onClicked: bar.app.closeDesign(tab.index)
                            }
                            ToolTip.visible: closeArea.containsMouse
                            ToolTip.delay: 600
                            ToolTip.text: "Close " + tab.modelData.name
                        }
                    }
                }
                ToolbarButton {
                    compact: true; glyph: "plus"; glyphColor: Theme.headerText; tip: "New Design (Ctrl+N)"
                    anchors.verticalCenter: parent.verticalCenter
                    onClicked: bar.app.newDesign()
                }
            }
        }

        Item { Layout.preferredWidth: 10 }

        ToolbarButton {
            compact: true
            glyph: "bug"
            glyphColor: Theme.headerText
            tip: "Report a bug on GitHub (opens a pre-filled issue)"
            onClicked: fissionApp.reportBug()
        }
        Text {
            text: "Fission 360  v" + fissionApp.version
            color: "#aab2bb"
            font.pixelSize: Theme.smallFont
        }
    }
}
