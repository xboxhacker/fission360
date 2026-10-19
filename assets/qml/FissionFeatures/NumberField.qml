// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../FissionApplication"

// Expression input bound to one feature property; previews live while typing
ColumnLayout {
    id: nf
    property var dialog
    property var spec: ({})
    readonly property var feature: dialog ? dialog.feature : null
    readonly property string stored: dialog && spec.key ? String(dialog.props[spec.key] ?? "") : ""
    readonly property string unitKind: spec.unit !== undefined ? spec.unit : dialog ? dialog.app.inputKind(spec.key || "") : "length"
    readonly property string typed: dialog ? (dialog.app.lengthUnit, dialog.app.withUnit(field.text, unitKind)) : field.text
    readonly property var result: dialog ? (dialog.app.rev, dialog.app.doc.evaluateExpression(typed)) : ({ value: 0, error: "" })
    spacing: 1

    function commit() {
        if (feature && typed !== stored) feature.set(spec.key, typed)
    }

    // Nudges the leading number with the arrow keys, keeping any unit suffix
    function nudge(delta) {
        const m = field.text.match(/^\s*(-?\d*\.?\d+)(.*)$/)
        if (!m) return
        const v = Math.round((parseFloat(m[1]) + delta) * 1e6) / 1e6
        field.text = v + m[2]
        commit()
    }

    onStoredChanged: if (!field.activeFocus) field.text = stored

    RowLayout {
        Layout.fillWidth: true
        Text {
            Layout.preferredWidth: 96
            text: nf.spec.label || ""
            color: Theme.text
            elide: Text.ElideRight
        }
        TextField {
            id: field
            Layout.fillWidth: true
            text: nf.stored
            selectByMouse: true
            color: nf.result.error.length ? Theme.error : Theme.text
            onTextEdited: debounce.restart()
            onEditingFinished: { debounce.stop(); nf.commit() }
            Keys.onUpPressed: nf.nudge(1)
            Keys.onDownPressed: nf.nudge(-1)
            background: Rectangle {
                color: "white"
                radius: 2
                border.color: field.activeFocus ? Theme.accent : nf.result.error.length ? Theme.error : Theme.border
            }
        }
    }
    Text {
        Layout.leftMargin: 100
        // A plain number (with or without a unit) already says its value; only expressions get a result line
        visible: field.text.length > 0 && (nf.result.error.length > 0
                                           || !/^\s*-?(\d+\.?\d*|\.\d+)\s*(mm|cm|m|in|ft|deg|rad|°)?\s*$/i.test(field.text))
        text: nf.result.error.length ? nf.result.error
            : nf.unitKind === "length" ? "= " + nf.dialog.app.lenText(nf.result.value, 3)
            : nf.unitKind === "angle" ? "= " + (Math.round(nf.result.value * 1000) / 1000) + "\u00b0"
            : "= " + (Math.round(nf.result.value * 1000) / 1000)
        color: nf.result.error.length ? Theme.error : Theme.textDim
        font.pixelSize: Theme.smallFont
    }

    Timer {
        id: debounce
        interval: 350
        onTriggered: nf.commit()
    }
}
