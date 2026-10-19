// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import "../FissionApplication"

// Floating Render settings panel (HDRI / lighting / offline capture). Live shading stays in the viewport —
// this panel does not have to be open for Live Render. Hosted in FloatingPanel like Appearance.
Item {
    id: panel
    property var app
    implicitHeight: 520

    readonly property var settings: (app && app.doc) ? app.doc.renderSettings : null
    readonly property var lightPresets: renderEnvironments ? renderEnvironments.presets() : []
    readonly property var hdris: renderEnvironments ? renderEnvironments.polyHavenHdris() : []
    readonly property var resolutions: renderAssets ? renderAssets.hdriResolutions() : []
    property string selectedHdriId: ""
    property string downloadRes: "2k"
    property int cacheRev: 0
    // 0 = viewport supersample, 1 = Monte Carlo path tracer (offline export only)
    property int renderEngine: 1

    function close() {
        if (app) app.showRender = false
    }

    function applyPreset(p) {
        if (!settings || !p) return
        settings.sceneBackgroundType = p.sceneBackgroundType
        settings.backgroundColor = p.backgroundColor
        settings.lightIntensity = p.lightIntensity
        settings.lightAngle = p.lightAngle
        settings.lightAltitude = p.lightAltitude
        settings.lightColor = p.lightColor
        settings.useSunLight = !!p.useSunLight
        if (app && app.viewport && app.viewport.renderer) {
            if (p.sceneBackgroundType === "solid") {
                app.viewport.renderer.backgroundTop = p.backgroundColor
                app.viewport.renderer.backgroundBottom = p.backgroundColor
            } else if (p.sceneBackgroundType === "gradient") {
                app.viewport.renderer.backgroundTop = Qt.lighter(p.backgroundColor, 1.15)
                app.viewport.renderer.backgroundBottom = Qt.darker(p.backgroundColor, 1.25)
            }
        }
    }

    function hdriStatus(id) {
        cacheRev
        return renderAssets ? renderAssets.statusForHdri(id) : ({})
    }

    function useLocalHdri(id) {
        if (!settings || !renderAssets) return
        const st = renderAssets.statusForHdri(id)
        if (!st.bestPath) {
            fissionApp.showMessage("Download this HDRI first (pick a resolution below)")
            return
        }
        settings.sceneBackgroundType = "environment"
        settings.environmentPath = st.bestPath
        fissionApp.showMessage("Using " + id + " (" + st.bestResolution + ") — live in the viewport")
        selectedHdriId = id
        cacheRev++
        if (app) app.ensureLiveRenderLighting()
    }

    function downloadSelected() {
        if (!selectedHdriId) {
            fissionApp.showMessage("Select an HDRI in the list first")
            return
        }
        renderAssets.downloadHdri(selectedHdriId, downloadRes)
    }

    Connections {
        target: renderAssets
        function onDownloadFinished(kind, id, localPath) {
            cacheRev++
            if (kind === "hdri" && settings) {
                settings.sceneBackgroundType = "environment"
                settings.environmentPath = localPath
                fissionApp.showMessage("HDRI ready — live in the viewport")
                if (app) {
                    app.liveRender = true
                    app.ensureLiveRenderLighting()
                }
            }
        }
        function onCacheChanged() { cacheRev++ }
    }

    FileDialog {
        id: saveDlg
        title: "Export render image"
        fileMode: FileDialog.SaveFile
        nameFilters: {
            const f = settings ? settings.outputFormat : "png"
            if (f === "jpg") return ["JPEG (*.jpg *.jpeg)"]
            if (f === "tiff") return ["TIFF (*.tif *.tiff)"]
            if (f === "bmp") return ["BMP (*.bmp)"]
            return ["PNG (*.png)"]
        }
        onAccepted: {
            const local = fissionApp.toLocalPath(selectedFile.toString())
            if (!app || !app.doc) return
            if (renderEngine === 1) {
                const cam = app.camera
                const camera = {
                    eye: cam ? [cam.eye.x, cam.eye.y, cam.eye.z] : [200, -200, 150],
                    target: cam ? [cam.target.x, cam.target.y, cam.target.z] : [0, 0, 0],
                    up: [0, 0, 1],
                    fov: cam ? cam.fieldOfView : 30,
                    ortho: cam ? cam.orthographic : false
                }
                // Ensure the chosen format matches the dialog filter (QImage::save needs an extension).
                let outPath = local
                const fmt = settings ? settings.outputFormat : "png"
                const ext = "." + (fmt === "jpg" ? "jpg" : fmt === "tiff" ? "tif" : fmt)
                if (!outPath.toLowerCase().endsWith(ext) && !outPath.toLowerCase().endsWith("." + fmt)
                        && !(fmt === "tiff" && outPath.toLowerCase().endsWith(".tiff"))
                        && !(fmt === "jpg" && outPath.toLowerCase().endsWith(".jpeg")))
                    outPath += ext
                const out = app.doc.pathTraceToFile(outPath, camera)
                if (out) {
                    const rid = "render-" + Date.now()
                    app.doc.linkAsset("render", rid, "Path trace", out)
                    fissionApp.showMessage("Path traced → " + out + " (linked to design)")
                } else {
                    fissionApp.showMessage(app.doc.lastError || "Path trace failed")
                }
                return
            }
            if (!app.viewport) return
            const q = settings ? settings.renderQuality : 1
            const scale = [1.0, 1.5, 2.0, 3.0][Math.max(0, Math.min(3, q))]
            const tw = Math.round((settings ? settings.outputWidth : 1920) * scale)
            const th = Math.round((settings ? settings.outputHeight : 1080) * scale)
            fissionApp.beginTask("offline-render", "Exporting…")
            app.viewport.grabToImage(function (result) {
                const ok = result.saveToFile(local)
                fissionApp.endTask("offline-render")
                if (ok) {
                    const rid = "render-" + Date.now()
                    app.doc.linkAsset("render", rid, "Viewport render", local)
                    fissionApp.showMessage("Exported " + tw + "×" + th + " → " + local + " (linked)")
                } else {
                    fissionApp.showMessage("Could not write " + local)
                }
            }, Qt.size(tw, th))
        }
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
                Text {
                    text: "Render Settings"
                    color: Theme.headerText
                    font.bold: true
                    font.pixelSize: 13
                    Layout.fillWidth: true
                }
                ToolbarButton {
                    compact: true
                    glyph: "close"
                    glyphColor: "white"
                    tip: "Close"
                    Layout.preferredWidth: 22
                    Layout.preferredHeight: 22
                    z: 2
                    onClicked: panel.close()
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
            text: "Live shading is in the design viewport (Live Render). This panel is for HDRI, lighting, and exporting an image."
        }

        TabBar {
            id: tabs
            Layout.fillWidth: true
            Layout.leftMargin: 8
            Layout.rightMargin: 8
            Layout.topMargin: 6
            TabButton { text: "Quality" }
            TabButton { text: "Scene" }
            TabButton { text: "Lighting" }
            TabButton { text: "Export" }
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.margins: 10
            Layout.topMargin: 4
            currentIndex: tabs.currentIndex

            ColumnLayout {
                spacing: 8
                Label { text: "Export engine"; font.bold: true; color: Theme.text }
                ComboBox {
                    Layout.fillWidth: true
                    model: ["Viewport (supersample)", "Path Trace (Monte Carlo)"]
                    currentIndex: renderEngine
                    onActivated: (i) => { renderEngine = i }
                }
                Label { text: "Quality"; font.bold: true; color: Theme.text }
                ComboBox {
                    Layout.fillWidth: true
                    model: renderEngine === 1
                           ? ["Low (8 spp)", "Medium (32 spp)", "High (96 spp)", "Ultra (256 spp)"]
                           : ["Low (1×)", "Medium (1.5× supersample)", "High (2×)", "Ultra (3×)"]
                    currentIndex: settings ? settings.renderQuality : 1
                    onActivated: (i) => { if (settings) settings.renderQuality = i }
                }
                Label {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    color: Theme.textDim
                    text: renderEngine === 1
                          ? "Used only when you Export Image. Live viewport uses sun / HDRI from Lighting."
                          : "Used only when you Export Image (supersampled grab of the lit viewport)."
                }
                Item { Layout.fillHeight: true }
            }

            ColumnLayout {
                spacing: 6
                Label { text: "Background"; font.bold: true; color: Theme.text }
                ComboBox {
                    Layout.fillWidth: true
                    model: ["Solid Color", "Gradient", "Environment", "Image"]
                    currentIndex: {
                        if (!settings) return 0
                        const m = { "solid": 0, "gradient": 1, "environment": 2, "image": 3 }
                        return m[settings.sceneBackgroundType] || 0
                    }
                    onActivated: (i) => {
                        if (settings) settings.sceneBackgroundType = ["solid", "gradient", "environment", "image"][i]
                    }
                }
                RowLayout {
                    Label { text: "Color:"; color: Theme.text }
                    Rectangle {
                        width: 30; height: 20; radius: 3
                        border.color: Theme.border
                        color: settings ? settings.backgroundColor : "#fff"
                        MouseArea { anchors.fill: parent; onClicked: bgColorDlg.open() }
                    }
                    ColorDialog {
                        id: bgColorDlg
                        selectedColor: settings ? settings.backgroundColor : "#ffffff"
                        onAccepted: if (settings) settings.backgroundColor = selectedColor
                    }
                }
                Label { text: "Light presets"; font.bold: true; color: Theme.text }
                Flow {
                    Layout.fillWidth: true
                    spacing: 4
                    Repeater {
                        model: lightPresets
                        Button { text: modelData.name; onClicked: applyPreset(modelData) }
                    }
                }
                Item { Layout.fillHeight: true }
            }

            ColumnLayout {
                spacing: 6
                CheckBox {
                    text: "Use sun light"
                    checked: settings ? settings.useSunLight : true
                    onToggled: if (settings) settings.useSunLight = checked
                }
                RowLayout {
                    Label { text: "Intensity:"; Layout.minimumWidth: 70; color: Theme.text }
                    Slider {
                        from: 0; to: 5; Layout.fillWidth: true
                        value: settings ? settings.lightIntensity : 1
                        onMoved: if (settings) settings.lightIntensity = value
                    }
                    Label { text: settings ? settings.lightIntensity.toFixed(2) : ""; color: Theme.textDim; Layout.preferredWidth: 36 }
                }
                RowLayout {
                    Label { text: "Angle:"; Layout.minimumWidth: 70; color: Theme.text }
                    Slider {
                        from: 0; to: 360; Layout.fillWidth: true
                        value: settings ? settings.lightAngle : 45
                        onMoved: if (settings) settings.lightAngle = value
                    }
                }
                RowLayout {
                    Label { text: "Altitude:"; Layout.minimumWidth: 70; color: Theme.text }
                    Slider {
                        from: 0; to: 90; Layout.fillWidth: true
                        value: settings ? settings.lightAltitude : 30
                        onMoved: if (settings) settings.lightAltitude = value
                    }
                }
                RowLayout {
                    Label { text: "Env intensity:"; Layout.minimumWidth: 70; color: Theme.text }
                    Slider {
                        from: 0; to: 5; Layout.fillWidth: true
                        value: settings ? settings.environmentIntensity : 1
                        onMoved: if (settings) settings.environmentIntensity = value
                    }
                }
                RowLayout {
                    Label { text: "Env rotation:"; Layout.minimumWidth: 70; color: Theme.text }
                    Slider {
                        from: 0; to: 360; Layout.fillWidth: true
                        value: settings ? settings.environmentRotation : 0
                        onMoved: if (settings) settings.environmentRotation = value
                    }
                }

                Label { text: "Poly Haven HDRIs (CC0)"; font.bold: true; color: Theme.text }
                ListView {
                    id: hdriList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: hdris
                    spacing: 2
                    delegate: Rectangle {
                        width: ListView.view.width
                        height: 38
                        radius: 3
                        color: selectedHdriId === modelData.id ? Theme.accentLight : Theme.panelAlt
                        border.color: Theme.border
                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 4
                            spacing: 6
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 0
                                Text { text: modelData.name; color: Theme.text; font.pixelSize: 12 }
                                Text {
                                    text: {
                                        const st = hdriStatus(modelData.id)
                                        return st.bestResolution
                                            ? ("Cached: " + st.bestResolution)
                                            : (modelData.category + " · not downloaded")
                                    }
                                    color: Theme.textDim; font.pixelSize: 10
                                }
                            }
                            Button {
                                text: hdriStatus(modelData.id).bestPath ? "Use" : "Select"
                                onClicked: {
                                    selectedHdriId = modelData.id
                                    if (hdriStatus(modelData.id).bestPath) useLocalHdri(modelData.id)
                                }
                            }
                        }
                        MouseArea {
                            anchors.fill: parent
                            z: -1
                            onClicked: selectedHdriId = modelData.id
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Label { text: "Res:"; color: Theme.text }
                    ComboBox {
                        Layout.preferredWidth: 140
                        model: resolutions.map(function (r) { return r.label })
                        currentIndex: 1
                        onActivated: (i) => {
                            downloadRes = (resolutions[i] && resolutions[i].id) ? resolutions[i].id : "2k"
                        }
                        Component.onCompleted: downloadRes = "2k"
                    }
                    Button {
                        text: renderAssets && renderAssets.busy ? "…" : "Get"
                        enabled: !!(selectedHdriId && renderAssets && !renderAssets.busy)
                        highlighted: true
                        onClicked: downloadSelected()
                    }
                    Button {
                        text: "Stop"
                        visible: !!(renderAssets && renderAssets.busy)
                        onClicked: renderAssets.cancel()
                    }
                }
                ProgressBar {
                    Layout.fillWidth: true
                    visible: !!(renderAssets && renderAssets.busy)
                    value: renderAssets && renderAssets.progress >= 0 ? renderAssets.progress : 0
                    indeterminate: renderAssets && renderAssets.progress < 0
                }
                Button {
                    text: "Open HDRI cache"
                    flat: true
                    onClicked: renderAssets && renderAssets.openCacheFolder()
                }
            }

            ColumnLayout {
                spacing: 8
                RowLayout {
                    Label { text: "Width:"; color: Theme.text }
                    SpinBox {
                        from: 64; to: 8192
                        value: settings ? settings.outputWidth : 1920
                        onValueModified: if (settings) settings.outputWidth = value
                    }
                    Label { text: "Height:"; color: Theme.text }
                    SpinBox {
                        from: 64; to: 8192
                        value: settings ? settings.outputHeight : 1080
                        onValueModified: if (settings) settings.outputHeight = value
                    }
                }
                ComboBox {
                    Layout.fillWidth: true
                    model: ["PNG", "TIFF", "BMP", "JPEG"]
                    currentIndex: {
                        if (!settings) return 0
                        const m = { "png": 0, "tiff": 1, "bmp": 2, "jpg": 3 }
                        return m[settings.outputFormat] || 0
                    }
                    onActivated: (i) => { if (settings) settings.outputFormat = ["png", "tiff", "bmp", "jpg"][i] }
                }
                Label {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    color: Theme.textDim
                    text: "Export writes a still image. Live Render continues in the viewport without saving."
                }
                Item { Layout.fillHeight: true }
                Button {
                    Layout.alignment: Qt.AlignRight
                    text: "Export Image…"
                    highlighted: true
                    onClicked: saveDlg.open()
                }
            }
        }
    }
}
