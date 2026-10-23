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
    property string selectedId: ""
    readonly property var timeline: app.doc ? app.doc.timeline : null
    readonly property int itemWidth: 34
    implicitHeight: 46
    color: Theme.panel

    // The wheel only slides the strip (8 chips a notch, Ctrl one). The marker moves with the arrow buttons,
    // a drag of the marker, or Move Marker Here on a chip.
    function revealIndex(i) {
        if (!panel.timeline || i < 0) return
        const x = i * (panel.itemWidth + 2)
        if (x < flick.contentX + 12)
            flick.contentX = Math.max(0, x - 12)
        else if (x + panel.itemWidth > flick.contentX + flick.width - 12)
            flick.contentX = Math.max(0, x + panel.itemWidth - flick.width + 24)
    }
    function featureIdForPick(it) {
        if (!it || !panel.app || !panel.app.doc) return ""
        const k = it.kind
        if (k === "sketchcurve" || k === "profile") return it.sketch || ""
        if (k === "plane" || k === "axis" || k === "point" || k === "feature") return it.id || ""
        if (k === "face" || k === "edge" || k === "body") {
            const b = panel.app.doc.body(it.body)
            if (!b) return ""
            return b.editedBy && b.editedBy.length ? b.editedBy : (b.sourceFeature || "")
        }
        return ""
    }
    function highlightFeature(id) {
        panel.selectedId = id || ""
        if (!panel.timeline || !panel.selectedId.length) return
        const i = panel.timeline.indexOfId(panel.selectedId)
        if (i >= 0) panel.revealIndex(i)
    }

    WheelHandler {
        onWheel: (event) => {
            if (!panel.timeline) return
            let d = event.angleDelta.y !== 0 ? event.angleDelta.y : event.angleDelta.x
            if (d === 0) d = event.pixelDelta.y !== 0 ? event.pixelDelta.y : event.pixelDelta.x
            if (d === 0) return
            const jump = (event.modifiers & Qt.ControlModifier) ? 1 : 8
            const dx = (-d / 120) * jump * (panel.itemWidth + 2)
            const limit = Math.max(0, flick.contentWidth - flick.width)
            flick.contentX = Math.max(0, Math.min(limit, flick.contentX + dx))
        }
    }
    Connections {
        target: panel.timeline
        function onMarkerChanged() { if (panel.timeline) panel.revealIndex(panel.timeline.marker) }
    }
    Connections {
        target: panel.app ? panel.app.selection : null
        function onSelectionChanged() {
            const sel = panel.app.selection
            if (!sel || sel.count < 1) { panel.selectedId = ""; return }
            panel.highlightFeature(panel.featureIdForPick(sel.items[sel.count - 1]))
        }
    }

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
                        readonly property bool picked: feature && feature.id === panel.selectedId
                        // get(), not props: props would copy the Fusion snapshot blob into JS for every chip
                        readonly property string fusionType: feature ? (feature.get("fusionType") || "") : ""
                        readonly property string fusionGroup: feature ? (feature.get("fusionGroup") || "") : ""
                        width: panel.itemWidth
                        height: 34
                        radius: 3
                        color: editing || picked ? Theme.accentLight : chipArea.containsMouse ? Theme.hover : "transparent"
                        border.width: picked ? 2 : 1
                        border.color: error.length ? Theme.error : (editing || picked) ? Theme.accent : "transparent"
                        opacity: active ? 1 : 0.38

                        Glyph {
                            anchors.centerIn: parent
                            width: 24
                            height: 24
                            name: Theme.featureGlyph(chip.type, chip.fusionType, chip.name)
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
                                else panel.highlightFeature(chip.feature ? chip.feature.id : "")
                            }
                            onDoubleClicked: panel.app.editFeature(chip.feature)
                        }
                        ToolTip.visible: chipArea.containsMouse
                        ToolTip.delay: 400
                        ToolTip.text: chip.name + (chip.suppressed ? " (suppressed)" : "")
                                      + (chip.fusionGroup.length ? "\nGroup: " + chip.fusionGroup : "")
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
        MenuItem { text: "Move Marker Here"; onTriggered: panel.timeline.marker = menu.targetIndex + 1 }
        MenuItem { text: "Move Marker Before"; onTriggered: panel.timeline.marker = menu.targetIndex }
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
