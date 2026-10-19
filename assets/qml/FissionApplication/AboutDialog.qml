// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "."

// Help ▸ About Fission 360
Popup {
    id: dlg
    property var app
    property bool showNotices: false
    onOpened: showNotices = false
    modal: true
    focus: true
    anchors.centerIn: Overlay.overlay
    width: Math.min(520, Overlay.overlay ? Overlay.overlay.width - 40 : 520)
    height: Math.min(560, Overlay.overlay ? Overlay.overlay.height - 40 : 560)
    padding: 0
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    readonly property string version: (typeof fissionApp !== "undefined" && fissionApp.version)
                                      ? fissionApp.version
                                      : ((typeof Qt !== "undefined" && Qt.application)
                                         ? Qt.application.version : "")

    background: Rectangle { color: Theme.panel; border.color: Theme.border; radius: 4 }

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
                text: "About Fission 360"
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
            Layout.fillHeight: true
            Layout.margins: 16
            spacing: 10

            Text {
                Layout.fillWidth: true
                text: "Fission 360"
                color: Theme.text
                font.pixelSize: 22
                font.bold: true
            }
            Text {
                Layout.fillWidth: true
                text: "Version " + (dlg.version || "dev")
                color: Theme.textDim
                font.pixelSize: 13
            }
            Text {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: Theme.text
                font.pixelSize: 13
                text: "Parametric CAD for solid modeling, assemblies, drawings, and manufacturing workflows."
            }
            Text {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: Theme.text
                font.pixelSize: 13
                text: "Copyright © FissionCAD contributors.\nLicensed under the GNU General Public License version 3 (GPLv3)."
            }
            Text {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: Theme.textDim
                font.pixelSize: Theme.smallFont
                text: "This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version. See the LICENSE file distributed with this software and <https://www.gnu.org/licenses/>."
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 6
                Button { text: "License (GPLv3)"; flat: true; highlighted: !dlg.showNotices; onClicked: dlg.showNotices = false }
                Button { text: "Third-party notices"; flat: true; highlighted: dlg.showNotices; onClicked: dlg.showNotices = true }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: Theme.header
                border.color: Theme.border
                radius: 3
                clip: true
                ScrollView {
                    anchors.fill: parent
                    anchors.margins: 1
                    TextArea {
                        id: licenseView
                        readOnly: true
                        wrapMode: TextArea.Wrap
                        selectByMouse: true
                        color: Theme.textDim
                        font.family: "Consolas"
                        font.pixelSize: 11
                        background: null
                        text: dlg.showNotices
                              ? (fissionApp.noticesText() || "(THIRD_PARTY_NOTICES.md not embedded — see the file next to the application.)")
                              : (fissionApp.licenseText() || "(LICENSE not embedded — see the LICENSE file next to the application.)")
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                Button {
                    text: "Close"
                    highlighted: true
                    onClicked: dlg.close()
                }
            }
        }
    }
}
