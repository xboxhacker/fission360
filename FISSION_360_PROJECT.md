# Fission 360 — Project Overview

## Goal
Build "Fission 360" — an offline CAD app that works like Autodesk Fusion 360, written from scratch. A working CAD app with a Fusion-style ribbon (workspace tabs, group menus), floating draggable dialogs, in-viewport sketch drawing with dimension text boxes, in-viewport feature drag handles (extrude distance and taper, fillet, chamfer, hole depth) that take typed values, planar face selection for sketch planes and for direct face extrudes / revolves / sweeps, the full set of Fusion SOLID modelling tools (including Joint / As-built Joint on bodies and component colour cycling), inspection (measure, interference, section, surface analyses), mesh bodies, Fusion's selection tools and keyboard shortcuts, document units, 2D drawings from the design, and Render & Appearance (materials, HDRI / IBL, PBR maps, path tracer).

**IP rule:** Fusion's behaviour, UI layout and public API documentation are used as reference only. No Autodesk code, QML, icons or other assets are copied into Fission 360, and its binaries are not reverse-engineered for code. `docs/fusion_*` holds Autodesk's copyrighted files: it is local reference material and must never be pushed to a public repository.

## Tech Stack
- C++20 + Qt6 QML + OpenCASCADE 7.6 — matches Fusion's real stack (Qt5 QML + C++ kernel)
- CMake + MSBuild (VS2022 BuildTools only, no full VS)
- Use OrcaSlicer's existing OCCT deps instead of fresh install (vcpkg not on PATH)
- QML compiled into the exe with `qt_add_resources` (no `resources.qrc` file) — no external file loading
- Build: the Release configuration in `build-rel/` (`build-rel\Release\Fission360.exe`). CMakeLists defaults single-config generators to RelWithDebInfo, but the Visual Studio generator is multi-config and Release is what is built and tested.
- Floating panels (`FloatingPanel.qml`) instead of a fixed right panel — the viewport gets the full width
- In-viewport Canvas2D overlays for the sketch preview, dimension text boxes, constraint annotations and feature drag handles
- MouseArea-based drag for floating panels instead of DragHandler (DragHandler API issues in Qt6)
- Picking: `BRepRenderer::pick(x, y, filter)` ray-casts the tessellation; sketch planes come from `planeRef()`, which accepts origin planes, construction planes and planar faces only
- Region selection: `BRepRenderer::pickRegion(polygon, crossing, filter)` projects the tessellation to the screen and tests it against the window box or freeform loop; a coarse depth grid of opaque body faces skips items hidden behind others (sketch curves and profiles stay pickable). Paint selection repeats `pick` along the drag
- Face references (`{kind: "face", body, index, count, x, y, z}`) keep the face index plus its centroid, so a reference survives topology changes earlier in the timeline. Extrude / Revolve / Sweep `profiles` hold either sketch profile indices or such face refs
- Simple feature dialogs are data: `SpecDialog.qml` holds a list of field specs per feature type (selection / number / choice / bool / vector / note / types / point3) and `FeatureDialog.qml` renders them (incl. `pull` and `origin` selection kinds for Draft and Joint)
- Mesh bodies are ordinary bodies whose shape is one `TopoDS_Face` carrying only a `Poly_Triangulation` (no surface). `MeshUtils::isMesh` guards every OCCT call that needs real geometry; the mesh is stored in the feature's props (float32, `qCompress`, base64)
- Analyses and section cuts are done in the renderer's shaders (`analysis`, `pullDirection`, `draftAngle`, `sectionEnabled/Normal/Offset` on `BRepRenderer`), so they cost no kernel work

## Architecture

