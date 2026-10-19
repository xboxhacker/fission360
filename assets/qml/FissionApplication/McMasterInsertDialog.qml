// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtWebEngine
import "."

// Insert ▸ McMaster-Carr: resizable in-app Chromium popup. Product pages and
// STEP downloads both feed importCAD into the design.
Popup {
    id: dlg
    property var app
    property real winW: 920
    property real winH: 680
    modal: true
    focus: true
    width: winW
    height: winH
    padding: 0
    closePolicy: Popup.CloseOnEscape
    // Free position so the user can drag/resize; centered on open
    x: Overlay.overlay ? Math.max(8, (Overlay.overlay.width - winW) / 2) : 40
    y: Overlay.overlay ? Math.max(8, (Overlay.overlay.height - winH) / 2) : 40

    readonly property var client: typeof mcmaster !== "undefined" ? mcmaster : null
    readonly property url homeUrl: "https://www.mcmaster.com/"

    onOpened: {
        const maxW = Overlay.overlay ? Overlay.overlay.width - 16 : 1200
        const maxH = Overlay.overlay ? Overlay.overlay.height - 16 : 900
        winW = Math.min(Math.max(winW, 720), maxW)
        winH = Math.min(Math.max(winH, 480), maxH)
        x = Math.max(8, (Overlay.overlay.width - winW) / 2)
        y = Math.max(8, (Overlay.overlay.height - winH) / 2)
        if (client)
            client.startCatalogBrowser()
        web.url = homeUrl
    }
    onClosed: {
        if (client) {
            if (client.busy) client.cancel()
            client.stopCatalogBrowser()
        }
    }

    property string pendingLabel: ""

    function onStepReady(path, pn, title) {
        if (!app || !app.doc) return
        if (app.doc.importCADBusy()) {
            fissionApp.showMessage("Wait for the current import to finish")
            return
        }
        if (app.commandFeatureId) app.finishCommand(true)
        if (app.activeSketchId) app.finishSketch()
        pendingLabel = (title && title.length) ? (pn + " — " + title) : (pn || "part")
        fissionApp.showMessage("Importing " + pendingLabel + "…")
        // Close so the UI stays usable while the STEP loads on a worker thread
        dlg.close()
        app.doc.importCADAsync(path)
    }

    function placeImportedBodies(firstBody, bodyCount) {
        if (!app || !app.doc || bodyCount <= 0) return
        const items = []
        for (let i = 0; i < bodyCount; ++i)
            items.push({ kind: "body", body: firstBody + i })
        app.selection.setItems(items)
        Qt.callLater(() => {
            if (app.viewport && app.viewport.renderer)
                app.viewport.renderer.fitAll(true)
            app.startCommand("Move")
            fissionApp.showMessage("Move the inserted part, then press Enter / OK")
        })
    }

    Connections {
        target: dlg.app && dlg.app.doc ? dlg.app.doc : null
        function onImportCADFinished(featureId, error, firstBody, bodyCount) {
            if (!featureId) {
                fissionApp.showMessage("McMaster insert failed: " + (error || "unknown error"))
                return
            }
            const f = app.doc.featureById(featureId)
            const label = dlg.pendingLabel || "part"
            fissionApp.showMessage(f && !f.healthy ? f.name + ": " + f.error : "Inserted " + label)
            dlg.placeImportedBodies(firstBody, bodyCount)
        }
    }

    function insertCurrentPart() {
        if (!client || !client.currentPartNumber.length || client.busy) return
        client.insertPart(client.currentPartNumber)
    }

    function handleDownload(download) {
        if (!client) {
            download.cancel()
            return
        }
        let name = download.downloadFileName || ""
        if (!name.length) {
            const path = download.url.toString().split("?")[0]
            name = path.substring(path.lastIndexOf("/") + 1) || "mcmaster.step"
        }
        const lower = name.toLowerCase()
        const urlLower = download.url.toString().toLowerCase()
        const isStep = lower.endsWith(".step") || lower.endsWith(".stp")
                       || urlLower.indexOf(".step") >= 0 || urlLower.indexOf(".stp") >= 0
                       || /3-?d\s*step|step\s*file|cad.*step/i.test(name)
        if (!isStep) {
            // Allow the download but only auto-import STEP
            download.accept()
            return
        }
        if (!lower.endsWith(".step") && !lower.endsWith(".stp"))
            name = (client.currentPartNumber || "part") + ".step"
        const dir = client.cacheRoot()
        download.downloadDirectory = dir
        download.downloadFileName = name
        download.accept()
        const fullPath = dir + "/" + name
        const pn = client.currentPartNumber || ""
        // QtWebEngine DownloadState: Completed=2, Cancelled=3, Interrupted=4
        function checkDone() {
            const st = download.state
            if (st === 2) {
                dlg.onStepReady(fullPath, pn, "")
            } else if (st === 3 || st === 4) {
                fissionApp.showMessage("McMaster STEP download failed")
            }
        }
        download.stateChanged.connect(checkDone)
        // Some builds finish before the handler connects
        Qt.callLater(checkDone)
    }

    Connections {
        target: dlg.client
        function onStepReady(localPath, partNumber, title) { dlg.onStepReady(localPath, partNumber, title) }
        function onInsertFailed(error) { fissionApp.showMessage("McMaster-Carr: " + error) }
        function onProductPageDetected(partNumber) {
            fissionApp.showMessage("McMaster: " + partNumber + " — downloading STEP…")
        }
    }

    background: Rectangle {
        color: Theme.panel
        border.color: Theme.border
        radius: 4

        // Resize grip (bottom-right)
        MouseArea {
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            width: 18
            height: 18
            cursorShape: Qt.SizeFDiagCursor
            property real startX
            property real startY
            property real startW
            property real startH
            onPressed: function(mouse) {
                startX = mouse.x
                startY = mouse.y
                startW = dlg.winW
                startH = dlg.winH
            }
            onPositionChanged: function(mouse) {
                if (!pressed) return
                const maxW = Overlay.overlay ? Overlay.overlay.width - dlg.x - 8 : 1600
                const maxH = Overlay.overlay ? Overlay.overlay.height - dlg.y - 8 : 1200
                dlg.winW = Math.min(maxW, Math.max(640, startW + mouse.x - startX))
                dlg.winH = Math.min(maxH, Math.max(420, startH + mouse.y - startY))
            }
            Rectangle {
                anchors.fill: parent
                anchors.margins: 4
                color: "transparent"
                Canvas {
                    anchors.fill: parent
                    onPaint: {
                        const c = getContext("2d")
                        c.clearRect(0, 0, width, height)
                        c.strokeStyle = Theme.border
                        c.beginPath()
                        c.moveTo(width, 2); c.lineTo(2, height)
                        c.moveTo(width, 7); c.lineTo(7, height)
                        c.moveTo(width, 12); c.lineTo(12, height)
                        c.stroke()
                    }
                    Component.onCompleted: requestPaint()
                }
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 0
        anchors.bottomMargin: 14
        anchors.rightMargin: 14
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 34
            color: Theme.header
            radius: 4
            MouseArea {
                id: dragTitle
                anchors.fill: parent
                anchors.rightMargin: 36
                property real ox
                property real oy
                property real sx
                property real sy
                onPressed: function(mouse) {
                    ox = mouse.x; oy = mouse.y
                    sx = dlg.x; sy = dlg.y
                }
                onPositionChanged: function(mouse) {
                    if (!pressed) return
                    dlg.x = Math.max(0, sx + mouse.x - ox)
                    dlg.y = Math.max(0, sy + mouse.y - oy)
                }
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                x: 12
                text: "Insert McMaster-Carr"
                color: Theme.headerText
                font.bold: true
                font.pixelSize: 13
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                anchors.right: parent.right
                anchors.rightMargin: 12
                text: "\u2715"
                color: Theme.headerText
                MouseArea { anchors.fill: parent; anchors.margins: -6; onClicked: dlg.close() }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.margins: 8
            Layout.bottomMargin: 4
            spacing: 6
            Button { text: "←"; flat: true; enabled: web.canGoBack; onClicked: web.goBack() }
            Button { text: "→"; flat: true; enabled: web.canGoForward; onClicked: web.goForward() }
            Button { text: "Home"; flat: true; onClicked: web.url = dlg.homeUrl }
            TextField {
                id: address
                Layout.fillWidth: true
                text: web.url
                selectByMouse: true
                onAccepted: {
                    let t = text.trim()
                    if (t && !t.match(/^https?:\/\//i))
                        t = "https://" + t
                    web.url = t
                }
            }
            Button { text: "Reload"; flat: true; onClicked: web.reload() }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: 8
            Layout.rightMargin: 8
            color: "#ffffff"
            border.color: Theme.border
            radius: 3
            clip: true

            WebEngineView {
                id: web
                anchors.fill: parent
                anchors.margins: 1
                profile: WebEngineProfile {
                    id: mcmProfile
                    storageName: "FissionMcMaster"
                    httpUserAgent: "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36"
                    httpCacheType: WebEngineProfile.DiskHttpCache
                    persistentCookiesPolicy: WebEngineProfile.AllowPersistentCookies
                    onDownloadRequested: function(download) { dlg.handleDownload(download) }
                }
                onUrlChanged: {
                    address.text = url
                    if (dlg.client)
                        dlg.client.onCatalogUrl(url.toString())
                }
                onLoadingChanged: function(request) {
                    if (request.status === WebEngineView.LoadSucceededStatus && dlg.client)
                        dlg.client.onCatalogUrl(web.url.toString())
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.margins: 8
            spacing: 6

            RowLayout {
                Layout.fillWidth: true
                Text {
                    Layout.fillWidth: true
                    text: {
                        if (!client) return "McMaster client unavailable"
                        if (client.currentPartNumber.length)
                            return "Part on page: " + client.currentPartNumber
                        return client.lastError.length ? client.lastError
                                                       : (client.status || "Open a product page, or download a 3-D STEP file")
                    }
                    color: client && client.lastError.length ? "#c44" : Theme.textDim
                    font.pixelSize: Theme.smallFont
                    elide: Text.ElideRight
                }
                CheckBox {
                    text: "STEP without threads"
                    checked: client ? client.preferNoThreads : false
                    onToggled: if (client) client.preferNoThreads = checked
                }
            }

            ProgressBar {
                Layout.fillWidth: true
                visible: client && client.busy
                from: 0
                to: 1
                value: client && client.progress >= 0 ? client.progress : 0
                indeterminate: client && client.progress < 0
            }

            RowLayout {
                Layout.fillWidth: true
                Button {
                    text: "Open cache"
                    flat: true
                    onClicked: if (client) client.openCacheFolder()
                }
                Item { Layout.fillWidth: true }
                Button {
                    text: "Insert STEP"
                    enabled: client && !client.busy && client.currentPartNumber.length > 0
                    highlighted: true
                    onClicked: dlg.insertCurrentPart()
                }
                Button {
                    text: "Done"
                    onClicked: {
                        if (client && client.busy) client.cancel()
                        dlg.close()
                    }
                }
            }
        }
    }
}
