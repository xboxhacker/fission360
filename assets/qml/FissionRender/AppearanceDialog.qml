// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../FissionApplication"

// Floating Appearance panel (materials + CC0 PBR maps). Hosted in FloatingPanel like Inspect / Properties.
Item {
    id: panel
    property var app
    implicitHeight: 480

    readonly property var library: appearanceLibrary ? appearanceLibrary.appearances() : []
    property int cacheRev: 0
    readonly property var textures: {
        cacheRev
        return renderAssets ? renderAssets.materialCatalog() : []
    }
    // Bind to selection so the hint updates when the user picks bodies
    readonly property int selCount: app && app.selection ? app.selection.count : 0

    function selectedBodyIndices() {
        const idxs = []
        if (!app || !app.selection) return idxs
        for (let i = 0; i < app.selection.count; ++i) {
            const it = app.selection.items[i]
            if (it && it.body !== undefined && idxs.indexOf(it.body) < 0) idxs.push(it.body)
        }
        return idxs
    }

    function selectedFaceCount() {
        if (!app || !app.selection) return 0
        let n = 0
        for (let i = 0; i < app.selection.count; ++i)
            if (app.selection.items[i] && app.selection.items[i].kind === "face") ++n
        return n
    }

    // Prefer selection; if nothing is selected, paint every visible body
    function targetBodyIndices() {
        const idxs = selectedBodyIndices()
        if (idxs.length > 0 || selectedFaceCount() > 0) return idxs
        if (!app || !app.doc) return []
        const all = []
        const n = app.doc.bodyCount()
        for (let i = 0; i < n; ++i) {
            const b = app.doc.body(i)
            if (b && b.visible) all.push(i)
        }
        return all
    }

    function selectionHint() {
        const faces = selectedFaceCount()
        if (faces > 0)
            return "Apply to " + faces + " selected face" + (faces === 1 ? "" : "s")
        const sel = selectedBodyIndices().length
        if (sel > 0)
            return "Apply to " + sel + " selected body" + (sel === 1 ? "" : "ies")
        const vis = targetBodyIndices().length
        if (vis > 0)
            return "No selection — apply to " + vis + " visible body" + (vis === 1 ? "" : "ies")
        return "No visible bodies to paint"
    }

    function applyId(id) {
        if (!app || !app.doc) return
        const faces = selectedFaceCount()
        if (faces > 0) {
            app.doc.applyAppearanceToSelection(id, app.selection.items)
            fissionApp.showMessage("Appearance applied to " + faces + " face" + (faces === 1 ? "" : "s"))
            return
        }
        const idxs = targetBodyIndices()
        if (idxs.length === 0) {
            fissionApp.showMessage("No visible bodies to paint")
            return
        }
        app.doc.applyAppearance(id, idxs)
        fissionApp.showMessage("Appearance applied to " + idxs.length + " body" + (idxs.length === 1 ? "" : "ies"))
    }

    function applyTexture(mat) {
        if (!app || !app.doc) return
        const faces = selectedFaceCount()
        const idxs = faces > 0 ? selectedBodyIndices() : targetBodyIndices()
        if (faces === 0 && idxs.length === 0) {
            fissionApp.showMessage("No visible bodies to paint")
            return
        }
        function paint(paths) {
            const albedo = paths.albedo || ""
            const rough = paths.roughness || ""
            const metal = paths.metalness || ""
            const nor = paths.normal || ""
            app.doc.checkpoint()
            if (faces > 0) {
                for (let i = 0; i < app.selection.count; ++i) {
                    const it = app.selection.items[i]
                    if (!it || it.kind !== "face") continue
                    const b = app.doc.body(it.body)
                    if (!b) continue
                    if (mat.appearanceId) b.applyAppearanceToFace(it.index, mat.appearanceId)
                    b.setFaceMaps(it.index, albedo, rough, metal, nor)
                }
            } else {
                for (let i = 0; i < idxs.length; ++i) {
                    const b = app.doc.body(idxs[i])
                    if (!b) continue
                    if (mat.appearanceId) b.applyAppearance(mat.appearanceId)
                    b.texturePath = albedo
                    b.roughnessPath = rough
                    b.metalnessPath = metal
                    b.normalPath = nor
                }
            }
            fissionApp.showMessage("PBR maps applied (" + mat.name + ")")
            cacheRev++
        }
        if (mat.cached) {
            paint(renderAssets.materialPaths(mat.id))
            return
        }
        const conn = function (kind, id, localPath) {
            if (kind !== "material" || id !== mat.id) return
            renderAssets.downloadFinished.disconnect(conn)
            paint(renderAssets.materialPaths(mat.id))
        }
        renderAssets.downloadFinished.connect(conn)
        renderAssets.downloadMaterial(mat.id)
    }

    Connections {
        target: renderAssets
        function onCacheChanged() { cacheRev++ }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 38
            color: Theme.header
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 8
                Text { text: "Appearance"; color: Theme.headerText; font.bold: true; font.pixelSize: 13; Layout.fillWidth: true }
                ToolbarButton {
                    compact: true
                    glyph: "close"
                    glyphColor: "white"
                    tip: "Close"
                    Layout.preferredWidth: 22
                    Layout.preferredHeight: 22
                    onClicked: if (panel.app) panel.app.showAppearance = false
                }
            }
        }

        Text {
            Layout.fillWidth: true
            Layout.leftMargin: 10
            Layout.rightMargin: 10
            Layout.topMargin: 8
            wrapMode: Text.WordWrap
            color: Theme.textDim
            font.pixelSize: 11
            text: panel.selCount, selectionHint()
        }

        TabBar {
            id: tabs
            Layout.fillWidth: true
            Layout.leftMargin: 8
            Layout.rightMargin: 8
            TabButton { text: "Colours" }
            TabButton { text: "Textures (CC0)" }
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.margins: 8
            currentIndex: tabs.currentIndex

            ListView {
                id: list
                clip: true
                model: library
                spacing: 3
                delegate: Rectangle {
                    width: list.width
                    height: 40
                    radius: 3
                    color: ma.containsMouse ? Theme.hover : Theme.panelAlt
                    border.color: Theme.border
                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 5
                        spacing: 8
                        Rectangle {
                            width: 26; height: 26; radius: 3
                            color: modelData.color
                            opacity: modelData.opacity
                            border.color: "#00000055"
                        }
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 0
                            Text { text: modelData.name; color: Theme.text; font.pixelSize: 12 }
                            Text {
                                text: modelData.type
                                      + "  ·  R " + Number(modelData.roughness).toFixed(2)
                                      + "  M " + Number(modelData.metalness).toFixed(2)
                                color: Theme.textDim; font.pixelSize: 10
                            }
                        }
                    }
                    MouseArea {
                        id: ma
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: applyId(modelData.id)
                    }
                }
            }

            ColumnLayout {
                spacing: 6
                ListView {
                    id: texList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: textures
                    spacing: 3
                    delegate: Rectangle {
                        width: texList.width
                        height: 38
                        radius: 3
                        color: Theme.panelAlt
                        border.color: Theme.border
                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 5
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 0
                                Text { text: modelData.name; color: Theme.text; font.pixelSize: 12 }
                                Text {
                                    text: modelData.cached ? "Cached — click Apply" : "Download & apply (CC0)"
                                    color: Theme.textDim; font.pixelSize: 10
                                }
                            }
                            Button {
                                text: modelData.cached ? "Apply" : "Get"
                                onClicked: applyTexture(modelData)
                            }
                        }
                    }
                }
                Button {
                    text: "Open cache folder"
                    onClicked: renderAssets && renderAssets.openCacheFolder()
                }
            }
        }
    }
}
