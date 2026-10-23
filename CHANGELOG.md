# Fission 360 — Change Log

All notable changes, newest first. Format follows [Keep a Changelog](https://keepachangelog.com/);
milestones match the phases in the Roadmap section of `FISSION_360_PROJECT.md`.

The git history of this repository is not usable (broken branch), so entries before Phase 3 were
reconstructed from the development notes. Add an entry for every change from now on — under
**Unreleased** until a phase is closed.

Test counts: `--selftest` = kernel regression tests, `--uitest` = scripted UI walkthrough checks.

---

## [Unreleased] — **1.13.35** is the current GitHub release ([v1.13.35](https://github.com/xboxhacker/fission360/releases/tag/v1.13.35)). Next: Phase 11 lock server / sync protocol; Phase 12 native CAM toolpaths; leftover Phase 4 analyses + organic mesh convert still open

### 1.13.40 — Linked drawing dimensions, Fusion units, faster big files, Python, Shell hang
- **Shell no longer hangs**:
  - Starting Shell with nothing picked hollowed the whole body at once. On a filleted, chamfered body (4KG Spool Hub) OCCT's offset never returned, and the window stopped responding.
  - Shell now waits: "Select the faces to remove, or a body to hollow". A picked body with no faces still hollows it.
  - Every offset and boolean step of Shell shares a 15 s budget, and stops with a message instead of freezing.
- **Fusion sketches at the right size (exporter 1.10)**:
  - Fusion's API reports every length in **cm** and every angle in **radians**, whatever the design's units. Sketch coordinates, radii, plane offsets and bare feature values were read as mm and degrees, so sketches came in 10× too small while the STEP solids were right.
  - Fission now reads them as cm and radians. That is the same length, not a rescale. Values with a Fusion expression (`25 mm`, `1 in`) were already right.
  - The design opens in the Fusion document's own units (mm, cm, m, in or ft). Before, anything that was not inches became mm.
  - The exporter labels its numbers (`geometryUnits: cm`, `angleUnits: rad`) and writes each sketch's exact frame (origin and X / Y directions). Sketches on offset planes and faces sit where they were in Fusion. Older exports use the plane's offset, which was ignored before.
- **Python scripts and add-ins** are built in. CMake finds Python 3.11+ development files, builds `PythonHost` on the stable ABI (`python3.lib`, `Py_LIMITED_API` 3.11) and delay-loads `python3.dll` from the user's own Python. Fission still starts without Python. Before, nothing in CMake turned it on, so the Scripts dialog said "Python support was not built in".
- **Drawings**:
  - **Dimensions follow their view and the design.** A dimension is stored on its view, in the view's model mm, with the edges it was measured on. Moving or rescaling the view moves it. When the view is projected again (Update from Design, Regenerate, a changed design), each pick is found again among the new edges and the value is re-measured. If an edge is gone, the dimension turns red. Deleting a view deletes its dimensions.
  - **Dimensions on a view can be selected and dragged**: they sit above the views, so a click no longer selects the view box underneath. A drag moves the dimension line or text (one undo step).
  - **Notes can be dragged** to a new place, also over views.
  - **The title block can be edited and deleted.** Click it to edit Title, Drawn by, Revision, Material and Date in the inspector. Delete (or the inspector button) removes it; DRAWING ▸ SHEET ▸ Title Block brings it back. Exports leave it out while it is deleted.
  - **Orientation box**: a click on its name focuses it, then Up / Down or the mouse wheel step through the orientations and the view re-projects at each step. The arrows on the right still open the list.
  - **Unsaved prompt**: drawing edits now mark the design unsaved. Leaving a drawing (Back to Design, or Design / Render / Manufacture) with unsaved changes asks Save, Don't Save or Cancel.
- **Big files open and save much faster.** On the user's 795 MB Fusion import (CompRT): open went from 30 s to 14 s (half of that is the viewport meshing), save from 66 s to 15 s, and the file is now 273 MB.
  - Version history stored a full compressed copy of the design for every version, Fusion step BReps and meshes included: 506 MB in five versions. A version now refers to large feature data (strings over 64 KB) by hash (`$blob:<sha1>`); revert takes it back from the open design.
  - Older versions are kept up to a 64 MB budget; the newest always stays. Versions saved before 1.13.40 still hold full copies, so they drop off at the next save.
  - Open no longer parses the version history: it is cut out of the file text first and parsed only for Version History, revert or save.
  - The open cache stores solids as binary BRep (`BinTools`), lightly compressed, instead of OCCT's text format. Mesh bodies took 8–11 s each to write as text. Older text caches still open. The cache key hashes the feature data directly instead of encoding it to JSON first (4 s on that file).
  - Save builds the design's JSON once for both the file and the version snapshot.
  - `FISSION_TRACE_IO=1` prints each open / save stage with its time.
  - Files saved by older versions open once with a full rebuild (their cache key changed); the next save writes the new cache.
- **Imported bodies and every solid tool**: new `--toolbench <file>` runs extrude, offset face, hole, draft, delete face, shell, thread, fillet, chamfer, move, scale, mirror and hollow on faces and edges spread over each body of a STEP / IGES / `.f360`. It prints a per-tool summary; results and the open work are in `FISSION_360_PROJECT.md` ("Imported bodies and solid tools").
  - **Delete Face** runs OCCT's defeaturing on a copy in a worker thread and gives up after 15 s. On the McMaster hex nut's thread faces it ran 85–200 s on the UI thread, because the defeaturing's own cancel check only comes between its stages.
  - Still open (found by the bench on the 4KG Spool Hub): Shell fails on curved / chamfered bodies; Offset Face sends curved non-chamfer faces down the chamfer path; Draft fails on some side faces; modeled Thread has no time limit.
- **UI test scripts**: `FISSION_UITEST_ONLY=shell` (Shell with nothing picked, on `FISSION_UITEST_FILE` or a box) and `=open` (open / save timing, saving a copy to `FISSION_UITEST_SAVE`). `panels` also checks view-attached dimensions, re-projection and the title block.

### 1.13.39 — Drawing dimensions, note styling, lighter startup
- **Drawing dimensions** (DRAWING ▸ DIMENSIONS, or D on a sheet), measured on projected model edges like Fusion's ViewCurve:
  - **Dimension** decides from the picks: one line gives its length, two parallel lines their distance, two lines that meet their angle, an arc its radius, a circle its diameter. Corners and circle centres snap.
  - Tools: **Linear** (horizontal or vertical, chosen by where you place it), **Aligned**, **Angular** (the sector you click in decides which angle), **Radius**, **Diameter**.
  - Pick, pick (optional), then click to place. The edge under the cursor is highlighted and the dimension follows the cursor. Esc clears the picks, then leaves the tool.
  - A click on a dimension selects it. The inspector overrides its text (clear it to measure again), and sets its decimals and text size. Delete removes it.
  - Values are model mm, using the view's scale. The projector now keeps each edge's geometry (line, or circle centre and radius) and tags every segment with its edge (`edges` on the view, saved in the `.f360`).
  - One module (`DrawingDimension`) builds the dimension and draws it for the sheet, PDF / PNG / TIFF and DXF. Dimensions from older files show as aligned dimensions with their own text.
- **Drawing notes**:
  - The inspector's text field saves on Enter or when it loses focus. Before, a click away lost the edit.
  - Any edit now shows on the sheet straight away. Before, the sheet kept drawing the old text, because an edit leaves the note count unchanged.
  - New: text size (ISO 3098 heights, 1.8–20 mm), **Bold** and *Italic*, also in PDF / PNG / TIFF.
- **DXF export**: views, notes and dimensions share one convention (Y = sheet height − y). Views were written y-up from a y-down origin, and notes y-down, so the two did not line up. Hidden lines are only written when the view shows them.
- **Hidden lines** are centred with the visible outline (they had their own centre and could sit shifted).
- **Projects drag-and-drop**: dragging a design tile onto the viewport now starts a real OS drag from C++ (`ProjectBrowser::startDrag`). The 1.13.38 fix still bound `Drag.active` to the MouseArea; the drag blocks until the drop, which looped the binding and Qt cancelled the drag.
- **Projects panel**: while a design opens, its progress shows along the bottom of the panel. The status-bar bar sat under the panel's dim and looked greyed out.
- **Manufacture panel**: its content was wider than the panel (the tab bar took 520 px of 400), which hid the ✕ and the Finish tab. It fits now. Going back to Design closes the panel; Render already did.
- **Lighter start**:
  - WebEngine (Chromium) is no longer loaded at startup. The exe does not link it; the McMaster dialog's `import QtWebEngine` loads it on first use, and startup only sets `AA_ShareOpenGLContexts`. About 15–20 fewer DLLs at launch.
  - `scripts/package-windows.ps1` leaves out what Fission never uses: Quick Controls styles other than Basic, the QML debugger plugins, the D3D12 shader compiler, and WebEngine DevTools resources. That is 1150 files and about 38 MB (12 %) of the package.
- **UI test**: `FISSION_UITEST_ONLY=panels` (drawing dimensions, notes, PDF/DXF export, Manufacture panel) and `=mcmaster` (WebEngine on demand) run a short script instead of the whole walkthrough.
- `--selftest` **427 passed, 0 failed**; `panels` 5/5, `mcmaster` 1/1.

### 1.13.38 — Projects drag-and-drop, Fusion linked designs, sharper icons
- **Projects drag-and-drop**: dragging a design tile from the Projects panel onto the viewport inserts it as a linked design again. The tile's press was taken by the grid (vertical flick) or the drawer (swipe to close) before the drag started. The tile now keeps the press, and the drawer no longer closes on a swipe; it still closes with Esc, a click outside, or when the drag starts.
- **Fusion linked designs (exporter 1.9)**:
  - The export records where the design lives in Fusion (`metadata.fusionFile`: project, folders, name, version).
  - Each timeline row that inserts another cloud design records the referenced file (`link`: project, folders, name, version, referenced version, out-of-date flag).
  - Fission resolves that to `<projects folder>/<project>/<folders>/<name>.f360` relative to the open file, or to the same folder for a flat export. It lists the insert under **Linked Designs** with "Fusion vN", or "original not exported" when that file is not there. Open Original opens it and Unlink drops the link.
  - The inserted geometry is still Fusion's snapshot, so there is no Update from Original for these yet.
  - Selftest: three inserts resolve in the same project, in another project, and to a missing file.
- **Fusion import**: `fusionGroup` (and `fusionLink`) are now kept on mapped chips too (Extrude, Fillet, …), not only on pass-through ones.
- **Delete Face glyph**: the box's top face is tinted red and crossed out with a red X (was a dashed outline and a minus). Remove keeps its X beside a whole box.
- `--selftest` **427 passed, 0 failed**.
- **Glyph strokes**: 1.25 px instead of 1.6 px, drawn half a pixel off the grid so lines sit on pixel centres. At 100 % scaling every icon was a two-pixel smear; now each line has a one-pixel core. This applies to every glyph: ribbon, browser and timeline.

### 1.13.37 — Fusion timeline order, Delete Face, timeline wheel
- **Fusion exporter 1.8** (`F3D_Bulk_Export`):
  - Collapsed timeline groups are expanded for the export and collapsed again afterwards. A collapsed group is one timeline row in Fusion's API, and its children have no index. Exporters up to 1.7 never wrote those children as rows and could not roll the marker into them.
  - Features and sketches now come from each timeline row's own entity. Before, the exporter walked every occurrence: two copies of a component wrote their features twice, and an inserted design added its own history, numbered by its own timeline. Those numbers collided with this design's rows.
  - Bodies are listed once per component, not once per occurrence.
- **Fusion import**:
  - A timeline row only takes a feature or sketch whose name and type match it. A plane row no longer becomes `Extrude2` from an inserted design.
  - Features and sketches that are not timeline rows are no longer appended at the end of the timeline.
  - Unknown Fusion rows without a snapshot (joints, for example) stay as chips instead of being dropped, so the chip count matches Fusion's timeline.
  - Rows inside a group remember the group name and show it in the chip tooltip. Files already converted and saved by 1.13.35 keep their old chips; re-export from Fusion with add-in 1.8.
- **Timeline icons**:
  - Imported steps take their icon from the Fusion type. Every Fusion step that has no Fission tool used to show the Insert STEP glyph.
  - New glyphs: Delete Face, Component (occurrence), Position (snapshot), Group, Boundary Fill, Form (T-spline), Thicken, Base Feature, Base Mesh Feature, Mesh Section, and a generic Fusion-step glyph for other types.
  - Fusion types that have a Fission tool (Move, Flange, Unfold, Refold, Mesh Reduce, Generate Face Groups, and others) use that tool's glyph.
- **Delete Face** (SOLID ▸ MODIFY, or select faces and press Delete): removes the faces and grows the faces around them to close the solid (OCCT defeaturing), like Fusion. It handles hole walls, fillets, chamfers and bosses. If the neighbours cannot close the gap, the body is left unchanged and the status bar says why. Delete with only whole bodies selected still removes the bodies. Before, Delete on a face removed the whole body. Selftests: a hole wall deleted from a 40×20×10 plate gives 8000 mm³; deleting every fillet face of a filleted cube gives 1000 mm³.
- **Timeline wheel**: the wheel (and a horizontal or touchpad scroll) only slides the strip, 8 chips a notch, or one with Ctrl. It no longer moves the marker. The marker moves with the arrow buttons, by dragging it, or with **Move Marker Here** / **Move Marker Before** on a chip's right-click menu (formerly Roll Back To Here / Before).

### 1.13.36 — Hotfix: 1.13.35 would not start
- **Startup**: the 1.13.35 build quit at launch with no window. `TimelinePanel.qml` set `pressDelay` on a `MouseArea` (that property only exists on `Flickable`), so `MainWindow` failed to load. The line is removed.
- **Packaging**: `scripts/package-windows.ps1` now launches the packaged exe and refuses to write the zip if it exits within 10 s or logs a QML load error. Build leftovers (`*.exp`, `*.lib`, `*.pdb`, `*.ilk`, `Fission360.previous.exe`) are no longer copied into the release.

### 1.13.35 — Drawing sheet, open cache, timeline scrub
`--selftest` **417 passed, 0 failed**. Windows zip: `Fission360-1.13.35-windows-x64.zip`.

- **Drawing save**: the live `.f360` stores a `drawings` array (sheets, views, projected curves, annotations, tables, title block, standard, projection angle, sheet size, image paths). Reopening the design restores the sheet. Files that only have a `linkedAssets` id still open. Version-history snapshots and autosave omit the drawing body, same as the thumbnail and nested history.
- **Drawing views**: HLR edges are already on the view plane, so a front view uses that plane's X and Y. Dotting them with world Z had collapsed every front view to a horizontal line. A 40×20×10 box front view is 40.0×20.0 mm. Scale labels are ratios (`1:1`, `2:1`, `1:2`), not `1.00` rendered as `1:00`. Round edges keep a 0.02 mm sagitta, so a cylinder top view stays a circle (selftest: 71 segments, 0.010 mm radial error) instead of a dozen-sided polygon. Opening a drawing rebuilds its views. The selection frame has a margin so its border sits clear of the outline. The orientation list opens under the menu.
- **Drawing notes**: a new note starts as "Note" and can be edited immediately. Click it and type in the inspector, or double-click it on the sheet. Enter commits, Escape cancels, and undo restores the previous string.
- **Isometric corners**: Orientation lists Front, Top, Right, Left, Back, Bottom, and the eight corners from Front top right through Back bottom left. Each corner is a different projector. A stored `isometric` or `home` value stays the original back-top-right axis.
- **Rendered drawing views**: Display is Wireframe (default) or Rendered. Rendered stores a shaded PNG of the tessellated solid along that view axis (`renderPng`) and reloads with the drawing. Hidden lines stay a wireframe option. A rendered detail or section view shows the whole solid in that orientation, fitted to the view frame.
- **Drawing sheet**: the DRAWING ribbon is the command bar (base, projected, section, detail, aux, back, dimension, text, tables, export, tidy up, hidden lines). The duplicate in-sheet toolbar is gone. Fit stays on the sheet. Middle-drag pans and the wheel zooms about the cursor, both as a transform. Placing a view leaves placement mode. Annotations and dimensions are stored in millimetres and drawn on the sheet. A new drawing auto-places one projected front view.
- **Press Pull**: a round chamfer face can be pulled. Positive distance shrinks it along its outward normal, negative distance grows it, and the Extrude dialog does the same when its profile is that face instead of reporting that only flat faces can be extruded. A circular chamfer split on a seam, and one that stops against a flat, are still one chamfer. Selecting the face pulls that cone directly; it does not offset the whole solid, and the pull gives up after two seconds instead of locking the window.
- **Open cache**: Save writes a `resultCache` into the `.f360` (compressed B-rep of the solids, plus the timeline marker). The key is a hash of the units, parameters, features, and marker. A matching file opens those solids and skips the rebuild. A stale key replays the timeline. Undo steps and version snapshots omit the blob. Save leaves the marker where it is, finishes a pending roll-back first, and caches the model at that point. Opening returns to that marker even when it is not at the end; features after it stay in the file and are not replayed.
- **Large open**: the status bar updates during read and rebuild. With four or more bodies, or any mesh, faces are shown before mesh edges. Morton sorting is skipped above 80,000 triangles.
- **Browser**: a viewport click on a face or edge highlights that body. A sketch, construction plane, axis, or point highlights its own row, opens a collapsed folder, and scrolls the row into view.
- **Timeline**: the wheel jumps the marker 8 features (Ctrl steps one) and keeps that marker on screen. Shift+wheel, or a drag along the chips, slides the strip without rolling the model. A click outlines a chip. A viewport click on a body, face, edge, sketch, fillet, chamfer, or other feature that last changed the body outlines that chip. Fusion files keep the last 8 decoded marker solids in memory, so rolling back to one of them does not decode it again. Exporter 1.7 writes Fusion's marker into the file.
- **Drawing edits**: changing a view's orientation or hidden lines projects that view immediately. Ctrl+Z / Ctrl+Y and the undo buttons step the drawing while a sheet is open, and the solid's own undo again once you leave it. The view panel stays on screen: × closes it, Delete removes the view, and the hidden-lines checkbox no longer covers its label.
- **Insert mesh**: the shared `insertmesh` glyph arrow points down.
- **Offset plane**: an offset construction plane has the same viewport drag arrow as Extrude. Dragging writes `offset`; the dialog field stays in sync. Typing still works.
- **Loft rail**: a rail that only meets the first and last sections builds a solid (selftest volume 7000). A pipe is kept only when the rail lies on the solid.
- **Fillet**: a result that is not a smaller finite solid is rejected. An 8 mm fillet on a 10 mm cube no longer stores a huge invalid body.
- **Shell**: when a taper-plus-fillet wall equals the fillet radius, thick-solid cannot offset. The punch fallback is kept when `BRepCheck` passes (selftest volume about 12839, hollow).
- **Parity tests**: distant-box join (#3526) and extrude join / through-all (#3563) already produced the right solids. The checks now expect those volumes. The kernel for those two was not changed.

The version number now goes up with every round of changes (`FISSION_VERSION` in `CMakeLists.txt`; it is
shown in the title bar and available to scripts as `fission.version`).

### 1.13.34 — Shell clean corners
- **Shell**: for planar bodies prefer Intersection joins (not Arc) so inner corners stay sharp; reject results with `BRepCheck` failures or tiny spike faces (< 0.01 mm²). Punch fallback grows the opening face in-plane before cutting and sews the result. Fixes the jagged “teeth” at shelled frustum corners.

### 1.13.33 — Shell frustum punch fallback + versions inside .f360
- **Shell**: unify coplanar faces first; more complete-intersection retries; accept volume-correct results after `ShapeFix` even when `BRepCheck` complains; fallback hollows by solid-offset cut then punches opening faces through the wall (fixes 45° face-extrude frustum shells that still failed after 1.13.31).
- Selftest: user recipe — box + 45° face extrude + shell sloped face 2 mm inside.
- **Version history** is embedded in the `.f360` as a compressed `history` array (no `<file>.f360.versions/` sidecar). Legacy sidecars are imported on open/save and removed after a successful save. Autosave stays in AppData.
- **INSERT ribbon**: captions show **Mesh** / **STEP** / **McMaster** (tooltips keep the full names). McMaster gets its own catalog glyph so it no longer looks identical to STEP import.

### 1.13.32 — Report Bug + SolidCraft issue parity tests
- **Report Bug** in the top command bar (bug glyph), File menu, Help command search, INSPECT / UTILITIES: opens a GitHub new-issue form for `xboxhacker/fission360` with version, OS, last status, and unhealthy feature errors pre-filled.
- **Selftest** `testSolidCraftParity`: geometry recipes inspired by public SolidCraft kernel issues (#3541 loft, #3578/#3526 combine, #3528 revolve seam cut, #3573 taper+fillet shell, #3561/#3587 oversized blends, #3563 extrude join/through-all). See `docs/solidcraft-issue-parity.md`.

### 1.13.31 — Shell on sloped / ridge faces
- **Shell**: retries in OCCT complete-intersection mode (intersection joins, 1e-7 then 1e-4) when the default arc-join offset fails, so planar bodies whose narrow faces vanish under the wall offset (hip-roof ridge strips, pyramid apexes) now shell like in Fusion. Results are validity- and volume-checked; OCCT exceptions are caught. The closed (no-opening) hollow gets the same retry.
- Selftest: hip-roof box (2 mm ridge) shelled through a sloped face at 2 mm.

### 1.13.30 — Update check, viewport PNG export, grid setting
- Startup check against GitHub latest release; popup to download when newer (Skip / Later).
- **File ▸ Export Viewport PNG**: size dialog, fast grab without grid.
- **Display Settings**: Show viewport grid toggle (persisted; syncs with viewport Grid button).
### 1.13.29 — Command registry + MCP server
- `CommandRegistry`: shared dotted commands (doc / feature / measure / param / edit) for scripts, QML (`fissionCommands`), and agents.
- `Fission360 --mcp`: headless MCP over stdio (Content-Length JSON-RPC) — list_commands, execute, batch, inspect, measure, export, open/save, undo/redo, set_parameter.
- Path sandbox: home, temp, open design folder, plus `--allow-path` / `--allow-any-path`. Docs: `docs/mcp.md`.
### 1.13.28 — Manufacture tools finished
- Full MFG catalog in toolbar / panel / command search: 2D, drill, 3D, multi-axis, turning, probe.
- Setup inspector (machine, WCS, stock offset) + **Refresh Stock** from body bounds.
- Tool library presets + editable type/Ø/feeds; ops get folder, tool pick, enable/disable, feed/speed/stepdown.
- **Simulate** per-op path estimate; **Post** writes op-type stock NC stubs (face zigzag, pocket insets, drill points, turning XZ, probe G31).
### 1.13.27 — Drawing workspace redesign (Fusion-like)
- Sheet **pan** (mid/right-drag) and **zoom** (wheel); Fit Sheet (F).
- **Base View** orientation bar + ghost preview; auto-places Front on empty sheets.
- **Projected views**: Shift+drag from a parent toward a direction (third/first-angle aware).
- Views float on the paper (no card chrome); floating **view inspector** (scale, hidden lines).
- Clearer HUD prompts; dimension rubber-band with arrowheads.
### 1.13.26 — render crash; face pattern cut; shell pick; Ctrl+Y
- **Path-trace export**: mesh a private face copy (no live-shape remesh race), guard camera/resolution, catch OCCT failures.
- **Pattern faces**: select a hole face → Rectangular/Circular pattern **Cuts** more holes into the body (cylinder / extruded-face tools).
- **Shell**: remesh tiny faces (chamfers / 45° bevels) so they stay pickable.
- **Ctrl+Y redo**: app-wide C++ event filter (QML Shortcut alone was still missing key events).
### 1.13.25 — Ctrl+Y redo; section offset arrow; MFG wiring; requestPaint
- **Ctrl+Y** (and Ctrl+Shift+Z) redo via `ApplicationShortcut`; status when nothing to redo.
- **Section Analysis** offset arrow: drag no longer required a command feature (`moveManipDrag` early-return bug).
- `Viewport` `requestPaint` TypeError: `manipCanvas` is a child id, not `vp.manipCanvas` — guarded `repaintManip()`.
- **Manufacture**: setups / tools / ops saved on the design; toolbar + panel + Post (placeholder G-code) + Simulate; Send to Slicer unchanged.
### 1.13.24 — fix: path-trace Export Image TypeError
- Render Export called `cam.fieldOfView()` but FOV was not a QML function — export aborted with no file.
- Expose `ViewportController.fieldOfView` as a property; append format extension; `QImage::save` uses explicit PNG/JPG/TIFF/BMP.
### 1.13.23 — source only: positioning + peer inspiration
- README positioning: OCCT + Fusion import + Send to Slicer (not “only open CAD on GitHub”).
- `docs/peer-inspiration.md` — ideas from SolidCraft’s *public* docs (command registry, MCP, CLI/oracle, packaging); **no source copied**.
### 1.13.22 — Join multi-profile extrude into one body
- **Join** on Extrude/Revolve/etc. with several sketch profiles now fuses the resulting solids before adding bodies. An empty document no longer creates Body1+Body2 for two selected profiles when Join is selected (New Body still creates separate bodies).
### 1.13.21 — source / rebuild: draft & section handles; sketch-edit body loss
- **Draft** shows a viewport angle knob (drag to set the draft angle).
- **Section Analysis** shows a viewport offset arrow (drag to move the cut plane); the panel slider still works.
- **Sketch edit**: finishing a sketch restores the timeline marker and rebuilds immediately. Stale Extrude profile indices after a sketch change fall back to the sketch's default closed profiles so the body is not left deleted with `Profile N no longer exists`.
### 1.13.20 — 2026-10-19: fix missing OCCT DLLs in Windows release
- Windows zip was missing `TKBRep.dll` (and related OCCT libs) because CMake copied from an incomplete `dep_OCCT-build/bin/occt` tree. Deploy now prefers `OrcaSlicer_dep/usr/local/bin/occt` (full runtime) and also copies `freetype.dll`.
- `scripts/package-windows.ps1` refuses to ship without `TKBRep.dll` and backfills from `FISSION_ORCA_ROOT` if needed.
### 1.13.19 — 2026-10-19: cleanup + Windows GitHub Release
- Removed root clutter: old `build*_log.txt` / `fission_build*.txt` / `occt_*.txt` dumps, `aqt`/smoke logs, duplicate `shot_*.png` (README screenshot remains `docs/screenshot.png`), empty `uitest/` and `docs/external_edits/`.
- Tightened `.gitignore`. Wiped `build-rel/`/`dist/` earlier, then rebuilt and published `Fission360-1.13.19-windows-x64.zip` to https://github.com/xboxhacker/fission360/releases/tag/v1.13.19
### 1.13.18 — source only (not built yet): multi-platform release sketch
- Added `docs/ci-release-sketch.md` (goals, CMake blockers, Qt/OCCT cache strategy, job flow, implementation order).
- Stub `.github/workflows/release.yml` (Windows/macOS/Linux + publish; all jobs disabled with `if: false` until caches/Unix CMake exist).
- Stub `scripts/package-macos.sh` and `scripts/package-linux.sh` (`.app` zip / AppImage-or-tar.gz contracts). README points at the sketch.
### 1.13.17 — 2026-10-19: Windows release package (+ GitHub README)
- **Windows x64 release** built and packed: `dist/Fission360-1.13.17-windows-x64/` and `dist/Fission360-1.13.17-windows-x64.zip` (~147 MB) with exe, Qt/OCCT/WebEngine runtime, `LICENSE`, `THIRD_PARTY_NOTICES.md`, `README.md`. Packaging script: `scripts/package-windows.ps1`. `dist/Fission360` is a junction to that folder for local testing. Selftest 348/1.
- Includes 1.13.16 slicer search/browse and the rewritten GitHub README (screenshot, accurate Windows build docs).
- **macOS / Linux releases are not available yet** — this machine is Windows-only, and CMake deploy (`windeployqt`, MSVC `/EHa`, Orca-deps OCCT paths) has no Mac/Linux packaging path. Native runners + OCCT/Qt setup are required before those zips can ship.

- Rewrote `README.md` for a public repo: status line, screenshot (`docs/screenshot.png`), accurate Windows build (aqtinstall Qt path, `FISSION_ORCA_ROOT`), run/selftest, Fusion add-in pointer, slicer settings, contributing, IP rule (`docs/fusion_*` never pushed), binary-release notice packing. macOS/Linux marked unverified.
### 1.13.16 — source only (not built yet): find your custom OrcaSlicer
- Display Settings ▸ **Slicer**: **Search for slicer** hunts the disk in the background (Documents/Desktop/Downloads/home, Program Files, drive roots) for `orca-slicer.exe` / `OrcaSlicer.exe` / `bambu-studio.exe`, including custom builds such as `MyOrcaSlicer\build-portable\src\Release`. Results are listed newest first; **Use** picks one. **Browse…** picks any exe by hand (a custom-named build counts as OrcaSlicer); **Reset** returns to auto-detect. The search can be stopped.
- The dialog now shows the exact path used for each slicer. A custom path always wins over the auto-detected install.
### 1.13.15 — licence housekeeping + build for testing
- Every source file (`.cpp` / `.h` / `.qml` / `.py`) carries an `SPDX-License-Identifier: GPL-3.0-or-later` header.
- New `THIRD_PARTY_NOTICES.md` (Qt, Qt WebEngine/Chromium, OpenCASCADE, FreeType, OrcaSlicer/Bambu Studio and trademark notes, source offer). Embedded in the app: **File ▸ About ▸ Third-party notices**.
- **CMake no longer hard-codes a personal path**: pass `-DFISSION_ORCA_ROOT=<OrcaSlicer checkout with built deps>` or set the `FISSION_ORCA_ROOT` env var (`../MyOrcaSlicer` and `%USERPROFILE%/Documents/GitHub/MyOrcaSlicer` are fallbacks).
- `.gitignore` also excludes `docs/external_edits/`, old build logs (`fission_build*.txt`, `occt_*.txt`) and the generated notices copy.
- This build also contains 1.13.13 (Orca icon) and 1.13.14 (Send to Slicer).
### 1.13.14 — Send to Slicer
- **Send to Slicer** (was Send to Orca) supports **OrcaSlicer and Bambu Studio**. Display Settings ▸ **Slicer**: Auto-detect (default; Program Files / LocalAppData / uninstall registry), OrcaSlicer, or Bambu Studio, with a live "detected ✓/✗" line. Optional manual paths via settings `orca/executable` / `bambu/executable`.
- **Only visible bodies are sent.** Hidden bodies are dropped even if explicitly selected; the status bar reports how many were skipped, or that nothing visible was left.
- Temp files now go to `%TEMP%/Fission360-Slicer`.

### 1.13.13 — Orca ribbon icon
- **Send to Orca** ribbon button uses the official Orca Slicer logo (`assets/icons/orca.svg`, glyph `orca`) instead of the generic export arrow.

### 1.13.12 — 2026-10-18: Linked design inserts; MANUFACTURE workspace; pinned tools; STEP crash guard
- **Linked designs**: drag a design tile from Projects (or a `.f360` / STEP / IGES / STL / OBJ file from Explorer) onto the viewport to insert it; Projects ▸ right-click ▸ *Insert into Current Design (linked)* does the same. A `.f360` insert remembers its source (path, documentId, contentRevision). When the original is saved with changes, a prompt offers **Update / Not Now**. Browser ▸ **Linked Designs**: Update from Original, Open Original, **Unlink** (keeps the bodies, never asks again), Delete Insert. Body right-click also has Unlink. New bodies are selected and **Move** starts, as with McMaster parts.
- **Fix — extruding a face of an imported STEP froze / killed the app**: the Join boolean itself took ~0.1 s on the McMaster nut, but the cleanup pass after it (`ShapeUpgrade_UnifySameDomain`, which merges coplanar faces) never finished on the thread's B-spline faces. Shapes with free-form faces now skip that cosmetic merge. All 8 flat faces of the nut now extrude with Join / Cut / New Body in 0.1–0.2 s.
- **Hang / crash guards**: Join / Cut / Intersect booleans stop after 20 s (OCCT progress cancel) with a message suggesting New Body. Booleans now also run in parallel. The OCCT build has `No_Exception`, so access violations inside OCCT used to kill the process; `OSD::SetSignal` + `/EHa` now turn them into feature errors, and `FissionFeature::run` catches everything else. On the McMaster threaded rod, joins on the threaded ends take 13–33 s in OCCT, so some hit the limit; New Body works on every face.
- New `--stepbench <file>` imports a STEP and extrudes every face (join / cut / new body), printing each step; `FISSION_TRACE_BOOLEANS=1` prints boolean / merge times.
- **About**: the GPLv3 text is now read from the resources in C++ (QML's XHR blocks `qrc:` reads, so the text box was empty).
- **MANUFACTURE** is now its own workspace under **DESIGN ▾** (Design / Render / Manufacture), no longer a Design ribbon tab.
- **About Fission 360...** at the bottom of the File menu.
- **Pin tools**: right-click any ribbon button or group-menu row ▸ *Pin to Toolbar* (shows in a PINNED group on every tab), *Unpin from Toolbar*, *Remove from Toolbar* (hides a quick button; still in its group menu), *Reset Toolbar*. Saved in settings.
- Bodies expose `sourceFeature` (the feature that created them).
- `.gitignore` now excludes `docs/fusion_*` (Autodesk reference material must never reach a public repo).
- Includes the perf cleanup below (now built).

### Perf cleanup (built in 1.13.12)
- **Lazy McMaster**: `McMasterInsertDialog` + QtWebEngine load only on first Insert McMaster (biggest cold-start win).
- **Defer** `.f360` shell registration until after the first event-loop tick.
- **Coalesce** timeline rebuild (16 ms) and viewport tessellate (32 ms) so rapid edits don’t remesh every property tick.
- Still optional later: lighter Live Render path, async File ▸ Insert STEP, trim always-on add-in scan.

### 1.13.11 — 2026-10-18: Orca STL handoff; McMaster async import + Move
- **Send to Orca** writes a temp **STL** (not 3MF) so Orca no longer warns about a foreign project/version.
- **McMaster** STEP import runs off the UI thread (`importCADAsync`); dialog closes while loading; after insert, new bodies are selected and **Move** starts so you can place the part.

### 1.13.10 — 2026-10-18: Send to Orca; McMaster resize/download; recovery UX
- **Phase 12** renamed **Manufacturing**: **Send to Orca** (temp 3MF + launch) is wired; Fusion-like MFG tools listed for later (no `libslic3r` embed).
- **MANUFACTURE** ribbon tab with Send to Orca + planned milling/turning/setup placeholders. Also File menu / command search.
- **McMaster** dialog is draggable/resizable; intercepts in-page **3-D STEP** downloads into the design; **Insert STEP** for the current product page; better PN-from-URL parsing.
- **Recover Autosave**: no startup prompt (File ▸ Recover Autosave only); **Delete** per entry; Recover clears that autosave from the list.

### 1.13.9 — 2026-10-18: McMaster in-app browser (Chrome); About + GPLv3
- **Insert McMaster-Carr** is an in-app popup with an embedded Chromium (`QtWebEngine`) view — not a full desktop browser window. Headless **Google Chrome** (CDP) still bootstraps Akamai cookies for STEP download. Opening a product page auto-selects 3-D STEP and imports it.
- **About Fission 360** (UTILITIES / Help / command search): version + full GPLv3 text. Project ships `LICENSE` (GNU GPLv3).

### 1.13.8 — 2026-10-18: McMaster browse in Edge — auto STEP into design
- **Insert McMaster-Carr** opened a visible Edge window on mcmaster.com (superseded by the in-app WebEngine popup in 1.13.9).

### 1.13.7 — 2026-10-17: Live Render stays in Render; McMaster on INSERT ribbon
- **Live Render** no longer jumps back to Design when clicked in the Render workspace (entering Render already turned Live Render on, so the button was treated as “off” and called `enterDesignWorkspace`). Toggle now only switches viewport shading; use **DESIGN ▾ → Design** to leave Render.
- Live Render applies a stronger sun / gradient (or cached HDRI) look; turning it off restores a flat CAD background.
- **Insert McMaster-Carr** is a third INSERT quick button (`quick: 3`). Refresh `dist\Fission360\` from `build-rel\Release` — an older dist copy lacked the menu entry.

### 1.13.6 — 2026-10-17: Large-file save no longer freezes the UI
- **Save** on designs with big Fusion `snapshotStepB64` blobs used to pretty-print the whole JSON twice on the UI thread (Indented + a second `toJson` for version history), which made Windows show **Not Responding**.
- Save now encodes **Compact** once, writes in 1 MiB chunks while pumping the event loop, reports progress on the status bar, and reuses that payload for version history.
- Selftest `testLargeFileSave`: ~11 MiB synthetic timeline, save under 8 s (measured **136 ms**), progress reports, round-trip reload. Suite **339/1**.

### 1.13.5 — 2026-10-17: Insert McMaster-Carr STEP (search + download)
- **Insert ▸ McMaster-Carr** (also in command search): search by part number or description, pick a result, download **3-D STEP** from McMaster-Carr, and import it into the current design via `importCAD`.
- Optional **STEP without threads**. Results show thumbnail + PN + title. Files cache under AppData `McMasterCarr/`.
- Uses McMaster's public Suggest + ProductContent endpoints. A short headless Edge session bootstraps Akamai session cookies, then Qt networking does search and download (Microsoft Edge required on Windows).
- Offline selftests cover PN extraction, bot-challenge detection, and STEP path selection. First build of this round was **331/1**; superseded by **339/1** at 1.13.6 (`loft: rail guide` still fails).

### 1.13.4 — 2026-10-17: Phases 2 and 3 finished (built as of 1.13.6)
- **Revolve and Sweep from a body face**: like Extrude, they take a flat face instead of a sketch profile. A face picked before starting is used.
- **Press Pull (Q)** on a face starts **Offset Face**. An edge still starts Fillet, anything else Extrude. Offset Face shows its arrow even at zero distance.
- **Variable fillet**: Radius type Constant / Variable. Variable runs from a start radius to an end radius along each edge (`radiusType`, `radius2`).
- **Loft guide**: None / Rail / Centerline (`guide`, `rail`, `centerline`). The guide is a sketch curve in its own sketch and must run through every section. A rail without a centerline uses a spine through the section centres. Only one rail, because OCCT takes a single guide.
- **Sweep twist angle** (`twist`): the profile turns by that angle along the path.
- **Hole / Thread standards**: Standard (Custom, ISO metric coarse, ISO metric fine, Inch UNC, Inch UNF) and Size.
  - A sized hole drills the tap drill when tapped. Otherwise it drills the ISO 273 or ASME close / normal / loose clearance hole (`standard`, `size`, `fit`), and shows its designation.
  - Thread takes a standard and either a size or "Fit the face", which picks the nearest size.
  - Scripts and QML get the tables from `doc.threadSizes(standard)`.
- **Move by dragging a body**: with Move open and its triad showing, dragging one of the picked bodies moves it freely in the view plane.
- **Draft pull direction** can be a round face (its axis), a straight edge, a construction axis, X/Y/Z or a sketch line, as well as a flat face or plane.
- **Region selection skips hidden items**: window, freeform and paint selection test each sample against a coarse depth grid of the shaded body faces. Faces, edges and points behind other bodies are no longer picked. Sketch curves and profiles are always picked.
- **Joint (J)** and **As-built Joint (Shift+J)**, in SOLID ▸ ASSEMBLE. Bodies stand in for components.
  - Pick the bodies that move, then the joint origins. An origin is a flat face where you click (along its normal), a round face (its axis), a circular edge (its centre) or a straight edge (its midpoint).
  - A Joint snaps the moving origin onto the target with the faces meeting (Flip turns it the other way). An As-built Joint leaves the parts where they are.
  - Motion types: Rigid, Revolute, Slider, Cylindrical. Angle turns the parts about the joint Z and Offset Z slides them along it; the offset has a drag arrow.
  - As-built joints are named "As-built Joint1", and so on.
- **Shift+N: component colour cycling**: each body is drawn in its own hue. Press again, or use ASSEMBLE ▸ Component Colors, to go back to the appearances.
- The Shortcuts dialog drops the "not in Fission yet" row and lists dragging a body during Move.
- New selftest group `testPhase2And3` (~17 checks): revolve from a face, variable fillet, sweep twist, loft centerline and rail, standard hole and thread sizes, draft from an edge, the Offset Face arrow, Joint and As-built Joint. Included in later suite runs; `loft: rail guide` still fails as of 1.13.6 (**339/1**).
- Docs: `FISSION_360_PROJECT.md` Current State / Tech Stack / Architecture / Phase 2–3 roadmap updated for that round (then 106 C++ / 52 QML; occlusion, Joint, face profiles, standards tables).

### 1.13.3 — 2026-10-17: Fusion body names and mesh bodies (exporter 1.6)
- **Body names from Fusion**: bodies imported from a Fusion export keep their Fusion names (for example "Body188" or "support") instead of "Body 1, Body 2…". The names come from the solids in each step's STEP. When the same name exists in two components, the component is added, for example "Body1 (Mobridge K2 v2)". This already works for files exported with add-in 1.5.
- **Exporter 1.6** writes a `stepBodies` list (name plus bounding box in mm) with every per-step STEP. Fission uses it to name solids whose STEP carries no name.
- **Mesh bodies**: exporter 1.6 writes Fusion mesh bodies to `meshBodies`, as raw triangles in mm and world coordinates, with occurrence transforms applied. Fission brings each one in as an **Insert Mesh** feature at the end of the timeline. The result is a real mesh body that keeps its Fusion name.
- The exporter hides mesh bodies while writing each STEP and turns them back on afterwards. Otherwise Fusion writes them as faceted B-rep (one face per triangle), which made large exports such as CompRT much bigger. If a STEP still holds such a copy, the imported mesh replaces it.
- Insert Mesh gains an optional `bodyName` prop.
- Selftest 308/0: new checks cover named step bodies (matched by bounding box), named mesh import, and replacing the STEP copy. Built and verified on CompRT: Fusion names show in the browser.

### 1.13.2 — 2026-10-17: Live Render in viewport, floating Render panel, floating ✕ fix (built with 1.13.3)
- **Live Render** only turns on sun / HDRI shading in the design viewport — no PNG save dialog. Toggle again to leave the Render workspace.
- **Render Settings** is a **floating panel** (like Appearance): HDRI / lighting / **Export Image…** for stills.
- **Floating panels**: title-strip drag no longer covers the close control (✕ was stolen by `DragHandler`).
- Viewport refreshes on `renderSettingsChanged` so Live Render / HDRI changes appear immediately.

### 1.13.1 — 2026-10-17: Fix OOM crash opening large Fusion exports with long timelines
- **Root cause (not portable-specific):** Fusion bulk exports with per-step BRep (`CompRT_…` ~180 MB, 181 steps) converted **every** STEP through OCCT on open, kept a duplicate `fusionTimeline`, and decoded all snapshots into the rebuild cache — multi‑GB peak → crash on open.
- **Fix:** keep exporter STEP as `snapshotStepB64` and convert **lazily**; rebuild applies only the **last** fusion snapshot at/before the marker; drop duplicated `fusionTimeline`/`fusionFeatures`; release the raw JSON tree before rebuild; catch `bad_alloc` with a clear error.

### 1.13.0 — 2026-10-17: Design linkage, version history, autosave, UI walkthrough GIF
- **Linked drawings & renders**: each design has a stable `documentId` and monotonic `contentRevision`. Creating a drawing or saving a render registers a `linkedAssets` entry; edits bump the revision and mark assets **out of date**. Drawing workspace / DRAWING ▸ **Update from Design** regenerates views and restamps the revision.
- **Internal save versions**: every Save writes a compressed snapshot under `<file>.f360.versions/` (capped, default 8). **File ▸ Version History** lists them; **Revert** restores a snapshot (linked assets go stale so drawings can be updated).
- **Autosave**: dirty designs are written to AppData on a timer (default 120s; Display Settings interval, 0 = off). Startup and **File ▸ Recover Autosave** restore after power loss; a successful Save clears that design’s autosave.
- **UI walkthrough GIF**: `--uitest` ends with `uiTest.exportGif("walkthrough.gif")` via a lightweight `GifWriter` (scaled 256-colour frames).
- Self-tests: drawing sync / update-from-design, version history + autosave round-trip, GIF writer.

### 1.12.1 — 2026-10-17: Render under DESIGN ▾, floating Appearance, live viewport, sketch trim paint + snap toggle
- **Render workspace** moves under the **DESIGN ▾** menu (Design / Render), Fusion-style — no longer a Design ribbon tab beside SOLID.
- **Appearance** is a **floating panel** (like Inspect / Properties). With no selection it paints all **visible** bodies; faces/bodies still take priority when selected.
- **Live Render** enables viewport sun / IBL shading for visible bodies (uses a cached HDRI when available). Render Settings remains for HDRI download and capture.
- **Trim (T)**: click still trims a curve piece; **drag paint-deletes** curves under the cursor (one undo step).
- **Snap to grid** checkbox on the sketch panel (persisted); endpoint snaps still win when grid snap is off.
- Self-tests: **291**.

### Added (external)
- **Fusion 360 Bulk Exporter** (`../F3D_Bulk_Export`, now 1.5) — Fusion add-in that exports the active design, all open designs or the active Data Panel folder (that folder only) to `.f360`, with STEP, per-step BRep and an embedded preview. The first version's recursive Data Panel tree was removed because walking the cloud folders hung Fusion.
- **Fusion export format support** in `FissionDocument::load()` — detects Fusion-exported `.f360` (`document`/`metadata`) and converts via `convertFromFusionExport()`.
- **Poly Haven (CC0)** — HDRI and texture CDN used by `RenderAssetStore` (downloaded on demand into AppData; not redistributed in the repo).

### 1.12.0 — 2026-10-16: Phase 10 complete — path tracer, PBR maps, per-face appearance
- **Monte Carlo path tracer** (`PathTracer`): CPU BVH + multi-bounce PBR with sun shadows and HDRI environment. Render dialog Engine = **Path Trace**; Quality maps to 8 / 32 / 96 / 256 spp. Viewport supersample remains as the alternate engine.
- **PBR maps**: Appearance / body / face store `roughnessPath`, `metalnessPath`, `normalPath` beside albedo. `RenderAssetStore::downloadMaterial` fetches Poly Haven 1K `diff` + optional `rough` / `metal` / `nor_gl` (404s skipped). Viewport triplanar sampling; path tracer uses the same maps.
- **Per-face appearance**: select faces → Appearance applies overrides (`faceAppearances` in `bodyDisplay`); body-level apply when only bodies are selected. `applyAppearanceToSelection` / `setFaceMaps`.
- Self-tests: **289** (face override round-trip, path-traced PNG, spp mapping, map path naming).

### 1.11.1 — 2026-10-16: HDRI download, IBL, textures, offline supersample
- **In-app Poly Haven downloads**: `RenderAssetStore` caches HDRIs and material albedos under `%AppData%/FissionCAD/Fission 360/Assets/`. Render ▸ Lighting: pick an HDRI, choose **1K / 2K / 4K / 8K**, **Download** (progress in the status bar), then **Use**. **Open cache** reveals the folder. Large files are never shipped with the app. Links `Qt6::Network`.
- **Viewport IBL**: downloaded `.hdr` loads via a Radiance RGBE reader (`HdrImage`); equirectangular sky replaces the gradient when background = Environment; mesh shading samples the map for ambient + specular (intensity / rotation sliders).
- **Appearance textures**: Appearance ▸ Textures (CC0) lists Poly Haven 1K diffuse maps; **Get** downloads and applies with world-space triplanar mapping (saved as `texturePath` on the body).
- **Offline Render**: Quality sets supersample (1× / 1.5× / 2× / 3×); Render captures the lit viewport at that size (viewport path; full offline path tracer still optional later).
- Docs: `FISSION_360_PROJECT.md` gains a **Render & Assets** section; `.f360` notes cover `bodyDisplay` PBR fields and `render` settings.
- Self-tests: **281** (asset store URLs, tiny `.hdr` load).

### 1.11.0 — 2026-10-16: Phase 10 Render & Appearance
- **RENDER ribbon tab** plus UTILITIES / MANAGE entries. **A** opens body **Appearance** (materials). Viewport background / zoom moved to **Display Settings**.
- **Appearance library** (`AppearanceLibrary`): 20+ built-in PBR presets (plastics, metals, glass, ceramic, rubber, fabric, wood/stone/concrete inspired by ambientCG / Poly Haven CC0). Assign to selected bodies; colour, roughness, metalness and opacity drive the viewport shader. Saved in `bodyDisplay` with the design.
- **Render settings** on the document (`RenderSettings`): quality, background, ground, sun lighting, environment path, output size/format. Persisted in the `.f360` `render` object.
- **Render dialog**: Quality / Scene / Lighting / Output. Built-in light presets (Studio Soft, Bright, Warm Key, Cool Overcast, Product Dark). Curated **Poly Haven CC0 HDRI** catalog (studio / indoor / outdoor) with 1k download URLs; credit shown in the UI. **Render** captures the viewport at the chosen resolution.
- **Viewport shading**: mesh vertices carry roughness/metalness; sun direction from render settings; metals get stronger tinted specular, rough surfaces flatten the highlight.
- `src/kernel/render/`: `Material` (Appearance), `RenderSettings`, `AppearanceLibrary`, `RenderEnvironmentLibrary`. QML: `FissionRender/AppearanceDialog.qml`, `RenderDialog.qml`.
- Roadmap: Phase 10 is Render & Appearance; **OrcaSlicer / MFG** moved to Phase 12 (later manufacturing stack). Phase 11 remains file sync.
- Self-tests: **279** (appearance library, Poly Haven catalog, aluminum apply, materials + render settings round-trip).

### 1.10.7 — 2026-10-16: build fixes + docs
- `CMakeLists.txt` builds `Fission360Thumbs.dll` again (the target had dropped out; the DLL in `build-rel` came from a stale project file) and the app depends on it.
- The app icon is linked into `Fission360.exe` again, through its own `Fission360Res` target: compiling the `.rc` inside the app target fails in rc.exe ("filename or extension is too long") because of the OCCT include list.
- `FISSION_360_PROJECT.md`: new sections for the `.f360` file format and the Fusion import pipeline (add-in options, per-step snapshots, pass-through features, installer to-do); add-in folder in the layout; build note for the DLL and icon targets; Phase 6 lists Fusion import.

### 1.10.6 — 2026-10-16: one-file previews (no PNG sidecar)
- The preview PNG lives only inside the `.f360`. Fission's own save now writes `"thumbnail"` as the first key (Qt sorts JSON keys, so it used to land after `bodies`); the Fusion exporter already did.
- Fusion exporter 1.5 (manifest 1.5.0) stops writing `*.f360.png` and deletes the sidecar left by 1.3–1.4 when it re-exports a file.
- Projects panel reads the embedded preview first, in 256 KB chunks until the value closes (cap 64 MB, so older files with the key further down still work); the sidecar is only a fallback for old exports.
- `Fission360Thumbs.dll` (Explorer thumbnails, registered with `--register`) accepts `"thumbnail":"` with or without the space.
- Self-tests: 272 (preview is the first key in a saved file; embedded preview reloads with the model).

### 1.10.5 — 2026-10-16: Fusion timeline scrubbing shows the BRep history
- **Per-step BRep for Fusion imports**: Fusion exporter v1.4 (manifest 1.4.0) rolls the Fusion timeline marker after each solid step and stores a STEP snapshot on that timeline entry (`stepData`, or `stepEmpty` when no bodies exist yet), then restores the marker. New option "BRep per timeline step", on by default; files get bigger and export is slower on long timelines.
- On load each snapshot is converted to an encoded BRep in the feature's `snapshot` prop; when a `fusionPassThrough` feature runs, its snapshot replaces the bodies (body names kept by position). Moving the timeline marker now shows the model as it was after that step. Fusion steps that Fission has no matching type for are kept as `ImportCAD` steps that carry their snapshot, so they are no longer dropped.
- Exporter 1.4.1: the full trailing STEP is only left out when at least one real step snapshot was written (all-empty snapshots no longer produce a file without BRep).
- Files with snapshots skip the single trailing STEP import. Older exports (v1.3) still load the same way as before.
- Self-tests: 270 (synthetic three-step Fusion export: final model, marker at step 1, marker at step 2).

### 1.10.4 — 2026-10-16: full Fusion timeline + embedded previews
- **Full Fusion timeline import**: every Extrude / Fillet / Hole / … lands on the Fission timeline with name, type, and numeric props (distance, radius, taper, profile sketch). Sketches rebuild for display; solid ops use `fusionPassThrough` so they stay healthy while BRep comes from STEP.
- **Projects panel preview**: Fusion exporter v1.3 embeds a viewport PNG as the first `.f360` key and writes a `*.f360.png` sidecar; Projects panel reads either.

### 1.10.3 — 2026-10-16: Fusion import timeline + project previews
- **Fusion `.f360` import**: Sketch features from curves + ImportCAD from STEP; avoided inventing broken Extrude stubs.
- **Projects panel**: head-scan for embedded thumbnail; capture-on-open if missing.

### 1.10.2 — 2026-10-16: manip box clearance, measure pair, zoom to cursor
- **Feature handle value box** no longer covers the distance arrow or the Extrude taper knob (placed clear of both).
- **Measure (I)**: the second face/edge/body/point click adds distance (ΔX/ΔY/ΔZ) and angle between the two selections without needing Shift.
- **Appearance ▸ Zoom to cursor**: optional mouse-wheel zoom toward the pointer (`viewport/zoomToCursor`); `ViewportController::zoomAt`.
- Tests: measure edge–edge distance/angle; zoomAt centre vs cursor.

### 1.10.1 — 2026-10-16: docs sync + Appearance key
- **A** opens Appearance (command search + Shortcuts); was still showing “not available”.
- Docs: `FISSION_360_PROJECT.md` / `CHANGELOG.md` match **1.10.x** — 87 C++ / 48 QML, Phase 9 complete, 262/262 self-tests, architecture tree includes DrawingProjector / AppearanceSettingsDialog / CreateDrawingDialog.

### 1.10.0 — 2026-10-15: viewport background + Phase 9 mop-up
- **Viewport background colour** (on the fly):
  - Top/bottom gradient on `BRepRenderer`; MANAGE / UTILITIES ▸ **Appearance** with presets and colour pickers.
  - Changes apply immediately and persist via `viewport/backgroundTop` / `viewport/backgroundBottom` settings.
- **Phase 9 Drawing finished**:
  - Parts list (BOM), revision table, bend table; sheet images; ANSI31-style section hatch; TIFF export; drawing history list.
  - DRAWING ribbon TABLE group; DWG remains out of scope (use DXF).
- Tests: **262/262** self-tests (drawing tables/TIFF/history + viewport background colour).

### 1.9.0 — 2026-10-15: Phase 9 Drawing complete
- **Drawing / From Design (Phase 9)**:
  - **Create Drawing** dialog: ISO/ASME, first/third-angle, sheet sizes A0–A4 / A–E / custom mm, title block fields.
  - **HLR projection** (`DrawingProjector` + OCCT `HLRBRep`): base, projected (third/first angle), section, detail, and auxiliary views show real edges from the design.
  - Drawing browser (sheets + views), regenerate all views, placeable dimensions / text / hole notes / center marks.
  - Title block on the paper (part, drawn by, rev, date, material, standard).
  - **Export** the active sheet to **PDF**, **DXF**, or **PNG**.
  - DRAWING ribbon: Create / Annotate / Export / Back to Design.
- Tests: 258/258 self-tests (projection, projected orientation, section/detail, annotations, PDF/DXF/PNG).

### 1.8.0 — 2026-10-15: Fusion-style Accurate face groups, Drawing workspace started
- **Accurate face groups match Fusion Accurate** (same dialog defaults: Accurate, Boundary Tolerance **0.001**):
  - **Boundary Tolerance is a fraction of the mesh diagonal** (Fusion docs / API range 0..0.01), not millimetres. Default `0.001` ≈ 0.2 mm on the test stock (~203 mm diagonal). Earlier builds treated `0.001 mm` as an absolute length and produced ~1 group per triangle (blotchy fillets).
  - Groups are planes / cylinders / cones / spheres only — no free-form B-spline mop-up (that stays for Convert without groups).
  - Accurate grows primitives greedily from single-triangle plane seeds (large CAD flats stay one group; fillets become thin bands). Fast still uses angle + minimum group size; Accurate ignores both.
  - Convert with face groups keeps **one face per group** (no mop-up / no demote shatter). Fast groups on the test stock: 13 groups → 13 faces. Dense Accurate groups that cannot sew fall back to faceted Convert (same as before).
  - Test stock (~90k tris): Accurate ≈ **4 000** groups (was ~55k–88k when tolerance was misread as mm).
- **Phase 9 Drawing / From Design (started)**:
  - Kernel: `DrawingManager`, `DrawingDocument`, `DrawingSheet`, `DrawingView`, `DrawingSketch` under `src/kernel/drawing/`.
  - `fissionApp.drawingManager`; DESIGN ▾ ▸ **Create Drawing** opens a sheet (ISO / A3 / third angle defaults) and the DRAWING ribbon.
  - Drawing workspace QML (`FissionDrawing/`) with paper, title block, placeable base-view frames; **Back to Design** returns to the model.
  - Projected / section / detail views, dimensions, PDF/DXF export still planned.
- Build: restored the full QML resource list and missing OCCT object libs (`BRepAlgo`, `BRepFeat`, `LocOpe`, `TopOpeBRepTool`) after the Drawing scaffolding landed with a broken CMakeLists.
- Tests: 247/247 self-tests (drawing manager / sheet / base view), 163/163 UI checks (Create Drawing, DRAWING tab, base view, back to design; screenshot `57_drawing`).

### 1.7.0 — 2026-10-15: progress bar for long operations, prismatic convert without the facets
- **Progress bar in the status bar** (bottom of the window, after the status message) for anything that takes a while: what is running and how far along, a sweeping bar while the length is unknown, and a count when several tasks run at once.
  - Mesh **Convert ▸ Prismatic** of large meshes (background) reports every stage: welding, fitting surfaces, joining leftover facets, building faces.
  - **Face Groups ▸ Accurate** of large meshes now runs in the background as well (it used to freeze the window for about 10 s on the test stock) and reports the same way.
  - Scripts and add-ins get their own tasks: `fission.beginTask(id, label)`, `fission.reportProgress(id, fraction, label)`, `fission.endTask(id)` (JS and Python; snake_case works in Python). `fissionApp.busy`, `progressText`, `progressValue`, `taskCount` for QML.
  - Meshes over 20 000 triangles go to the background; smaller ones are converted straight away.
  - Kernel side: `FissionProgress` (thread-safe hub) and a `MeshUtils::Progress` callback on `toFittedBRep` and `generateFaceGroups`; a shared `MeshBackgroundJob` runs and caches the slow mesh work for the features.
- **Convert ▸ Prismatic is prismatic now**: the thousands of one- and two-triangle planes are gone.
  - A mop-up pass after fitting: leftover facets with a smooth rim join the fitted face next to them (free-form faces are refitted to run over them), and groups nothing takes get free-form faces of their own.
  - Free-form patches that fold over their base plane are cut across the bend (the direction the normals turn along), not lengthwise, so a long round-over splits into a few patches instead of hundreds of slivers.
  - Free-form fits sample along long edges and inside big triangles, with the rim weighted heavily, so a face follows its outline closely and its edges need no large tolerances.
  - Rim splines must stay on both faces they bound; otherwise the rim is kept as straight segments.
  - Free-form faces whose rims do not meet their neighbours are halved and refitted before they are given up as facets; models up to 6 000 faces are sewn when the faces do not close up by themselves.
  - Pieces joining a face must keep its outline a single loop (no pinched corners), and strips are not combed apart between two faces.
  - Test stock (90 498 triangles): Fast groups 4 506 faces → **735** (152 planes, 583 free-form); Accurate groups (dialog defaults) 6 733 faces → **852** (449 planes, 2 cylinders, 401 free-form); no face groups 566 faces. Edges outside the part: 0. The conversion runs in the background, about 11–15 s.
- Tests: 245/245 self-tests (progress hub, progress reports of a conversion, background conversion and face groups), 161/161 UI checks (progress bar: sweeping, 50 %, several tasks, gone when done).

### 1.6.0 — 2026-10-15: sheet metal and scripting finished, Explorer previews, cleaner prismatic convert
- **Explorer previews for `.f360` files**:
  - New `Fission360Thumbs.dll` (a Windows thumbnail handler, built with the app). It shows the thumbnail saved inside each design as the file's icon in Explorer.
  - Fission 360 registers the `.f360` file type for the current user on every normal start (no admin rights): its own icon, double-click opens it, and the preview.
  - `Fission360.exe --register` and `--unregister` do it by hand.
  - New app icon (original artwork, `assets/icons/make_icon.py`).
  - Thumbnails are cropped clear of the view cube and navigation bar.
- **Convert ▸ Prismatic, no more stray lines and loops**:
  - Free-form rims are fitted as smooth splines that are checked against the mesh, falling back to straight segments.
  - Arcs run in the right direction.
  - On the test stock with the dialog defaults (Accurate, 0.001 mm), edges outside the part went from 172 to 0.
  - Smooth face groups that one surface cannot cover become free-form patches instead of planes.
- **Sheet metal (Phase 7 complete)**:
  - **Contour Flange**: an open chain of sketch lines becomes a sheet with a bend at every corner. Options: distance, thickness side, direction.
  - **Hem**: flat (with gap), open (radius) or rolled (angle) fold of a sheet edge.
  - **Tab**: a sketch profile added to the sheet in its plane.
  - **Bend**: folds a sheet along a sketch line. The flat length is kept.
  - **Unfold / Refold**:
    - Unfold lays the sheet flat in place.
    - Holes and cuts made while it is flat follow the bends when Refold folds it back.
    - Pieces on the flat areas move rigidly; pieces on bends are wrapped in 3° slices.
  - **Flat Pattern**:
    - Unfolds every bend in timeline order, so flanges on flanges, hems, contour corners, bends and tabs all lay flat.
    - **Export DXF** (FLAT PATTERN group) writes the outline plus the bend centre lines on a `BEND` layer.
  - **Sheet Metal Rules** dialog:
    - Edits the design's rule (thickness, bend radius, K-factor).
    - A rule library: Steel 1 mm, Steel 2 mm, Aluminium 1.5 mm, Stainless 0.8 mm, Copper 0.5 mm, plus your own rules, saved for every design.
  - New original toolbar glyphs for each command.
- **Scripting (Phase 8 complete)**:
  - **Python scripts**:
    - Uses the Python 3.11+ you have installed: `FISSION_PYTHON`, or the newest 64-bit one in the registry.
    - Loaded through the stable ABI only when first used, so Fission 360 still runs without Python.
    - Python sees the same `app`, `doc` and `fission` objects as JavaScript. Every property and method is available, and snake_case names work too (`doc.add_feature`).
    - `import adsk.core, adsk.fusion` gives `Application.get().activeProduct` and `userInterface.messageBox`.
    - Output, error lines and the runaway-script stop work as in JavaScript.
  - **Add-ins**:
    - Scripts in `%APPDATA%\FissionCAD\Fission 360\AddIns` run when the app starts.
    - `addCommand(id, label, tooltip, fn)` (Python: `add_command`) puts a button in UTILITIES ▸ ADD-INS.
    - The Scripts dialog has an Add-Ins tab to switch each one on or off, reload it, and see its status and commands.
    - Commands always act on the active design.
    - Samples in JavaScript and Python are included.
- Tests: **239/239 self-tests** (flanges on flanges, hem, tab, bend, contour flange, unfold/refold, DXF, Python, add-ins, stray edges) and **156/156 UI walkthrough checks** (sheet metal toolbar, rule library, DXF, add-in buttons).

### 1.5.0 — 2026-10-15: free-form convert, center sketch tools, projects folder and panel
- **Projects folder** (default `Documents\Fission 360 Projects`; change it from the panel and the choice is remembered):
  - Save As and Open start there. Save on an unsaved design suggests `<projects>\<design name>.f360`.
  - Every save stores a 320 × 200 thumbnail of the view inside the `.f360` file.
- **Projects panel** (grid button left of the File menu, File ▸ Projects, Ctrl+Alt+P, or command search). It slides out from the left:
  - Folder and design tiles with thumbnails, folders first, and a breadcrumb with Up, New Folder, Save Here and Refresh.
  - Double-click opens a folder or a design.
  - Right-click on a tile: Open, Rename, Move to (any folder, or a new folder), Duplicate, Export (STEP, IGES, STL, OBJ, 3MF), Show in Explorer, Delete (to the Recycle Bin).
  - Right-click on empty space: New Folder, Save Current Design Here, Refresh, Show in Explorer.
  - Moving or renaming a design that is open keeps its tab pointing at the new file.
- **Center Rectangle** sketch tool: click the center, then a corner. The center point sits on a construction diagonal, so only the width and height remain free. Typed Width and Height become dimensions.
- **Center Line** sketch tool: click the midpoint, then an end. The line runs the same length the other way, with a midpoint constraint, horizontal/vertical snapping and a typed Length.
- **Generate Face Groups, Fusion-style**:
  - The dialog shows **Type** (Accurate / Fast).
  - Accurate has a **Boundary Tolerance**, default 0.001 mm, which sets how closely the fitted planes and surfaces must follow the mesh.
  - Fast keeps the angle threshold.
  - Designs saved earlier keep the automatic tolerance.
- **Convert ▸ Prismatic, free-form areas**:
  - A face group that no plane, cylinder, cone, sphere or extrusion fits becomes a fitted B-spline surface face instead of hundreds of planar facets.
  - Large or folded areas are split into a few patches.
  - On the 90 000-triangle test stock with Accurate groups, the result went from about 55 000 faces to about 4 800 (about 270 of them free-form faces) in about 7 s.
  - A free-form face that will not join its neighbours falls back to facets instead of failing the convert.
- **Mesh body icon** in the browser is now a pyramid, matching Insert Mesh.
- Tests: 219 self-tests, 145 UI walkthrough checks.
  - New self-test: a wavy mesh top converts to one B-spline face.
  - New UI checks: center rectangle and center line; projects save with thumbnail, sub folder, panel listing, move of an open design, duplicate, export, rename, browse and delete.

### 1.4.0 — 2026-10-15: Phase 5 (mesh) and Phase 6 (export & import) finished, much faster on large meshes
- **Generate Face Groups** (MESH ▸ PREPARE): splits a mesh into coloured face groups.
  - **Accurate** finds planes, cylinders, cones and spheres and joins the free-form areas. **Fast** splits by the angle between neighbouring triangles.
  - Groups smaller than the minimum size are merged into a neighbour.
  - Convert ▸ Prismatic makes one face from each fitted group.
- **Remesh**: even triangles at a target edge length.
- **Mesh display**: a viewport menu switches between shaded, wireframe, shaded + edges, vertices and face-group colours. Each body has its own colour and opacity.
- **Mesh statistics** in Properties: vertices, triangles, open edges and whether the mesh is manifold.
- **Mesh body icon**: mesh bodies have their own icon in the browser; solid bodies keep the cube.
- **Drag handles**: Mesh Shell has a thickness arrow. Plane Cut has an Offset field with an arrow along the plane normal.
- **Prismatic convert of extruded shapes**:
  - Free-form walls of an extrusion become one swept face each.
  - Walls are split at sharp corners of the profile.
  - Short straight edges are no longer mistaken for parts of a curve.
- **Convert no longer selects anything on its own**: pick the mesh after the dialog opens.
- **Speed on large meshes** (measured with the new `--uiperf <stl>` script on a 90 000-triangle STL):
  - Body volume, area and centre are cached. Planar triangulated faces are summed directly, so updating the Properties palette went from about 5.7 s to near zero.
  - Picking under the mouse uses cached face/edge maps and bounding-box chunks of triangles instead of testing every triangle on each move.
  - Convert runs in a background thread for meshes over 20 000 triangles; the dialog shows "Converting … in the background" until it finishes.
- **STEP / IGES import with assembly structure**: bodies are named "Assembly/Part", with instance and part colours and placements.
- **3MF export** (body names and colours) from Export, Save As Mesh and Batch Export.
- **DXF from a planar face**: pick a planar face and use Export Face as DXF (INSERT group, File menu or command search). Sketch DXF export now uses the same writer.
- **Batch Export** (File menu, INSERT/EXPORT groups, command search):
  - Choose bodies, formats (STEP, IGES, STL, OBJ, 3MF) and a folder.
  - Writes one file per body, or one file for all the bodies.
- Tests: 218 self-tests, 132 UI walkthrough checks.
  - New self-tests: fast mass properties, Plane Cut offset, Shell arrow, STEP assembly import, 3MF package, face DXF and batch export.
  - New UI checks: mesh icon flag, face-group display, 3MF export, face DXF and the Batch Export dialog.

### 1.3.0 — 2026-10-15: Smoother app, design tabs, prismatic convert on organic meshes, sketch Move fixed
- **Far fewer "Not Responding" freezes**:
  - **Incremental timeline rebuild**: each feature has a revision number, and the document keeps the result after every timeline step. A change only recomputes from the first changed feature onward; the cache is dropped when parameters or units change.
  - **Body display cache**: the viewport keeps the tessellation of each body and only re-tessellates bodies whose shape actually changed.
  - **Hover no longer re-uploads the scene**: pre-highlight is drawn from its own small GPU buffer, so moving the mouse over a large model stays cheap.
  - Operations over 50 ms (each rebuilt feature, tessellation, scene assembly) are logged to the `FISSION_LOG` file to find remaining slow spots.
- **Design tabs**: every open design gets a tab at the top of the window.
  - New Design (Ctrl+N or the + button) and Open add a tab instead of replacing the current design; opening a file that is already open switches to its tab.
  - Click a tab to switch to it, or use Ctrl+Tab / Ctrl+Shift+Tab. Each tab keeps its own camera.
  - Close a tab with its ×, a middle click, Ctrl+W or File ▸ Close Design. Unsaved designs ask first. Closing the last tab leaves an empty design.
  - A running command, sketch or sketch move is finished before switching tabs.
- **Convert Mesh ▸ Prismatic on organic meshes**: it no longer fails with "the fitted faces do not meet". Curved patches whose fitted surfaces cannot be joined are turned back into flat facets (up to 8 passes), so the result is a closed solid with true planes / cylinders where possible. The summary reports how many curved patches were left faceted.
- **Sketch Move (M) fixed**: it works like Fusion: press M first and then click the curves, or select them first. Sketch curves picked outside the sketch also start a sketch move.
- Tests: 202 self-tests, 125 UI walkthrough checks (new: real-mouse sketch move, design tabs).

### 1.2.0 — 2026-10-15: Move/Copy like Fusion, faceted meshes, capped section view, mesh Repair / Shell / Combine
- **Mesh display**: mesh bodies are drawn faceted (flat shading per triangle) with every triangle outlined while edges are shown.
- **Convert Mesh no longer appears to hang** on large meshes (it was busy, not crashed):
  - The viewport no longer re-tessellates large converted solids that already carry triangles (about 9 s saved on a 30 000-triangle STL).
  - The conversion result is cached per mesh and method, so later rebuilds are instant.
- **Move/Copy (M)**, rebuilt after Fusion's command:
  - Move types: **Free move** (X/Y/Z distance and X/Y/Z angle about a pivot, with Set Pivot), **Translate**, **Rotate** (about an axis), **Point to Point** and **Point to Position**, plus **Create copy**. Mesh bodies can be moved too.
  - An on-screen **triad**: drag an arrow to slide along an axis, a square to slide in its plane, a ring to turn (Shift = 15° steps), or the centre ball to drag freely. Values snap and show in a readout.
  - Points are picked in the view and snap to vertices, circle centres, edge midpoints and the origin.
  - Designs saved before 1.2.0 keep their old move behaviour.
- **Move/Copy in sketches**: selected sketch curves (even a single line) can be moved, rotated or copied with their own panel and a 2D triad. Constraints tying them to curves left behind are dropped.
- **Section Analysis**:
  - It no longer changes the body. The cut is drawn on the GPU, and the inside is shown as a **light-green hatched cap**, so it reads as different from the outside. Meshes are capped too.
  - Opening Section Analysis shows the origin planes and waits for a plane or face to be picked before cutting.
- **Mesh Repair** (MESH > PREPARE): welds vertices within a tolerance, removes duplicate triangles, makes the normals of each piece agree (pointing out of closed pieces) and fills holes. The panel reports what it fixed.
- **Mesh Shell** (MESH > MODIFY): hollows a closed mesh, or thickens an open one into a closed wall, inward or outward.
- **Mesh Combine** (MESH > MODIFY): merges several mesh bodies into the first one.
- **Debugging**: set `FISSION_LOG=<file>` to write Qt / QML warnings to a file (the Windows build has no console).
- Tests: 202 self-tests (+20: move types, sketch move, mesh repair / shell / combine), 118 UI walkthrough checks (+6: move triad, point to point, sketch move, section plane pick).

### 1.1.0 — 2026-10-14: section analyses, mesh fitting, STEP/IGES import, scripts, sheet metal (Phase 7)
- **Draft with two faces**: the first face picked sets the pull direction, and the second (and any after it) is the face to draft. The pull face may be any planar face and is resolved on the body before the draft.
- **Hole placement**:
  - The hole has a move handle that slides over the whole face. The X/Y/Z position box in the hover panel is gone.
  - The handle snaps to the face centre, to the centres of other holes and circular edges on the face, and to the points and circle centres of visible sketches that lie on the face.
- **Fillet / Chamfer on a face**: picking a face (in the view or in the Edges input) takes every edge around it.
- **Select all and delete bodies**:
  - Ctrl+A selects every visible body.
  - Delete (or Remove in the body menu) adds a **Remove** feature to the timeline, so it can be undone or rolled back.
- **Section Analysis, finished**:
  - The cut can be any planar face, construction plane or origin plane: pick it while the panel is open, or use the XY / XZ / YZ buttons. A new cut starts through the middle of the model.
  - The cut faces of solids are **capped** with a darker version of the body colour.
  - **OK saves the cut** as an analysis (Section1, Section2, ...) in a new **Analysis** folder in the browser. It has an eye toggle; double-click or Edit reopens it; it can be deleted.
  - Only one section is shown at a time. Sections are saved with the design and take part in undo.
- **Convert Mesh → solid with fitted surfaces** (`MeshFit.cpp`), with a method choice:
  - **Prismatic**: the mesh is split at sharp edges, and each region is fitted with a **plane, cylinder, cone or sphere**. The faces are rebuilt on the true surfaces with line and circle edges. For example, a tessellated cone on a plate comes back as 6 planes and 1 cone, and a block with a hole and a fillet comes back with its original 8 faces.
  - **Faceted**: the coplanar triangles are merged into flat faces.
  - Regions that fit nothing stay faceted. If fitting fails, it falls back to faceted, then to one face per triangle, so the conversion always gives a solid.
  - The panel shows what was found (for example "7 faces (6 planes, 1 cone)").
  - It is linear in the triangle count: about 10 ms for 500 triangles and about 40–130 ms for the test parts.
  - Not yet fitted: organic (free-form / NURBS) surfaces.
- **Insert STEP / IGES** (INSERT group, `FissionImport`):
  - Reads B-rep files in mm.
  - Every solid becomes a body; loose surfaces become one surface body.
  - The geometry is compressed into the design, so the file is not needed afterwards.
  - Imported bodies work with every modeling tool.
- **Scripts and Add-Ins** (Shift+S, UTILITIES ▸ ADD-INS; `ScriptHost`, `ScriptsDialog.qml`):
  - JavaScript scripts with full access to the application:
    - `app`: every command, the selection and the view;
    - `doc`: features, parameters, bodies, measure, save and export;
    - `fission`: new or open documents, messages and the version;
    - `print(...)`.
  - Scripts live in `%APPDATA%/FissionCAD/Fission 360/Scripts`, which is seeded with three samples. The dialog edits, saves and runs them (Ctrl+Enter runs).
  - Errors report their line. A runaway script is stopped after 30 s.
- **Phase 7: Sheet metal started** (SHEET METAL tab, `FissionSheetMetal`):
  - **Sheet Metal Rules** adds the parameters SheetThickness (1 mm), BendRadius (1 mm) and KFactor (0.44); changing them updates every sheet metal feature.
  - **Flange** makes a sheet from a sketch profile.
  - **Edge Flange** bends a flange up (or down, with Flip) from an edge of the sheet, with height, angle and bend radius.
  - **Create Flat Pattern** unrolls the bends into a flat body, using bend allowance = angle × (R + K × T).
  - Not yet available: Bend, Contour Flange and Unfold. Flat Pattern does not unroll flanges built on other flanges; it reports how many it skipped.
- Version **1.1.0**.
- Tests: 37 new self-tests and 12 UI checks.
  - Self-tests cover Remove, edges of a face, draft with a face pull, hole snaps, section planes and analyses (save, load, undo), prismatic and faceted mesh fitting (cylinder, cone, dome, hole + fillet), the STEP / IGES round trip, sheet metal volumes and flat-pattern length, and scripts (print, error line, timeout, save / load).
  - UI checks cover section save, caps and hiding; fillet on a face; Ctrl+A and Delete with undo; a script driving the document; and the SHEET METAL tab.
  - `--selftest` **182/182**, `--uitest` **112/112**.

### Added — 2026-10-14: solid tools, ribbon tabs, typed handle values, Inspect (Phase 4) and Mesh (Phase 5)
- **New SOLID tools.** Each one is a timeline feature with its own dialog (`SpecDialog.qml`, built from field specs) and its own glyph:
  - **Hole** (H): Simple, Counterbore or Countersink; a distance or through-all extent; a drill-point angle (0 gives a flat bottom); and **Tapped**, which gives a modeled ISO metric thread or a cosmetic one. Click a flat face to place a hole at the clicked point, or pick a sketch to get one hole per sketch point. It has a depth handle.
  - **Thread**: pick a cylindrical face. External or internal is detected from the face. With an empty pitch it uses ISO coarse, and the panel shows the designation (for example "M10x1.5 external"). Modeled threads use a real helical 60° groove.
  - **Combine** (join / cut / intersect, with Keep Tools), **Move/Copy** (M; translate and rotate about an axis, optionally as a copy), and **Scale** (uniform or non-uniform, about a point).
  - **Split Body** (by a plane or a tool body), **Split Face**, **Offset Face**, **Offset** (offset copies of faces or bodies), and **Replace Face** (onto a plane).
  - **Draft** (fixed plane or parting line), **Pattern on Path**, **Rib** (wall grown to the next face) and **Web** (symmetric wall).
  - Shell, Mirror, Rectangular Pattern and Circular Pattern already existed.
- **Ribbon tabs like Fusion's**:
  - A DESIGN ▾ workspace button and seven tabs: SOLID, SURFACE, MESH, SHEET METAL, PLASTIC, MANAGE and UTILITIES.
  - Each tab has tool groups. The SOLID groups are CREATE, MODIFY, CONFIGURE, CONSTRUCT, INSPECT, INSERT, ASSEMBLE and SELECT; MESH adds PREPARE, CONVERT and EXPORT.
  - Each group shows its most used tools as buttons, and its "TITLE ▾" caption opens a menu with every tool in the group.
  - Tools from later phases are listed with a "planned" note instead of being left out.
  - A contextual SKETCH tab replaces the groups while a sketch is being edited.
- **Typing a number goes straight into the handle's value box.** While a command shows a size handle (Fillet, Chamfer, Extrude, Hole, ...), the first digit, "." or "-" replaces the value; there is no need to click the box first. The value is applied after a short pause or on Enter/OK. When the command ends, focus goes back to the view, so single-key shortcuts work again.
- **Phase 4: INSPECT** (with a floating panel):
  - **Measure (I)**: for one item, the area, normal, radius or diameter of a face; the length, radius or centre of an edge; the volume and area of a body; or a point's position. For two items: the minimum distance with ΔX, ΔY and ΔZ, and the angle between plane normals or lines (0–90°).
  - **Interference**: the overlapping volume of each pair of solid bodies.
  - **Section Analysis**: cuts the view with the XY, XZ or YZ plane, with an offset slider and a flip option. Inside faces seen through the cut are tinted.
  - **Zebra**, **Draft** (pull axis and minimum angle; green, yellow or red) and **Curvature Map** analyses are drawn by the shader.
  - Kernel: `FissionDocument::measure` and `interference` (in `FissionInspect.cpp`).
- **Phase 5: mesh bodies** (MESH tab):
  - **Insert Mesh** reads STL (ASCII or binary) and OBJ in the document units. The triangles are compressed into the design, so the file is not needed afterwards.
  - **Tessellate** (solid to mesh) and **Convert Mesh** (a closed mesh to a solid; up to 60 000 triangles).
  - **Reverse Normal**, **Plane Cut** (with Fill Hole), **Smooth** (Taubin smoothing, which does not shrink the mesh) and **Reduce** (keep a percentage of the triangles).
  - Mesh bodies show their volume and area in Properties and export to STL and OBJ.
  - Solid tools refuse mesh bodies and tell you to convert them first; automatic body choices skip them.
  - Kernel: `MeshUtils`, `FissionMeshFeatures`.
- **Fixed:**
  - Vector inputs showed "undefined" while empty.
  - Plane preset buttons (XY / XZ / YZ) overflowed narrow panels.
  - The Shortcuts dialog still listed H, M and I under "Not yet". They now appear with their commands, and the number-key entry was added.
- STEP / IGES export skips mesh bodies, because they have no surfaces; STL / OBJ export includes them.
- Tests: 49 new self-tests and 13 UI checks.
  - Self-tests cover every new solid tool by volume, holes of every type, modeled internal and external threads, the mesh round trip, cut, reduce, smooth and convert, Measure and Interference.
  - UI checks cover the tabs and groups, Hole from the ribbon, typing a fillet size, I for Measure, section and analyses, and tessellate plus plane cut.
  - `--selftest` **145/145**, `--uitest` **100/100**.

### Added / Fixed — 2026-10-14: extrude a face, window / freeform / paint selection
- **Fixed: extruding the top face of a tapered body rebuilt the whole body without its taper.** Extrude only took sketch profiles, so the picked face was ignored. The command then fell back to the last sketch, already used by the first extrude, and joined a second, untapered copy of it over the body.
- **Extrude a body face directly, with no sketch**: pick a flat face (before pressing E, or in the Profile input) and it is extruded from the body as it is, then joined or cut. Several faces in one plane can be extruded together. A face and a sketch profile are not mixed; the latest pick wins. Kernel: `RebuildContext::resolveFaceProfiles`, and Extrude's `profiles` holds face refs instead of profile indices.
- With nothing picked, Extrude and Revolve now start only from a sketch no feature has used yet. If every sketch is used, they wait for a pick.
- Like Fusion, dragging an extrude's distance arrow into the body switches Join to Cut, and dragging it back out switches it to Join. The very first solid never switches.
- **Selection tools** (Fusion's keys **1 / 2 / 3**, and a new SELECT group on the toolbar):
  - **Window (1)**: left-drag on empty space draws a box. Left to right takes items fully inside (solid blue box); right to left takes everything it touches (dashed green box).
  - **Freeform (2)**: drag a loop; items fully inside it are selected.
  - **Paint (3)**: everything the cursor passes over is selected.
  - Shift or Ctrl adds to the selection, as does any drag while a command is running.
  - What gets selected depends on the context: bodies (plus construction geometry) with no command, the active sketch's curves inside a sketch, and whatever the active command input accepts (edges, faces, profiles, ...).
  - 1/2/3 are off only while a sketch tool is drawing, so digits still go to the dimension boxes. Region selection does not test what is hidden behind other geometry; for a "fully inside" box that only matters if the back of an item is outside the box.
  - The Shortcuts dialog lists 1/2/3 and the drag controls; they are no longer under "Not yet".
  - Renderer: `BRepRenderer::pickRegion(polygon, crossing, filter)` tests the projected tessellation against the box or loop.
- Feature dialogs no longer show the "= 39.5°" line under a plain value; it only appears for an expression (for example `width / 2`) or an error.
- Tests: 3 new self-tests (face extrude join on a 40° tapered body, cut, arrow position) and 13 UI checks (face pick + E, no new sketch, taper kept, no sketch reuse, window / crossing / freeform / paint, window selection of sketch curves). `--selftest` **96/96**, `--uitest` **87/87**.

### Added — 2026-10-14: Fusion keyboard shortcuts, document settings and units
- **Keyboard shortcuts with Fusion's default keys** (from Autodesk's published shortcut list):
  - Model: **E** Extrude, **Q** Press Pull (starts Fillet when an edge is picked, otherwise Extrude), **F** Fillet, **V** hide/show the selected bodies, **Ctrl+B** recompute the timeline, **Del** delete the selected construction features.
  - Sketch: **L** Line, **R** Rectangle, **C** Circle, **T** Trim, **O** Offset, **P** Project, **D** Dimension, **X** construction on/off. Outside a sketch, a sketch key asks for a plane first and then starts that tool.
  - General: **Ctrl+N / O / S / Z / Y**, **Ctrl+Shift+S** Save As (in Fusion this key is a recovery save), **F6** fit, **Ctrl+Alt+B** browser, **Ctrl+Alt+V** ViewCube, **Ctrl+Alt+N** navigation bar, **Ctrl+Alt+R** reset the layout.
  - Fusion keys for commands that do not exist yet (**H** Hole, **M** Move, **J / Shift+J** Joint, **A** Appearance, **I** Measure, **Shift+S** Scripts, **Shift+N** component colours, **1/2/3** selection modes) show a "not available yet" message instead of doing nothing. 1/2/3 are off inside a sketch so that digits can be typed into the dimension boxes. (1/2/3 were built in the next round.)
- **S opens a command search** (Fusion's toolbox): type part of a command name, then use Up/Down and Enter. It lists only the commands that fit the current mode (model or sketch) and shows their keys.
- **Shortcuts dialog** (INSPECT → Shortcuts) lists every key, the mouse controls and the keys that are not available yet.
- **Document Settings in the browser**, like Fusion:
  - A "Document Settings" folder with a **"Units: mm, g"** row. Click it to open the settings, or right-click it to switch the length unit straight away. It also has "Part Design" and "Properties...".
  - The Document Settings dialog sets the length unit (mm, cm, m, in, ft), mass unit (g, kg, lb, oz), density, name, part number, description and designer, and shows the design type and file path.
  - The document row has a right-click menu with Rename, Document Settings and Save.
- **Named Views** folder: Home, Top, Front, Right, Bottom, Back and Left. Clicking one turns the camera to it and fits the model.
- **Units throughout the UI.** The model is still stored in mm and degrees; the unit applies to input and display only:
  - A number typed without a unit uses the document unit: "10" becomes "10 in" and "10 / 2" becomes "(10 / 2) in". Angle inputs take degrees and counts stay plain numbers. Existing dimensions keep their own units, so changing the unit never resizes the model.
  - This covers feature dialogs, sketch dimension boxes, constraint values, viewport manipulator fields and parameters.
  - Preview labels, cursor coordinates, dimension annotations and Properties (volume, area, mass, centroid, bounding box) are shown in the document units.
  - Units and the design info are saved in the `.f360` file, and changing them is an undo step.
- Kernel: `FissionDocument` gains `lengthUnit`, `massUnit`, `info`, `formatLength`, `formatMass`, `withUnit` and `setInfo`.
- Fixed after an outside edit: the browser panel (a duplicated header handler, and a missing body menu) and the renderer (a lost pick-only face skip, and a missing edge colour).
- Build: links `Qt6::Test` so the UI walkthrough can send real key presses. `windeployqt` deploys `Qt6Test.dll`.
- Tests: 7 new self-tests (unit defaults and switching, `withUnit`, evaluation in inches, formatting, save/load, undo) and 17 UI checks (sketch keys, construction toggle, inch input and labels, V, Ctrl+Alt keys, Press Pull, command search, E, Document Settings). `--selftest` **93/93**, `--uitest` **74/74**.

### Added / Fixed — 2026-10-14: review round (sketch input, handles, previews, mesh export)
- **Line axis snap**: while drawing a line, a direction within 4° of horizontal or vertical snaps onto the axis, and the preview label says "— horizontal" / "— vertical". Snapping to an existing point still wins.
- **Line angle box**: the line tool has a second text box, "Angle", next to Length. It takes degrees counter-clockwise from +X (a bare number means degrees; expressions are allowed). The preview label shows the length and the angle.
- **Finish Sketch returns to the home (ISO) view** and fits the model.
- **Re-editing a sketch shows its curves.** A sketch used by a feature is hidden, so editing it showed an empty view; it is now shown while edited and hidden again when finished.
- **Save As Mesh**: right-click a body in the browser → "Save As Mesh..." writes that body alone to STL (binary) or OBJ. The same menu has Hide/Show. Kernel: `FissionDocument::exportBodies(path, bodies, format)`; `exportFile` now uses it for the visible bodies.
- **One eye for the whole Origin folder**: shows or hides every origin plane, axis and the point. It reads "on" when any of them is shown.
- **Spline handles**: with no tool active, every fit point of a spline in the active sketch is drawn as a small square. Drag one to reshape the spline (one undo step, grid snap applies).
- **Extrude taper knob**: next to the distance arrow, a round knob on a dashed side line at the profile's edge. Drag it outward for a negative (widening) taper and inward for a positive one, in 0.5° steps and clamped to ±60°. Its label shows the angle. Kernel: `FissionFeature::setAngleManipulator`, with the manipulator map gaining an `angle` entry.
- **Mirror picks in order**: once a body is picked, the dialog moves on to the "Mirror plane" input. A flat face or a construction/origin plane is accepted. `FeatureDialog` supports `autoNext` for this.
- **Resize a construction plane by dragging an edge**: with no command running, press within 6 px of a plane's border and drag that side. The rectangle is kept in the new `display` prop (`[umin, umax, vmin, vmax]`) and is display-only. `FissionConstructionPlane::displayInfo()` exposes it.
- **Full preview of solid operations**: while a feature is edited, its result is drawn as the complete shaded solid (for example the filleted body) instead of an orange outline over the original body. The input body stays pickable underneath, and its faces and edges light up only when hovered or selected.
- Fixed: an animated view change followed by an animated "fit" (as Finish Sketch does) lost the rotation; a non-animated view change during an animation was overwritten by it.
- Tests: 5 new self-tests (taper knob placement and value, Save As Mesh body only, missing body error, plane resize) and 16 UI checks. `--selftest` **86/86**, `--uitest` **57/57**.

### Fixed — 2026-10-14: loft sections
- **A loft could not use a solid face.** Sections only accepted sketch profiles, so clicking the face of an existing body picked a sketch behind it (often the sketch that created the body). The loft then started on that sketch and had already narrowed by the time it reached the face.
  - Sections now accept faces and sketch profiles, in the order you pick them. A face is lofted from its whole outer boundary.
  - Loft no longer pre-fills the last two sketches.
- **The ✕ on a loft section field left a section behind.** Seeded sections were not in the highlight, so clearing the selection did not change the count. ✕ now clears every item in that field.
- Tests: 2 new self-tests (face section joins and still fills the face corners) and 3 UI checks (loft starts empty, both profiles can be picked, ✕ clears them). `--selftest` **81/81**, `--uitest` **41/41**.

### Added — 2026-10-14: origin selection, tangent chains, sketch modify tools, drag, DXF
- **Origin planes, axes and point in the view**:
  - Renderer property `originItems` (`O`, `X`, `Y`, `Z`, `XY`, `XZ`, `YZ`), with new pick kinds `originplane`, `originaxis` and `originpoint`.
  - Planes are drawn as translucent orange squares in the positive quadrants. They are 30 mm across in an empty model, or 30 % of the model size. Axes are red, green and blue; the origin is a point.
  - The planes appear automatically whenever a plane can be picked (Create Sketch, and plane inputs). Axes appear for axis inputs and the point for point inputs.
  - The selection filters treat them as `plane`, `axis` and `point`, so Revolve, Circular Pattern, Mirror and the construction tools accept them. Dialogs show the "XY", "X", ... presets as picked origin items.
- **Create Sketch picks the plane in the view**, like Fusion: the floating "XY / XZ / YZ" text box is gone. Click an origin plane, a construction plane or a planar face. With no bodies, the view fits the origin planes.
- **Browser Origin folder**: rows for O, X, Y, Z, XY, XZ and YZ, each with an eye to show it permanently and a click to select it. Clicking a plane row while Create Sketch is waiting starts the sketch on it.
- **Tangent chain selection** for every edge input:
  - `GeomUtils::tangentChain` walks edges through shared vertices while their tangents match (within 1°). `FissionDocument::tangentChain(edgeRef)` exposes it.
  - Fillet and Chamfer have a new "Tangent chain" checkbox, on by default. Picking an edge adds its whole smooth chain; deselecting any edge of a chain removes the chain, both in the view and with the chip ✕.
- **Sketch MODIFY tools** (toolbar group and new glyphs):
  - **Trim**: removes the part of a line, arc or circle between the nearest crossings around the click. A circle becomes an arc, and a curve with no crossings is deleted. Constraints on moved points are dropped.
  - **Extend**: lengthens the nearer end of a line or arc to the next curve it meets.
  - **Offset**: pick a curve, which selects its connected chain, then click the side. Circles, polygons and slots grow or shrink. Line/arc chains are offset and their corners rejoined, with coincident constraints at the joints. The distance is set in the sketch panel (default 2 mm, expressions allowed).
  - **Fillet**: pick two lines. They are trimmed back and joined by an arc, with coincident ×2, tangent ×2 and a driving radius dimension. The radius is set in the sketch panel (default 2 mm).
  - **Project**: pick a body edge or face; its edges are projected onto the sketch as fixed, purple, "(projected)" geometry. Lines and plane-parallel circles and arcs stay exact; other curves become splines.
- **Dragging sketch geometry** (no tool active):
  - Drag a point or end to move it. Drag a line to translate it; when translation is blocked, the end nearest the grab moves instead. Drag a circle or arc to change its radius while the centre holds.
  - The solver keeps every constraint while dragging, and each drag is one undo step. New solver constraint `dragto` and `FissionSketch::dragTo(targets)`.
- **Draggable dimension labels**: linear dimensions slide perpendicular to what they measure, radial labels swing around the centre, and angular arcs change radius. The placement is saved as the constraint's `offset` (`setDimensionOffset`).
- **Fixed entities**: an entity with `fixed: true` (all projected geometry) is held by the solver like a Fix constraint.
- **DXF export** of the active sketch: toolbar "DXF" button or "Export DXF..." in the sketch panel. It writes R12 ASCII in mm with layers `0` and `CONSTRUCTION`. Lines, circles and arcs stay exact; ellipses and splines become polylines; points are exported too.
- Code: the new kernel file `src/kernel/SketchEdit.cpp` holds trim, extend, offset, fillet, project, drag and DXF.

### Changed — 2026-10-14
- `Revolve` and `Circular Pattern` fall back to a picked origin axis when no other axis is selected.
- `UiTest` creates its first sketch by clicking the XY origin plane in the view.

### Tests — 2026-10-14
- 13 new self-tests:
  - Tangent chain: the slot loop has 4 edges; a sharp edge stays alone.
  - Trim: a line at a crossing; a circle becomes an arc.
  - Extend to the next line.
  - Offset: a rectangle loop inward (16 × 6, 2 profiles); a circle outward.
  - Sketch fillet R3.
  - Drag: a line end follows the cursor; projected geometry stays fixed.
  - Project a face as 4 fixed lines.
  - A dimension label offset is kept.
  - DXF export.
- 6 new UI checks:
  - The origin planes are shown for the plane pick, and the XY origin plane can be clicked.
  - The Trim tool works in the view.
  - Pressing a line end starts a drag, and dragging moves it.
  - The browser eye toggles the origin axis and point.
  - New screenshot: `20_trim_drag_origin`.

### Fixed — 2026-10-14: deselecting faces and edges in feature commands
- **Picked edges and faces could not be deselected** in Fillet, Chamfer, Shell and other edge/face inputs. The live preview replaced the body with the feature's result, so the picked edge or face no longer existed to click again.
  - New `FissionDocument::inputFeatureId`. While it is set, the last rebuild keeps the bodies as they were just before that feature (`showingInput()`, `inputShapes()`, `displayShape(i)`, `displayBodyCount()`).
  - `FeatureDialog` sets it while an `edges`, `faces` or `edge` field is active and clears it on other fields, OK, Cancel and close. `MainWindow.finishCommand` also clears it.
  - The renderer draws and picks those input bodies. The feature result is drawn as an orange outline on top (new non-pickable `PickTag::Preview`), and selected or hovered input edges are drawn above that outline.
  - Selection refs are now taken from the shapes the feature actually consumes. Before, they came from its own result.
- **Edges behind a body could be picked** whenever the filter excluded faces (any edge-only input). Body faces now always block edge picks, whatever the filter.

### Added — 2026-10-14
- Edge/face inputs list each picked item as a chip ("Edge 12 · Body1") with its own ✕ to remove only that item. The field's ✕ still clears all of them.
- Tests:
  - 2 new self-tests: the input view shows the pre-fillet box while the fillet result is kept, and clearing it restores the normal view.
  - 7 new UI checks: Shell shows its input body; the picked shell face stays clickable; clicking it again deselects it; cancelling restores the view; edges behind the body can't be picked; Fillet on one picked edge; clicking the edge again deselects it. New screenshots: `06b_shell_face_selected`, `06c_fillet_edge_selected`.

### Tests (after the edge/face chip round)
- `--selftest` **79/79**, `--uitest` **38/38**.

### Added — 2026-10-14: constraint solver and driving dimensions
- **`SketchSolver`** (`src/kernel/SketchSolver.h/.cpp`), a new 2D constraint solver:
  - It treats every entity field that holds a plain number as an unknown. Fields holding an expression, and every field of a fixed entity, are held constant.
  - Damped minimum-norm Gauss-Newton with a numeric Jacobian, so geometry moves as little as possible.
  - Degree-of-freedom analysis from the Jacobian's row space, giving the total DOF and per-entity fully-constrained flags.
  - Geometric constraints: coincident (point–point, point–line, point–circle), horizontal, vertical, parallel, perpendicular, collinear, tangent (line–circle, circle–circle inner/outer), equal, concentric, midpoint, symmetric, fix.
  - Dimensions: distance (aligned / horizontal / vertical), point–line distance, radius, diameter, angle. Values are expressions, so parameters drive sketches.
  - The sketch origin can be referenced as `{e: -1, p: "origin"}`.
- **`FissionSketch` constraint support**:
  - New `constraints` prop.
  - Every rebuild solves the constraints before the curves are built. A conflict fails the sketch with a clear error.
  - `addConstraint` and `setConstraintValue` solve immediately and write the solved geometry back. On failure they return `{ok:false, error}` and change nothing. `addConstraint` also records the current configuration: line side, inner/outer tangency, angle direction.
  - Also new: `removeConstraint`, `constraint`, `constraintCount`, `roleAt`.
  - UI data from the last rebuild: `handles()`, `annotations()`, `degreesOfFreedom()`, `entityFullyConstrained()`.
  - `addRectangleLines`: four lines with 4 coincident, 2 horizontal and 2 vertical constraints.
  - `addArcThroughPoints`: a 3-point arc stored as a counter-clockwise center arc, so it can be constrained.
- **Viewport constraint tools**:
  - A **CONSTRAINTS** ribbon group with Dimension (**D**) and 12 constraint tools, each with a new glyph.
  - Picks are read as the snapped point first, then the curve under the cursor. Each tool declares the item sequences it accepts.
  - Dimension tool: circle gives a diameter and arc gives a radius. Line then placement click gives the length. Line + line gives an angle; parallel lines give their distance. Point + line gives a distance. Two points then a placement click gives a horizontal, vertical or aligned distance.
  - An **inline value editor** opens on a new dimension, and on double-clicking any dimension label.
- **Sketch annotations**:
  - Dimension lines with arrows and value labels; labels for expression values are shown in blue with `fx:`.
  - Badges for geometric constraints, and dots for coincidence.
  - Click an annotation to select it; **Delete** removes it.
- **Auto-constraints while drawing**:
  - Line chains are coincident end to start, and closing a loop ties the last line to the first.
  - Exactly horizontal or vertical lines get the matching constraint.
  - A click that snapped to an existing point adds a coincident constraint, or a midpoint constraint if it snapped to a line's midpoint.
  - Typed Line length, Rectangle width/height and Circle diameter become **driving dimensions**, and the typed expression is kept.
- **Renderer**:
  - Sketch curves are coloured by state: blue when under-constrained, white when fully constrained, orange for construction geometry.
  - `sketchPoint()` now reports the entity and role of the point it snapped to.
- **Sketch panel**: shows the remaining degrees of freedom ("Fully constrained"), and a constraints list with editable dimension values and delete buttons.
- **Tests**:
  - 12 new self-tests: rectangle DOF, origin coincidence, full constraint, dimensions driving an extrude, a parameter in a dimension, conflict rejection, tangent side, constraint cleanup on delete, three-point arc conversion, angle dimension, annotation anchors.
  - 5 new UI checks: constrained rectangle, dimension tool and editor, editing a value, full constraint, perpendicular picked in the view.

### Changed — 2026-10-14
- The Rectangle tool now creates **four constrained lines** instead of one `rectangle` entity. Existing `rectangle` entities still load and build, but they can't be constrained.
- The 3-point Arc tool now creates a center-form `arc`. `arc3` entities still load.
- Deleting a sketch curve also deletes the constraints that reference it, and renumbers the rest.
- Esc closes the dimension editor first. Delete removes the selected constraint before it removes curves.
- `FissionSketch::buildEntity` uses numeric values directly, instead of re-parsing them as expressions.

### Fixed — 2026-10-14
- Constraint badges weren't drawn because anchor lists were flattened. In Qt, `QVariantList << QVariantList` concatenates the lists instead of appending one as an element.
- Fully-constrained curves and dimension lines were drawn in near-black, which is invisible on the dark viewport.
- A variable named `far` failed to compile on MSVC, because `far` is a macro in the Windows headers.

### Tests (after the constraint work)
- `--selftest` **64/64**, `--uitest` **25/25**.

---

## [0.2.5] — Phase 2.5: Fusion-style interaction

### Added
- **Floating command panels** (`FloatingPanel.qml`):
  - Feature dialogs and the Sketch panel float over the viewport and are dragged by their header.
  - The position is shared and remembered across commands (`MainWindow.panelPos`).
  - The panel height follows its content, and ✕ closes it.
- **Typed dimensions while sketching**:
  - A dimension box follows the cursor. Fields per tool: Line length; Rectangle and Ellipse width/height; Circle diameter; Polygon radius; Slot length and width.
  - Tab cycles between fields, and digits start typing.
  - A typed value locks that dimension while the cursor still sets the direction.
  - Enter commits, and expressions are accepted.
- **Feature drag handles**: an orange arrow for Extrude distance (at the profile centroid), Fillet radius and Chamfer distance (on the first edge, along the outward bisector).
  - Drags snap to half the grid step.
  - The value box next to the arrow is editable.
  - Kernel side: `FissionFeature::manipulator` and `setManipulator()`.
  - Renderer side: `projectPoint()` and `axisParameter()`.
  - Helpers: `GeomUtils::shapeCentroid`, `edgeOutwardBisector`.
- Single-key shortcuts:
  - E / F / S when no sketch is active.
  - L / R / C / X while sketching.
- New `close` glyph and `Theme.manipulator` colour.
- 2 manipulator self-tests (52 in total) and 4 new UI checks (20 in total).

### Fixed
- An externally edited `MainWindow.qml` / `Viewport.qml` used a non-existent API (`FissionRenderer is not a type`), and the whole UI failed to load.
  - The working QML was recovered from the resources of the `dist` exe.
  - The intended features were then re-implemented on the real API.
  - The broken drafts are kept in `docs/external_edits/`.
- Corrected `FISSION_360_PROJECT.md`, which claimed features that didn't exist (pybind11 scripting) and listed the wrong build directory, OCCT toolkit list and file names.

### Removed
- Unused `assets/resources.qrc`.

---

## [0.2.0] — Phase 2: Solid Modeling Toolkit

### Added
- **Parametric kernel**:
  - `FissionFeature` base class, with inputs in a `props` map. Numeric inputs are expressions in mm / degrees.
  - `RebuildContext`, which carries the bodies and resolves references.
  - Timeline with a rollback marker; suppress, reorder, delete.
  - Coalesced automatic rebuilds; failing features are flagged and the rebuild continues.
  - Undo/redo through checkpoints.
- **User parameters**: an expression evaluator (units, functions, cycle detection) and a parameters dialog with the model parameters table.
- **Features**:
  - Extrude: distance, symmetric, two sides, through all, taper, flip.
  - Revolve, Fillet, Shell, Loft, Sweep.
  - Chamfer: equal distance, two distances, distance + angle.
  - Mirror, rectangular and circular pattern (of bodies).
  - Construction plane (offset, angle, midplane, 3 points), construction axis and construction point.
  - Join / Cut / Intersect / New Body.
- **Sketch**:
  - Entities: line, rectangle, circle, 3-point arc, polygon, slot, ellipse, spline, point, and construction geometry.
  - Snapping to endpoints, centres and midpoints, plus an adaptive grid.
  - Profiles with holes (default selection by nesting depth).
  - Sketches on origin planes, construction planes or planar faces.
- **References**: planes, axes, edges and faces stored as reference maps. They are re-matched by index or nearest-point hint after topology changes.
- **UI wiring**:
  - Data-driven feature dialogs (`FeatureDialog`), with selection fields linked to the viewport pick filter.
  - Browser tree, timeline strip with context menu, Properties palette, marking menu.
  - `.f360` JSON save/load, and STEP / IGES / STL / OBJ export.
- **Tests**: 50 self-tests and a 16-step UI walkthrough with screenshots (`--uitest`).

---

## [0.1.0] — Phase 1: Foundation

### Added
- **CMake**:
  - Links the clean shared OCCT 7.6 install from the OrcaSlicer dependency build.
  - The three toolkits that install lacks — **TKFillet, TKOffset, TKFeat** — are compiled from OCCT source as OBJECT libraries.
  - OCCT DLLs are deployed and `windeployqt` runs after each build.
- **Qt 6.8.2 QML UI** that boots: command bar, ribbon, browser, viewport, timeline, status bar.
- **`BRepRenderer`**, a real OpenGL 3.3 viewport:
  - Tessellation cache, shaded rendering with edges, grid.
  - CPU picking of faces, edges, profiles, planes, sketch curves and axes.
  - Hover and selection highlight.
  - Z-up orbit camera, ViewCube, fit, home and ortho.
- **`--selftest`** (sketch → extrude) and **`--screenshot`** for launch verification.

### Fixed
- Link errors (TopOpeBRep*, BRepAlgoAPI, BRepCheck, …) caused by mixing a static, partially rebuilt OCCT tree with the shared one.
- QML errors that stopped the UI from loading.
