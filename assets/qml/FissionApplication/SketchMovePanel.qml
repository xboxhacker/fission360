// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "."

// MOVE/COPY inside a sketch: the curves selected when M was pressed move on their own
Item {
    id: mp
    property var app
    readonly property var st: (app.sketchMoveRev, app.sketchMove)
    implicitHeight: col.implicitHeight

    readonly property var types: [["free", "Free move", "movefree"], ["translate", "Translate", "movetranslate"],
                                  ["rotate", "Rotate", "moverotate"], ["pointToPoint", "Point to point", "movepointtopoint"]]

    function fieldText(v, angle) { return angle ? (Math.round(v * 1000) / 1000) + " deg" : app.lenText(v, 4) }
    function commit(key, text, angle) {
        const r = app.doc.evaluateExpression(app.withUnit(text, angle ? "angle" : "length"))
        if (r.error) { fissionApp.showMessage(r.error); return }
        const o = {}
        o[key] = r.value
        app.setSketchMove(o)
    }
    function pickFrom() {
        app.requestPoint("Point to point: click the point to move from", (p) => {
            const from = p
            app.requestPoint("Now click where it goes", (q) => {
                app.setSketchMove({ du: q[0] - from[0], dv: q[1] - from[1], angle: 0 })
            }, true)
        }, true)
    }

    ColumnLayout {
        id: col
        width: parent.width
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 38
            color: Theme.header
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                Glyph { name: "move"; color: Theme.headerText; Layout.preferredWidth: 20; Layout.preferredHeight: 20 }
                Text {
                    Layout.fillWidth: true
                    text: "MOVE/COPY"
                    color: Theme.headerText
                    font.bold: true
                    font.pixelSize: 13
                }
                ToolbarButton {
                    compact: true
                    glyph: "close"
                    glyphColor: Theme.headerText
                    tip: "Cancel (Esc)"
                    Layout.preferredWidth: 22
                    Layout.preferredHeight: 22
                    onClicked: mp.app.finishSketchMove(false)
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.margins: 12
            spacing: 8

            RowLayout {
                Text { text: "Objects"; color: Theme.text; Layout.preferredWidth: 96 }
                Text {
                    Layout.fillWidth: true
                    text: !mp.st ? "" : mp.st.entities.length ? mp.st.entities.length + " sketch curve(s) - click to add / remove"
                                                              : "Click the sketch curves to move"
                    color: mp.st && !mp.st.entities.length ? Theme.accent : Theme.text
                    wrapMode: Text.WordWrap
                }
            }
            RowLayout {
                spacing: 2
                Text { text: "Move type"; color: Theme.text; Layout.preferredWidth: 96 }
                Repeater {
                    model: mp.types
                    ToolbarButton {
                        required property var modelData
                        objectName: "sketchMoveType_" + modelData[0]
                        compact: true
                        glyph: modelData[2]
                        tip: modelData[1]
                        checkable: true
                        checked: !!mp.st && mp.st.type === modelData[0]
                        onClicked: {
                            mp.app.setSketchMove({ type: modelData[0] })
                            if (modelData[0] === "pointToPoint") mp.pickFrom()
                        }
                    }
                }
                Item { Layout.fillWidth: true }
            }
            Repeater {
                model: [["du", "X distance", false, ["free", "translate", "pointToPoint"]],
                        ["dv", "Y distance", false, ["free", "translate", "pointToPoint"]],
                        ["angle", "Angle", true, ["free", "rotate"]]]
                RowLayout {
                    required property var modelData
                    visible: !!mp.st && modelData[3].indexOf(mp.st.type) >= 0
                    Text { text: modelData[1]; color: Theme.text; Layout.preferredWidth: 96 }
                    TextField {
                        Layout.fillWidth: true
                        selectByMouse: true
                        text: mp.st ? mp.fieldText(mp.st[modelData[0]], modelData[2]) : ""
                        onEditingFinished: if (mp.st && text !== mp.fieldText(mp.st[modelData[0]], modelData[2]))
                                               mp.commit(modelData[0], text, modelData[2])
                    }
                }
            }
            RowLayout {
                visible: !!mp.st && (mp.st.type === "rotate" || mp.st.type === "free")
                Text { text: "Pivot"; color: Theme.text; Layout.preferredWidth: 96 }
                Text {
                    Layout.fillWidth: true
                    text: mp.st ? "(" + mp.app.lenValue(mp.st.pu, 2) + ", " + mp.app.lenValue(mp.st.pv, 2) + ")" : ""
                    color: Theme.textDim
                }
                Button {
                    text: "Set Pivot"
                    onClicked: mp.app.requestPoint("Click the pivot point", (p) =>
                        mp.app.setSketchMove({ pu: p[0] - mp.st.du, pv: p[1] - mp.st.dv, pivotSet: true }), true)
                }
            }
            Button {
                visible: !!mp.st && mp.st.type === "pointToPoint"
                text: "Pick the two points again"
                onClicked: mp.pickFrom()
            }
            CheckBox {
                text: "Create copy"
                checked: !!mp.st && mp.st.copy
                onToggled: mp.app.setSketchMove({ copy: checked })
            }
            Text {
                Layout.fillWidth: true
                visible: !!mp.st && mp.st.error.length > 0
                text: mp.st ? mp.st.error : ""
                color: "#c0392b"
                wrapMode: Text.WordWrap
            }
            Text {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: Theme.textDim
                font.pixelSize: Theme.smallFont
                text: "Only the selected curves move: their ties to curves left behind are dropped. Drag the arrows "
                      + "or the square to slide, the ring to turn (Shift = 15\u00b0 steps)."
            }
        }

        Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: Theme.border }
        RowLayout {
            Layout.fillWidth: true
            Layout.margins: 10
            Item { Layout.fillWidth: true }
            Button { text: "OK"; highlighted: true; onClicked: mp.app.finishSketchMove(true) }
            Button { text: "Cancel"; onClicked: mp.app.finishSketchMove(false) }
        }
    }
}
