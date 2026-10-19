// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "."

// File ▸ Version History — internal save versions for the active design; Revert restores a snapshot.
Popup {
    id: dlg
    property var app
    readonly property var doc: app ? app.doc : null
    property var versions: []
    modal: true
    focus: true
    anchors.centerIn: Overlay.overlay
    width: 440
    height: 360
    padding: 0
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    background: Rectangle { color: Theme.panel; border.color: Theme.border; radius: 4 }

    function reload() {
        versions = (doc && doc.filePath) ? doc.versionHistory() : []
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
                text: "Version History" + (doc ? (" — v" + doc.versionNumber) : "")
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
            text: doc && !doc.filePath
                  ? "Save the design once to start version history."
                  : "Each Save stores a compressed snapshot next to the file. Revert loads that snapshot (linked drawings may then be out of date)."
            color: Theme.textDim
            font.pixelSize: 11
            wrapMode: Text.WordWrap
        }

        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.margins: 12
            clip: true
            model: dlg.versions
            spacing: 4
            delegate: Rectangle {
                required property var modelData
                width: ListView.view.width
                height: 40
                color: Theme.panel
                border.color: Theme.border
                radius: 3
                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 8
                    Text {
                        text: "v" + modelData.version + (modelData.label ? (" — " + modelData.label) : "")
                        color: Theme.text
                        font.bold: true
                        Layout.fillWidth: true
                    }
                    Text {
                        text: modelData.savedAt || ""
                        color: Theme.textDim
                        font.pixelSize: 10
                    }
                    Button {
                        text: "Revert"
                        flat: true
                        onClicked: {
                            if (app && app.revertDesignToVersion)
                                app.revertDesignToVersion(modelData.version)
                            dlg.reload()
                            dlg.close()
                        }
                    }
                }
            }
            Text {
                anchors.centerIn: parent
                visible: dlg.versions.length === 0
                text: "No versions yet"
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