### Directory Structure (as built)
```
fission360/
├── CMakeLists.txt          # C++ sources are globbed; QML files are listed in qt_add_resources (and in qmldir).
│                           # Re-run `cmake -S . -B build-rel` after adding either
├── src/
│   ├── main.cpp
│   ├── kernel/             # OCCT-based CAD kernel
│   │   ├── FissionDocument.h/cpp      # bodies, timeline, parameters, save/load, export, undo,
│   │   │                              # documentId / contentRevision, linkedAssets, version revert;
│   │   │                              # save = Compact JSON once, chunked write + FissionProgress
│   │   ├── DocumentVersionStore.h/cpp # embedded .f360 history helpers + AppData autosave (+ legacy sidecar migrate)
│   │   ├── FissionTimeline.h/cpp
│   │   ├── FissionFeature.h/cpp       # base class: props, rebuild, drag-handle (manipulator) data
│   │   ├── FissionBody.h/cpp
│   │   ├── FissionSketch.h/cpp, SketchSolver.h/cpp, SketchEdit.cpp
│   │   ├── FissionExtrude, FissionRevolve, FissionFillet (constant / variable radius), FissionChamfer (header only),
│   │   │   FissionShell, FissionLoft (rail / centerline guide), FissionSweep (twist), FissionTransform (mirror, patterns),
│   │   │   FissionConstruction (plane, axis, point)      # .h/.cpp each
│   │   ├── FissionHole.h/cpp          # Hole + Thread; ThreadUtils (ISO coarse/fine, UNC/UNF, clearance / tap drill, helical groove)
│   │   ├── FissionSolidTools.h/cpp    # Combine, Move, Scale, Split Body/Face, Offset Face, Offset, Replace Face,
│   │   │                              # Draft (plane / edge / axis pull), Joint / As-built Joint, Pattern on Path, Rib, Web
│   │   ├── MeshUtils.h/cpp            # mesh bodies: triangles-only faces, STL/OBJ read, smooth, reduce, plane cut, to B-rep
│   │   ├── MeshFit.cpp                # mesh -> solid with fitted planes / cylinders / cones / spheres / B-spline faces (prismatic) or merged facets
│   │   ├── FissionProgress.h/cpp      # thread-safe progress hub for long operations (status bar progress bar)
│   │   ├── FissionMeshFeatures.h/cpp  # Insert Mesh, Convert, Reverse, Smooth, Reduce, Plane Cut; MeshBackgroundJob (worker thread + cache)
│   │   ├── FissionImport.h/cpp        # Insert STEP / IGES (ImportCAD feature, shape stored in the design)
│   │   ├── mcmaster/McMasterCarrClient.h/cpp  # Insert McMaster-Carr: Edge cookie bootstrap, Suggest search,
│   │   │                                      # ProductContent STEP URL, download cache → importCAD
│   │   ├── FissionSheetMetal.h/cpp    # Flange, Edge Flange, Contour Flange, Hem, Tab, Bend, Flat Pattern,
│   │   │                              # Unfold / Refold, flat pattern DXF; sheet metal rule parameters
│   │   ├── FissionInspect.cpp         # FissionDocument::measure, ::interference
│   │   ├── FissionParameter.h/cpp, Expression.h/cpp
│   │   ├── FissionTypes.h, GeomUtils.h/cpp
│   │   ├── drawing/                   # DrawingManager, DrawingDocument, DrawingSheet, DrawingView,
│   │   │                              # DrawingSketch, DrawingProjector (HLR)
│   │   └── render/                    # Appearance, RenderSettings, AppearanceLibrary,
│   │                                  # RenderEnvironmentLibrary, RenderAssetStore (download/cache),
│   │                                  # HdrImage (Radiance RGBE), PathTracer (CPU Monte Carlo)
│   ├── ui/                 # BRepRenderer (OpenGL + picking, region occlusion, PBR-ish shading, IBL /
│   │                       # triplanar textures, component colour cycling, viewport background),
│   │                       # ViewportController, SelectionManager
│   ├── api/                # FissionApplication (QML engine, file dialogs, messages, version, drawingManager),
│   │                       # ScriptHost (scripts + add-ins: `fissionScripts` in QML), PythonHost (embedded CPython),
│   │                       # settings; app/ShellIntegration (.f360 file type + Explorer preview registration)
│   ├── app/                # SelfTest (--selftest), UiTestDriver (--uitest, exportGif)
│   ├── util/               # GifWriter (animated walkthrough export)
│   ├── python/, scripts/   # empty (Python scripting lives in api/PythonHost)
├── shell/                  # F360Thumbnail.cpp/.def → Fission360Thumbs.dll (Explorer thumbnail handler for .f360);
│                           # Fission360.rc (app icon, its own Fission360Res target)
├── assets/icons/           # app icon (fission360.ico/.png, drawn by make_icon.py)
├── assets/qml/
│   ├── main.qml
│   ├── FissionApplication/ # MainWindow, Viewport, Toolbar, BrowserPanel, TimelinePanel, ViewCube,
│   │                       # FloatingPanel, MarkMenu, ParametersDialog, StatusBar, Theme, UiTest,
│   │                       # DocumentSettingsDialog, AppearanceSettingsDialog (= Display Settings + autosave),
│   │                       # VersionHistoryDialog, RecoverAutosaveDialog,
│   │                       # CommandSearch (S key), ShortcutsDialog, InspectPanel, ScriptsDialog,
│   │                       # SheetMetalRulesDialog, McMasterInsertDialog, ProjectsPanel, ...
│   ├── FissionFeatures/    # FeatureDialog (shared form) + Extrude/Revolve/Fillet/Chamfer/Shell/Loft/
│   │                       # Sweep/Mirror/Pattern/Construction dialogs; SpecDialog = field specs for
│   │                       # Hole, Thread, Joint, the other solid tools and the mesh features
│   ├── FissionSketch/      # SketchPanel
│   ├── FissionDrawing/     # DrawingWorkspace, DrawingViewItem, CreateDrawingDialog (Phase 9)
│   └── FissionRender/      # AppearanceDialog (materials / faces / CC0 PBR maps), RenderDialog (HDRI, path trace / viewport)
├── build-rel/              # CMake build tree; exe in build-rel/Release/
├── dist/Fission360/        # runnable copy (robocopy of build-rel/Release without .pdb/.lib)
├── docs/fusion_*           # Autodesk reference material, local only (see IP rule)
├── CHANGELOG.md
└── FISSION_360_PROJECT.md

../F3D_Bulk_Export/         # Fusion 360 add-in (Python) that exports Fusion designs to .f360 — see "Fusion import"
├── F3D_Bulk_Export.py
└── F3D_Bulk_Export.manifest   # JSON; "version" must match EXPORTER_VERSION in the .py
```

### Planned layout (not built yet)
This was the original plan for Phases 4–5. What is built so far lives in the flat files above (`MeshUtils`, `FissionMeshFeatures`, `FissionInspect`, `InspectPanel`, `SpecDialog`); the rest of the list is still the target for the remaining tools. New kernel files follow the `Fission*` naming.
```
├── kernel/
│   └── Mesh/          # Mesh workspace
│       ├── MeshBody.h/cpp
│       ├── MeshFeature.h/cpp
│       ├── MeshRepairFeature.h/cpp
│       ├── MeshSmoothFeature.h/cpp
│       ├── MeshReduceFeature.h/cpp
│       ├── MeshRemeshFeature.h/cpp
│       ├── MeshGenerateFaceGroupsFeature.h/cpp
│       ├── MeshShellFeature.h/cpp
│       ├── MeshCombineFeature.h/cpp
│       ├── MeshPlaneCutFeature.h/cpp
│       ├── MeshConvertFeature.h/cpp
│       └── MeshBodyDisplayOverrides.h/cpp
├── analysis/         # Inspection & Analysis tools
│   ├── Analysis.h/cpp
│   ├── MeasureManager.h/cpp
│   ├── InterferenceAnalysis.h/cpp
│   ├── SectionAnalysis.h/cpp
│   ├── CurvatureCombAnalysis.h/cpp
│   ├── CurvatureMapAnalysis.h/cpp
│   ├── DraftAnalysis.h/cpp
│   ├── EnvironmentMapAnalysis.h/cpp
│   ├── IsoCurveAnalysis.h/cpp
│   ├── ZebraAnalysis.h/cpp
│   ├── AccessibilityAnalysis.h/cpp
│   ├── MinimumRadiusAnalysis.h/cpp
│   └── AnalysisResults.h/cpp
├── ui/
│   └── AnalysisDisplay.h/cpp
├── api/
│   └── FissionAPI.h/cpp
├── qml/
│   ├── inspection/    # Inspection tool dialogs
│   │   ├── MeasureDialog.qml
│   │   ├── InterferenceDialog.qml
│   │   ├── SectionAnalysisDialog.qml
│   │   ├── CurvatureCombDialog.qml
│   │   ├── CurvatureMapDialog.qml
│   │   ├── DraftAnalysisDialog.qml
│   │   ├── EnvMapDialog.qml
│   │   ├── IsoCurveDialog.qml
│   │   ├── ZebraDialog.qml
│   │   ├── AccessibilityDialog.qml
│   │   └── MinRadiusDialog.qml
│   └── mesh/          # Mesh tool dialogs
│       ├── MeshRepairDialog.qml
│       ├── MeshSmoothDialog.qml
│       ├── MeshReduceDialog.qml
│       ├── MeshRemeshDialog.qml
│       ├── MeshFaceGroupsDialog.qml
│       ├── MeshShellDialog.qml
│       ├── MeshCombineDialog.qml
│       ├── MeshPlaneCutDialog.qml
│       └── MeshConvertDialog.qml
```

