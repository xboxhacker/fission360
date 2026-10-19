// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "."

// UTILITIES > Scripts and Add-Ins (Shift+S): JavaScript and Python files in the scripts folder, the add-ins
// that run at startup, an editor and the run output. Scripts get app (the main window and all its commands),
// doc (the active design), fission (the application), print() and addCommand(id, label, tooltip, fn).
Popup {
    id: dlg
    property var app
    property string mode: "scripts"      // "scripts" | "addins"
    property string current: ""          // file name being edited ("" = unsaved)
    property bool dirty: false
    property var lastRun: null           // {ok, output, error, line}
    modal: true
    focus: true
    anchors.centerIn: Overlay.overlay
    width: Math.min(940, app.width - 80)
    height: Math.min(660, app.height - 80)
    padding: 0
    closePolicy: Popup.CloseOnEscape

    background: Rectangle { color: Theme.panel; border.color: Theme.border; radius: 4 }

    readonly property bool addInMode: mode === "addins"
    readonly property var addIns: fissionScripts.addIns
    readonly property var names: addInMode ? addIns.map(a => a.name) : fissionScripts.scripts
    readonly property string language: current.toLowerCase().endsWith(".py") ? "py" : "js"
    readonly property var currentAddIn: addIns.find(a => a.name === current) || null

    onOpened: if (!current && names.length) openScript(names[0])
    onModeChanged: {
        current = ""
        editor.text = ""
        dirty = false
        lastRun = null
        // names follows mode through a binding that may not have updated yet
        Qt.callLater(() => { if (!current && names.length) openScript(names[0]) })
    }

    function openScript(name) {
        current = name
        editor.text = fissionScripts.load(name, addInMode)
        dirty = false
        lastRun = null
        if (addInMode && currentAddIn && (currentAddIn.error || currentAddIn.output.length))
            lastRun = { ok: !currentAddIn.error, output: currentAddIn.output, error: currentAddIn.error, line: 0 }
    }
    function newScript(lang) {
        const base = addInMode ? "AddIn" : "Script", ext = lang === "py" ? ".py" : ".js"
        let n = 1
        while (names.indexOf(base + n + ext) >= 0) ++n
        current = base + n + ext
        if (addInMode)
            editor.text = lang === "py"
                ? "# " + current + ": runs when Fission 360 starts\ndef hello():\n    fission.show_message(\"Hello from " + current + "\")\n\nadd_command(\"" + base + n + ".hello\", \"Hello\", \"Say hello\", hello)\n"
                : "// " + current + ": runs when Fission 360 starts\naddCommand(\"" + base + n + ".hello\", \"Hello\", \"Say hello\", () => fission.showMessage(\"Hello from " + current + "\"))\n"
        else
            editor.text = lang === "py"
                ? "# " + current + "\n# app, doc, fission and print() are available\nprint(\"Bodies:\", doc.body_count())\n"
                : "// " + current + "\n// app, doc, fission and print() are available\nprint(\"Bodies:\", doc.bodyCount())\n"
        dirty = true
        lastRun = null
    }
    function saveScript() {
        if (!current) return
        if (fissionScripts.save(current, editor.text, addInMode)) { dirty = false; fissionApp.showMessage("Saved " + current) }
        else fissionApp.showMessage("Could not save " + current)
    }
    function runScript() {
        if (dirty && current) saveScript()
        if (!app.doc) return
        if (app.commandFeatureId) app.finishCommand(true)
        if (addInMode) {
            if (!current) return
            lastRun = fissionScripts.runAddIn(current, app)
        } else {
            lastRun = fissionScripts.run(editor.text, app, 30000, language)
        }
        fissionApp.showMessage(lastRun.ok ? (current || "Script") + " finished"
                                          : (current || "Script") + ": " + lastRun.error)
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 34
            color: Theme.header
            radius: 4
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                Glyph { name: "script"; color: Theme.headerText; Layout.preferredWidth: 18; Layout.preferredHeight: 18 }
                Text { text: "Scripts and Add-Ins"; color: Theme.headerText; font.bold: true; font.pixelSize: 13; Layout.fillWidth: true }
                Text {
                    text: "\u2715"
                    color: Theme.headerText
                    MouseArea { anchors.fill: parent; anchors.margins: -6; onClicked: dlg.close() }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.margins: 8
            spacing: 8

            // Script / add-in list
            ColumnLayout {
                Layout.preferredWidth: 270
                Layout.maximumWidth: 270
                Layout.fillHeight: true
                spacing: 4
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 2
                    Button {
                        Layout.fillWidth: true
                        text: "Scripts"
                        highlighted: !dlg.addInMode
                        onClicked: dlg.mode = "scripts"
                    }
                    Button {
                        objectName: "addInsTab"
                        Layout.fillWidth: true
                        text: "Add-Ins"
                        highlighted: dlg.addInMode
                        onClicked: dlg.mode = "addins"
                    }
                }
                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    color: Theme.panelAlt
                    border.color: Theme.border
                    ListView {
                        id: list
                        anchors.fill: parent
                        anchors.margins: 1
                        clip: true
                        model: dlg.names
                        delegate: Rectangle {
                            id: row
                            required property string modelData
                            readonly property var info: dlg.addInMode ? dlg.addIns.find(a => a.name === modelData) : null
                            width: ListView.view.width
                            height: 26
                            color: modelData === dlg.current ? Theme.accentLight : area.containsMouse ? Theme.hover : "transparent"
                            MouseArea {
                                id: area
                                anchors.fill: parent
                                hoverEnabled: true
                                onClicked: dlg.openScript(row.modelData)
                                onDoubleClicked: { dlg.openScript(row.modelData); dlg.runScript() }
                            }
                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 4
                                anchors.rightMargin: 6
                                spacing: 4
                                CheckBox {
                                    visible: !!row.info
                                    checked: !!row.info && row.info.enabled
                                    padding: 0
                                    Layout.preferredWidth: 22
                                    ToolTip.visible: hovered
                                    ToolTip.text: checked ? "Runs at startup (click to switch off)" : "Switched off (click to run at startup)"
                                    onToggled: fissionScripts.setAddInEnabled(row.modelData, checked, dlg.app)
                                }
                                Text {
                                    Layout.fillWidth: true
                                    text: row.modelData
                                    elide: Text.ElideRight
                                    color: Theme.text
                                }
                                Text {
                                    text: row.modelData.toLowerCase().endsWith(".py") ? "PY" : "JS"
                                    font.pixelSize: 9
                                    font.bold: true
                                    color: Theme.textDim
                                }
                                Rectangle {
                                    visible: !!row.info && row.info.enabled
                                    width: 8; height: 8; radius: 4
                                    color: row.info && row.info.error ? Theme.error : row.info && row.info.loaded ? "#3aa655" : Theme.textDim
                                }
                            }
                        }
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 2
                    Button { Layout.fillWidth: true; text: "New JS"; onClicked: dlg.newScript("js") }
                    Button { Layout.fillWidth: true; text: "New Python"; onClicked: dlg.newScript("py") }
                    Button {
                        Layout.fillWidth: true
                        text: "Delete"
                        enabled: dlg.names.indexOf(dlg.current) >= 0
                        onClicked: {
                            fissionScripts.remove(dlg.current, dlg.addInMode)
                            dlg.current = ""
                            editor.text = ""
                            if (dlg.names.length) dlg.openScript(dlg.names[0])
                        }
                    }
                }
                Text {
                    Layout.fillWidth: true
                    elide: Text.ElideMiddle
                    font.pixelSize: Theme.smallFont
                    color: Theme.textDim
                    text: "Folder: " + (dlg.addInMode ? fissionScripts.addInFolder : fissionScripts.folder)
                    MouseArea {
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        ToolTip.visible: containsMouse
                        ToolTip.text: "Open " + (dlg.addInMode ? fissionScripts.addInFolder : fissionScripts.folder)
                        onClicked: Qt.openUrlExternally("file:///" + (dlg.addInMode ? fissionScripts.addInFolder : fissionScripts.folder))
                    }
                }
                Text {
                    Layout.fillWidth: true
                    elide: Text.ElideMiddle
                    font.pixelSize: Theme.smallFont
                    color: Theme.textDim
                    text: fissionScripts.pythonStatus
                }
            }

            // Editor and output
            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 4
                RowLayout {
                    Layout.fillWidth: true
                    TextField {
                        id: nameField
                        Layout.fillWidth: true
                        text: dlg.current
                        placeholderText: dlg.addInMode ? "Add-in name" : "Script name"
                        onEditingFinished: {
                            const n = text.trim()
                            if (!n || n === dlg.current) return
                            dlg.current = /\.(js|py)$/i.test(n) ? n : n + (dlg.language === "py" ? ".py" : ".js")
                            dlg.dirty = true
                        }
                    }
                    Button { text: dlg.dirty ? "Save *" : "Save"; enabled: !!dlg.current; onClicked: dlg.saveScript() }
                    Button {
                        objectName: "runScript"
                        text: dlg.addInMode ? "Reload" : "Run"
                        highlighted: true
                        onClicked: dlg.runScript()
                    }
                }
                ScrollView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    TextArea {
                        id: editor
                        font.family: "Consolas"
                        font.pixelSize: 13
                        wrapMode: TextEdit.NoWrap
                        selectByMouse: true
                        tabStopDistance: 28
                        background: Rectangle { color: "#1e2227"; border.color: Theme.border }
                        color: "#e6e6e6"
                        onTextChanged: if (activeFocus) dlg.dirty = true
                        Keys.onPressed: (e) => {
                            if (e.key === Qt.Key_Tab) { insert(cursorPosition, "    "); e.accepted = true }
                            else if ((e.key === Qt.Key_Return || e.key === Qt.Key_Enter) && (e.modifiers & Qt.ControlModifier)) {
                                dlg.runScript(); e.accepted = true
                            }
                        }
                    }
                }
                Text {
                    visible: dlg.addInMode
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    color: Theme.textDim
                    font.pixelSize: Theme.smallFont
                    text: {
                        const cmds = fissionScripts.commands.filter(c => c.addIn === dlg.current).map(c => c.label)
                        return cmds.length ? "Commands in UTILITIES \u203a ADD-INS: " + cmds.join(", ")
                                           : "Add-ins add buttons with addCommand(id, label, tooltip, fn) (Python: add_command)."
                    }
                }
                Text { text: "Output"; font.bold: true; color: Theme.text }
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 130
                    color: Theme.panelAlt
                    border.color: Theme.border
                    ScrollView {
                        anchors.fill: parent
                        anchors.margins: 4
                        TextArea {
                            readOnly: true
                            selectByMouse: true
                            font.family: "Consolas"
                            font.pixelSize: 12
                            color: dlg.lastRun && !dlg.lastRun.ok ? Theme.error : Theme.text
                            background: null
                            text: {
                                const r = dlg.lastRun
                                if (!r) return dlg.addInMode ? "Reload runs the add-in again (its commands are replaced)."
                                                             : "Run a script to see its output. Ctrl+Enter runs from the editor."
                                const lines = (r.output || []).slice()
                                if (r.ok && r.result !== undefined) lines.push("=> " + r.result)
                                if (!r.ok) lines.push((r.line ? "Line " + r.line + ": " : "") + r.error)
                                return lines.length ? lines.join("\n") : "(no output)"
                            }
                        }
                    }
                }
            }
        }
    }
}
