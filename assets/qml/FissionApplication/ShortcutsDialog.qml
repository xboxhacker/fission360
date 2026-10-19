// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "."

// Help > Keyboard Shortcuts: the Fusion default keys and what they do in Fission 360
Popup {
    id: dlg
    property var app
    modal: true
    focus: true
    anchors.centerIn: Overlay.overlay
    width: 460
    height: Math.min(620, app.height - 80)
    padding: 0
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    background: Rectangle { color: Theme.panel; border.color: Theme.border; radius: 4 }

    readonly property var extra: [
        ["Esc", "Cancel the current input, tool or command", "General"],
        ["Enter", "OK in a command dialog; commit a typed sketch dimension", "General"],
        ["Del", "Delete selected sketch curves / constraints, or construction geometry", "General"],
        ["S", "Search commands (toolbox)", "General"],
        ["Tab", "Next dimension box while sketching", "Sketch"],
        ["Middle drag", "Pan", "Mouse"],
        ["Shift + middle drag / right drag", "Orbit", "Mouse"],
        ["Wheel", "Zoom", "Mouse"],
        ["Left drag on empty space", "Select with the current tool (window, freeform or paint)", "Mouse"],
        ["Shift / Ctrl + drag", "Add to the selection", "Mouse"],
        ["0-9 . -", "With a size handle shown: type the value straight into its box", "General"],
        ["Left drag on a body (Move)", "Drag the picked bodies freely in the view plane", "Mouse"]
    ]
    readonly property var rows: {
        const out = []
        for (const c of app.commandCatalog()) if (c.keys) out.push([c.keys, c.name, c.section])
        return out.concat(extra)
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 34
            color: Theme.header
            radius: 4
            Text { anchors.verticalCenter: parent.verticalCenter; x: 12; text: "Keyboard Shortcuts"; color: Theme.headerText; font.bold: true; font.pixelSize: 13 }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                anchors.right: parent.right
                anchors.rightMargin: 12
                text: "\u2715"
                color: Theme.headerText
                MouseArea { anchors.fill: parent; anchors.margins: -6; onClicked: dlg.close() }
            }
        }

        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.margins: 8
            clip: true
            model: dlg.rows
            delegate: RowLayout {
                required property var modelData
                width: ListView.view.width
                height: 24
                spacing: 10
                Rectangle {
                    Layout.preferredWidth: 150
                    Layout.preferredHeight: 18
                    radius: 3
                    color: Theme.panelAlt
                    border.color: Theme.border
                    Text { anchors.centerIn: parent; text: modelData[0]; font.pixelSize: 11; color: Theme.text; elide: Text.ElideRight; width: parent.width - 8; horizontalAlignment: Text.AlignHCenter }
                }
                Text { text: modelData[1]; color: Theme.text; Layout.fillWidth: true; elide: Text.ElideRight }
                Text { text: modelData[2]; color: Theme.textDim; font.pixelSize: Theme.smallFont }
            }
        }
    }
}
