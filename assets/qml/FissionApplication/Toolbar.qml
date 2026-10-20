// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "."

// Ribbon: a row of workspace tabs (SOLID, SURFACE, MESH, ...) over groups of tools. Each group shows its
// most used tools as buttons; its title opens a menu with every tool of the group.
// While a sketch is edited the contextual SKETCH tab replaces the groups.
Rectangle {
    id: toolbar
    property var app
    property string tab: "SOLID"
    readonly property bool sketching: app.activeSketchId.length > 0
    readonly property string shownTab: sketching ? "SKETCH" : tab
    implicitHeight: 106
    color: Theme.panel

    Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.border }

    // ------------------------------------------------------------------ tool model
    // {text, glyph, tip, type, extra}             starts a feature command
    // {text, glyph, tip, run, checked}            calls run(); checked() highlights it
    // {text, glyph, planned: "Phase N"}            listed but not available yet
    // "-"                                          menu separator
    function cmd(text, glyph, type, extra, tip) { return { text: text, glyph: glyph, type: type, extra: extra, tip: tip || text } }
    function act(text, glyph, run, tip, checked) { return { text: text, glyph: glyph, run: run, tip: tip || text, checked: checked } }
    function planned(text, glyph, phase) { return { text: text, glyph: glyph || "comingsoon", planned: phase } }

    function runItem(it) {
        if (!it || it === "-") return
        if (it.planned) { fissionApp.showMessage(it.text + " is planned for " + it.planned); return }
        if (it.run) { it.run(); return }
        app.startCommand(it.type, it.extra)
    }

    // ------------------------------------------------------------------ pinned / removed tools (right-click)
    // pinned: [{tab, text}] shown in a PINNED group on every design tab; hidden: ["TAB|GROUP|text"] quick buttons
    function loadList(key) {
        try { return JSON.parse(fissionApp.setting(key, "[]")) || [] } catch (e) { return [] }
    }
    property var pinned: loadList("toolbar/pinned")
    property var hidden: loadList("toolbar/hidden")
    function savePins() {
        fissionApp.setSetting("toolbar/pinned", JSON.stringify(pinned))
        fissionApp.setSetting("toolbar/hidden", JSON.stringify(hidden))
    }
    function isPinned(text) { return pinned.some(p => p.text === text) }
    function pin(tabName, text) {
        if (isPinned(text)) return
        pinned = pinned.concat([{ tab: tabName, text: text }])
        hidden = hidden.filter(h => !h.endsWith("|" + text))
        savePins()
        fissionApp.showMessage(text + " pinned to the toolbar")
    }
    function unpin(text) {
        pinned = pinned.filter(p => p.text !== text)
        savePins()
    }
    function hideQuick(tabName, groupTitle, text) {
        const key = tabName + "|" + groupTitle + "|" + text
        if (!hidden.includes(key)) hidden = hidden.concat([key])
        savePins()
        fissionApp.showMessage(text + " removed from the toolbar (still in the " + groupTitle + " menu)")
    }
    function restoreToolbar() {
        pinned = []
        hidden = []
        savePins()
    }
    function findTool(tabName, text) {
        for (const g of (tabs[tabName] || []))
            for (const it of g.items)
                if (it !== "-" && it.text === text) return it
        for (const t of Object.keys(tabs))
            for (const g of tabs[t])
                for (const it of g.items)
                    if (it !== "-" && it.text === text) return it
        return null
    }
    readonly property var pinnedGroup: ({ title: "PINNED", quick: 99, pinnedGroup: true,
                                          items: pinned.map(p => findTool(p.tab, p.text)).filter(t => !!t && !t.planned) })
    // where: "quick" (a ribbon button), "menu" (a group dropdown row) or "pinned" (the PINNED group)
    function showToolMenu(tool, groupTitle, where) {
        if (!tool || tool === "-") return
        toolMenu.tool = tool
        toolMenu.groupTitle = groupTitle
        toolMenu.where = where
        toolMenu.popup()
    }

    Menu {
        id: toolMenu
        property var tool: null
        property string groupTitle: ""
        property string where: ""
        readonly property bool pinnedTool: !!tool && (toolbar.pinned, toolbar.isPinned(tool.text))
        MenuItem {
            text: "Pin to Toolbar"
            visible: !toolMenu.pinnedTool && toolMenu.tool && !toolMenu.tool.planned
            height: visible ? implicitHeight : 0
            onTriggered: toolbar.pin(toolbar.tab, toolMenu.tool.text)
        }
        MenuItem {
            text: "Unpin from Toolbar"
            visible: toolMenu.pinnedTool
            height: visible ? implicitHeight : 0
            onTriggered: toolbar.unpin(toolMenu.tool.text)
        }
        MenuItem {
            text: "Remove from Toolbar"
            visible: toolMenu.where === "quick"
            height: visible ? implicitHeight : 0
            onTriggered: toolbar.hideQuick(toolbar.tab, toolMenu.groupTitle, toolMenu.tool.text)
        }
        MenuSeparator {}
        MenuItem {
            text: "Reset Toolbar"
            enabled: toolbar.pinned.length > 0 || toolbar.hidden.length > 0
            onTriggered: toolbar.restoreToolbar()
        }
    }

    readonly property var selectGroup: ({ title: "SELECT", quick: 3, items: [
        act("Window", "selwindow", () => app.setSelectionTool("window"),
            "Window Selection (1): drag left to right for items fully inside, right to left for items touched",
            () => app.selectionTool === "window"),
        act("Freeform", "selfreeform", () => app.setSelectionTool("freeform"), "Freeform Selection (2): drag a loop around the items",
            () => app.selectionTool === "freeform"),
        act("Paint", "selpaint", () => app.setSelectionTool("paint"), "Paint Selection (3): drag over the items",
            () => app.selectionTool === "paint")
    ] })
    readonly property var constructGroup: ({ title: "CONSTRUCT", quick: 2, items: [
        cmd("Offset Plane", "planeoffset", "ConstructionPlane", { mode: "offset" }),
        cmd("Plane at Angle", "planeangle", "ConstructionPlane", { mode: "angle" }),
        cmd("Midplane", "midplane", "ConstructionPlane", { mode: "midplane" }),
        cmd("Plane Through Three Points", "plane3", "ConstructionPlane", { mode: "threePoints" }),
        "-",
        cmd("Axis", "axis", "ConstructionAxis"),
        cmd("Point", "point", "ConstructionPoint")
    ] })
    readonly property var inspectGroup: ({ title: "INSPECT", quick: 2, items: [
        act("Measure", "measure", () => app.openInspect("measure"), "Measure (I): pick one or two items"),
        act("Interference", "interference", () => app.openInspect("interference"), "Interference: overlapping bodies"),
        act("Section Analysis", "section", () => app.openInspect("section"), "Section Analysis: cut the view with a plane",
            () => app.sectionEnabled),
        "-",
        act("Zebra Analysis", "zebra", () => app.openInspect("zebra"), "", () => app.analysis === "zebra"),
        act("Draft Analysis", "draftanalysis", () => app.openInspect("draft"), "", () => app.analysis === "draft"),
        act("Curvature Map Analysis", "curvature", () => app.openInspect("curvature"), "", () => app.analysis === "curvature"),
        "-",
        act("Properties", "properties", () => { app.showProperties = !app.showProperties }, "", () => app.showProperties),
        act("Keyboard Shortcuts", "keyboard", () => app.openShortcuts(), "Keyboard shortcuts (press S to search commands)"),
        act("About Fission 360", "info", () => app.openAbout(), "About Fission 360 (GPLv3)")
    ] })

    readonly property var tabs: ({
        SOLID: [
            { title: "CREATE", quick: 4, items: [
                cmd("Create Sketch", "sketch", "Sketch"),
                cmd("Extrude", "extrude", "Extrude", undefined, "Extrude (E)"),
                cmd("Revolve", "revolve", "Revolve"),
                cmd("Hole", "hole", "Hole", undefined, "Hole (H)"),
                cmd("Sweep", "sweep", "Sweep"),
                cmd("Loft", "loft", "Loft"),
                cmd("Rib", "rib", "Rib"),
                cmd("Web", "web", "Web"),
                cmd("Thread", "thread", "Thread"),
                "-",
                cmd("Rectangular Pattern", "rectpattern", "RectangularPattern"),
                cmd("Circular Pattern", "circpattern", "CircularPattern"),
                cmd("Pattern on Path", "pathpattern", "PathPattern"),
                cmd("Mirror", "mirror", "Mirror")
            ] },
            { title: "MODIFY", quick: 4, items: [
                act("Press Pull", "offsetface", () => app.pressPull(), "Press Pull (Q)"),
                cmd("Fillet", "fillet", "Fillet", undefined, "Fillet (F)"),
                cmd("Chamfer", "chamfer", "Chamfer"),
                cmd("Shell", "shell", "Shell"),
                cmd("Draft", "draft", "Draft"),
                cmd("Scale", "scale", "Scale"),
                cmd("Combine", "combine", "Combine"),
                "-",
                cmd("Offset Face", "offsetface", "OffsetFace"),
                cmd("Replace Face", "replaceface", "ReplaceFace"),
                cmd("Split Face", "splitface", "SplitFace"),
                cmd("Split Body", "splitbody", "SplitBody"),
                cmd("Offset", "offsetbody", "Offset", undefined, "Offset: copies of faces or bodies"),
                "-",
                cmd("Move/Copy", "move", "Move", undefined, "Move/Copy (M)"),
                act("Change Parameters", "parameters", () => app.openParameters())
            ] },
            { title: "CONFIGURE", quick: 1, items: [
                act("Parameters", "parameters", () => app.openParameters(), "Change Parameters"),
                act("Document Settings", "settings", () => app.openDocumentSettings())
            ] },
            constructGroup,
            inspectGroup,
            { title: "INSERT", quick: 3, items: [
                act("Insert Mesh", "insertmesh", () => app.insertMesh(), "Insert Mesh: STL or OBJ file"),
                act("Insert STEP / IGES", "importcad", () => app.importCAD(), "Insert a STEP or IGES file as solid bodies"),
                act("Insert McMaster-Carr", "importcad", () => app.insertMcMaster(), "Search McMaster-Carr and insert a STEP part"),
                act("Export", "export", () => app.exportFile(), "Export STEP / IGES / STL / OBJ / 3MF"),
                act("Batch Export", "export", () => app.openBatchExport(), "Batch Export: several bodies and formats into a folder"),
                act("Export Face as DXF", "dxf", () => app.exportDxf(), "Export a picked planar face as a DXF outline")
            ] },
            { title: "ASSEMBLE", quick: 1, items: [
                cmd("Joint", "joint", "Joint", { asBuilt: false }, "Joint (J): snap a part onto another"),
                cmd("As-built Joint", "asbuiltjoint", "Joint", { asBuilt: true }, "As-built Joint (Shift+J): a joint where the parts already are"),
                act("Component Colors", "facegroups", () => app.toggleComponentColors(), "Component Color Cycling (Shift+N)",
                    () => app.viewport.renderer.componentColors),
                "-",
                planned("New Component", "design", "Phase 6")
            ] },
            selectGroup
        ],
        SURFACE: [
            { title: "CREATE", quick: 3, items: [
                planned("Extrude", "extrude", "Phase 7"), planned("Patch", "splitface", "Phase 7"),
                planned("Offset", "offsetbody", "Phase 7"), planned("Thicken", "shell", "Phase 7")
            ] },
            { title: "MODIFY", quick: 3, items: [
                planned("Trim", "trim", "Phase 7"), planned("Extend", "extend", "Phase 7"), planned("Stitch", "combine", "Phase 7")
            ] },
            constructGroup, inspectGroup, selectGroup
        ],
        MESH: [
            { title: "CREATE", quick: 2, items: [
                act("Insert Mesh", "insertmesh", () => app.insertMesh(), "Insert Mesh: STL or OBJ file"),
                cmd("Tessellate", "tomesh", "MeshConvert", { direction: "toMesh" }, "Tessellate: a solid body becomes a mesh body")
            ] },
            { title: "PREPARE", quick: 3, items: [
                cmd("Repair", "meshrepair", "MeshRepair", undefined, "Repair: weld, orient normals, fill holes"),
                cmd("Generate Face Groups", "facegroups", "MeshFaceGroups", undefined,
                    "Generate Face Groups: split the mesh into coloured groups (planes, cylinders, free-form areas)"),
                cmd("Reverse Normal", "reversenormals", "MeshReverse")
            ] },
            { title: "MODIFY", quick: 3, items: [
                cmd("Plane Cut", "planecut", "MeshPlaneCut"),
                cmd("Remesh", "remesh", "MeshRemesh", undefined, "Remesh: even triangles at a target edge length"),
                cmd("Smooth", "smooth", "MeshSmooth"),
                cmd("Reduce", "reduce", "MeshReduce"),
                cmd("Shell", "shell", "MeshShell", undefined, "Mesh Shell: hollow or thicken a mesh"),
                cmd("Combine", "combine", "MeshCombine", undefined, "Mesh Combine: merge mesh bodies into one"),
                cmd("Reverse Normal", "reversenormals", "MeshReverse")
            ] },
            { title: "CONVERT", quick: 1, items: [
                cmd("Convert Mesh", "tobrep", "MeshConvert", { direction: "toBRep" }, "Convert Mesh: a mesh body becomes a solid")
            ] },
            constructGroup, inspectGroup, selectGroup,
            { title: "EXPORT", quick: 1, items: [
                act("Export", "export", () => app.exportFile(), "Export STL / OBJ / 3MF"),
                act("Batch Export", "export", () => app.openBatchExport(), "Batch Export: several bodies and formats into a folder")
            ] }
        ],
        "SHEET METAL": [
            { title: "CREATE", quick: 4, items: [
                act("Create Sketch", "sketch", () => app.startCommand("Sketch")),
                cmd("Flange", "flange", "BaseFlange", undefined, "Flange: thicken a sketch profile into a sheet"),
                cmd("Edge Flange", "edgeflange", "EdgeFlange", undefined, "Edge Flange: bend a flange up from a straight sheet edge"),
                cmd("Contour Flange", "contourflange", "ContourFlange", undefined,
                    "Contour Flange: an open chain of sketch lines made into a sheet with a bend at every corner"),
                "-",
                cmd("Hem", "hem", "Hem", undefined, "Hem: fold a sheet edge back on itself (flat, open or rolled)"),
                cmd("Tab", "tab", "Tab", undefined, "Tab: add a sketch profile to a sheet in its plane"),
                cmd("Bend", "bend", "Bend", undefined, "Bend: fold a sheet along a sketch line")
            ] },
            { title: "MODIFY", quick: 3, items: [
                act("Sheet Metal Rules", "rulelibrary", () => app.openSheetMetalRules(),
                    "Sheet Metal Rules: thickness, bend radius and K-factor, and a library of named rules"),
                cmd("Unfold", "unfold", "Unfold", undefined, "Unfold: lay the sheet flat in place to cut across bends"),
                cmd("Refold", "refold", "Refold", undefined, "Refold: fold an unfolded sheet back with its new cuts")
            ] },
            { title: "FLAT PATTERN", quick: 2, items: [
                cmd("Create Flat Pattern", "flatpattern", "FlatPattern", undefined, "Flat Pattern: the sheet unfolded beside the model"),
                act("Export DXF", "flatdxf", () => app.exportFlatPatternDxf(),
                    "Export DXF: the flat pattern outline with its bend lines (layer BEND)")
            ] },
            constructGroup, inspectGroup, selectGroup
        ],
        PLASTIC: [
            { title: "CREATE", quick: 2, items: [ planned("Boss", "hole", "Phase 8"), planned("Snap Fit", "rib", "Phase 8") ] },
            { title: "MODIFY", quick: 2, items: [ cmd("Rib", "rib", "Rib"), cmd("Web", "web", "Web"), cmd("Draft", "draft", "Draft") ] },
            constructGroup, inspectGroup, selectGroup
        ],
        RENDER: [
            { title: "APPEARANCE", quick: 2, items: [
                act("Appearance", "properties", () => app.openAppearance(),
                    "Floating Appearance panel — colours and textures on visible / selected bodies (A)"),
                act("Display Settings", "settings", () => app.openDisplaySettings(), "Viewport background colour and zoom options")
            ] },
            { title: "RENDER", quick: 2, items: [
                act("Live Render", "view", () => app.toggleLiveRender(),
                    "Sun / HDRI shading in the design viewport — does not save a PNG"),
                act("Render Settings", "settings", () => app.openRenderSettings(),
                    "Floating panel: HDRI download, lighting, export still image")
            ] },
            selectGroup
        ],
        MANAGE: [
            { title: "MANAGE", quick: 3, items: [
                act("Parameters", "parameters", () => app.openParameters(), "Change Parameters"),
                act("Document Settings", "settings", () => app.openDocumentSettings()),
                act("Appearance", "properties", () => app.openAppearance(), "Assign a material to selected bodies (A)")
            ] },
            selectGroup
        ],
        UTILITIES: [
            { title: "UTILITIES", quick: 4, items: [
                act("Compute All", "finish", () => { app.doc.rebuild(); fissionApp.showMessage("Recomputed the timeline") }, "Compute All (Ctrl+B)"),
                act("Command Search", "parameters", () => app.openCommandSearch(), "Search commands (S)"),
                act("Display Settings", "settings", () => app.openDisplaySettings(), "Viewport background colour (on the fly)"),
                act("Live Render", "view", () => app.toggleLiveRender(), "Live shading in the design viewport"),
                act("Render Settings", "settings", () => app.openRenderSettings(), "Floating Render settings panel"),
                act("Keyboard Shortcuts", "keyboard", () => app.openShortcuts()),
                act("About Fission 360", "info", () => app.openAbout(), "About Fission 360 — version and GPLv3 license")
            ] },
            { title: "ADD-INS", quick: 1 + Math.min(3, fissionScripts.commands.length), items: [
                act("Scripts and Add-Ins", "script", () => app.openScripts(),
                    "Scripts and Add-Ins (Shift+S): JavaScript and Python with full access to Fission 360")
            ].concat(fissionScripts.commands.map(c => act(c.label, "addin", () => app.runAddInCommand(c.id),
                                                          c.label + ": " + c.tooltip + " (" + c.addIn + ")"))) },
            selectGroup
        ],
        MANUFACTURE: [
            { title: "ADDITIVE", quick: 1, items: [
                act("Send to Slicer", "orca", () => app.sendToOrca(),
                    "Export the selected (or all) VISIBLE bodies to a temp STL and open OrcaSlicer or Bambu Studio (Display Settings \u25B8 Slicer). Hidden bodies are never sent")
            ] },
            { title: "SETUP", quick: 3, items: [
                act("New Setup", "plane", () => app.mfgNewSetup(), "Create a manufacture setup (stock from body bounds)"),
                act("Refresh Stock", "plane", () => app.mfgRefreshStock(), "Rebuild stock box from body bounds + offset"),
                act("Tool Library", "settings", () => app.openManufacture(1), "Edit tools saved with this design"),
                act("Manufacture Panel", "settings", () => app.openManufacture(), "Open the Manufacture panel")
            ] },
            { title: "2D MILL", quick: 4, items: [
                act("Face", "extrude", () => app.mfgAddOp("face", "Face")),
                act("2D Adaptive", "extrude", () => app.mfgAddOp("2d-adaptive", "2D Adaptive")),
                act("2D Pocket", "extrude", () => app.mfgAddOp("2d-pocket", "2D Pocket")),
                act("2D Contour", "extrude", () => app.mfgAddOp("2d-contour", "2D Contour")),
                act("Slot", "extrude", () => app.mfgAddOp("slot", "Slot")),
                act("Trace", "extrude", () => app.mfgAddOp("trace", "Trace")),
                act("Bore", "hole", () => app.mfgAddOp("bore", "Bore")),
                act("Circular", "circle", () => app.mfgAddOp("circular", "Circular")),
                act("Engrave", "extrude", () => app.mfgAddOp("engrave", "Engrave")),
                act("Chamfer", "chamfer", () => app.mfgAddOp("chamfer", "Chamfer")),
                act("Thread", "hole", () => app.mfgAddOp("thread", "Thread"))
            ] },
            { title: "DRILL", quick: 2, items: [
                act("Drill", "hole", () => app.mfgAddOp("drill", "Drill")),
                act("Tap", "hole", () => app.mfgAddOp("tap", "Tap")),
                act("Thread Mill", "hole", () => app.mfgAddOp("thread-mill", "Thread Mill"))
            ] },
            { title: "3D MILL", quick: 3, items: [
                act("Adaptive Clearing", "extrude", () => app.mfgAddOp("3d-adaptive", "Adaptive Clearing")),
                act("Pocket Clearing", "extrude", () => app.mfgAddOp("3d-pocket", "Pocket Clearing")),
                act("Parallel", "extrude", () => app.mfgAddOp("parallel", "Parallel")),
                act("3D Contour", "extrude", () => app.mfgAddOp("3d-contour", "3D Contour")),
                act("Flat", "extrude", () => app.mfgAddOp("flat", "Flat")),
                act("Scallop", "extrude", () => app.mfgAddOp("scallop", "Scallop")),
                act("Radial", "extrude", () => app.mfgAddOp("radial", "Radial")),
                act("Spiral", "extrude", () => app.mfgAddOp("spiral", "Spiral")),
                act("Morphed Spiral", "extrude", () => app.mfgAddOp("morphed-spiral", "Morphed Spiral")),
                act("Pencil", "extrude", () => app.mfgAddOp("pencil", "Pencil")),
                act("Ramp", "extrude", () => app.mfgAddOp("ramp", "Ramp")),
                act("Steep & Shallow", "extrude", () => app.mfgAddOp("steep-shallow", "Steep & Shallow")),
                act("Project", "extrude", () => app.mfgAddOp("project", "Project")),
                act("Morph", "extrude", () => app.mfgAddOp("morph", "Morph")),
                act("Flow", "extrude", () => app.mfgAddOp("flow", "Flow")),
                act("Blend", "extrude", () => app.mfgAddOp("blend", "Blend"))
            ] },
            { title: "MULTI-AXIS", quick: 2, items: [
                act("Swarf", "extrude", () => app.mfgAddOp("swarf", "Swarf")),
                act("Multi-Axis Contour", "extrude", () => app.mfgAddOp("multi-contour", "Multi-Axis Contour")),
                act("Rotary / Wrap", "revolve", () => app.mfgAddOp("rotary", "Rotary / Wrap"))
            ] },
            { title: "TURNING", quick: 3, items: [
                act("Facing", "revolve", () => app.mfgAddOp("turning-face", "Facing")),
                act("Profiling", "revolve", () => app.mfgAddOp("turning-profile", "Profiling")),
                act("Grooving", "revolve", () => app.mfgAddOp("turning-groove", "Grooving")),
                act("Parting", "revolve", () => app.mfgAddOp("turning-part", "Parting")),
                act("Threading", "revolve", () => app.mfgAddOp("turning-thread", "Threading")),
                act("Adaptive Roughing", "revolve", () => app.mfgAddOp("turning-adaptive", "Adaptive Roughing"))
            ] },
            { title: "PROBE", quick: 1, items: [
                act("Probe WCS", "measure", () => app.mfgAddOp("probe-wcs", "Probe WCS")),
                act("Probe Geometry", "measure", () => app.mfgAddOp("probe-geometry", "Probe Geometry"))
            ] },
            { title: "FINISH", quick: 2, items: [
                act("Simulate", "view", () => app.mfgSimulate(), "Estimate toolpath length for the active setup"),
                act("Post Process", "export", () => app.mfgPostProcess(), "Export op-aware NC stub for the active setup")
            ] },
            selectGroup
        ],
        DRAWING: [
            { title: "CREATE", quick: 4, items: [
                act("Base View", "plane", () => {
                    if (app.drawingWorkspace) app.drawingWorkspace.beginBaseView("front")
                }, "Place a base view — pick orientation, click the sheet"),
                act("Projected View", "planeoffset", () => {
                    if (app.drawingWorkspace) app.drawingWorkspace.beginProjectMode()
                }, "Shift+drag from a parent view toward the projection (Fusion-style)"),
                act("Section View", "section", () => {
                    if (!app.drawingWorkspace || !app.drawingWorkspace.selectedView) {
                        fissionApp.showMessage("Select a parent view first"); return
                    }
                    app.drawingWorkspace.activeTool = "sectionView"
                    fissionApp.showMessage("Click the sheet to place a section view")
                }, "Place a section view from the selected parent"),
                act("Detail View", "circle", () => {
                    if (!app.drawingWorkspace || !app.drawingWorkspace.selectedView) {
                        fissionApp.showMessage("Select a parent view first"); return
                    }
                    app.drawingWorkspace.activeTool = "detailView"
                    fissionApp.showMessage("Click the sheet to place a detail view")
                }, "Place a detail view from the selected parent"),
                act("Iso View", "plane", () => {
                    if (app.drawingWorkspace) app.drawingWorkspace.beginBaseView("isometric")
                }, "Place an isometric base view"),
                act("Update from Design", "finish", () => app.updateActiveDrawingFromDesign(),
                    "Rebuild all views from the current design and clear out-of-date"),
                act("Regenerate Sheet", "finish", () => app.drawingWorkspace && app.drawingWorkspace.regenerateAll(),
                    "Rebuild views on the active sheet from the design"),
                act("Fit Sheet", "view", () => app.drawingWorkspace && app.drawingWorkspace.fitSheet(),
                    "Reset pan/zoom to fit the sheet (F)")
            ] },
            { title: "ANNOTATE", quick: 3, items: [
                act("Dimension", "measure", () => {
                    if (app.drawingWorkspace) {
                        app.drawingWorkspace.activeTool = "dimension"
                        fissionApp.showMessage("Drag on the sheet to place a dimension")
                    }
                }, "Drag on the sheet to place a linear dimension"),
                act("Text", "script", () => {
                    if (app.drawingWorkspace) app.drawingWorkspace.activeTool = "text"
                }, "Click to place a text note"),
                act("Hole Note", "hole", () => {
                    if (app.drawingWorkspace) {
                        app.drawingWorkspace.activeTool = "text"
                        fissionApp.showMessage("Click to place a hole note")
                    }
                }, "Place a hole / callout note"),
                act("Center Mark", "circle", () => {
                    if (app.drawingWorkspace) app.drawingWorkspace.activeTool = "centerMark"
                }, "Click to place a center mark"),
                act("Image", "export", () => app.drawingWorkspace && app.drawingWorkspace.insertImage(),
                    "Insert a raster image on the sheet")
            ] },
            { title: "TABLE", quick: 2, items: [
                act("Parts List", "parameters", () => app.drawingWorkspace && app.drawingWorkspace.addPartsList(),
                    "Place a parts list (BOM) from the design bodies"),
                act("Revision Table", "parameters", () => app.drawingWorkspace && app.drawingWorkspace.addRevisionTable(),
                    "Place a revision table"),
                act("Bend Table", "bend", () => app.drawingWorkspace && app.drawingWorkspace.addBendTable(),
                    "Place a sheet-metal bend table")
            ] },
            { title: "EXPORT", quick: 2, items: [
                act("PDF", "export", () => app.drawingWorkspace && app.drawingWorkspace.exportPdf(), "Export the sheet to PDF"),
                act("DXF", "dxf", () => app.drawingWorkspace && app.drawingWorkspace.exportDxf(), "Export the sheet to DXF"),
                act("PNG", "export", () => app.drawingWorkspace && app.drawingWorkspace.exportPng(), "Export the sheet to PNG"),
                act("TIFF", "export", () => app.drawingWorkspace && app.drawingWorkspace.exportTiff(), "Export the sheet to TIFF")
            ] },
            { title: "SHEET", quick: 1, items: [
                act("Back to Design", "finish", () => app.leaveDrawing(), "Leave the drawing and return to the design")
            ] }
        ]
    })
    // RENDER and MANUFACTURE are workspaces under DESIGN ▾ (Fusion-style), not Design ribbon tabs
    readonly property var tabNames: {
        if (!toolbar.app) return ["SOLID"]
        if (toolbar.app.inDrawing) return ["DRAWING"]
        if (toolbar.app.workspace === "render") return ["RENDER"]
        if (toolbar.app.workspace === "manufacture") return ["MANUFACTURE"]
        return ["SOLID", "SURFACE", "MESH", "SHEET METAL", "PLASTIC", "MANAGE", "UTILITIES"]
    }

    // ------------------------------------------------------------------ components

    component Separator: Rectangle {
        Layout.preferredWidth: 1
        Layout.fillHeight: true
        Layout.topMargin: 6
        Layout.bottomMargin: 6
        color: Theme.border
    }

    // Group of buttons with a caption underneath (sketch groups)
    component Group: ColumnLayout {
        property string title
        default property alias buttons: row.data
        spacing: 0
        Layout.fillHeight: true
        RowLayout {
            id: row
            spacing: 1
            Layout.topMargin: 4
        }
        Item { Layout.fillHeight: true }
        Text {
            text: parent.title
            Layout.alignment: Qt.AlignHCenter
            Layout.bottomMargin: 4
            font.pixelSize: 10
            font.bold: true
            color: Theme.textDim
        }
    }

    // Group built from a tool list: quick buttons + "TITLE ▾" menu with every tool
    component DataGroup: ColumnLayout {
        id: dg
        property var group: ({ title: "", quick: 0, items: [] })
        readonly property var tools: group.items.filter(i => i !== "-")
        readonly property bool pinnedGroup: !!group.pinnedGroup
        spacing: 0
        Layout.fillHeight: true
        RowLayout {
            spacing: 1
            Layout.topMargin: 4
            Repeater {
                model: dg.tools.slice(0, dg.group.quick).filter(t => dg.pinnedGroup
                    || !toolbar.hidden.includes(toolbar.tab + "|" + dg.group.title + "|" + t.text))
                ToolbarButton {
                    required property var modelData
                    text: modelData.text.length > 10 ? modelData.text.split(" ")[0] : modelData.text
                    glyph: modelData.glyph
                    tip: modelData.planned ? modelData.text + " (planned: " + modelData.planned + ")" : modelData.tip
                    checked: modelData.checked ? (toolbar.app.rev, toolbar.app.selectionTool, toolbar.app.showProperties,
                                                  toolbar.app.analysis, toolbar.app.sectionEnabled, modelData.checked()) : false
                    opacity: modelData.planned ? 0.45 : 1
                    onClicked: toolbar.runItem(modelData)
                    onRightClicked: toolbar.showToolMenu(modelData, dg.group.title, dg.pinnedGroup ? "pinned" : "quick")
                }
            }
        }
        Item { Layout.fillHeight: true }
        AbstractButton {
            id: menuButton
            objectName: "groupMenu_" + dg.group.title
            Layout.alignment: Qt.AlignHCenter
            Layout.bottomMargin: 2
            hoverEnabled: true
            focusPolicy: Qt.NoFocus
            padding: 2
            leftPadding: 6
            rightPadding: 6
            contentItem: Text {
                text: dg.group.title + " \u25be"
                font.pixelSize: 10
                font.bold: true
                color: menuButton.hovered || menu.visible ? Theme.accent : Theme.textDim
            }
            background: Rectangle { radius: 2; color: menuButton.hovered || menu.visible ? Theme.hover : "transparent" }
            onClicked: menu.visible ? menu.close() : menu.popup(menuButton, 0, menuButton.height)
        }
        Menu {
            id: menu
            objectName: "groupMenuPopup_" + dg.group.title
            width: 250
            Instantiator {
                model: dg.group.items
                delegate: MenuItem {
                    id: mi
                    required property var modelData
                    readonly property bool sep: modelData === "-"
                    readonly property var tool: sep ? null : modelData
                    text: tool ? tool.text + (tool.planned ? "  (" + tool.planned + ")" : "") : ""
                    enabled: !sep
                    implicitHeight: sep ? 9 : 30
                    contentItem: Item {
                        Rectangle {
                            visible: mi.sep
                            anchors.verticalCenter: parent.verticalCenter
                            width: parent.width
                            height: 1
                            color: Theme.border
                        }
                        Row {
                            visible: !mi.sep
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: 8
                            Glyph {
                                name: mi.tool ? mi.tool.glyph : "box"
                                width: 18
                                height: 18
                                color: mi.tool && mi.tool.planned ? "#a0a6ad" : Theme.header
                                anchors.verticalCenter: parent.verticalCenter
                            }
                            Text {
                                text: mi.text
                                color: mi.tool && mi.tool.planned ? Theme.textDim : Theme.text
                                font.pixelSize: 12
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }
                        MouseArea {
                            anchors.fill: parent
                            acceptedButtons: Qt.RightButton
                            enabled: !mi.sep
                            onClicked: {
                                menu.close()
                                toolbar.showToolMenu(mi.tool, dg.group.title, dg.pinnedGroup ? "pinned" : "menu")
                            }
                        }
                    }
                    onTriggered: toolbar.runItem(tool)
                }
                onObjectAdded: (index, obj) => menu.insertItem(index, obj)
                onObjectRemoved: (index, obj) => menu.removeItem(obj)
            }
        }
    }

    component SketchTool: ToolbarButton {
        property var app
        property string tool
        checked: app && app.sketchTool === tool
        glyph: tool
        onClicked: app.sketchTool = app.sketchTool === tool ? "" : tool
    }

    // ------------------------------------------------------------------ layout

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // Workspace + tab row
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 26
            color: Theme.header
            RowLayout {
                anchors.fill: parent
                spacing: 0
                Rectangle {
                    Layout.preferredWidth: Math.max(86, workspaceLabel.implicitWidth + 20)
                    Layout.fillHeight: true
                    color: Qt.darker(Theme.header, 1.15)
                    Text {
                        id: workspaceLabel
                        anchors.centerIn: parent
                        text: {
                            if (!toolbar.app) return "DESIGN \u25be"
                            if (toolbar.app.inDrawing) return "DRAWING \u25be"
                            if (toolbar.app.workspace === "render") return "RENDER \u25be"
                            if (toolbar.app.workspace === "manufacture") return "MANUFACTURE \u25be"
                            return "DESIGN \u25be"
                        }
                        color: Theme.headerText
                        font.bold: true
                        font.pixelSize: 11
                    }
                    MouseArea {
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: designMenu.open()
                    }
                    Menu {
                        id: designMenu
                        y: parent.height
                        MenuItem {
                            text: "Design"
                            checkable: true
                            checked: !!(toolbar.app && toolbar.app.workspace === "design" && !toolbar.app.inDrawing)
                            onTriggered: toolbar.app.enterDesignWorkspace()
                        }
                        MenuItem {
                            text: "Render"
                            checkable: true
                            checked: !!(toolbar.app && toolbar.app.workspace === "render" && !toolbar.app.inDrawing)
                            onTriggered: toolbar.app.enterRenderWorkspace()
                        }
                        MenuItem {
                            text: "Manufacture"
                            checkable: true
                            checked: !!(toolbar.app && toolbar.app.workspace === "manufacture" && !toolbar.app.inDrawing)
                            onTriggered: toolbar.app.enterManufactureWorkspace()
                        }
                        MenuSeparator {}
                        MenuItem {
                            text: "Create Drawing"
                            enabled: !!(toolbar.app && toolbar.app.doc && !toolbar.app.inDrawing)
                            onTriggered: toolbar.app.createDrawingFromDesign()
                        }
                        MenuItem {
                            text: "Back to Design"
                            enabled: !!(toolbar.app && toolbar.app.inDrawing)
                            onTriggered: {
                                toolbar.app.leaveDrawing()
                                toolbar.app.enterDesignWorkspace()
                            }
                        }
                    }
                }
                Repeater {
                    model: toolbar.sketching ? ["SOLID", "SKETCH"] : toolbar.tabNames
                    AbstractButton {
                        id: tabButton
                        required property string modelData
                        objectName: "tab_" + modelData
                        readonly property bool current: toolbar.shownTab === modelData
                        Layout.fillHeight: true
                        hoverEnabled: true
                        focusPolicy: Qt.NoFocus
                        leftPadding: 14
                        rightPadding: 14
                        contentItem: Text {
                            text: tabButton.modelData
                            color: tabButton.current ? "white" : tabButton.hovered ? "white" : Qt.rgba(1, 1, 1, 0.65)
                            font.bold: tabButton.current
                            font.pixelSize: 11
                            verticalAlignment: Text.AlignVCenter
                        }
                        background: Rectangle {
                            color: tabButton.current ? (tabButton.modelData === "SKETCH" ? Theme.sketch : Theme.accent)
                                 : tabButton.hovered ? Qt.lighter(Theme.header, 1.25) : "transparent"
                        }
                        onClicked: if (!toolbar.sketching) toolbar.tab = modelData
                    }
                }
                Item { Layout.fillWidth: true }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: 6
            spacing: 6

            // ---------------- pinned tools (right-click any tool → Pin to Toolbar)
            DataGroup {
                visible: !toolbar.sketching && !toolbar.app.inDrawing && toolbar.pinnedGroup.items.length > 0
                group: toolbar.pinnedGroup
            }
            Separator { visible: !toolbar.sketching && !toolbar.app.inDrawing && toolbar.pinnedGroup.items.length > 0 }

            // ---------------- workspace tabs
            Repeater {
                model: toolbar.sketching ? [] : (toolbar.tabs[toolbar.tab] || [])
                RowLayout {
                    required property var modelData
                    required property int index
                    Layout.fillHeight: true
                    spacing: 6
                    Separator { visible: index > 0 }
                    DataGroup { group: modelData }
                }
            }

            // ---------------- SKETCH (contextual)
            Group {
                visible: toolbar.sketching
                title: "CREATE"
                SketchTool { app: toolbar.app; text: "Line"; tool: "line"; tip: "Line (L)" }
                SketchTool { app: toolbar.app; text: "Center Line"; tool: "centerline"; tip: "Center Line: click the midpoint, then an end" }
                SketchTool { app: toolbar.app; text: "Rectangle"; tool: "rectangle"; tip: "2-Point Rectangle (R)" }
                SketchTool { app: toolbar.app; text: "Center Rect"; tool: "centerrectangle"; tip: "Center Rectangle: click the center, then a corner" }
                SketchTool { app: toolbar.app; text: "Circle"; tool: "circle"; tip: "Center Diameter Circle (C)" }
                SketchTool { app: toolbar.app; text: "Arc"; tool: "arc"; tip: "3-Point Arc" }
                SketchTool { app: toolbar.app; text: "Polygon"; tool: "polygon"; tip: "Circumscribed Polygon" }
                SketchTool { app: toolbar.app; text: "Slot"; tool: "slot"; tip: "Center to Center Slot" }
                SketchTool { app: toolbar.app; text: "Ellipse"; tool: "ellipse" }
                SketchTool { app: toolbar.app; text: "Spline"; tool: "spline"; tip: "Fit Point Spline (double-click to finish)" }
                SketchTool { app: toolbar.app; text: "Point"; tool: "point" }
            }
            Separator { visible: toolbar.sketching }
            Group {
                visible: toolbar.sketching
                title: "MODIFY"
                SketchTool { app: toolbar.app; text: "Trim"; tool: "trim"; tip: "Trim (T): click a piece to trim, or drag to paint-delete curves" }
                SketchTool { app: toolbar.app; text: "Extend"; tool: "extend"; tip: "Extend: click near the end of a line or arc" }
                SketchTool { app: toolbar.app; text: "Offset"; tool: "offset"; tip: "Offset (O): click a curve chain, then the side" }
                SketchTool { app: toolbar.app; text: "Fillet"; tool: "fillet"; glyph: "sketchfillet"; tip: "Sketch Fillet: click two lines" }
                SketchTool { app: toolbar.app; text: "Project"; tool: "project"; tip: "Project (P): click body edges or faces" }
                ToolbarButton {
                    text: "Move/Copy"
                    glyph: "move"
                    tip: "Move/Copy (M): move or copy the selected sketch curves"
                    onClicked: toolbar.app.startSketchMove()
                }
            }
            Separator { visible: toolbar.sketching }
            Group {
                visible: toolbar.sketching
                title: "CONSTRAINTS"
                SketchTool { app: toolbar.app; text: "Dimension"; tool: "dimension"; tip: "Sketch Dimension (D)" }
                GridLayout {
                    rows: 2
                    rowSpacing: 0
                    columnSpacing: 0
                    flow: GridLayout.TopToBottom
                    Repeater {
                        model: [["coincident", "Coincident"], ["horizontal", "Horizontal"], ["vertical", "Vertical"],
                                ["parallel", "Parallel"], ["perpendicular", "Perpendicular"], ["collinear", "Collinear"],
                                ["tangent", "Tangent"], ["equal", "Equal"], ["concentric", "Concentric"],
                                ["midpoint", "Midpoint"], ["symmetric", "Symmetry"], ["fix", "Fix / Unfix"]]
                        SketchTool {
                            required property var modelData
                            app: toolbar.app
                            compact: true
                            implicitWidth: 28
                            implicitHeight: 28
                            tool: "constraint:" + modelData[0]
                            tip: modelData[1]
                        }
                    }
                }
            }
            Separator { visible: toolbar.sketching }
            Group {
                visible: toolbar.sketching
                title: "OPTIONS"
                ToolbarButton {
                    text: "Construction"
                    glyph: "construction"
                    checked: toolbar.app.sketchConstruction
                    tip: "New curves are construction geometry (X)"
                    onClicked: toolbar.app.sketchConstruction = !toolbar.app.sketchConstruction
                }
                ToolbarButton {
                    text: "Look At"
                    glyph: "home"
                    onClicked: toolbar.app.viewport.renderer.lookAtSketch(toolbar.app.activeSketchId, true)
                }
                ToolbarButton { text: "Params"; glyph: "parameters"; onClicked: toolbar.app.openParameters() }
                ToolbarButton { text: "DXF"; glyph: "dxf"; tip: "Export the sketch as DXF"; onClicked: toolbar.app.exportDxf() }
            }
            Separator { visible: toolbar.sketching }
            Group {
                visible: toolbar.sketching
                title: "FINISH"
                ToolbarButton {
                    text: "Finish Sketch"
                    glyph: "finish"
                    onClicked: toolbar.app.finishSketch()
                }
            }
            Separator { visible: toolbar.sketching }
            DataGroup { visible: toolbar.sketching; group: toolbar.selectGroup }

            Item { Layout.fillWidth: true }
        }
    }
}
