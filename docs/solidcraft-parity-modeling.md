# SolidCraft solid-modeling parity (Fission)

Comparison of public [tangency-labs/solidcraft](https://github.com/tangency-labs/solidcraft) issues against Fission 360 (OpenCASCADE 7.6, Qt). Behaviour only. No SolidCraft or truck source was copied. No Fission C++ or QML was changed, and no repro was executed on a running Fission.

## Method and limits

- Issue list: GitHub `GET /repos/tangency-labs/solidcraft/issues?state=all&per_page=100`, paginated, on 2026-10-21. **2542** issues (numbers 1 through 4616), pull requests excluded. `docs/solidcraft-issue-parity.md` had recorded an earlier pull of 249; the public tracker is larger than that note.
- Search totals from `GET /search/issues` (title and body, open and closed), same day: shell 564, thicken 25, fillet 778, chamfer 362, extrude 966, loft 181, sweep 248, revolve 257, boolean 762, union 284, subtract 18, intersect 245, pattern 465, mirror 206, draft 491, taper 174, offset 575, split 419, combine 299, hole 762. `draft` and `offset` include non-modeling uses (document drafts, sketch offset). Search pages were not each opened; the full issue list was, and titles were filtered.
- Review set: title matches shell, thicken, fillet, chamfer, extrude, loft, sweep, revolve, boolean, union, subtract, intersect, pattern, mirror, draft, taper, offset, split, combine, or hole, and the issue is unlabeled or `area:kernel`. **990** issues. Another **63** title matches sit in sketch, UI, docs, IO, render, or infra and are not given per-issue rows.
- Face/edge title hits that did not also match those words were about 112. They are mostly selection, naming, or sketch-on-face, and are not in the 990.
- Bodies were read (first portion, code fences dropped) for the parity leftovers and the other issues cited as LIKELY-SAME or ALREADY-HANDLED. The long `FEATURE: operation failed` / `unexpected result` / crash-while-running set was classified from the title.
- **LIKELY-SAME** is used only where Fission source, `SelfTest.cpp`, or `CHANGELOG.md` shows the same missing behaviour. It does not mean that SolidCraft recipe was run here.
- Truck-only failures are **NOT-APPLICABLE**: Rust panics, `cargo`/`wasm`, `tool_nudge`, cavity ownership, lumps limits, planar-only exact fallback, implausible-volume checks, and "not supported yet" predicates this kernel does not implement. A user-visible OCCT risk that remains is marked POSSIBLY-SAME instead.
- Fission is not truck. Issues that only exist because of the Rust kernel were not treated as Fission bugs.

## Counts

| Verdict | Count |
|---|---:|
| ALREADY-HANDLED | 22 |
| LIKELY-SAME | 48 |
| NOT-APPLICABLE | 165 |
| POSSIBLY-SAME | 130 |
| UNKNOWN | 625 |
| **Reviewed** | **990** |

## Prioritized Fission gaps

Ordered by how often a modeler hits them. No patches.

1. **Shell of saddles, sweeps, and rounds smaller than the wall** still has no repair (`FissionShell.cpp`). Planar boxes, hip roofs, the 45 degree frustum, and the taper-plus-fillet box (#3573, CHANGELOG 1.13.35) are handled. Curved bodies try arc joins first, tiny faces are rejected, and the punch fallback only grows planar openings. Issues: #142, #1494, #566, #319.
2. **Fillets and chamfers can keep a bad solid, and edge identity can jump.** An oversized fillet that does not leave a smaller finite solid is rejected (CHANGELOG 1.13.35, #3561). Other fillet results are still stored when `IsDone()` is true, without a full `BRepCheck` (#3435, #1080, #1390, #1293, #1574, #2719). `GeomUtils.cpp` tangent chain includes seams (#3370, #775). `FissionFeature.cpp` `matchSubShape` snaps to the nearest edge with no maximum distance when topology changes (#4341). There is no setback or mid-point radius (#9, #52) and no fit-to-geometry option (#293).
3. **Booleans.** Extrude, hole, sweep, revolve, and loft booleans cancel at 20s (`FissionFeature.cpp` `applyOperation`). **Combine does not** (`FissionSolidTools.cpp`), so #1107, #2992, and #3455 can hang. Empty targets edit every solid whose bounding box meets the tool, including hidden bodies (#2464, #4224, #1988, #4572).
4. **Loft.** Inner wires are discarded (`FissionLoft.cpp` `OuterWire`), so a ring section becomes a solid (#1184, #3111). A rail that only meets the first and last sections now builds (CHANGELOG 1.13.35, selftest "loft: rail guide", volume 7000). A pipe is kept only when the rail lies on the solid.
5. **Extrude extents and taper.** No To-Object, up-to-face, or start offset (#3635, #3729, #928, #2312, #2826). The only taper path is `DraftAngle` on side faces, and only for a one-sided distance. Spline and text sides fail that add, and a long text taper has no time limit (#831, #4431). Line-plus-arc (#546) and tangent circles (#1225) were not confirmed.
6. **Sweep, revolve, loft, fillet, chamfer, shell, and draft `Build()` calls have no time limit.** The 20s cap is only on the boolean afterwards. #3897 (sweep join), #4606 (revolve), #2570 (loft freeze).
7. **Round-face offset.** If `BRepOffset_MakeOffset` fails, a non-planar face is a hard error (`FissionSolidTools.cpp`, #2939). A planar face beside a fillet does not drag the fillet (#3468).
8. **Missing solid tools that these issues ask for:** thin extrude (#721), surface extrude/revolve/sweep/loft and thicken (#757, #765, #771, #4358), split by sketch curves (#1552), rectangular-pattern instance skip (#3247).

Already covered for the recipes in `testSolidCraftParity()`, and passing on the 1.13.35 local build (394/0): #3541 loft pinch, #3578 combine join, #3526 two bodies kept (Fission does not make one multi-lump compound; the overlapping pair intersects to 1500), #3528 revolve-seam cut, #3573 taper+fillet shell, #3561 oversized fillet rejected, #3587 oversized chamfer, #3563 join adds only the sticking-out pad (8500) and through-all removes the cylinder it hits (8217). Closed hollow shell (#4009) and "a spline anywhere blocks offset face" (#745) are handled in code. Those are not in the fix list.

## Judged issues

One line each. Evidence is a Fission file or `n/a` when the issue does not apply. "Not run" means the SolidCraft steps were not executed here.

### LIKELY-SAME (48)

| Issue | Title | Evidence | Why |
|---:|---|---|---|
| 9 | Fillets: setback corners and variable radius with mid points | FissionFillet.h | Only a linear start/end radius; no setback corner and no mid-edge radii. |
| 52 | Fillet corners: 6-sided setback patch for three unequal radii | FissionFillet.cpp | One MakeFillet corner; no 6-sided setback patch. |
| 142 | Shell: saddle corners and four-face corners touching curved faces are still refused | FissionShell.cpp | No saddle-corner case; curved bodies try arc joins first; tiny faces are rejected; punch growth is planar-only. |
| 293 | Fillet: option to fit the radius instead of refusing (reduce, later taper) | FissionFillet.cpp | A radius that does not fit returns an error; nothing reduces it to fit. |
| 319 | Shell fails on a disc whose circular edge is rounded smaller than the thickness | FissionShell.cpp | A round smaller than the wall has no dedicated case; a failed offset fails the feature. |
| 566 | Shell refuses swept bars and sheet metal parts (a corner whose faces don't meet the same way after the move) | FissionShell.cpp | A swept bar is a curved shell; failure ends in "walls could not be offset" with no slanted-corner repair. |
| 721 | Extrude: thin extrude (walls of a given thickness along open or closed sketch curves) | FissionExtrude.h | No thin-wall extrude; extents are distance, symmetric, twoSides, throughAll. |
| 757 | Surface Extrude, Revolve, Sweep and Loft: surface bodies from open and closed curves | FissionFeature.cpp | resolveProfiles requires closed profiles, so open curves cannot make surface bodies. |
| 765 | Thicken every surface kind: cones, spheres, tori, B-splines and sharp-edged surface bodies | FissionSolidTools.cpp | Offset copies a shape; it does not thicken an open surface into a solid wall. |
| 771 | Sweep guide rails, two-rail sweep, orientation, twist and scale | FissionSweep.cpp | Twist and a fixed or corrected frame exist. Guide rails and scale do not. |
| 775 | Fillet of a shell's inner corners follows the pick point onto a smooth seam after the wall gets thinner than the outer round | GeomUtils.cpp | Same G1 walk can follow a seam onto a face that should not be rounded. |
| 831 | Tapered extrude: text/spline profiles (misleading "mixing lines and arcs" error) and arcs meeting lines at a corner | FissionExtrude.cpp | Spline and text sides are not draftable analytic faces; taper fails the feature. |
| 928 | Extrude To Object onto a body in a moved occurrence ignores the placement (stops at a flat plane through the point) | FissionExtrude.h | No To-Object extent, so placement of a target body is never consulted. |
| 1080 | Fillets and chamfers too large for their neighbours are accepted instead of refused | FissionFillet.cpp | Same: an oversized blend is kept when OCCT reports done. |
| 1107 | solid.combine of hollow hub + 9 lofted blades fails: 'cavity ownership is ambiguous' (1-2 tools work) | FissionSolidTools.cpp | Combine has no time limit. The "cavity ownership" wording is truck-only; the hang is not. |
| 1184 | A loft section that is a ring (a profile with a hole) builds a solid, with no error | FissionLoft.cpp | Every section is reduced to OuterWire, so a ring becomes a solid disc. |
| 1293 | Chamfer on the hinge knuckle's cap circle adds material at x = 40 | FissionFillet.cpp | Same missing volume guard on MakeChamfer. |
| 1390 | Chamfer on a sphere cut's rim grows the part (robustness seed 57) | FissionFillet.cpp | Chamfer result is kept on IsDone() with no check that material was only removed. |
| 1494 | Shell refuses a saddle corner on a cylinder seam (robustness seed 6) | FissionShell.cpp | Same curved/saddle shell path; a cylinder seam saddle is not a special case. |
| 1552 | Support Sketch Entities as Splitting Tools in Split Body | FissionSolidTools.h | Split Body takes a plane or a tool body, not sketch curves. |
| 1574 | Chamfering the edge of a hole that touches the bottom face adds material below the part | FissionFillet.cpp | A chamfer that adds material under the part is not rejected. |
| 1988 | Modeling: In the loft operation, hidden bodies are merged. | FissionLoft.cpp | Loft join calls applyOperation with the feature targets, which default to every touching body. |
| 2312 | Extrude: a taper cannot be combined with a start offset | FissionExtrude.cpp | Taper is refused unless the extent is a one-sided distance, so it cannot combine with a start offset. |
| 2464 | Extrude Join/Cut changes hidden bodies too, and the dialog can't pick its target bodies | FissionFeature.cpp | Empty targets boolean every solid whose box meets the tool, hidden or not. |
| 2531 | SHELL: a face picked by point opens a second face after a parameter edit (silent, see #23) | FissionShell.cpp | After unify, the opening is the nearest face centroid within 1 mm, so a second face can open. |
| 2570 | Modeling: Lofting between some shapes causes the application to freeze. | FissionLoft.cpp | ThruSections.Build has no time limit. |
| 2719 | Chamfer as wide as the round it runs past makes a sphere patch whose edges are off its surface (bad B-rep in STEP) | FissionFillet.cpp | No BRepCheck after chamfer, so a done-but-bad solid can be stored. |
| 2826 | Cut up to a face leaves the hole closed by a zero-thickness skin | FissionExtrude.h | There is no up-to-face extent. Distance, two sides, symmetric, and through-all only. |
| 2939 | I cannot offset round face | FissionSolidTools.cpp | If the solid offset fails, a non-planar face hits "Offset Face failed on a curved face". |
| 2992 | solid.combine: join of disk and 36 patterned twisted loft blades blocks for minutes (0.16.1) | FissionSolidTools.cpp | A many-tool combine can run without the boolean time limit. |
| 3111 | LOFT: Loft does not support profiles with inner loops (holes) | FissionLoft.cpp | Inner loops are dropped; the feature still reports success. |
| 3247 | [Feature Request] Support Skipping Individual Instances in Rectangular Patterns | FissionTransform.cpp | Rectangular pattern emits every index; there is no skip list. |
| 3370 | Fillet's Tangent Chain picks up smooth seam edges, and unticking it keeps them | GeomUtils.cpp | Tangent chain keeps any edge within 1 degree, including smooth seams. |
| 3435 | Fillet larger than the ring it rounds is accepted, and the body gets bigger | FissionFillet.cpp | IsDone() keeps the solid with no volume or BRepCheck test, so a fillet can grow the part. |
| 3455 | kernel-perf: solid.combine of many separate bodies grows steeply (64 pegs take 36 s) | FissionSolidTools.cpp | Same unbounded Combine build. |
| 3506 | Modeling: Split body cannot shell both resulting bodies on mating faces | FissionShell.cpp | One shell feature requires every opening on the same body and does not look at visibility. |
| 3635 | Extrude, Hole, Loft Features should have Start / End behavior like Fusion Extrude command | FissionExtrude.h | No start/end to a face, plane, or offset; only distance, two sides, symmetric, through all. |
| 3729 | Extrude `to` a point on an edge or corner of the target face ends in the wrong place, with no error | FissionExtrude.h | There is no "to point / to face" extent to pick the wrong face. |
| 3897 | Sweep of a ring profile along a short arc with Join freezes at 'Computing…' (pipe segment) | FissionSweep.cpp | MakePipe.Build has no time limit; the 20s cap applies only to the later boolean. |
| 4056 | Loft rails refuse a straight-line section drawn in a rail's plane | FissionLoft.cpp | Rail lofts use MakePipeShell; CHANGELOG still records the rail selftest as failing. |
| 4224 | Extrude tool does boolean operations on hidden bodys - not right | FissionFeature.cpp | Same empty-target rule; visibility is not read. |
| 4341 | Modeling: Unexpected behavior — Chamfer and Fillet lose their references and jump to unrelated edges | FissionFeature.cpp | When the edge count changes, the nearest midpoint wins with no distance limit. |
| 4358 | Modeling: unexpected result (Can't create Surface Loft between two profiles/sketches) | FissionLoft.cpp | Loft builds a solid from outer wires; there is no surface loft. |
| 4431 | Tapered text cut or join into a body is 40-180x slower than without taper | FissionExtrude.cpp | DraftAngle on many glyph faces has no time limit. |
| 4484 | Thicken (Engrosar) feature generates corrupted/invalid geometry on complex surface bodies | FissionSolidTools.cpp | No surface-thicken feature; offset failure is a hard error. |
| 4572 | EXTRUDE: Join operation targets the wrong Body | FissionFeature.cpp | Join with no explicit target uses bounding boxes. |
| 4579 | Serious concerns with SurfaceLoft and ruled surface | FissionLoft.cpp | Same: ruled surface loft is not a feature. |
| 4606 | REVOLVE:  never finishes | FissionRevolve.cpp | MakeRevol.Build has no time limit. |

### ALREADY-HANDLED (22)

| Issue | Title | Evidence | Why |
|---:|---|---|---|
| 950 |Hole drills the wrong way on faces of parts on the XY plane (and old designs keep it)| FissionHole.cpp | A face hole drills along the picked face normal, reversed into the body. |
| 4234 |loft from model surface - FR| FissionLoft.cpp | Sections accept solid faces as well as sketches. CHANGELOG records that fix. |
| 745 | Offset Face refuses a planar face when the body has a freeform face anywhere, even far from the moved face | FissionSolidTools.cpp | A failed solid offset falls back to a prism on planar faces, so a distant spline does not veto them. |
| 758 | surface.offset: offset faces into a new surface body (0 copies them) | FissionSolidTools.cpp | Offset of faces or bodies adds new bodies via MakeOffsetShape. |
| 1257 | Smart Extrude: direction from the clicked side, Cut into a body, and no flipping to Join mid-drag | CHANGELOG.md | Dragging the extrude arrow into the body switches Join to Cut. |
| 2440 | Tapped Hole: select metric thread sizes and derive the tap drill diameter | FissionHole.cpp | A tapped standard size uses the tap drill; selftest covers M6. |
| 2672 | Chamfer: one pick stops following the tangent chain once the part is over ~200 mm across (fillet unaffected) | GeomUtils.cpp | Tangent chain is a 1 degree walk with no part-size cutoff. |
| 2755 | Pattern and Mirror copies of a primitive cut or join every body they meet, ignoring the seed's targets | FissionTransform.cpp | Pattern and mirror booleans target the source body index, not every body they meet. |
| 2890 | cant move hole freely like move object, during add hole | FissionHole.cpp | A face hole sets a move manipulator on the face. |
| 3300 | Assymetric Chamfer | FissionChamfer.h | chamferType equal, twoDistances, and distanceAngle are implemented. |
| 3526 | solid.combine intersect silently returns empty result when the target body has several separate lumps | SelfTest.cpp | Distant join keeps two bodies. Intersect of the overlapping pair is 1500. Fission does not store both lumps in one compound. |
| 3528 | Boolean cut near the seam of a revolved body removes too much volume without warning | SelfTest.cpp | A tube cut across the revolve seam must keep most of the volume. The reported dome is a harder case. |
| 3540 | Extrude cut with coincident cylindrical face takes minutes and freezes the UI (0.16.2) | FissionFeature.cpp | Extrude cut goes through applyOperation, which cancels a boolean after 20s. |
| 3541 | LOFT: unexpected result | SelfTest.cpp | testSolidCraftParity checks a ruled circle frustum volume and full base diameter. |
| 3561 | FILLET: operation failed | SelfTest.cpp | An 8 mm fillet on a 10 mm cube is rejected and the 1000 mm³ box stays (1.13.35). |
| 3563 | EXTRUDE: operation failed | SelfTest.cpp | Join adds only the sticking-out 5 mm (8500). Through-all removes the cylinder it hits (8217). To-object is still missing (see 3635). |
| 3573 | Shell of a tapered, filleted box: the inner corner fillets' edges lie 0.02 mm off their surface (STEP readers reject the faces) | SelfTest.cpp | Taper plus fillet plus shell passes BRepCheck and loses volume (about 53070 down to 12839). CHANGELOG 1.13.35. |
| 3578 | COMBINE: operation failed | SelfTest.cpp | Overlapping boxes fuse to 1500 mm3. The public report has no sizes. |
| 3587 | CHAMFER: operation failed | SelfTest.cpp | Chamfer taller than the wall must fail or stay valid. |
| 3781 | Target Bodies selection for Boolean operations (Extrude, Sweep, Revolve) | FissionExtrude.h | Kernel targets exist. Whether the dialog exposes them was not checked. |
| 4009 | Shell with no removed face (closed hollow body with an internal void) is not possible: "select 1...1000 faces" | FissionShell.cpp | An empty face list hollows the solid by an offset cut. |
| 4435 | CIRCULAR PATTERN: Quantity parameter not saved | FissionTransform.h | Circular pattern count is an expression key and is stored with the feature. |

### POSSIBLY-SAME (130)

| Issue | Title | Evidence | Why |
|---:|---|---|---|
| 368 | One extrude cutting many circles from one sketch still scales super-linearly: 128 circles take 60 s | FissionFeature.cpp | One multi-profile cut is a boolean capped at 20 s, so it should not run for 60 s. It can still be slow. Not run. |
| 2673 | Pill sketch revolve | FissionRevolve.cpp | A stadium profile can be revolved. This report has no sizes and was not run. |
| 3630 | Modeling: Extruding a rounded sketch (with fillets) leads to an invisible solid -works with non-rounded sketches | FissionExtrude.cpp | Sketch fillets are closed profiles and should extrude. An invisible result was not reproduced. |
| 546 |solid.extrude with taper refuses profiles of lines and arcs: "not supported yet: tapered profiles mixing lines and arcs"| FissionExtrude.cpp | Taper is DraftAngle on each side face and aborts if Add fails. A line-plus-arc side was not run, so this is not a confirmed failure. |
| 556 |Fillet or chamfer of a single edge fails where its ends meet curved, slanted or several faces| FissionFillet.cpp | OCCT may fillet an edge that ends on a curved face. The truck "ends must be planar" rule is not in this kernel. Not run. |
| 1225 |Modeling: no possible to add taper angle for extrude| FissionExtrude.cpp | Taper exists for a one-sided distance extrude. A tangent pair of circles was not run. |
| 11 | STEP export merges faces the user split on purpose | GeomUtils.cpp | unify merges coplanar faces after booleans, which can erase a split the user wanted. |
| 12 | Measure and Offset Face treat split face pieces as one face | GeomUtils.cpp | Same merge can make split pieces one face before a later offset. |
| 30 | Profiles picked in the Extrude dialog are stored by index: a face sketch renumbers them on recompute (cut becomes a new body) | FissionFeature.cpp | Stale profile indices are replaced with the sketch default profiles, which can be the wrong region. |
| 58 | Saddle fillet between two cylinders isn't an exact rolling-ball surface (~0.05 mm) | FissionFillet.cpp | OCCT fillets are not an exact rolling-ball surface. The 0.05 mm figure was not measured here. |
| 68 | Cannot extrude top of chamfer. | FissionExtrude.cpp | Only flat faces extrude. A chamfer top that is not planar is refused. |
| 69 | Cut extrude of a smaller circle inside an earlier pocket restores the pocket (wrong volume, no error) | FissionFeature.cpp | A cut that should deepen a pocket. Not run. |
| 114 | Boolean Union fails when extruding overlapping cylindrical faces | FissionFeature.cpp | Coincident cylindrical faces are a known OCCT fuse hazard. The "not supported yet" string is truck-only. |
| 141 | Boss ring on a 0.5° fall around a drain: the boolean returns no result | FissionFeature.cpp | A 0.5 degree conical boss is an ordinary fuse. Not run. |
| 261 | Rectangular pattern of a hole cut scales super-linearly: 128 holes take 30 s, 512 holes do not finish | FissionTransform.cpp | Same cap. A 512-hole pattern will not run forever; it can still be too slow. |
| 307 | Tangent hole contacts split faces inconsistently (3 of 4 symmetric contacts) | src/kernel | Fission has this operation. The report was not reproduced. |
| 324 | Fillet over hole feature performs wrong behaviour | FissionFillet.cpp | No special case for a fillet crossing a hole; OCCT may contour it or fail. |
| 334 | Fillet/chamfer refuses the top outline after rounding the concave edges of two cylinders | src/kernel | Fission has this operation. The report was not reproduced. |
| 342 | Kernel: shell with a round equal to the thickness is not refused on Windows (shell_open_beside_a_round_as_large_as_the_wall_is_refused fails) | src/kernel | Fission has this operation. The report was not reproduced. |
| 390 | solid.draft on a flange end face fails: "faces next to the drafted walls must be square to the pull direction" | FissionSolidTools.cpp | DraftAngle fails if a face will not take the pull. There is no "must be square" pre-check. |
| 391 | Text deboss on the flat flange of a filleted + shelled body fails (boolean Cut: no result) | FissionFeature.cpp | Text deboss is an extrude cut. No text selftest on a shelled fillet. |
| 420 | Two intersecting cylinders cause sketch and shell operations to fail | FissionShell.cpp | Two cylinders make a curved shell. Not run. |
| 443 | Loft with operation intersect leaves the body unchanged when the cone meets the wall 1 mm below the top face (no error) | src/kernel | Fission has this operation. The report was not reproduced. |
| 450 | Revolve: a spherical shell sketched off the origin is built broken (volume +33 %) | FissionRevolve.cpp | MakeRevol has no volume check against an off-origin spherical profile. |
| 485 | Fillet fails on outer perimeter edges, including partial selections | src/kernel | Fission has this operation. The report was not reproduced. |
| 487 | Revolving adjacent profiles on the axis makes zero-area faces; booleans with the body fail | FissionRevolve.cpp | Adjacent profiles on the axis can make degenerate faces. Not run. |
| 506 | Profiles selected together leave the merged outline split where shared edges ended; booleans fail on the extra vertices | src/kernel | Fission has this operation. The report was not reproduced. |
| 545 | Ring cut leaving a thin floor below a recess splits a connected part into two bodies and loses volume | src/kernel | Fission has this operation. The report was not reproduced. |
| 547 | solid.draft on a cylinder's side face fails: "a corner whose faces don't meet the same way after the draft" | FissionSolidTools.cpp | Drafting a cylinder side is attempted and fails if Add does not stick. |
| 559 | A through-all Hole fails where it runs along a wall or beside curved faces (boolean Cut: no result / implausible volume) | FissionHole.cpp | Through-all is a long cutter (model size times 4) with no implausible-volume test. |
| 641 | Even then HOLE depth changed from negative to positive the error "negative" persists | src/kernel | Fission has this operation. The report was not reproduced. |
| 672 | The hole tool gives strange solution in certain view orientations | src/kernel | Fission has this operation. The report was not reproduced. |
| 703 | Extrude from a face is refused where another body's face touches it flush ("no body face"), with no way to pick the body | FissionFeature.cpp | Face profiles require a plane. Flush contact with another body is not a special case. |
| 707 | A through-all hole on the ribbed lid's side wall cuts or fails from run to run | src/kernel | Fission has this operation. The report was not reproduced. |
| 769 | Fillet between separate surface bodies (trim and stitch the sources) | src/kernel | Fission has this operation. The report was not reproduced. |
| 783 | Fillet and chamfer accept edge picks any distance from an edge and silently modify the nearest edge (15 mm / 30 mm off, or inside the solid) | src/kernel | Fission has this operation. The report was not reproduced. |
| 793 | A coil can't be split (or cut) by a plane across its axis: "boolean Intersect: no result" | FissionSolidTools.cpp | Split by a plane exists. A coil/helix solid was not run. |
| 814 | Chamfer of a hex hole's vertical corners cuts a notch at each corner instead of one bevel | FissionFillet.cpp | Hex-hole vertical corners. Not run. |
| 818 | A blind cut splits every vertical edge at the cut depth, and the fillet corners render black | GeomUtils.cpp | A blind cut can leave edges at the depth; unify merges only coplanar faces. Black fillet corners were not checked. |
| 823 | EXTRUDE to CUT object failed | src/kernel | Fission has this operation. The report was not reproduced. |
| 827 | Cylinder union fails on filleted laptop lid / hinge mounting leaf (MCP reproduction) | src/kernel | Fission has this operation. The report was not reproduced. |
| 851 | Fillets and chamfers fail where edges meet at corners on a curved wall or a knife-edge tip | FissionFillet.cpp | Corners on a curved wall. Not run. |
| 873 | Error when attempting to extrude an object next to a screw | src/kernel | Fission has this operation. The report was not reproduced. |
| 881 | Loft between two polygon sections with the same vertex count pairs vertices wrongly (twisted body, volume 27% low) | FissionLoft.cpp | CheckCompatibility is on; it does not guarantee vertex pairing. |
| 897 | Shell refuses robustness seed 14 since the 13 October kernel merges | src/kernel | Fission has this operation. The report was not reproduced. |
| 968 | Cutting one lofted tube from another fails for some section counts | src/kernel | Fission has this operation. The report was not reproduced. |
| 1023 | HOLE creation hang the UI | FissionHole.cpp | The hole boolean is capped at 20s. Building a modeled thread is not. |
| 1038 | A small hole on the Pi case takes 7.5 s, meshing its intersection edges | src/kernel | Fission has this operation. The report was not reproduced. |
| 1093 | Circular pattern of a lofted join takes ~31 min (9 copies) with 8 boolean tolerance retries | FissionFeature.cpp | Each pattern join is cancelled at 20s, so a 31 minute run should not happen. It can still be slow or fail. |
| 1126 | Modeling: unexpected result - increasing a draft angle doesnt cut off properly | FissionSolidTools.cpp | Draft changes volume in selftest. "Cuts off" at a larger angle was not checked. |
| 1140 | Cutting a hollow lofted tube in half fails with "no result": a vertex lands a hair inside an edge end | FissionFeature.cpp | Cutting a hollow loft in half. Not run. |
| 1183 | Tapered hexagon pockets fail on a plate with 10 mm corner fillets (8 mm works) | FissionExtrude.cpp | Taper plus an existing fillet is a draft of side faces, then a boolean. Not tested at 10 mm vs 8 mm. |
| 1207 | Fillet refused on the edge where a flat face meets a cylindrical boss (partial arc edge) | FissionFillet.cpp | Partial arc where a plane meets a boss. OCCT often succeeds. Not run. |
| 1212 | Extruding a floor into a hole it fills fails: "boolean Union: the operation failed: no result" | FissionFeature.cpp | Filling a hole flush is a tangent union. |
| 1219 | Cut into a bore is 350x slower (0.2 s -> 73 s, 24 retries) when the bore edge has a fillet the cut doesn't touch | FissionFeature.cpp | A cut near an untouched fillet is a normal boolean with a 20s cap, not 24 truck retries. |
| 1231 | Extrude cut fails when cutting Sheet Metal part | FissionSheetMetal.cpp | Sheet bodies are solids and can be cut; this report was not run. |
| 1234 | EXTRUDE: operation fails when cutting through multiple components | src/kernel | Fission has this operation. The report was not reproduced. |
| 1275 | Extrude Cut from a whole face fails after a chamfer or fillet on a rounded-corner body ("boolean Cut: no result") | FissionExtrude.cpp | Extrude of a whole face after a fillet. Not run. |
| 1279 | Cut fails with "no result" when the tool has a cylinder coaxial with an existing hole, 0.1 mm inside its wall | FissionFeature.cpp | A cylinder 0.1 mm inside a hole wall is a near-coincident cut with no fuzzy value set. |
| 1337 | Extrude crashes on complicated shape | src/kernel | Fission has this operation. The report was not reproduced. |
| 1391 | Joining a flange flush with the end of a bent, tapered loft fails (Union: no result) | FissionFeature.cpp | Join onto a bent tapered loft. Not run. |
| 1490 | Loft join into a shelled case fails ("boolean Union: no result", no retry) for wall 2.4–2.6 mm; works at 2.8 and 3.0 mm | src/kernel | Fission has this operation. The report was not reproduced. |
| 1510 | Split Body fails ("boolean Intersect: no result") when the plane is coplanar with a face that runs into a through hole | FissionSolidTools.cpp | A plane coplanar with a face can leave a zero-thickness piece. |
| 1531 | Tapered extrusion fails when the result has overlapping output geometry | FissionExtrude.cpp | Overlapping tapered sides fail the draft or the boolean. Not run. |
| 1617 | Box intersect fails ('no result') after a fit:reduce side fillet on a prism with a raked face (vertical face works) | src/kernel | Fission has this operation. The report was not reproduced. |
| 1648 | Boolean: union of a sphere and a box on the grid leaves a face that does not mesh | src/kernel | Fission has this operation. The report was not reproduced. |
| 1649 | Boolean: union of two crossing cylinders loses volume when both are moved 1000 mm | FissionFeature.cpp | Booleans are not moved near the origin first. |
| 1668 | Fillet refuses inner + outer rounds of a thin wall in one feature ("largest radius that fits is 4 mm"), but the same two rounds as two features work | FissionFillet.cpp | One MakeFillet call for every selected edge; mixed radii can fail even when split would work. |
| 1669 | Mirror of a cut + loft join fails at wall 3.5 mm ("boolean Union: no result") while the same features built directly succeed (but take 9 s) | FissionTransform.cpp | Mirror join is a fuse. A 3.5 mm wall case was not run. |
| 1690 | Text cut into a part with rounded edges can't be cut again or its face extruded (boolean Cut: no result) | FissionFeature.cpp | A second cut or face extrude after text. Not run. |
| 1691 | Split Body by a plane across a chamfer's cone loses 294 mm³ with no error | FissionSolidTools.cpp | Split is a half-space cut with no volume check against the chamfer cone. |
| 1692 | Cut of the face outside a ring round a hole fails on a chamfered disc (tool tangent to the chamfer) | FissionFeature.cpp | A cutter tangent to a chamfer. Not run. |
| 1736 | Co-centered hole from internal side - problems to create | FissionHole.cpp | Direction is the face normal into the body, with flip. An internal co-centered hole was not run. |
| 1856 | kernel-perf: union of a 20-tooth lofted helical pinion takes minutes | FissionLoft.cpp | A 20-tooth helical pinion loft-union can be slow. Each fuse in applyOperation stops at 20s. |
| 1972 | SHELL: unexpected result   shelling  a half Sphere applied the Shell to the hidden half | FissionShell.cpp | Shell follows the face's body, not which half is hidden. |
| 1986 | Chamfer refused on an arc edge: a prism join leaves one cylindrical wall as a stack of faces | FissionFillet.cpp | An arc edge whose wall was split into stacked faces. Not run. |
| 2038 | Modeled thread: second thread on same body fails, combine of two threaded bodies hangs | FissionHole.cpp | A second thread is another cut of a cylindrical face. Combine of the two bodies is the unbounded combine path. |
| 2040 | Extrude from a face rounded by fillets: cut, join and taper all fail with boolean no result | FissionExtrude.cpp | A planar face can be extruded; the boolean after a fillet is uncapped only by the 20s limit. |
| 2076 | Rectangular pattern of a cylinder join scales ~quadratically: 100 pins 84 s, 225 pins 527 s (one-sketch extrude: 5 s / 12 s; patterned cut: 3 s) | FissionTransform.cpp | Each joined copy is a boolean capped at 20s, so hundreds of pins get slow or start failing. |
| 2121 | Circular pattern of a feature not updating body geometry after change | FissionTransform.cpp | Body patterns recompute. A pattern of a feature that is not a body copy does not exist. |
| 2178 | operation SHELL fails when taper angle of previous extrusion is negative | FissionShell.cpp | A negative taper on a box is still planar, which prefers intersection joins. Not in the +5 degree selftest. |
| 2258 | Cutting a shelled body beside an earlier cut through its curved end fails (robustness seed 61) | FissionShell.cpp | Cut beside a shelled curved end. Not run. |
| 2295 | Revolve Operation Body Cut Failed - 'boolean cut: the operation failed: no result' | FissionRevolve.cpp | Revolve cut returned no result. Not run. |
| 2358 | Pattern of countersunk holes slows roughly quadratically: 135 countersinks take 19 min (135 plain holes: 21 s) | src/kernel | Fission has this operation. The report was not reproduced. |
| 2579 | Boolean: a small hole in a large STEP body's face can be lost when the face is divided | src/kernel | Fission has this operation. The report was not reproduced. |
| 2591 | Cut ending exactly on a face leaves a zero-thickness double disc when it reaches a fillet's tangent edge (0.15.0 regression) | FissionFeature.cpp | A cut that ends on a fillet tangent is not checked for a zero-thickness disc. |
| 2688 | Improve Error Message When Creating Threads with Incorrect Hole Diameter | src/kernel | Fission has this operation. The report was not reproduced. |
| 2700 | Extrude: distance2 is ignored when the first side is through_all | FissionExtrude.cpp | throughAll does not read distance2, so a two-sided through cut is not available. |
| 2705 | Thread: "the face is too far from the thread's diameter" on a slightly oversized shaft or undersized hole | FissionHole.cpp | Thread requires a cylindrical face near the tool diameter. A slightly off shaft can fail the face match. |
| 2770 | Join extrude of a rim ring on a Gridfinity-style base fails after a minute: boolean Union: no result | FissionFeature.cpp | Flush ring join, same class as 4058. |
| 2797 | Chamfer: Two Distances / Distance+Angle not supported on plane–cylinder edges | FissionFillet.cpp | Two-distance and distance-angle exist; OCCT often rejects them on plane-cylinder edges. |
| 2810 | Tapered extrude: a loop of arcs meeting at corners gets wrong walls, without an error | FissionExtrude.cpp | A loop of arcs is drafted as side faces. Wrong walls versus a hard fail was not compared. |
| 2811 | kernel-perf: filleting many edges (grille rib roots) grows faster than N²: a whole-body mesh per edge lookup | src/kernel | Fission has this operation. The report was not reproduced. |
| 2844 | Join onto a perforated plate splits a cylinder wall into full-height strip faces (64 instead of 2) | GeomUtils.cpp | unify skips shapes that contain free-form faces, so a perforated join can leave strip faces. Not run on 64 strips. |
| 2847 | Loft through rotated non-circular sections folds the solid once they span more than ~90° (volume 6% of expected at 180°, no error) | FissionLoft.cpp | ThruSections with CheckCompatibility can still twist past about 90 degrees. Not in selftest. |
| 2904 | solid.hole also processes a distant body, and its retry silently drops part of a countersink ("not verified ... that part is ignored") | FissionHole.cpp | Hole cut uses empty targets. A body 120 mm away is only hit if boxes overlap. The silent countersink drop is truck. |
| 2947 | Boolean gives up ("loop not inside any face piece") when a revolved face holds its surface's seam | FissionRevolve.cpp | A revolved face still has a seam. Only the tube recipe in selftest is checked. |
| 2974 | Counterbored hole + pattern ~8× slower when the body has a joined cylinder elsewhere (follow-up to #1197, non-planar tool) | src/kernel | Fission has this operation. The report was not reproduced. |
| 3062 | fillet / chamfer not working as expected on round object | FissionFillet.cpp | Round-object fillet and chamfer. Not run. |
| 3243 | solid.combine fails to union an overlapping cylindrical rim and rounded tooth | FissionSolidTools.cpp | Cylindrical rim union. Not run. |
| 3244 | Cylinder intersection reports success and exports geometry outside the clipping cylinder | src/kernel | Fission has this operation. The report was not reproduced. |
| 3311 | ModelingThread/tapped hole issues: hole diameter not set to tap drill size, only one modeled thread per body, faceted geometry after threading: unexpected result | FissionHole.cpp | Tap-drill diameter is handled. One modeled thread per body and faceting were not confirmed. |
| 3383 | Combine: a tool body joined into a rotated occurrence lands in the target's local frame | src/kernel | Fission has this operation. The report was not reproduced. |
| 3454 | Keyhole cut fails ('no result') on a plate with filleted rounded corners when the slot end is coaxial with the entry hole | FissionFeature.cpp | Keyhole slot coaxial with a hole on a filleted plate. Not run. |
| 3460 | Combine (Cut) fails to subtract small curved snap-fit tabs from target body | FissionSolidTools.cpp | Small curved cutters. Not run. |
| 3468 | Offset Face / Press Pull: refuses a face next to a fillet (a corner whose faces don't meet the same way) | FissionSolidTools.cpp | Global face offset can fail beside a fillet; the prism fallback does not carry the fillet along. |
| 3585 | SHELL: operation failed | FissionShell.cpp | Freeform faces are not refused up front; curved offset can still fail. No B-spline shell selftest. |
| 3604 | Boolean cut fails for a swept G2 corner-removal profile along a projected edge (side-dependent) | FissionSweep.cpp | Swept G2 cutter. Sweep has no G2 guide. |
| 3655 | Chamfer: the same edges are refused or accepted depending on the order they're listed | FissionFillet.cpp | Edges are added in list order with no sort or retry. |
| 3665 | Join extrude of a column standing on the target's floor and wall fails (Union: no result) while new body + Combine joins | FissionFeature.cpp | Join and Combine both fuse; a column on a floor and a curved wall was not run. |
| 3672 | THICKEN: operation failed | src/kernel | Fission has this operation. The report was not reproduced. |
| 3731 | Cylinder joins onto a tapered (drafted) wall are 20-100x slower than onto a vertical wall; 2+ cylinders on the conical corners trigger a boolean retry | FissionFeature.cpp | Joins onto a drafted wall use the same fuse, with a 20s cap, and no conical special case. |
| 3732 | COMBINE: La soustraction avec conservation d'outil ne produit aucun résultat | FissionSolidTools.cpp | keepTools is implemented. A cut that keeps the tool and returns nothing was not run. |
| 3774 | Offset when "CUT in Combine" for better fits | src/kernel | Fission has this operation. The report was not reproduced. |
| 3785 | Pattern along a path doesn't go around corners. | FissionSolidTools.cpp | Path pattern follows arc length on one sketch wire. The report is about selecting faces, not that spacing. |
| 3899 | Transform: a non-affine matrix is accepted and builds a body that meshes to nonsense (a later boolean runs 36 min, then "no result") | src/kernel | Fission has this operation. The report was not reproduced. |
| 3902 | Face names: the pieces of a split face swap #1/#2 between evaluations of the same design | src/kernel | Fission has this operation. The report was not reproduced. |
| 3936 | Modeling: Loft join operation fails if one surface is circular and the other is rectangular | FissionLoft.cpp | Circle-circle and square-square lofts are tested; circle-to-rectangle is not. |
| 4008 | Symmetric revolve cut refused ("could not be computed reliably") when the tool's end faces lie in the body's side face | FissionRevolve.cpp | Symmetric revolve exists. End faces lying in a side face were not tested. |
| 4010 | Shell refuses to remove the wall of a through hole ("offset: a corner whose faces don't meet the same way after the move") | FissionShell.cpp | Removing a cylindrical hole wall uses the curved thick-solid path; the punch tool is a single direction. |
| 4055 | Chamfer that reaches a joined pin leaves the part open (or is refused) | FissionFillet.cpp | Chamfer that reaches a pin. No open-shell rejection after IsDone(). |
| 4058 | Join: walls extruded flush round a part's outline fail with "boolean Union: the operation failed: no result" | FissionFeature.cpp | A flush, non-overlapping join is a tangent fuse. OCCT often returns nothing. Not run. |
| 4095 | Boolean Cut silently removes the wrong volume when a planar face stands closer to a cylinder than the coincidence tolerance | FissionFeature.cpp | No fuzzy tolerance and no volume check after a successful cut. |
| 4101 | Chamfer on a block with 1° sides leaves four 0.00016 mm edges at the corners | FissionFillet.cpp | No pass that removes 0.00016 mm edges left on a 1 degree block. |
| 4140 | CHAMFER: two edges selected results in error | FissionFillet.cpp | Two edges in one chamfer. Not run. |
| 4250 | [Kernel] Surface revolutions sometimes split at strange increments. | FissionRevolve.cpp | MakeRevol leaves a seam. "Strange increments" were not measured. |
| 4294 | Extrude Cut leaves a stray planar sheet outside a slotted cylindrical body, with no error | FissionFeature.cpp | Cut keeps solids only; a zero-thickness sheet might still survive if OCCT returns it as a solid. |
| 4336 | FILLET: operation failed between Radius and line | FissionFillet.cpp | Fillet between an arc and a line. Not run. |
| 4469 | Chamfer of an offset outline wall's rim: many short edges, partial tangent chain, refused or very slow | GeomUtils.cpp | A partial chain of many short edges can be slow; there is no timeout around Build(). |
| 4495 | FILLET: operation failed - Radius on edge on tilted surface | FissionFillet.cpp | Radius on a tilted face. Not run. |

### NOT-APPLICABLE (165)

| Issue | Title | Evidence | Why |
|---:|---|---|---|
| 193 |Bug: Fillets dont get copied| FissionTransform.cpp | Patterns copy the finished body. They do not replay a fillet feature onto copies. |
| 400 |Rectangular pattern of a cut whose instances touch: every instance takes the exact planar fallback (40 instances 6.5 s vs 0.04 s as one cut)| n/a | Truck planar-fallback timing, not an OCCT pattern bug. |
| 494 |Revolve, Offset Face and Boss drop every face source of the body| n/a | Face-name bookkeeping after revolve and offset. |
| 588 |Modeling: Area is not split by tangent constraint.| n/a | Sketch tangent constraint, not a solid split. |
| 623 |Blind bore cut into joined bosses intermittently fails: "lumps: cannot mesh a complete shell boundary" (result differs between runs)| n/a | Truck lumps mesher ("cannot mesh a complete shell boundary"). |
| 665 |Regression from #644: fuzz seed 16 (box, joined cylinder, circular pattern) fails fixed_seeds_run_clean| n/a | Truck fuzz-seed regression. |
| 711 |action_matrix_sample is flaky: lid_ribs_flush_with_window Lid/F11:+y face.hole fails in some runs| n/a | Truck action-matrix flake. |
| 788 |Cutting a hole that crosses an equal-radius hole at right angles is refused ("would change the body away from the tool")| n/a | Truck refusal "would change the body away from the tool". |
| 973 |Draft, Accessibility and Curvature Map analyses have no legend| n/a | Analysis legend, not the Draft solid feature. |
| 1143 |solidcraft-cli run and eval can write their JSON to a file (--json-out), so that no shell redirect re-encodes it| n/a | CLI JSON output. The title matched the word shell in "shell redirect". |
| 1213 |A round or chamfer on the end circle of a bent lofted tube is refused: "a curved seam where the edge chain turns"| n/a | Truck "curved seam where the edge chain turns". |
| 1252 |A panic from IntersectionCurve::subs (unwrap on None) is printed while meshing, then caught| n/a | Rust IntersectionCurve panic while meshing. |
| 1363 |Compare two bodies: where two versions of a part differ, without a boolean of the two| n/a | Version compare, not a solid boolean. |
| 1384 |Fillet/chamfer on an inside edge: the drag arrow points the wrong way| n/a | Drag-arrow direction. UI. |
| 1535 |New tab shows the previous design's fillet preview; + adds only one blank tab| n/a | Fillet preview left on a new tab. UI. |
| 2154 |Offset after explode text| n/a | Sketch text offset. |
| 2163 |Procedural material patterns (no stored images), with Poly Haven CC0 scans as references| n/a | Render materials. The title matched pattern. |
| 2327 |Offset Plane: default the distance to 0 mm| n/a | Construction-plane default distance. |
| 2652 |Split Manufacture megapr (#1782) into a stacked, interface-first PR tree| n/a | Manufacture PR split. Not a solid split. |
| 2891 |Sweep cut crashes website| n/a | Website/WASM crash. Fission is a desktop OCCT app. |
| 2978 |OFFSET MOUSE: the pointing mouse is offset frome real cordinates| n/a | Mouse cursor offset. UI. |
| 3418 |SheetMetal Flat: Fillets and Chamfer not showing in flat| n/a | Sheet-metal flat pattern display. |
| 4098 |Extrude: document which way a positive taper goes| FissionExtrude.cpp | Docs request. Positive taper here narrows away from the sketch plane. |
| 4594 |project intersect issue| n/a | Sketch project/intersect, not a solid boolean. |
| 10 | Boolean and blend robustness (random-design fuzzer) | n/a | Robustness-fuzzer tracking issue, not one geometry bug. |
| 13 | Debug builds: split_body fails with "Two same vertices cannot construct an edge" | n/a | Sketch, sheet metal, UI, naming, export, or truck-only from the title. |
| 24 | Booleans: report when a fallback (jitter/nudge) was needed, and by how much | n/a | Asks truck to report nudge retries. Fission has no nudge fallback. |
| 29 | Plastic Lip fails on a rounded (filleted) shelled rim: coincident faces on curved bodies | n/a | Plastic Lip feature; Fission has no lip tool. |
| 45 | Sketch: trim / extend / offset robustness | n/a | Sketch, sheet metal, UI, naming, export, or truck-only from the title. |
| 46 | Sketch: fillet / chamfer of corners (line-arc, arc-arc, chains, too-large radius) | n/a | Sketch, sheet metal, UI, naming, export, or truck-only from the title. |
| 164 | Sketch highlight for extrude is less visble when more zoomed in. | n/a | Sketch, sheet metal, UI, naming, export, or truck-only from the title. |
| 167 | Ability to create offset planes from surface | n/a | Offset plane from a surface. Construction. |
| 218 | solid.pattern.circular: expressions in axis.origin are silently stored as [0,0,0] | n/a | Expression storage in a truck pattern axis. |
| 219 | Rectangular pattern of [cut + fillet] fails after a parameter makes the pocket much taller ("faces at the edge ends must be planar") | n/a | Truck "faces at the edge ends must be planar". |
| 228 | Panic in vendored truck-shapeops intersection_curve/mod.rs:39 (polyline[0] on empty polyline) | n/a | Rust panic in truck-shapeops. |
| 232 | Union of similar-size spheres refused: "lumps: contact work exceeds the limit" (0.3.2 regression) | n/a | Truck lumps contact-work limit. |
| 237 | Fillet refuses the straight edge between two cylinders: "a curved edge where the blend ends" | n/a | Truck "curved edge where the blend ends". |
| 265 | Renaming a pattern copy renumbers the remaining copies, so their old names stop working | n/a | Renaming pattern copies. |
| 266 | Multi-profile `solid.extrude` with `body_name` names only the first body; the others get generic `BodyN` names | n/a | Body name strings. |
| 271 | Command docs omit parameters that work: `sheet.flange` `body_name`, `solid.loft` `targets` | n/a | Command docs. |
| 296 | Sheet metal: cutting a hole in a base with two adjacent edge flanges fails (sew: nonmanifold edge contact) | n/a | Sketch, sheet metal, UI, naming, export, or truck-only from the title. |
| 311 | Ramp cuts distort cylindrical meshes and groove fillets create open seams in SolidCraft 0.3.2 | MeshUtils.cpp | 0.3.2 mesh distortion and open seams, not the OCCT solid kernel. |
| 372 | Raising a pattern's count after renaming a copy gives the new copy the freed name | n/a | Copy names. |
| 373 | A profile added to the sketch after a named multi-profile extrude gets a generic `BodyN` name | n/a | Body name strings. |
| 392 | solid.emboss with `face` on a tapered (drafted) planar wall: "`face` must identify exactly one target body surface" | n/a | Sketch, sheet metal, UI, naming, export, or truck-only from the title. |
| 396 | Loft joined onto a box: body_count is 1, but the STL export contains two disconnected shells | n/a | Sketch, sheet metal, UI, naming, export, or truck-only from the title. |
| 401 | Renaming a feature breaks features that reference it by name (pattern fails with "unknown feature") | n/a | Features referenced by name. |
| 403 | Extrude in a moved component from another component's sketch is built in the wrong place | n/a | Sketch, sheet metal, UI, naming, export, or truck-only from the title. |
| 416 | Sheet metal: a round hole or a slanted slot across a bend line cannot be folded | n/a | Sketch, sheet metal, UI, naming, export, or truck-only from the title. |
| 431 | Fillet along a frame's top outline (lines and arcs, planar top) refused: "the ball doesn't fit along the edge" | FissionFillet.cpp | Truck refuses non-straight edges. Fission fillets whatever edges were selected, including arcs. |
| 451 | Cut with a hollow body (a spherical shell): a body inside the hollow is removed; a body crossing it can't be cut | FissionFeature.cpp | The quoted failure is truck's planar exact fallback. |
| 480 | Combine dialog: OK is always greyed out (preview reports "operation must be join, cut or intersect") | n/a | Combine OK button stays disabled. UI. |
| 491 | Sketch on a side face that Join merged from two extrudes is refused | n/a | Sketch, sheet metal, UI, naming, export, or truck-only from the title. |
| 492 | Sketch on a face split into pieces by a later feature is refused | n/a | Sketch, sheet metal, UI, naming, export, or truck-only from the title. |
| 493 | Flat faces made by Cut walls and chamfers have no exact source to sketch on | n/a | Sketch, sheet metal, UI, naming, export, or truck-only from the title. |
| 517 | Sketch: Offset tool is missing a dialog | n/a | Sketch, sheet metal, UI, naming, export, or truck-only from the title. |
| 535 | Sheet metal: hole faces on a folded panel lose their sketch-curve names (side:c1#1 becomes F6:face:10) | n/a | Sketch, sheet metal, UI, naming, export, or truck-only from the title. |
| 541 | Sheet metal: a primitive box/cylinder cut straight through a flat sheet is refused, though the equivalent extrude cut works | n/a | Sketch, sheet metal, UI, naming, export, or truck-only from the title. |
| 542 | Sheet metal: sheet.flat_pattern returns round holes as sampled polylines instead of circles | n/a | Sketch, sheet metal, UI, naming, export, or truck-only from the title. |
| 544 | Blind Ø2 hole into a long D-profile bar is refused ("exact fallback … takes planar faces only") although it stays clear of the round face | n/a | Truck exact fallback is planar-only. |
| 612 | STEP export: straight-edge fillets and extruded arcs are written as B-spline surfaces with oversized bounds (a 60 mm part reads as -34.9…94.9 mm in OCCT) | n/a | Truck STEP writer bounds. |
| 613 | Pasted fillet keeps the original `body`: paste with translate moves the edge picks but fillets the old body ("none of the selected edges exist any more") | n/a | Paste keeps a fillet's old body id. Different reference scheme. |
| 615 | Extrude Cut Error: "invalid input: a boolean needs a solid body" when intersecting with mesh | FissionFeature.cpp | A mesh target is rejected. This is not a solid-boolean bug. |
| 640 | Blind hole into an extruded D-profile bar fails: "exact fallback ... takes planar faces only", the nudged tool reports the volume of the whole bar | n/a | Same planar-only fallback. |
| 647 | Convert to Sheet Metal: convert a solid or a shelled body from its fixed face and bend edges | n/a | Sketch, sheet metal, UI, naming, export, or truck-only from the title. |
| 657 | HOLE: with "multiple" hole type center alignment to circle center on sketch works, on "single" - not | n/a | Sketch, sheet metal, UI, naming, export, or truck-only from the title. |
| 684 | Extrude pre-picks the last sketch's already-used profile (and defaults to Cut) when nothing is selected | n/a | Extrude pre-picks the last sketch. UI default. |
| 696 | Reuse Solid dialogs for Sketch Mirror, Pattern, and Offset with editable selections and live previews | n/a | Sketch, sheet metal, UI, naming, export, or truck-only from the title. |
| 726 | Revolve emits planar faces as 3 patches, so sketch/extrude on a revolved face fail until another feature re-derives the body | n/a | Sketch, sheet metal, UI, naming, export, or truck-only from the title. |
| 742 | Booleans on scaled spheres: cost depends on the sphere's patch orientation (identical geometry, 2.5× wall time); mesh tolerance comes from vertex boxes | n/a | Truck sphere-patch orientation cost. Not how OCCT booleans are timed. |
| 790 | Bug Report: Sketch Pattern Unexpected Result | n/a | Sketch, sheet metal, UI, naming, export, or truck-only from the title. |
| 796 | Kernel: fine tessellation of a fillet tapering to zero loses volume on Windows (taper_follows_the_room_along_the_edge fails) | n/a | Windows tessellation of a zero-end fillet. |
| 882 | Cut through the open rim of a shelled body fails after an R1 fillet on the opposite end face ("exact fallback takes planar faces only") | n/a | Same planar-only fallback. |
| 884 | R1 fillet on one outer end loop refused after R5 ("curve blend: could not fit the rolling-ball surface"), even on a single straight edge between planar faces | n/a | Truck rolling-ball fitter refusal. |
| 954 | Can't sketch on the side walls of one sketch extruded up and down and joined | n/a | Sketch, sheet metal, UI, naming, export, or truck-only from the title. |
| 967 | Hole tool: audit and fixes | FissionHole.cpp | SolidCraft hole-audit list. Face holes here use the picked face normal. |
| 1026 | Sketch fillet fails at a corner that also has a construction line ending in it ("the corner must join exactly two lines") | n/a | Sketch, sheet metal, UI, naming, export, or truck-only from the title. |
| 1089 | Sheet metal: the flat-pattern DXF writes ellipses, splines and text as 0.001 mm LINEs, in a file with no DXF version | n/a | Sketch, sheet metal, UI, naming, export, or truck-only from the title. |
| 1092 | Convert to Sheet Metal: bent parts as other systems model them (radius or chamfer bends, holes and labels in their faces) can't be converted | n/a | Sketch, sheet metal, UI, naming, export, or truck-only from the title. |
| 1113 | Kernel: radius-10 fillet on arc top edge returns an unmeshable face | n/a | Unmeshable display face. |
| 1181 | STEP export: shell of a tapered rounded-rectangle extrusion writes the inner walls as zero-area B-splines at z = −61 (volume +46 % in OCCT) | n/a | Truck STEP of a tapered shell. |
| 1197 | Feature pattern falls back to slow boolean retry on every instance when the body has any curved face (follow-up to #400) | n/a | Truck boolean-retry on every pattern instance. |
| 1198 | STEP export: fillet and arc faces cut by an angled feature are written as B-splines with a bounding box ~2× the part (incl. an uncut arc end, see #612) | n/a | Truck STEP of trimmed fillets. |
| 1208 | Sketch fillet refuses a corner between a line and an arc | n/a | Sketch, sheet metal, UI, naming, export, or truck-only from the title. |
| 1228 | Browser: custom names that match a UI label are translated (a body named "Fillet" shows as "Vollrundung") | n/a | Sketch, sheet metal, UI, naming, export, or truck-only from the title. |
| 1263 | Kernel: display planner rejects knife-edge arc fixture before filleting | n/a | Display planner. |
| 1284 | Sheet metal: the flat-pattern DXF puts every contour on one layer, draws the bend lines continuous and writes one body only | n/a | Sketch, sheet metal, UI, naming, export, or truck-only from the title. |
| 1483 | Sheet metal flat pattern view can't be panned or zoomed, and the Design camera moves beneath it | n/a | Sketch, sheet metal, UI, naming, export, or truck-only from the title. |
| 1488 | Boolean retry ("tool_nudge") leaves 0.0001 mm sliver faces; every later fillet on the body then fails ("rounds would leave openings") | n/a | tool_nudge slivers. |
| 1492 | solid.mirror: plane {origin, normal} doesn't accept expressions, so the mirror plane can't follow a parameter | n/a | Mirror plane expression API. |
| 1519 | sketch loses its face attatchment when changing its face-creating extrude even when there is still a face | n/a | Sketch, sheet metal, UI, naming, export, or truck-only from the title. |
| 1544 | Feature Request: Mirror Entities in Sketch Mode | n/a | Sketch, sheet metal, UI, naming, export, or truck-only from the title. |
| 1559 | Crash: not implemented: intersection curve cannot connect by homotopy | n/a | Truck homotopy intersection is not implemented. |
| 1597 | Kernel roadmap: slow fillets and booleans, and where to help | n/a | Kernel roadmap note. |
| 1637 | A feature that names its body still looks for its named face on every body (rover `Shell2`: 6–8 s for a 6 ms shell) | n/a | Named-face lookup cost. |
| 1647 | Boolean: a box and a cylinder in general position, Intersect refused as "implausible result volume" | FissionSolidTools.cpp | The failure is truck's implausible-volume check. OCCT common has no such test. |
| 1666 | STL/3MF export of a body with loft faces isn't a valid mesh: each ruled face is two triangles, self-intersecting, and non-manifold once joined | n/a | STL/3MF tessellation of loft faces, not the solid loft. |
| 1667 | measure: volume and centre of mass wrong after fillets (+11 492 mm³ on a 472 000 mm³ body); STEP of the same body measures correctly | n/a | Measure after fillets. |
| 1678 | `solid.hole`: let `position` take parameter expressions so point-placed holes follow the design | n/a | Hole position expression API. |
| 1805 | Extrude: show a New Body preview in a different colour from Join | n/a | New-body preview colour. |
| 1824 | Sketch fillet can only pick the corner point, so a corner where more than two lines meet cannot be filleted | n/a | Sketch, sheet metal, UI, naming, export, or truck-only from the title. |
| 1865 | Union of two solids that meet along cylindrical faces fails: the exact fallback is planar-only | FissionFeature.cpp | The refusal is truck planar_boolean. Fission uses BRepAlgoAPI_Fuse. |
| 1983 | Booleans stop terminating after a faceted-fallback result enters the body | n/a | Truck faceted-fallback booleans stop terminating. |
| 2100 | Instead, "PRESS PULL" opens “EXTRUDE” | n/a | Press-pull opens Extrude. UI. |
| 2103 | Extrude cut preview drawn in the wrong place for a sketch on a face in a mirrored STEP component (result is correct) | n/a | Preview drawn in the wrong place; the report says the result is correct. |
| 2110 | Hole: placement "single" not updating as expected | n/a | Hole placement widget. |
| 2114 | Revolved body - OFFSET PLANE / CREATE SKETCH / EXTRUDE: operation failed | n/a | Sketch, sheet metal, UI, naming, export, or truck-only from the title. |
| 2171 | TIPS FOR A BETTER EXPERIENCE ABOUT HOLE FUNCTION | n/a | Hole UX notes. |
| 2174 | Sketch: no live preview for fillet and chamfer corners | n/a | Sketch, sheet metal, UI, naming, export, or truck-only from the title. |
| 2186 | Joints on holes in thin sheet lose their hole, flip sides or turn when parameters change | n/a | Sketch, sheet metal, UI, naming, export, or truck-only from the title. |
| 2341 | Pattern copies of a chamfer or fillet are blended one copy at a time: a 6 × 5 grid of chamfered bosses takes 57 s (13 s as one feature) | FissionTransform.cpp | Same: no per-copy blend feature. |
| 2356 | Flush plug over a countersunk hole: Boolean retry leaves a 0.1 µm step, and the STL/3MF export has open and non-manifold edges | n/a | A 0.1 um step left by a truck retry. |
| 2357 | Joining a ring of 90 straight segments: caught panic at truck intersection_curve.rs:133, then the join is refused after 21 min | n/a | Rust panic in intersection_curve.rs. |
| 2360 | Boolean retry on a cut extrude whose profile edges are coincident with existing wall faces | n/a | Truck retry when profile edges lie on existing walls. Extrude cut here is the 20s fuse. |
| 2385 | A joint's offset, angle and values don't follow the parameters they were typed with | n/a | Sketch, sheet metal, UI, naming, export, or truck-only from the title. |
| 2398 | [UX / Feature Request] Visual feedback and face snapping for Extrude Cut depth | n/a | Extrude preview graphics. |
| 2402 | Improve visual distinction between Extrude Distance and Draft Angle manipulators | n/a | Manipulator graphics. |
| 2435 | LOFT: not suported yet | FissionLoft.cpp | Closed "not supported yet" with no remaining Fission gap identified from the title. |
| 2467 | Renaming a body recomputes every feature after it, on the window thread (minutes with a large pattern) | n/a | Sketch, sheet metal, UI, naming, export, or truck-only from the title. |
| 2574 | Hexagonal Pattern | FissionSketch.cpp | Request is a sketch hexagonal pattern, not a solid pattern. |
| 2581 | Modeling: Difficult Selection of Origin Planes during Mirroring | n/a | Picking origin planes while mirroring. UI. |
| 2599 | `joint.as_built` at a face or hole origin doesn't follow that face or hole | n/a | Sketch, sheet metal, UI, naming, export, or truck-only from the title. |
| 2663 | Black-box coverage: blends: fillets, chamfers and their corners (6 of 41 sampled mutants caught) | n/a | Sketch, sheet metal, UI, naming, export, or truck-only from the title. |
| 2665 | Black-box coverage: booleans and extrude-type features (10 of 44 sampled mutants caught) | n/a | Sketch, sheet metal, UI, naming, export, or truck-only from the title. |
| 2666 | Black-box coverage: offset, shell, emboss and lip (0 of 14 sampled mutants caught) | n/a | Sketch, sheet metal, UI, naming, export, or truck-only from the title. |
| 2667 | Black-box coverage: primitives, profiles and sweeps (2 of 11 sampled mutants caught) | n/a | Sketch, sheet metal, UI, naming, export, or truck-only from the title. |
| 2670 | cargo xtask ci runs the nightly-only boolean_props tests, so the local gate fails on a clean main | n/a | Sketch, sheet metal, UI, naming, export, or truck-only from the title. |
| 2711 | No manipulator for Distance 2 on Chamfer Type "Two Distances" | n/a | Chamfer distance-2 manipulator. The kernel reads distance2. |
| 2722 | CREATE SKETCH: operation failed on face of thin extrude | n/a | Sketch, sheet metal, UI, naming, export, or truck-only from the title. |
| 2729 | In Sheet Metal mode, allow user to split an edge and apply flanges or other features independently. | n/a | Sketch, sheet metal, UI, naming, export, or truck-only from the title. |
| 2774 | Feature patterns: blend copied fillets and chamfers once for all copies, and name copies' blend faces after the source's | FissionTransform.cpp | Patterns copy finished bodies; they do not re-fillet each copy. |
| 2843 | Modeling: Unable to modify text for extrude numeric commands or report a bug | n/a | Cannot type into an extrude field or file a bug. UI. |
| 2851 | Union fails when a sphere is joined tangent to a cylinder's end (same diameter, centred on the cap): "exact fallback… takes planar faces only" | n/a | Same planar-only fallback on a tangent sphere. |
| 2862 | Joining three pipes that meet at one point fails (non-planar Y junction): truck-geometry panics at intersection_curve.rs:133, then "Union: no result" | n/a | Rust panic then a refused union. The panic cannot occur here. |
| 3020 | Boolean retries: a coarse candidate whose trim edge runs off its surface is meshed without bound in the volume check | n/a | Truck retry meshing. |
| 3035 | kernel-perf: shared intersection edges are sampled repeatedly during healing and validation | n/a | Truck healing samples shared edges repeatedly. |
| 3075 | Modeling: unexpected result when right clicking edit on fillet / round | n/a | Right-click edit of a fillet. UI. |
| 3085 | Crash when selecting edge for Fillet/Chamfer after Combine with imported STEP body Modeling: unexpected result | n/a | Crash while picking an edge after a STEP combine. Selection, no public solid. |
| 3156 | Revolve join at an angle that puts its end faces in a target face's plane (e.g. ±90°) needs a tool_nudge retry | n/a | Truck tool_nudge on a revolve join. |
| 3324 | Flat pattern and DXF silently ignore non-sheet features on a sheet body | n/a | Sketch, sheet metal, UI, naming, export, or truck-only from the title. |
| 3353 | Sheet metal: a mirrored or copied sheet body loses its flat pattern | n/a | Sketch, sheet metal, UI, naming, export, or truck-only from the title. |
| 3369 | Fillet refused on an edge whose ends sit on the tangent seam of a rounded face ("the faces at the edge ends must be planar") | FissionFillet.cpp | The refusal is truck's "ends must be planar" rule, which this kernel does not implement. |
| 3378 | Circular pattern of a join that merged its targets fails with `unknown body` (regression since 0.15.0) | n/a | Truck "unknown body" regression. |
| 3379 | Circular pattern of a spoke thinner than the ring it joins fails after ~10 s with a `lumps` error | n/a | Truck lumps error. |
| 3567 | OFFSET PLANE: operation failed | n/a | Offset-plane failure. Construction. Parity note already skipped it. |
| 3574 | OFFSET: operation failed | SketchEdit.cpp | The body is a sketch.offset spline certificate, not a solid offset. |
| 3618 | Renaming Split Bodies Causes Unexpected Behavior | n/a | Sketch, sheet metal, UI, naming, export, or truck-only from the title. |
| 3634 | Modeling: unexpected result, hole command auto-switch to multi-hole mode | n/a | Hole dialog switches to multi-hole. UI. |
| 3865 | SELECTION: Multiple selection not working right in combine operation | n/a | Combine multi-select UI. |
| 3975 | Rework of the hole tool. | n/a | Hole-tool rewrite tracking issue. |
| 4005 | STEP export of a revolve about an oblique axis reads back with a third of the volume ("bad cone" faces dropped) | n/a | STEP writer dropped cones. Fission uses OCCT's writer, not this bug. |
| 4006 | STEP export of a first revolve with operation join reads back as an open body ("bad cone"): 26386 instead of 27251 mm3 | n/a | Same STEP cone export. |
| 4011 | Mirror of a cut whose copy lands under a wall removes 129.8 mm3 instead of 113.1 (a direct cut there removes 113.08) | FissionTransform.cpp | Mirror copies bodies. It does not replay a cut feature under a wall. |
| 4029 | Modeling: unexpected result - Offset plane | n/a | Offset plane result. Construction, not a solid op. |
| 4092 | EXTRUDE Union on native loft with ribs: recovered intersection_curve panic, then no result | n/a | Recovered intersection_curve panic. |
| 4093 | Guided sweep cut succeeds but STL has open boundaries and nonmanifold edges (native 3-turn thread coupon) | n/a | STL open boundaries of a sweep, not the solid. |
| 4102 | Booleans on a FreeCAD-exported STEP solid are ~15x slower than on the same shape built in SolidCraft | n/a | Compares truck boolean time on a FreeCAD STEP with a native truck solid. |
| 4230 | The ribbon does not show the selection status properly: The Sheet Metal tab does not follow the standard pattern to show its status | n/a | Sketch, sheet metal, UI, naming, export, or truck-only from the title. |
| 4260 | Pattern dialogs: the Quantity field has no step arrows, so a count can only be typed | n/a | Pattern quantity spin buttons. |
| 4285 | In Offset command, only Distance 1 can be controlled using widget. Distance 2 has no such control. | n/a | Offset widget for a second distance. |
| 4345 | Offset Plane created from a face loses its reference and snaps to the origin | n/a | Offset plane reference. Construction. |
| 4384 | Placing a feature on a face: one position input for Hole, Boss, Snap Fit, Rest and the primitives | n/a | One placement field for several features. UX proposal. |
| 4440 | Sheet metal: Flat Pattern view doesn't draw perforations | n/a | Sketch, sheet metal, UI, naming, export, or truck-only from the title. |
| 4443 | Saved model is never used when a boolean needed a retry ("saved model: diagnostics") | n/a | Truck saved-model retry diagnostics. |
| 4468 | Measure overestimates the volume after a boolean, while the geometry is exact | n/a | Measure volume, not the boolean result. |

### UNKNOWN (625)

Auto-reports (596), body not opened. Grouped by title prefix.

- **CHAMFER** (56): 397, 438, 992, 1276, 1303, 1389, 1792, 1940, 1951, 2051, 2081, 2192, 2213, 2215, 2230, 2383, 2394, 2408, 2472, 2477, 2532, 2533, 2538, 2589, 2654, 2692, 2771, 2781, 2784, 2794, 2907, 2966, 3082, 3119, 3125, 3442, 3445, 3565, 3575, 3644, 3694, 3747, 3763, 3853, 4047, 4066, 4157, 4235, 4265, 4366, 4428, 4429, 4498, 4543, 4569, 4575
- **CIRCULAR PATTERN** (14): 2450, 2524, 2885, 3279, 3302, 3608, 3725, 3727, 3753, 3811, 3816, 4280, 4448, 4593
- **COMBINE** (32): 864, 1025, 1029, 1471, 1511, 1965, 2161, 2305, 2345, 2407, 2503, 2527, 2565, 2657, 2765, 2911, 2941, 3007, 3019, 3026, 3212, 3232, 3251, 3411, 3428, 3633, 4218, 4313, 4316, 4317, 4530, 4532
- **CRASH** (12): 1123, 1349, 1351, 2318, 2350, 2395, 3066, 3473, 3962, 4286, 4542, 4547
- **DRAFT** (3): 2586, 2766, 3176
- **EXTRUDE** (185): 206, 226, 325, 456, 458, 522, 586, 632, 679, 746, 858, 887, 938, 953, 990, 999, 1002, 1021, 1125, 1133, 1137, 1216, 1253, 1343, 1360, 1436, 1440, 1451, 1467, 1504, 1509, 1602, 1633, 1662, 1672, 1740, 1765, 1807, 1834, 1848, 1882, 1944, 1946, 1963, 1964, 2048, 2059, 2104, 2143, 2148, 2196, 2234, 2268, 2281, 2282, 2285, 2291, 2340, 2362, 2370, 2400, 2418, 2454, 2461, 2466, 2495, 2522, 2525, 2534, 2543, 2551, 2553, 2601, 2602, 2610, 2740, 2750, 2754, 2798, 2799, 2803, 2873, 2887, 2949, 2950, 2965, 2970, 2973, 3038, 3051, 3056, 3079, 3090, 3101, 3112, 3117, 3132, 3138, 3147, 3149, 3157, 3192, 3193, 3194, 3227, 3255, 3271, 3293, 3303, 3374, 3396, 3403, 3406, 3426, 3427, 3448, 3456, 3496, 3498, 3531, 3539, 3547, 3607, 3645, 3648, 3649, 3656, 3657, 3667, 3703, 3707, 3709, 3734, 3741, 3757, 3767, 3777, 3787, 3789, 3791, 3793, 3808, 3823, 3866, 3869, 3904, 3926, 3932, 3948, 3961, 3978, 4017, 4036, 4041, 4075, 4111, 4124, 4151, 4191, 4195, 4210, 4211, 4238, 4249, 4262, 4278, 4289, 4322, 4347, 4348, 4355, 4361, 4383, 4401, 4408, 4413, 4424, 4437, 4477, 4516, 4523, 4534, 4551, 4597, 4600
- **FILLET** (122): 190, 199, 215, 691, 828, 896, 903, 984, 989, 1001, 1039, 1370, 1557, 1572, 1573, 1719, 1728, 1774, 1875, 1876, 1933, 1945, 1955, 2042, 2083, 2116, 2128, 2135, 2170, 2189, 2245, 2269, 2296, 2375, 2379, 2420, 2422, 2439, 2498, 2500, 2519, 2567, 2573, 2593, 2653, 2690, 2694, 2704, 2708, 2752, 2800, 2874, 2921, 2971, 2972, 2976, 2983, 2986, 3064, 3091, 3115, 3169, 3172, 3180, 3233, 3282, 3284, 3317, 3333, 3342, 3377, 3392, 3398, 3401, 3436, 3462, 3464, 3493, 3501, 3508, 3520, 3533, 3549, 3562, 3572, 3606, 3611, 3651, 3684, 3686, 3735, 3739, 3746, 3854, 3910, 3943, 3971, 3982, 4027, 4034, 4076, 4079, 4133, 4137, 4145, 4174, 4187, 4241, 4261, 4269, 4276, 4346, 4365, 4369, 4373, 4444, 4472, 4474, 4496, 4499, 4518, 4550
- **HOLE** (15): 367, 637, 1714, 2055, 2133, 2387, 3050, 3130, 3205, 3680, 3824, 3968, 4070, 4433, 4481
- **LOFT** (7): 1775, 2576, 2883, 3308, 4037, 4524, 4528
- **MIRROR** (11): 2003, 2159, 2328, 2541, 3211, 3632, 3652, 4042, 4077, 4240, 4458
- **OFFSET** (15): 971, 2098, 2149, 2307, 2445, 2676, 2761, 2935, 3158, 3159, 3492, 3742, 3828, 4300, 4583
- **OFFSET FACE** (7): 1996, 2239, 3283, 3290, 3650, 4331, 4492
- **OFFSET PLANE** (9): 1855, 2478, 2493, 2795, 3603, 3621, 4259, 4509, 4584
- **RECTANGULAR PATTERN** (8): 2783, 3034, 3042, 3405, 3479, 3514, 3586, 3803
- **REVOLVE** (28): 1853, 2004, 2183, 2333, 2453, 2515, 2516, 2607, 3076, 3191, 3215, 3241, 3545, 3619, 3691, 3697, 3717, 3724, 3736, 3752, 3911, 3960, 4139, 4173, 4454, 4506, 4521, 4576
- **SHELL** (48): 209, 819, 1378, 1525, 2132, 2137, 2280, 2368, 2421, 2432, 2452, 2480, 2556, 2557, 2659, 2745, 2886, 2905, 2917, 2990, 3000, 3031, 3074, 3103, 3105, 3161, 3163, 3181, 3341, 3346, 3524, 3669, 3721, 3795, 3933, 4052, 4073, 4120, 4144, 4194, 4279, 4306, 4377, 4467, 4503, 4522, 4573, 4574
- **SPLIT BODY** (10): 2566, 2959, 2979, 3100, 3118, 3518, 3602, 3830, 4123, 4184
- **SURFACE EXTRUDE** (1): 4465
- **SURFACE LOFT** (2): 3063, 3726
- **SURFACE SWEEP** (1): 2816
- **SWEEP** (10): 1402, 2005, 2216, 2442, 2656, 2954, 2993, 3006, 3345, 4305

Other UNKNOWN, title read:

| Issue | Title | Evidence | Why |
|---:|---|---|---|
| 2747 |Revolve| FissionRevolve.cpp | Title is only "Revolve". |
| 201 | Loft: Feature request x 2 | FissionLoft.cpp |  "Feature request x 2" with no geometry in the title. |
| 1379 | SHELL: operation failed | FissionShell.cpp | Shell failed, no public body in the title. |
| 1441 | Modeling: unexpected result of extrude | FissionExtrude.cpp | No public geometry. |
| 1760 | SPLIT BODY: operation failed | FissionSolidTools.cpp | If this stayed descriptive: split failed with no sizes. |
| 1969 | SHELL: operation failed  -Shell Sphere | FissionShell.cpp | Shell a sphere, no sizes. Curved shell may fail; not demonstrated. |
| 2301 | Modeling: unexpected result - Where/How to edit or remove a fillet? | FissionFillet.cpp | How to edit or remove a fillet; no geometry failure given. |
| 2686 | Modeling: Suppressing fillet on grill sample after adding extrusion fails | FissionFillet.cpp | Suppressing a fillet on a grill sample. No public model. |
| 2699 | Unexpected fillet result | FissionFillet.cpp | No public geometry. |
| 2703 | FILLET: Not implemented yet | FissionFillet.cpp |  "Not implemented yet" with no fillet kind named in the title. |
| 2706 | FILLET: Not implemented yet | FissionFillet.cpp | Same. |
| 2822 | Fillet and thread: strange behaviour | FissionHole.cpp | Fillet and thread "strange behaviour". No sizes. |
| 2997 | PATTERN ON PATH: operation failed | FissionSolidTools.cpp | Path pattern failed. No public path. |
| 3276 | Boolean join precission loss | FissionFeature.cpp |  "Join precision loss" with no amount. |
| 3326 | Lofting geometry incorrect. | FissionLoft.cpp | No public geometry in the title. |
| 3402 | Simple loft doesn't look right | FissionLoft.cpp |  "Doesn't look right" with no public sizes. |
| 3404 | Extrude command is triggered with the tab profile selected, the operation fails and no solid is created. | FissionExtrude.cpp | Tab profile selected and extrude fails. No sizes. |
| 3510 | Modeling: Split body unexpected result | FissionSolidTools.cpp | Split result called wrong, with no public sizes. |
| 3598 | Modeling: unexpected result - Extrude on extrude "chain" | FissionExtrude.cpp | Extrude-on-extrude chain. No sizes. |
| 3843 | CIRCULAR PATTERN: unexpected result | FissionTransform.cpp | Circular pattern unexpected, no sizes. |
| 3844 | EXTRUDE: Holes are not visible | FissionExtrude.cpp | Holes not visible after extrude. May be display. |
| 3879 | EXTRUDE: unexpected result | n/a | No public sizes. |
| 4213 | Extrude. | FissionExtrude.cpp | Title is only "Extrude." |
| 4222 | Fillet Error | FissionFillet.cpp | No public recipe. |
| 4412 | RECTANGULAR PATTERN: unexpected result | FissionTransform.cpp | No public sizes. |
| 4483 | EXTRUDE, COMBINE, FILET: unexpected result from .STEP file | n/a | STEP file result. No public file read. |
| 4505 | Combine operation failed | FissionSolidTools.cpp |  "Combine operation failed" with no recipe. |
| 4555 | SHELL: operation failed - bottom face of a stacked component fails | FissionShell.cpp | Bottom face of a stacked component. No sizes. |
| 4567 | SHELL: operation failed | FissionShell.cpp | No sizes. |

## Notes on the parity leftovers

`docs/solidcraft-issue-parity.md` left these unencoded. This pass:

- #3585 freeform shell: POSSIBLY-SAME (no analytic-only refusal, no selftest).
- #3570 boundary surface: outside this set if the title did not match; surface patch, not a solid op. Not a Fission shell bug.
- #3575, #3577, #3565 chamfer and #3572, #3549, #3562, #3533 fillet: UNKNOWN auto-reports. Bodies were not opened. Do not treat them as the same bug as #3435.
- #3574 offset: NOT-APPLICABLE, body is `sketch.offset`.
- #3567 offset plane: NOT-APPLICABLE, construction plane.
- #3519 thread, #3518 split, #3510 split: UNKNOWN, no public sizes.
- #3560 `document.inspect` crash: not in the solid-op set.
- #3540 coincident-cylinder extrude freeze: ALREADY-HANDLED for unbounded hang (20s). Combine is not covered.
- #3536 and #3550 are not kernel solid failures in the parity note (sketch / vague).

