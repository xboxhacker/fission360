// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../FissionApplication"

// Drop-down bound to one feature property. spec.options = [[value, label], ...], or spec.optionsOf(props)
// for lists that depend on other fields; spec.onPick(value) runs after a pick
RowLayout {
    id: cf
    property var dialog
    property var spec: ({})
    readonly property var options: spec.optionsOf && dialog ? spec.optionsOf(dialog.props) : (spec.options || [])
    readonly property var current: dialog && spec.key ? dialog.props[spec.key] : undefined

    Text {
        Layout.preferredWidth: 96
        text: cf.spec.label || ""
        color: Theme.text
        elide: Text.ElideRight
    }
    ComboBox {
        id: combo
        Layout.fillWidth: true
        implicitHeight: 28
        model: cf.options.map(o => o[1])
        currentIndex: Math.max(0, cf.options.findIndex(o => o[0] === cf.current))
        onActivated: (i) => {
            const v = cf.options[i][0]
            cf.dialog.feature.set(cf.spec.key, v)
            if (cf.spec.onPick) cf.spec.onPick(v)
        }
        background: Rectangle {
            color: combo.hovered ? "#f4f8fb" : "white"
            radius: 2
            border.color: combo.activeFocus || combo.popup.visible ? Theme.accent : Theme.border
        }
        indicator: Text {
            x: combo.width - width - 8
            anchors.verticalCenter: parent.verticalCenter
            text: "\u25be"
            color: Theme.textDim
        }
        contentItem: Text {
            leftPadding: 8
            text: combo.displayText
            color: Theme.text
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
    }
}
