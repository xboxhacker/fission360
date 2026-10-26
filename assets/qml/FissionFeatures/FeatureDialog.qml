// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../FissionApplication"

// Data-driven feature form. Subclasses only set `fields`:
//   {kind:"selection", label, key, sel, filter, presets:[[value,label]..], when}
//      sel: profiles | sections | edges | faces | bodies | body | plane | axis | edge | face | point | path | sketch
//           | pull (a plane ref, or else an axis ref: draft pull directions)
//           | origin (a face pick with its clicked point, or an edge: joint origins)
//      hitKey (face): also stores the clicked point there as [x, y, z]
//   {kind:"number", label, key, when}          numeric expression ("10 mm", "width / 2")
//   {kind:"choice", label, key, options:[[value,label]..], when}
//   {kind:"bool", label, key, when}
//   {kind:"vector", label, key, when}           [x, y, z] expressions; labels: [3 labels] = one row each
//   {kind:"types", label, key, options:[[value, tip, glyph]..], onPick: value => .., when}   glyph buttons
//   {kind:"point3", label, key, prompt, then: key, when}   [x, y, z] picked in the canvas (vertex, center,
//      midpoint or face point) or typed; then = the point3 key to ask for next
//   {kind:"note", text, textOf: feature => string, when}
ColumnLayout {
    id: dlg
    property var app
    property var feature
    property var fields: []
    readonly property var props: feature ? feature.props : ({})
    property string activeKey: ""
    property bool syncing: false
    spacing: 8

    readonly property var operations: [["join", "Join"], ["cut", "Cut"], ["intersect", "Intersect"], ["newBody", "New Body"]]
    readonly property var planePresets: [["XY", "XY"], ["XZ", "XZ"], ["YZ", "YZ"]]
    readonly property var axisPresets: [["X", "X"], ["Y", "Y"], ["Z", "Z"]]

    function shown(spec) { return !spec.when || spec.when(props) }

    function setProps(obj) {
        const p = feature.props
        for (const k in obj) p[k] = obj[k]
        feature.props = p
    }

    function featureName(id) {
        const f = app.doc.featureById(id)
        return f ? f.name : "missing"
    }

    function refText(v) {
        if (v === undefined || v === null || v === "") return "None"
        if (typeof v === "string") {
            if (v.indexOf("feature:") === 0) return featureName(v.substring(8))
            if (v.indexOf("sketchline:") === 0) return "Sketch line"
            return v
        }
        if (v.kind === "face") return "Face (Body" + (v.body + 1) + ")"
        if (v.kind === "edge") return "Edge (Body" + (v.body + 1) + ")"
        return v.kind || "?"
    }

    function summary(spec) {
        const v = props[spec.key]
        switch (spec.sel) {
        case "profiles":
            if (isFaceList(props.profiles)) return props.profiles.length + " face(s) \u2014 Body" + (props.profiles[0].body + 1)
            if (!props.sketch) return "Select a profile"
            return ((props.profiles || []).length ? props.profiles.length + " profile(s)" : "Default profiles")
                    + " \u2014 " + featureName(props.sketch)
        case "sections": return (v || []).length + " section(s)"
        case "edges": case "faces": return (v || []).length + " selected"
        case "bodies": return (v || []).length ? v.length + " bod" + (v.length > 1 ? "ies" : "y") : "All bodies"
        case "body": return v === undefined || v < 0 ? "None" : "Body" + (v + 1)
        case "path": return v ? featureName(v) : "Select a sketch curve"
        case "sketch": return v ? featureName(v) : "Select sketch curves"
        case "point": return v ? refText(v) : "Origin"
        case "face": return v ? refText(v) : "Select a face"
        case "plane": return v ? refText(v) : "Select a face or plane"
        case "pull": return v ? refText(v) : "Select a face, plane, edge or axis"
        case "origin": return v ? refText(v) : "Select a face or edge"
        default: return refText(v)
        }
    }

    // Selection refs that represent the current value of a field (so they highlight when it is active)
    function itemsFor(spec) {
        const v = props[spec.key]
        switch (spec.sel) {
        case "profiles":
            if (isFaceList(props.profiles)) return props.profiles
            return (props.profiles || []).map(i => ({ kind: "profile", sketch: props.sketch, index: i }))
        case "sections":
            return (v || []).map(s => s.kind === "face" ? s
                                 : s.profile !== undefined ? { kind: "profile", sketch: s.sketch, index: s.profile } : null)
                            .filter(s => s)
        case "edges": case "faces": return v || []
        case "bodies": return (v || []).map(b => ({ kind: "body", body: b }))
        case "body": return v !== undefined && v >= 0 ? [{ kind: "body", body: v }] : []
        case "point":
            return typeof v === "string" && v.indexOf("feature:") === 0 ? [{ kind: "point", id: v.substring(8) }] : []
        case "origin": return v && typeof v === "object" ? [v] : []
        case "face": case "plane": case "axis": case "edge": case "pull":
            if (v && typeof v === "object") return [v]
            if (typeof v === "string" && v.indexOf("feature:") === 0) {
                const ref = app.doc.featureById(v.substring(8))
                const isAxis = spec.sel === "axis" || (spec.sel === "pull" && !!ref && ref.type === "ConstructionAxis")
                return [{ kind: isAxis ? "axis" : "plane", id: v.substring(8) }]
            }
            if ((spec.sel === "plane" || spec.sel === "pull") && ["XY", "XZ", "YZ"].indexOf(v) >= 0) return [{ kind: "originplane", id: v }]
            if ((spec.sel === "axis" || spec.sel === "pull") && ["X", "Y", "Z"].indexOf(v) >= 0) return [{ kind: "originaxis", id: v }]
            return []
        }
        return []
    }

    // Profile lists hold sketch profile indices or, for a face extrude, face refs
    function isFaceList(list) { return !!list && list.length > 0 && typeof list[0] === "object" }

    // Edge/face fields pick topology the feature itself consumes, so show its input bodies meanwhile
    function consumesTopology(spec) {
        return spec.sel === "edges" || spec.sel === "faces" || spec.sel === "edge" || spec.sel === "face" || !!spec.inputView
            || (spec.sel === "profiles" && (spec.filter || "").indexOf("face") >= 0)
    }

    function setInputFeature(id) { if (app && app.doc) app.doc.inputFeatureId = id }

    function activate(spec) {
        activeKey = spec.key
        setInputFeature(consumesTopology(spec) && feature ? feature.id : "")
        syncing = true
        app.selection.filter = spec.filter || ""
        app.selection.setItems(itemsFor(spec))
        syncing = false
    }

    function removeItem(spec, i) {
        const prev = props[spec.key] || []
        let list = prev.slice()
        list.splice(i, 1)
        if (spec.sel === "edges" && props.tangentChain !== false) list = withTangentChains(prev, list)
        const o = {}
        o[spec.key] = list
        if (list.length) o.body = list[0].body
        setProps(o)
        if (activeKey === spec.key) {
            syncing = true
            app.selection.setItems(list)
            syncing = false
        }
    }

    // The ✕ drops every picked item, including ones that were seeded and are not in the highlight
    function clearField(spec) {
        syncing = true
        app.selection.clear()
        syncing = false
        const o = {}
        if (["sections", "edges", "faces", "bodies", "profiles"].indexOf(spec.sel) >= 0) o[spec.key] = []
        else if (spec.sel === "face" || spec.sel === "origin") o[spec.key] = null
        else if (spec.sel === "sketch" || spec.sel === "point") o[spec.key] = ""
        else if (spec.sel === "body" && spec.presets) o[spec.key] = -1
        else return
        setProps(o)
    }

    function itemLabel(v) {
        const k = v.kind === "edge" ? "Edge " : "Face "
        return k + (v.index + 1) + " \u00b7 Body" + (v.body + 1)
    }

    // Picking an edge adds its whole tangent chain; deselecting any edge of a chain removes the chain
    function withTangentChains(prev, next) {
        const key = r => r.body + "|" + r.index
        const prevKeys = prev.map(key), nextKeys = next.map(key)
        let out = next.slice()
        for (const r of prev.filter(r => nextKeys.indexOf(key(r)) < 0)) {
            const chain = app.doc.tangentChain(r).map(key)
            out = out.filter(x => chain.indexOf(key(x)) < 0)
        }
        for (const r of next.filter(r => prevKeys.indexOf(key(r)) < 0))
            for (const c of app.doc.tangentChain(r))
                if (!out.some(x => key(x) === key(c))) out.push(c)
        return out
    }

    function apply(spec, items) {
        const last = items.length ? items[items.length - 1] : null
        const o = {}
        let resync = null
        switch (spec.sel) {
        case "profiles": {
            // Faces and sketch profiles do not mix: the kind of the latest pick wins
            if (last && last.kind === "face") {
                const faces = items.filter(i => i.kind === "face")
                o.sketch = ""
                o.profiles = faces
                if (faces.length !== items.length) resync = faces
                break
            }
            const pr = items.filter(i => i.kind === "profile")
            if (pr.length !== items.length) resync = pr
            if (!pr.length) { o.profiles = []; break }
            const sk = pr[pr.length - 1].sketch
            o.sketch = sk
            o.profiles = pr.filter(i => i.sketch === sk).map(i => i.index)
            break
        }
        case "sections": {
            const secs = []
            for (const i of items) {
                if (i.kind === "profile") secs.push({ kind: "profile", sketch: i.sketch, profile: i.index })
                else if (i.kind === "face") secs.push(i)
            }
            o[spec.key] = secs
            break
        }
        case "edges": case "faces": {
            const k = spec.sel === "edges" ? "edge" : "face"
            let src = items
            if (spec.sel === "edges" && spec.faceEdges && items.some(i => i.kind === "face")) {
                // A picked face stands for every edge around it
                src = items.filter(i => i.kind === "edge")
                for (const f of items.filter(i => i.kind === "face"))
                    for (const e of app.doc.faceEdges(f))
                        if (!src.some(x => x.body === e.body && x.index === e.index)) src.push(e)
            }
            let list = src.filter(i => i.kind === k)
            if (spec.sel === "edges" && props.tangentChain !== false) list = withTangentChains(props[spec.key] || [], list)
            o[spec.key] = list
            if (list.length) o.body = list[0].body
            if (list.length !== items.length) resync = list
            break
        }
        case "bodies":
            o[spec.key] = app.bodyIndices(items)
            if (spec.autoNext && !(props[spec.key] || []).length && o[spec.key].length) Qt.callLater(() => activateNext(spec))
            break
        case "body":
            if (last && last.body !== undefined) {
                if (spec.autoNext && props[spec.key] !== last.body) Qt.callLater(() => activateNext(spec))
                o[spec.key] = last.body
            }
            break
        case "face":
            if (last && last.kind === "face") {
                o[spec.key] = last
                if (spec.hitKey && last.hit) o[spec.hitKey] = last.hit.map(n => app.lenText(n, 3))
            }
            break
        case "sketch":
            if (last && last.sketch) o[spec.key] = last.sketch
            break
        case "point":
            if (last && last.kind === "point") o[spec.key] = "feature:" + last.id
            else if (last && last.kind === "originpoint") o[spec.key] = ""
            break
        case "plane": {
            const r = app.planeRef(last)
            if (r !== undefined) {
                if (spec.autoNext && JSON.stringify(props[spec.key]) !== JSON.stringify(r)) Qt.callLater(() => activateNext(spec))
                o[spec.key] = r
                if (spec.placeStart && JSON.stringify(props[spec.key]) !== JSON.stringify(r))
                    Qt.callLater(() => releaseForStartPoint())
            }
            break
        }
        case "axis": {
            const r = app.axisRef(last)
            if (r !== undefined) o[spec.key] = r
            break
        }
        case "pull": {
            // Faces (flat or round) and planes, or straight edges, axes and sketch lines
            let r = app.planeRef(last)
            if (r === undefined) r = app.axisRef(last)
            if (r !== undefined) {
                if (spec.autoNext && JSON.stringify(props[spec.key]) !== JSON.stringify(r)) Qt.callLater(() => activateNext(spec))
                o[spec.key] = r
            }
            break
        }
        case "edge":
            if (last && last.kind === "edge") o[spec.key] = last
            break
        case "origin":
            if (last && (last.kind === "face" || last.kind === "edge")) {
                if (spec.autoNext && JSON.stringify(props[spec.key]) !== JSON.stringify(last)) Qt.callLater(() => activateNext(spec))
                o[spec.key] = last
            }
            break
        case "path":
            if (last && last.sketch) o[spec.key] = last.sketch
            break
        }
        if (Object.keys(o).length) setProps(o)
        if (resync) {
            syncing = true
            app.selection.setItems(resync)
            syncing = false
        }
    }

    Connections {
        target: dlg.app ? dlg.app.selection : null
        function onSelectionChanged() {
            if (dlg.syncing || !dlg.feature) return
            const spec = dlg.fields.find(f => f.kind === "selection" && f.key === dlg.activeKey)
            if (spec) dlg.apply(spec, dlg.app.selection.items)
        }
    }

    // autoNext fields hand over to the next selection field once they have something
    function activateNext(spec) {
        const sels = fields.filter(f => f.kind === "selection" && shown(f))
        const i = sels.findIndex(f => f.key === spec.key)
        if (i >= 0 && i + 1 < sels.length && activeKey === spec.key) activate(sels[i + 1])
    }

    function hasValue(v) {
        if (v === undefined || v === null) return false
        if (Array.isArray(v) || typeof v === "string") return v.length > 0
        if (typeof v === "number") return v >= 0
        return true
    }

    // The plane is chosen. The next click on it sets the primitive's start point.
    function releaseForStartPoint() {
        activeKey = ""
        setInputFeature("")
        if (app && app.selection) {
            app.selection.filter = ""
            app.selection.clear()
        }
        fissionApp.showMessage("Click a point on the plane to place the solid")
    }

    function activateFirst() {
        const sels = fields.filter(f => f.kind === "selection" && shown(f))
        let first = sels[0]
        if (first && first.autoNext && hasValue(props[first.key]) && sels.length > 1) first = sels[1]
        if (first) activate(first)
        else { activeKey = ""; setInputFeature(""); app.selection.filter = "*"; app.selection.clear() }
    }
    onFieldsChanged: Qt.callLater(activateFirst)
    Component.onCompleted: Qt.callLater(activateFirst)
    Component.onDestruction: {
        setInputFeature("")
        if (app && app.selection) app.selection.filter = ""
    }

    // ------------------------------------------------------------------ field types

    component Label_: Text {
        Layout.preferredWidth: 96
        color: Theme.text
        elide: Text.ElideRight
    }

    Component {
        id: selectionComp
        ColumnLayout {
            id: sf
            property var spec: ({})
            readonly property bool active: dlg.activeKey === spec.key
            spacing: 3
            RowLayout {
                Layout.fillWidth: true
                Label_ { text: sf.spec.label || "" }
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 26
                    radius: 3
                    color: sf.active ? Theme.accentLight : "white"
                    border.color: sf.active ? Theme.accent : Theme.border
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.left: parent.left
                        anchors.leftMargin: 8
                        anchors.right: clearText.left
                        text: sf.spec.sel ? (dlg.app.rev, dlg.summary(sf.spec)) : ""
                        elide: Text.ElideRight
                        color: Theme.text
                    }
                    Text {
                        id: clearText
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.right: parent.right
                        anchors.rightMargin: 8
                        text: "\u2715"
                        color: Theme.textDim
                        visible: sf.active
                        MouseArea { anchors.fill: parent; anchors.margins: -4; onClicked: dlg.clearField(sf.spec) }
                    }
                    MouseArea {
                        anchors.fill: parent
                        anchors.rightMargin: 24
                        onClicked: dlg.activate(sf.spec)
                    }
                }
            }
            Flow {
                id: chips
                readonly property var items: (dlg.app.rev, (sf.spec.sel === "edges" || sf.spec.sel === "faces")
                                              ? (dlg.props[sf.spec.key] || []) : [])
                visible: items.length > 0
                Layout.fillWidth: true
                Layout.leftMargin: 100
                spacing: 4
                Repeater {
                    model: chips.items
                    Rectangle {
                        required property var modelData
                        required property int index
                        width: chipRow.implicitWidth + 12
                        height: 20
                        radius: 10
                        color: sf.active ? Theme.accentLight : "#eef0f3"
                        border.color: sf.active ? Theme.accent : Theme.border
                        Row {
                            id: chipRow
                            anchors.centerIn: parent
                            spacing: 5
                            Text {
                                text: dlg.itemLabel(modelData)
                                color: Theme.text
                                font.pixelSize: Theme.smallFont
                            }
                            Text {
                                objectName: "chipRemove"
                                text: "\u2715"
                                color: Theme.textDim
                                font.pixelSize: Theme.smallFont
                                MouseArea {
                                    anchors.fill: parent
                                    anchors.margins: -4
                                    onClicked: dlg.removeItem(sf.spec, index)
                                }
                            }
                        }
                    }
                }
            }
            RowLayout {
                visible: !!sf.spec.presets
                Layout.leftMargin: 100
                Layout.fillWidth: true
                spacing: 4
                Repeater {
                    model: sf.spec.presets || []
                    Button {
                        required property var modelData
                        Layout.fillWidth: true
                        Layout.preferredWidth: 40
                        text: modelData[1]
                        highlighted: dlg.props[sf.spec.key] === modelData[0]
                        implicitHeight: 24
                        padding: 4
                        leftPadding: 10
                        rightPadding: 10
                        onClicked: { const o = {}; o[sf.spec.key] = modelData[0]; dlg.setProps(o) }
                    }
                }
            }
        }
    }

    Component { id: numberComp; NumberField { dialog: dlg } }
    Component { id: choiceComp; ChoiceField { dialog: dlg } }

    Component {
        id: boolComp
        RowLayout {
            id: bf
            property var spec: ({})
            Label_ { text: bf.spec.label || "" }
            CheckBox {
                checked: !!dlg.props[bf.spec.key]
                onToggled: dlg.feature.set(bf.spec.key, checked)
            }
            Item { Layout.fillWidth: true }
        }
    }

    Component {
        id: vectorComp
        GridLayout {
            id: vf
            property var spec: ({})
            readonly property bool rows: !!spec.labels
            readonly property var value: {
                const v = dlg.props[spec.key]
                return v && v.length === 3 ? v : ["", "", ""]
            }
            columns: rows ? 2 : 4
            columnSpacing: 5
            rowSpacing: 6
            Label_ { visible: !vf.rows; text: vf.spec.label || "" }
            Repeater {
                model: 3
                RowLayout {
                    required property int index
                    Layout.fillWidth: true
                    Layout.columnSpan: vf.rows ? 2 : 1
                    spacing: 5
                    Label_ { visible: vf.rows; text: vf.rows ? vf.spec.labels[index] : "" }
                    TextField {
                        objectName: "vector_" + vf.spec.key + "_" + index
                        Layout.fillWidth: true
                        Layout.preferredWidth: 50
                        text: String(vf.value[index])
                        selectByMouse: true
                        placeholderText: ["x", "y", "z"][index]
                        onEditingFinished: {
                            const v = vf.value.slice()
                            if (String(v[index]) === text) return
                            v[index] = text
                            for (let k = 0; k < 3; ++k) if (String(v[k]) === "") v[k] = "0"
                            dlg.feature.set(vf.spec.key, v)
                        }
                    }
                }
            }
        }
    }

    Component {
        id: typesComp
        RowLayout {
            id: tf
            property var spec: ({})
            spacing: 2
            Label_ { text: tf.spec.label || "" }
            Repeater {
                model: tf.spec.options || []
                ToolbarButton {
                    required property var modelData
                    objectName: "type_" + modelData[0]
                    compact: true
                    glyph: modelData[2]
                    tip: modelData[1]
                    checkable: true
                    checked: dlg.props[tf.spec.key] === modelData[0]
                    onClicked: {
                        dlg.feature.set(tf.spec.key, modelData[0])
                        if (tf.spec.onPick) tf.spec.onPick(modelData[0])
                    }
                }
            }
            Item { Layout.fillWidth: true }
        }
    }

    // Asks for a point in the canvas and stores it in key; follows on with spec.then
    function pickPoint3(spec) {
        app.requestPoint(spec.prompt || ("Click the " + (spec.label || "point").toLowerCase()), (p, label) => {
            const o = {}
            o[spec.key] = p.map(v => app.lenText(v, 4))
            const also = spec.alsoSet ? spec.alsoSet(props) : ""
            if (also) o[also] = o[spec.key]
            setProps(o)
            fissionApp.showMessage((spec.label || "Point") + ": " + label)
            const next = spec.then ? fields.find(f => f.kind === "point3" && f.key === spec.then) : null
            if (next && shown(next)) pickPoint3(next)
        })
    }

    Component {
        id: point3Comp
        ColumnLayout {
            id: pf
            property var spec: ({})
            readonly property var value: (dlg.app.rev, dlg.props[spec.key] || ["0", "0", "0"])
            readonly property bool picking: !!dlg.app.pointPick
            spacing: 4
            RowLayout {
                Layout.fillWidth: true
                Label_ { text: pf.spec.label || "" }
                Text {
                    Layout.fillWidth: true
                    text: "(" + pf.value.map(v => dlg.app.lenValue(dlg.app.num(v), 2)).join(", ") + ")"
                    color: Theme.textDim
                    elide: Text.ElideRight
                }
                Button {
                    objectName: "pick_" + pf.spec.key
                    text: "Pick"
                    implicitHeight: 24
                    onClicked: dlg.pickPoint3(pf.spec)
                }
            }
        }
    }

    Component {
        id: noteComp
        Text {
            property var spec: ({})
            text: spec.textOf ? (dlg.app.rev, spec.textOf(dlg.feature)) : spec.text || ""
            wrapMode: Text.WordWrap
            color: Theme.textDim
            font.pixelSize: Theme.smallFont
        }
    }

    Repeater {
        model: dlg.fields
        Loader {
            required property int index
            readonly property var spec: dlg.fields[index]
            Layout.fillWidth: true
            visible: !!spec && dlg.shown(spec)
            active: visible
            sourceComponent: {
                switch (spec ? spec.kind : "") {
                case "selection": return selectionComp
                case "number": return numberComp
                case "choice": return choiceComp
                case "bool": return boolComp
                case "vector": return vectorComp
                case "types": return typesComp
                case "point3": return point3Comp
                default: return noteComp
                }
            }
            onLoaded: item.spec = spec
        }
    }
}