## Build System Notes
- CMake generator: `"Visual Studio 17 2022" -A x64`
- MSBuild path: `C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe`
- Always use `/m:1` (serial) to avoid D8040 process-limit crash
- OCCT flat headers: `...\OrcaSlicer_dep\usr\local\include\occt` (7034 files)
- OCCT source headers: `...\dep_OCCT\src` (448 subdirs) — both paths needed
- OCCT libs: `...\dep_OCCT-build\lib\occt` (TKernel.lib, TKG2d.lib, TKG3d.lib, TKMath.lib, TKShHealing.lib, TKBRep.lib, TKGeomAlgo.lib, TKGeomBase.lib, TKMesh.lib, TKPrim.lib, TKTopAlgo.lib, TKBO.lib, TKBool.lib, TKHLR.lib, TKOffset.lib, TKFillet.lib, TKFeat.lib)
- Missing OCCT toolkits built as OBJECT libs inside Fission360's CMake: BRepFill, TopOpeBRepTool, TopOpeBRepDS, TopOpeBRepBuild, BRepAlgoAPI, BRepCheck, BRepLib, BRepAlgo, BRepFeat, LocOpe
- Before rebuild: `Stop-Process cl,link,moc -Force`
- Build: `MSBuild.exe build-rel\Fission360.vcxproj /p:Configuration=Release /m:1 /v:minimal` (run `cmake -S . -B build-rel` first after adding a .cpp/.h or a QML file; a new QML file also goes into `qt_add_resources` in CMakeLists.txt and into `assets/qml/FissionApplication/qmldir`). This also builds `Fission360Thumbs.dll` and the icon resource (`Fission360Res`), which the app target depends on
- Qt6 components: Core, Quick, QuickControls2, Svg, Widgets, OpenGL, OpenGLWidgets, **Network** (HDRI / texture downloads), Test
- The icon `.rc` must stay in its own target: compiled inside `Fission360`, rc.exe gets the whole OCCT include list and fails with "The filename or extension is too long"
- Performance: `--uiperf <stl>` scripts insert / hover / orbit / Convert on a mesh and prints `[PERF]` timings (set `FISSION_LOG=<file>` for the slow-operation log); `--meshbench <stl>` times the converter alone (`MESHBENCH_QUICK=1` = only the grouped conversions; `FIT_DEBUG=1` prints fitting statistics, `FIT_TIMING=1` stage times, `FIT_NOMOP=1` skips the facet mop-up, `FIT_CHECK=1` forces the full `BRepCheck` on big meshes; stderr goes to a file best, PowerShell reports it as errors otherwise)
- Tests: `build-rel\Release\Fission360.exe --selftest` (kernel) and `--uitest <screenshot dir>` (scripted UI walkthrough; set `QT_FORCE_STDERR_LOGGING=1` to see QML logs). The walkthrough sends real key presses through `QTest` (`uiTest.key("Ctrl+Alt+B")`) and drives region selection through `Viewport.beginRegion / moveRegion / endRegion`
- dist refresh: `robocopy build-rel\Release dist\Fission360 /E /R:1 /W:1 /XF *.pdb *.ilk *.exp *.lib` (exit code 1 = files copied; `/R:1 /W:1` stops it hanging on a locked exe)
- A self-test or UI-test process left running locks the exe and the next link fails with LNK1104: `Get-Process Fission360 | Stop-Process -Force` before building, and start test runs with a timeout (`$p.WaitForExit(300000)`)

