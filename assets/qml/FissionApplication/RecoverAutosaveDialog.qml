// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "."

// File ▸ Recover Autosave — restore or delete crash / power-loss snapshots from AppData.
// Startup no longer auto-prompts; use this dialog from the File menu.
Popup {
    id: dlg
    property var pending: []
    modal: true
    focus: true
    anchors.centerIn: Overlay.overlay
    width: 520
    height: 360
    padding: 0
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    background: Rectangle { color: Theme.panel; border.color: Theme.border; radius: 4 }

    function reload() {
        pending = fissionApp.pendingAutosaves()
    }
    onOpened: reload()

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 34
            color: Theme.header
            radius: 4
            Text {
                anchors.verticalCenter: parent.verticalCenter
                x: 12
                text: "Recover Autosave"
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

        Text {
            Layout.fillWidth: true
            Layout.margins: 12
            Layout.bottomMargin: 0
            text: "Autosaves are written while a design is dirty (default every 2 minutes). Recover opens the snapshot and removes it from this list. Delete discards it without opening."
            color: Theme.textDim
            font.pixelSize: 11
            wrapMode: Text.WordWrap
        }

        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.margins: 12
            clip: true
            model: dlg.pending
            spacing: 4
            delegate: Rectangle {
                required property var modelData
                width: ListView.view.width
                height: 48
                color: Theme.panel
                border.color: Theme.border
                radius: 3
                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 6
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2
                        Text {
                            text: modelData.name || modelData.documentId
                            color: Theme.text
                            font.bold: true
                            elide: Text.ElideMiddle
                            Layout.fillWidth: true
                        }
                        Text {
                            text: (modelData.originalPath || "(unsaved)") + " · " + (modelData.savedAt || modelData.modified || "")
                            color: Theme.textDim
                            font.pixelSize: 10
                            elide: Text.ElideMiddle
                            Layout.fillWidth: true
                        }
                    }
                    Button {
                        text: "Delete"
                        onClicked: {
                            if (fissionApp.deleteAutosave(modelData.documentId))
                                dlg.reload()
                            else
                                fissionApp.showMessage("Could not delete autosave")
                        }
                    }
                    Button {
                        text: "Recover"
                        highlighted: true
                        onClicked: {
                            if (fissionApp.recoverAutosave(modelData.documentId)) {
                                dlg.reload()
                                if (dlg.pending.length === 0)
                                    dlg.close()
                            } else {
                                fissionApp.showMessage("Could not recover autosave")
                            }
                        }
                    }
                }
            }
            Text {
                anchors.centerIn: parent
                visible: dlg.pending.length === 0
                text: "No autosaves found"
                color: Theme.textDim
            }
        }

        RowLayout {
            Layout.alignment: Qt.AlignRight
            Layout.margins: 12
            Button { text: "Refresh"; onClicked: dlg.reload() }
            Button { text: "Close"; onClicked: dlg.close() }
        }
    }
}
