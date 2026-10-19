// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
import QtQuick
import QtQuick.Controls
import "."

// Large icon + caption button used in the toolbar ribbon
AbstractButton {
    id: btn
    property string glyph: "box"
    property string tip: text
    property bool compact: false
    property color glyphColor: Theme.header
    signal rightClicked()

    implicitWidth: compact ? 30 : Math.max(46, label.implicitWidth + 10)
    implicitHeight: compact ? 30 : 52
    hoverEnabled: true
    focusPolicy: Qt.NoFocus

    background: Rectangle {
        radius: 3
        color: btn.checked ? Theme.accentLight : btn.down ? "#cfd7df" : btn.hovered ? Theme.hover : "transparent"
        border.color: btn.checked ? Theme.accent : "transparent"
    }

    contentItem: Item {
        Glyph {
            id: icon
            name: btn.glyph
            color: btn.enabled ? btn.glyphColor : "#a0a6ad"
            width: btn.compact ? 20 : 24
            height: width
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.top
            anchors.topMargin: btn.compact ? 5 : 5
        }
        Text {
            id: label
            visible: !btn.compact
            text: btn.text
            font.family: Theme.fontFamily
            font.pixelSize: 10
            color: btn.enabled ? Theme.text : "#a0a6ad"
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 3
        }
    }

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.RightButton
        onClicked: btn.rightClicked()
    }

    ToolTip.visible: hovered && tip.length > 0
    ToolTip.delay: 600
    ToolTip.text: tip
}