## Current State
*(Source at version **1.13.34** — Shell clean corners (Intersection-first, reject spike faces); 1.13.33 frustum punch + embedded versions; 1.13.32 Report Bug / SolidCraft parity.
- **Version 1.13.34** (`FISSION_VERSION` in `CMakeLists.txt`; bump it every round of changes); Fusion add-in **1.6** / manifest **1.6.0** (`../F3D_Bulk_Export`)
- **License:** GNU GPL-3.0-or-later (`LICENSE`; every source file has an `SPDX-License-Identifier` header); **About Fission 360** shows version, license text and `THIRD_PARTY_NOTICES.md` (Qt/LGPL, WebEngine/Chromium, OCCT, FreeType, slicer trademarks). Ship both files and a source link with every binary release. Build needs `-DFISSION_ORCA_ROOT=<OrcaSlicer checkout with built deps>` (or env var) for OCCT. New source files: add the SPDX header.
- Fusion import: full timeline with BRep history (timeline scrubbing works) and an embedded preview — see "Fusion import" below
- **108 C++** under `src/` (55 `.cpp` / 53 `.h`, incl. `kernel/drawing/`, `kernel/render/` + `PathTracer`, `kernel/mcmaster/`), **53 QML** under `assets/qml/` (incl. `FissionDrawing/`, `FissionRender/`, `McMasterInsertDialog`), Explorer thumbnail handler under `shell/`
- Last built binary: `build-rel\Release\Fission360.exe` from **1.13.12** (links `Qt6::Test` / `Qt6::Network` / `Qt6::WebEngineQuick`). Refresh runnable copy with robocopy into `dist\Fission360\`
- Self-tests: **339/1** at 1.13.6; UI walkthrough last recorded at 163 checks when Drawing scaffolding landed
- **Save**: Compact JSON once, chunked write + progress (large Fusion exports stay responsive); `testLargeFileSave` (~11 MB synthetic)
- **Phases done:** 1–3, 5 (mesh except organic convert), 6 (import/export + Fusion), 7 (sheet metal), 8 (scripts), 9 (drawing), **10 (Render & Appearance)**; Phase 12 **Send to Orca** (thin handoff). **Open:** Phase 4 leftovers (curvature comb, …), Phase 5 organic mesh convert, Phase 11 sync; Phase 12 MFG catalog wired (true CAM kernel / collision sim / mfg model still later)
- **Phase 10 Render & Appearance ✅** — **DESIGN ▾ → Render** workspace; floating Appearance (visible bodies if nothing selected); live viewport shading; HDRI/IBL; path tracer + supersample capture. Sketch: Trim paint-delete drag; Snap to grid toggle — see "Render & Assets" below
- Status bar progress bar for long operations (`FissionProgress` → `fissionApp.busy / progressText / progressValue`); mesh Convert and Face Groups ▸ Accurate of large meshes run on a worker thread and report through it; **Save** reports through it too (large Fusion exports); scripts can run their own tasks (`fission.beginTask / reportProgress / endTask`)
- **Perf (pending rebuild):** McMaster/WebEngine lazy-loaded; shell assoc deferred; rebuild 16 ms / tessellate 32 ms debounce — see CHANGELOG Unreleased
- Parametric timeline with expressions and user parameters; undo/redo; `.f360` save/load (Compact JSON; thumbnail still written as the first key by hand)
- Solid features: Extrude / Revolve / Sweep (sketch profile or flat body face; Sweep twist), Loft (rail or centerline guide; one rail), Fillet (constant or variable radius), Chamfer, Shell, Mirror, Rectangular/Circular Pattern, Pattern on Path, Hole (simple / counterbore / countersink, tapped; Custom or ISO coarse / fine / UNC / UNF size + fit), Thread (modeled or cosmetic; standard size or fit the face), Combine, Move/Copy (incl. free body drag), Scale, Split Body, Split Face, Offset Face, Offset, Replace Face, Draft (fixed plane / parting line; pull from flat or round face, edge or axis), Rib, Web, Joint / As-built Joint (rigid / revolute / slider / cylindrical), construction plane/axis/point
- Ribbon like Fusion's: DESIGN ▾ (incl. **Create Drawing**) plus the SOLID / SURFACE / MESH / SHEET METAL / PLASTIC / **RENDER** / MANAGE / UTILITIES tabs and a contextual DRAWING / SKETCH tab; each group has quick buttons and a "TITLE ▾" menu. SOLID ▸ ASSEMBLE: Joint, As-built Joint, Component Colors (Shift+N); New Component still planned (toolbar placeholder)
- Typing digits while a command shows a size handle goes straight into its value box
- INSPECT: Measure (I) — first pick shows properties, second face/edge/body/point shows minimum distance and angle; Interference; Section Analysis (any plane or planar face, capped, saved in the browser's Analysis folder); Zebra, Draft and Curvature Map analyses
- Feature drag-handle value box is placed clear of the distance arrow and Extrude taper knob
- MESH: Insert Mesh (STL/OBJ), Tessellate, Repair, Remesh, Generate Face Groups (Accurate / Fast — Accurate Boundary Tolerance is a **fraction of the body diagonal**, Fusion-style default 0.001), Mesh Shell, Mesh Combine, Convert Mesh to solid (prismatic fitted surfaces incl. B-spline faces, or faceted; one face per face group), Reverse Normal, Plane Cut, Smooth, Reduce
- DRAWING (Phase 9 ✅): Create Drawing dialog; HLR base / projected / section / detail / auxiliary views; dimensions / text / center marks / images; parts list / revision / bend tables; section hatch; title block; drawing browser + history; PDF / DXF / PNG / TIFF export
- SHEET METAL: Sheet Metal Rules (with a rule library), Flange, Edge Flange, Contour Flange, Hem, Tab, Bend, Unfold / Refold, Create Flat Pattern, flat pattern DXF export
- Scripts and Add-Ins (Shift+S): JavaScript and Python (3.11+, if installed) with full access to the app, document and commands; add-ins run at startup and add buttons to UTILITIES ▸ ADD-INS
- Ctrl+A selects all bodies; Delete removes them with an undoable Remove feature
- Hole: standard size tables + move handle over the face with snaps (face centre, other holes, sketch points); Fillet/Chamfer: a picked face takes all its edges; Draft: pull direction first (face / plane / edge / axis), then faces to tilt; Press Pull (Q) on a face = Offset Face
- While a feature is edited, its result is shown as the full shaded solid and the input body stays pickable
- Sketches: constraint solver, dimensions, line (axis snap, typed length and angle), center line, rectangle, center rectangle, circle, arc, polygon, slot, ellipse, spline (draggable fit points), point; trim, extend, offset, fillet, project; drag editing; DXF export
- Drag handles: extrude distance arrow and taper knob, fillet/chamfer arrows, hole depth arrow, joint offset, construction plane edges; typing digits replaces the handle's value
- Browser: document row (right-click Rename / Document Settings / Save), Document Settings folder ("Units: mm, g", Part Design, Properties), Named Views, origin items (one eye for the whole folder or per item), bodies (right-click Save As Mesh / Hide), sketches, construction geometry, parameters
- Document units: length mm / cm / m / in / ft and mass g / kg / lb / oz, saved in the file. The kernel stays in mm and degrees. A number typed without a unit gets the document unit, and labels and Properties are shown in it
- Extrude / Revolve / Sweep take a sketch profile or a flat body face directly (no sketch needed)
- Selection: click, Shift/Ctrl+click, and left-drag on empty space with Window (1, box: left-to-right inside / right-to-left touching), Freeform (2, lasso) or Paint (3); region picks skip items hidden behind opaque body faces; SELECT group on the toolbar
- Keyboard shortcuts use Fusion's default keys (E, Q, F, H, M, I, V, L, R, C, T, O, P, D, X, **A** Appearance/materials, 1/2/3, Shift+S, Ctrl+A, Del, Ctrl+B, F6, Ctrl+Alt+B/V/N/R, Ctrl+N/O/S/Z/Y, **J** / **Shift+J** joints, **Shift+N** component colours). S opens the command search; INSPECT → Shortcuts lists them all
- Export: STL / OBJ / STEP / IGES for the visible bodies (mesh bodies go to STL / OBJ only); STL / OBJ for one body
- Import: STL / OBJ as mesh bodies (Insert Mesh); STEP / IGES as B-rep bodies (Insert STEP / IGES); **Insert McMaster-Carr** opens an in-app McMaster browser — product pages auto-select 3-D STEP (headless Chrome CDP + cache)
- Windows: `.f360` files get the app icon and an Explorer preview (thumbnail handler registered per user at startup; `--register` / `--unregister`)
- Projects: a projects folder (default `Documents\Fission 360 Projects`) and a pop-out Projects panel (Ctrl+Alt+P) with sub folders, thumbnail tiles (saved in each `.f360`) and right-click Open / Rename / Move to / Duplicate / Export / Show in Explorer / Delete

## `.f360` File Format
- One JSON file (**Compact** encoding since 1.13.6 — Indented froze the UI on large Fusion imports). The preview PNG (base64) is always the **first key**, `"thumbnail"`, so the Projects panel and the Explorer handler read only the head of the file. `FissionDocument::save` inserts it by hand because `QJsonObject` sorts keys, then writes in 1 MiB chunks while pumping the event loop. The same Compact payload feeds version history (no second `toJson`). There is no `.png` sidecar any more (old Fusion exports may still have one; it is only a fallback).
- Native files have `"format": "Fission360"` plus the timeline, parameters, bodies and settings.
- **`bodyDisplay`**: per-body `visible`, `color`, `opacity`, `appearanceId`, `roughness`, `metalness`, `texturePath` / `roughnessPath` / `metalnessPath` / `normalPath`, optional `faceAppearances` (face index → override map).
- **`render`**: document `RenderSettings` (quality, background type/colour, ground, environment path/intensity/rotation, sun, output size/format). Environment path should be a **local** cached `.hdr` for IBL (http URLs are ignored until downloaded).
- Files from the Fusion add-in have `document` / `metadata` instead and are converted on load by `convertFromFusionExport()`.
- Installer to-do: register the file type and `Fission360Thumbs.dll` (run `Fission360.exe --register`, or write the same keys under HKLM for all users), and ship the DLL next to the exe.

## Render & Assets (Phase 10 ✅)
- **UI**: **DESIGN ▾ → Render** enters the Render workspace and turns **Live Render** on (sun/HDRI shading in the design viewport — no PNG prompt). **Live Render** toggles that shading while staying on the RENDER tab; **DESIGN ▾ → Design** leaves Render. **Render Settings** is a floating panel (HDRI + **Export Image…**). **A** opens floating Appearance. Floating-panel ✕ no longer fights the drag strip. With no selection, colours/textures apply to all **visible** bodies.
- **Built-in appearances** (`AppearanceLibrary`): plastics, metals, glass, ceramic, rubber, fabric, wood/stone/concrete. Drive viewport / path-tracer PBR shading.
- **Per-face overrides**: `FissionBody::applyAppearanceToFace` / `setFaceMaps`; `FissionDocument::applyAppearanceToSelection` prefers face picks when present.
- **Downloads** (`RenderAssetStore`, needs network): Poly Haven **CC0** HDRIs (1K–8K `.hdr`) and 1K material packs (`diff` + optional `rough` / `metal` / `nor_gl`). Cache: `%AppData%/FissionCAD/Fission 360/Assets/` → `HDRIs/`, `Materials/`, `Previews/`. Credit: polyhaven.com (CC0).
- **How to use an HDRI**: Render ▸ Lighting → select → resolution → **Download** → **Use**. Sets Environment background + local `environmentPath`.
- **Viewport**: equirect sky + IBL; triplanar albedo / roughness / metalness / normal maps on bodies (and face overrides when set).
- **Offline Render**: Engine **Path Trace (Monte Carlo)** = CPU BVH path tracer (`PathTracer`, quality → spp). Engine **Viewport** = supersampled `grabToImage` (1×…3×).

## Fusion Import (`F3D_Bulk_Export` add-in → Fission)
- **Add-in** (Fusion: UTILITIES ▸ ADD-INS ▸ Scripts and Add-Ins). Modes: Active design / All open designs / Active Data Panel folder (one folder, never a recursive cloud walk — that hangs Fusion). Options: Include STEP, BRep per timeline step, Unhide all, Overwrite.
  - Writes sketches (curves, points, dimensions, constraints, plane), features with their parameters (`<key>` + `<key>Expr`), the timeline, bodies and a viewport preview.
  - **BRep per timeline step**: moves `design.timeline.markerPosition` after each solid step and stores a STEP of the model on that timeline entry (`stepData`, or `stepEmpty` before the first body), then restores the marker. The single full STEP on `bodies[0]` is only written when no step snapshot exists.
  - Since 1.6, each snapshot also has `stepBodies` (`[{name, box [xmin..zmax] mm}]`, B-rep bodies in world coordinates). `meshBodies` holds `[{name, component, isVisible, nodes, triangles}]`, where `nodes` is base64 float32 xyz in mm and `triangles` is base64 int32, both little-endian, with occurrence transforms applied. Mesh bodies are hidden while each STEP is written, because Fusion would otherwise write them as faceted B-rep.
  - Refreshing in Fusion: stop the add-in, then run it again (or restart Fusion); the dialog shows the manifest `version`.
- **Fission side** (`FissionDocument::convertFromFusionExport`):
  - Sketches become real Fission sketches. Extrude / Revolve / Fillet / Chamfer / Shell / Hole become features of the same type with their props. Fusion types with no Fission match become `ImportCAD` steps.
  - Imported solid features carry `fusionPassThrough` (they never recompute, so they stay healthy) and a `snapshot` prop (the step's BRep, encoded like ImportCAD). When one runs, its snapshot replaces the bodies, so moving the timeline marker shows the Fusion model after that step.
  - Exports without snapshots (add-in 1.3 and older) get one ImportCAD from the full STEP at the end.
  - Body names: each solid takes the name written on it in the STEP (Fusion's body name; `read.stepcaf.subshapes.name`). If the solid has no name, it takes the name from the exporter's `stepBodies`, matched by bounding box. Otherwise the earlier positional name is kept. Names that repeat get the component, for example "Body1 (Comp)".
  - Mesh bodies: each `meshBodies` entry becomes an `InsertMesh` at the end of the timeline, after the snapshots that replace the bodies, with props `bodyName` and `replaceFusionCopy` set. A faceted B-rep copy of the same name from the STEP is removed.
- Editing an imported feature's numbers does not change the geometry yet (the snapshot wins); making them recompute natively is open work.

---

## Roadmap

### Phase 1 — Foundation ✅
- [x] CMake project with Qt6 + OCCT
- [x] Document/Component/Body kernel classes
- [x] BRepRenderer with OCCT viewer
- [x] ViewportController, SelectionManager
- [x] Floating Popup dialogs with MouseArea drag
- [x] MainWindow.qml with theme, toolbar, dialogs

### Phase 2 — Solid Modeling ✅
- [x] Extrude (sketch profile or flat body face, direction, distance, taper, both-sides, cut/join/intersect/new-body; dragging into the body switches Join to Cut)
- [x] Revolve (profile, axis, angle)
- [x] Revolve / Sweep from a body face (like Extrude: pick a flat face instead of a profile) *(1.13.4)*
- [x] Press Pull (Q) on a face starts Offset Face (an edge still starts Fillet, a profile Extrude) *(1.13.4)*
- [x] Fillet (edge selection with tangent chains, radius)
- [x] Fillet variable radius (start / end radius along each edge) *(1.13.4)*
- [x] Chamfer (edge selection, distance, angle)
- [x] Loft (sections from sketch profiles and solid faces, ruled option)
- [x] Loft rail or centerline guide (sketch curves in their own sketch; one rail, OCCT takes a single guide) *(1.13.4)*
- [x] Sweep (profile, path)
- [x] Sweep twist angle *(1.13.4)*
- [x] Shell, Mirror, Rectangular/Circular Pattern, construction plane/axis/point
- [x] Hole (simple / counterbore / countersink; distance or all; drill point; tapped with modeled or cosmetic thread; on a face or at sketch points; Custom or ISO coarse / fine / UNC / UNF size + clearance fit) *(standards 1.13.4)*
- [x] Thread (cylindrical face, internal/external detected; Custom or ISO coarse / fine / UNC / UNF, or fit the face; modeled helical groove)
- [x] Combine, Move/Copy (free / translate / rotate / point-to-point / point-to-position; free body drag), Scale, Split Body, Split Face, Offset Face, Offset, Replace Face, Draft (plane / parting line; pull from flat or round face, edge or axis), Pattern on Path, Rib, Web *(Move drag + Draft pull 1.13.4)*
- [x] Fusion-style ribbon tabs and group menus; digits typed straight into the handle value box
- [x] Timeline with undo/redo
- [x] Feature drag handles (extrude distance and taper knob, fillet/chamfer distance, hole depth, joint offset, construction plane edges)
- [x] Full shaded preview of the feature result while editing

### Phase 3 — Sketch Tools ✅
- [x] Line, Center Line, Circle, Rectangle, Center Rectangle, Arc (3-point), Slot, Polygon, Ellipse, Spline, Point
- [x] In-viewport Canvas2D preview overlay
- [x] Dimension text boxes (line length + angle, with 90° axis snap)
- [x] Planar face / origin plane / construction plane selection for the sketch plane
- [x] Sketch panel as floating dialog
- [x] Constraint solver, constraints and driving dimensions
- [x] Trim, Extend, Offset, Fillet, Project; drag editing; spline fit-point handles; DXF export
- [x] Fusion default keyboard shortcuts, S command search, Shortcuts dialog
- [x] Document Settings in the browser: units (length, mass), density, design info; Named Views
- [x] Window / freeform / paint selection (1/2/3)
- [x] 96 self-tests and 87 UI walkthrough checks passing when Phase 3 closed (last verified **339/1** at 1.13.6 — remaining fail is `loft: rail guide` from 1.13.4; UI walkthrough last at 163). Live Render / McMaster ribbon fix in **1.13.7**.
- [x] Region selection skips items hidden behind body faces (coarse depth grid; sketch curves and profiles are always pickable) *(1.13.4)*
- [x] Hole (H) and Move/Copy (M) keys now start their commands; Shift+S opens Scripts; Ctrl+A / Del select and remove bodies; **A** opens Appearance (bodies or faces)
- [x] Joint (J) and As-built Joint (Shift+J) on bodies (rigid / revolute / slider / cylindrical, angle and offset); Shift+N component colour cycling (one hue per body) *(1.13.4)*

### Phase 4 — Inspection & Analysis Tools (started)
- [x] **Measure** — face area / normal / radius, edge length / radius / centre, body volume and area, point position; minimum distance with ΔX/ΔY/ΔZ and the angle between two items
- [x] **Interference** — overlapping volume for each pair of solid bodies (selected bodies or all)
- [x] **Section Analysis** — view cut by the XY / XZ / YZ plane, a construction plane or a planar face, with an offset slider and flip; solids are capped on the cut
- [x] Section analyses saved in the browser (Analysis folder: show / hide, edit, delete; saved with the design)
- [ ] **Curvature Comb Analysis** — display curvature comb along selected edges
- [x] **Curvature Map Analysis** — colour map of curvature on faces
- [x] **Draft Analysis** — green / yellow / red against a pull axis and a minimum angle
- [ ] **Environment Map Analysis** — reflect an environment map on bodies (real-time or frozen)
- [ ] **Isocurve Analysis** — display isoparametric curves on faces
- [x] **Zebra Analysis** — stripe reflection for surface continuity
- [ ] Zebra options (direction, repeats, opacity)
- [ ] **Accessibility Analysis** — colour-map tool accessibility on faces
- [ ] **Minimum Radius Analysis** — find and highlight the smallest radius on a body
- [ ] Zebra / Draft / Curvature analyses saved in the browser like sections (they are transient display modes)
- [x] INSPECT group with dropdown on the ribbon
- [x] I for Measure
- [x] Results in a floating panel (`InspectPanel.qml`)

### Phase 5 — Mesh Workspace (done except organic convert)
- [x] **Mesh bodies** — a body whose face holds only a `Poly_Triangulation`; shown, picked, measured, exported to STL/OBJ like other bodies
- [x] **Insert Mesh** — STL (ASCII / binary) and OBJ in the document units; triangles stored compressed in the design
- [x] **MeshBodyDisplayOverrides** — per-body opacity, colour, visibility
- [x] **Mesh features as parametric timeline items**:
  - [x] **Repair** — weld vertices, remove duplicate triangles, orient normals (outward on closed pieces), fill holes
  - [x] **Reverse Normal**
  - [x] **Smooth** — Taubin smoothing (iterations, smoothness), does not shrink
  - [x] **Reduce** — vertex clustering to a percentage of the triangles
  - [x] **Remesh** — retriangulate (target edge length, adaptivity)
  - [x] **Generate Face Groups** — Type Accurate (fitted planes / cylinders / cones / spheres; Boundary Tolerance = fraction of body diagonal, Fusion default 0.001) or Fast (dihedral angle + min group size); Convert ▸ Prismatic makes one face per group
  - [x] **Mesh Shell** — hollow a closed mesh / thicken an open one, inward or outward along vertex normals
  - [x] **Mesh Combine** — merge multiple mesh bodies into one
  - [x] **Plane Cut** — keeps the side behind the plane, optional fill
  - [x] **Convert** — B-rep → mesh (coarse / medium / fine), closed mesh → solid (up to 300 000 triangles; over 20 000 it runs in the background)
  - [x] Convert mesh → B-rep **prismatic** (fitted planes, cylinders, cones, spheres with line / circle edges) or **faceted** (merged coplanar facets); falls back to faceted, then one face per triangle
  - [x] Prismatic convert recognises extruded free-form walls (one swept face per wall, split at profile corners); large meshes convert in the background with progress in the status bar
  - [x] Prismatic convert makes fitted B-spline faces for free-form face groups (bicubic least-squares height fields, rim-weighted, split across the bend where an area folds)
  - [x] Leftover facets are mopped up when converting **without** face groups; with face groups, Convert keeps one face per group (Fast test stock: 13→13). Accurate (Boundary Tolerance 0.001 × diagonal) yields ~4k groups on the test stock — dense fillet bands like Fusion; sew may fall back to faceted
  - [ ] Convert mesh → B-rep **organic** — remaining: a handful of patches (10 on the test stock with Accurate groups, 1 with Fast) still end as facets where their rims cannot be matched to their neighbours; the strict `BRepCheck` validity is skipped for meshes over 40 000 triangles (fitted faces there still carry large edge tolerances at coarse, long-triangle areas)
- [x] MESH ribbon tab (CREATE, PREPARE, MODIFY, CONVERT, CONSTRUCT, INSPECT, SELECT, EXPORT)
- [x] Floating dialogs for each mesh feature
- [x] Mesh bodies are drawn faceted (flat shading) with every triangle outlined while edges are shown
- [x] Mesh display mode toggle (shaded, wireframe, shaded+edges, vertices, face groups colouring)
- [x] Mesh selection in viewport: face groups pick as faces (vertex / edge snapping on meshes not built)
- [x] Drag handles for mesh operations (Shell thickness, Plane Cut offset along the plane normal)
- [x] Mesh statistics (vertex / triangle counts, open edges, manifold status) in Properties
- [x] Mesh bodies have their own browser icon (solid bodies keep the cube)

### Phase 6 — Export & Import (done)
- [x] STL / OBJ / STEP / IGES export
- [x] STL / OBJ import as mesh bodies (Insert Mesh)
- [x] STEP / IGES import (B-rep; solids become bodies, the shape is stored in the design)
- [x] STEP / IGES assembly structure, colours and names on import (XCAF; bodies named "Assembly/Part")
- [x] 3MF export (3D Manufacturing Format; body names and colours), from Export, Save As Mesh and Batch Export
- [x] DXF 2D export from a planar face or a sketch (`FileExport::DxfWriter`)
- [x] Batch export dialog (select bodies, formats, folder, one file per body or one for all)
- [x] Fusion 360 designs via the `F3D_Bulk_Export` add-in: sketches, full timeline, per-step BRep (scrubbing), embedded preview
- [x] Insert McMaster-Carr (in-app WebEngine catalog → auto 3-D STEP → importCAD; Chrome CDP on Windows) *(1.13.5–1.13.9)*
- [x] About dialog + GPLv3 `LICENSE` *(1.13.9)*; also at the bottom of the File menu *(1.13.12)*
- [x] Imported STEP bodies work with every solid tool; OCCT access violations become feature errors (`OSD::SetSignal` + `/EHa`) instead of crashing; `--stepbench <file>` extrudes every face of a STEP *(1.13.12)*
- [x] Large-file save stays responsive (Compact JSON, chunked write, progress) *(1.13.6)*
- [ ] Imported Fusion features that recompute natively when their values are edited

### Phase 7 — Sheet Metal (done)
- [x] Sheet Metal Rules (SheetThickness, BendRadius, KFactor parameters) with a rule library dialog (presets + saved rules)
- [x] Flange from a sketch profile
- [x] Edge Flange (height, angle, bend radius, flip)
- [x] Contour Flange (open line chain, bends at the corners), Hem (flat / open / rolled), Tab, Bend along a sketch line
- [x] Create Flat Pattern (bend allowance with the K-factor) for every bend, flanges on flanges included
- [x] Unfold / Refold (cuts made flat follow the bends)
- [x] Flat pattern DXF export (outline + `BEND` layer)
- Sheet features implement `SheetMetalPart` (`src/kernel/FissionSheetMetal.h`): base sheet, bends (end face, fold, material beyond) and additions. The flat pattern walks them in timeline order with one rigid transform per region.

### Phase 8 — Scripting (done)
- [x] JavaScript scripts with full access to `app`, `doc` and `fission` (every command and the whole document API)
- [x] Scripts and Add-Ins dialog (Shift+S): list, edit, save, run, output with error lines, runaway-script stop
- [x] Python scripts over the same objects (`src/api/PythonHost.cpp`):
  - CPython 3.11+ stable ABI. `python3.dll` is delay loaded from the user's install (`FISSION_PYTHON` or the registry).
  - Data exports are looked up with `GetProcAddress`, because a delay-loaded DLL can only be called.
  - A generic Qt-object proxy: properties, invokable methods and snake_case aliases.
  - `adsk.core.Application.get()` / `adsk.fusion.Design.cast()` entry points.
  - Built without Python headers, the app reports that Python is unavailable.
- [x] Add-ins (`%APPDATA%\FissionCAD\Fission 360\AddIns`, `.js` / `.py`):
  - They run at startup unless switched off.
  - `addCommand` / `add_command` buttons appear in UTILITIES ▸ ADD-INS.
  - The Scripts dialog has an Add-Ins tab.

### Phase 9 — Drawing / From Design ✅
- [x] **DrawingManager** — `createDrawingInput()`, `createDrawingDocument()`, `projectView`, `regenerateSheet`, PDF/DXF/PNG/TIFF export, parts/revision/bend tables, images
- [x] **CreateDrawingInput** + dialog — ISO/ASME, first/third angle, A0–A4 / A–E / custom size, orientation, title / part / drawn-by / material
- [x] **CustomSheetSize** — `addCustomSheet(widthMm, heightMm, orientation)`
- [x] **Sheet** — name, size, orientation, scale, title block fields, annotations, sketches, tables
- [x] **Sheets** — add / remove / active sheet index; Add Sheet in the drawing browser
- [x] **Views** — base, projected, section, detail, auxiliary via `addView(type, options)` + HLR projection (`DrawingProjector`)
- [x] **View** — type, scale, position, orientation, parent, curves (visible / hidden / section), display style / hidden lines / tangent options
- [x] **DrawingViewOptions** — display style, hidden lines, tangent edge handling on each view
- [x] **Base view** — click to place; OCCT HLR orthographic projection of the design
- [x] **Projected view** — from a selected parent (up/down/left/right) with first- or third-angle mapping
- [x] **Section view** — projected looking-direction view with a cutting-line overlay + ANSI31-style hatch
- [x] **Detail view** — cropped circular region at a higher scale
- [x] **Auxiliary view** — isometric / custom orientation projection
- [x] **DrawingSketch** — lines, circles, arcs, rectangles, text on a sheet
- [x] **DrawingSketches** — `addSketch()` on the sheet
- [x] **Dimensions** — drag linear dimensions on the sheet (aligned length label)
- [x] **Annotations** — text notes, hole notes, leaders, center marks, images
- [x] **Tables** — parts list (BOM), revision table, bend table (sheet metal features)
- [x] **Center marks** — placeable on the sheet
- [x] **Hatch patterns** — ANSI31-style hatch on section views
- [x] **Drawing export** — PDF, DXF, PNG, TIFF from the sheet
- [x] **TIFF export** — via `exportTiff` (DWG remains out of scope; use DXF)
- [x] **Images** — insert PNG/JPEG/BMP/TIFF on the sheet
- [x] **Drawing history** — `pushHistory` / `history` list for sheet actions (browser timeline)
- [x] DRAWING ribbon — Create, Annotate, Table, Export, Back to Design
- [x] **Title block** — part name, drawn by, revision, date, material, standard / size on the paper
- [x] **Auto-populated fields** — date on sheet create; part name / drawn by from the Create Drawing dialog
- [x] **Drawing browser** — sheets and views list with selection and regenerate

### Phase 10 — Render & Appearance ✅
- [x] **Appearance** class — colour, roughness, metalness, opacity, albedo / rough / metal / normal paths; 9 types; JSON
- [x] **AppearanceLibrary** — built-in PBR presets (plastics, metals, glass, …); CC0-inspired wood/stone/concrete
- [x] Apply appearance to selected **bodies or faces** (**A** / RENDER ▸ Appearance); saved with the design
- [x] Viewport shading uses roughness / metalness, PBR maps, and document sun lighting
- [x] **RenderSettings** on the document — quality, background, ground, HDRI path, sun, output size/format
- [x] **RenderEnvironmentLibrary** — built-in light presets + curated Poly Haven CC0 HDRI catalog
- [x] **Render dialog** — Engine / Quality / Scene / Lighting / Output; path trace or viewport capture
- [x] RENDER ribbon tab
- [x] **Download / cache** Poly Haven HDRIs (1K–8K) and material PBR packs (`RenderAssetStore`); Open cache folder
- [x] **Image-based lighting** in the viewport (equirect sky + ambient/specular from local `.hdr`)
- [x] **Texture maps** — Poly Haven 1K albedo + roughness / metalness / normal; triplanar in viewport + path tracer
- [x] **Offline render** — supersampled viewport capture (1×…3×) **and** Monte Carlo path tracer (8…256 spp)
- [x] **Per-face appearance** overrides (`faceAppearances` in `bodyDisplay`)

### Phase 11 — Multi-Computer & File Sync
- [x] `.f360` file format
- [x] **Design linkage** — `documentId` + `contentRevision`; drawings / renders in `linkedAssets` with out-of-date flags; **Update from Design** restamps drawings
- [x] **Linked design inserts** — drag a design from Projects (or a `.f360` from Explorer) onto the viewport, or Projects ▸ right-click ▸ Insert into Current Design. Stored as an `ImportCAD` feature with `link = {path, documentId, revision, name}` (`FissionLinkedDesign.cpp`). A 4 s check compares the source file's saved `contentRevision` (read from the file head, cached by file time) and prompts **Update / Not Now**; browser **Linked Designs** folder: Update from Original, Open Original, **Unlink** (keeps the bodies, never asks again), Delete Insert *(1.13.12)*
- [x] **Internal version history** — compressed snapshots inside the `.f360` `history` array (`DocumentVersionStore`); File ▸ Version History / Revert; legacy `.versions` sidecars migrate on open/save
- [x] **Autosave** — AppData crash recovery timer; File ▸ Recover Autosave; interval in Display Settings
- [x] **UI walkthrough GIF** — `--uitest` exports `walkthrough.gif` via `GifWriter`
- [x] **Large Fusion timeline open** — lazy per-step STEP decode; only the marker’s last snapshot is applied (avoids OOM on 100MB+ exports)
- [x] **Fusion body names + mesh bodies** — STEP solid names / exporter `stepBodies`; exporter 1.6 `meshBodies` → named Insert Mesh (1.13.3)
- [ ] Lock server, sync protocol

### Phase 12 — Manufacturing
Keep Fission light: **do not** link `libslic3r` / embed Orca into the CAD process. Additive handoff is temp mesh/3MF + launch Orca. Native CAM tools below are Fusion-parity MFG workspace items to build later inside Fission.

#### Additive / Orca (thin handoff — not an embedded slicer)
- [x] **Send to Slicer** — tessellate selected (or all) VISIBLE bodies (hidden never sent) to a temp **STL** and launch the auto-detected / chosen slicer (OrcaSlicer or Bambu Studio, Display Settings ▸ Slicer) (STL avoids Orca’s foreign-project 3MF warning) *(1.13.10 / 1.13.11)*
- [ ] Optional: remember last Orca path / “Open cache folder for prints”
- [ ] Optional: arrange / orient hints via Orca CLI flags

#### MFG workspace (Fusion-like)
- [x] **MANUFACTURE** workspace under **DESIGN ▾** (separate from Design / Render; its own MANUFACTURE tab) *(1.13.12)*
- [x] **Setup** — New Setup, WCS, stock offset + refresh from bounds, machine selection *(1.13.28)*
- [x] **Tool Library** — mills, drills, taps, turning, probe; editable feeds & speeds *(1.13.28)*
- [x] **2D milling** — Face, 2D Adaptive, 2D Pocket, 2D Contour, Slot, Trace, Bore, Circular, Engrave, Chamfer, Thread *(catalog + stock NC stubs 1.13.28)*
- [x] **Drilling** — Drill, Tap, Thread mill *(1.13.28)*
- [x] **3D milling** — Adaptive / Pocket Clearing, Parallel, Contour, Flat, Scallop, Radial, Spiral, Morphed Spiral, Pencil, Ramp, Steep & Shallow, Project, Morph, Flow, Blend *(1.13.28)*
- [x] **Multi-axis** — Swarf, Multi-Axis Contour, Rotary / wrap *(ops + planar NC stub 1.13.28; true multi-axis TBD)*
- [x] **Turning** — Facing, Profiling, Grooving, Parting, Threading, Adaptive roughing *(XZ stub 1.13.28)*
- [ ] **Additive (in-app)** — print setup / supports (beyond Send to Orca), if needed later
- [x] **Probing** — Probe WCS / Probe Geometry *(G31 stub 1.13.28)*
- [x] **Simulate** — per-op path-length estimate from stock + type *(1.13.28; no collision yet)*
- [x] **Post Process** — op-type-aware stock NC export *(1.13.28; not a full CAM post)*
- [ ] **Manufacturing Model** — isolated edits for manufacture without changing the design
- [x] **Folders** — op folder labels *(1.13.28; patterns/templates TBD)*

---

## Fusion Reference Material

Reference only — see the IP rule under Goal. Nothing here is copied into `src/` or `assets/`, and `docs/fusion_*` stays out of public repositories.

### Extracted QML (96 files)
All 96 QML files from Fusion's webdeploy install at:
`C:\Users\GEARSHADE\AppData\Local\Autodesk\webdeploy\production\f0877c667ada77b44502b5b3316b697a2ba18ad4`

Located in: `docs/fusion_qml/`

### Extracted TypeScript API (5 files, ~3.5MB total)
- `fusion_core_api.d.ts` (730KB) — Core: Application, Document, Component, Sketch, BRepBody, MeasureManager, Selection
- `fusion_fusion_api.d.ts` (2.2MB) — Fusion-specific: Design, ExtrudeFeature, FilletFeature, Analysis tools, Mesh tools, RenderManager, CAM bodies
- `fusion_cam_api.d.ts` (422KB) — CAM operations, toolpaths, post-processing
- `fusion_drawing_api.d.ts` (95KB) — Drawing sheets, views, dimensions, annotations
- `fusion_sim_api.d.ts` (83KB) — Simulation setups, loads, constraints, results

### Analysis API Classes (from fusion_fusion_api.d.ts)
All analysis classes extend `Analysis` base class:
- `AccessibilityAnalysis` (line 27428)
- `CurvatureCombAnalysis` (line 27462)
- `CurvatureMapAnalysis` (line 27496)
- `DraftAnalysis` (line 27530)
- `EnvironmentMapAnalysis` (line ~27600)
- `IsoCurveAnalysis` (line 27626)
- `MinimumRadiusAnalysis` (line 27660)
- `SectionAnalysis` (line 27718)
- `InterferenceInput`, `InterferenceResults`, `InterferenceResult` (line 25634)
- `ZebraAnalysis`, `ZebraAnalysisInput` (line 27884)
- `MeasureManager`, `MeasureResults` (fusion_core_api.d.ts line 3670)

### Mesh API Classes (from fusion_fusion_api.d.ts)
- `MeshBodies` (line 29435) — collection
- `MeshBody` (line 29536) — triangle mesh
- `MeshBodyDisplayOverrides` (line 29520) — display properties
- `MeshManager` (line 1350) — BRep→Mesh conversion
- `MeshFeature` (line 29856) — base class
- `MeshRepairFeature` (line 29992)
- `MeshSmoothFeature` (line 30118)
- `MeshReduceFeature` (line 30273)
- `MeshRemeshFeature` (line 30452)
- `MeshGenerateFaceGroupsFeature` (line 30620)
- `MeshShellFeature` (line 30746)
