// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "."

// Model browser: document settings, named views, origin, bodies, sketches, construction geometry and parameters
Rectangle {
    id: browser
    property var app
    readonly property var doc: app.doc
    color: Theme.panel

    Rectangle { anchors.right: parent.right; width: 1; height: parent.height; color: Theme.border }

    component Folder: ColumnLayout {
        id: folder
        property string title
        property string glyph: ""
        property int count: -1
        property bool open: true
        property bool showEye: false
        property bool eyeOn: true
        signal eyeToggled()
        default property alias rows: body.data
        spacing: 0
        Layout.fillWidth: true

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 24
            color: headArea.containsMouse ? Theme.hover : "transparent"
            MouseArea { id: headArea; anchors.fill: parent; hoverEnabled: true; onClicked: folder.open = !folder.open }
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 6
                spacing: 4
                Text { text: folder.open ? "\u25be" : "\u25b8"; color: Theme.textDim; font.pixelSize: 11 }
                Glyph {
                    visible: folder.showEye
                    name: folder.eyeOn ? "eye" : "eyeoff"
                    Layout.preferredWidth: 16
                    Layout.preferredHeight: 16
                    MouseArea { anchors.fill: parent; onClicked: folder.eyeToggled() }
                }
                Glyph {
                    visible: folder.glyph.length > 0
                    name: folder.glyph
                    Layout.preferredWidth: 16
                    Layout.preferredHeight: 16
                }
                Text {
                    text: folder.title + (folder.count >= 0 ? "  (" + folder.count + ")" : "")
                    font.bold: true
                    color: Theme.text
                    Layout.fillWidth: true
                }
            }
        }
        ColumnLayout {
            id: body
            visible: folder.open
            spacing: 0
            Layout.fillWidth: true
            Layout.leftMargin: 14
        }
    }

    component Row_: Rectangle {
        id: row
        property string glyph: "box"
        property color glyphColor: Theme.header
        property string label
        property bool showEye: false
        property bool eyeOn: true
        property bool selected: false
        property bool bad: false
        signal clicked()
        signal doubleClicked()
        signal eyeToggled()
        signal contextMenu(real x, real y)
        Layout.fillWidth: true
        Layout.preferredHeight: 22
        color: selected ? Theme.accentLight : rowArea.containsMouse ? Theme.hover : "transparent"
        MouseArea {
            id: rowArea
            anchors.fill: parent
            hoverEnabled: true
            acceptedButtons: Qt.LeftButton | Qt.RightButton
            onClicked: (m) => m.button === Qt.RightButton ? row.contextMenu(m.x, m.y) : row.clicked()
            onDoubleClicked: row.doubleClicked()
        }
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 4
            anchors.rightMargin: 6
            spacing: 5
            Glyph {
                visible: row.showEye
                name: row.eyeOn ? "eye" : "eyeoff"
                Layout.preferredWidth: 16
                Layout.preferredHeight: 16
                MouseArea { anchors.fill: parent; onClicked: row.eyeToggled() }
            }
            Glyph { name: row.glyph; color: row.glyphColor; Layout.preferredWidth: 16; Layout.preferredHeight: 16 }
            Text {
                text: row.label
                color: row.bad ? Theme.error : row.eyeOn ? Theme.text : Theme.textDim
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
        }
    }

    function isSelected(ref) { return (app.selection.count, app.selection.contains(ref)) }

    property int linkRev: 0
    Connections {
        target: browser.doc
        function onLinkedDesignsChanged() { ++browser.linkRev }
        function onRebuilt() { ++browser.linkRev }
    }
    readonly property var linked: (linkRev, app.linkedRevision, doc ? doc.linkedDesigns() : [])
    function linkFor(featureId) { return linked.find(l => l.featureId === featureId) || null }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 28
            color: Theme.panelAlt
            Text {
                anchors.verticalCenter: parent.verticalCenter
                anchors.left: parent.left
                anchors.leftMargin: 8
                text: "BROWSER"
                font.bold: true
                font.pixelSize: 11
                color: Theme.textDim
            }
        }

        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentWidth: availableWidth

            ColumnLayout {
                width: browser.width - 2
                spacing: 2

                Row_ {
                    glyph: "box"
                    label: browser.doc ? browser.doc.name + (browser.doc.modified ? "  (unsaved)" : "") : ""
                    Layout.topMargin: 4
                    onDoubleClicked: renameField.visible = true
                    onContextMenu: (x, y) => docMenu.popup()
                }
                TextField {
                    id: renameField
                    visible: false
                    Layout.fillWidth: true
                    Layout.leftMargin: 6
                    Layout.rightMargin: 6
                    text: browser.doc ? browser.doc.name : ""
                    onVisibleChanged: if (visible) { forceActiveFocus(); selectAll() }
                    onAccepted: { browser.doc.name = text; visible = false }
                    onActiveFocusChanged: if (!activeFocus) visible = false
                }

                Folder {
                    title: "Document Settings"
                    glyph: "settings"
                    open: false
                    Row_ {
                        objectName: "unitsRow"
                        glyph: "units"
                        glyphColor: "#d89a2b"
                        label: browser.doc ? "Units: " + browser.doc.lengthUnit + ", " + browser.doc.massUnit : ""
                        onClicked: browser.app.openDocumentSettings()
                        onDoubleClicked: browser.app.openDocumentSettings()
                        onContextMenu: (x, y) => unitsMenu.popup()
                    }
                    Row_ {
                        glyph: "design"
                        label: "Part Design"
                        onDoubleClicked: browser.app.openDocumentSettings()
                    }
                    Row_ {
                        glyph: "properties"
                        label: "Properties..."
                        onClicked: browser.app.openDocumentSettings()
                    }
                }

                Folder {
                    title: "Named Views"
                    glyph: "view"
                    open: false
                    Repeater {
                        model: [["home", "Home"], ["top", "Top"], ["front", "Front"], ["right", "Right"],
                                ["bottom", "Bottom"], ["back", "Back"], ["left", "Left"]]
                        Row_ {
                            required property var modelData
                            glyph: "view"
                            label: modelData[1]
                            onClicked: {
                                browser.app.camera.setView(modelData[0], true)
                                browser.app.viewport.renderer.fitAll(true)
                            }
                        }
                    }
                }

                Folder {
                    objectName: "analysisFolder"
                    title: "Analysis"
                    glyph: "section"
                    visible: !!browser.doc && browser.doc.analyses.length > 0
                    count: browser.doc ? browser.doc.analyses.length : 0
                    showEye: true
                    eyeOn: !!browser.doc && browser.doc.analyses.some(a => a.visible)
                    onEyeToggled: {
                        const list = browser.doc.analyses
                        if (eyeOn) { for (const a of list) if (a.visible) browser.doc.updateAnalysis(a.id, { visible: false }) }
                        else if (list.length) browser.doc.updateAnalysis(list[list.length - 1].id, { visible: true })
                    }
                    Repeater {
                        model: browser.doc ? browser.doc.analyses : []
                        Row_ {
                            required property var modelData
                            glyph: "section"
                            glyphColor: "#d9534f"
                            label: modelData.name
                            showEye: true
                            eyeOn: !!modelData.visible
                            selected: browser.app.inspectMode === "section" && browser.app.sectionId === modelData.id
                            onEyeToggled: browser.doc.updateAnalysis(modelData.id, { visible: !eyeOn })
                            onDoubleClicked: browser.app.editSection(modelData.id)
                            onContextMenu: (x, y) => analysisMenu.openFor(modelData)
                        }
                    }
                }

                Folder {
                    title: "Manufacture"
                    glyph: "settings"
                    visible: {
                        if (!browser.doc || !browser.doc.manufacture) return false
                        const m = browser.doc.manufacture
                        return (m.setups && m.setups.length) || (m.operations && m.operations.length)
                    }
                    count: {
                        if (!browser.doc || !browser.doc.manufacture) return 0
                        const m = browser.doc.manufacture
                        return (m.setups ? m.setups.length : 0) + (m.operations ? m.operations.length : 0)
                    }
                    Repeater {
                        model: browser.doc && browser.doc.manufacture ? (browser.doc.manufacture.setups || []) : []
                        Row_ {
                            required property var modelData
                            glyph: "plane"
                            glyphColor: Theme.accent
                            label: modelData.name + (modelData.id === (browser.doc.manufacture.activeSetupId || "") ? " ●" : "")
                            selected: browser.app.showManufacture
                            onClicked: {
                                browser.doc.setActiveMfgSetup(modelData.id)
                                browser.app.openManufacture(0)
                            }
                            onDoubleClicked: browser.app.openManufacture(0)
                        }
                    }
                    Repeater {
                        model: browser.doc && browser.doc.manufacture ? (browser.doc.manufacture.operations || []) : []
                        Row_ {
                            required property var modelData
                            glyph: "extrude"
                            glyphColor: "#6b8e23"
                            label: modelData.name
                            onClicked: browser.app.openManufacture(2)
                            onDoubleClicked: browser.app.openManufacture(2)
                        }
                    }
                }

                Folder {
                    title: "Origin"
                    open: false
                    showEye: true
                    eyeOn: Object.keys(browser.app.originVisible).some(k => browser.app.originVisible[k])
                    onEyeToggled: browser.app.setAllOriginVisible(!eyeOn)
                    Repeater {
                        // [id, label, ref kind]
                        model: [["O", "O (Origin)", "originpoint"], ["X", "X Axis", "originaxis"], ["Y", "Y Axis", "originaxis"],
                                ["Z", "Z Axis", "originaxis"], ["XY", "XY Plane", "originplane"], ["XZ", "XZ Plane", "originplane"],
                                ["YZ", "YZ Plane", "originplane"]]
                        Row_ {
                            required property var modelData
                            readonly property var ref: ({ kind: modelData[2], id: modelData[0] })
                            glyph: modelData[2] === "originplane" ? "plane" : modelData[2]
                            glyphColor: modelData[0] === "X" ? "#e14646" : modelData[0] === "Y" ? "#3fae4f"
                                      : modelData[0] === "Z" ? "#4f82eb" : Theme.construction
                            label: modelData[1]
                            showEye: true
                            eyeOn: !!browser.app.originVisible[modelData[0]]
                            selected: browser.isSelected(ref)
                            onEyeToggled: browser.app.setOriginVisible(modelData[0], !eyeOn)
                            onClicked: {
                                if (browser.app.pickingSketchPlane) {
                                    if (modelData[2] === "originplane") browser.app.createSketch(modelData[0])
                                    return
                                }
                                if (browser.app.commandFeatureId) browser.app.selection.toggle(ref)
                                else browser.app.selection.select(ref, false)
                            }
                        }
                    }
                }

                Folder {
                    objectName: "linkedFolder"
                    title: "Linked Designs"
                    glyph: "design"
                    visible: browser.linked.length > 0
                    count: browser.linked.length
                    Repeater {
                        model: browser.linked
                        Row_ {
                            required property var modelData
                            glyph: "design"
                            glyphColor: modelData.outOfDate ? "#d89a2b" : Theme.accent
                            label: modelData.name + (modelData.missing ? "  (original missing)"
                                                     : modelData.outOfDate ? "  (out of date)" : "")
                            bad: modelData.missing
                            onDoubleClicked: if (modelData.outOfDate) browser.app.updateLinkedDesign(modelData.featureId)
                            onContextMenu: (x, y) => linkMenu.openFor(modelData)
                        }
                    }
                }

                Folder {
                    title: "Bodies"
                    count: browser.doc ? browser.doc.bodies.length : 0
                    Repeater {
                        model: browser.doc ? browser.doc.bodies : []
                        Row_ {
                            required property var modelData
                            required property int index
                            glyph: modelData.isMesh ? "meshbody" : "body"
                            label: modelData.name
                            showEye: true
                            eyeOn: modelData.visible
                            selected: browser.isSelected({ kind: "body", body: index })
                            onEyeToggled: modelData.visible = !modelData.visible
                            onClicked: {
                                const ref = { kind: "body", body: index }
                                if (browser.app.commandFeatureId) browser.app.selection.toggle(ref)
                                else browser.app.selection.select(ref, false)
                            }
                            onContextMenu: (x, y) => bodyMenu.openFor(index, modelData)
                        }
                    }
                }

                Folder {
                    title: "Sketches"
                    Repeater {
                        model: browser.doc ? browser.doc.timeline : null
                        Row_ {
                            required property var feature
                            required property string type
                            required property string name
                            required property string error
                            required property bool active
                            visible: type === "Sketch"
                            glyph: "sketch"
                            glyphColor: Theme.sketch
                            label: name + (active ? "" : "  (rolled back)")
                            bad: error.length > 0
                            showEye: true
                            eyeOn: feature.visible
                            selected: browser.app.activeSketchId === feature.id
                            onEyeToggled: feature.visible = !feature.visible
                            onDoubleClicked: browser.app.editSketch(feature.id, false)
                            onContextMenu: (x, y) => featureMenu.openFor(feature, this)
                        }
                    }
                }

                Folder {
                    title: "Construction"
                    Repeater {
                        model: browser.doc ? browser.doc.timeline : null
                        Row_ {
                            required property var feature
                            required property string type
                            required property string name
                            required property string error
                            visible: type.indexOf("Construction") === 0
                            glyph: Theme.featureGlyph(type)
                            glyphColor: Theme.construction
                            label: name
                            bad: error.length > 0
                            showEye: true
                            eyeOn: feature.visible
                            selected: browser.isSelected({ kind: type === "ConstructionPlane" ? "plane" : type === "ConstructionAxis" ? "axis" : "point", id: feature.id })
                            onEyeToggled: feature.visible = !feature.visible
                            onClicked: {
                                const ref = { kind: type === "ConstructionPlane" ? "plane" : type === "ConstructionAxis" ? "axis" : "point", id: feature.id }
                                if (browser.app.pickingSketchPlane && ref.kind === "plane") { browser.app.createSketch("feature:" + feature.id); return }
                                if (browser.app.commandFeatureId) browser.app.selection.toggle(ref)
                                else browser.app.selection.select(ref, false)
                            }
                            onDoubleClicked: browser.app.editFeature(feature)
                            onContextMenu: (x, y) => featureMenu.openFor(feature, this)
                        }
                    }
                }

                Folder {
                    title: "Parameters"
                    count: browser.doc ? browser.doc.parameters.length : 0
                    Repeater {
                        model: browser.doc ? browser.doc.parameters : []
                        Row_ {
                            required property var modelData
                            glyph: "parameters"
                            label: modelData.name + " = " + modelData.expression
                            bad: modelData.error.length > 0
                            onDoubleClicked: browser.app.openParameters()
                        }
                    }
                    Row_ {
                        glyph: "parameters"
                        glyphColor: Theme.accent
                        label: "Change Parameters..."
                        onClicked: browser.app.openParameters()
                    }
                }

                Item { Layout.fillHeight: true }
            }
        }
    }

    Menu {
        id: featureMenu
        property var target: null
        function openFor(f, item) { target = f; popup() }
        MenuItem { text: "Edit"; onTriggered: browser.app.editFeature(featureMenu.target) }
        MenuItem {
            text: featureMenu.target && featureMenu.target.visible ? "Hide" : "Show"
            onTriggered: featureMenu.target.visible = !featureMenu.target.visible
        }
        MenuItem { text: "Delete"; onTriggered: browser.app.deleteFeature(featureMenu.target) }
    }

    Menu {
        id: bodyMenu
        property int index: -1
        property var target: null
        property var link: null
        function openFor(i, b) { index = i; target = b; link = browser.linkFor(b.sourceFeature); popup() }
        MenuItem {
            text: "Unlink" + (bodyMenu.link ? " " + bodyMenu.link.sourceName : "")
            visible: !!bodyMenu.link
            height: visible ? implicitHeight : 0
            onTriggered: browser.app.unlinkDesign(bodyMenu.link.featureId)
        }
        MenuItem {
            text: "Update from Original"
            visible: !!bodyMenu.link && bodyMenu.link.outOfDate
            height: visible ? implicitHeight : 0
            onTriggered: browser.app.updateLinkedDesign(bodyMenu.link.featureId)
        }
        MenuItem { text: "Save As Mesh..."; onTriggered: browser.app.saveBodyAsMesh(bodyMenu.index) }
        MenuItem { text: "Remove"; onTriggered: browser.app.removeBodies([bodyMenu.index]) }
        MenuItem {
            text: bodyMenu.target && bodyMenu.target.visible ? "Hide" : "Show"
            onTriggered: bodyMenu.target.visible = !bodyMenu.target.visible
        }
        Menu {
            title: "Color"
            Repeater {
                model: [["Default", "#aab2bd"], ["Steel", "#8c96a3"], ["Graphite", "#4f555c"], ["Red", "#c8504a"],
                        ["Orange", "#e0903c"], ["Yellow", "#d9c24a"], ["Green", "#5fa86a"], ["Blue", "#4f86c6"],
                        ["Purple", "#8a6cc0"], ["White", "#eceff2"]]
                MenuItem {
                    required property var modelData
                    text: modelData[0]
                    checkable: true
                    checked: !!bodyMenu.target && bodyMenu.target.color.toString() === modelData[1]
                    onTriggered: bodyMenu.target.color = modelData[1]
                }
            }
        }
        Menu {
            title: "Opacity"
            Repeater {
                model: [100, 75, 50, 25]
                MenuItem {
                    required property int modelData
                    text: modelData + " %"
                    checkable: true
                    checked: !!bodyMenu.target && Math.abs(bodyMenu.target.opacity * 100 - modelData) < 1
                    onTriggered: bodyMenu.target.opacity = modelData / 100
                }
            }
        }
    }

    Menu {
        id: linkMenu
        property var target: null
        function openFor(l) { target = l; popup() }
        MenuItem {
            text: "Update from Original"
            enabled: !!linkMenu.target && linkMenu.target.outOfDate
            onTriggered: browser.app.updateLinkedDesign(linkMenu.target.featureId)
        }
        MenuItem {
            text: "Open Original"
            enabled: !!linkMenu.target && !linkMenu.target.missing
            onTriggered: browser.app.openLinkedOriginal(linkMenu.target.path)
        }
        MenuSeparator {}
        MenuItem {
            text: "Unlink"
            onTriggered: browser.app.unlinkDesign(linkMenu.target.featureId)
        }
        MenuItem {
            text: "Delete Insert"
            onTriggered: {
                const f = browser.doc.featureById(linkMenu.target.featureId)
                if (f) browser.app.deleteFeature(f)
            }
        }
    }

    Menu {
        id: analysisMenu
        property var target: null
        function openFor(a) { target = a; popup() }
        MenuItem { text: "Edit"; onTriggered: browser.app.editSection(analysisMenu.target.id) }
        MenuItem {
            text: analysisMenu.target && analysisMenu.target.visible ? "Hide" : "Show"
            onTriggered: browser.doc.updateAnalysis(analysisMenu.target.id, { visible: !analysisMenu.target.visible })
        }
        MenuItem { text: "Delete"; onTriggered: browser.doc.removeAnalysis(analysisMenu.target.id) }
    }

    Menu {
        id: unitsMenu
        Instantiator {
            model: browser.doc ? browser.doc.lengthUnits() : []
            delegate: MenuItem {
                required property string modelData
                text: modelData
                checkable: true
                checked: !!browser.doc && browser.doc.lengthUnit === modelData
                onTriggered: browser.app.setLengthUnit(modelData)
            }
            onObjectAdded: (i, item) => unitsMenu.insertItem(i, item)
            onObjectRemoved: (i, item) => unitsMenu.removeItem(item)
        }
        MenuSeparator {}
        MenuItem { text: "Document Settings..."; onTriggered: browser.app.openDocumentSettings() }
    }

    Menu {
        id: docMenu
        MenuItem { text: "Rename"; onTriggered: renameField.visible = true }
        MenuItem { text: "Document Settings..."; onTriggered: browser.app.openDocumentSettings() }
        MenuItem { text: "Save"; onTriggered: browser.app.saveOrSaveAs() }
    }
}
