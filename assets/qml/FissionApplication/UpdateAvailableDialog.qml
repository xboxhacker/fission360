// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "."

// Startup popup when GitHub has a newer release than this build.
Popup {
    id: dlg
    property string latestVersion: ""
    property string releaseUrl: ""
    property string summary: ""

    modal: true
    focus: true
    anchors.centerIn: Overlay.overlay
    width: Math.min(440, Overlay.overlay ? Overlay.overlay.width - 40 : 440)
    padding: 0
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    background: Rectangle { color: Theme.panel; border.color: Theme.border; radius: 4 }

    function showUpdate(version, url, text) {
        latestVersion = version
        releaseUrl = url
        summary = text || ("Fission 360 " + version + " is available.")
        open()
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
                text: "Update available"
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
                color: Theme.text
                font.pixelSize: 13
                text: "A newer Fission 360 is on GitHub.\n\nYou have "
                      + ((typeof fissionApp !== "undefined" && fissionApp.version) ? fissionApp.version : "?")
                      + " — latest is " + dlg.latestVersion + "."
            }
            Text {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: Theme.textDim
                font.pixelSize: 11
                text: dlg.summary
                visible: dlg.summary.length > 0
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                Button {
                    text: "Download"
                    highlighted: true
                    onClicked: {
                        fissionApp.openUpdateUrl(dlg.releaseUrl)
                        dlg.close()
                    }
                }
                Button {
                    text: "Later"
                    onClicked: dlg.close()
                }
                Item { Layout.fillWidth: true }
                Button {
                    text: "Skip this version"
                    flat: true
                    onClicked: {
                        fissionApp.skipUpdateVersion(dlg.latestVersion)
                        dlg.close()
                    }
                }
            }
        }
    }
}
