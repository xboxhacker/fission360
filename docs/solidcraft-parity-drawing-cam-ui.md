# SolidCraft parity — drawings, CAM, import/export, files, UI

Behaviour and failure modes only. No SolidCraft or truck source is copied here, and this note does not change Fission code.

Scan date: 2026-10-22 UTC. Repo: [tangency-labs/solidcraft](https://github.com/tangency-labs/solidcraft) (public issues). Fission tree: OCCT 7.6 + Qt/QML. **1.13.35** is the published Windows release (`Fission360-1.13.35-windows-x64.zip`). The drawing sheet and drawing-save path below are in that build.

## Method

`gh` against `tangency-labs/solidcraft`, authenticated for the search API, token not recorded.

- Public issue count at scan time: **2542**.
- Body/title keyword search (first page, best match) was too noisy to verdict one-by-one. Examples: `drawing` 105, `dimension` 113, `step` 513, `import` 262, `export` 224, `crash` 218, `undo` 120, `performance` 49, `dxf` 51, `stl` 62, `iges` 31, `g-code` 40, `pdf` 16, `"section view"` 16, `"parts list"` 5, `toolpath` 6, `cam` 18.
- The tables below are **title** searches (`in:title`) for drawing, section view / Section Analysis, DXF/PDF/DWG, IGES/STL/3MF/OBJ, STEP, import/export, manufacture/CAM/toolpath/g-code, undo/autosave/file dialog, dimension, and crash/freeze/hang/performance. Those queries returned 10, 14, 13, 22, 73, 95, 5, 6, 20, and 157 hits. STEP, import/export, and the crash query were fully listed when the total was ≤100; the crash query’s first 100 of 157 were classified.
- Truck-only kernel failures (`not supported yet`, Rust panics, `ZeroRange`, truck STEP **writer** cones/tori), wgpu/Vulkan startup, website, CI, account, localization, and agent-chat issues are **NOT-APPLICABLE** unless the title is a CAD failure Fission can share. Sketch-dimension titles are listed once and left to the sketch pass.
- Verdicts use only Fission code or `CHANGELOG.md` read for this note. **LIKELY-SAME** means that evidence shows the same user-visible failure or the same missing capability. **ALREADY-HANDLED** means the failure mode is closed in the code that was read. **POSSIBLY-SAME** means a related path exists but the SolidCraft recipe was not reproduced. **UNKNOWN** means the report has no public recipe that maps onto code that was read.

No patches.

## Drawings

Fission Phase 9 (`CHANGELOG.md` 1.9.0 / 1.10.0) has sheets, HLR views, dimensions, PDF/DXF/PNG/TIFF, and parts/revision/bend tables in `DrawingManager`. The 1.13.35 sheet (`DrawingWorkspace.qml`) is in the published Windows build. Front views use the HLR plane's X/Y, so a box keeps its height. The DRAWING ribbon is the command bar.

| Issue | Title | Verdict | Evidence |
|---|---|---|---|
| [#860](https://github.com/tangency-labs/solidcraft/issues/860) | [RFC] Drawings: community requirements and phased development | POSSIBLY-SAME | Umbrella for the W-items below. Fission has a drawing workspace (changelog 1.9.0). Open children that match real gaps are #3874, #3890, #4258. The RFC checklist itself was not copied. |
| [#2053](https://github.com/tangency-labs/solidcraft/issues/2053) | Drawing W01a: validated section geometry and frame contract (closed) | NOT-APPLICABLE | Truck section-frame contract. Fission does not use that representation. The user-visible section gap is #4258. |
| [#2054](https://github.com/tangency-labs/solidcraft/issues/2054) | Drawing W09a: length display formatting below the UI (closed) | NOT-APPLICABLE | Truck text-formatting share, not an OCCT drawing bug. |
| [#2105](https://github.com/tangency-labs/solidcraft/issues/2105) | Drawing W05a: exact hidden-line investigation | NOT-APPLICABLE | Analytic hidden-line study for the truck kernel. Fission hidden lines are OCCT `HLRBRep` (`DrawingProjector.cpp`, `project`). |
| [#2455](https://github.com/tangency-labs/solidcraft/issues/2455) | Drawing W06a: exact orthographic hidden lines for the analytic subset | NOT-APPLICABLE | Same truck analytic subset. Fission already toggles `showHiddenLines` on HLR output. |
| [#3874](https://github.com/tangency-labs/solidcraft/issues/3874) | Drawing W24–W26: bill of materials, parts lists, balloons | LIKELY-SAME | `addPartsList` emits one row per body, quantity 1, one material (`DrawingManager.cpp`). No balloon annotation exists in `DrawingWorkspace.qml`. The DRAWING ribbon can place the table; PDF `paintSheet` draws tables. |
| [#3875](https://github.com/tangency-labs/solidcraft/issues/3875) | Drawing W18: sheets, borders, title blocks, templates | POSSIBLY-SAME | Sheets and a title block exist (`DrawingDocument::addSheet`, PDF title block in `paintSheet`). The 1.13.35 sheet draws the part name, size, standard, and angle, and maps A0–A4 plus ANSI A–E (`DrawingWorkspace.qml` `mmW`/`mmH`; anything else becomes 420×297). No drawing-template files. |
| [#3890](https://github.com/tangency-labs/solidcraft/issues/3890) | Drawing W11, W12, W15: where drawings are saved | ADDRESSED in 1.13.35 | Save writes a `drawings` array on the live `.f360` (sheets, views, curves, annotations, tables, title block, standard, projection, sheet size, image paths). Version snapshots still store only the link id. Older files that have no `drawings` array still open. |
| [#4258](https://github.com/tangency-labs/solidcraft/issues/4258) | Section view is generally buggy | LIKELY-SAME | `DrawingProjector::projectSection` does not cut the solid. It projects the look direction and appends one centre line. `DrawingViewItem` then hatches the whole view box. The 1.13.35 sheet drag does pass `projectDirection` from the cut line. |
| [#4562](https://github.com/tangency-labs/solidcraft/issues/4562) | Section view requests | UNKNOWN | Title only. No public recipe was read. Overlaps #4258. |
| [#4326](https://github.com/tangency-labs/solidcraft/issues/4326) | Drawing a diameter line does not snap through the circle centre | NOT-APPLICABLE | Sketch line snap, not a drawing view. Leave it to the sketch pass. |
| [#4440](https://github.com/tangency-labs/solidcraft/issues/4440) | Flat Pattern view doesn’t draw perforations | NOT-APPLICABLE | Fission has no perforate feature. Related gap: flat DXF is outline edges plus bend lines only (`FissionSheetMetal.cpp` `exportFlatPatternDxf`). |
| [#1483](https://github.com/tangency-labs/solidcraft/issues/1483) | Flat pattern view can’t be panned; Design camera moves under it (closed) | NOT-APPLICABLE | Fission flat pattern is a body in the normal viewport (`Toolbar.qml` “unfolded beside the model”), not a camera-blocking overlay. |
| [#4491](https://github.com/tangency-labs/solidcraft/issues/4491) | Offscreen model render returns blank PNGs; viewport stops drawing | NOT-APPLICABLE | Their offscreen GPU path. Fission path-trace export meshes a private copy and catches OCCT failures (changelog 1.13.26). |

## CAM / manufacturing

Changelog unreleased line: Phase 12 native CAM toolpaths are still open. 1.13.28 added the manufacture catalog, setup, tool presets, simulate, and post. Post is explicitly a stock-box stub.

| Issue | Title | Verdict | Evidence |
|---|---|---|---|
| [#1153](https://github.com/tangency-labs/solidcraft/issues/1153) | [RFC] Manufacture/CAM: 2.5D milling, toolpaths and G-code posts | LIKELY-SAME | `FissionDocument::exportMfgGCode` writes a “stock-aware NC stub”: face zigzags, pocket insets, and drill points on the stock box, not on model faces. `simulateManufacture` estimates length from the stock box and says it is not collision-checked. Multi-axis ops append “(Multi-axis stub — planar contour only)”. |
| [#2304](https://github.com/tangency-labs/solidcraft/issues/2304) | Send STL to a slicer on a LAN server | LIKELY-SAME | Send to Slicer writes a temp STL and launches a local OrcaSlicer or Bambu Studio executable (changelog 1.13.14, `FissionApplication.cpp`). No LAN/server target was found. |
| [#1621](https://github.com/tangency-labs/solidcraft/issues/1621) | Research: CAM plugin (Machining & G-code) | NOT-APPLICABLE | WASM plugin host. The missing toolpaths are #1153. |
| [#1625](https://github.com/tangency-labs/solidcraft/issues/1625) | Research: DfAM plugin | NOT-APPLICABLE | Plugin research, not a Fission kernel bug. |
| [#2652](https://github.com/tangency-labs/solidcraft/issues/2652) | Split Manufacture megapr into a PR tree | NOT-APPLICABLE | Their pull-request plan. |

## Import / export and files

Readers: STEP and IGES via `STEPCAFControl_Reader` / `IGESCAFControl_Reader`, then `ShapeFix_Shape` (`FissionImport.cpp`). Empty results error with “no surfaces or solids”. STL and OBJ via `insertMesh` → `MeshUtils::readFile`. Writers: STEP `STEPControl_Writer`, IGES `IGESControl_Writer`, STL `StlAPI_Writer`, 3MF `FileExport::write3mf` (indexed vertices, `displaycolor`), OBJ `writeObj` (`FissionDocument.cpp`). DXF R12 (`AC1009`) is `FileExport::DxfWriter`: real LINE/ARC/CIRCLE, ellipses and splines as 64-point polylines. No DXF, 3MF, DWG, `.f3d`, or `.FCStd` reader. File ▸ Insert STEP/IGES calls `doc.importCAD` on the UI thread (`MainWindow.qml` `importCADFile`). McMaster uses `importCADAsync` (changelog 1.13.11). Changelog 1.13.12 still lists async File ▸ Insert STEP as later work. Booleans cancel after 20 s (1.13.12). Large `.f360` save is compact and chunked (1.13.6). Version history is a compressed `history` array inside the `.f360` (1.13.33).

| Issue | Title | Verdict | Evidence |
|---|---|---|---|
| [#67](https://github.com/tangency-labs/solidcraft/issues/67) | Sketch import DXF and SVG fail on macOS (closed) | LIKELY-SAME | No DXF or SVG importer in `src/`. Export exists (`FissionSketch::exportDxf`, face DXF, flat DXF, drawing DXF). macOS is also unshipped (changelog 1.13.17). |
| [#2052](https://github.com/tangency-labs/solidcraft/issues/2052) | dxf import issue | LIKELY-SAME | Same missing importer. The report text was not needed to see that import cannot succeed. |
| [#2300](https://github.com/tangency-labs/solidcraft/issues/2300) | Importing DXF failed | LIKELY-SAME | Same. |
| [#1089](https://github.com/tangency-labs/solidcraft/issues/1089) | Flat DXF writes ellipses, splines, and text as 0.001 mm lines and has no version (closed) | ALREADY-HANDLED | `DxfWriter::open` writes `$ACADVER` AC1009. Circles and arcs stay CIRCLE/ARC. Splines become polylines, not clouds of 0.001 mm lines (`FileExport.cpp`). |
| [#1284](https://github.com/tangency-labs/solidcraft/issues/1284) | Flat DXF: one layer, continuous bends, one body (closed) | LIKELY-SAME | `exportFlatPatternDxf` puts every top-face edge on layer `0` and bend lines on `BEND`. Every layer is linetype CONTINUOUS (`DxfWriter::open`). One flat feature is written. Holes are not a separate layer. |
| [#3431](https://github.com/tangency-labs/solidcraft/issues/3431) | Export Current View as DXF: unexpected result | UNKNOWN | Fission has no model-view DXF command. Exports are STEP/IGES/STL/OBJ/3MF, face or sketch DXF, flat DXF, or a drawing sheet (`Toolbar.qml`). |
| [#3708](https://github.com/tangency-labs/solidcraft/issues/3708) | Export Current View as DXF: operation failed (closed) | NOT-APPLICABLE | That command is not in Fission. |
| [#4314](https://github.com/tangency-labs/solidcraft/issues/4314) | Exporting dxf: question | UNKNOWN | Title only. |
| [#469](https://github.com/tangency-labs/solidcraft/issues/469) | Hostile-input coverage for SVG and DXF importers | NOT-APPLICABLE | Their test gap. |
| [#3324](https://github.com/tangency-labs/solidcraft/issues/3324) | Flat pattern and DXF ignore non-sheet features (closed) | NOT-APPLICABLE | Truck sheet-metal classification. |
| [#144](https://github.com/tangency-labs/solidcraft/issues/144) | 3MF vertices recopied; DXF INSERTs recurse (closed) | NOT-APPLICABLE | Their importer. Fission DXF writer does not emit INSERT. |
| [#2812](https://github.com/tangency-labs/solidcraft/issues/2812) | Agent chat: paste images and PDFs | NOT-APPLICABLE | Agent chat, not CAD export. |
| [#784](https://github.com/tangency-labs/solidcraft/issues/784) | STL export does not weld vertices; 3MF of the same body is closed (closed) | POSSIBLY-SAME | STL is `StlAPI_Writer` on a `BRepMesh_IncrementalMesh`. 3MF uses `MeshUtils::fromShape`, which welds (`MeshUtils.cpp`). No selftest compares open edges. |
| [#1666](https://github.com/tangency-labs/solidcraft/issues/1666) | Loft STL/3MF is two triangles per ruled face | NOT-APPLICABLE | Truck mesher. Fission meshes with OCCT deflection `max(0.005, size * 0.0005)` (`exportBodies`). |
| [#4451](https://github.com/tangency-labs/solidcraft/issues/4451) | OBJ export: no shared vertices, no colours, names lose spaces | LIKELY-SAME | `writeObj` emits `v`/`f` per face triangulation and restarts the index per face. No `usemtl`, no vertex colours. It also writes no `o` body names at all. 3MF of the same bodies does carry colour (`write3mf`). |
| [#3858](https://github.com/tangency-labs/solidcraft/issues/3858) | 3MF import: name Bambu bodies from model_settings.config | NOT-APPLICABLE | No 3MF reader. `insertMesh` is STL/OBJ only. The naming bug cannot occur. Missing 3MF import is a gap. |
| [#2089](https://github.com/tangency-labs/solidcraft/issues/2089) | 3MF import of multiple parts errors (closed) | NOT-APPLICABLE | Same: no 3MF import. |
| [#4130](https://github.com/tangency-labs/solidcraft/issues/4130) | Opening STL File error | UNKNOWN | Title only. Fission can read ASCII and binary STL (`MeshUtils::readStl`) but this file was not available. |
| [#4093](https://github.com/tangency-labs/solidcraft/issues/4093) | Guided sweep STL has open boundaries (thread coupon) | NOT-APPLICABLE | Truck thread mesh. |
| [#4553](https://github.com/tangency-labs/solidcraft/issues/4553) | FreeCAD `.FCStd` import is non-manifold | NOT-APPLICABLE | Fission does not open `.FCStd`. The broken-solid symptom is their cadmpeg path. Missing native FreeCAD import is a gap. |
| [#2346](https://github.com/tangency-labs/solidcraft/issues/2346) | Cannot import Fusion `.f3d` archives | NOT-APPLICABLE | Fission imports the bulk-exporter `.f360` JSON (changelog 1.10.x), not a native `.f3d` zip. |
| [#1708](https://github.com/tangency-labs/solidcraft/issues/1708) | F3D import: editable history and partial recovery | NOT-APPLICABLE | Same. Fusion history arrives only through that exporter’s timeline, not `.f3d`. |
| [#1033](https://github.com/tangency-labs/solidcraft/issues/1033) | Large STEP with many parts is slow and cancel does not free memory | LIKELY-SAME | `importCADFile` calls `doc.importCAD` on the UI thread. There is no cancel. McMaster is the path that uses `importCADAsync`. |
| [#3171](https://github.com/tangency-labs/solidcraft/issues/3171) | STEP open sticks on loading; cancel does not finish | LIKELY-SAME | Same synchronous insert. |
| [#4086](https://github.com/tangency-labs/solidcraft/issues/4086) | STEP-import file too large | POSSIBLY-SAME | No size cap in `FissionImportCAD::readFile`. A huge STEP can still exhaust memory; Fission does not emit their “file too large” string. The 1.13.1 OOM fix is for Fusion per-step snapshots, not a raw STEP insert. |
| [#1784](https://github.com/tangency-labs/solidcraft/issues/1784) | ~38 MB STEP import exhausts RAM | POSSIBLY-SAME | Same reader, no streaming cap. |
| [#1374](https://github.com/tangency-labs/solidcraft/issues/1374) | STEP import OOM while meshing a face | POSSIBLY-SAME | Import returns a shape; viewport tessellation is separate. Not reproduced. |
| [#3924](https://github.com/tangency-labs/solidcraft/issues/3924) | STEP AP242 tessellated parts left out with no warning | POSSIBLY-SAME | Reader is `STEPCAFControl_Reader` (OCCT 7.6) and walks solids, shells, and faces. A file that transfers nothing errors (“no surfaces or solids”). A mixed file can drop tessellation-only products with no specific warning. Not proven on an AP242 sample. |
| [#3422](https://github.com/tangency-labs/solidcraft/issues/3422) | Combine cut never finishes on a ~1900-face STEP | POSSIBLY-SAME | Join/Cut/Intersect cancel at 20 s (changelog 1.13.12) instead of running forever. The cut can still fail or time out. |
| [#4102](https://github.com/tangency-labs/solidcraft/issues/4102) | Booleans on a FreeCAD STEP are much slower than a native body | POSSIBLY-SAME | 1.13.12 records multi-second booleans and a skipped face-merge on B-spline STEP faces. The 15× figure is theirs. |
| [#533](https://github.com/tangency-labs/solidcraft/issues/533) | No linked insert; STEP insert drops sheet-metal history (closed) | POSSIBLY-SAME | `.f360` inserts can stay linked and update (changelog 1.13.12). A STEP/IGES insert is an `ImportCAD` body with no source features (`FissionImport.h`). |
| [#1386](https://github.com/tangency-labs/solidcraft/issues/1386) | Import always creates a new component (closed) | ALREADY-HANDLED | `importCADFile` inserts into the open document. |
| [#1265](https://github.com/tangency-labs/solidcraft/issues/1265) | STEP import: orientation and body names (closed) | ALREADY-HANDLED | XCAF names and colours are kept (`labelName`, `parts` in `FissionImport.cpp`). There is no Y-up/Z-up toggle (#3). |
| [#3221](https://github.com/tangency-labs/solidcraft/issues/3221) | Saving a design with a 256–512 MiB STEP import fails (closed) | ALREADY-HANDLED | Large save no longer pretty-prints JSON twice on the UI thread (changelog 1.13.6, `testLargeFileSave`). Their container size limit is a different format. |
| [#4275](https://github.com/tangency-labs/solidcraft/issues/4275) | Slow zoom on a 662-body STEP | POSSIBLY-SAME | Large-assembly navigation was not timed. Import itself is synchronous (#1033). |
| [#3892](https://github.com/tangency-labs/solidcraft/issues/3892) | Nested STEP assembly: browser hover is slow | POSSIBLY-SAME | Browser cost on deep XCAF trees was not measured. Names are stored per solid. |
| [#4554](https://github.com/tangency-labs/solidcraft/issues/4554) | History bar hover on a large STEP rigid group stalls | POSSIBLY-SAME | Fission has no rigid-group occurrence hover. Timeline hover on a large import was not measured. |
| [#3720](https://github.com/tangency-labs/solidcraft/issues/3720) | Viewport lag, 1163-body STEP | POSSIBLY-SAME | Same class as #4275. |
| [#3595](https://github.com/tangency-labs/solidcraft/issues/3595) | Orbit stutters with ~100 STEP bodies | POSSIBLY-SAME | Same class. |
| [#2814](https://github.com/tangency-labs/solidcraft/issues/2814) | Prusa Core One STEP takes about an hour | POSSIBLY-SAME | No cancel, UI thread (#1033). Duration not measured. |
| [#3108](https://github.com/tangency-labs/solidcraft/issues/3108) | STEP files take long to load | POSSIBLY-SAME | Same. |
| [#4186](https://github.com/tangency-labs/solidcraft/issues/4186) | Big step lag | POSSIBLY-SAME | Title only; same import path. |
| [#3085](https://github.com/tangency-labs/solidcraft/issues/3085) | Crash picking an edge to fillet after combine on an imported STEP | POSSIBLY-SAME | 1.13.12 turns OCCT access violations into feature errors (`OSD::SetSignal`). This pick crash was not reproduced. |
| [#4483](https://github.com/tangency-labs/solidcraft/issues/4483) | Extrude, combine, fillet: unexpected result from a STEP | UNKNOWN | No public geometry recipe in the title. |
| [#4552](https://github.com/tangency-labs/solidcraft/issues/4552) | Imported wheels have mesh gaps | UNKNOWN | User STEP not available. |
| [#1468](https://github.com/tangency-labs/solidcraft/issues/1468) | Strange seam on an imported STEP | UNKNOWN | No recipe read. |
| [#3730](https://github.com/tangency-labs/solidcraft/issues/3730) | STEP void volume added for `ORIENTED_CLOSED_SHELL` (closed) | UNKNOWN | OCCT 7.6 may or may not apply that shell flag the same way. Not tested. |
| [#2](https://github.com/tangency-labs/solidcraft/issues/2) | Imported STEP colours lost after the next feature (closed) | UNKNOWN | Colours are applied on insert. Whether the next rebuild keeps `FissionBody` colour was not traced. |
| [#3](https://github.com/tangency-labs/solidcraft/issues/3) | Y-up STEP files lie on their backs (closed) | NOT-APPLICABLE | OCCT XDE placement is used. There is no extra “lay it on Z” option in `readFile`. |
| [#3951](https://github.com/tangency-labs/solidcraft/issues/3951) | STEP spline vertex past the start panics (`ZeroRange`) | NOT-APPLICABLE | Rust panic in their curve kernel. |
| [#4598](https://github.com/tangency-labs/solidcraft/issues/4598) | STEP body silently empty when an edge doubles back | NOT-APPLICABLE | Truck regression of the same class. Fission either ShapeFixes or errors if nothing transferred. |
| [#1376](https://github.com/tangency-labs/solidcraft/issues/1376) | Web app: STEP open freezes the page (closed) | NOT-APPLICABLE | Website. The desktop analogue is #1033. |
| [#1452](https://github.com/tangency-labs/solidcraft/issues/1452) | Periodic cylinder mesh leaves the trim and volume changes | NOT-APPLICABLE | Truck display mesh. |
| [#3847](https://github.com/tangency-labs/solidcraft/issues/3847) | STEP threads lose their display plan and render cracked (closed) | NOT-APPLICABLE | Truck display plan. |
| [#1289](https://github.com/tangency-labs/solidcraft/issues/1289) | Imported STEP threads display with artifacts (closed) | NOT-APPLICABLE | Same display-plan class. |
| [#11](https://github.com/tangency-labs/solidcraft/issues/11), [#612](https://github.com/tangency-labs/solidcraft/issues/612), [#1181](https://github.com/tangency-labs/solidcraft/issues/1181), [#1198](https://github.com/tangency-labs/solidcraft/issues/1198), [#3394](https://github.com/tangency-labs/solidcraft/issues/3394), [#4005](https://github.com/tangency-labs/solidcraft/issues/4005), [#4006](https://github.com/tangency-labs/solidcraft/issues/4006), [#4007](https://github.com/tangency-labs/solidcraft/issues/4007), [#4438](https://github.com/tangency-labs/solidcraft/issues/4438) | Truck STEP **writer**: merged splits, oversized B-spline bounds, lost faces, oblique-revolve “bad cone”, horn torus volume drift, torus with no seam that OCCT 7.6 reads as volume 0 | NOT-APPLICABLE | Those writers are truck. Fission writes with `STEPControl_Writer` (`exportBodies`). #4438 is specifically their file being unreadable by OCCT 7.6, which is the kernel Fission already uses to write. |
| [#14](https://github.com/tangency-labs/solidcraft/issues/14), [#3358](https://github.com/tangency-labs/solidcraft/issues/3358), [#3806](https://github.com/tangency-labs/solidcraft/issues/3806), [#3845](https://github.com/tangency-labs/solidcraft/issues/3845), [#3885](https://github.com/tangency-labs/solidcraft/issues/3885), [#3912](https://github.com/tangency-labs/solidcraft/issues/3912), [#3928](https://github.com/tangency-labs/solidcraft/issues/3928) | Truck IGES trim assembly: dropped faces, reversed composite trim, arc read as a sphere, micrometre gaps, tiny knot span, separate solids as one open body | NOT-APPLICABLE | Truck IGES kernel. Fission uses `IGESCAFControl_Reader` plus `ShapeFix_Shape`. That does not prove these files heal; it is a different reader, and the panics do not apply. |

## UI, undo, section analysis, crashes

Section Analysis is one clip plane (`MainWindow.qml` `sectionRef` / `sectionOffset`, `BRepRenderer::setSectionEnabled`). The cut cap is always a hatched green (`kCapFS` in `BRepRenderer.cpp`). There is no hatch-off switch in `InspectPanel.qml`. Edit restores the saved plane and offset (`editSection`). OK refuses to save until a plane is picked (`commitSection`). Cancel closes the panel and `syncSections` shows a cut only when a saved analysis is visible. Hidden bodies are not meshed (`BRepRenderer.cpp` skips `!isVisible()`), and the pick depth grid skips triangles on the cut side.

Hide does not enter the undo stack. `FissionBody::setVisible` and `FissionFeature::setVisible` do not call `checkpoint`. Feature `visibilityChanged` only marks the document modified (`FissionDocument.cpp`). The browser eye sets `visible` directly (`BrowserPanel.qml`). `checkpoint` stores a full JSON snapshot, capped at 100.

| Issue | Title | Verdict | Evidence |
|---|---|---|---|
| [#2219](https://github.com/tangency-labs/solidcraft/issues/2219) | Undo does not undo hide | LIKELY-SAME | Visibility changes are not checkpointed. Undo restores the previous modeling snapshot and the body stays hidden. |
| [#2437](https://github.com/tangency-labs/solidcraft/issues/2437) | Section Analysis: quarter section, two planes, clear exit | LIKELY-SAME | One plane and one offset. No second plane. |
| [#4563](https://github.com/tangency-labs/solidcraft/issues/4563) | Section Analysis: hatch option, off for a plain cut | LIKELY-SAME | `kCapFS` always stripes the cap. The panel exposes offset and flip, not hatch. |
| [#2410](https://github.com/tangency-labs/solidcraft/issues/2410) | Section Analysis cannot be turned off; Cancel keeps the cut | ALREADY-HANDLED | `closeInspect` clears the draft and `syncSections` drops the cut when nothing saved is visible. OK without a plane shows a message and does not save. |
| [#3614](https://github.com/tangency-labs/solidcraft/issues/3614) | Dragging the section cut stalls on a large import (closed) | ALREADY-HANDLED | `setSectionOffset` calls `update()`, not a remesh. |
| [#3615](https://github.com/tangency-labs/solidcraft/issues/3615) | Edit Section Analysis reopens at 0 mm with no plane (closed) | ALREADY-HANDLED | `editSection` copies `plane`, `offset`, and `flip` from the saved analysis. |
| [#4138](https://github.com/tangency-labs/solidcraft/issues/4138) | Hidden faces still block picks under Section Analysis | ALREADY-HANDLED | Hidden bodies are not meshed, so they are not in the depth grid. Cut-side triangles are skipped while building that grid. |
| [#822](https://github.com/tangency-labs/solidcraft/issues/822) | Section analysis not in the browser (closed) | ALREADY-HANDLED | `commitSection` stores a browser analysis (`addAnalysis`). |
| [#3540](https://github.com/tangency-labs/solidcraft/issues/3540) | Extrude cut freezes the UI for minutes | ALREADY-HANDLED | Join/Cut/Intersect abort after 20 s and booleans are off the unbounded UI path (changelog 1.13.12). |
| [#3977](https://github.com/tangency-labs/solidcraft/issues/3977) | Section Analysis | UNKNOWN | Title only. |
| [#1611](https://github.com/tangency-labs/solidcraft/issues/1611) | Unexpected result on Section Analysis | UNKNOWN | No recipe read. |
| [#2117](https://github.com/tangency-labs/solidcraft/issues/2117) | SECTION ANALYSIS: unexpected result | UNKNOWN | No recipe read. |
| [#1229](https://github.com/tangency-labs/solidcraft/issues/1229) | Section analysis cuts into the origin planes (closed) | UNKNOWN | Not checked against origin-plane drawing. |
| [#3643](https://github.com/tangency-labs/solidcraft/issues/3643) | Menu bar, including Undo, dead in full screen | UNKNOWN | Full-screen menu code was not read. |
| [#3947](https://github.com/tangency-labs/solidcraft/issues/3947) | File dialogs start in the last-used folder (closed) | UNKNOWN | Save sets `currentFolder` from the design path (`MainWindow.qml`). A shared last folder for every dialog was not confirmed. |
| [#3737](https://github.com/tangency-labs/solidcraft/issues/3737) | Orbit and zoom freeze while edge highlighting runs | UNKNOWN | Highlight cost was not measured. |
| [#3848](https://github.com/tangency-labs/solidcraft/issues/3848) | Modeling locks up, sometimes crashes | UNKNOWN | No recipe. |
| [#4604](https://github.com/tangency-labs/solidcraft/issues/4604) | Measure on a large assembly freezes until Windows kills it | UNKNOWN | Measure was not timed. Their trigger is an agent `inspect_design`. |
| [#3269](https://github.com/tangency-labs/solidcraft/issues/3269) | macOS freezes opening sample designs | NOT-APPLICABLE | macOS build is not shipped (changelog 1.13.17). |
| [#2891](https://github.com/tangency-labs/solidcraft/issues/2891) | Sweep cut crashes the website | NOT-APPLICABLE | Website. |
| [#1210](https://github.com/tangency-labs/solidcraft/issues/1210), [#1638](https://github.com/tangency-labs/solidcraft/issues/1638), [#1789](https://github.com/tangency-labs/solidcraft/issues/1789), [#2363](https://github.com/tangency-labs/solidcraft/issues/2363), [#3024](https://github.com/tangency-labs/solidcraft/issues/3024), [#4110](https://github.com/tangency-labs/solidcraft/issues/4110), [#4394](https://github.com/tangency-labs/solidcraft/issues/4394), [#4605](https://github.com/tangency-labs/solidcraft/issues/4605) | wgpu / DX12 / Metal startup, RTSS Vulkan teardown, Mac finder crash | NOT-APPLICABLE | Fission draws with Qt OpenGL (`BRepRenderer`), not wgpu. |
| [#1330](https://github.com/tangency-labs/solidcraft/issues/1330), [#3989](https://github.com/tangency-labs/solidcraft/issues/3989) | UI test process crashes in the Vulkan loader (one closed) | NOT-APPLICABLE | Their egui/wgpu test harness. |
| [#321](https://github.com/tangency-labs/solidcraft/issues/321) | Home page shortcuts act on a hidden design (closed) | NOT-APPLICABLE | Their home page. |
| [#1192](https://github.com/tangency-labs/solidcraft/issues/1192) | macOS file dialog re-enters winit (closed) | NOT-APPLICABLE | winit. Fission uses Qt file dialogs. |
| [#2884](https://github.com/tangency-labs/solidcraft/issues/2884) | Autosave only covers the active tab (closed) | NOT-APPLICABLE | No multi-design tab strip in the save path that was read. Autosave stays in AppData (changelog 1.13.33). |

### Sketch dimensions (out of this pass)

These 20 title hits are sketch constraint/UI bugs, not drawing sheets. Verdict for this document: **NOT-APPLICABLE**. They belong in the sketch parity note.

#44, #130, #320, #754, #1361, #1803, #1936, #2190, #2366, #2724, #2916, #2925, #2961, #3167, #3722, #3786, #3812, #4064, #4352, #4385.

### Crash telemetry (not individually rowed)

`crash OR freeze OR hang OR performance in:title` returned **157**. The first 100 included **47** “ended without a clean exit” reports, plus wgpu validation and Rust `unwrap` / assertion / stack-overflow titles. Those are truck, wgpu, or empty telemetry. **NOT-APPLICABLE** as a class. They are not in the counts below. Named modeling crashes that describe a CAD failure are in the tables (#3085, #3540, #3848, #4604).

## Counts

Issues given a verdict in the tables and the sketch list, telemetry excluded:

| Verdict | Count |
|---|---|
| LIKELY-SAME | 14 |
| POSSIBLY-SAME | 19 |
| ALREADY-HANDLED | 11 |
| UNKNOWN | 18 |
| NOT-APPLICABLE | 81 |
| **Total** | **143** |

LIKELY-SAME (14): #67, #1033, #1153, #1284, #2052, #2219, #2300, #2304, #2437, #3171, #3874, #4258, #4451, #4563.

POSSIBLY-SAME (19): #533, #784, #860, #1374, #1784, #2814, #3085, #3108, #3422, #3595, #3720, #3875, #3892, #3924, #4086, #4102, #4186, #4275, #4554.

ALREADY-HANDLED (11): #822, #1089, #1265, #1386, #2410, #3221, #3540, #3614, #3615, #3890, #4138. #3890 shipped in 1.13.35.

UNKNOWN (18): #2, #1229, #1468, #1611, #2117, #3431, #3643, #3730, #3737, #3848, #3947, #3977, #4130, #4314, #4483, #4552, #4562, #4604.

NOT-APPLICABLE is the other 81 issues: 61 in the tables (including nine truck STEP-writer issues, seven truck IGES issues, and eight GPU-startup issues, each counted once) and the 20 sketch-dimension issues.

## Prioritized Fission gaps

1. **Drawings are saved in 1.13.35.** The live `.f360` has a `drawings` body; version snapshots still omit it. Older files with only a link id still open (#3890).
2. **Section views are not sections.** `projectSection` projects another orthographic view and draws a centre line. The sheet hatch covers the view box, not the cut face (#4258). The 1.13.35 drag passes `projectDirection`; it still does not cut the solid.
3. **Sheet dimensions: addressed in 1.13.39.** Dimensions are picked on projected model edges (line length, distance between parallel lines, angle, radius, diameter, plus Linear / Aligned / Angular / Radius / Diameter tools) and drawn the same on the sheet, in PDF / PNG / TIFF and in DXF. Since 1.13.40 they are stored on their view, follow it, and are re-measured when the view is projected again (red when an edge is gone).
4. **Parts list is a body dump, and there are no balloons** (#3874). Quantity is always 1. The DRAWING ribbon places the table; PDF table painting already exists.
5. **CAM post is not a toolpath** (#1153). G-code walks the stock box. Phase 12 in the changelog is still open. Simulate is an estimate, not a removal simulation.
6. **No DXF import** (#67, #2052, #2300). Flat-pattern DXF still puts holes on layer `0` and writes continuous bend lines (#1284). Ellipses and splines are polylines (`FileExport.cpp`), which is enough to avoid #1089.
7. **Insert STEP/IGES blocks the UI and cannot be cancelled** (#1033, #3171). `importCADAsync` exists and is used for McMaster, not for File ▸ Insert.
8. **Mesh exchange holes.** No 3MF import (#2089, #3858). OBJ export does not share vertices across faces and writes no colours or body names (#4451). STL uses `StlAPI_Writer` while 3MF welds (#784). No `.f3d` or `.FCStd` reader (#2346, #4553); Fusion comes in through the `.f360` exporter.
9. **Hide is invisible to undo** (#2219).
10. **Section Analysis is one plane and always hatched** (#2437, #4563). Edit, cancel, browser entry, and hidden-body picking look handled (#2410, #3615, #822, #4138).

`DrawingWorkspace.qml` maps A0–A4 and ANSI A–E. A size it does not know still opens as A3 paper (#3875). There are still no drawing-template files.
