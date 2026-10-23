// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import "."

// Projects panel: slides out from the left over the projects folder. Folders and designs as thumbnail
// tiles; double-click opens, right-click manages (rename, move, duplicate, export, delete)
Drawer {
    id: panel
    property var app
    property string folder: projects.root
    property var items: []
    property var folderList: []
    property var current: null          // the tile the menu or a dialog acts on
    property int revision: 0            // bumps to reload thumbnails
    edge: Qt.LeftEdge
    width: Math.min(560, parent ? parent.width * 0.6 : 560)
    height: parent ? parent.height : 600
    modal: true
    dim: true
    interactive: false   // no swipe to close: a sideways drag on a tile is a drag onto the viewport
    padding: 0

    function reload() {
        if (!projects.isInside(folder)) folder = projects.root
        items = projects.entries(folder)
        folderList = projects.folders()
        ++revision
    }
    function show(path) {
        folder = path && projects.isInside(path) ? path : projects.root
        reload()
        open()
    }
    function openItem(item) {
        if (!item) return
        if (item.folder) { folder = item.path; reload(); return }
        app.leaveDesign()
        if (fissionApp.openDocument(item.path)) close()
    }
    function report(ok) {
        if (!ok) fissionApp.showMessage(projects.lastError())
        return ok
    }
    function moveTo(item, dest) {
        if (!item || !dest) return ""
        const to = projects.move(item.path, dest)
        report(to.length > 0)
        return to
    }
    function crumbs() {
        const rel = projects.relative(folder)
        const out = [{ label: projects.root.substring(projects.root.lastIndexOf("/") + 1), path: projects.root }]
        if (!rel) return out
        let p = projects.root
        for (const part of rel.split("/")) { p += "/" + part; out.push({ label: part, path: p }) }
        return out
    }

    onOpened: reload()
    Connections {
        target: projects
        function onChanged() { if (panel.opened) panel.reload() }
    }

    background: Rectangle { color: Theme.panel; border.color: Theme.border }

    FolderDialog {
        id: rootDialog
        title: "Projects folder"
        currentFolder: projects.rootUrl
        onAccepted: { projects.root = selectedFolder.toString(); panel.folder = projects.root; panel.reload() }
    }
    FileDialog {
        id: exportDialog
        title: "Export design"
        fileMode: FileDialog.SaveFile
        nameFilters: ["STEP (*.step *.stp)", "IGES (*.iges *.igs)", "STL mesh (*.stl)", "Wavefront OBJ (*.obj)",
                      "3D Manufacturing Format (*.3mf)"]
        onAccepted: {
            if (panel.current && panel.report(projects.exportDesign(panel.current.path, selectedFile.toString())))
                fissionApp.showMessage("Exported " + panel.current.name)
        }
    }

    // Name prompt for New Folder, Rename and "Move to > New Folder"
    Dialog {
        id: nameDialog
        property string mode: "folder"     // folder | rename | moveNew
        anchors.centerIn: parent
        modal: true
        width: 320
        title: mode === "rename" ? "Rename" : "New Folder"
        standardButtons: Dialog.Ok | Dialog.Cancel
        function ask(m, initial) {
            mode = m
            nameField.text = initial
            open()
            nameField.forceActiveFocus()
            nameField.selectAll()
        }
        onAccepted: {
            const name = nameField.text
            if (mode === "rename") panel.report(projects.rename(panel.current.path, name).length > 0)
            else {
                const made = projects.makeFolder(panel.folder, name)
                if (panel.report(made.length > 0) && mode === "moveNew") panel.moveTo(panel.current, made)
            }
        }
        TextField {
            id: nameField
            width: parent.width
            selectByMouse: true
            onAccepted: nameDialog.accept()
        }
    }
    Dialog {
        id: deleteDialog
        anchors.centerIn: parent
        modal: true
        width: 340
        title: "Delete"
        standardButtons: Dialog.Yes | Dialog.No
        onAccepted: if (panel.current) panel.report(projects.remove(panel.current.path))
        Text {
            width: parent.width
            wrapMode: Text.WordWrap
            color: Theme.text
            text: panel.current ? "Move \"" + panel.current.name + "\"" + (panel.current.folder ? " and everything in it" : "")
                                  + " to the Recycle Bin?" : ""
        }
    }

    Menu {
        id: itemMenu
        MenuItem { text: panel.current && panel.current.folder ? "Open Folder" : "Open"; onTriggered: panel.openItem(panel.current) }
        MenuItem {
            text: "Insert into Current Design (linked)"
            enabled: !!panel.current && !panel.current.folder && !!panel.app.doc
            onTriggered: {
                const path = panel.current.path
                panel.close()
                panel.app.insertLinkedDesign(path)
            }
        }
        MenuSeparator {}
        MenuItem { text: "Rename..."; onTriggered: nameDialog.ask("rename", panel.current.name) }
        Menu {
            id: moveMenu
            title: "Move to"
            Instantiator {
                model: panel.folderList.filter(f => panel.current && f.path !== panel.folder && f.path !== panel.current.path
                                               && !f.path.startsWith(panel.current.path + "/"))
                delegate: MenuItem {
                    required property var modelData
                    text: modelData.label
                    onTriggered: panel.moveTo(panel.current, modelData.path)
                }
                onObjectAdded: (index, object) => moveMenu.insertItem(index, object)
                onObjectRemoved: (index, object) => moveMenu.removeItem(object)
            }
            MenuSeparator {}
            MenuItem { text: "New Folder..."; onTriggered: nameDialog.ask("moveNew", "New Folder") }
        }
        MenuItem {
            text: "Duplicate"
            enabled: panel.current && !panel.current.folder
            onTriggered: panel.report(projects.duplicate(panel.current.path).length > 0)
        }
        MenuItem {
            text: "Export..."
            enabled: panel.current && !panel.current.folder
            onTriggered: { exportDialog.selectedFile = projects.urlOf(panel.folder + "/" + panel.current.name + ".step"); exportDialog.open() }
        }
        MenuItem { text: "Show in Explorer"; onTriggered: projects.reveal(panel.current.path) }
        MenuSeparator {}
        MenuItem { text: "Delete"; onTriggered: deleteDialog.open() }
    }
    Menu {
        id: blankMenu
        MenuItem { text: "New Folder..."; onTriggered: nameDialog.ask("folder", "New Folder") }
        MenuItem { text: "Save Current Design Here..."; onTriggered: panel.app.saveInto(panel.folder) }
        MenuItem { text: "Refresh"; onTriggered: panel.reload() }
        MenuItem { text: "Show in Explorer"; onTriggered: projects.reveal(panel.folder) }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 34
            color: Theme.header
            Text { anchors.verticalCenter: parent.verticalCenter; x: 12; text: "Projects"; color: Theme.headerText; font.bold: true; font.pixelSize: 13 }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                anchors.right: parent.right
                anchors.rightMargin: 12
                text: "\u2715"
                color: Theme.headerText
                MouseArea { anchors.fill: parent; anchors.margins: -6; onClicked: panel.close() }
            }
        }

        // Projects folder
        RowLayout {
            Layout.fillWidth: true
            Layout.margins: 8
            spacing: 6
            Glyph { name: "folder"; Layout.preferredWidth: 18; Layout.preferredHeight: 18 }
            Text {
                Layout.fillWidth: true
                text: projects.root
                elide: Text.ElideMiddle
                color: Theme.textDim
                font.pixelSize: Theme.smallFont
                ToolTip.visible: rootHover.hovered
                ToolTip.text: projects.root
                HoverHandler { id: rootHover }
            }
            Button { text: "Change..."; onClicked: rootDialog.open() }
        }

        // Breadcrumb and folder actions
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 34
            color: Theme.panelAlt
            border.color: Theme.border
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 6
                anchors.rightMargin: 6
                spacing: 2
                ToolbarButton {
                    compact: true; glyph: "folderup"; tip: "Up one folder"
                    enabled: projects.relative(panel.folder).length > 0
                    onClicked: { panel.folder = projects.parentFolder(panel.folder); panel.reload() }
                }
                Flickable {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    contentWidth: crumbRow.width
                    clip: true
                    Row {
                        id: crumbRow
                        height: parent.height
                        spacing: 2
                        Repeater {
                            model: { panel.revision; return panel.crumbs() }
                            Row {
                                required property var modelData
                                required property int index
                                anchors.verticalCenter: parent.verticalCenter
                                spacing: 2
                                Text { visible: index > 0; text: "\u203A"; color: Theme.textDim; font.pixelSize: 13 }
                                Text {
                                    text: modelData.label
                                    color: crumbArea.containsMouse ? Theme.accent : Theme.text
                                    font.pixelSize: Theme.fontSize
                                    font.bold: modelData.path === panel.folder
                                    MouseArea {
                                        id: crumbArea
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        onClicked: { panel.folder = modelData.path; panel.reload() }
                                    }
                                }
                            }
                        }
                    }
                }
                ToolbarButton { compact: true; glyph: "newfolder"; tip: "New folder"; onClicked: nameDialog.ask("folder", "New Folder") }
                ToolbarButton { compact: true; glyph: "save"; tip: "Save the current design here"; onClicked: panel.app.saveInto(panel.folder) }
                ToolbarButton { compact: true; glyph: "refresh"; tip: "Refresh"; onClicked: panel.reload() }
            }
        }

        GridView {
            id: grid
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.margins: 6
            clip: true
            cellWidth: Math.floor(width / Math.max(1, Math.floor(width / 170)))
            cellHeight: 150
            model: panel.items
            ScrollBar.vertical: ScrollBar {}
            currentIndex: -1

            MouseArea {
                anchors.fill: parent
                z: -1
                acceptedButtons: Qt.LeftButton | Qt.RightButton
                onClicked: (m) => { grid.currentIndex = -1; if (m.button === Qt.RightButton) blankMenu.popup() }
            }

            delegate: Item {
                id: tile
                required property var modelData
                required property int index
                width: grid.cellWidth
                height: grid.cellHeight
                readonly property string thumb: { panel.revision; return modelData.folder ? "" : projects.thumbnail(modelData.path) }
                Rectangle {
                    anchors.fill: parent
                    anchors.margins: 4
                    radius: 4
                    color: grid.currentIndex === tile.index ? Theme.accentLight : tileArea.containsMouse ? Theme.hover : "white"
                    border.color: grid.currentIndex === tile.index ? Theme.accent : Theme.border
                    Rectangle {
                        id: preview
                        anchors { left: parent.left; right: parent.right; top: parent.top; margins: 6 }
                        height: width * 10 / 16
                        color: tile.modelData.folder ? "transparent" : "#dfe3e8"
                        Image {
                            anchors.fill: parent
                            visible: tile.thumb.length > 0
                            source: tile.thumb
                            fillMode: Image.PreserveAspectFit
                            asynchronous: true
                            cache: false
                        }
                        Glyph {
                            visible: tile.thumb.length === 0
                            anchors.centerIn: parent
                            width: 48; height: 48
                            name: tile.modelData.folder ? "folder" : "design"
                            color: tile.modelData.folder ? "#c58b1d" : Theme.textDim
                        }
                    }
                    Text {
                        anchors { left: parent.left; right: parent.right; top: preview.bottom; margins: 6 }
                        text: tile.modelData.name
                        elide: Text.ElideRight
                        horizontalAlignment: Text.AlignHCenter
                        color: Theme.text
                        font.pixelSize: Theme.fontSize
                    }
                    Text {
                        anchors { left: parent.left; right: parent.right; bottom: parent.bottom; bottomMargin: 4 }
                        text: tile.modelData.folder ? "Folder" : tile.modelData.modified
                        horizontalAlignment: Text.AlignHCenter
                        color: Theme.textDim
                        font.pixelSize: Theme.smallFont - 1
                    }
                }
                // Dragging a design tile onto the viewport inserts it linked. An OS drag started from C++ once the
                // mouse has moved 8 px: binding Drag.active to the MouseArea looped (the drag blocks until the drop)
                // and Qt cancelled it.
                MouseArea {
                    id: tileArea
                    property point pressAt
                    property bool dragging: false
                    anchors.fill: parent
                    hoverEnabled: true
                    // The grid (vertical flick) and the drawer would otherwise take the press away
                    preventStealing: !tile.modelData.folder
                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                    onPressed: (m) => { pressAt = Qt.point(m.x, m.y); dragging = false }
                    onPositionChanged: (m) => {
                        if (!pressed || dragging || tile.modelData.folder || !(m.buttons & Qt.LeftButton)) return
                        if (Math.abs(m.x - pressAt.x) + Math.abs(m.y - pressAt.y) < 8) return
                        dragging = true
                        const path = tile.modelData.path
                        panel.close()
                        Qt.callLater(() => { projects.startDrag(path); tileArea.dragging = false })
                    }
                    onClicked: (m) => {
                        if (dragging) return
                        grid.currentIndex = tile.index
                        panel.current = tile.modelData
                        if (m.button === Qt.RightButton) itemMenu.popup()
                    }
                    onDoubleClicked: (m) => { if (m.button === Qt.LeftButton) panel.openItem(tile.modelData) }
                }
                ToolTip.visible: tileArea.containsMouse && !tile.modelData.folder
                ToolTip.delay: 700
                ToolTip.text: tile.modelData.name + "\n" + tile.modelData.modified + "  \u2022  "
                              + Math.max(1, Math.round(tile.modelData.size / 1024)) + " KB"
            }

            Text {
                anchors.centerIn: parent
                visible: grid.count === 0
                width: parent.width - 40
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
                color: Theme.textDim
                text: "This folder is empty.\nSave a design here, or right-click for a new folder."
            }
        }
        // Opening a big design runs while this panel is still up, and the dim behind it greys out the status
        // bar, so the panel shows the same progress along its bottom edge
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 44
            visible: fissionApp.busy
            color: Theme.header
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 8
                spacing: 5
                Text {
                    Layout.fillWidth: true
                    text: fissionApp.progressText || "Working…"
                    color: "#9fd4f0"
                    elide: Text.ElideRight
                    font.pixelSize: Theme.fontSize
                }
                Rectangle {
                    id: openTrack
                    Layout.fillWidth: true
                    Layout.preferredHeight: 6
                    radius: 3
                    color: Qt.darker(Theme.header, 1.6)
                    clip: true
                    readonly property bool indeterminate: fissionApp.progressValue < 0
                    Rectangle {
                        height: parent.height
                        radius: 3
                        color: Theme.accent
                        width: openTrack.indeterminate ? openTrack.width * 0.3
                                                       : openTrack.width * Math.max(0, Math.min(1, fissionApp.progressValue))
                        Behavior on width { enabled: !openTrack.indeterminate; NumberAnimation { duration: 120 } }
                        SequentialAnimation on x {
                            running: openTrack.visible && openTrack.indeterminate
                            loops: Animation.Infinite
                            NumberAnimation { from: 0; to: openTrack.width * 0.7; duration: 900; easing.type: Easing.InOutSine }
                            NumberAnimation { from: openTrack.width * 0.7; to: 0; duration: 900; easing.type: Easing.InOutSine }
                        }
                    }
                }
            }
        }
    }
}
