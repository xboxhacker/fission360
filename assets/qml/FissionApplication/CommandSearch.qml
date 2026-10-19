// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "."

// S: searchable list of every command available in the current mode; Enter or click runs it
Popup {
    id: box
    property var app
    property var matches: []
    property int current: 0
    modal: false
    focus: true
    x: (parent ? parent.width : 800) / 2 - width / 2
    y: 120
    width: 380
    height: Math.min(440, 52 + list.contentHeight)
    padding: 0
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    background: Rectangle { color: Theme.panel; border.color: Theme.border; radius: 5 }

    function available() {
        const sketching = !!app.activeSketchId
        return app.commandCatalog().filter(c => c.mode === "any" || (c.mode === "sketch") === sketching)
    }
    function refresh() {
        const q = query.text.trim().toLowerCase()
        const all = available()
        matches = q ? all.filter(c => (c.name + " " + c.section + " " + c.keys).toLowerCase().indexOf(q) >= 0) : all
        current = 0
    }
    function openSearch() {
        query.text = ""
        refresh()
        open()
        query.forceActiveFocus()
    }
    function search(text) {
        query.text = text
        refresh()
    }
    function run(i) {
        const c = matches[i]
        if (!c) return
        close()
        c.run()
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 6
        spacing: 4

        TextField {
            id: query
            Layout.fillWidth: true
            placeholderText: box.app.activeSketchId ? "Search sketch commands" : "Search commands"
            selectByMouse: true
            onTextEdited: box.refresh()
            Keys.onDownPressed: box.current = Math.min(box.current + 1, box.matches.length - 1)
            Keys.onUpPressed: box.current = Math.max(box.current - 1, 0)
            onAccepted: box.run(box.current)
            background: Rectangle { color: "white"; radius: 3; border.color: query.activeFocus ? Theme.accent : Theme.border }
        }

        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: box.matches
            currentIndex: box.current
            delegate: Rectangle {
                required property var modelData
                required property int index
                width: list.width
                height: 26
                radius: 3
                color: index === box.current ? Theme.accentLight : hover.containsMouse ? Theme.hover : "transparent"
                MouseArea {
                    id: hover
                    anchors.fill: parent
                    hoverEnabled: true
                    onEntered: box.current = index
                    onClicked: box.run(index)
                }
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 8
                    anchors.rightMargin: 8
                    Text { text: modelData.name; color: Theme.text; Layout.fillWidth: true; elide: Text.ElideRight }
                    Text { text: modelData.section; color: Theme.textDim; font.pixelSize: Theme.smallFont }
                    Rectangle {
                        visible: modelData.keys.length > 0
                        Layout.preferredWidth: keyText.implicitWidth + 10
                        Layout.preferredHeight: 18
                        radius: 3
                        color: Theme.panelAlt
                        border.color: Theme.border
                        Text { id: keyText; anchors.centerIn: parent; text: modelData.keys; font.pixelSize: 11; color: Theme.text }
                    }
                }
            }
        }
    }
}
