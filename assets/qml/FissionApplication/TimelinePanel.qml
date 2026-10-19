// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "."

// Parametric history. Features right of the marker are rolled back.
Rectangle {
    id: panel
    property var app
    readonly property var timeline: app.doc ? app.doc.timeline : null
    readonly property int itemWidth: 34
    implicitHeight: 46
    color: Theme.panel

    Rectangle { width: parent.width; height: 1; color: Theme.border }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 8
        anchors.rightMargin: 8
        spacing: 2

        ToolbarButton { compact: true; glyph: "first"; tip: "Move marker to the beginning"
            onClicked: panel.timeline.marker = 0 }
        ToolbarButton { compact: true; glyph: "prev"; tip: "Step back"
            onClicked: panel.timeline.marker = panel.timeline.marker - 1 }
        ToolbarButton { compact: true; glyph: "next"; tip: "Step forward"
            onClicked: panel.timeline.marker = panel.timeline.marker + 1 }
        ToolbarButton { compact: true; glyph: "last"; tip: "Move marker to the end"
            onClicked: panel.timeline.marker = panel.timeline.count }

        Rectangle { Layout.preferredWidth: 1; Layout.preferredHeight: 26; color: Theme.border; Layout.leftMargin: 4; Layout.rightMargin: 4 }

        Flickable {
            id: flick
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: strip.width + 40
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            interactive: contentWidth > width

            Row {
                id: strip
                anchors.verticalCenter: parent.verticalCenter
                spacing: 2
                Repeater {
                    id: rep
                    model: panel.timeline
                    delegate: Rectangle {
                        id: chip
                        required property var feature
                        required property string name
                        required property string type
                        required property bool suppressed
                        required property string error
                        required property bool active
                        required property int index
                        readonly property bool editing: feature && (feature.id === panel.app.commandFeatureId || feature.id === panel.app.activeSketchId)
                        width: panel.itemWidth
                        height: 34
                        radius: 3
                        color: editing ? Theme.accentLight : chipArea.containsMouse ? Theme.hover : "transparent"
                        border.color: error.length ? Theme.error : editing ? Theme.accent : "transparent"
                        opacity: active ? 1 : 0.38

                        Glyph {
                            anchors.centerIn: parent
                            width: 24
                            height: 24
                            name: Theme.featureGlyph(chip.type)
                            color: chip.suppressed ? "#9aa1a8" : Theme.featureColor(chip.type)
                        }
                        Rectangle {   // suppressed strike-through
                            visible: chip.suppressed
                            anchors.centerIn: parent
                            width: 26; height: 2; rotation: -45; color: Theme.textDim
                        }
                        Rectangle {
                            visible: chip.error.length > 0
                            width: 9; height: 9; radius: 4.5
                            color: Theme.error
                            anchors.right: parent.right
                            anchors.top: parent.top
                            anchors.margins: 1
                        }
                        MouseArea {
                            id: chipArea
                            anchors.fill: parent
                            hoverEnabled: true
                            acceptedButtons: Qt.LeftButton | Qt.RightButton
                            onClicked: (m) => {
                                if (m.button === Qt.RightButton) menu.openFor(chip.feature, chip.index)
                            }
                            onDoubleClicked: panel.app.editFeature(chip.feature)
                        }
                        ToolTip.visible: chipArea.containsMouse
                        ToolTip.delay: 400
                        ToolTip.text: chip.name + (chip.suppressed ? " (suppressed)" : "")
                                      + (chip.error.length ? "\n\u26a0 " + chip.error : "")
                    }
                }
            }

            // Marker: drag to roll the model back or forward
            Rectangle {
                id: marker
                x: (panel.timeline ? panel.timeline.marker : 0) * (panel.itemWidth + 2) - 3
                y: 2
                width: 6
                height: flick.height - 4
                radius: 2
                color: Theme.accent
                Rectangle { anchors.horizontalCenter: parent.horizontalCenter; width: 12; height: 6; radius: 2; color: Theme.accent }
                MouseArea {
                    id: markerArea
                    anchors.fill: parent
                    anchors.margins: -4
                    cursorShape: Qt.SizeHorCursor
                    drag.target: marker
                    drag.axis: Drag.XAxis
                    drag.minimumX: -3
                    drag.maximumX: strip.width
                    onReleased: {
                        const m = Math.round((marker.x + 3) / (panel.itemWidth + 2))
                        panel.timeline.marker = m
                        marker.x = Qt.binding(() => (panel.timeline ? panel.timeline.marker : 0) * (panel.itemWidth + 2) - 3)
                    }
                }
            }
        }

        Text {
            text: panel.timeline ? panel.timeline.count + " features" : ""
            color: Theme.textDim
            font.pixelSize: Theme.smallFont
        }
    }

    Menu {
        id: menu
        property var target: null
        property int targetIndex: -1
        function openFor(f, i) { target = f; targetIndex = i; popup() }
        MenuItem { text: "Edit Feature"; onTriggered: panel.app.editFeature(menu.target) }
        MenuItem {
            text: menu.target && menu.target.suppressed ? "Unsuppress" : "Suppress"
            onTriggered: { panel.app.doc.checkpoint(); menu.target.suppressed = !menu.target.suppressed }
        }
        MenuItem { text: "Roll Back To Here"; onTriggered: panel.timeline.marker = menu.targetIndex + 1 }
        MenuItem { text: "Roll Back Before"; onTriggered: panel.timeline.marker = menu.targetIndex }
        MenuSeparator {}
        MenuItem {
            text: "Move Earlier"
            enabled: menu.targetIndex > 0
            onTriggered: panel.app.doc.moveFeature(menu.targetIndex, menu.targetIndex - 1)
        }
        MenuItem {
            text: "Move Later"
            enabled: panel.timeline && menu.targetIndex < panel.timeline.count - 1
            onTriggered: panel.app.doc.moveFeature(menu.targetIndex, menu.targetIndex + 1)
        }
        MenuSeparator {}
        MenuItem {
            text: menu.target && menu.target.visible ? "Hide" : "Show"
            onTriggered: menu.target.visible = !menu.target.visible
        }
        MenuItem { text: "Delete"; onTriggered: panel.app.deleteFeature(menu.target) }
    }
}
