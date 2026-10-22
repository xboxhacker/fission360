# SolidCraft public issue census

Public issues from [tangency-labs/solidcraft](https://github.com/tangency-labs/solidcraft/issues), fetched 2026-10-21. Snapshot only. Fission fixes after this fetch are in `CHANGELOG.md` **1.13.35**. This census was not re-fetched.
Titles and labels only. Issue bodies were skimmed when a title did not name a feature, and are not copied here.
One bucket per issue. `IGNORE-TRUCK` is Rust/truck toolchain or kernel-internal work with no user CAD-feature result (fillet, extrude, join, and similar failures stay in the feature bucket even if a truck panic is mentioned).

Total issues: **2542**

| Bucket | Count |
|---|---:|
| IGNORE-TRUCK | 14 |
| MODELING | 1483 |
| SKETCH | 218 |
| DRAWING-CAM-IO-UI | 730 |
| OTHER | 97 |

## Coverage

- GitHub GraphQL `repository.issues.totalCount` (open and closed, pull requests excluded): **2542**.
- Fetched: **2542** unique issue numbers, 26 pages of 100, **no failed pages**.
- States in the fetch: 1755 open, 787 closed.
- Search API cross-check the same day: 2542 issues (1755 open, 787 closed).
- The repository `open_issues_count` field was 2249 and includes pull requests, so it is not the issue total.

## IGNORE-TRUCK

#75 | closed | IGNORE-TRUCK | clippy -D warnings fails on macOS/Windows: unused mut in apps/solvecraft/src/main.rs
#163 | closed | IGNORE-TRUCK | 0.2.0 build guides advertise Rust 1.90 but egui 0.36 requires 1.95
#228 | closed | IGNORE-TRUCK | Panic in vendored truck-shapeops intersection_curve/mod.rs:39 (polyline[0] on empty polyline)
#287 | open | IGNORE-TRUCK | Performance: multithreading, SIMD, parallel evaluation and geometry caching (umbrella)
#331 | open | IGNORE-TRUCK | Evaluate NVIDIA usd-brep / SMLib (Apache-2.0) as the B-rep kernel
#468 | closed | IGNORE-TRUCK | CI: scan dependency advisories nightly (cargo-deny)
#669 | open | IGNORE-TRUCK | Possible direct translation from csg.js
#1105 | closed | IGNORE-TRUCK | clippy -D warnings fails on Windows: unused mut in crates/mcp (mutated only under cfg(unix))
#1400 | open | IGNORE-TRUCK | Meshing: an exact but far-out boundary parameter gets through sane() and spikes the face (minutes per face)
#1597 | open | IGNORE-TRUCK | Kernel roadmap: slow fillets and booleans, and where to help
#2661 | open | IGNORE-TRUCK | Tests that survive the move to solidcore: black-box coverage gaps in the kernel
#2670 | open | IGNORE-TRUCK | cargo xtask ci runs the nightly-only boolean_props tests, so the local gate fails on a clean main
#3035 | open | IGNORE-TRUCK | kernel-perf: shared intersection edges are sampled repeatedly during healing and validation
#3992 | open | IGNORE-TRUCK | Kernel work is paused while we evaluate a new geometry kernel

## MODELING

#9 | open | MODELING | Fillets: setback corners and variable radius with mid points
#10 | open | MODELING | Boolean and blend robustness (random-design fuzzer)
#12 | closed | MODELING | Measure and Offset Face treat split face pieces as one face
#13 | closed | MODELING | Debug builds: split_body fails with "Two same vertices cannot construct an edge"
#15 | closed | MODELING | View cube: chamfered corners; faces and corner facets are the only targets
#20 | closed | MODELING | Plane perpendicular to a plane (CONSTRUCT)
#21 | closed | MODELING | Split Face: use exact sketch curves
#23 | open | MODELING | Persistent naming: stress tests for splits, merges, symmetry and tiny features
#24 | closed | MODELING | Booleans: report when a fallback (jitter/nudge) was needed, and by how much
#25 | open | MODELING | Reference tests: compare shapes spatially, not just volume/area/counts
#27 | open | MODELING | Acceptance project: a sensor enclosure / controller mounting assembly, edited end to end
#29 | closed | MODELING | Plastic Lip fails on a rounded (filleted) shelled rim: coincident faces on curved bodies
#30 | closed | MODELING | Profiles picked in the Extrude dialog are stored by index: a face sketch renumbers them on recompute (cut becomes a new body)
#37 | open | MODELING | No constraints for hole tool
#41 | closed | MODELING | Snap Fit (and other point-placed plastic features) become a floating body when their face moves
#42 | closed | MODELING | Suppressing a patterned feature keeps the pattern's copies
#52 | open | MODELING | Fillet corners: 6-sided setback patch for three unequal radii
#58 | open | MODELING | Saddle fillet between two cylinders isn't an exact rolling-ball surface (~0.05 mm)
#59 | open | MODELING | Flat hem folds to the wrong side of the flange
#63 | open | MODELING | Parametric Mesh Conversion
#68 | closed | MODELING | Cannot extrude top of chamfer.
#69 | closed | MODELING | Cut extrude of a smaller circle inside an earlier pocket restores the pocket (wrong volume, no error)
#78 | closed | MODELING | XZ plane is oriented opposite to Fusion's (normal −Y vs +Y)
#114 | open | MODELING | Boolean Union fails when extruding overlapping cylindrical faces
#133 | open | MODELING | 'Mesh' Tab Unavailable
#135 | closed | MODELING | Join merges the tool into the first target body only; other bodies it meets stay separate and overlap
#136 | closed | MODELING | Kernel-reported exact face history (replace sampled naming)
#137 | open | MODELING | Tight Bounding Box Under Properties of Body or Component and Make Length, Width, Thickness Separately Accessible Variables
#139 | open | MODELING | acceptance_sensor_enclosure is flaky under heavy CPU load (lid volume +0.15 %)
#140 | closed | MODELING | Join adds material where a touching face hangs past the other body's edge
#141 | open | MODELING | Boss ring on a 0.5° fall around a drain: the boolean returns no result
#142 | open | MODELING | Shell: saddle corners and four-face corners touching curved faces are still refused
#143 | closed | MODELING | Self-referencing construction plane overflows the stack and aborts on file open
#167 | closed | MODELING | Ability to create offset planes from surface
#168 | open | MODELING | Bodies that share an infinitesimally small border are joined.
#190 | closed | MODELING | FILLET: operation failed
#193 | closed | MODELING | Bug: Fillets dont get copied
#199 | closed | MODELING | FILLET: operation failed
#201 | closed | MODELING | Loft: Feature request x 2
#204 | open | MODELING | How to create Tanget plane?
#206 | open | MODELING | EXTRUDE: operation failed
#209 | closed | MODELING | SHELL: operation failed
#215 | closed | MODELING | FILLET: operation failed
#218 | closed | MODELING | solid.pattern.circular: expressions in axis.origin are silently stored as [0,0,0]
#219 | closed | MODELING | Rectangular pattern of [cut + fillet] fails after a parameter makes the pocket much taller ("faces at the edge ends must be planar")
#220 | closed | MODELING | Construction plane named "Top" silently resolves to world XY at z=0 in sketch.create
#226 | closed | MODELING | EXTRUDE: operation failed
#232 | closed | MODELING | Union of similar-size spheres refused: "lumps: contact work exceeds the limit" (0.3.2 regression)
#237 | closed | MODELING | Fillet refuses the straight edge between two cylinders: "a curved edge where the blend ends"
#246 | closed | MODELING | Modeling: unexpected result
#250 | closed | MODELING | T-spline surfaces: core library and N-sided Boundary Fill
#258 | open | MODELING | Cones to a point, a plain half-torus, and a freeform face put surface far off the part
#259 | closed | MODELING | Join with explicit `targets` also merges a non-target body that the tool touches
#260 | closed | MODELING | Targeted cut deletes another body that the tool fully covers
#262 | closed | MODELING | sheet.fold: bend zone reaching the blank outline is reported as "a slanted or curved edge" although every edge is straight
#263 | closed | MODELING | sheet.fold accepts a `fixed` point that is not on the sheet and silently picks a side
#264 | closed | MODELING | Sheet metal: a fold on one tab is refused when another tab lies on the same fold line; two edge flanges in one feature fail with "nonmanifold edge contact"
#265 | closed | MODELING | Renaming a pattern copy renumbers the remaining copies, so their old names stop working
#266 | closed | MODELING | Multi-profile `solid.extrude` with `body_name` names only the first body; the others get generic `BodyN` names
#268 | closed | MODELING | Panic printed from truck-topology lib.rs:186 "Two same vertices cannot construct an edge" when joining struts that end on a bore wall
#271 | closed | MODELING | Command docs omit parameters that work: `sheet.flange` `body_name`, `solid.loft` `targets`
#291 | open | MODELING | Bug Report: Floating point errors
#293 | open | MODELING | Fillet: option to fit the radius instead of refusing (reduce, later taper)
#296 | closed | MODELING | Sheet metal: cutting a hole in a base with two adjacent edge flanges fails (sew: nonmanifold edge contact)
#303 | closed | MODELING | Design files: zip container with binary blobs, and an embedded evaluated-geometry cache for instant open
#307 | closed | MODELING | Tangent hole contacts split faces inconsistently (3 of 4 symmetric contacts)
#308 | closed | MODELING | Blind cut ending inside a cylinder creates a sphere and a freeform face (regression in 4e595de)
#311 | open | MODELING | Ramp cuts distort cylindrical meshes and groove fillets create open seams in SolidCraft 0.3.2
#319 | closed | MODELING | Shell fails on a disc whose circular edge is rounded smaller than the thickness
#324 | open | MODELING | Fillet over hole feature performs wrong behaviour
#325 | closed | MODELING | EXTRUDE: operation failed
#334 | closed | MODELING | Fillet/chamfer refuses the top outline after rounding the concave edges of two cylinders
#338 | closed | MODELING | Modeling: unexpected result
#342 | closed | MODELING | Kernel: shell with a round equal to the thickness is not refused on Windows (shell_open_beside_a_round_as_large_as_the_wall_is_refused fails)
#346 | open | MODELING | CONVERT MESH: operation failed
#351 | closed | MODELING | Project fails on a cylinder's wall or a round's face: "open or invalid boundary"
#358 | open | MODELING | CONVERT MESH: operation failed
#367 | open | MODELING | HOLE: operation failed
#370 | closed | MODELING | A stored fold fails on recompute when a parameter change moves its `fixed` point off the sheet
#371 | closed | MODELING | sheet.fold still accepts a `fixed` point far above the sheet
#372 | closed | MODELING | Raising a pattern's count after renaming a copy gives the new copy the freed name
#373 | closed | MODELING | A profile added to the sketch after a named multi-profile extrude gets a generic `BodyN` name
#374 | closed | MODELING | A feature reply doesn't say which bodies the feature created
#390 | open | MODELING | solid.draft on a flange end face fails: "faces next to the drafted walls must be square to the pull direction"
#391 | open | MODELING | Text deboss on the flat flange of a filleted + shelled body fails (boolean Cut: no result)
#392 | closed | MODELING | solid.emboss with `face` on a tapered (drafted) planar wall: "`face` must identify exactly one target body surface"
#393 | closed | MODELING | joint.create: circle origin given at the circle centre fails with "no cylindrical face there" (works with a point on the edge)
#395 | open | MODELING | solid.mirror of a rectangular pattern feature: "Pat cannot be patterned or mirrored yet"
#396 | closed | MODELING | Loft joined onto a box: body_count is 1, but the STL export contains two disconnected shells
#397 | open | MODELING | CHAMFER: operation failed
#400 | closed | MODELING | Rectangular pattern of a cut whose instances touch: every instance takes the exact planar fallback (40 instances 6.5 s vs 0.04 s as one cut)
#401 | closed | MODELING | Renaming a feature breaks features that reference it by name (pattern fails with "unknown feature")
#403 | closed | MODELING | Extrude in a moved component from another component's sketch is built in the wrong place
#405 | closed | MODELING | Sheet metal: edge flanges only close at a corner when both are 90°
#416 | closed | MODELING | Sheet metal: a round hole or a slanted slot across a bend line cannot be folded
#419 | open | MODELING | EXTRUDE: operation failed
#420 | closed | MODELING | Two intersecting cylinders cause sketch and shell operations to fail
#424 | open | MODELING | Bolts glitching
#428 | closed | MODELING | To Object picks a nearby larger body instead of the face at the picked point
#431 | open | MODELING | Fillet along a frame's top outline (lines and arcs, planar top) refused: "the ball doesn't fit along the edge"
#436 | closed | MODELING | Sheet metal: an edge flange feature gives all its edges one angle and one height
#438 | open | MODELING | CHAMFER: operation failed
#439 | closed | MODELING | COMBINE: unexpected result. Moved fastener to top after creation at origin and it does not cut the body at a new location but at the old one.
#442 | closed | MODELING | inspect.interference reports no interference when a peg overlaps a small rib on a slot wall (0.36 mm³ missed)
#443 | closed | MODELING | Loft with operation intersect leaves the body unchanged when the cone meets the wall 1 mm below the top face (no error)
#446 | open | MODELING | T-spline modelling commands (create, edit, refine, convert): proposal
#450 | closed | MODELING | Revolve: a spherical shell sketched off the origin is built broken (volume +33 %)
#451 | open | MODELING | Cut with a hollow body (a spherical shell): a body inside the hollow is removed; a body crossing it can't be cut
#456 | open | MODELING | EXTRUDE: operation failed
#458 | open | MODELING | EXTRUDE: unexpected result
#466 | open | MODELING | Modeling: unexpected result
#467 | open | MODELING | Extruding from an object produces incorrect results.
#470 | closed | MODELING | Sheet metal: a sketch on a flange or base face can't be attached to it, and a frozen one doesn't follow the sheet
#473 | closed | MODELING | Modeling: unexpected result
#479 | closed | MODELING | Sheet metal: the rule's corner relief is never cut, and there is no relief shape to choose
#480 | closed | MODELING | Combine dialog: OK is always greyed out (preview reports "operation must be join, cut or intersect")
#485 | closed | MODELING | Fillet fails on outer perimeter edges, including partial selections
#487 | closed | MODELING | Revolving adjacent profiles on the axis makes zero-area faces; booleans with the body fail
#494 | closed | MODELING | Revolve, Offset Face and Boss drop every face source of the body
#495 | closed | MODELING | Sheet metal thickness faces can't take an attached sketch
#500 | open | MODELING | Cutting a coaxial cylinder from a thin disc spanning x = −1..1 loses about 2.6% of the volume
#501 | open | MODELING | Edge flange with different per-edge angles leaves open gaps at the corners
#506 | closed | MODELING | Profiles selected together leave the merged outline split where shared edges ended; booleans fail on the extra vertices
#511 | closed | MODELING | A body cut into pieces a second time gets a piece named like an existing body (duplicate body names)
#514 | open | MODELING | Modeling: Unable to refold sheetmetal part after unfolding it
#519 | open | MODELING | Thread Suggestion
#521 | open | MODELING | EXTRUDE: operation failed
#522 | open | MODELING | EXTRUDE: operation failed
#524 | open | MODELING | Threads cut into the objects
#527 | closed | MODELING | inspect.interference refuses designs with more than 200 bodies, so an assembly with its fasteners can't be clash-checked
#529 | closed | MODELING | Sheet metal: a sheet with four folds fails above 77 slot cutouts (flat geometry exceeds the bounded processing budget)
#531 | closed | MODELING | Body visibility is not saved with the design (browser.visibility is view state only)
#535 | closed | MODELING | Sheet metal: hole faces on a folded panel lose their sketch-curve names (side:c1#1 becomes F6:face:10)
#541 | closed | MODELING | Sheet metal: a primitive box/cylinder cut straight through a flat sheet is refused, though the equivalent extrude cut works
#542 | closed | MODELING | Sheet metal: sheet.flat_pattern returns round holes as sampled polylines instead of circles
#544 | closed | MODELING | Blind Ø2 hole into a long D-profile bar is refused ("exact fallback … takes planar faces only") although it stays clear of the round face
#545 | closed | MODELING | Ring cut leaving a thin floor below a recess splits a connected part into two bodies and loses volume
#546 | closed | MODELING | solid.extrude with taper refuses profiles of lines and arcs: "not supported yet: tapered profiles mixing lines and arcs"
#547 | closed | MODELING | solid.draft on a cylinder's side face fails: "a corner whose faces don't meet the same way after the draft"
#556 | closed | MODELING | Fillet or chamfer of a single edge fails where its ends meet curved, slanted or several faces
#558 | closed | MODELING | Construction geometry, Hole and Press Pull miss what is picked on a moved or pasted component
#559 | open | MODELING | A through-all Hole fails where it runs along a wall or beside curved faces (boolean Cut: no result / implausible volume)
#560 | closed | MODELING | Hole on a sheet metal face is refused ("this sheet machining cannot be preserved")
#561 | closed | MODELING | Loft, Sweep and Coil faces can't take an attached sketch
#562 | open | MODELING | Press Pull fails on faces beside rounds and curved neighbours (pushing a face: a neighbouring edge is not straight)
#566 | open | MODELING | Shell refuses swept bars and sheet metal parts (a corner whose faces don't meet the same way after the move)
#567 | closed | MODELING | Axis or tangent plane on a drill tip fails with "no face at NaN"
#568 | closed | MODELING | Construction geometry on a model edge, face or vertex is lost when a parameter moves it
#569 | closed | MODELING | Pieces that share a name can swap numbers between runs (#1/#2, ~2/~3)
#570 | closed | MODELING | Sheet metal: sheet.flange silently ignores thickness / k_factor / bend_radius and builds the part at the rule's 2.5 mm
#580 | closed | MODELING | SPHERE: unexpected result
#586 | open | MODELING | EXTRUDE: operation failed
#590 | open | MODELING | Modeling: Constrains does not prevent movement as expected.
#593 | closed | MODELING | Sheet metal: a flange on a flange's far edge misses that edge's ends, and such flanges run through each other at corners
#602 | closed | MODELING | Add threads to objects makes it impossible to modify them without crashing.
#608 | open | MODELING | Modeling: unexpected result - small edge from double cut opperation
#613 | closed | MODELING | Pasted fillet keeps the original `body`: paste with translate moves the edge picks but fillets the old body ("none of the selected edges exist any more")
#615 | open | MODELING | Extrude Cut Error: "invalid input: a boolean needs a solid body" when intersecting with mesh
#616 | closed | MODELING | Modeling: unexpected result
#617 | open | MODELING | Modeling: unexpected result
#621 | open | MODELING | Modeling: unexpected result
#622 | open | MODELING | The MCP agent's cursor does not properly deploy or unfold the submenus.
#623 | closed | MODELING | Blind bore cut into joined bosses intermittently fails: "lumps: cannot mesh a complete shell boundary" (result differs between runs)
#624 | open | MODELING | Modeling: changing an extrusion from one sided to symmetrical breaks future features
#630 | closed | MODELING | Missing Measurement Units in Input Fields (Box, Cylinder, Sphere)
#632 | open | MODELING | EXTRUDE: operation failed
#637 | closed | MODELING | HOLE: unexpected result
#638 | closed | MODELING | Sheet metal: after folding one of two collinear tabs, folding the other one is refused (follow-up to #264)
#639 | closed | MODELING | Sheet metal: a tab on an inside edge of an L-shaped blank cannot be folded when `fixed` lies on the other leg ("folding tabs that join again beyond the bend")
#640 | closed | MODELING | Blind hole into an extruded D-profile bar fails: "exact fallback ... takes planar faces only", the nudged tool reports the volume of the whole bar
#641 | closed | MODELING | Even then HOLE depth changed from negative to positive the error "negative" persists
#645 | open | MODELING | Hole view on weird angle
#647 | closed | MODELING | Convert to Sheet Metal: convert a solid or a shelled body from its fixed face and bend edges
#657 | closed | MODELING | HOLE: with "multiple" hole type center alignment to circle center on sketch works, on "single" - not
#659 | closed | MODELING | Sheet metal: a cut-out drawn with an ellipse, a spline or text becomes 0.001 mm chords, one face each
#662 | open | MODELING | Benchmark proposal: public MCP rover fixture for native opening and large-assembly interaction
#665 | closed | MODELING | Regression from #644: fuzz seed 16 (box, joined cylinder, circular pattern) fails fixed_seeds_run_clean
#672 | closed | MODELING | The hole tool gives strange solution in certain view orientations
#674 | closed | MODELING | Join a tool whose face lies partly on a face of the body and partly past its edge
#679 | closed | MODELING | EXTRUDE: operation failed
#684 | closed | MODELING | Extrude pre-picks the last sketch's already-used profile (and defaults to Cut) when nothing is selected
#685 | closed | MODELING | Regression in 0.8.0: sheet.fold refuses a fixed point that lies exactly on the sheet face ("the fixed point … is not on the sheet")
#689 | closed | MODELING | Home/Fit zoom out to where geometry used to be after moving or jointing
#691 | open | MODELING | FILLET: operation failed
#703 | closed | MODELING | Extrude from a face is refused where another body's face touches it flush ("no body face"), with no way to pick the body
#707 | closed | MODELING | A through-all hole on the ribbed lid's side wall cuts or fails from run to run
#711 | closed | MODELING | action_matrix_sample is flaky: lid_ribs_flush_with_window Lid/F11:+y face.hole fails in some runs
#716 | closed | MODELING | Changing the design's units resizes a sphere or cylinder made with a bare diameter
#721 | closed | MODELING | Extrude: thin extrude (walls of a given thickness along open or closed sketch curves)
#723 | closed | MODELING | Sheet metal: a `tear` corner relief (no relief; each bend is cut straight from the corner to its flange's end)
#726 | closed | MODELING | Revolve emits planar faces as 3 patches, so sketch/extrude on a revolved face fail until another feature re-derives the body
#732 | open | MODELING | Sheet metal: perforated panels cannot be modelled: a sheet takes ~500 round holes, a perforated panel needs tens of thousands
#744 | open | MODELING | CONVERT MESH: operation failed
#745 | open | MODELING | Offset Face refuses a planar face when the body has a freeform face anywhere, even far from the moved face
#746 | open | MODELING | EXTRUDE: operation failed
#757 | open | MODELING | Surface Extrude, Revolve, Sweep and Loft: surface bodies from open and closed curves
#758 | closed | MODELING | surface.offset: offset faces into a new surface body (0 copies them)
#759 | open | MODELING | Patch on edges: G1/G2 to neighbouring faces, non-height-field and N-sided loops, guide points
#760 | closed | MODELING | surface.extend: curved faces and edges, natural/tangent/curvature modes, up to a face
#761 | closed | MODELING | surface.untrim: remove holes and restore a face to its natural bounds
#762 | closed | MODELING | surface.ruled: ruled strips off an edge chain (normal, tangent, angle, direction)
#763 | closed | MODELING | surface.boundary: surface through 2–4 boundary curves or a curve network, G0/G1/G2 per edge
#764 | closed | MODELING | surface.blend: bridge two edge chains with G1/G2 continuity
#765 | closed | MODELING | Thicken every surface kind: cones, spheres, tori, B-splines and sharp-edged surface bodies
#766 | closed | MODELING | surface.unstitch and surface.reverse_normal
#767 | open | MODELING | Surface trim: mutual trim, trim by projected curves, keep several regions
#768 | closed | MODELING | face.delete heal: false leaves an open surface body
#769 | open | MODELING | Fillet between separate surface bodies (trim and stitch the sources)
#771 | closed | MODELING | Sweep guide rails, two-rail sweep, orientation, twist and scale
#772 | open | MODELING | Surface modelling (M8): toolset and order of work
#774 | closed | MODELING | CONVERT TO SHEET METAL: operation failed
#775 | closed | MODELING | Fillet of a shell's inner corners follows the pick point onto a smooth seam after the wall gets thinner than the outer round
#783 | closed | MODELING | Fillet and chamfer accept edge picks any distance from an edge and silently modify the nearest edge (15 mm / 30 mm off, or inside the solid)
#787 | open | MODELING | Add: Standard Components Library
#788 | closed | MODELING | Cutting a hole that crosses an equal-radius hole at right angles is refused ("would change the body away from the tool")
#793 | open | MODELING | A coil can't be split (or cut) by a plane across its axis: "boolean Intersect: no result"
#794 | open | MODELING | SHELL: unexpected result
#796 | closed | MODELING | Kernel: fine tessellation of a fillet tapering to zero loses volume on Windows (taper_follows_the_room_along_the_edge fails)
#814 | closed | MODELING | Chamfer of a hex hole's vertical corners cuts a notch at each corner instead of one bevel
#818 | open | MODELING | A blind cut splits every vertical edge at the cut depth, and the fillet corners render black
#819 | closed | MODELING | Shell: unexpected result
#822 | closed | MODELING | Section analysis not in browser panel
#823 | open | MODELING | EXTRUDE to CUT object failed
#827 | closed | MODELING | Cylinder union fails on filleted laptop lid / hinge mounting leaf (MCP reproduction)
#828 | closed | MODELING | FILLET: operation failed
#831 | open | MODELING | Tapered extrude: text/spline profiles (misleading "mixing lines and arcs" error) and arcs meeting lines at a corner
#851 | closed | MODELING | Fillets and chamfers fail where edges meet at corners on a curved wall or a knife-edge tip
#858 | open | MODELING | EXTRUDE: operation failed
#859 | open | MODELING | Modeling: unexpected result
#864 | open | MODELING | COMBINE: operation failed
#871 | closed | MODELING | Modeling: unexpected result
#873 | open | MODELING | Error when attempting to extrude an object next to a screw
#878 | closed | MODELING | Windows: robustness golden seed 0 differs in area, edges and volume during render integration validation
#881 | closed | MODELING | Loft between two polygon sections with the same vertex count pairs vertices wrongly (twisted body, volume 27% low)
#882 | closed | MODELING | Cut through the open rim of a shelled body fails after an R1 fillet on the opposite end face ("exact fallback takes planar faces only")
#884 | closed | MODELING | R1 fillet on one outer end loop refused after R5 ("curve blend: could not fit the rolling-ball surface"), even on a single straight edge between planar faces
#887 | open | MODELING | EXTRUDE: operation failed
#892 | closed | MODELING | LIP: operation failed
#896 | closed | MODELING | FILLET: operation failed
#897 | closed | MODELING | Shell refuses robustness seed 14 since the 13 October kernel merges
#903 | open | MODELING | FILLET: operation failed
#928 | closed | MODELING | Extrude To Object onto a body in a moved occurrence ignores the placement (stops at a flat plane through the point)
#938 | open | MODELING | EXTRUDE: operation failed
#939 | open | MODELING | OFFSET: operation failed
#944 | open | MODELING | Rectangular pattern - Feature
#950 | closed | MODELING | Hole drills the wrong way on faces of parts on the XY plane (and old designs keep it)
#951 | open | MODELING | pi-case: editing a parameter after an extrude on the inner floor loses its profile
#953 | open | MODELING | EXTRUDE: operation failed
#960 | open | MODELING | Error when attempting to shell an object
#967 | open | MODELING | Hole tool: audit and fixes
#968 | closed | MODELING | Cutting one lofted tube from another fails for some section counts
#971 | closed | MODELING | OFFSET: operation failed
#973 | closed | MODELING | Draft, Accessibility and Curvature Map analyses have no legend
#979 | open | MODELING | CIRCULAR PATTERN: unexpected result
#984 | open | MODELING | FILLET: operation failed
#987 | open | MODELING | Modeling: unexpected result
#989 | open | MODELING | FILLET: operation failed
#990 | open | MODELING | EXTRUDE: operation failed
#992 | open | MODELING | CHAMFER: operation failed
#995 | closed | MODELING | Threads keep going the wrong way when making nuts
#999 | open | MODELING | EXTRUDE: operation failed
#1001 | open | MODELING | FILLET: operation failed
#1002 | open | MODELING | EXTRUDE: operation failed
#1008 | open | MODELING | Modeling: unexpected result
#1021 | closed | MODELING | EXTRUDE: operation failed
#1025 | open | MODELING | COMBINE: operation failed (join of 23 cone/cylinder tools into a body with 20 curved faces)
#1029 | closed | MODELING | COMBINE: operation failed
#1030 | open | MODELING | FILLET: unexpected result
#1035 | closed | MODELING | Pasting a cylinder Emboss fails: "selected cylinder face has no persistent identity"
#1036 | closed | MODELING | Right-clicking a construction plane in the browser opens no menu
#1039 | open | MODELING | FILLET: operation failed, Solidworks Modelmania 2022 YT Video, R12mm
#1047 | open | MODELING | Add the option to move the hole
#1080 | closed | MODELING | Fillets and chamfers too large for their neighbours are accepted instead of refused
#1091 | closed | MODELING | Face commands take the names `model.faces` lists; `model.faces` gives a point on each face and a cylinder's axis and radius
#1092 | closed | MODELING | Convert to Sheet Metal: bent parts as other systems model them (radius or chamfer bends, holes and labels in their faces) can't be converted
#1106 | open | MODELING | Modeling: unexpected result
#1107 | open | MODELING | solid.combine of hollow hub + 9 lofted blades fails: 'cavity ownership is ambiguous' (1-2 tools work)
#1108 | closed | MODELING | body.rename does not update features that reference the body in 'targets' (unknown body after rename)
#1109 | closed | MODELING | Renaming auto-named bodies renumbers other features' auto-named bodies (Body3 becomes Body1)
#1113 | closed | MODELING | Kernel: radius-10 fillet on arc top edge returns an unmeshable face
#1120 | open | MODELING | Modeling: unexpected result
#1125 | open | MODELING | EXTRUDE: operation failed
#1126 | open | MODELING | Modeling: unexpected result - increasing a draft angle doesnt cut off properly
#1133 | open | MODELING | EXTRUDE: operation failed
#1134 | open | MODELING | Pattern/mirror seed refs (features: Vec<String>) are name-keyed while every other feature reference is id-keyed
#1137 | open | MODELING | EXTRUDE: operation failed
#1140 | open | MODELING | Cutting a hollow lofted tube in half fails with "no result": a vertex lands a hair inside an edge end
#1155 | open | MODELING | Modeling: unexpected result
#1160 | closed | MODELING | Hide and show joints in the browser, like other items
#1183 | open | MODELING | Tapered hexagon pockets fail on a plate with 10 mm corner fillets (8 mm works)
#1184 | closed | MODELING | A loft section that is a ring (a profile with a hole) builds a solid, with no error
#1191 | open | MODELING | Modeling: unexpected result
#1200 | open | MODELING | CONVERT MESH: operation failed
#1202 | open | MODELING | extrusion problem
#1207 | open | MODELING | Fillet refused on the edge where a flat face meets a cylindrical boss (partial arc edge)
#1209 | closed | MODELING | Persistent face names: a square cut through one boss renames part of that boss's outer cylinder as a wall of the cut
#1212 | closed | MODELING | Extruding a floor into a hole it fills fails: "boolean Union: the operation failed: no result"
#1213 | closed | MODELING | A round or chamfer on the end circle of a bent lofted tube is refused: "a curved seam where the edge chain turns"
#1214 | open | MODELING | Materials: bounded body-local visual mapping frame and grain-axis suggestion
#1216 | open | MODELING | EXTRUDE: operation failed
#1225 | open | MODELING | Modeling: no possible to add taper angle for extrude
#1229 | closed | MODELING | Section analysis cuts into the origin planes
#1231 | open | MODELING | Extrude cut fails when cutting Sheet Metal part
#1232 | closed | MODELING | Sheet metal: should an Edge Flange be able to choose its own corner relief?
#1234 | closed | MODELING | EXTRUDE: operation fails when cutting through multiple components
#1236 | closed | MODELING | Symmetric extrusion doubles the entered distance instead of keeping total thickness
#1239 | closed | MODELING | JOINT: don't work
#1240 | closed | MODELING | Assembly from several parts not possible
#1252 | closed | MODELING | A panic from IntersectionCurve::subs (unwrap on None) is printed while meshing, then caught
#1253 | open | MODELING | EXTRUDE: operation failed
#1257 | closed | MODELING | Smart Extrude: direction from the clicked side, Cut into a body, and no flipping to Join mid-drag
#1259 | closed | MODELING | CONVERT MESH: operation failed
#1262 | closed | MODELING | Kernel: small blend on convex boss-bottom arc is rejected
#1263 | closed | MODELING | Kernel: display planner rejects knife-edge arc fixture before filleting
#1275 | closed | MODELING | Extrude Cut from a whole face fails after a chamfer or fillet on a rounded-corner body ("boolean Cut: no result")
#1276 | open | MODELING | CHAMFER: operation failed
#1279 | open | MODELING | Cut fails with "no result" when the tool has a cylinder coaxial with an existing hole, 0.1 mm inside its wall
#1285 | closed | MODELING | Convert to Sheet Metal: a converted box ignores the rule's corner relief, and a bracket whose wall covers part of an edge is refused
#1289 | closed | MODELING | Imported STEP threads (SolidWorks) display with geometric artifacts
#1293 | open | MODELING | Chamfer on the hinge knuckle's cap circle adds material at x = 40
#1295 | closed | MODELING | Sheet metal: a cut that parts a flange from the base keeps it without a word
#1303 | closed | MODELING | CHAMFER: operation failed
#1320 | closed | MODELING | View-dependent display refinement: pick a body's display quality from its screen-space error
#1322 | open | MODELING | MOVE/COPY: operation failed
#1325 | open | MODELING | CONVERT TO SHEET METAL: operation failed
#1326 | open | MODELING | CONVERT TO SHEET METAL: operation failed
#1328 | open | MODELING | Modeling: unexpected result
#1337 | open | MODELING | Extrude crashes on complicated shape
#1343 | open | MODELING | EXTRUDE: operation failed
#1355 | open | MODELING | Concentric holes do not work
#1359 | closed | MODELING | FEATURE SUGGESTION: infinite length construction geometry lines
#1360 | closed | MODELING | EXTRUDE: operation failed
#1363 | closed | MODELING | Compare two bodies: where two versions of a part differ, without a boolean of the two
#1370 | open | MODELING | FILLET: operation failed
#1378 | closed | MODELING | SHELL: operation failed
#1379 | open | MODELING | SHELL: operation failed
#1384 | closed | MODELING | Fillet/chamfer on an inside edge: the drag arrow points the wrong way
#1389 | open | MODELING | CHAMFER: operation failed
#1390 | open | MODELING | Chamfer on a sphere cut's rim grows the part (robustness seed 57)
#1391 | open | MODELING | Joining a flange flush with the end of a bent, tapered loft fails (Union: no result)
#1392 | closed | MODELING | Measure: centre-to-centre for holes, diameters for circles, slot sizes instead of closest-edge and length
#1402 | open | MODELING | SWEEP: operation failed
#1409 | open | MODELING | EMBOSS: operation failed
#1431 | open | MODELING | Surface Tab -> Modify -> Flatten
#1436 | open | MODELING | EXTRUDE: operation failed
#1437 | open | MODELING | Modeling: unexpected result during profile extrusion
#1440 | open | MODELING | EXTRUDE: operation failed
#1441 | open | MODELING | Modeling: unexpected result of extrude
#1442 | open | MODELING | Modeling: unexpected result mesh
#1443 | open | MODELING | THREAD: operation failed
#1444 | open | MODELING | Modeling: no icons
#1451 | open | MODELING | EXTRUDE: operation failed
#1467 | open | MODELING | EXTRUDE: operation failed
#1471 | open | MODELING | COMBINE: operation failed
#1475 | open | MODELING | CONVERT TO SHEET METAL: operation failed
#1483 | closed | MODELING | Sheet metal flat pattern view can't be panned or zoomed, and the Design camera moves beneath it
#1488 | open | MODELING | Boolean retry ("tool_nudge") leaves 0.0001 mm sliver faces; every later fillet on the body then fails ("rounds would leave openings")
#1490 | closed | MODELING | Loft join into a shelled case fails ("boolean Union: no result", no retry) for wall 2.4–2.6 mm; works at 2.8 and 3.0 mm
#1492 | closed | MODELING | solid.mirror: plane {origin, normal} doesn't accept expressions, so the mirror plane can't follow a parameter
#1494 | open | MODELING | Shell refuses a saddle corner on a cylinder seam (robustness seed 6)
#1504 | closed | MODELING | EXTRUDE: operation failed
#1509 | closed | MODELING | EXTRUDE: operation failed
#1510 | closed | MODELING | Split Body fails ("boolean Intersect: no result") when the plane is coplanar with a face that runs into a through hole
#1511 | open | MODELING | COMBINE: operation failed
#1516 | open | MODELING | Multi-instance assemblies: joint picks and sub-assembly DOFs
#1525 | open | MODELING | SHELL: operation failed
#1531 | open | MODELING | Tapered extrusion fails when the result has overlapping output geometry
#1534 | closed | MODELING | Extrusion error when extruding downward.
#1535 | closed | MODELING | New tab shows the previous design's fillet preview; + adds only one blank tab
#1552 | open | MODELING | Support Sketch Entities as Splitting Tools in Split Body
#1557 | closed | MODELING | FILLET: operation failed - perpendicular fillets?
#1562 | open | MODELING | Modeling: unexpected result
#1572 | open | MODELING | FILLET: operation failed
#1573 | open | MODELING | FILLET: operation failed
#1574 | closed | MODELING | Chamfering the edge of a hole that touches the bottom face adds material below the part
#1602 | closed | MODELING | EXTRUDE: operation failed when tapering an extrusion too much
#1603 | open | MODELING | Modeling: Sheet-metal corner closure unexpectedly removes integral tray mounting hooks
#1611 | open | MODELING | Modeling: unexpected result on Section Analysis
#1617 | closed | MODELING | Box intersect fails ('no result') after a fit:reduce side fillet on a prism with a raked face (vertical face works)
#1628 | closed | MODELING | Modeling: unexpected result
#1630 | closed | MODELING | Modeling: unexpected result
#1633 | closed | MODELING | EXTRUDE: unexpected result
#1637 | closed | MODELING | A feature that names its body still looks for its named face on every body (rover `Shell2`: 6–8 s for a 6 ms shell)
#1647 | open | MODELING | Boolean: a box and a cylinder in general position, Intersect refused as "implausible result volume"
#1648 | closed | MODELING | Boolean: union of a sphere and a box on the grid leaves a face that does not mesh
#1649 | open | MODELING | Boolean: union of two crossing cylinders loses volume when both are moved 1000 mm
#1662 | open | MODELING | EXTRUDE: operation failed
#1664 | open | MODELING | Modeling: unexpected result
#1665 | closed | MODELING | Modeling: unexpected result
#1666 | open | MODELING | STL/3MF export of a body with loft faces isn't a valid mesh: each ruled face is two triangles, self-intersecting, and non-manifold once joined
#1668 | closed | MODELING | Fillet refuses inner + outer rounds of a thin wall in one feature ("largest radius that fits is 4 mm"), but the same two rounds as two features work
#1669 | open | MODELING | Mirror of a cut + loft join fails at wall 3.5 mm ("boolean Union: no result") while the same features built directly succeed (but take 9 s)
#1672 | open | MODELING | EXTRUDE: operation failed
#1673 | open | MODELING | Modeling: unexpected result
#1674 | open | MODELING | Modeling: unexpected result
#1678 | closed | MODELING | `solid.hole`: let `position` take parameter expressions so point-placed holes follow the design
#1690 | open | MODELING | Text cut into a part with rounded edges can't be cut again or its face extruded (boolean Cut: no result)
#1691 | open | MODELING | Split Body by a plane across a chamfer's cone loses 294 mm³ with no error
#1692 | open | MODELING | Cut of the face outside a ring round a hole fails on a chamfered disc (tool tangent to the chamfer)
#1714 | closed | MODELING | HOLE: unexpected result
#1717 | open | MODELING | Cutting a sphere that overlaps an earlier sphere cut fails (robustness seed 2)
#1719 | open | MODELING | FILLET: operation failed
#1728 | open | MODELING | FILLET: operation failed
#1731 | open | MODELING | THREAD: operation failed
#1736 | open | MODELING | Co-centered hole from internal side - problems to create
#1737 | open | MODELING | Modeling: unexpected result
#1739 | closed | MODELING | Modeling: visual artifacts
#1740 | closed | MODELING | EXTRUDE: unexpected result
#1760 | open | MODELING | SPLIT BODY: operation failed
#1765 | open | MODELING | EXTRUDE: operation failed
#1774 | open | MODELING | FILLET: operation failed
#1775 | open | MODELING | LOFT: operation failed
#1783 | open | MODELING | Modeling: resource limit on collectionItems : BudgetExceeded
#1788 | closed | MODELING | [Possibly Intentional] points on sketch result in separation lines on flat faces in geometry
#1792 | open | MODELING | CHAMFER: operation failed
#1793 | open | MODELING | MOVE: operation failed
#1794 | open | MODELING | THREAD: operation failed
#1797 | closed | MODELING | Sheet metal: a torn bend end or a slanted cut-out across a bend folds to 32 cubic pieces, which viewers draw as a fan of lines
#1801 | closed | MODELING | Modeling: unexpected result
#1805 | closed | MODELING | Extrude: show a New Body preview in a different colour from Join
#1807 | open | MODELING | EXTRUDE: operation failed
#1834 | closed | MODELING | EXTRUDE: operation failed
#1835 | closed | MODELING | fix(skills): refresh Boolean checking facts that fail on the current release
#1845 | closed | MODELING | Midplane runs with no planes selected and reports that `a` is required
#1848 | open | MODELING | EXTRUDE: operation failed
#1853 | closed | MODELING | REVOLVE: operation failed
#1855 | closed | MODELING | OFFSET PLANE: operation failed
#1865 | open | MODELING | Union of two solids that meet along cylindrical faces fails: the exact fallback is planar-only
#1875 | open | MODELING | FILLET: operation failed
#1876 | open | MODELING | FILLET: operation failed
#1882 | open | MODELING | EXTRUDE: operation failed
#1883 | closed | MODELING | Modeling: unexpected result
#1888 | closed | MODELING | Bill of materials: part properties on components and an assembly.bom command
#1889 | closed | MODELING | Clearance report: overlaps, near pairs and nearest bodies in one call
#1924 | open | MODELING | Modeling: unexpected result
#1926 | open | MODELING | Cut operation not working
#1933 | open | MODELING | FILLET: operation failed
#1939 | open | MODELING | PLANE AT ANGLE: unexpected result
#1940 | open | MODELING | CHAMFER: operation failed
#1944 | open | MODELING | EXTRUDE: operation failed
#1945 | open | MODELING | FILLET: operation failed
#1946 | open | MODELING | EXTRUDE: operation failed
#1947 | closed | MODELING | Modeling: unexpected result
#1951 | open | MODELING | CHAMFER: operation failed
#1953 | closed | MODELING | Modeling: unexpected result
#1954 | closed | MODELING | Modeling: unexpected result
#1955 | open | MODELING | FILLET: operation failed
#1958 | open | MODELING | RECTANGULAR PATTERN: unexpected result
#1963 | open | MODELING | EXTRUDE: operation failed
#1964 | open | MODELING | EXTRUDE: operation failed
#1965 | open | MODELING | COMBINE: operation failed
#1967 | open | MODELING | Modeling: unexpected result
#1969 | open | MODELING | SHELL: operation failed -Shell Sphere
#1970 | open | MODELING | Modeling: unexpected result
#1972 | open | MODELING | SHELL: unexpected result shelling a half Sphere applied the Shell to the hidden half
#1979 | closed | MODELING | Sheet metal: a cut after a text cut is dropped, and many holes or labels take seconds per cut
#1983 | open | MODELING | Booleans stop terminating after a faceted-fallback result enters the body
#1986 | open | MODELING | Chamfer refused on an arc edge: a prism join leaves one cylindrical wall as a stack of faces
#1987 | closed | MODELING | Modeling: unexpected result
#1988 | open | MODELING | Modeling: In the loft operation, hidden bodies are merged.
#1990 | closed | MODELING | solidcraft-cli script output fails to replay when a sketch and its extrude share a name
#1994 | open | MODELING | SNAP FIT: unexpected result
#1995 | open | MODELING | Modeling: unexpected result
#1996 | open | MODELING | OFFSET FACE: operation failed
#2003 | open | MODELING | MIRROR: operation failed
#2004 | open | MODELING | REVOLVE: operation failed
#2005 | closed | MODELING | SWEEP: operation failed
#2009 | open | MODELING | After operations involving a flange, reusing the flange does not work properly.
#2015 | closed | MODELING | Modeling: no normal sized preferences window when changing font
#2038 | open | MODELING | Modeled thread: second thread on same body fails, combine of two threaded bodies hangs
#2040 | open | MODELING | Extrude from a face rounded by fillets: cut, join and taper all fail with boolean no result
#2042 | open | MODELING | FILLET: operation failed
#2046 | open | MODELING | RECTANGULAR PATTERN: operation failed
#2048 | open | MODELING | EXTRUDE: operation failed
#2051 | open | MODELING | CHAMFER: operation failed
#2055 | open | MODELING | HOLE: operation failed
#2056 | closed | MODELING | Rib/Web: choose the direction (in the sketch plane or along the sketch normal)
#2059 | closed | MODELING | EXTRUDE: operation failed
#2070 | open | MODELING | Find hole stacks: holes that line up through touching parts
#2071 | open | MODELING | Fastener families from a part library file
#2072 | open | MODELING | Fill hole stacks with fasteners in one command
#2079 | open | MODELING | Tracker: modelled threads
#2081 | open | MODELING | CHAMFER: operation failed
#2083 | open | MODELING | FILLET: operation failed
#2091 | open | MODELING | INSERT PART: unexpected result
#2096 | open | MODELING | Modeling: Overlapping tool names.
#2098 | open | MODELING | OFFSET: operation failed
#2099 | open | MODELING | Copy option does not exist on it's own
#2100 | open | MODELING | Instead, "PRESS PULL" opens “EXTRUDE”
#2103 | open | MODELING | Extrude cut preview drawn in the wrong place for a sketch on a face in a mirrored STEP component (result is correct)
#2104 | open | MODELING | EXTRUDE: unexpected result
#2110 | closed | MODELING | Hole: placement "single" not updating as expected
#2115 | open | MODELING | Threaded Holes not rendering or creating correctly
#2116 | open | MODELING | FILLET: operation failed
#2117 | open | MODELING | SECTION ANALYSIS: unexpected result
#2121 | open | MODELING | Circular pattern of a feature not updating body geometry after change
#2122 | open | MODELING | Modeling: unexpected result
#2128 | open | MODELING | FILLET: operation failed
#2130 | open | MODELING | Modeling: visual artifacts
#2132 | closed | MODELING | SHELL: operation failed
#2133 | open | MODELING | HOLE: operation failed
#2135 | open | MODELING | FILLET: operation failed
#2137 | open | MODELING | SHELL: operation failed
#2138 | closed | MODELING | Orbit around "Point under the cursor" makes bodies disappear when the orbit starts over empty space
#2140 | open | MODELING | Modeling: unexpected result
#2143 | open | MODELING | EXTRUDE: operation failed
#2145 | open | MODELING | Modeling: unexpected result
#2146 | open | MODELING | Modeling: unexpected result
#2148 | closed | MODELING | EXTRUDE: operation failed
#2149 | open | MODELING | OFFSET: operation failed
#2152 | open | MODELING | THREAD: operation failed
#2153 | open | MODELING | Modeling: visual artifacts
#2159 | open | MODELING | MIRROR: operation failed
#2160 | open | MODELING | RENAME: operation failed
#2161 | open | MODELING | COMBINE: operation failed
#2164 | open | MODELING | Modeling: unexpected result
#2165 | closed | MODELING | Modeling: unexpected result
#2167 | open | MODELING | Delete face tool not working from the toolbar
#2169 | open | MODELING | Modeling: unexpected result
#2170 | open | MODELING | FILLET: operation failed
#2171 | open | MODELING | TIPS FOR A BETTER EXPERIENCE ABOUT HOLE FUNCTION
#2173 | open | MODELING | Delete and Delete Face issue
#2175 | open | MODELING | When renaming a body, the original body disappears, and many other bodies appear that do not actually exist.
#2178 | open | MODELING | operation SHELL fails when taper angle of previous extrusion is negative
#2179 | open | MODELING | Modeling: unexpected result
#2182 | open | MODELING | New feature request.
#2183 | open | MODELING | REVOLVE: operation failed
#2186 | closed | MODELING | Joints on holes in thin sheet lose their hole, flip sides or turn when parameters change
#2189 | open | MODELING | FILLET: operation failed
#2192 | open | MODELING | CHAMFER: operation failed
#2193 | open | MODELING | THREAD: operation failed filetage M33x3,5
#2196 | open | MODELING | EXTRUDE: operation failed
#2197 | open | MODELING | Modeling: unexpected result
#2204 | open | MODELING | Modeling: unexpected result
#2213 | open | MODELING | CHAMFER: unexpected result
#2215 | open | MODELING | CHAMFER: operation failed
#2216 | open | MODELING | SWEEP: unexpected result
#2229 | open | MODELING | Plane at Angle: a model edge can't be picked as the axis (only origin axes and sketch lines)
#2230 | open | MODELING | CHAMFER: operation failed
#2231 | open | MODELING | Cut into a face fails where its walls are tangent to the faces beside it (block with rounded ends)
#2233 | open | MODELING | Modeling: unexpected result
#2234 | open | MODELING | EXTRUDE: operation failed
#2235 | open | MODELING | CONVERT MESH: operation failed
#2236 | open | MODELING | Modeling: unexpected result
#2238 | open | MODELING | Modeling: unexpected result
#2239 | open | MODELING | OFFSET FACE: operation failed
#2245 | open | MODELING | FILLET: operation failed
#2251 | open | MODELING | Modeling: unexpected result
#2253 | open | MODELING | Modeling: unexpected result delete face does not seem to work , even on a basic cude
#2256 | open | MODELING | Modeling: unexpected result
#2258 | open | MODELING | Cutting a shelled body beside an earlier cut through its curved end fails (robustness seed 61)
#2266 | open | MODELING | Modeling: unexpected result
#2268 | open | MODELING | EXTRUDE: operation failed - Pushpull on face , that has been split not working
#2269 | open | MODELING | FILLET: operation failed
#2270 | open | MODELING | Modeling: unexpected visual result
#2272 | closed | MODELING | Sheet metal faces need names that say what they are and stay put
#2280 | open | MODELING | SHELL: operation failed (unable to select the body, only faces)
#2281 | open | MODELING | EXTRUDE: unexpected result
#2282 | open | MODELING | EXTRUDE: operation failed
#2283 | closed | MODELING | inspect.clearance reports 0.000 contact with a witness point off the body (untrimmed surface?) for a 0.2 mm coaxial hinge gap
#2285 | open | MODELING | EXTRUDE: operation failed
#2286 | open | MODELING | Agent chat: a context picker (pick elements, features and bodies, show them as chips, send them as context)
#2290 | open | MODELING | Modeling: unexpected result
#2291 | open | MODELING | EXTRUDE: operation failed
#2293 | open | MODELING | Modeling: unexpected result
#2295 | open | MODELING | Revolve Operation Body Cut Failed - 'boolean cut: the operation failed: no result'
#2296 | open | MODELING | FILLET: operation failed
#2301 | open | MODELING | Modeling: unexpected result - Where/How to edit or remove a fillet?
#2305 | open | MODELING | COMBINE: operation failed
#2307 | open | MODELING | OFFSET: operation failed
#2310 | open | MODELING | Creation of separate parts and assemblies workflow seems missing
#2312 | closed | MODELING | Extrude: a taper cannot be combined with a start offset
#2323 | open | MODELING | INSERT PART: operation failed
#2327 | closed | MODELING | Offset Plane: default the distance to 0 mm
#2328 | open | MODELING | MIRROR: operation failed
#2333 | open | MODELING | REVOLVE: operation failed
#2335 | open | MODELING | Modeling: unexpected result
#2340 | open | MODELING | EXTRUDE: operation failed
#2345 | open | MODELING | COMBINE: operation failed
#2348 | open | MODELING | Open a linked part from its assembly Browser row
#2353 | open | MODELING | Modeling: unexpected result
#2356 | open | MODELING | Flush plug over a countersunk hole: Boolean retry leaves a 0.1 µm step, and the STL/3MF export has open and non-manifold edges
#2357 | open | MODELING | Joining a ring of 90 straight segments: caught panic at truck intersection_curve.rs:133, then the join is refused after 21 min
#2360 | open | MODELING | Boolean retry on a cut extrude whose profile edges are coincident with existing wall faces
#2362 | open | MODELING | EXTRUDE: operation failed
#2367 | open | MODELING | Threaded holes: no cosmetic thread display (tapped holes look like plain holes)
#2368 | open | MODELING | SHELL: operation failed
#2369 | open | MODELING | THREAD: operation failed
#2370 | open | MODELING | EXTRUDE: operation failed
#2372 | open | MODELING | Modeling: unexpected result
#2375 | open | MODELING | FILLET: operation failed
#2376 | open | MODELING | Modeling: unexpected result
#2379 | open | MODELING | FILLET: operation failed
#2383 | open | MODELING | CHAMFER: operation failed
#2385 | closed | MODELING | A joint's offset, angle and values don't follow the parameters they were typed with
#2387 | closed | MODELING | HOLE: operation failed
#2392 | open | MODELING | A part jointed to a face spins a right angle when the face tilts past about 64°
#2394 | open | MODELING | CHAMFER: operation failed (when selecting multiple edge loops)
#2398 | open | MODELING | [UX / Feature Request] Visual feedback and face snapping for Extrude Cut depth
#2399 | open | MODELING | FLANGE: unexpected result
#2400 | open | MODELING | EXTRUDE: unexpected result - happens at 90 deg instead along the extrusion axis
#2402 | open | MODELING | Improve visual distinction between Extrude Distance and Draft Angle manipulators
#2403 | closed | MODELING | Joint origins can't follow an edge or a corner of a face
#2406 | open | MODELING | Modeling: Thread not visible
#2407 | open | MODELING | COMBINE: operation failed
#2408 | closed | MODELING | CHAMFER: operation failed
#2418 | open | MODELING | EXTRUDE: operation failed
#2419 | open | MODELING | Modeling: unexpected result
#2420 | closed | MODELING | FILLET: unexpected result
#2421 | closed | MODELING | SHELL: operation failed
#2422 | open | MODELING | FILLET: operation failed
#2423 | open | MODELING | Modeling: unexpected result Centerpoint & 3 Point arc slot (issue across all slot functions)
#2430 | open | MODELING | Copy a saved part between documents and paste it as a linked assembly component
#2432 | open | MODELING | SHELL: operation failed
#2435 | closed | MODELING | LOFT: not suported yet
#2436 | open | MODELING | Modeling: unexpected result
#2437 | open | MODELING | Section Analysis: quarter section with two planes and a clear exit action
#2439 | open | MODELING | FILLET: operation failed
#2440 | open | MODELING | Tapped Hole: select metric thread sizes and derive the tap drill diameter
#2442 | open | MODELING | SWEEP: operation failed
#2445 | open | MODELING | OFFSET: operation failed
#2450 | open | MODELING | CIRCULAR PATTERN: unexpected result
#2452 | open | MODELING | SHELL: operation failed
#2453 | closed | MODELING | REVOLVE: operation failed
#2454 | open | MODELING | EXTRUDE: operation failed
#2461 | closed | MODELING | EXTRUDE: operation failed
#2462 | closed | MODELING | Rectangular Pattern: Direction 2 has no drag arrow for Spacing 2 (sketch and solid)
#2464 | open | MODELING | Extrude Join/Cut changes hidden bodies too, and the dialog can't pick its target bodies
#2466 | open | MODELING | EXTRUDE: operation failed
#2471 | open | MODELING | Modeling: unexpected result
#2472 | open | MODELING | CHAMFER: operation failed
#2473 | open | MODELING | Modeling: unexpected result
#2474 | closed | MODELING | Plane through two edges: a is required error
#2477 | open | MODELING | CHAMFER: operation failed
#2478 | open | MODELING | OFFSET PLANE: operation failed
#2480 | open | MODELING | SHELL: operation failed
#2491 | closed | MODELING | Modeling: unexpected result
#2493 | open | MODELING | OFFSET PLANE: operation failed
#2494 | closed | MODELING | [macOS v0.15.0] Axis Through Two Planes fails with “‘a’ is required” when invoked from GUI
#2495 | open | MODELING | EXTRUDE: operation failed
#2496 | closed | MODELING | Joints can't tie only part of a placement (parallel, a point on a face…)
#2498 | closed | MODELING | FILLET: operation failed
#2500 | open | MODELING | FILLET: operation failed
#2503 | open | MODELING | COMBINE: operation failed
#2507 | open | MODELING | "Colored components" toggle
#2511 | open | MODELING | Allow selecting hidden edges while using edge-based modeling tools
#2515 | open | MODELING | REVOLVE: operation failed
#2516 | open | MODELING | REVOLVE: operation failed
#2518 | open | MODELING | MOVE/COPY: operation failed
#2519 | open | MODELING | FILLET: operation failed
#2522 | open | MODELING | EXTRUDE: operation failed
#2524 | open | MODELING | CIRCULAR PATTERN: operation failed
#2525 | open | MODELING | EXTRUDE: operation failed
#2527 | open | MODELING | COMBINE: operation failed
#2529 | open | MODELING | Modeling: unexpected result
#2531 | open | MODELING | SHELL: a face picked by point opens a second face after a parameter edit (silent, see #23)
#2532 | open | MODELING | CHAMFER: operation failed
#2533 | open | MODELING | CHAMFER: operation failed
#2534 | open | MODELING | EXTRUDE: operation failed
#2538 | open | MODELING | CHAMFER: operation failed
#2541 | open | MODELING | MIRROR: operation failed
#2543 | open | MODELING | EXTRUDE: operation failed
#2547 | open | MODELING | External file instancing for components in assemblies, aka parts in SolidWorks.
#2551 | open | MODELING | EXTRUDE: operation failed
#2553 | open | MODELING | EXTRUDE: unexpected result
#2556 | open | MODELING | SHELL: operation failed
#2557 | open | MODELING | SHELL: operation failed
#2558 | open | MODELING | Modeling: unexpected result SpaceMouse rotation
#2559 | open | MODELING | List of operand components
#2560 | open | MODELING | SAVE AS MESH: unexpected result
#2565 | open | MODELING | COMBINE: operation failed
#2566 | open | MODELING | SPLIT BODY: operation failed
#2567 | open | MODELING | FILLET: operation failed
#2573 | open | MODELING | FILLET: operation failed
#2574 | open | MODELING | Hexagonal Pattern
#2575 | open | MODELING | Modeling: unexpected result
#2576 | open | MODELING | LOFT: operation failed
#2581 | closed | MODELING | Modeling: Difficult Selection of Origin Planes during Mirroring
#2586 | open | MODELING | DRAFT: operation failed
#2587 | closed | MODELING | Modeling: axis trough cylinder
#2588 | open | MODELING | SPLIT BODY: unexpected result
#2589 | open | MODELING | CHAMFER: operation failed
#2591 | open | MODELING | Cut ending exactly on a face leaves a zero-thickness double disc when it reaches a fillet's tangent edge (0.15.0 regression)
#2593 | open | MODELING | FILLET: operation failed
#2597 | closed | MODELING | A point joint origin ignores `x`, so a turned part turns back when jointed
#2599 | closed | MODELING | `joint.as_built` at a face or hole origin doesn't follow that face or hole
#2601 | open | MODELING | EXTRUDE: operation failed
#2602 | open | MODELING | EXTRUDE: operation failed
#2606 | open | MODELING | PLANE AT ANGLE: operation failed
#2607 | open | MODELING | REVOLVE: operation failed
#2610 | open | MODELING | EXTRUDE: operation failed
#2616 | open | MODELING | Orbit around "Point under the cursor": body jumps off screen when starting the orbit away from it, only far from the origin (follow-up to #2138)
#2617 | open | MODELING | Modeling: unexpected result
#2653 | open | MODELING | FILLET: operation failed
#2654 | open | MODELING | CHAMFER: operation failed
#2656 | open | MODELING | SWEEP: operation failed
#2657 | open | MODELING | COMBINE: operation failed
#2659 | open | MODELING | SHELL: operation failed
#2662 | open | MODELING | Black-box coverage: tessellation and the display mesh (0 of 19 sampled mutants caught)
#2663 | open | MODELING | Black-box coverage: blends: fillets, chamfers and their corners (6 of 41 sampled mutants caught)
#2665 | open | MODELING | Black-box coverage: booleans and extrude-type features (10 of 44 sampled mutants caught)
#2666 | open | MODELING | Black-box coverage: offset, shell, emboss and lip (0 of 14 sampled mutants caught)
#2667 | open | MODELING | Black-box coverage: primitives, profiles and sweeps (2 of 11 sampled mutants caught)
#2668 | open | MODELING | Black-box coverage: measure, body validity and topology (5 of 26 sampled mutants caught)
#2669 | open | MODELING | Robustness seed 71 builds 19 faces instead of 20 on macOS arm64
#2672 | closed | MODELING | Chamfer: one pick stops following the tangent chain once the part is over ~200 mm across (fillet unaffected)
#2673 | open | MODELING | Pill sketch revolve
#2676 | open | MODELING | OFFSET: operation failed. TEXT Offset too complex.
#2680 | closed | MODELING | Assembly UI: change the size of an inserted standard part
#2684 | open | MODELING | eval_more: one value for the face that face_loops, face_outline and plate_recesses measure
#2686 | open | MODELING | Modeling: Suppressing fillet on grill sample after adding extrusion fails
#2688 | closed | MODELING | Improve Error Message When Creating Threads with Incorrect Hole Diameter
#2690 | open | MODELING | FILLET: operation failed
#2691 | open | MODELING | Modeling: unexpected result
#2692 | open | MODELING | CHAMFER: unexpected result
#2693 | open | MODELING | Modeling: unexpected result
#2694 | open | MODELING | FILLET: operation failed
#2695 | open | MODELING | CHAFER: Blend is larger than walls
#2699 | open | MODELING | Unexpected fillet result
#2700 | closed | MODELING | Extrude: distance2 is ignored when the first side is through_all
#2703 | open | MODELING | FILLET: Not implemented yet
#2704 | open | MODELING | FILLET: operation failed
#2705 | open | MODELING | Thread: "the face is too far from the thread's diameter" on a slightly oversized shaft or undersized hole
#2706 | open | MODELING | FILLET: Not implemented yet
#2707 | open | MODELING | measure: volume of a round reads up to 9 % high, and the error grows with the part's size, not the feature's
#2708 | open | MODELING | FILLET: operation failed
#2709 | closed | MODELING | A joint between two faces that already meet can spin the part a half turn
#2711 | closed | MODELING | No manipulator for Distance 2 on Chamfer Type "Two Distances"
#2714 | closed | MODELING | Clearance: a pin in a bore of its own size reads "not verified" at some placements
#2719 | open | MODELING | Chamfer as wide as the round it runs past makes a sphere patch whose edges are off its surface (bad B-rep in STEP)
#2721 | open | MODELING | Modeling: unexpected result
#2727 | open | MODELING | 3d scan and direct modelling
#2729 | open | MODELING | In Sheet Metal mode, allow user to split an edge and apply flanges or other features independently.
#2730 | open | MODELING | INSERT PART: operation failed
#2731 | open | MODELING | INSERT PART: operation failed
#2732 | open | MODELING | Enhance the customizability of flanges (add properties and design parameters)
#2737 | open | MODELING | Construct: a Plane Through Three Points dialog that can pick sketch crossings
#2740 | open | MODELING | EXTRUDE: unexpected result
#2745 | open | MODELING | SHELL: operation failed
#2746 | open | MODELING | face move
#2747 | open | MODELING | Revolve
#2750 | closed | MODELING | EXTRUDE: operation failed
#2751 | closed | MODELING | Lip and groove on round object: graphical bug
#2752 | open | MODELING | FILLET: operation failed
#2754 | open | MODELING | EXTRUDE: operation failed
#2755 | closed | MODELING | Pattern and Mirror copies of a primitive cut or join every body they meet, ignoring the seed's targets
#2759 | closed | MODELING | Joints: a round origin jumps to the other end when the face across its rim gets new neighbours
#2761 | open | MODELING | OFFSET: operation failed
#2765 | open | MODELING | COMBINE: operation failed
#2766 | open | MODELING | DRAFT: operation failed
#2767 | open | MODELING | Modeling: unexpected result
#2769 | open | MODELING | SAVE AS MESH: unexpected result
#2770 | open | MODELING | Join extrude of a rim ring on a Gridfinity-style base fails after a minute: boolean Union: no result
#2771 | open | MODELING | CHAMFER: operation failed
#2772 | closed | MODELING | Relink a linked assembly component after its source design moves
#2774 | open | MODELING | Feature patterns: blend copied fillets and chamfers once for all copies, and name copies' blend faces after the source's
#2781 | open | MODELING | CHAMFER: operation failed
#2783 | open | MODELING | RECTANGULAR PATTERN: operation failed
#2784 | open | MODELING | CHAMFER: operation failed
#2794 | open | MODELING | CHAMFER: operation failed
#2795 | open | MODELING | OFFSET PLANE: operation failed
#2797 | open | MODELING | Chamfer: Two Distances / Distance+Angle not supported on plane–cylinder edges
#2798 | open | MODELING | EXTRUDE: operation failed
#2799 | open | MODELING | EXTRUDE: operation failed
#2800 | closed | MODELING | FILLET: operation failed
#2803 | open | MODELING | EXTRUDE: unexpected result
#2804 | open | MODELING | Assembly: first part grounded by default; drag a free part to reposition it
#2810 | closed | MODELING | Tapered extrude: a loop of arcs meeting at corners gets wrong walls, without an error
#2816 | open | MODELING | SURFACE SWEEP: unexpected result
#2818 | open | MODELING | Modeling: unexpected result
#2820 | open | MODELING | no cut or hollow option in pipe
#2822 | open | MODELING | Fillet and thread: strange behaviour
#2824 | open | MODELING | Modeling: unexpected result
#2826 | closed | MODELING | Cut up to a face leaves the hole closed by a zero-thickness skin
#2843 | open | MODELING | Modeling: Unable to modify text for extrude numeric commands or report a bug
#2844 | open | MODELING | Join onto a perforated plate splits a cylinder wall into full-height strip faces (64 instead of 2)
#2846 | open | MODELING | Save as Mesh: Add refinement options for surface deviation
#2847 | closed | MODELING | Loft through rotated non-circular sections folds the solid once they span more than ~90° (volume 6% of expected at 180°, no error)
#2851 | open | MODELING | Union fails when a sphere is joined tangent to a cylinder's end (same diameter, centred on the cap): "exact fallback… takes planar faces only"
#2857 | open | MODELING | Modeling: unexpected result
#2862 | open | MODELING | Joining three pipes that meet at one point fails (non-planar Y junction): truck-geometry panics at intersection_curve.rs:133, then "Union: no result"
#2865 | closed | MODELING | inspect.clearance meshes a large body again for every pair
#2873 | open | MODELING | EXTRUDE: unexpected result. Unable to Press Pull on rounded faces.
#2874 | open | MODELING | FILLET: unexpected result
#2880 | open | MODELING | No seperate feature tree for individual components + featrure "display component colors"
#2882 | closed | MODELING | Modeling: unexpected result. Holes are being projected to a different body
#2883 | open | MODELING | LOFT: unexpected result
#2885 | open | MODELING | CIRCULAR PATTERN: operation failed
#2886 | open | MODELING | SHELL: operation failed
#2887 | open | MODELING | EXTRUDE: operation failed
#2890 | open | MODELING | cant move hole freely like move object, during add hole
#2892 | open | MODELING | THREAD: operation failed
#2903 | open | MODELING | Modeling: unexpected result problemas filettatura, crea poligoni
#2904 | closed | MODELING | solid.hole also processes a distant body, and its retry silently drops part of a countersink ("not verified ... that part is ignored")
#2905 | open | MODELING | SHELL: operation failed
#2907 | open | MODELING | CHAMFER: operation failed
#2911 | open | MODELING | COMBINE: operation failed
#2917 | open | MODELING | SHELL: operation failed
#2921 | open | MODELING | FILLET: operation failed
#2926 | open | MODELING | Modeling: unexpected result
#2927 | open | MODELING | Modeling: clicking on a different linetype removes the focus from the input fields.
#2929 | closed | MODELING | Flaky tests: apply_off_thread tests can miss the computing state when the window thread is held up
#2935 | closed | MODELING | OFFSET: unexpected result
#2939 | open | MODELING | I cannot offset round face
#2941 | open | MODELING | COMBINE: operation failed
#2945 | open | MODELING | Boolean of an imported part with a small B-spline face and a large plate gives up (intersection curve refused, open shell, inside-out result)
#2947 | open | MODELING | Boolean gives up ("loop not inside any face piece") when a revolved face holds its surface's seam
#2949 | open | MODELING | EXTRUDE: unexpected result
#2950 | open | MODELING | EXTRUDE: operation failed
#2953 | open | MODELING | Modeling: unexpected result
#2954 | open | MODELING | SWEEP: operation failed
#2959 | open | MODELING | SPLIT BODY: unexpected result
#2963 | open | MODELING | Cut leaves a zero-thickness face when the tool's end face lies on the part's side wall plane
#2965 | open | MODELING | EXTRUDE: unexpected result
#2966 | closed | MODELING | CHAMFER: operation failed
#2969 | open | MODELING | THREAD: operation failed
#2970 | open | MODELING | EXTRUDE: operation failed
#2971 | open | MODELING | FILLET: operation failed
#2972 | open | MODELING | FILLET: operation failed
#2973 | open | MODELING | EXTRUDE: operation failed
#2976 | open | MODELING | FILLET: unexpected result
#2977 | open | MODELING | Press Pull: unexpected result
#2979 | open | MODELING | SPLIT BODY: operation failed
#2981 | open | MODELING | Modeling: unexpected result
#2982 | open | MODELING | Modeling: unexpected result
#2983 | open | MODELING | FILLET: operation failed
#2984 | open | MODELING | Extruding Sketch with Browser List misbehaviour
#2986 | open | MODELING | FILLET: operation failed
#2987 | open | MODELING | FLANGE: operation failed
#2988 | open | MODELING | Construct missing
#2990 | open | MODELING | SHELL: operation failed when shelling a loft
#2991 | open | MODELING | Prism join: a ring sunk into a perforated plate keeps short wall stubs at its foot
#2993 | open | MODELING | SWEEP: unexpected result -
#2997 | open | MODELING | PATTERN ON PATH: operation failed
#3000 | open | MODELING | SHELL: operation failed
#3001 | closed | MODELING | Modeling: mesh.convert can't select body
#3006 | open | MODELING | SWEEP: unexpected result
#3007 | open | MODELING | COMBINE: operation failed
#3014 | open | MODELING | Occurrence Move/Copy arrows are visible but do not respond to dragging
#3016 | open | MODELING | Modeling: unexpected result
#3017 | open | MODELING | Modeling: unexpected result. Edge and modify - incorrect work.
#3019 | open | MODELING | COMBINE: unexpected result
#3020 | closed | MODELING | Boolean retries: a coarse candidate whose trim edge runs off its surface is meshed without bound in the volume check
#3026 | open | MODELING | COMBINE: operation failed
#3031 | open | MODELING | SHELL: operation failed
#3034 | open | MODELING | RECTANGULAR PATTERN: unexpected result
#3038 | open | MODELING | EXTRUDE: operation failed
#3040 | open | MODELING | Joint: select persistent secondary alignment directions
#3042 | open | MODELING | RECTANGULAR PATTERN: operation failed
#3050 | open | MODELING | HOLE: operation failed
#3051 | open | MODELING | EXTRUDE: operation failed
#3053 | open | MODELING | Open saved designs instantly from a trusted geometry cache
#3056 | open | MODELING | EXTRUDE: operation failed
#3058 | open | MODELING | Modeling: unexpected result
#3062 | open | MODELING | fillet / chamfer not working as expected on round object
#3063 | open | MODELING | SURFACE LOFT: unexpected result
#3064 | open | MODELING | FILLET: operation failed
#3065 | open | MODELING | Modeling: unexpected result
#3071 | open | MODELING | A sliver face whose loop crosses itself is meshed as its whole surface (overlap volumes read too high)
#3074 | open | MODELING | SHELL: operation failed
#3075 | open | MODELING | Modeling: unexpected result when right clicking edit on fillet / round
#3076 | open | MODELING | REVOLVE: operation failed
#3078 | closed | MODELING | FLANGE: operation failed
#3079 | open | MODELING | EXTRUDE: operation failed
#3082 | open | MODELING | CHAMFER: operation failed
#3090 | open | MODELING | EXTRUDE: operation failed
#3091 | closed | MODELING | FILLET: operation failed
#3099 | closed | MODELING | Modeling: unexpected result
#3100 | open | MODELING | SPLIT BODY: operation failed
#3101 | open | MODELING | EXTRUDE: operation failed
#3103 | open | MODELING | SHELL: operation failed
#3105 | open | MODELING | SHELL: operation failed
#3110 | open | MODELING | THREAD: operation failed
#3111 | open | MODELING | LOFT: Loft does not support profiles with inner loops (holes)
#3112 | open | MODELING | EXTRUDE: operation failed
#3115 | open | MODELING | FILLET: unexpected result
#3116 | open | MODELING | Modeling: unexpected result
#3117 | open | MODELING | EXTRUDE: operation failed
#3118 | open | MODELING | SPLIT BODY: operation failed
#3119 | open | MODELING | CHAMFER: operation failed
#3125 | open | MODELING | CHAMFER: operation failed
#3126 | open | MODELING | SPHERE: unexpected result
#3128 | open | MODELING | MOVE: operation failed
#3129 | open | MODELING | Modeling: unexpected result
#3130 | open | MODELING | HOLE: operation failed
#3131 | open | MODELING | THREAD: operation failed
#3132 | open | MODELING | EXTRUDE: unexpected result
#3133 | open | MODELING | Feature confirmation rebuilds the dependent suffix on the window thread
#3137 | open | MODELING | Modeling: unexpected result
#3138 | open | MODELING | EXTRUDE: operation failed
#3141 | closed | MODELING | Modeling: unexpected result - 2 Tangent and 3 tangent circle tool doesn't create circles
#3145 | open | MODELING | Threads as fusion
#3147 | open | MODELING | EXTRUDE: operation failed
#3149 | open | MODELING | EXTRUDE: operation failed
#3154 | open | MODELING | CONVERT TO SHEET METAL: operation failed
#3156 | open | MODELING | Revolve join at an angle that puts its end faces in a target face's plane (e.g. ±90°) needs a tool_nudge retry
#3157 | open | MODELING | EXTRUDE: operation failed
#3158 | open | MODELING | OFFSET: operation failed
#3159 | open | MODELING | OFFSET: operation failed
#3161 | open | MODELING | SHELL: operation failed
#3163 | open | MODELING | SHELL: operation failed
#3165 | open | MODELING | THREAD: operation failed
#3169 | open | MODELING | FILLET: operation failed
#3170 | open | MODELING | Sheet metal: choose how flanges meet at a corner (butt, partial overlap, full overlap)
#3172 | open | MODELING | FILLET: operation failed
#3173 | closed | MODELING | Modeling: unexpected result
#3176 | open | MODELING | DRAFT: operation failed
#3180 | open | MODELING | FILLET: operation failed
#3181 | open | MODELING | SHELL: operation failed
#3183 | open | MODELING | THREAD: operation failed
#3191 | open | MODELING | REVOLVE: unexpected result
#3192 | closed | MODELING | EXTRUDE: operation failed
#3193 | open | MODELING | EXTRUDE: operation failed
#3194 | open | MODELING | EXTRUDE: operation failed
#3203 | open | MODELING | Modeling: unexpected result
#3205 | open | MODELING | HOLE: unexpected result
#3206 | open | MODELING | Modeling: unexpected result
#3211 | open | MODELING | MIRROR: operation failed
#3212 | open | MODELING | COMBINE: operation failed
#3214 | open | MODELING | Modeling: unexpected result
#3215 | open | MODELING | REVOLVE: unexpected result
#3218 | open | MODELING | Modeling: unexpected result
#3227 | open | MODELING | EXTRUDE: operation failed
#3231 | open | MODELING | Add flexible modeling
#3232 | open | MODELING | COMBINE: operation failed
#3233 | open | MODELING | FILLET: unexpected result
#3234 | closed | MODELING | In Sheet Metal mode, SC allows us to adda flange to an existing bend line, resulting in a double flange with a gap.
#3241 | closed | MODELING | REVOLVE: operation failed
#3243 | open | MODELING | solid.combine fails to union an overlapping cylindrical rim and rounded tooth
#3244 | closed | MODELING | Cylinder intersection reports success and exports geometry outside the clipping cylinder
#3245 | closed | MODELING | Joint history click leaves a stale feature selected, so Delete removes the wrong target
#3247 | open | MODELING | [Feature Request] Support Skipping Individual Instances in Rectangular Patterns
#3249 | closed | MODELING | inspect.interference and inspect.clearance silently skip mesh bodies and report no overlap
#3251 | open | MODELING | COMBINE: operation failed
#3254 | closed | MODELING | Feature preview keeps showing the last good geometry after the current inputs fail
#3255 | open | MODELING | EXTRUDE: operation failed
#3259 | open | MODELING | Modeling: unexpected result
#3270 | closed | MODELING | Allow moving bodies from child component to parent component
#3271 | open | MODELING | EXTRUDE: unexpected result
#3276 | open | MODELING | Boolean join precission loss
#3278 | open | MODELING | Modeling: unexpected result using 2-point circle tool
#3279 | open | MODELING | CIRCULAR PATTERN: operation failed
#3281 | open | MODELING | Modeling: unexpected result
#3282 | open | MODELING | FILLET: operation failed
#3283 | open | MODELING | OFFSET FACE: unexpected result
#3284 | open | MODELING | FILLET: operation failed
#3290 | open | MODELING | OFFSET FACE: operation failed
#3292 | open | MODELING | PLANE AT ANGLE: operation failed
#3293 | open | MODELING | EXTRUDE: operation failed
#3300 | open | MODELING | Assymetric Chamfer
#3302 | open | MODELING | CIRCULAR PATTERN: operation failed
#3303 | open | MODELING | EXTRUDE: operation failed
#3305 | open | MODELING | Modeling: Using OpenCode provider doesn't allow to choose model
#3307 | open | MODELING | Threaded holes not working
#3308 | open | MODELING | LOFT: operation failed
#3311 | open | MODELING | ModelingThread/tapped hole issues: hole diameter not set to tap drill size, only one modeled thread per body, faceted geometry after threading: unexpected result
#3313 | closed | MODELING | Renaming body
#3317 | open | MODELING | FILLET: operation failed
#3323 | closed | MODELING | contact.check misses two plates crossing through each other (inspect.interference finds the overlap)
#3324 | closed | MODELING | Flat pattern and DXF silently ignore non-sheet features on a sheet body
#3326 | open | MODELING | Lofting geometry incorrect.
#3330 | open | MODELING | Update Links can silently move a joint to another face of a linked component
#3332 | open | MODELING | Modeling: unexpected result
#3333 | open | MODELING | FILLET: operation failed
#3337 | closed | MODELING | THREAD: operation failed
#3341 | open | MODELING | SHELL: operation failed
#3342 | open | MODELING | FILLET: operation failed
#3345 | open | MODELING | SWEEP: unexpected result
#3346 | open | MODELING | SHELL: operation failed
#3353 | closed | MODELING | Sheet metal: a mirrored or copied sheet body loses its flat pattern
#3357 | open | MODELING | Geometry issues. File attached
#3366 | open | MODELING | THREAD: operation failed
#3367 | open | MODELING | THREAD: operation failed
#3369 | open | MODELING | Fillet refused on an edge whose ends sit on the tangent seam of a rounded face ("the faces at the edge ends must be planar")
#3370 | closed | MODELING | Fillet's Tangent Chain picks up smooth seam edges, and unticking it keeps them
#3373 | open | MODELING | Modeling: unexpected result
#3374 | open | MODELING | EXTRUDE: operation failed
#3377 | open | MODELING | FILLET: operation failed
#3378 | closed | MODELING | Circular pattern of a join that merged its targets fails with `unknown body` (regression since 0.15.0)
#3379 | open | MODELING | Circular pattern of a spoke thinner than the ring it joins fails after ~10 s with a `lumps` error
#3383 | closed | MODELING | Combine: a tool body joined into a rotated occurrence lands in the target's local frame
#3391 | open | MODELING | Driving a joint re-evaluates (nearly) the whole timeline on every pose
#3392 | open | MODELING | FILLET: operation failed
#3396 | open | MODELING | EXTRUDE: operation failed
#3398 | closed | MODELING | FILLET: operation failed
#3401 | open | MODELING | FILLET: operation failed
#3402 | open | MODELING | Simple loft doesn't look right
#3403 | open | MODELING | EXTRUDE: operation failed
#3404 | open | MODELING | Extrude command is triggered with the tab profile selected, the operation fails and no solid is created.
#3405 | open | MODELING | RECTANGULAR PATTERN: operation failed
#3406 | open | MODELING | EXTRUDE: operation failed
#3411 | open | MODELING | COMBINE: operation failed
#3417 | closed | MODELING | Sheet metal: two corner reliefs that would overlap leave one out, and which one depends on the order
#3418 | open | MODELING | SheetMetal Flat: Fillets and Chamfer not showing in flat
#3422 | open | MODELING | solid.combine (cut) does not finish on an imported B-spline-heavy STEP solid (~1,900 faces)
#3423 | open | MODELING | SNAP FIT: unexpected result
#3425 | open | MODELING | EMBOSS: operation failed
#3426 | open | MODELING | EXTRUDE: unexpected result
#3427 | open | MODELING | EXTRUDE: operation failed
#3428 | open | MODELING | COMBINE: operation failed
#3435 | open | MODELING | Fillet larger than the ring it rounds is accepted, and the body gets bigger
#3436 | open | MODELING | FILLET: operation failed
#3442 | open | MODELING | CHAMFER: operation failed
#3445 | open | MODELING | CHAMFER: unexpected result
#3448 | open | MODELING | EXTRUDE: operation failed
#3450 | open | MODELING | MOVE: unexpected result
#3454 | open | MODELING | Keyhole cut fails ('no result') on a plate with filleted rounded corners when the slot end is coaxial with the entry hole
#3456 | open | MODELING | EXTRUDE: operation failed
#3460 | open | MODELING | Combine (Cut) fails to subtract small curved snap-fit tabs from target body
#3462 | open | MODELING | FILLET: operation failed
#3464 | open | MODELING | FILLET: operation failed
#3467 | open | MODELING | THREAD: operation failed
#3468 | open | MODELING | Offset Face / Press Pull: refuses a face next to a fillet (a corner whose faces don't meet the same way)
#3471 | open | MODELING | If SC produces errors when we delete some geometry, it gets captured in the History, but these erroneous steps cannot be reproduced nor reversed
#3474 | closed | MODELING | Assembly: a closed loop that meets only to within nanometres can't be driven (stopped_by_loop at fraction 0)
#3477 | open | MODELING | Modeling: 主页模图显示有马赛克
#3478 | open | MODELING | Modeling: Always-visible plan selection
#3479 | open | MODELING | RECTANGULAR PATTERN: operation failed
#3486 | open | MODELING | MOVE: POINT to POINT / POINT to POSITION unexpected result
#3492 | open | MODELING | OFFSET: operation failed
#3493 | open | MODELING | FILLET: operation failed
#3495 | open | MODELING | Modeling: unexpected result
#3496 | open | MODELING | EXTRUDE: operation failed
#3498 | open | MODELING | EXTRUDE: operation failed
#3499 | open | MODELING | Modeling: unexpected result
#3500 | open | MODELING | FLANGE: operation failed
#3501 | open | MODELING | FILLET: operation failed
#3502 | open | MODELING | FLANGE: operation failed
#3506 | open | MODELING | Modeling: Split body cannot shell both resulting bodies on mating faces
#3507 | open | MODELING | Modeling: unexpected result
#3508 | open | MODELING | FILLET: operation failed
#3510 | open | MODELING | Modeling: Split body unexpected result
#3511 | open | MODELING | Copy and paste bodies and components between designs (tabs)
#3514 | open | MODELING | RECTANGULAR PATTERN: operation failed
#3516 | open | MODELING | [Feature Request] Edge Flange: Define Flange Profile Length Using Coplanar Vertices or Edges
#3518 | open | MODELING | SPLIT BODY: operation failed
#3519 | open | MODELING | THREAD: operation failed
#3520 | open | MODELING | FILLET: operation failed
#3521 | open | MODELING | Modeling: unexpected result
#3524 | open | MODELING | SHELL: operation failed
#3526 | open | MODELING | solid.combine intersect silently returns empty result when the target body has several separate lumps
#3528 | open | MODELING | Boolean cut near the seam of a revolved body removes too much volume without warning
#3531 | closed | MODELING | EXTRUDE: unexpected result
#3533 | open | MODELING | FILLET: operation failed
#3536 | open | MODELING | Modeling: unexpected result
#3539 | open | MODELING | EXTRUDE: operation failed
#3541 | open | MODELING | LOFT: unexpected result
#3545 | open | MODELING | REVOLVE: operation failed
#3547 | open | MODELING | EXTRUDE: unexpected result
#3549 | open | MODELING | FILLET: unexpected result
#3553 | open | MODELING | Modeling: unexpected result
#3555 | open | MODELING | Modeling: unexpected result
#3561 | open | MODELING | FILLET: operation failed
#3562 | open | MODELING | FILLET: operation failed
#3563 | open | MODELING | EXTRUDE: operation failed
#3565 | open | MODELING | CHAMFER: operation failed
#3567 | open | MODELING | OFFSET PLANE: operation failed
#3570 | open | MODELING | BOUNDARY SURFACE: operation failed
#3571 | open | MODELING | Extrution selects the wrong edge.
#3572 | open | MODELING | FILLET: operation failed
#3573 | open | MODELING | Shell of a tapered, filleted box: the inner corner fillets' edges lie 0.02 mm off their surface (STEP readers reject the faces)
#3574 | open | MODELING | OFFSET: operation failed
#3575 | open | MODELING | CHAMFER: operation failed
#3577 | open | MODELING | Modeling: unexpected result
#3578 | open | MODELING | COMBINE: operation failed
#3583 | open | MODELING | Windows: six engine tests fail on main (saved model geometry, robustness seed 36)
#3585 | open | MODELING | SHELL: operation failed
#3586 | open | MODELING | RECTANGULAR PATTERN: operation failed
#3587 | open | MODELING | CHAMFER: operation failed
#3593 | open | MODELING | Modeling: unexpected result
#3598 | open | MODELING | Modeling: unexpected result - Extrude on extrude "chain"
#3602 | open | MODELING | SPLIT BODY: operation failed
#3603 | open | MODELING | OFFSET PLANE: operation failed
#3604 | open | MODELING | Boolean cut fails for a swept G2 corner-removal profile along a projected edge (side-dependent)
#3605 | open | MODELING | Modeling: unexpected result
#3606 | open | MODELING | FILLET: operation failed
#3607 | open | MODELING | EXTRUDE: operation failed
#3608 | open | MODELING | CIRCULAR PATTERN: operation failed
#3611 | open | MODELING | FILLET: operation failed
#3618 | closed | MODELING | Renaming Split Bodies Causes Unexpected Behavior
#3619 | open | MODELING | REVOLVE: operation failed
#3621 | open | MODELING | OFFSET PLANE: operation failed
#3630 | open | MODELING | Modeling: Extruding a rounded sketch (with fillets) leads to an invisible solid -works with non-rounded sketches
#3632 | open | MODELING | MIRROR: operation failed
#3633 | open | MODELING | COMBINE: operation failed
#3634 | open | MODELING | Modeling: unexpected result, hole command auto-switch to multi-hole mode
#3635 | open | MODELING | Extrude, Hole, Loft Features should have Start / End behavior like Fusion Extrude command
#3636 | open | MODELING | COIL: operation failed
#3644 | open | MODELING | CHAMFER: operation failed
#3645 | open | MODELING | EXTRUDE: operation failed
#3648 | closed | MODELING | EXTRUDE: operation failed
#3649 | open | MODELING | EXTRUDE: operation failed
#3650 | open | MODELING | OFFSET FACE: unexpected result
#3651 | open | MODELING | FILLET: operation failed
#3652 | open | MODELING | MIRROR: operation failed
#3655 | open | MODELING | Chamfer: the same edges are refused or accepted depending on the order they're listed
#3656 | open | MODELING | EXTRUDE: operation failed
#3657 | open | MODELING | EXTRUDE: operation failed
#3665 | open | MODELING | Join extrude of a column standing on the target's floor and wall fails (Union: no result) while new body + Combine joins
#3667 | open | MODELING | EXTRUDE: operation failed
#3669 | open | MODELING | SHELL: operation failed
#3672 | open | MODELING | THICKEN: operation failed
#3674 | open | MODELING | Modeling: tangent plane
#3675 | open | MODELING | Modeling: unexpected result
#3678 | open | MODELING | Modeling: unexpected result : unmerged coplanar faces
#3679 | open | MODELING | Modeling: Unexpected movment direction with 6DOF mouse
#3680 | open | MODELING | HOLE: operation failed
#3684 | open | MODELING | FILLET: operation failed
#3686 | open | MODELING | FILLET: operation failed
#3688 | open | MODELING | Modeling: Timeline playback controls differ from Fusion 360
#3691 | open | MODELING | REVOLVE: operation failed
#3693 | open | MODELING | Modeling: Timeline "Play to end" does not work.
#3694 | open | MODELING | CHAMFER: operation failed
#3696 | closed | MODELING | can´t use origin in the move from point to point function
#3697 | open | MODELING | REVOLVE: unexpected result
#3703 | open | MODELING | EXTRUDE: unexpected result
#3707 | open | MODELING | EXTRUDE: operation failed
#3709 | open | MODELING | EXTRUDE: operation failed
#3710 | open | MODELING | THREAD: unexpected result
#3717 | open | MODELING | REVOLVE: operation failed
#3719 | open | MODELING | bendy cables
#3721 | open | MODELING | SHELL: operation failed on filleted box
#3724 | open | MODELING | REVOLVE: operation failed
#3725 | open | MODELING | CIRCULAR PATTERN: operation failed
#3726 | open | MODELING | SURFACE LOFT: unexpected result
#3727 | open | MODELING | CIRCULAR PATTERN: operation failed
#3728 | closed | MODELING | Modeling: unexpected result
#3729 | closed | MODELING | Extrude `to` a point on an edge or corner of the target face ends in the wrong place, with no error
#3732 | open | MODELING | COMBINE: La soustraction avec conservation d'outil ne produit aucun résultat
#3734 | open | MODELING | EXTRUDE: operation failed
#3735 | open | MODELING | FILLET: operation failed
#3736 | open | MODELING | REVOLVE: operation failed
#3739 | open | MODELING | FILLET: operation failed
#3741 | open | MODELING | EXTRUDE: operation failed
#3742 | open | MODELING | OFFSET: operation failed
#3746 | open | MODELING | FILLET: operation failed
#3747 | open | MODELING | CHAMFER: operation failed
#3749 | closed | MODELING | EDIT JOINT: cannot delete
#3752 | open | MODELING | REVOLVE: operation failed
#3753 | open | MODELING | CIRCULAR PATTERN: operation failed
#3757 | open | MODELING | EXTRUDE: operation failed on plane
#3763 | open | MODELING | CHAMFER: unexpected result
#3764 | open | MODELING | Extrusion Direction
#3767 | open | MODELING | EXTRUDE: operation failed
#3769 | open | MODELING | PLANE AT ANGLE: operation failed
#3772 | open | MODELING | Modeling: unexpected result
#3773 | open | MODELING | PLANE AT ANGLE: operation failed
#3774 | open | MODELING | Offset when "CUT in Combine" for better fits
#3775 | open | MODELING | PLANE AT ANGLE: unexpected result
#3777 | open | MODELING | EXTRUDE: operation failed
#3779 | open | MODELING | CHAMFER: Chamfer operation fails on spline-based geometry
#3781 | open | MODELING | Target Bodies selection for Boolean operations (Extrude, Sweep, Revolve)
#3785 | open | MODELING | Pattern along a path doesn't go around corners.
#3787 | closed | MODELING | EXTRUDE: unexpected result
#3789 | open | MODELING | EXTRUDE: operation failed
#3790 | open | MODELING | Pipes with more faces
#3791 | open | MODELING | EXTRUDE: operation failed
#3793 | open | MODELING | EXTRUDE: operation failed
#3795 | open | MODELING | Shell: unexpected result
#3798 | closed | MODELING | Modeling: unexpected result
#3803 | open | MODELING | RECTANGULAR PATTERN: operation failed
#3808 | open | MODELING | EXTRUDE: operation failed
#3811 | open | MODELING | CIRCULAR PATTERN: operation failed
#3816 | open | MODELING | CIRCULAR PATTERN: unexpected result
#3823 | open | MODELING | EXTRUDE: operation failed
#3824 | open | MODELING | HOLE: operation failed
#3828 | open | MODELING | OFFSET: operation failed
#3830 | open | MODELING | SPLIT BODY: unexpected result
#3832 | open | MODELING | COIL: operation failed
#3835 | open | MODELING | Modeling: failed
#3839 | open | MODELING | Modeling: unexpected result
#3843 | open | MODELING | CIRCULAR PATTERN: unexpected result
#3844 | open | MODELING | EXTRUDE: Holes are not visible
#3847 | closed | MODELING | Threads and pointed ends from STEP files lose their display plan and render cracked and faceted
#3853 | open | MODELING | CHAMFER: unexpected result
#3854 | open | MODELING | FILLET: operation failed
#3857 | open | MODELING | Rigid joint saved with wrong occurrence when two occurrences share a component (Adapter:2 saved as Adapter:1)
#3862 | open | MODELING | COIL: missing parameters and not editable
#3865 | open | MODELING | SELECTION: Multiple selection not working right in combine operation
#3866 | open | MODELING | EXTRUDE: operation failed
#3869 | open | MODELING | EXTRUDE: operation failed
#3879 | open | MODELING | EXTRUDE: unexpected result
#3883 | open | MODELING | Find in Browser on a body in the viewport finds its feature, not the body
#3896 | open | MODELING | Modeling: unexpected result
#3899 | closed | MODELING | Transform: a non-affine matrix is accepted and builds a body that meshes to nonsense (a later boolean runs 36 min, then "no result")
#3902 | closed | MODELING | Face names: the pieces of a split face swap #1/#2 between evaluations of the same design
#3904 | open | MODELING | EXTRUDE: unexpected result
#3906 | open | MODELING | Measuring an assembly re-measures every placement of the same part, at a very fine tessellation
#3910 | open | MODELING | FILLET: operation failed
#3911 | open | MODELING | REVOLVE: unexpected result
#3916 | open | MODELING | Modeling: unexpected result
#3926 | open | MODELING | EXTRUDE: operation failed
#3930 | open | MODELING | Modeling: unexpected result : extrusion result appears on another body
#3932 | open | MODELING | EXTRUDE: operation failed
#3933 | open | MODELING | SHELL: operation failed
#3935 | open | MODELING | MOVE: Unable to move complex component
#3936 | open | MODELING | Modeling: Loft join operation fails if one surface is circular and the other is rectangular
#3942 | open | MODELING | CONVERT MESH: operation failed
#3943 | open | MODELING | FILLET: operation failed
#3948 | open | MODELING | EXTRUDE: operation failed
#3952 | open | MODELING | THREAD: operation failed
#3959 | open | MODELING | Modeling: unexpected result
#3960 | open | MODELING | REVOLVE: operation failed
#3961 | open | MODELING | EXTRUDE: unexpected result
#3967 | open | MODELING | CONFIGURATIONS: unexpected result
#3968 | open | MODELING | HOLE: operation failed
#3971 | open | MODELING | FILLET: operation failed
#3973 | open | MODELING | Modeling: unexpected result
#3975 | open | MODELING | Rework of the hole tool.
#3977 | open | MODELING | Section Analysis
#3978 | open | MODELING | EXTRUDE: operation failed
#3982 | open | MODELING | FILLET: operation failed
#3986 | open | MODELING | Modeling: unexpected result
#4002 | open | MODELING | PLANE THROUGH THREE POINTS: operation failed
#4008 | open | MODELING | Symmetric revolve cut refused ("could not be computed reliably") when the tool's end faces lie in the body's side face
#4009 | open | MODELING | Shell with no removed face (closed hollow body with an internal void) is not possible: "select 1...1000 faces"
#4010 | open | MODELING | Shell refuses to remove the wall of a through hole ("offset: a corner whose faces don't meet the same way after the move")
#4011 | open | MODELING | Mirror of a cut whose copy lands under a wall removes 129.8 mm3 instead of 113.1 (a direct cut there removes 113.08)
#4017 | open | MODELING | EXTRUDE: operation failed
#4027 | open | MODELING | FILLET: operation failed. I'm just giving what the app gave me. as Schultz would say I know NOthing! (old show but fits)
#4029 | open | MODELING | Modeling: unexpected result - Offset plane
#4030 | open | MODELING | Modeling: unexpected result
#4033 | open | MODELING | THREAD: unexpected result filettatura sfaccetta facce
#4034 | open | MODELING | FILLET: operation failed non crea raggio
#4036 | open | MODELING | EXTRUDE: operation failed
#4037 | closed | MODELING | LOFT: operation failed
#4041 | open | MODELING | EXTRUDE: operation failed
#4042 | open | MODELING | MIRROR: operation failed
#4046 | open | MODELING | Modeling: unexpected result
#4047 | open | MODELING | CHAMFER: operation failed
#4049 | open | MODELING | Modeling: unexpected result
#4052 | open | MODELING | SHELL: operation failed
#4053 | open | MODELING | Modeling: Failure to Re-Roll Timeline
#4055 | open | MODELING | Chamfer that reaches a joined pin leaves the part open (or is refused)
#4056 | open | MODELING | Loft rails refuse a straight-line section drawn in a rail's plane
#4058 | open | MODELING | Join: walls extruded flush round a part's outline fail with "boolean Union: the operation failed: no result"
#4061 | open | MODELING | Modeling: unexpected result
#4062 | open | MODELING | Modeling: unexpected result
#4063 | open | MODELING | Modeling: unexpected result
#4066 | open | MODELING | CHAMFER: operation failed
#4067 | open | MODELING | Modeling: unexpected result
#4070 | open | MODELING | HOLE: unexpected result
#4073 | open | MODELING | SHELL: operation failed
#4075 | open | MODELING | EXTRUDE: operation failed
#4076 | open | MODELING | FILLET: operation failed
#4077 | open | MODELING | MIRROR: operation failed
#4079 | open | MODELING | FILLET: operation failed
#4082 | open | MODELING | Modeling: unexpected result
#4083 | open | MODELING | FILLET: operation failed after resizing original sketch
#4085 | open | MODELING | THREAD: operation failed
#4092 | open | MODELING | EXTRUDE Union on native loft with ribs: recovered intersection_curve panic, then no result
#4093 | open | MODELING | Guided sweep cut succeeds but STL has open boundaries and nonmanifold edges (native 3-turn thread coupon)
#4094 | open | MODELING | Sheet metal: Path Flange, a chain of bends on an existing sheet edge from a sketched path
#4095 | open | MODELING | Boolean Cut silently removes the wrong volume when a planar face stands closer to a cylinder than the coincidence tolerance
#4098 | open | MODELING | Extrude: document which way a positive taper goes
#4099 | open | MODELING | inspect.measure: an item picked by its point alone is refused with "missing field index"
#4101 | open | MODELING | Chamfer on a block with 1° sides leaves four 0.00016 mm edges at the corners
#4103 | open | MODELING | Modeling: Join two objects is stuck in computing
#4108 | open | MODELING | Modeling: unexpected result
#4111 | open | MODELING | EXTRUDE: unexpected result
#4120 | open | MODELING | SHELL: operation failed
#4122 | open | MODELING | Press/Pull Button V0.18.2
#4123 | open | MODELING | SPLIT BODY: operation failed
#4124 | open | MODELING | EXTRUDE: unexpected result
#4125 | open | MODELING | Modeling: unexpected result
#4133 | open | MODELING | FILLET: operation failed
#4134 | open | MODELING | Modeling:Test
#4137 | open | MODELING | FILLET: operation failed
#4139 | open | MODELING | REVOLVE: operation failed
#4140 | open | MODELING | CHAMFER: two edges selected results in error
#4141 | open | MODELING | Boolean cut through a body vertex next to a B-spline face falls back to tool_nudge, leaving a 0.56 µm edge that breaks a later fillet
#4143 | open | MODELING | Fillet on a cylinder × B-spline edge ending against a square wall is refused above r1.5 ("blend is larger than the neighbouring faces"); OCC does r4
#4144 | open | MODELING | SHELL: operation failed
#4145 | open | MODELING | FILLET: operation failed
#4148 | open | MODELING | Unfortunately, it hasn't been even remotely usable so far.
#4149 | open | MODELING | SolidCraft 0.18.2 crashes with EXCEPTION_STACK_OVERFLOW when creating modeled M10x1.5 internal thread
#4151 | open | MODELING | EXTRUDE: operation failed
#4153 | open | MODELING | [Feature Request] Support for custom thread parameters and non-standard metric sizes (e.g., M33)
#4155 | closed | MODELING | FLANGE: operation failed
#4157 | open | MODELING | CHAMFER: operation failed
#4160 | open | MODELING | FILLET: operation failed 3d sketch (3d sketch is totally broken BUG !!!!)
#4161 | open | MODELING | FOLD: operation failed
#4173 | open | MODELING | REVOLVE: operation failed
#4174 | open | MODELING | FILLET: operation failed
#4175 | open | MODELING | Modeling: unexpected result
#4183 | open | MODELING | THREAD: operation failed
#4184 | open | MODELING | SPLIT BODY: operation failed
#4187 | open | MODELING | FILLET: operation failed
#4191 | open | MODELING | EXTRUDE: operation failed
#4194 | open | MODELING | SHELL: operation failed
#4195 | open | MODELING | EXTRUDE: operation failed
#4196 | open | MODELING | Modeling: unexpected result
#4206 | open | MODELING | Auto-Resize for planes
#4208 | open | MODELING | Modeling: unexpected result
#4210 | open | MODELING | EXTRUDE: operation failed
#4211 | open | MODELING | EXTRUDE: operation failed
#4213 | open | MODELING | Extrude.
#4218 | open | MODELING | COMBINE: operation failed
#4222 | open | MODELING | Fillet Error
#4224 | open | MODELING | Extrude tool does boolean operations on hidden bodys - not right
#4230 | open | MODELING | The ribbon does not show the selection status properly: The Sheet Metal tab does not follow the standard pattern to show its status
#4234 | open | MODELING | loft from model surface - FR
#4235 | open | MODELING | CHAMFER: operation failed
#4236 | open | MODELING | Coil: a section from a sketch profile
#4238 | open | MODELING | EXTRUDE: operation failed
#4240 | open | MODELING | MIRROR: operation failed
#4241 | open | MODELING | FILLET: operation failed
#4247 | open | MODELING | Modeling: unexpected result
#4249 | open | MODELING | EXTRUDE: operation failed
#4250 | open | MODELING | [Kernel] Surface revolutions sometimes split at strange increments.
#4257 | open | MODELING | Surfaces should not be same body type as solids
#4259 | open | MODELING | OFFSET PLANE: operation failed
#4261 | open | MODELING | FILLET: operation failed
#4262 | open | MODELING | EXTRUDE: unexpected result
#4265 | open | MODELING | CHAMFER: operation failed
#4266 | open | MODELING | Modeling: unexpected result
#4269 | open | MODELING | FILLET: operation failed
#4272 | open | MODELING | Modeling: unexpected result
#4276 | open | MODELING | FILLET: operation failed
#4277 | closed | MODELING | Modeling: unexpected result
#4278 | open | MODELING | EXTRUDE: operation failed
#4279 | open | MODELING | SHELL: operation failed
#4280 | open | MODELING | CIRCULAR PATTERN: operation failed
#4281 | open | MODELING | Modeling: unexpected result with thread
#4282 | open | MODELING | Modeling: unexpected result - computing takes too long
#4287 | open | MODELING | Show the size of the selected edge, face or body in a corner of the viewport
#4289 | open | MODELING | EXTRUDE: operation failed
#4294 | open | MODELING | Extrude Cut leaves a stray planar sheet outside a slotted cylindrical body, with no error
#4297 | open | MODELING | FLANGE: operation failed
#4300 | open | MODELING | OFFSET: unexpected result
#4301 | closed | MODELING | FLANGE: operation failed
#4302 | open | MODELING | Modeling: unexpected result - Incorrect behavior when suppressing a feature
#4305 | open | MODELING | SWEEP: unexpected result - Sweep doesn't follow path selection
#4306 | open | MODELING | SHELL: operation failed
#4312 | open | MODELING | Modeling: unexpected result
#4313 | open | MODELING | COMBINE: operation failed
#4315 | open | MODELING | Split/intersect fails after a STEP round trip when a cylinder is joined over a box edge
#4316 | open | MODELING | COMBINE: operation failed
#4317 | closed | MODELING | COMBINE: operation failed
#4321 | closed | MODELING | PLANE AT ANGLE: operation failed
#4322 | open | MODELING | EXTRUDE: operation failed
#4328 | open | MODELING | CONVERT TO SHEET METAL: operation failed
#4331 | open | MODELING | OFFSET FACE: operation failed
#4332 | open | MODELING | Two-distance chamfer of a spline outline's rim refused: "loop edges must be lines or arcs"
#4334 | open | MODELING | Modeling: distance hint
#4336 | open | MODELING | FILLET: operation failed between Radius and line
#4337 | open | MODELING | MEASURE: unexpected result
#4341 | open | MODELING | Modeling: Unexpected behavior — Chamfer and Fillet lose their references and jump to unrelated edges
#4345 | open | MODELING | Offset Plane created from a face loses its reference and snaps to the origin
#4346 | open | MODELING | FILLET: operation failed
#4347 | open | MODELING | EXTRUDE: operation failed
#4348 | open | MODELING | EXTRUDE: operation failed
#4353 | open | MODELING | Renaming a construction plane loses its sketch projections, its canvases and its hidden state
#4355 | open | MODELING | EXTRUDE: operation failed
#4357 | open | MODELING | Modeling: unexpected result
#4358 | open | MODELING | Modeling: unexpected result (Can't create Surface Loft between two profiles/sketches)
#4359 | open | MODELING | Modeling: unexpected result
#4360 | closed | MODELING | Modeling: unexpected result - bodies not highlighted in browser
#4361 | open | MODELING | EXTRUDE: operation failed
#4363 | open | MODELING | Plane along path at an angle needed.
#4365 | open | MODELING | FILLET: operation failed
#4366 | open | MODELING | CHAMFER: operation failed
#4368 | open | MODELING | DoF count is confusing and weird
#4369 | open | MODELING | FILLET: operation failed
#4372 | open | MODELING | Modeling: unexpected result
#4373 | open | MODELING | FILLET: operation failed
#4374 | open | MODELING | Rotate
#4375 | open | MODELING | Modeling: unexpected result
#4377 | open | MODELING | SHELL: operation failed
#4378 | open | MODELING | THREAD: unexpected result sfaccetttatura dopo filettatura
#4379 | open | MODELING | Modeling: unexpected result
#4380 | closed | MODELING | Modeling: unexpected result
#4383 | open | MODELING | EXTRUDE: operation failed
#4384 | open | MODELING | Placing a feature on a face: one position input for Hole, Boss, Snap Fit, Rest and the primitives
#4393 | open | MODELING | Modeling: unexpected result
#4399 | open | MODELING | Modeling: unexpected result
#4401 | open | MODELING | EXTRUDE: operation failed
#4402 | open | MODELING | Alignment is not working correctly
#4407 | open | MODELING | Modeling: unexpected result
#4408 | open | MODELING | EXTRUDE: operation failed
#4410 | open | MODELING | Modeling: unexpected result
#4412 | open | MODELING | RECTANGULAR PATTERN: unexpected result
#4413 | open | MODELING | EXTRUDE: operation failed
#4414 | open | MODELING | Thread: persistent right- and left-handed direction for holes and cylindrical threads
#4419 | open | MODELING | Renaming a construction axis or point breaks the planes and axes built on it ("unknown construction")
#4420 | open | MODELING | Renaming a parameter misses sheet-metal and plastic rules and canvases ("unknown parameter")
#4421 | open | MODELING | Renaming a body or component loses its plastic rule and shows hidden copies again; undoing a rename shows a hidden body
#4424 | open | MODELING | EXTRUDE: operation failed
#4428 | open | MODELING | CHAMFER: operation failed
#4429 | open | MODELING | CHAMFER: operation failed
#4433 | open | MODELING | HOLE: operation failed
#4434 | open | MODELING | Join: say so when the joined piece doesn't touch the body
#4435 | open | MODELING | CIRCULAR PATTERN: Quantity parameter not saved
#4437 | open | MODELING | EXTRUDE: operation failed
#4440 | open | MODELING | Sheet metal: Flat Pattern view doesn't draw perforations
#4443 | open | MODELING | Saved model is never used when a boolean needed a retry ("saved model: diagnostics")
#4444 | open | MODELING | FILLET: operation failed
#4446 | open | MODELING | erededePLANE AT ANGLE: operation failed
#4448 | open | MODELING | CIRCULAR PATTERN: unexpected result
#4450 | open | MODELING | Modeling: unexpected result
#4452 | open | MODELING | Modeling: unexpected result
#4454 | open | MODELING | REVOLVE: operation failed
#4456 | open | MODELING | PLANE AT ANGLE: operation failed
#4457 | open | MODELING | Modeling: unexpected result
#4458 | open | MODELING | MIRROR: unexpected result
#4459 | open | MODELING | MOVE: Components selectable outside of active component
#4463 | open | MODELING | Modeling: unexpected result
#4465 | open | MODELING | SURFACE EXTRUDE: unexpected result
#4467 | open | MODELING | SHELL: operation failed
#4468 | open | MODELING | Measure overestimates the volume after a boolean, while the geometry is exact
#4469 | open | MODELING | Chamfer of an offset outline wall's rim: many short edges, partial tangent chain, refused or very slow
#4472 | open | MODELING | FILLET: operation failed
#4474 | open | MODELING | FILLET: operation failed
#4475 | open | MODELING | Modeling: unexpected result
#4477 | open | MODELING | EXTRUDE: operation failed
#4481 | open | MODELING | HOLE: unexpected result
#4483 | open | MODELING | EXTRUDE, COMBINE, FILET: unexpected result from .STEP file
#4484 | open | MODELING | Thicken (Engrosar) feature generates corrupted/invalid geometry on complex surface bodies
#4485 | open | MODELING | CONVERT MESH: operation failed
#4488 | open | MODELING | EMBOSS: operation failed on inner side of cylindic slice.
#4489 | open | MODELING | MOVE: unexpected result
#4492 | open | MODELING | OFFSET FACE: operation failed
#4494 | open | MODELING | Modeling: unexpected result by asking for a 3D modelisation of a thread
#4495 | open | MODELING | FILLET: operation failed - Radius on edge on tilted surface
#4496 | open | MODELING | FILLET: operation failed
#4498 | open | MODELING | CHAMFER: operation failed
#4499 | open | MODELING | FILLET: unexpected result
#4500 | open | MODELING | CONVERT MESH: operation failed
#4503 | open | MODELING | SHELL: operation failed
#4505 | open | MODELING | Combine operation failed
#4506 | open | MODELING | REVOLVE: operation failed
#4508 | open | MODELING | Parameters by Component
#4509 | open | MODELING | OFFSET PLANE: operation failed
#4510 | open | MODELING | Modeling: unexpected result Delete face while in Surface Workspace
#4514 | open | MODELING | FLANGE: operation failed
#4516 | open | MODELING | EXTRUDE: operation failed
#4518 | open | MODELING | FILLET: operation failed
#4521 | open | MODELING | REVOLVE: operation failed
#4522 | open | MODELING | SHELL: operation failed
#4523 | open | MODELING | EXTRUDE: operation failed
#4524 | open | MODELING | LOFT: operation failed
#4525 | open | MODELING | BOUNDARY SURFACE: unexpected result
#4526 | open | MODELING | Modeling: unexpected result
#4528 | open | MODELING | LOFT: operation failed
#4529 | open | MODELING | Modeling: unexpected result
#4530 | open | MODELING | COMBINE: operation failed
#4532 | open | MODELING | COMBINE: unexpected result
#4534 | closed | MODELING | EXTRUDE: operation failed
#4537 | open | MODELING | THREAD: operation failed
#4540 | open | MODELING | Modeling: unexpected result
#4541 | open | MODELING | Modeling: unexpected result
#4543 | open | MODELING | CHAMFER: operation failed
#4550 | open | MODELING | FILLET: operation failed
#4551 | open | MODELING | EXTRUDE: operation failed
#4552 | open | MODELING | Modeling: unexpected result (wheels imported with mesh gaps
#4555 | open | MODELING | SHELL: operation failed - bottom face of a stacked component fails
#4557 | open | MODELING | Modeling: unexpected result, non-manifold eges
#4558 | open | MODELING | Modeling: unexpected result, changing simple sketch causes body to disappear
#4560 | open | MODELING | CONVERT MESH: operation failed
#4561 | open | MODELING | CONVERT MESH: operation failed
#4564 | open | MODELING | CONVERT MESH: operation failed
#4567 | open | MODELING | SHELL: operation failed
#4568 | open | MODELING | Modeling: unexpected result
#4569 | open | MODELING | CHAMFER: operation failed
#4570 | open | MODELING | Modeling: unexpected result
#4571 | open | MODELING | Modeling: unexpected result
#4572 | open | MODELING | EXTRUDE: Join operation targets the wrong Body
#4573 | open | MODELING | SHELL: operation failed
#4574 | open | MODELING | SHELL: operation failed
#4575 | closed | MODELING | CHAMFER: unexpected result
#4576 | open | MODELING | REVOLVE: operation failed
#4577 | open | MODELING | THREAD: Custom threads on cylinders
#4579 | open | MODELING | Serious concerns with SurfaceLoft and ruled surface
#4583 | open | MODELING | OFFSET: operation failed
#4584 | open | MODELING | OFFSET PLANE: operation failed
#4586 | open | MODELING | CONVERT MESH: operation failed
#4593 | open | MODELING | CIRCULAR PATTERN: operation failed
#4594 | open | MODELING | project intersect issue
#4595 | open | MODELING | Press/Pull cylinders
#4597 | open | MODELING | EXTRUDE: operation failed
#4600 | open | MODELING | EXTRUDE: operation failed
#4606 | open | MODELING | REVOLVE: never finishes
#4609 | open | MODELING | CONVERT MESH: operation failed

## SKETCH

#1 | closed | SKETCH | Sketch on a moved component's face lands in the wrong place
#31 | closed | SKETCH | Hidden origin planes are still pickable (and win ties against faces) in Create Sketch
#40 | closed | SKETCH | Offsets of projected sketch geometry get new ids on recompute: a profile built from them fails after a parameter change
#43 | closed | SKETCH | Sketch: constraint glyphs are hard to read (overlap, size, which entity)
#44 | open | SKETCH | Sketch: dimension placement and editing
#45 | closed | SKETCH | Sketch: trim / extend / offset robustness
#46 | closed | SKETCH | Sketch: fillet / chamfer of corners (line-arc, arc-arc, chains, too-large radius)
#47 | open | SKETCH | Sketch: Project / Include edges of other bodies and components
#81 | closed | SKETCH | Angle constraints don't work as expected
#105 | open | SKETCH | TAB Switching Bug
#116 | open | SKETCH | Sketcher UX improvements
#130 | open | SKETCH | Improvements UI-arrangement, dimensioning tool and visualization construction sequence
#145 | closed | SKETCH | Sketch point and constraint caps are not enforced at design load; solver allocates dense matrices and aborts
#162 | closed | SKETCH | Sketching Limitations
#164 | closed | SKETCH | Sketch highlight for extrude is less visble when more zoomed in.
#175 | closed | SKETCH | Dimentions are rendered bellow constraints
#177 | closed | SKETCH | Sketch: a centerline can't be switched directly to construction
#187 | closed | SKETCH | CREATE SKETCH: operation failed
#191 | closed | SKETCH | Feature Request: Add point to hotbar in sketch mode
#195 | open | SKETCH | CREATE SKETCH: operation failed
#203 | closed | SKETCH | Sketch solver: constructive solving (Owen/Fudos–Hoffmann) so edits and drags keep their solution
#217 | closed | SKETCH | Modeling: doubleclicking a single point in sketch produces an error
#236 | closed | SKETCH | Text in sketch can not be moved and edited.
#257 | closed | SKETCH | Horizontal and Vertical dimensions
#267 | closed | SKETCH | timeline.delete of a sketch leaves later features that use its body by name; they fail without being reported
#269 | closed | SKETCH | MCP `list_commands` filter also matches parameter docs: `sketch.delete` returns 56 commands (37 kB)
#320 | open | SKETCH | Creating a dimension which over constrains should result in a Driven Dimension instead of fail
#344 | open | SKETCH | CREATE SKETCH: operation failed
#394 | closed | SKETCH | sketch.constraint.symmetry about a sketch axis: line "y" gives "unknown sketch curve `y`"
#415 | open | SKETCH | CREATE SKETCH: operation failed
#417 | closed | SKETCH | Sketch Palette and dialogs: heading bands, a Sketch Status label, less empty space under Finish Sketch
#421 | closed | SKETCH | Sketch Palette: Linetype buttons don't show the line type of the selected curves
#423 | open | SKETCH | Through-all cut removes material behind its sketch plane after the coplanar-separation retry
#440 | closed | SKETCH | CREATE SKETCH: operation failed
#489 | closed | SKETCH | CREATE SKETCH: operation failed
#491 | closed | SKETCH | Sketch on a side face that Join merged from two extrudes is refused
#492 | closed | SKETCH | Sketch on a face split into pieces by a later feature is refused
#493 | closed | SKETCH | Flat faces made by Cut walls and chamfers have no exact source to sketch on
#513 | closed | SKETCH | Sketch text: set it in another font (an installed family or a font file), not only in the built-in Liberation Sans
#517 | closed | SKETCH | Sketch: Offset tool is missing a dialog
#520 | closed | SKETCH | Polygon Sketch Tool
#557 | closed | SKETCH | A sketch or offset plane on a patterned or mirrored feature's faces is refused
#563 | closed | SKETCH | After a Draft, and on a tapered Extrude, no face takes an attached sketch
#564 | closed | SKETCH | The rim a Shell leaves can't take an attached sketch
#565 | closed | SKETCH | After Split Body or Combine Intersect, no face takes an attached sketch
#581 | closed | SKETCH | Sketch: Construction lines are rendered incorrectly.
#584 | closed | SKETCH | Sketch: Changing line type deselects the current tool.
#588 | closed | SKETCH | Modeling: Area is not split by tangent constraint.
#597 | open | SKETCH | Creating an extrusion with a taper angle refuses to let you create a sketch on the top face.
#628 | open | SKETCH | Faces of a lid with joined ribs can't take an attached sketch
#631 | open | SKETCH | CREATE SKETCH: operation failed
#635 | closed | SKETCH | Sketcher issue with coincidence with a projected feature
#651 | closed | SKETCH | circular and rectangular in sketch dont create consrains and no menu
#680 | closed | SKETCH | Sketch preferences should apply globally across parts and documents
#681 | closed | SKETCH | CREATE SKETCH: operation failed
#687 | closed | SKETCH | Sketch: a clover of same-size rectangles is refused as over-constrained
#696 | closed | SKETCH | Reuse Solid dialogs for Sketch Mirror, Pattern, and Offset with editable selections and live previews
#712 | closed | SKETCH | Sketch: a projected edge becomes another edge of the body when the body changes size
#722 | closed | SKETCH | [Feature Request] Support Inline Unit Conversion in Sketch Dimensions
#727 | open | SKETCH | Parametric text: update linked extrusions after legend changes and preserve letter counters (keycaps)
#741 | open | SKETCH | Profiles from planar loops of 3D sketch curves (regions out of the sketch plane)
#754 | open | SKETCH | Dimensions: Missing Dimension type ->Arc Lenght <-
#755 | closed | SKETCH | A sketch whose spline/arc loop is crossed at its junctions by two lines (closed by an arc) finds no profiles at all
#790 | closed | SKETCH | Bug Report: Sketch Pattern Unexpected Result
#891 | closed | SKETCH | Rounded Rectangle
#943 | open | SKETCH | CREATE SKETCH: operation failed on angled face
#954 | closed | SKETCH | Can't sketch on the side walls of one sketch extruded up and down and joined
#1026 | closed | SKETCH | Sketch fillet fails at a corner that also has a construction line ending in it ("the corner must join exactly two lines")
#1069 | open | SKETCH | No visual difference between selected and unselected sketch vertices
#1075 | closed | SKETCH | Symmetry constraint does not support point symmetry, cannot select origin axes / point
#1110 | open | SKETCH | Missing Sketch Features: Intersection and Project
#1121 | closed | SKETCH | Sketch: a line end on a circle near its quadrant is placed off the axis the preview shows
#1128 | closed | SKETCH | Sketching: Plane, Line, Point not working
#1167 | closed | SKETCH | Deleted solid bodies reappear when entering Sketch Edit mode Description
#1174 | open | SKETCH | Sketch: unexpected result
#1208 | closed | SKETCH | Sketch fillet refuses a corner between a line and an arc
#1227 | open | SKETCH | CREATE SKETCH: operation failed
#1335 | closed | SKETCH | BUG: Create Sketch Menu 2x bugs
#1361 | closed | SKETCH | Dimensions reference the lowest point of a line, not the closest point to the dimension arrow, resulting in ugly and hard-to-read sketches.
#1362 | closed | SKETCH | Sketch text: height, place and angle take numbers only, so a label cannot follow a parameter
#1365 | closed | SKETCH | Explicit dimensions should take precedence over implicit ones
#1480 | closed | SKETCH | Copy, cut and paste of sketch geometry (Ctrl+C/X/V, context menu) is missing
#1485 | closed | SKETCH | Sketching: Tangent arc tool doesn't properly recognize line or arc ends when endpoint was generated by trimming along construction lines
#1519 | closed | SKETCH | sketch loses its face attatchment when changing its face-creating extrude even when there is still a face
#1536 | closed | SKETCH | sketch.cut panics when its parameters aren't a JSON object (hostile_params_never_panic fails on main since #1500)
#1544 | closed | SKETCH | Feature Request: Mirror Entities in Sketch Mode
#1586 | closed | SKETCH | Sketch on an origin plane faces the side you clicked, not a fixed side
#1639 | open | SKETCH | Feature request: Interactive transformation gizmo for sketch entities
#1679 | open | SKETCH | Proposed remaining fixes: free orbit, spline path anchoring, sketch references, and preview status
#1755 | closed | SKETCH | Create Sketch: show origin planes through the model, and pick among overlapping planes with press-and-hold
#1756 | closed | SKETCH | Draw the view orthographic while a sketch is open
#1772 | closed | SKETCH | Support Text/String Parameters for Sketch Text
#1796 | open | SKETCH | Symmetry relation does not allow for aligning a centerline between two fixed lines
#1803 | closed | SKETCH | Sketch Dimension (D) can't pick a circle's edge, so there's no tangent/edge distance between a line and a circle
#1804 | closed | SKETCH | Pressing Escape twice in a sketch exits the sketch
#1824 | closed | SKETCH | Sketch fillet can only pick the corner point, so a corner where more than two lines meet cannot be filleted
#1842 | closed | SKETCH | Explode text
#1874 | open | SKETCH | CREATE SKETCH: operation failed
#1878 | open | SKETCH | CREATE SKETCH: operation failed
#1884 | open | SKETCH | Unable to Create Sketch on a Planar Face
#1886 | open | SKETCH | Bug: Sketch Appears on Opposite Side of Selected Face & Ctrl+8 View Toggle
#1942 | open | SKETCH | How to draw polygon, polyline ?
#1976 | closed | SKETCH | Constrained sketch point dragging stops before the closest feasible cursor target
#2000 | closed | SKETCH | Sketch: show open and untied profile ends
#2031 | open | SKETCH | Using move to move sketch objects doesn't work great.
#2087 | closed | SKETCH | Sketch: allow selecting a line's midpoint as a point in Horizontal/Vertical constraints
#2097 | open | SKETCH | CREATE SKETCH: operation failed
#2111 | open | SKETCH | CREATE SKETCH: operation failed
#2114 | closed | SKETCH | Revolved body - OFFSET PLANE / CREATE SKETCH / EXTRUDE: operation failed
#2125 | open | SKETCH | CREATE SKETCH: operation failed
#2147 | open | SKETCH | Modeling: unexpected result (sketch behaviour)
#2154 | open | SKETCH | Offset after explode text
#2166 | closed | SKETCH | 2-D SKETCH: 3-point Arc, midpoint not associated and not changing
#2174 | closed | SKETCH | Sketch: no live preview for fillet and chamfer corners
#2190 | open | SKETCH | Clicking a parameter suggestion in the dimension input doesn't accept it (Enter works)
#2227 | open | SKETCH | While in Sketcher, SpaceMouse Orbit does not work.
#2351 | open | SKETCH | CREATE SKETCH: operation failed
#2355 | closed | SKETCH | Reduce repeated sketch diagnostics and add reproducible solver profiling
#2364 | open | SKETCH | cannot add sketch dimensions between sketch and axis
#2366 | open | SKETCH | Sketch dimension input: accept math expressions (40/2, 25+3.5, width/2) like the API already does
#2371 | open | SKETCH | [Feature request] - creating parameters dynamically using #parameterName when typing dimensions
#2380 | open | SKETCH | CREATE SKETCH: operation failed
#2424 | closed | SKETCH | OFFSET (Spline): Result does not generate constraint points at ends
#2427 | open | SKETCH | Modeling: unexpected result - misaligned sketch grid to sketch plane
#2448 | closed | SKETCH | Sketch: a click that moves while the button is down is lost (slot, line, arc, spline…) or draws a sliver (rectangle, circle, polygon)
#2459 | open | SKETCH | Sketch tools: press a tool’s shortcut repeatedly to cycle through its creation modes
#2476 | open | SKETCH | Add a keyboard shortcut for "Look At" (selected face/plane/sketch)
#2505 | open | SKETCH | CREATE SKETCH: operation failed
#2514 | open | SKETCH | CREATE SKETCH: operation failed
#2530 | open | SKETCH | CREATE SKETCH: operation failed
#2535 | closed | SKETCH | Sketch: slot outline is missing while choosing its width
#2542 | open | SKETCH | Sketch does not highlight segments correctly for extrusion.
#2585 | open | SKETCH | CREATE SKETCH: operation failed
#2621 | closed | SKETCH | Moving sketch drags some lines like a string instead of just moving them.
#2698 | open | SKETCH | Mesh Section Sketch + Fit Curves (scan-to-CAD)
#2716 | closed | SKETCH | Sketch: Starting a sketch from the Browser but being back in the timeline
#2722 | open | SKETCH | CREATE SKETCH: operation failed on face of thin extrude
#2724 | closed | SKETCH | DIMENSION: angle does not match expected result
#2725 | open | SKETCH | Sketch: snap to where two curves cross
#2743 | open | SKETCH | CREATE SKETCH: operation failed
#2792 | closed | SKETCH | sketch.edit_text: several labels mean several full rebuilds
#2802 | open | SKETCH | Exit Sketch Mode when extruding, revolving, etc
#2823 | closed | SKETCH | Grid in sketch mode misses some vertical or horizontal lines as I zoom in/out.
#2863 | closed | SKETCH | sketch: point coordinates can't follow the design's parameters
#2906 | closed | SKETCH | When we begin a new sketch, the dialog shows "fully constrained" even when the canvas is blank!
#2916 | closed | SKETCH | Assign dimension name while entering dimension on sketch
#2919 | closed | SKETCH | Don't default new dimensions to 10mm
#2925 | open | SKETCH | When entering a dimension, typing a string finds matching parameters using "contains" match as well as "starts with" matches
#2940 | open | SKETCH | Misbehavior of Symmetric Constraint
#2961 | open | SKETCH | Bug: Symmetry constraint fails after dimensioning a sketch line
#2980 | open | SKETCH | Cant move a sketch
#3022 | open | SKETCH | Feature Request: Automatically Align Grid with Active Sketch Plane.
#3032 | open | SKETCH | Add Create a Sketch to right click menu
#3037 | open | SKETCH | CREATE SKETCH: operation failed
#3052 | closed | SKETCH | The trim tool does not highlight the segment that will be deleted. It highlights the entire line, circle, etc.
#3109 | open | SKETCH | CREATE SKETCH: operation failed
#3114 | closed | SKETCH | Sketch › Create panel is missing Project / Include 3D Geometry / Intersect buttons (P shortcut works)
#3167 | closed | SKETCH | Sketch dimension: type-first switching from aligned to horizontal/vertical keeps the aligned value and misplaces the dimension
#3196 | open | SKETCH | TRIM: unexpected result
#3198 | closed | SKETCH | Look At on a sketch centres on its plane's origin: a sketch away from the origin ends up out of view
#3204 | open | SKETCH | Feature Request: Wrap arbitrary 2D sketches onto cylindrical surfaces, including interrupted cylindrical faces
#3248 | closed | SKETCH | sketch.create: a custom plane's `origin` given as an expression or string silently becomes [0, 0, 0]
#3250 | open | SKETCH | The Information Architecture (UI state management) is flawed in calling Sketcher from the top-level workbenches.
#3261 | closed | SKETCH | Topological Naming Failure: Upstream sketch edit creating a new face breaks downstream feature attachments
#3267 | closed | SKETCH | Modeling: Sketch point visibility
#3301 | open | SKETCH | CREATE SKETCH: operation failed
#3320 | open | SKETCH | Cannot reference plane's axis for dimensions.
#3360 | closed | SKETCH | Constrained sketch edge drags lag behind the cursor and release misses the final position
#3393 | closed | SKETCH | Feature Request [Sketching]: Arc arrow to continue arc radius reference when not pointing to the arc line directly
#3395 | open | SKETCH | 3D sketching issue
#3438 | open | SKETCH | CREATE SKETCH: operation failed
#3451 | open | SKETCH | Coincident and other constraints need a way to externally reference other geometry
#3470 | open | SKETCH | CREATE SKETCH: operation failed
#3484 | open | SKETCH | CREATE SKETCH: operation failed
#3488 | closed | SKETCH | CREATE SKETCH: operation failed
#3530 | open | SKETCH | CREATE SKETCH: operation failed
#3546 | open | SKETCH | CREATE SKETCH: unexpected result
#3550 | open | SKETCH | CREATE SKETCH: unexpected result
#3590 | open | SKETCH | CREATE SKETCH: operation failed
#3599 | open | SKETCH | Pierce Constraint - sketches
#3631 | open | SKETCH | Modeling: unexpected result, not seeing all sketch detail when highlight
#3637 | open | SKETCH | Multiple profiles per sketch
#3639 | open | SKETCH | 3D Sketch in SolidCraft
#3701 | closed | SKETCH | CREATE SKETCH: operation failed
#3723 | open | SKETCH | CREATE SKETCH: operation failed
#3786 | open | SKETCH | Sketch: adding a dimension to one shape moves everything around
#3805 | closed | SKETCH | Sketch: Error when using mid point tool on two lines
#3812 | open | SKETCH | [UI/UX / Sketcher] Unit Mismatch: Dimension input callout box defaults to millimeters despite active Imperial/Inch unit settings
#3819 | closed | SKETCH | Sketch: editing a circular pattern to more copies previews only some of them (gap next to the original)
#3838 | open | SKETCH | Cancel Sketch
#3855 | open | SKETCH | 3D sketch only works with "Line" and "Fit Point Spline".
#3905 | open | SKETCH | Modeling: Centerline not closing profile
#3917 | closed | SKETCH | Sketch: trimming an offset's curve reports success but leaves the curve whole
#3923 | open | SKETCH | Excessive re-computation after adding sketch constraints
#3931 | open | SKETCH | Canvas to Sketch: trace a canvas image into an editable sketch
#4003 | open | SKETCH | Frames and weldments: structural members along sketch lines, corner treatments, end caps, gussets, welds and cut lists
#4057 | open | SKETCH | Dialog curve picks lose their sketch when sketches share curve ids
#4064 | open | SKETCH | Sketch Dimension: point or circle to the edge of a circle or arc
#4069 | open | SKETCH | Sketch Constraint: Group Entities
#4078 | open | SKETCH | CREATE SKETCH: operation failed
#4097 | open | SKETCH | Sketch fillet: a corner placed by an expression loses its parameter link, so the outline stops following the parameter
#4109 | open | SKETCH | modifying circles
#4135 | open | SKETCH | CREATE SKETCH: operation failed
#4193 | open | SKETCH | Sketch Mirror: origin axis cannot be used as mirror line
#4227 | open | SKETCH | Sketch Edit lookat Locked Rotation toggle while editing
#4274 | closed | SKETCH | Importing Images as Sketch Reference
#4325 | open | SKETCH | CREATE SKETCH: operation failed
#4352 | open | SKETCH | Dimensioning: Many Issues
#4385 | open | SKETCH | Two-sided sketch offset only shows the Distance dimension
#4395 | open | SKETCH | CREATE SKETCH: operation failed
#4447 | open | SKETCH | Full-3d constraints similar to SolveSpace
#4464 | open | SKETCH | Sketch: an Offset takes the turning freedom away from its source lines (Horizontal and angle edits refused)
#4502 | open | SKETCH | Sketch mode: viewport turns solid blue (v0.18.3, Intel integrated graphics)
#4578 | open | SKETCH | CREATE SKETCH: operation failed
#4585 | open | SKETCH | Sketching: Unconstrained sketch doesn't move if beyond a certain complexity even with only 1 DoF.
#4591 | open | SKETCH | Modeling: Closed sketch geometry not detected.
#4612 | open | SKETCH | CREATE SKETCH: operation failed
#4615 | open | SKETCH | Extend tool does not extend lines properly

## DRAWING-CAM-IO-UI

#2 | closed | DRAWING-CAM-IO-UI | Imported STEP colours are lost after any feature on the body
#3 | closed | DRAWING-CAM-IO-UI | Y-up STEP files import lying on their backs (add Y-up / Z-up orientation)
#4 | closed | DRAWING-CAM-IO-UI | `solvecraft-cli snapshot` ignores body and face colours
#11 | closed | DRAWING-CAM-IO-UI | STEP export merges faces the user split on purpose
#14 | closed | DRAWING-CAM-IO-UI | Test the IGES reader against Fusion's IGES exports
#16 | closed | DRAWING-CAM-IO-UI | Draw construction axes and points in the viewport
#17 | closed | DRAWING-CAM-IO-UI | Feature dialogs: rows for the newer engine options
#18 | closed | DRAWING-CAM-IO-UI | Feature dialogs: truncated labels and a cut preview for Hole
#19 | closed | DRAWING-CAM-IO-UI | Dialogs and icons for the surface tools
#22 | closed | DRAWING-CAM-IO-UI | Toolbar overflows on narrow windows
#33 | closed | DRAWING-CAM-IO-UI | Quitting with unsaved changes needs a save prompt
#34 | closed | DRAWING-CAM-IO-UI | Web: opening a new design shows a black screen
#36 | closed | DRAWING-CAM-IO-UI | Fusion-style Preferences, opened from the top-right of the window
#48 | closed | DRAWING-CAM-IO-UI | Viewport: silhouette edges on curved surfaces like Fusion (crisp, depth-correct, also in previews and the CPU snapshot)
#49 | closed | DRAWING-CAM-IO-UI | Viewport: transparency sort for see-through bodies and faces
#50 | closed | DRAWING-CAM-IO-UI | Viewport: selected edges and faces stay visible where the model hides them
#51 | closed | DRAWING-CAM-IO-UI | Viewport: Section Analysis visuals like Fusion
#60 | closed | DRAWING-CAM-IO-UI | Debug builds: two UI tests panic with "Dropped TexturesDelta with unapplied deltas"
#62 | closed | DRAWING-CAM-IO-UI | Fusion-like start experience: a Home tab, Open Recent, links
#64 | closed | DRAWING-CAM-IO-UI | Menu don't react on mouse hovering
#67 | closed | DRAWING-CAM-IO-UI | Sketch - Import DXF and Import SVG Fail on MacOS
#79 | closed | DRAWING-CAM-IO-UI | Agent cursor: Instant speed breaks; speeds should be Normal / Fast / Instant
#82 | closed | DRAWING-CAM-IO-UI | View cube shows BACK on top when Y-up is on
#87 | closed | DRAWING-CAM-IO-UI | Mousewheel scrolling in the keyboard shortcuts popup also scrolls the workspace
#88 | closed | DRAWING-CAM-IO-UI | 3D view: circular edges and curved faces only highlight and select half
#92 | closed | DRAWING-CAM-IO-UI | Hover highlight flickers rapidly while panning or rotating the view
#94 | open | DRAWING-CAM-IO-UI | More like solidworks
#103 | open | DRAWING-CAM-IO-UI | Add "in-between" navigation states to the navigation cube.
#104 | closed | DRAWING-CAM-IO-UI | Web version "Save As" doesn't open up a window to save. It just saves.
#107 | closed | DRAWING-CAM-IO-UI | Perspective tool won't move when holding down with cursor.
#111 | closed | DRAWING-CAM-IO-UI | Do you want a fresh app icon for this project?
#144 | closed | DRAWING-CAM-IO-UI | 3MF build items re-copy every vertex and DXF nested INSERTs recurse exponentially (tiny files exhaust memory or hang)
#146 | closed | DRAWING-CAM-IO-UI | STEP/IGES import: four unbounded expansions (IGES composite fan-out, shared parameter ranges, STEP knot multiplicity, bounds per face) abort on OOM
#148 | open | DRAWING-CAM-IO-UI | Feature Request: Support for 3Dconnexion SpaceMouse
#158 | closed | DRAWING-CAM-IO-UI | Import-budget regression tests fail wall-clock assertions under parallel CI load
#165 | closed | DRAWING-CAM-IO-UI | Bring back (optionally) the cube
#170 | closed | DRAWING-CAM-IO-UI | CLI run --results silently skips --save and --out
#173 | closed | DRAWING-CAM-IO-UI | Wheel zoom stutters on large projects: add an option to disable smoothing or reduce navigation overhead
#182 | closed | DRAWING-CAM-IO-UI | Ground plane grid lines disappear at low viewing angle in perspective view
#183 | closed | DRAWING-CAM-IO-UI | Tool highlighting
#185 | closed | DRAWING-CAM-IO-UI | Shift Middle Click changes perspective in discrete steps now, not freely like before
#186 | closed | DRAWING-CAM-IO-UI | AppImage on Cachy Linux (ARCH derivative) fails to start
#198 | closed | DRAWING-CAM-IO-UI | [Bug] SpacMouse Rotation somehow wrong
#200 | open | DRAWING-CAM-IO-UI | [Feature] .f3z File Import Possible?
#216 | closed | DRAWING-CAM-IO-UI | APPEARANCE: » seems like it would collapse the panel but doesn't
#221 | open | DRAWING-CAM-IO-UI | issue with trying to set mm scaling to inches
#225 | closed | DRAWING-CAM-IO-UI | Windows installer uses old project name for exe and folders.
#235 | open | DRAWING-CAM-IO-UI | Shift selecting faces is buggy.
#242 | closed | DRAWING-CAM-IO-UI | Issue report sendings
#254 | closed | DRAWING-CAM-IO-UI | Performance: reuse analytic sketch segments when certifying extrusions
#255 | open | DRAWING-CAM-IO-UI | Performance: profile remaining cold file-open geometry rebuild costs
#261 | closed | DRAWING-CAM-IO-UI | Rectangular pattern of a hole cut scales super-linearly: 128 holes take 30 s, 512 holes do not finish
#272 | closed | DRAWING-CAM-IO-UI | UI scale issue
#288 | closed | DRAWING-CAM-IO-UI | Toolbar drop-downs: highlight the row under the pointer
#289 | closed | DRAWING-CAM-IO-UI | Toolbar: drag commands from drop-downs onto the toolbar, and make customisation discoverable
#290 | closed | DRAWING-CAM-IO-UI | Preferences window: width jumps between pages, buttons not right-aligned, gear icon looks like a sun
#292 | open | DRAWING-CAM-IO-UI | Rebuild the reference oracle on analytic and FreeCAD reference values
#304 | closed | DRAWING-CAM-IO-UI | Navigation preset "Alias" should use Alias's mouse bindings
#321 | closed | DRAWING-CAM-IO-UI | Home page: Save, Undo, Redo, Export and design shortcuts act on the hidden design
#323 | closed | DRAWING-CAM-IO-UI | Toolbar: pinning is capped; dragging more commands up from a drop-down stops adding buttons
#328 | closed | DRAWING-CAM-IO-UI | App no longer starts after choosing an anti-aliasing (MSAA) the graphics device doesn't support
#332 | closed | DRAWING-CAM-IO-UI | ## Bug: macOS app crashes on launch due to unsupported MSAA sample count
#333 | closed | DRAWING-CAM-IO-UI | Toolbar: a command dropped on a panel's label or under its buttons is silently not pinned
#356 | open | DRAWING-CAM-IO-UI | flashing windows
#359 | closed | DRAWING-CAM-IO-UI | Toolbar drop-downs taller than the window can't be scrolled
#361 | closed | DRAWING-CAM-IO-UI | Trackpad gestures: pinch to zoom, two-finger scroll to pan
#365 | closed | DRAWING-CAM-IO-UI | The application crashed with no information
#368 | closed | DRAWING-CAM-IO-UI | One extrude cutting many circles from one sketch still scales super-linearly: 128 circles take 60 s
#369 | closed | DRAWING-CAM-IO-UI | wgpu error: Validation Error
#375 | open | DRAWING-CAM-IO-UI | Viewport: no way to read absolute scale — add grid spacing control + labeled ticks (or a scale bar)
#409 | closed | DRAWING-CAM-IO-UI | Toolbar: a panel label too long for its panel overlaps the next one; one-button panels aren't centred
#465 | closed | DRAWING-CAM-IO-UI | Navigation: customizable mouse bindings, roll, middle double-click fit and keyboard view rotation, consistent in every mode
#469 | open | DRAWING-CAM-IO-UI | Uneven hostile-input coverage across the file importers (SVG and DXF have almost none)
#478 | closed | DRAWING-CAM-IO-UI | UI: zoom seems fixed and huge
#528 | closed | DRAWING-CAM-IO-UI | No way to apply a saved named view or a camera from scripts/MCP (view.save stores views, screenshot only takes standard views)
#530 | closed | DRAWING-CAM-IO-UI | MCP batch replies repeat the full body list after every step (no compact reply option)
#532 | closed | DRAWING-CAM-IO-UI | Physical materials: no user-defined materials (library is fixed to 14 entries)
#533 | closed | DRAWING-CAM-IO-UI | Assemblies: no way to insert another design as a linked component (STEP insert drops sheet-metal history and never updates)
#579 | closed | DRAWING-CAM-IO-UI | Save As keeps the design named "Untitled" (tab, later Save As and export defaults)
#611 | open | DRAWING-CAM-IO-UI | Desktop/MCP: long rebuild waits in a 350+ feature editing history (timings and improvement suggestions)
#612 | closed | DRAWING-CAM-IO-UI | STEP export: straight-edge fillets and extruded arcs are written as B-spline surfaces with oversized bounds (a 60 mm part reads as -34.9…94.9 mm in OCCT)
#620 | open | DRAWING-CAM-IO-UI | Render workspace: improve the integrated renderer, add backend selection, HDRI and PBR — implementation roadmap
#626 | open | DRAWING-CAM-IO-UI | Version-control-friendly designs: stable text form, semantic diff and merge
#633 | closed | DRAWING-CAM-IO-UI | Drop down menus flats too short on CREATE and MODIFY
#642 | closed | DRAWING-CAM-IO-UI | Render foundation: backend capabilities and request resolution for the existing renderer (R1)
#658 | closed | DRAWING-CAM-IO-UI | Render R2: persistent named setups and render-only document refresh
#660 | closed | DRAWING-CAM-IO-UI | Crash report
#661 | open | DRAWING-CAM-IO-UI | Crash: called `Option::unwrap()` on a `None` value
#673 | closed | DRAWING-CAM-IO-UI | Render R3: product scene and integrated Render workspace
#678 | closed | DRAWING-CAM-IO-UI | Scrolling the material list in the Physical Material dialog also zooms the model
#688 | open | DRAWING-CAM-IO-UI | [Feature Request] Native support for FreeCAD files (.FCStd)
#717 | open | DRAWING-CAM-IO-UI | [ feature/fix ] : view port navigation system
#719 | closed | DRAWING-CAM-IO-UI | [ feature ] : urdf export for robotics/mechatrionics virtual commitioning
#720 | closed | DRAWING-CAM-IO-UI | Web: Japanese (CJK) text renders as tofu when UI language is English
#724 | closed | DRAWING-CAM-IO-UI | S command search: a typed key repeats and stays down for the whole desktop
#742 | open | DRAWING-CAM-IO-UI | Booleans on scaled spheres: cost depends on the sphere's patch orientation (identical geometry, 2.5× wall time); mesh tolerance comes from vertex boxes
#743 | open | DRAWING-CAM-IO-UI | GUI control channel: each request waits for a GUI frame, and MCP `batch` is one round-trip per step, so batches cost N frames (1 s each when the window is off-screen)
#756 | closed | DRAWING-CAM-IO-UI | Render R4a: bounded jobs, cancellation and PNG export
#770 | open | DRAWING-CAM-IO-UI | inspect.continuity: G0/G1/G2 across edges, surface curvature combs and isocurves
#778 | closed | DRAWING-CAM-IO-UI | Accessibility - Colourblind Support
#784 | closed | DRAWING-CAM-IO-UI | STL export does not weld shared vertices: same body as 3MF has open edges in STL (52 open edges vs 0 in 3MF)
#785 | open | DRAWING-CAM-IO-UI | Add: Materials Library & Appearance Library
#786 | open | DRAWING-CAM-IO-UI | Material Library: Wood Grain Texture
#813 | open | DRAWING-CAM-IO-UI | Render R4b: bounded HDRI environments and PBR preview
#829 | closed | DRAWING-CAM-IO-UI | render: bounded cooperative Radiance HDR input foundation (R4b prerequisite)
#842 | open | DRAWING-CAM-IO-UI | Render: phased integration of existing settings, workspace, jobs and HDR input
#844 | closed | DRAWING-CAM-IO-UI | Join of two overlapping swept spiral walls: error, slow, or open body
#846 | closed | DRAWING-CAM-IO-UI | Libraries: first portable local appearance and physical-material catalog
#860 | open | DRAWING-CAM-IO-UI | [RFC] Drawings: community requirements and phased development
#869 | closed | DRAWING-CAM-IO-UI | Crash: wgpu error: Validation Error
#874 | closed | DRAWING-CAM-IO-UI | Toolbar dropdown size issue
#883 | closed | DRAWING-CAM-IO-UI | Loading large steps files causes hang and possible memory leak
#886 | closed | DRAWING-CAM-IO-UI | Export a design's timeline as a replayable command script (solidcraft-cli eval format)
#900 | closed | DRAWING-CAM-IO-UI | Render resources: bounded texture input admission
#919 | closed | DRAWING-CAM-IO-UI | Crash: wgpu error: Validation Error
#920 | closed | DRAWING-CAM-IO-UI | Crash: wgpu error: Validation Error
#922 | open | DRAWING-CAM-IO-UI | Materials: phased physical-property and PBR quality roadmap for wood, polymers, glass and metals
#923 | closed | DRAWING-CAM-IO-UI | Physical material cards: typed SI data, conditions, provenance and elastic stability validation
#924 | open | DRAWING-CAM-IO-UI | Robustness goldens: 16 of 96 seeds build differently on Windows (outcome flips included)
#972 | closed | DRAWING-CAM-IO-UI | Timeline and Feature History show a failed feature by colour only
#974 | closed | DRAWING-CAM-IO-UI | ui.render draws the light theme's colours while the window is in the dark theme
#975 | open | DRAWING-CAM-IO-UI | Curvature Map colour scale: its lightness rises and falls, and its upper part merges for red-green colour vision loss
#976 | open | DRAWING-CAM-IO-UI | Three different sets of X, Y and Z axis colours
#994 | closed | DRAWING-CAM-IO-UI | Physical materials: directional Young modulus for validated isotropic and orthotropic cards
#996 | closed | DRAWING-CAM-IO-UI | [Linux, GUI] Bug reporting window gets wider and wider without actually doing anything
#997 | closed | DRAWING-CAM-IO-UI | [macOS] Double-clicking .solidcraft file in Finder fails: "cannot open files in the format"
#1000 | closed | DRAWING-CAM-IO-UI | [Linux] Spacemouse should work without editing /etc/udev rules.
#1004 | open | DRAWING-CAM-IO-UI | Render R4b.1: shared surface contract and integrated opaque scalar PBR consumer
#1005 | closed | DRAWING-CAM-IO-UI | ui_layout dialog-width test can't find any dialog since per-title dialog ids
#1006 | open | DRAWING-CAM-IO-UI | Orbit Style: Orbit style does not match Fusion360/Solidworks/Inventor/Onshape/etc
#1012 | closed | DRAWING-CAM-IO-UI | GPU display: selected opaque faces show striped or speckled highlighting on Vulkan/MSAA4
#1015 | closed | DRAWING-CAM-IO-UI | feat: Document metadata field with namespaced extension keys
#1016 | open | DRAWING-CAM-IO-UI | bug: Design load accepts stale next_id and duplicate IDs, so new items can reuse existing IDs
#1017 | open | DRAWING-CAM-IO-UI | feat: Random string IDs in design files for git merges and non-Rust JSON tools
#1023 | open | DRAWING-CAM-IO-UI | HOLE creation hang the UI
#1033 | open | DRAWING-CAM-IO-UI | Step files not importing properly
#1034 | closed | DRAWING-CAM-IO-UI | Timeline features can't be copied and pasted from the GUI
#1038 | closed | DRAWING-CAM-IO-UI | A small hole on the Pi case takes 7.5 s, meshing its intersection edges
#1044 | open | DRAWING-CAM-IO-UI | Orbit point under cursor wanders off
#1048 | open | DRAWING-CAM-IO-UI | Materials: persist validated engineering cards with atomic edits and offline reopen
#1049 | closed | DRAWING-CAM-IO-UI | Straight edges drawn as many segments and uneven face hover after several cuts
#1052 | closed | DRAWING-CAM-IO-UI | Add Ukrainian language to the user interface
#1062 | open | DRAWING-CAM-IO-UI | Libraries: versioned portable PBR appearances and engineering material cards
#1063 | closed | DRAWING-CAM-IO-UI | Hovering a face shimmers instead of lighting the whole face
#1076 | closed | DRAWING-CAM-IO-UI | MCP screenshot fails on Windows when %TEMP% is an 8.3 short path (rejects its own temp dir)
#1088 | closed | DRAWING-CAM-IO-UI | macOS: resizing the window while an Open or Save dialog is open crashes the app
#1089 | closed | DRAWING-CAM-IO-UI | Sheet metal: the flat-pattern DXF writes ellipses, splines and text as 0.001 mm LINEs, in a file with no DXF version
#1090 | closed | DRAWING-CAM-IO-UI | Add Blender-style mouse navigation preset
#1093 | open | DRAWING-CAM-IO-UI | Circular pattern of a lofted join takes ~31 min (9 copies) with 8 boolean tolerance retries
#1094 | open | DRAWING-CAM-IO-UI | Materials: engineering-card inspector and explicit validated local import
#1097 | open | DRAWING-CAM-IO-UI | Face selection show glitches
#1098 | closed | DRAWING-CAM-IO-UI | Crash at startup on D3D12 with FXC: the GPU tessellation shader fails to compile (X3511)
#1104 | open | DRAWING-CAM-IO-UI | Crash: called `Option::unwrap()` on a `None` value
#1115 | closed | DRAWING-CAM-IO-UI | Title bar: inactive tabs have no outline; the hovered Home tab hides its label in the light theme
#1123 | open | DRAWING-CAM-IO-UI | Crash: SolidCraft 0.10.0 (started 2026-10-14T15:00:56Z) ended without a clean exit while running `solid.extrude`
#1145 | closed | DRAWING-CAM-IO-UI | UI: Russian Report a bug label overlaps the header preferences control
#1146 | open | DRAWING-CAM-IO-UI | SpaceMouse (3Dconnexion) not working on Linux Flatpak build, works in FreeCAD via spacenavd
#1147 | open | DRAWING-CAM-IO-UI | A 1,300-body design built over MCP takes minutes to build and show
#1153 | open | DRAWING-CAM-IO-UI | [RFC] Manufacture/CAM: 2.5D milling, toolpaths and G-code posts
#1156 | open | DRAWING-CAM-IO-UI | Windows: intermittent test-process abort in atio6axx.dll after passing app tests
#1161 | open | DRAWING-CAM-IO-UI | cadmpeg for file import/export
#1162 | closed | DRAWING-CAM-IO-UI | App icon not showing on CachyOS/Arch Linux
#1165 | closed | DRAWING-CAM-IO-UI | Parameters: Arrow keys stop parameter name editing instead of moving the cursor
#1170 | closed | DRAWING-CAM-IO-UI | Parameters: Expression field does not reset after adding a parameter
#1172 | closed | DRAWING-CAM-IO-UI | Linux: SpaceMouse roll and spin swapped when no 3DxWare profile exists (follow-up to #198)
#1175 | open | DRAWING-CAM-IO-UI | Feature Request / Working Patch — Unrestricted Free Orbit and Improved 3Dconnexion SpaceMouse Navigation
#1178 | closed | DRAWING-CAM-IO-UI | Parameters: Delete button for parameter lacks hover outline and shows unnecessary tooltip
#1181 | closed | DRAWING-CAM-IO-UI | STEP export: shell of a tapered rounded-rectangle extrusion writes the inner walls as zero-area B-splines at z = −61 (volume +46 % in OCCT)
#1186 | closed | DRAWING-CAM-IO-UI | Two tests fail on a Windows checkout: a CRLF parse in feature_walk_tests, an 8.3 TEMP path in libraries_tests
#1187 | closed | DRAWING-CAM-IO-UI | Parameters: Feature Request: Allow reordering of parameters after creation
#1192 | closed | DRAWING-CAM-IO-UI | macOS: crash (panic) when choosing File > Open, native file dialog re-enters the winit event handler
#1197 | closed | DRAWING-CAM-IO-UI | Feature pattern falls back to slow boolean retry on every instance when the body has any curved face (follow-up to #400)
#1198 | closed | DRAWING-CAM-IO-UI | STEP export: fillet and arc faces cut by an angled feature are written as B-splines with a bounding box ~2× the part (incl. an uncut arc end, see #612)
#1205 | closed | DRAWING-CAM-IO-UI | Unable to set Units to Inches
#1210 | open | DRAWING-CAM-IO-UI | Crash on launch on Surface Pro 7 (Windows 11 25H2)
#1218 | closed | DRAWING-CAM-IO-UI | Render textures: explicit bounded precision, orientation and ICC normalization
#1219 | open | DRAWING-CAM-IO-UI | Cut into a bore is 350x slower (0.2 s -> 73 s, 24 retries) when the bore edge has a fillet the cut doesn't touch
#1228 | closed | DRAWING-CAM-IO-UI | Browser: custom names that match a UI label are translated (a body named "Fillet" shows as "Vollrundung")
#1230 | open | DRAWING-CAM-IO-UI | Performance on complex part
#1237 | open | DRAWING-CAM-IO-UI | Render resources: portable design ownership and preparation lifetime
#1245 | closed | DRAWING-CAM-IO-UI | Scripted pictures can be framed like the MCP's: snapshot --camera and --view <saved view>, and grid: false / axes: false for the ground grid and the origin's axes
#1250 | open | DRAWING-CAM-IO-UI | Why are threads so inefficient?
#1265 | closed | DRAWING-CAM-IO-UI | STEP import fits files from other CAD systems: up for Y-up files and names for the bodies' names
#1266 | closed | DRAWING-CAM-IO-UI | Windows CI: startup journal rebase test cannot read its live locked path
#1278 | closed | DRAWING-CAM-IO-UI | Add support for Autodesk Inventor files
#1284 | closed | DRAWING-CAM-IO-UI | Sheet metal: the flat-pattern DXF puts every contour on one layer, draws the bend lines continuous and writes one body only
#1287 | closed | DRAWING-CAM-IO-UI | Display quality: finer tessellation for smooth curved faces and edges
#1288 | closed | DRAWING-CAM-IO-UI | Optional supersampling (SSAA) for a cleaner viewport on sub-4K screens
#1290 | closed | DRAWING-CAM-IO-UI | feat: Add a UI font setting to Preferences
#1327 | closed | DRAWING-CAM-IO-UI | Render textures: bounded CPU sampler for the first wood consumer
#1330 | closed | DRAWING-CAM-IO-UI | Windows: the UI tests hang or crash when the offscreen GPU tests start every backend side by side
#1332 | closed | DRAWING-CAM-IO-UI | Render textures: bounded linear mip preparation for the wood consumer
#1336 | closed | DRAWING-CAM-IO-UI | BUG: Spacemouse doesn't work properly on Web
#1339 | closed | DRAWING-CAM-IO-UI | Render textures: independent isotropic LOD reference from UV gradients
#1345 | open | DRAWING-CAM-IO-UI | Render materials: first design-owned opaque wood texture consumer
#1346 | closed | DRAWING-CAM-IO-UI | Render textures: independent perspective-correct UV differential reference
#1347 | closed | DRAWING-CAM-IO-UI | Decimal point (dot and comma)
#1349 | closed | DRAWING-CAM-IO-UI | Crash: SolidCraft 0.11.1 (started 2026-10-15T08:38:35Z) ended without a clean exit while running `solid.extrude`
#1351 | open | DRAWING-CAM-IO-UI | Crash: SolidCraft 0.11.1 (started 2026-10-15T09:20:24Z) ended without a clean exit while running `solid.combine`
#1354 | closed | DRAWING-CAM-IO-UI | Right-click menu falls out of the visible window area
#1357 | open | DRAWING-CAM-IO-UI | Original opaque material finish presets and reproducible demo
#1358 | closed | DRAWING-CAM-IO-UI | Midpoint lines only render one side until confirmed
#1368 | open | DRAWING-CAM-IO-UI | Render: optional CPU raytracing core and progressive integrator, with a later GPU path
#1369 | open | DRAWING-CAM-IO-UI | Document format 21: required extensions to prevent silent loss in published readers
#1373 | open | DRAWING-CAM-IO-UI | Crash: SolidCraft 0.11.1 (started 2026-10-15T09:54:22Z) ended without a clean exit between commands
#1374 | open | DRAWING-CAM-IO-UI | Importing a STEP file can run out of memory and abort: unbounded curve division while meshing a face
#1376 | closed | DRAWING-CAM-IO-UI | Web app: opening a STEP file freezes the page until the import ends (no progress, no Cancel)
#1380 | closed | DRAWING-CAM-IO-UI | Windows: with no 3DxWare profile, the SpaceMouse's roll and spin are swapped and tilting zooms in steps
#1381 | closed | DRAWING-CAM-IO-UI | Windows: resize borders sometimes have no grab area; dragging between monitors with different DPI breaks
#1383 | open | DRAWING-CAM-IO-UI | Metal hover pixel regression fails in parallel full-suite run but passes in isolation
#1385 | closed | DRAWING-CAM-IO-UI | Windows: render-export file-scope test fails with an 8.3 TEMP path
#1386 | closed | DRAWING-CAM-IO-UI | Importing a STEP (or any model file) always creates a new component
#1397 | open | DRAWING-CAM-IO-UI | External STEP corpus: nightly import checks on real-world files (tolerance-based, licence-clean)
#1399 | closed | DRAWING-CAM-IO-UI | Duplicate desktop entries/icons on Linux (AppImage)
#1411 | open | DRAWING-CAM-IO-UI | Crash: SolidCraft 0.10.0 (started 2026-10-15T12:29:01Z) ended without a clean exit while running `solid.coil`
#1413 | open | DRAWING-CAM-IO-UI | Add a search bar to the Model Tree Browser
#1434 | closed | DRAWING-CAM-IO-UI | Viewport: hidden edges show through faces (about 27 mm on a large model in orthographic, 0.16% of the distance in perspective), and thin faces z-fight
#1448 | closed | DRAWING-CAM-IO-UI | crash test startup_steps_survive_a_rebase fails on Windows: it reads the locked live journal through a second handle
#1452 | open | DRAWING-CAM-IO-UI | STEP複数ボディ読込みで周期円筒面のメッシュがトリム範囲を逸脱し、体積が変化する（v0.11.1）
#1454 | closed | DRAWING-CAM-IO-UI | Render workspace: camera controls ignore the navigation preset, orbit setting, keys and SpaceMouse
#1463 | open | DRAWING-CAM-IO-UI | SpaceMouse calibration in Preferences, and support for DIY 3D mice
#1464 | closed | DRAWING-CAM-IO-UI | Render preview fails with a pixel limit on 4K monitors
#1468 | open | DRAWING-CAM-IO-UI | Strange Seam when imported STEP
#1469 | closed | DRAWING-CAM-IO-UI | Crash: Failed to create staging buffer for index data. Index count: 31326. Required index buffer size: 125304. Actual size 150936 and capacity: 150936 (bytes)
#1470 | closed | DRAWING-CAM-IO-UI | No grids on other planes
#1478 | closed | DRAWING-CAM-IO-UI | Start page stays in English in every language
#1491 | closed | DRAWING-CAM-IO-UI | timeline.edit silently ignores unknown fields and reports success (recomputed: 0)
#1493 | open | DRAWING-CAM-IO-UI | Flat B-spline faces of an imported box render black
#1502 | closed | DRAWING-CAM-IO-UI | Auto Unit conversion
#1505 | closed | DRAWING-CAM-IO-UI | Import glTF / GLB files
#1522 | closed | DRAWING-CAM-IO-UI | UI: Dropdown menu does not close when clicking the same button again
#1539 | open | DRAWING-CAM-IO-UI | Agent chat: a context folder per design, the agent bound to its design, pop-out window, resume
#1541 | closed | DRAWING-CAM-IO-UI | Feature Request: SolidWorks-Style View Navigation Shortcuts
#1549 | closed | DRAWING-CAM-IO-UI | UI tests: held joint-drive slider preview times out under concurrent load
#1553 | open | DRAWING-CAM-IO-UI | Render interchange: bounded glTF/GLB scene export and optional .blend handoff
#1554 | closed | DRAWING-CAM-IO-UI | Windows: agent discovery search-order test expects an extensionless executable
#1559 | open | DRAWING-CAM-IO-UI | Crash: not implemented: intersection curve cannot connect by homotopy
#1569 | closed | DRAWING-CAM-IO-UI | Glossary: one ubiquitous language for SolidCraft's terms
#1621 | open | DRAWING-CAM-IO-UI | Research Request for Plugin: CAM (Machining & G-code)
#1625 | open | DRAWING-CAM-IO-UI | Research Request for Plugin: DfAM (Design for Additive Manufacturing)
#1626 | open | DRAWING-CAM-IO-UI | Research Request for Plugin: 3D Printing
#1631 | open | DRAWING-CAM-IO-UI | Feature request: Model-anchored, screen-facing notes
#1638 | closed | DRAWING-CAM-IO-UI | Crash on startup: Black screen and system freeze using DX12/AMD (v0.13.1)
#1645 | closed | DRAWING-CAM-IO-UI | 3D mouse: tilting the cap to the right rolls the model anticlockwise
#1651 | open | DRAWING-CAM-IO-UI | Render: bounded rectangular area lights and CPU soft shadows (RT3)
#1654 | open | DRAWING-CAM-IO-UI | Render: bounded mirror and rough opaque scene reflections
#1655 | open | DRAWING-CAM-IO-UI | Render: thin dielectric and closed-volume transmission with absorption
#1656 | open | DRAWING-CAM-IO-UI | Render: procedural roughness and bump surface effects with real CPU consumption
#1661 | closed | DRAWING-CAM-IO-UI | Crash: tried to handle event while another event is currently being handled
#1667 | closed | DRAWING-CAM-IO-UI | measure: volume and centre of mass wrong after fillets (+11 492 mm³ on a 472 000 mm³ body); STEP of the same body measures correctly
#1677 | closed | DRAWING-CAM-IO-UI | MCP `screenshot` over `--connect`: when the app window presents no frame, the default (window) capture waits ~8 s and silently returns a model render
#1688 | closed | DRAWING-CAM-IO-UI | [Feature Request] Add Brazilian Portuguese language.
#1693 | open | DRAWING-CAM-IO-UI | Render: bounded equirectangular HDR environment sampling and CPU lighting integration
#1694 | open | DRAWING-CAM-IO-UI | Render: anisotropic GGX response and matched sampling for brushed surfaces
#1695 | open | DRAWING-CAM-IO-UI | Render: explicit bounded surface emission in CPU images
#1696 | open | DRAWING-CAM-IO-UI | Render: bounded linear film accumulation for progressive CPU rendering
#1706 | open | DRAWING-CAM-IO-UI | STEP import: some bodies import with coordinates ~10^6 mm and negative volume
#1708 | open | DRAWING-CAM-IO-UI | F3D import: editable history, explicit partial recovery, and opening regressions
#1710 | closed | DRAWING-CAM-IO-UI | File Name - Save As Problem
#1722 | open | DRAWING-CAM-IO-UI | Render: extensible starter appearance pack for wood, metals, glass, plastics and space scenes
#1723 | open | DRAWING-CAM-IO-UI | Render: bounded point and spot lights with a real CPU visibility consumer
#1724 | open | DRAWING-CAM-IO-UI | Render: bounded LED strip emitters reusing rectangular area-light sampling
#1750 | closed | DRAWING-CAM-IO-UI | Czech (cs) UI translation
#1751 | open | DRAWING-CAM-IO-UI | Crash: SolidCraft 0.10.0 (started 2026-10-14T12:15:09Z) ended without a clean exit between commands
#1766 | closed | DRAWING-CAM-IO-UI | Crash: SolidCraft 0.14.0 (started 2026-10-16T15:45:19Z) ended without a clean exit during startup (window, 0.0 s after launch)
#1767 | closed | DRAWING-CAM-IO-UI | Windows: Parameters editor duplicates typed characters and pasted text
#1770 | closed | DRAWING-CAM-IO-UI | Galician (gl) UI translation
#1777 | open | DRAWING-CAM-IO-UI | Request for addition of support for importing .png and .jpg/.jepg
#1784 | open | DRAWING-CAM-IO-UI | STEP import of ~38 MB file exhausts RAM, process killed by kernel OOM
#1785 | open | DRAWING-CAM-IO-UI | Middle mouse button does not allow for dragging the view in the SolidWorks-like navigation mode
#1787 | closed | DRAWING-CAM-IO-UI | Viewport: Ground plane changes to different orientations when moving the camera
#1789 | open | DRAWING-CAM-IO-UI | Crash on startup: Black screen freeze on DX12/AMD — Surface not configured for presentation (v0.14.0)
#1800 | closed | DRAWING-CAM-IO-UI | The Preferences window doesn't open when font is changed
#1802 | closed | DRAWING-CAM-IO-UI | Mixed-number inch input: `2 1/8"` gives 0.25" (2 × 1/8") instead of 2.125"
#1809 | open | DRAWING-CAM-IO-UI | Shortcut recorder: allow time to assemble modifier chords
#1811 | open | DRAWING-CAM-IO-UI | Crash: SolidCraft 0.14.0 (started 2026-10-16T19:20:36Z) ended without a clean exit between commands
#1819 | closed | DRAWING-CAM-IO-UI | fix(web): browser try link redirects to a missing Pages app
#1821 | closed | DRAWING-CAM-IO-UI | fix(docs): README MCP tool list omits crash_reports
#1856 | open | DRAWING-CAM-IO-UI | kernel-perf: union of a 20-tooth lofted helical pinion takes minutes
#1867 | open | DRAWING-CAM-IO-UI | 中文显示问题
#1869 | open | DRAWING-CAM-IO-UI | Crash: wgpu error: Validation Error
#1870 | closed | DRAWING-CAM-IO-UI | Windows MSI installation in a custom directory is incorrectly detected as portable ZIP
#1881 | closed | DRAWING-CAM-IO-UI | UI misalignment
#1890 | closed | DRAWING-CAM-IO-UI | Place an occurrence or an inserted STEP file by a transformation matrix
#1902 | closed | DRAWING-CAM-IO-UI | Bug: Thai characters rendered as square boxes in AI chat
#1905 | closed | DRAWING-CAM-IO-UI | Stand this part up? offer: its buttons publish no ui.handles, and Leave it has no control equivalent
#1932 | open | DRAWING-CAM-IO-UI | Modeling: PULL face opens EXTRUDE dialogue
#1934 | closed | DRAWING-CAM-IO-UI | Step import fails
#1935 | closed | DRAWING-CAM-IO-UI | Crash: wgpu error: Validation Error
#1936 | open | DRAWING-CAM-IO-UI | Missing measurement units in dimension input fields (Plastic Lip, Boss, Snap Fit)
#1948 | open | DRAWING-CAM-IO-UI | R6: progressive final mode as a GPU path-tracing executor on the integrated backend
#1957 | open | DRAWING-CAM-IO-UI | Crash: SolidCraft 0.14.0 (started 2026-10-17T10:20:11Z) ended without a clean exit while running `file.export`
#1959 | closed | DRAWING-CAM-IO-UI | Changing font resulted in preference window to become unusable.
#1962 | open | DRAWING-CAM-IO-UI | Modeling: unexpected result - trying to create a Construction plane - Type Mid, not presenting dialog box to select surfaces.
#1973 | open | DRAWING-CAM-IO-UI | Poor UI performance Linux - NVIDIA
#1974 | closed | DRAWING-CAM-IO-UI | Importing Steam Frame CAD/Step File fails
#1978 | closed | DRAWING-CAM-IO-UI | File selection defaults to "Designs, CAD files and meshes" for svg insertions etc.
#1985 | closed | DRAWING-CAM-IO-UI | STEP export never finishes on a small box/cylinder part (STL and eval fine); hangs the app in MCP live mode
#1993 | closed | DRAWING-CAM-IO-UI | Grid jumps between the ground and an upright plane while orbiting near level
#1998 | closed | DRAWING-CAM-IO-UI | Crash on macos because the dmg was not signed
#2006 | closed | DRAWING-CAM-IO-UI | Crash: SolidCraft 0.14.0 (started 2026-10-17T12:38:26Z) ended without a clean exit while running `sketch.drag`
#2007 | closed | DRAWING-CAM-IO-UI | macOS Touchpad: Option to Reverse Horizontal Pan Direction
#2016 | open | DRAWING-CAM-IO-UI | Ribbon redesign: implementation tracker
#2017 | closed | DRAWING-CAM-IO-UI | Ribbon redesign: Tokens: spacing, radius, type scale and per-tab accents
#2018 | closed | DRAWING-CAM-IO-UI | Ribbon redesign: Icons: fill the gaps left by main and add the flat assistant icons
#2019 | closed | DRAWING-CAM-IO-UI | Ribbon redesign: Per-tab accent behaviour
#2020 | open | DRAWING-CAM-IO-UI | Ribbon redesign: Application bar
#2021 | open | DRAWING-CAM-IO-UI | Ribbon redesign: Assistant button, activity card and setup card
#2022 | closed | DRAWING-CAM-IO-UI | Ribbon redesign: Ribbon data: new tab order and promoted commands
#2023 | open | DRAWING-CAM-IO-UI | Ribbon redesign: Ribbon painting: workspace card, ribbon card, section lists, search
#2024 | closed | DRAWING-CAM-IO-UI | Ribbon redesign: Floating shell: browser card, compass, docked dialogs, history chips
#2025 | closed | DRAWING-CAM-IO-UI | Ribbon redesign: Dense dialog kit
#2026 | closed | DRAWING-CAM-IO-UI | Ribbon redesign: Workspace switcher card and menu
#2027 | open | DRAWING-CAM-IO-UI | Ribbon redesign: Start page becomes the File view
#2028 | closed | DRAWING-CAM-IO-UI | Ribbon redesign: Design and restyle the remaining surfaces
#2029 | closed | DRAWING-CAM-IO-UI | Ribbon redesign: Compact layout preference
#2030 | open | DRAWING-CAM-IO-UI | Ribbon redesign: Dark mode and colour-vision pass; book screenshots
#2037 | open | DRAWING-CAM-IO-UI | Multi-face extrusion: slow preview, unresponsive confirmation, and failing STEP cuts
#2049 | open | DRAWING-CAM-IO-UI | Crash: SolidCraft 0.14.0 (started 2026-10-17T12:53:55Z) ended without a clean exit between commands
#2052 | open | DRAWING-CAM-IO-UI | dxf import issue
#2053 | closed | DRAWING-CAM-IO-UI | Drawing W01a: validated section geometry and frame contract
#2054 | closed | DRAWING-CAM-IO-UI | Drawing W09a: share existing length display formatting below the UI
#2058 | open | DRAWING-CAM-IO-UI | Crash: SolidCraft 0.14.0 (started 2026-10-17T14:04:26Z) ended without a clean exit between commands
#2061 | closed | DRAWING-CAM-IO-UI | UI scenario: target the colour-vision selector after scrolling Preferences
#2063 | open | DRAWING-CAM-IO-UI | UI: Windows cursor is offset from the actual section
#2065 | open | DRAWING-CAM-IO-UI | Crash: SolidCraft 0.14.0 (started 2026-10-17T14:33:05Z) ended without a clean exit between commands
#2066 | closed | DRAWING-CAM-IO-UI | The preferences window isn't opening.
#2076 | open | DRAWING-CAM-IO-UI | Rectangular pattern of a cylinder join scales ~quadratically: 100 pins 84 s, 225 pins 527 s (one-sketch extrude: 5 s / 12 s; patterned cut: 3 s)
#2077 | open | DRAWING-CAM-IO-UI | Tracker: 3D mouse (SpaceMouse) support across platforms
#2078 | open | DRAWING-CAM-IO-UI | Tracker: free / trackball orbit
#2080 | open | DRAWING-CAM-IO-UI | Tracker: units shown on dimensions and inputs
#2084 | closed | DRAWING-CAM-IO-UI | Changing font
#2088 | open | DRAWING-CAM-IO-UI | Crash: SolidCraft 0.14.0 (started 2026-10-17T09:00:14Z) ended without a clean exit between commands
#2089 | closed | DRAWING-CAM-IO-UI | Import bug - 3mf with multiple parts gives a error
#2105 | open | DRAWING-CAM-IO-UI | Drawing W05a: exact hidden-line investigation (orthographic, analytic subset)
#2108 | closed | DRAWING-CAM-IO-UI | Toolbar hides icons and leaves a blank gap on medium-width windows
#2112 | open | DRAWING-CAM-IO-UI | Navigating on a MacBook with a touchpad: unexpected behavior
#2113 | open | DRAWING-CAM-IO-UI | Crash reports: tell hangs from crashes, readable backtraces, a replay in the issue, and native crash capture on Linux and macOS
#2124 | open | DRAWING-CAM-IO-UI | [Feature Request] Replace or enhance Axis Gizmo with a 3D ViewCube for improved navigation
#2131 | open | DRAWING-CAM-IO-UI | probleme de menu prferences
#2144 | open | DRAWING-CAM-IO-UI | Perspective/Orthographic toggle button *Sometimes* does nothing
#2162 | open | DRAWING-CAM-IO-UI | Crash: wgpu error: Validation Error
#2163 | open | DRAWING-CAM-IO-UI | Procedural material patterns (no stored images), with Poly Haven CC0 scans as references
#2168 | open | DRAWING-CAM-IO-UI | Modeling: Crash after confirm a modeling operation
#2172 | open | DRAWING-CAM-IO-UI | [Feature Request] VR mode via OpenXR (cross-platform)
#2181 | open | DRAWING-CAM-IO-UI | Crash: SolidCraft 0.15.0 (started 2026-10-17T19:40:00Z) ended without a clean exit while running `inspect.interference`
#2195 | open | DRAWING-CAM-IO-UI | Crash: Failed to get mapped range for buffer created with mapped_at_creation: MapRangeError("Validation Error\n\nCaused by:\n Buffer with 'sc_capture_backgroun
#2198 | open | DRAWING-CAM-IO-UI | Feature Request: Replace current coordinate axis with a FreeCAD-style navigation cube
#2200 | closed | DRAWING-CAM-IO-UI | Crash: wgpu error: Validation Error
#2201 | closed | DRAWING-CAM-IO-UI | Center Point Arc: on-canvas input follows cursor and blocks picking the end point
#2211 | open | DRAWING-CAM-IO-UI | File selector failing to open on NixOS
#2217 | open | DRAWING-CAM-IO-UI | [FEAT] drag the mouse to change length values on the fly
#2218 | open | DRAWING-CAM-IO-UI | Add search to settings pane
#2219 | open | DRAWING-CAM-IO-UI | Failure to undo hide commands
#2223 | closed | DRAWING-CAM-IO-UI | Crash: wgpu error: Validation Error
#2228 | open | DRAWING-CAM-IO-UI | [FEAT] [PLASTIC] add 3d printing utilities for better prints
#2232 | open | DRAWING-CAM-IO-UI | [CHANGE] make parameters and configuration accessible all the time
#2240 | open | DRAWING-CAM-IO-UI | No edit preferences
#2244 | closed | DRAWING-CAM-IO-UI | Bug report dialog: retain margins on resize and distinguish answers from diagnostics
#2255 | closed | DRAWING-CAM-IO-UI | Custom colour themes: import/export theme files, Gruvbox built in
#2257 | open | DRAWING-CAM-IO-UI | Changed interface font to Inter, not pref pane is inaccessible.
#2261 | open | DRAWING-CAM-IO-UI | Font change breaks settings menu
#2273 | open | DRAWING-CAM-IO-UI | Check that a parametric design holds across a parameter's range
#2300 | open | DRAWING-CAM-IO-UI | Importing DXF failed
#2304 | open | DRAWING-CAM-IO-UI | Send STL to a slicer on a LAN server
#2306 | open | DRAWING-CAM-IO-UI | Support Openaxis 3d mice (rotatrix)
#2314 | open | DRAWING-CAM-IO-UI | Inspect: passive measurement readout for the current selection
#2316 | open | DRAWING-CAM-IO-UI | Crash: SolidCraft 0.15.0 (started 2026-10-18T00:41:00Z) ended without a clean exit between commands
#2318 | open | DRAWING-CAM-IO-UI | Crash: SolidCraft 0.15.0 (started 2026-10-17T21:44:14Z) ended without a clean exit while running `solid.fillet`
#2319 | open | DRAWING-CAM-IO-UI | Crash: Failed to locate position. This is a bug in spade.
#2320 | open | DRAWING-CAM-IO-UI | Feature request: Allow user to control numerical input by dragging a Blender-style slider or pressing UpArrow/DownArrow
#2324 | open | DRAWING-CAM-IO-UI | Crash: wgpu error: Validation Error
#2325 | open | DRAWING-CAM-IO-UI | Feature request: Show the available options for the next step in the status line (or in heads-up display style tip)
#2329 | closed | DRAWING-CAM-IO-UI | Snapshot: a see-through appearance is drawn opaque in every saved picture, so a tool body cannot be seen inside its target
#2332 | open | DRAWING-CAM-IO-UI | Crash: SolidCraft 0.15.0 (started 2026-10-18T03:53:58Z) ended without a clean exit between commands
#2334 | open | DRAWING-CAM-IO-UI | Linux/Hyprland: pressing the compositor shortcut Super+S with SolidCraft focused blocks input in every window until SolidCraft is closed
#2341 | open | DRAWING-CAM-IO-UI | Pattern copies of a chamfer or fillet are blended one copy at a time: a 6 × 5 grid of chamfered bosses takes 57 s (13 s as one feature)
#2344 | closed | DRAWING-CAM-IO-UI | STEP import garbles CJK names: adjacent \X2\ runs not decoded, raw GBK bytes become U+FFFD
#2346 | open | DRAWING-CAM-IO-UI | Cannot import Autodesk Fusion Archive files (.f3d)
#2350 | open | DRAWING-CAM-IO-UI | Crash: SolidCraft 0.15.0 (started 2026-10-18T05:10:50Z) ended without a clean exit while running `solid.loft`
#2358 | open | DRAWING-CAM-IO-UI | Pattern of countersunk holes slows roughly quadratically: 135 countersinks take 19 min (135 plain holes: 21 s)
#2361 | open | DRAWING-CAM-IO-UI | Feature request: macOS please make use of menubar
#2363 | open | DRAWING-CAM-IO-UI | Windows in a VMware VM: crash in vm3dgl64.dll (3D off) or black window (3D on) via OpenGL fallback
#2365 | open | DRAWING-CAM-IO-UI | Linux spacenavd: twist and tilt left/right swapped (#1172 again, via the spacenavd reader from #1825)
#2377 | open | DRAWING-CAM-IO-UI | Crash: SolidCraft 0.15.0 (started 2026-10-18T07:44:14Z) ended without a clean exit between commands
#2395 | open | DRAWING-CAM-IO-UI | Crash: SolidCraft 0.15.0 (started 2026-10-18T08:18:48Z) ended without a clean exit while running `solid.extrude`
#2410 | open | DRAWING-CAM-IO-UI | Section Analysis can't be turned off from its dialog: OK refuses without a plane, Cancel keeps the section
#2413 | open | DRAWING-CAM-IO-UI | Opening an already-open model creates duplicate tabs instead of activating its tab
#2415 | open | DRAWING-CAM-IO-UI | Preferences menu is not working properly
#2451 | open | DRAWING-CAM-IO-UI | Crash: SolidCraft 0.15.0 (started 2026-10-18T10:09:11Z) ended without a clean exit while running `solid.thread`
#2455 | open | DRAWING-CAM-IO-UI | Drawing W06a: exact orthographic hidden lines for the analytic subset
#2456 | open | DRAWING-CAM-IO-UI | Crash: SolidCraft 0.15.0 (started 2026-10-18T07:48:05Z) ended without a clean exit between commands
#2458 | open | DRAWING-CAM-IO-UI | Right-click menus often close in the frame they open: the opening click's own press counts as a click outside
#2467 | open | DRAWING-CAM-IO-UI | Renaming a body recomputes every feature after it, on the window thread (minutes with a large pattern)
#2469 | open | DRAWING-CAM-IO-UI | 3DConnexion Space Mouse Pro not working on ARM macOS
#2470 | open | DRAWING-CAM-IO-UI | Crash: SolidCraft 0.15.0 (started 2026-10-17T22:09:53Z) crashed with EXCEPTION_STACK_OVERFLOW (0xC00000FD) in ntdll.dll+0x3213F while running `solid.thread`
#2475 | open | DRAWING-CAM-IO-UI | 3dware Spacemouse Software Changes not working
#2481 | closed | DRAWING-CAM-IO-UI | Crash: wgpu error: Validation Error
#2499 | open | DRAWING-CAM-IO-UI | Movement switches axes on Spacemouse
#2501 | closed | DRAWING-CAM-IO-UI | Languages: English (UK) as its own option, with one spelling in each English
#2504 | open | DRAWING-CAM-IO-UI | Polish localization of the program
#2520 | closed | DRAWING-CAM-IO-UI | Browser (tree) panel on the left can't be resized
#2528 | open | DRAWING-CAM-IO-UI | Rendering artifacts (triangular wedges) near seam lines on cylindrical faces
#2536 | open | DRAWING-CAM-IO-UI | einstellungen geht nicht mehr . es kommt nur noch ein label mit einer aufschrift und dann kommt man nicht in die einstellungen.
#2544 | closed | DRAWING-CAM-IO-UI | S command search: names, places and matching are English in every language
#2548 | open | DRAWING-CAM-IO-UI | Performance overlay: frame-time graph and histogram, triangles, draw calls
#2550 | open | DRAWING-CAM-IO-UI | Browser page showing 404 error
#2552 | open | DRAWING-CAM-IO-UI | Crash: SolidCraft 0.14.0 (started 2026-10-16T16:59:44Z) ended without a clean exit while running `solid.coil`
#2570 | closed | DRAWING-CAM-IO-UI | Modeling: Lofting between some shapes causes the application to freeze.
#2571 | open | DRAWING-CAM-IO-UI | Preferences: changed font, window is collapsed
#2578 | open | DRAWING-CAM-IO-UI | AI preferences: an agent shows "signed in" next to its own "Sign-in needed" (the chip checks only that a login file exists)
#2579 | closed | DRAWING-CAM-IO-UI | Boolean: a small hole in a large STEP body's face can be lost when the face is divided
#2582 | open | DRAWING-CAM-IO-UI | AI preferences: provider presets from models.dev, and a model choice per agent in Preferences
#2595 | open | DRAWING-CAM-IO-UI | 3D Printing Slicer Handoff
#2604 | open | DRAWING-CAM-IO-UI | Add more settings for Snap to Grid, Line coloring, Hover to snap to equal distance.
#2605 | open | DRAWING-CAM-IO-UI | SVG IMPORT: Palette missing
#2614 | open | DRAWING-CAM-IO-UI | SolidCraft fires up unexpected usb events
#2648 | open | DRAWING-CAM-IO-UI | Option to turn off AntiAliased text
#2651 | open | DRAWING-CAM-IO-UI | Options: Changing default font breaks Preferences dialog
#2652 | open | DRAWING-CAM-IO-UI | Split Manufacture megapr (#1782) into a stacked, interface-first PR tree
#2664 | open | DRAWING-CAM-IO-UI | Black-box coverage: STEP and IGES import and export (3 of 18 sampled mutants caught)
#2674 | open | DRAWING-CAM-IO-UI | Switching fonts makes preferences dialog permanently unusable
#2678 | closed | DRAWING-CAM-IO-UI | Deleting a feature freezes the window while its dependency check recomputes the timeline
#2682 | open | DRAWING-CAM-IO-UI | Perspective with Ortho Faces View
#2685 | open | DRAWING-CAM-IO-UI | Preferences: Unexpected Behavior After Font Selection
#2687 | open | DRAWING-CAM-IO-UI | Solidcraft freezing during part modelling
#2701 | closed | DRAWING-CAM-IO-UI | 3D Connexion Space Mouse not working
#2702 | open | DRAWING-CAM-IO-UI | Crash: SolidCraft 0.15.0 (started 2026-10-18T17:04:16Z) ended without a clean exit between commands
#2723 | closed | DRAWING-CAM-IO-UI | Parameters window: large frame-rate drop on big parametric designs
#2733 | open | DRAWING-CAM-IO-UI | FPS drops when opening Parameters dialog in large parametric design (0.15.0, Linux)
#2742 | open | DRAWING-CAM-IO-UI | Navigation preset: HFSS (classic) — modifier + left drag to orbit, pan and zoom, Alt + double-click for standard views
#2744 | open | DRAWING-CAM-IO-UI | Crash: SolidCraft 0.15.0 (started 2026-10-18T20:07:24Z) ended without a clean exit between commands
#2748 | open | DRAWING-CAM-IO-UI | Parameter import fails on forward references
#2757 | closed | DRAWING-CAM-IO-UI | Parameter autocomplete matches only leading characters, not substrings
#2758 | open | DRAWING-CAM-IO-UI | Feature Request: Expanded Tooltips with Written Explanations and Visual Previews (Like Fusion 360)
#2763 | closed | DRAWING-CAM-IO-UI | Keep status messages: a Messages window to read and copy them again
#2776 | closed | DRAWING-CAM-IO-UI | parameters.import grows with the square of the row count; parameters.change takes one name
#2778 | open | DRAWING-CAM-IO-UI | Linux spacenavd: view keeps creeping after release on relative-axis 3D mice (Space Navigator 046d:c626)
#2779 | closed | DRAWING-CAM-IO-UI | Reverse horizontal pan direction checked or not checked change nothing
#2786 | open | DRAWING-CAM-IO-UI | Rotation using 3DConnection SpaceNavigator stops
#2796 | open | DRAWING-CAM-IO-UI | Crash: SolidCraft 0.11.1 (started 2026-10-18T21:52:14Z) ended without a clean exit while running `solid.thread`
#2806 | closed | DRAWING-CAM-IO-UI | Program Fails to Launch on Win 11/AMD 5700G System
#2807 | open | DRAWING-CAM-IO-UI | inspect.clearance measures every pair again when only one part moved
#2809 | open | DRAWING-CAM-IO-UI | CORE One STEP import: periodic UV boundary expansion causes ~7-minute orientation meshes repeated per instance
#2811 | open | DRAWING-CAM-IO-UI | kernel-perf: filleting many edges (grille rib roots) grows faster than N²: a whole-body mesh per edge lookup
#2814 | open | DRAWING-CAM-IO-UI | STEP import: the Prusa Core One full assembly takes about an hour to open
#2817 | open | DRAWING-CAM-IO-UI | UI: mouse pointer button hovers and clicks off position - no action
#2872 | closed | DRAWING-CAM-IO-UI | Modeling: Step file model unexpected result
#2884 | closed | DRAWING-CAM-IO-UI | Autosave covers only the active tab: unsaved changes in other tabs are lost on a crash or hang
#2888 | closed | DRAWING-CAM-IO-UI | OpenGL startup crash: ambient-occlusion shader uses unsupported depth-texture textureLoad (WSL2)
#2891 | open | DRAWING-CAM-IO-UI | Sweep cut crashes website
#2913 | open | DRAWING-CAM-IO-UI | Crash: SolidCraft 0.15.0 (started 2026-10-19T04:26:54Z) ended without a clean exit between commands
#2914 | open | DRAWING-CAM-IO-UI | Insert image with 2-point canvas calibration
#2920 | closed | DRAWING-CAM-IO-UI | Headers for User Parameters do not align with entry fields in Parameters dialog
#2922 | closed | DRAWING-CAM-IO-UI | Assembly: joint motion re-tessellates unchanged bodies and stalls the viewport
#2934 | closed | DRAWING-CAM-IO-UI | Mouse gestures: a customizable ring of tools on the right button (SolidWorks mouse mode)
#2943 | closed | DRAWING-CAM-IO-UI | Links in update dialogue do not work.
#2955 | closed | DRAWING-CAM-IO-UI | Some arrow buttons show empty boxes: ▲ ▼ ↑ ↓ are not in the built-in fonts
#2960 | open | DRAWING-CAM-IO-UI | Crash: SolidCraft 0.16.2 (started 2026-10-19T07:53:42Z) ended without a clean exit between commands
#2962 | open | DRAWING-CAM-IO-UI | Crash: SolidCraft 0.12.0 (started 2026-10-15T19:27:32Z) ended without a clean exit during startup (graphics instance, 0.0 s after launch)
#2967 | open | DRAWING-CAM-IO-UI | Crash: SolidCraft 0.16.2 (started 2026-10-19T08:18:17Z) ended without a clean exit while running `component.properties`
#2974 | open | DRAWING-CAM-IO-UI | Counterbored hole + pattern ~8× slower when the body has a joined cylinder elsewhere (follow-up to #1197, non-planar tool)
#2978 | open | DRAWING-CAM-IO-UI | OFFSET MOUSE: the pointing mouse is offset frome real cordinates
#2989 | open | DRAWING-CAM-IO-UI | Crash: SolidCraft 0.16.2 (started 2026-10-19T09:05:14Z) ended without a clean exit between commands
#2992 | open | DRAWING-CAM-IO-UI | solid.combine: join of disk and 36 patterned twisted loft blades blocks for minutes (0.16.1)
#2998 | closed | DRAWING-CAM-IO-UI | 3MF and DXF budget tests check wall-clock time and fail on a busy machine
#3005 | open | DRAWING-CAM-IO-UI | Browser: selecting a body in the viewport does not highlight it in the Browser tree
#3021 | open | DRAWING-CAM-IO-UI | Settings, :Two-finger scroll pans option changes nothing
#3024 | open | DRAWING-CAM-IO-UI | [Bug]: Application crashes at startup on macOS 12.7.6 (Intel GPU) due to wgpu Device(Lost) / validation errors
#3043 | open | DRAWING-CAM-IO-UI | Windows 11: after the screen turns off and the PC wakes, the app draws into a quarter of the window
#3044 | closed | DRAWING-CAM-IO-UI | Renaming a body freezes the app for a while; a rename shouldn't rebuild anything
#3045 | closed | DRAWING-CAM-IO-UI | Latest command repeat
#3049 | open | DRAWING-CAM-IO-UI | Doesn't Orbit with Shift key
#3054 | open | DRAWING-CAM-IO-UI | Crash: SolidCraft 0.16.2 (started 2026-10-19T12:01:04Z) ended without a clean exit between commands
#3055 | closed | DRAWING-CAM-IO-UI | Add a context menu to file tabs
#3057 | open | DRAWING-CAM-IO-UI | Crash: SolidCraft 0.16.2 (started 2026-10-19T09:14:31Z) ended without a clean exit between commands
#3066 | open | DRAWING-CAM-IO-UI | Crash: SolidCraft 0.16.2 (started 2026-10-19T12:13:19Z) ended without a clean exit while running `solid.extrude`
#3080 | open | DRAWING-CAM-IO-UI | Startup fails with RequestDeviceError: Device(Lost) on Intel HD 4000 (MacBook Air 2012, macOS Sequoia 15.8.1 via OCLP)
#3083 | open | DRAWING-CAM-IO-UI | Linux: blurry UI under fractional scaling on XWayland; choose the window system in Preferences
#3085 | open | DRAWING-CAM-IO-UI | Crash when selecting edge for Fillet/Chamfer after Combine with imported STEP body Modeling: unexpected result
#3086 | open | DRAWING-CAM-IO-UI | impossible d'ouvrir un fichier freecad
#3087 | closed | DRAWING-CAM-IO-UI | The grid disapears when you zoom in to far
#3088 | open | DRAWING-CAM-IO-UI | Crash: SolidCraft 0.16.2 (started 2026-10-19T12:37:07Z) ended without a clean exit while running `solid.coil`
#3108 | open | DRAWING-CAM-IO-UI | STEP files take long to load
#3120 | closed | DRAWING-CAM-IO-UI | Step file not imported correctly
#3142 | closed | DRAWING-CAM-IO-UI | ATTRIBUTION.md: most of the Files table renders as plain text on GitHub
#3151 | open | DRAWING-CAM-IO-UI | GPU ray tracer roadmap: what 1.0 and 2.0 need to be state of the art
#3155 | open | DRAWING-CAM-IO-UI | Modeling: unexpected result, SpacePilot cannot rotate in the sketch.
#3171 | open | DRAWING-CAM-IO-UI | LOADING STUCK ON STEP OPENING
#3187 | open | DRAWING-CAM-IO-UI | Reorder design tabs by dragging them
#3216 | open | DRAWING-CAM-IO-UI | Can't import CSV parameters
#3217 | closed | DRAWING-CAM-IO-UI | GPU path tracer: DX12 with FXC takes ~2 h to compile the path shader
#3219 | open | DRAWING-CAM-IO-UI | Crash: SolidCraft 0.12.0 (started 2026-10-15T16:40:59Z) ended without a clean exit between commands
#3220 | open | DRAWING-CAM-IO-UI | Model cache: fit big designs (thin checkpoints, then final state) instead of skipping the cache
#3221 | closed | DRAWING-CAM-IO-UI | Saving a design with a 256–512 MiB STEP import fails: container part too large
#3222 | open | DRAWING-CAM-IO-UI | Sketch on step file slow kubuntu
#3223 | closed | DRAWING-CAM-IO-UI | Auto aligned step files in part view additional feaures create themselves on differend coordinate system
#3229 | open | DRAWING-CAM-IO-UI | Import and Calibrate Reference Images in Sketches (Canvas Feature)
#3236 | open | DRAWING-CAM-IO-UI | Decision: allow one unsafe opt-in in solidcraft-gpu for hardware ray tracing (wgpu ExperimentalFeatures)
#3237 | open | DRAWING-CAM-IO-UI | MCP reverse engineer step files
#3257 | closed | DRAWING-CAM-IO-UI | Can't select parameters by clicking
#3262 | open | DRAWING-CAM-IO-UI | Intuitive right click menu
#3263 | open | DRAWING-CAM-IO-UI | Sample prview images
#3269 | open | DRAWING-CAM-IO-UI | macOS freezes completely when opening sample designs (SolidCraft 0.16.2)
#3291 | closed | DRAWING-CAM-IO-UI | 3D connexion Space Mouse doesn't yet work
#3321 | open | DRAWING-CAM-IO-UI | Graphics: recover from a lost GPU instead of panicking, and fix the AMD Vega D3D12 hang behind #1789
#3328 | closed | DRAWING-CAM-IO-UI | Cannot run 0.16.2 locally on CachyOS
#3334 | open | DRAWING-CAM-IO-UI | Crash: SolidCraft 0.16.2 (started 2026-10-19T23:02:57Z) ended without a clean exit during startup (adapter request, 0.1 s after launch)
#3348 | open | DRAWING-CAM-IO-UI | Windows: three tests fail on main (robustness goldens seed 13/67, paste file links)
#3350 | closed | DRAWING-CAM-IO-UI | solidcraft
#3351 | closed | DRAWING-CAM-IO-UI | STEP import drops faces whose edge vertices sit slightly off their curves
#3356 | closed | DRAWING-CAM-IO-UI | No scroll bar on horizontal Timeline
#3358 | closed | DRAWING-CAM-IO-UI | IGES import: trimmed surfaces whose trim pieces run end to start are dropped
#3365 | open | DRAWING-CAM-IO-UI | Very slow loading file since v0.16.1
#3372 | open | DRAWING-CAM-IO-UI | Keyboard navigation basics
#3376 | closed | DRAWING-CAM-IO-UI | Garbled characters after setting the language to “Chinese”
#3386 | open | DRAWING-CAM-IO-UI | Crash: SolidCraft 0.16.2 (started 2026-10-19T23:26:58Z) ended without a clean exit while running `solid.thread`
#3389 | open | DRAWING-CAM-IO-UI | Driving a joint rebuilds the moving components' feature history: crank move takes 50–200 s in a 4-joint engine (unstable text emboss)
#3394 | closed | DRAWING-CAM-IO-UI | step export lose faces
#3407 | open | DRAWING-CAM-IO-UI | Viewport SSAO: reduce structured banding around planar contact regions
#3413 | open | DRAWING-CAM-IO-UI | Viewport: extend ambient occlusion to the existing virtual ground
#3414 | open | DRAWING-CAM-IO-UI | Viewport: add selectable projected ground shadows in Graphics
#3419 | open | DRAWING-CAM-IO-UI | Feature Request: Native SolidWorks File Support (.SLDPRT / .SLDASM)
#3424 | open | DRAWING-CAM-IO-UI | Crash: wgpu error: Validation Error
#3430 | open | DRAWING-CAM-IO-UI | Crash: SolidCraft 0.16.2 (started 2026-10-19T16:37:20Z) ended without a clean exit during startup (surface configure, 45429.6 s after launch)
#3431 | open | DRAWING-CAM-IO-UI | Export Current View as DXF: unexpected result
#3433 | open | DRAWING-CAM-IO-UI | GPU path tracer: keep compiled shaders between runs (pipeline cache)
#3440 | open | DRAWING-CAM-IO-UI | macOS: Fusion navigation preset does not support Shift + two-finger orbit
#3443 | open | DRAWING-CAM-IO-UI | Feature request: Move and scale SVG geometry during insertion
#3455 | open | DRAWING-CAM-IO-UI | kernel-perf: solid.combine of many separate bodies grows steeply (64 pegs take 36 s)
#3457 | open | DRAWING-CAM-IO-UI | Hebrew interface language (with right-to-left text)
#3473 | open | DRAWING-CAM-IO-UI | Crash: SolidCraft 0.16.2 (started 2026-10-20T07:32:32Z) ended without a clean exit while running `solid.pattern.circular`
#3483 | closed | DRAWING-CAM-IO-UI | Settings menu kind of broken
#3490 | open | DRAWING-CAM-IO-UI | Crash: SolidCraft 0.16.2 (started 2026-10-20T08:39:54Z) ended without a clean exit while running `inspect.interference`
#3491 | open | DRAWING-CAM-IO-UI | I submitted my Traditional Chinese language support.
#3503 | open | DRAWING-CAM-IO-UI | [RFC] Display states: save and restore what is shown, not only the camera
#3504 | open | DRAWING-CAM-IO-UI | Isolate: layers that restore what was hidden before (UI for display states)
#3513 | closed | DRAWING-CAM-IO-UI | Minor ui bug, ai settings
#3523 | open | DRAWING-CAM-IO-UI | [Linux] 3D mouse via spacenavd: Y/Z and Ry/Rz swapped, and the view drifts after releasing the cap
#3529 | open | DRAWING-CAM-IO-UI | Can't create a line on geometry in a sketch imported from a SVG
#3532 | open | DRAWING-CAM-IO-UI | Keyboard shortcut assignment captures Ctrl as "ControlLeft" instead of the pressed key (Windows 11)
#3537 | open | DRAWING-CAM-IO-UI | STEP import: a body's boundary tolerance doesn't survive saving and reopening the design
#3540 | open | DRAWING-CAM-IO-UI | Extrude cut with coincident cylindrical face takes minutes and freezes the UI (0.16.2)
#3560 | open | DRAWING-CAM-IO-UI | Crash: SolidCraft 0.16.2 (started 2026-10-20T08:16:44Z) ended without a clean exit while running `document.inspect`
#3568 | open | DRAWING-CAM-IO-UI | macOS: make feature-preview reselect configurable and avoid Command screenshot shortcuts
#3579 | open | DRAWING-CAM-IO-UI | Key shortcuts not working when selecting joints from browser
#3580 | closed | DRAWING-CAM-IO-UI | Press move keyshortcut when component is selected opens face-move
#3589 | open | DRAWING-CAM-IO-UI | Ältere MAC Late 2015
#3595 | open | DRAWING-CAM-IO-UI | Orbiting stutters with ~100 imported STEP bodies: one CPU core at 100 %, GPU mostly idle (0.16.2, Linux, NVIDIA)
#3614 | closed | DRAWING-CAM-IO-UI | Section Analysis: dragging the cut stalls on a large imported part
#3615 | closed | DRAWING-CAM-IO-UI | Section Analysis: Edit reopens the dialog at 0 mm with no plane
#3620 | open | DRAWING-CAM-IO-UI | Deutsche Übersetzung Fehlerhaft
#3642 | closed | DRAWING-CAM-IO-UI | Can't add a photo from an insert tab
#3643 | open | DRAWING-CAM-IO-UI | The menu bar, including Undo and Redo, does not work in full-screen mode.
#3661 | open | DRAWING-CAM-IO-UI | Crash: SolidCraft 0.16.1 (started 2026-10-19T20:23:28Z) ended without a clean exit between commands
#3663 | open | DRAWING-CAM-IO-UI | Expression completion hides names that start with a unit: typing `rad` offers nothing although `radius` exists
#3668 | open | DRAWING-CAM-IO-UI | cursor
#3676 | closed | DRAWING-CAM-IO-UI | Crash: SolidCraft 0.15.0 (started 2026-10-20T17:06:00Z) ended without a clean exit while running `timeline.redefine`
#3677 | closed | DRAWING-CAM-IO-UI | Crash: SolidCraft 0.15.0 (started 2026-10-20T17:06:00Z) ended without a clean exit while running `timeline.redefine`
#3683 | open | DRAWING-CAM-IO-UI | Selection fields: remove one selected item from the dialog, not only clear all
#3704 | open | DRAWING-CAM-IO-UI | MCP: tools to manage the desktop app (tabs, quit, preferences, agent cursor)
#3708 | closed | DRAWING-CAM-IO-UI | Export Current View as DXF: operation failed
#3713 | open | DRAWING-CAM-IO-UI | Exploded views: export as an animation (like motion.export_animation)
#3720 | open | DRAWING-CAM-IO-UI | Severe viewport navigation lag with a 1,163-body STEP assembly (0.17.0, Linux, Quadro P620)
#3722 | open | DRAWING-CAM-IO-UI | All three dimension planes in the 3D origin are shown in the same (clay) color. Instead, display them in RGB colors for instant recognition.
#3730 | closed | DRAWING-CAM-IO-UI | STEP import: a part with an internal void reads back with the void's volume added (SOLIDWORKS-written ORIENTED_CLOSED_SHELL .F.)
#3731 | open | DRAWING-CAM-IO-UI | Cylinder joins onto a tapered (drafted) wall are 20-100x slower than onto a vertical wall; 2+ cylinders on the conical corners trigger a boolean retry
#3733 | open | DRAWING-CAM-IO-UI | Maintain selected faces when right-clicking
#3737 | open | DRAWING-CAM-IO-UI | Orbit and zoom freeze for several seconds — main thread blocked in edge highlighting
#3740 | open | DRAWING-CAM-IO-UI | Time Line
#3745 | closed | DRAWING-CAM-IO-UI | Hovering a large thread freezes while the face highlight is computed
#3750 | open | DRAWING-CAM-IO-UI | Control channel: ui.inspect doesn't report the 'Recover unsaved design?' prompt
#3755 | closed | DRAWING-CAM-IO-UI | mouse pointer is flickering and kind a slow when opening dialogs
#3762 | open | DRAWING-CAM-IO-UI | Arabic interface language (with right-to-left text)
#3771 | open | DRAWING-CAM-IO-UI | 3D mouse: support device buttons (views, Fit, Dominant, rotation/pan lock)
#3778 | open | DRAWING-CAM-IO-UI | Support for external references
#3783 | open | DRAWING-CAM-IO-UI | Crash: SolidCraft 0.17.0 (started 2026-10-20T16:59:20Z) ended without a clean exit while running `timeline.redefine`
#3784 | open | DRAWING-CAM-IO-UI | Windows: robustness goldens seeds 13, 23, 36, 67 differ on main; seed 36 fails a step that builds on Linux
#3806 | closed | DRAWING-CAM-IO-UI | IGES import: a revolved arc comes in as a whole sphere
#3814 | open | DRAWING-CAM-IO-UI | Crash: SolidCraft 0.17.0 (started 2026-10-20T20:15:34Z) ended without a clean exit between commands
#3820 | open | DRAWING-CAM-IO-UI | [RFC] Cut lists from the model: stock sizes, nesting onto boards and sheets, and build plans
#3827 | open | DRAWING-CAM-IO-UI | Browser: a name too long for the panel runs over its edge instead of being elided
#3837 | open | DRAWING-CAM-IO-UI | [M9] Exploded views: steps, easing and trail lines
#3845 | closed | DRAWING-CAM-IO-UI | IGES import: faces dropped where trim pieces miss each other by a few micrometres
#3848 | open | DRAWING-CAM-IO-UI | Modeling: program locks up. sometimes comes back to life until I click something. Sometimes crashes.
#3858 | open | DRAWING-CAM-IO-UI | 3MF import: name the bodies of a Bambu Studio project after its objects (Metadata/model_settings.config)
#3863 | open | DRAWING-CAM-IO-UI | SpaceMouse does not work as expected on Linux RPM and AppImage
#3874 | open | DRAWING-CAM-IO-UI | Drawing W24–W26: bill of materials, sheet tables (parts lists) and balloons
#3875 | open | DRAWING-CAM-IO-UI | Drawing W18: sheets, borders, title blocks and drawing templates
#3885 | closed | DRAWING-CAM-IO-UI | IGES import: a B-spline curve over a tiny knot span loses the faces it bounds
#3890 | open | DRAWING-CAM-IO-UI | Drawing W11, W12, W15: where drawings are saved, their records and their commands
#3892 | open | DRAWING-CAM-IO-UI | A large nested STEP assembly makes the browser tree slow: 0.25–1.3 s frames while hovering it
#3897 | open | DRAWING-CAM-IO-UI | Sweep of a ring profile along a short arc with Join freezes at 'Computing…' (pipe segment)
#3903 | open | DRAWING-CAM-IO-UI | Crash: SolidCraft 0.16.2 (started 2026-10-20T22:55:00Z) ended without a clean exit during startup (first frame, 14.8 s after launch)
#3912 | open | DRAWING-CAM-IO-UI | IGES import: a trimmed surface whose composite trim starts with a reversed piece is dropped
#3924 | open | DRAWING-CAM-IO-UI | STEP import: AP242 tessellated parts are left out without a warning
#3927 | open | DRAWING-CAM-IO-UI | Windowed text is blurry at some window sizes, and blurry in one part of the frame and sharp in another (not anti-aliasing)
#3928 | closed | DRAWING-CAM-IO-UI | IGES import: a part made of separate closed pieces comes in as one open body
#3947 | closed | DRAWING-CAM-IO-UI | File dialogs start in the folder last used
#3951 | open | DRAWING-CAM-IO-UI | STEP import: a closed spline edge whose vertex sits just past the curve's start loses the whole body (ZeroRange panic)
#3962 | open | DRAWING-CAM-IO-UI | Crash: SolidCraft 0.18.1 (started 2026-10-20T23:06:35Z) ended without a clean exit while running `solid.combine`
#3965 | open | DRAWING-CAM-IO-UI | Unable to edit parameter name when it's invalid
#3988 | open | DRAWING-CAM-IO-UI | 3Dconnexion 3D mouse: most axes do not respond; forward/backward tilt zooms instead of rotating
#3989 | open | DRAWING-CAM-IO-UI | ui-egui tests: the test process crashes (SIGSEGV) in the Vulkan loader when offscreen GPU tests start side by side
#4005 | open | DRAWING-CAM-IO-UI | STEP export of a revolve about an oblique axis reads back with a third of the volume ("bad cone" faces dropped)
#4006 | open | DRAWING-CAM-IO-UI | STEP export of a first revolve with operation join reads back as an open body ("bad cone"): 26386 instead of 27251 mm3
#4007 | open | DRAWING-CAM-IO-UI | STEP export of a revolved horn torus (arc radius equal to its distance from the axis) reads back 17% or 30% too large, and differs from run to run
#4015 | open | DRAWING-CAM-IO-UI | Preferences: choose the graphics API, with a trial start that goes back if it fails
#4019 | open | DRAWING-CAM-IO-UI | Designs can't be attached to GitHub issues: .solidcraft isn't an accepted attachment type
#4023 | open | DRAWING-CAM-IO-UI | Selecting a pinned tool on the Toolbar replaces the primary tool
#4028 | open | DRAWING-CAM-IO-UI | SolidCraft (Start Page Config)
#4031 | open | DRAWING-CAM-IO-UI | Crash: assertion failed: !previous_region.is_empty() // from == final_vertex
#4044 | open | DRAWING-CAM-IO-UI | Crash: SolidCraft 0.18.2 (started 2026-10-21T05:10:20Z) ended without a clean exit between commands
#4048 | open | DRAWING-CAM-IO-UI | Canvas insert and canvas file filter extension problem
#4050 | closed | DRAWING-CAM-IO-UI | On a 2K-resolution monitor, the mouse pointer
#4060 | open | DRAWING-CAM-IO-UI | Crash: SolidCraft 0.18.2 (started 2026-10-21T05:36:45Z) ended without a clean exit between commands
#4068 | open | DRAWING-CAM-IO-UI | Crash: SolidCraft 0.16.2 (started 2026-10-19T09:51:12Z) ended without a clean exit between commands
#4071 | open | DRAWING-CAM-IO-UI | Define a named parameter inline from any value input (`name = expression`)
#4072 | open | DRAWING-CAM-IO-UI | Crash: wgpu error: Validation Error
#4074 | open | DRAWING-CAM-IO-UI | System ejection sound
#4086 | open | DRAWING-CAM-IO-UI | STEP-import file too large
#4087 | open | DRAWING-CAM-IO-UI | Improve thread face continuity and clearance adjustment for 3D printing
#4089 | closed | DRAWING-CAM-IO-UI | Crash: SolidCraft 0.18.2 (started 2026-10-21T07:21:28Z) ended without a clean exit between commands
#4091 | open | DRAWING-CAM-IO-UI | Crash: SolidCraft 0.16.2 (started 2026-10-20T08:51:38Z) ended without a clean exit between commands
#4100 | open | DRAWING-CAM-IO-UI | Deleting an inserted STEP leaves its components and joints behind and moves a root body into one of them
#4102 | open | DRAWING-CAM-IO-UI | Booleans on a FreeCAD-exported STEP solid are ~15x slower than on the same shape built in SolidCraft
#4104 | open | DRAWING-CAM-IO-UI | Windows GNU robustness tests select MSVC-verified Windows golden overrides
#4105 | open | DRAWING-CAM-IO-UI | Normal view to surface
#4107 | open | DRAWING-CAM-IO-UI | UI: Draft Analysis dialog overlaps and gets hidden behind bottom timeline/history bar
#4110 | open | DRAWING-CAM-IO-UI | Windows Vulkan teardown crashes with RTSS layer; standalone wgpu multi-instance reproduction
#4116 | open | DRAWING-CAM-IO-UI | Trackpad: two-finger vertical swipe zooms instead of panning (macOS, browser)
#4118 | open | DRAWING-CAM-IO-UI | The header of the drop-down list of the menu commands must have more prominence compared to the individual menu commands
#4119 | open | DRAWING-CAM-IO-UI | Crash: SolidCraft 0.18.2 (started 2026-10-21T07:58:40Z) ended without a clean exit while running `timeline.redefine`
#4121 | open | DRAWING-CAM-IO-UI | Crash: SolidCraft 0.18.2 (started 2026-10-21T08:08:38Z) ended without a clean exit between commands
#4126 | open | DRAWING-CAM-IO-UI | Crash: SolidCraft 0.18.1 (started 2026-10-21T07:58:59Z) ended without a clean exit while running `timeline.redefine`
#4127 | open | DRAWING-CAM-IO-UI | Render preview: the 128 MiB geometry budget blocks large assemblies on desktop, and its message is nearly unreadable
#4128 | open | DRAWING-CAM-IO-UI | Preference: Hide (V) acts on the element under the cursor, without selecting it first
#4130 | open | DRAWING-CAM-IO-UI | Opening STL File error
#4132 | open | DRAWING-CAM-IO-UI | Crash: wgpu error: Validation Error
#4138 | open | DRAWING-CAM-IO-UI | Faces & Objects not properly hidden in Section-Analysis
#4142 | open | DRAWING-CAM-IO-UI | Intersect sharing coplanar faces with a non-prism body (B-spline ramp or step) falls back to tool_nudge when either side has curved faces; a plain box is exact
#4146 | open | DRAWING-CAM-IO-UI | Theme Request: Dark UI / Light Viewport
#4150 | open | DRAWING-CAM-IO-UI | Clicking on Document settings buttons a second time closes the window
#4162 | open | DRAWING-CAM-IO-UI | Cannot open web file in local app write "Update version"
#4163 | open | DRAWING-CAM-IO-UI | Setup shortcuts gives a error with multi-button shortcuts / Unable to setup same shortcut button for multiple features
#4164 | open | DRAWING-CAM-IO-UI | Settings file (app.ron) damaged when two SolidCraft windows save at once; all settings reset
#4165 | open | DRAWING-CAM-IO-UI | Browser: surface bodies are shown with the solid body icon in the Bodies folder
#4167 | open | DRAWING-CAM-IO-UI | BUG: Ambient occlusion Ghosts
#4172 | open | DRAWING-CAM-IO-UI | Crash: called `Result::unwrap()` on an `Err` value: Lost
#4176 | open | DRAWING-CAM-IO-UI | [Navigation] No option to adjust zoom scroll speed
#4180 | open | DRAWING-CAM-IO-UI | Crash: tolerance must be no less than 1e-6
#4181 | open | DRAWING-CAM-IO-UI | Crash: SolidCraft 0.18.2 (started 2026-10-21T07:15:51Z) ended without a clean exit between commands
#4186 | open | DRAWING-CAM-IO-UI | Big step lag
#4188 | open | DRAWING-CAM-IO-UI | Crash: called `Result::unwrap()` on an `Err` value: Lost
#4190 | open | DRAWING-CAM-IO-UI | can we make it look and work like solidworkss?
#4197 | open | DRAWING-CAM-IO-UI | Canvas: accept BMP, GIF, WebP and TIFF pictures
#4198 | open | DRAWING-CAM-IO-UI | Canvas: stretch sideways or lengthways, and flip top to bottom
#4199 | open | DRAWING-CAM-IO-UI | Canvas: draw over the model, and click it in the view
#4200 | open | DRAWING-CAM-IO-UI | Canvas: one dialog for inserting and editing, on the Insert tab
#4201 | open | DRAWING-CAM-IO-UI | Canvas: move, turn, stretch and flip the picture with handles
#4209 | open | DRAWING-CAM-IO-UI | Holding to Select other, results go off the window
#4214 | open | DRAWING-CAM-IO-UI | Fade viewport grid rather than Cutoff
#4215 | open | DRAWING-CAM-IO-UI | Crash: called `Result::unwrap()` on an `Err` value: Lost
#4216 | open | DRAWING-CAM-IO-UI | Play Button On The History Bar AND More Themes and Materials for participation in the TooTallToby Competitions.
#4217 | open | DRAWING-CAM-IO-UI | Browser: a Bodies folder under every component pushes deep assemblies far to the right
#4226 | open | DRAWING-CAM-IO-UI | Escape Button to Cancel
#4231 | open | DRAWING-CAM-IO-UI | Feature request
#4237 | open | DRAWING-CAM-IO-UI | Crash: SolidCraft 0.16.2 (started 2026-10-21T10:37:40Z) ended without a clean exit between commands
#4243 | open | DRAWING-CAM-IO-UI | Crash: SolidCraft 0.18.1 (started 2026-10-20T23:23:09Z) ended without a clean exit between commands
#4244 | open | DRAWING-CAM-IO-UI | Crash: SolidCraft 0.16.2 (started 2026-10-19T03:46:20Z) ended without a clean exit between commands
#4246 | open | DRAWING-CAM-IO-UI | Crash: SolidCraft 0.18.2 (started 2026-10-21T11:39:45Z) ended without a clean exit between commands
#4252 | open | DRAWING-CAM-IO-UI | Feature in the Timeline and Fonts
#4254 | closed | DRAWING-CAM-IO-UI | Crash: SolidCraft 0.18.2 (started 2026-10-21T11:37:20Z) ended without a clean exit between commands
#4255 | open | DRAWING-CAM-IO-UI | Button to change selection mode
#4258 | open | DRAWING-CAM-IO-UI | Section view is generally buggy
#4260 | open | DRAWING-CAM-IO-UI | Pattern dialogs: the Quantity field has no step arrows, so a count can only be typed
#4271 | open | DRAWING-CAM-IO-UI | Feature Request: Enhanced Apple Pencil Support (Standalone usage without keyboard/mouse)
#4275 | open | DRAWING-CAM-IO-UI | Slow Zoom performance on large 662 Body STEP import
#4285 | open | DRAWING-CAM-IO-UI | In Offset command, only Distance 1 can be controlled using widget. Distance 2 has no such control.
#4286 | open | DRAWING-CAM-IO-UI | Crash: SolidCraft 0.18.2 (started 2026-10-21T10:31:11Z) ended without a clean exit while running `solid.combine`
#4290 | open | DRAWING-CAM-IO-UI | Crash: SolidCraft 0.18.2 (started 2026-10-21T08:17:32Z) ended without a clean exit between commands
#4293 | open | DRAWING-CAM-IO-UI | Crash: assertion failed: !previous_region.is_empty() // from == final_vertex
#4298 | open | DRAWING-CAM-IO-UI | Agent chat: "Refresh" stays disabled when Preferences → AI is opened from the Home page
#4309 | closed | DRAWING-CAM-IO-UI | The orbit mode with right drag is problematic (both start and end)
#4314 | open | DRAWING-CAM-IO-UI | Exporting dxf: question
#4319 | open | DRAWING-CAM-IO-UI | Crash: SolidCraft 0.18.2 (started 2026-10-21T13:46:35Z) ended without a clean exit while running `solid.thread`
#4323 | open | DRAWING-CAM-IO-UI | Gruvbox Dark and Dark Soft: the selected Model tab's label is below 4.5:1 contrast
#4326 | open | DRAWING-CAM-IO-UI | Drawing a diameter line does not snap through the circle centre
#4329 | open | DRAWING-CAM-IO-UI | Crash: called `Result::unwrap()` on an `Err` value: Lost
#4338 | open | DRAWING-CAM-IO-UI | Magic mouse zoom MacOS
#4343 | open | DRAWING-CAM-IO-UI | Browser: a feature's row under Features has no Rename in its context menu
#4370 | open | DRAWING-CAM-IO-UI | UI strings live outside the message catalog, so they cannot be translated
#4376 | open | DRAWING-CAM-IO-UI | Lasso select
#4388 | open | DRAWING-CAM-IO-UI | select edge(s), face(s), etc. and see dimensions (or angle) in bottom right corner
#4390 | open | DRAWING-CAM-IO-UI | Mouse navigation issue on lower resolution screen
#4394 | open | DRAWING-CAM-IO-UI | 0.18.3 crashed finder on Mac Silicon
#4406 | open | DRAWING-CAM-IO-UI | Crash: SolidCraft 0.17.0 (started 2026-10-21T16:27:52Z) ended without a clean exit between commands
#4416 | open | DRAWING-CAM-IO-UI | Crash: SolidCraft 0.18.3 (started 2026-10-21T16:28:51Z) ended without a clean exit between commands
#4417 | open | DRAWING-CAM-IO-UI | Browser: hovering a feature or joint row doesn't light it in the viewport
#4422 | open | DRAWING-CAM-IO-UI | Crash: wgpu error: Validation Error
#4431 | open | DRAWING-CAM-IO-UI | Tapered text cut or join into a body is 40-180x slower than without taper
#4438 | open | DRAWING-CAM-IO-UI | STEP export writes a whole torus as two faces on one shared loop with no seam edges; OCCT 7.6 (occt-import-js) reads it back at volume 0
#4451 | open | DRAWING-CAM-IO-UI | OBJ export: no shared vertices (triangle soup, ~30x the 3MF size), no colours, and spaces in body names become underscores
#4461 | open | DRAWING-CAM-IO-UI | Parameter row briefly jumps down when focus moves from name to expression input
#4476 | open | DRAWING-CAM-IO-UI | Crash: SolidCraft 0.18.3 (started 2026-10-21T15:56:38Z) ended without a clean exit while running `timeline.dependents`
#4478 | open | DRAWING-CAM-IO-UI | Crash: assertion failed: !previous_region.is_empty() // from == final_vertex
#4479 | open | DRAWING-CAM-IO-UI | problème de sauvegarde / backup issue
#4491 | open | DRAWING-CAM-IO-UI | Offscreen model render returns blank/garbage PNGs; viewport can stop drawing solids after a graphics-preset change
#4493 | closed | DRAWING-CAM-IO-UI | Frame-rate readout is hidden behind the History strip in the floating layout
#4504 | open | DRAWING-CAM-IO-UI | VIEW: No perspective view
#4512 | open | DRAWING-CAM-IO-UI | RENAME: doesn't work
#4515 | open | DRAWING-CAM-IO-UI | Analysis Window position
#4517 | closed | DRAWING-CAM-IO-UI | Autodesk Fusion File (.f3d)d
#4519 | closed | DRAWING-CAM-IO-UI | Norwegian (nb) UI localization
#4542 | open | DRAWING-CAM-IO-UI | Crash: SolidCraft 0.18.3 (started 2026-10-21T21:39:56Z) ended without a clean exit while running `solid.extrude`
#4544 | open | DRAWING-CAM-IO-UI | Add Norwegian Bokmål interface translation
#4547 | open | DRAWING-CAM-IO-UI | Crash: SolidCraft 0.18.3 (started 2026-10-21T21:44:30Z) ended without a clean exit while running `solid.combine`
#4553 | open | DRAWING-CAM-IO-UI | Freecad import
#4554 | open | DRAWING-CAM-IO-UI | History bar: hovering a Rigid Group of a large STEP assembly stalls the window for up to 1.5 s
#4559 | open | DRAWING-CAM-IO-UI | Crash: SolidCraft 0.17.0 (started 2026-10-20T20:21:01Z) ended without a clean exit between commands
#4562 | open | DRAWING-CAM-IO-UI | Section view requests
#4563 | open | DRAWING-CAM-IO-UI | Section Analysis: a Hatch option, off for a plain cut in the body's colour
#4581 | open | DRAWING-CAM-IO-UI | Feature request: View and edit camera Yaw, Pitch, and Roll numerically
#4582 | open | DRAWING-CAM-IO-UI | Keyboard Shortcuts shows shift+lshift
#4589 | open | DRAWING-CAM-IO-UI | Expression suggestions open over the Drive Joints slider while it is dragged, and come back after being dismissed
#4598 | open | DRAWING-CAM-IO-UI | STEP import: a body comes in silently empty when an edge curve doubles back (regression in 0.18.0, from #1653)
#4599 | closed | DRAWING-CAM-IO-UI | Crash: SolidCraft 0.18.4 (started 2026-10-22T00:12:19Z) ended without a clean exit between commands
#4604 | open | DRAWING-CAM-IO-UI | Measuring a large assembly freezes the window until Windows closes it (an agent's first inspect_design does it)
#4605 | open | DRAWING-CAM-IO-UI | Crash report on startup on Mac Mini
#4616 | open | DRAWING-CAM-IO-UI | Viewport: a part placed many times keeps a moved copy of its triangles per placement (41 GB, 45 ms frames for 25 placements of a 2 M-triangle mesh)

## OTHER

#5 | closed | OTHER | Rename Fusion-derived command ids to our own dotted ids
#6 | closed | OTHER | Merge the unmerged wip/* branches
#7 | closed | OTHER | Add a CONTRIBUTING guide and refresh the README
#8 | closed | OTHER | GitHub Actions CI for pull requests
#26 | closed | OTHER | ROADMAP.md is stale
#28 | closed | OTHER | Documentation site (user guide + developer guide) and the web app on GitHub Pages
#32 | closed | OTHER | Release v0.1.0 for all platforms
#35 | closed | OTHER | Agent cursor: show MCP/control-channel actions as a visible pointer (optional)
#57 | open | OTHER | Consider renaming the project - too many name collisions
#61 | closed | OTHER | CI: gates (riscv64) fails fetching riscv64 packages from security.ubuntu.com
#110 | open | OTHER | Suggestion regarding naming and branding
#113 | open | OTHER | MacOs does not recognize the app as 'safe'
#147 | closed | OTHER | Control channel reads unbounded lines; vfs::read reads special files without a cap
#156 | closed | OTHER | Add Flatpak to flat park
#171 | closed | OTHER | CLI commands panics when its stdout pipe is closed
#194 | closed | OTHER | Solidcraft still is running solvecraft as the cli
#205 | closed | OTHER | Security: MCP/control-channel commands can read, write and delete files outside the design (no path scope), and the control port has no auth
#231 | open | OTHER | Feature Request: Distribute macOS builds via Homebrew / Cask
#270 | closed | OTHER | `solvecraft-cli mcp --connect` to an app started without `--control`: the error gives no hint, and MCP clients only see CONNECTION_CLOSED
#347 | closed | OTHER | macOS app still appears as SolveCraft after the SolidCraft rename
#433 | closed | OTHER | Organize application resources under a single documented assets structure
#876 | closed | OTHER | Proposal: one typed walk over the references inside a feature
#904 | open | OTHER | Related pure-Rust CAD projects: ogeom-rs and printCAD
#1024 | open | OTHER | Windows: thread tests crash the test process with STATUS_HEAP_CORRUPTION
#1077 | closed | OTHER | MCP: path into a not-yet-existing folder reports 'outside the folders this agent may use'
#1143 | closed | OTHER | solidcraft-cli run and eval can write their JSON to a file (--json-out), so that no shell redirect re-encodes it
#1188 | open | OTHER | CI tooling: consider a bounded local test-concurrency budget
#1221 | open | OTHER | Plugins: sandboxed WebAssembly plugins with a WIT interface
#1255 | closed | OTHER | Action matrix: 40 actions that passed in the baseline fail on main
#1270 | closed | OTHER | Proposal: a type for each role of a feature's fields (Expr, BodyName, FacePick…)
#1457 | open | OTHER | Agent chat: show tool calls as readable summaries, raw JSON behind an expander
#1458 | open | OTHER | Agent chat in Flatpak and Snap: the sandbox can't see or start the user's agents
#1486 | open | OTHER | MCP: compare the model with a reference picture (overlap, edge offsets in mm, overlay)
#1537 | closed | OTHER | Agent chat: use a signed-in Grok CLI (SuperGrok)
#1591 | closed | OTHER | claim bot test
#1618 | closed | OTHER | Control channel: engine.execute returns 'timeout' while the operation keeps running and later applies; retries then fail
#1620 | open | OTHER | Research Request for Plugin: FEA
#1622 | open | OTHER | Research Request for Plugin: CFD
#1623 | open | OTHER | Research Request for Plugin: Thermal Analysis
#1624 | open | OTHER | Research Request for Plugin: Electrical
#1790 | closed | OTHER | Replace @google/gemini-cli with Google Antigravity CLI in AI Chat
#1818 | closed | OTHER | fix(cli): explicit --help prints an error and exits 1
#1822 | closed | OTHER | fix(docs): CLAUDE.md omits current shared contributor rules
#1833 | closed | OTHER | Per-command timings for script runs and batches
#1887 | open | OTHER | SolveCraft install fails on Linux: couldn't find `solvecraft` executable (named `solidcraft` in archive)
#1920 | closed | OTHER | Flatpak installation instructions reference outdated repository URL (404)
#1930 | closed | OTHER | Windows: `cargo xtask ci` fails at UI icon standards (CRLF checkout breaks the icon hashes, palette saves and icon tests)
#1943 | open | OTHER | Documentation is missing
#1989 | open | OTHER | Control channel: a never-finishing engine.execute blocks the session; no cancel, control.pending also returns busy (follow-up to #1618)
#2136 | closed | OTHER | Updater: 0.11–0.14 can't install 0.15.0 because release URLs now name tangency-labs
#2150 | closed | OTHER | Web app stays on a cached build forever when the site sits behind a sign-in
#2155 | closed | OTHER | update flake.nix with each release
#2177 | open | OTHER | zenity is used for dialogs but is missing from flake
#2207 | open | OTHER | Agent chat: an adapter runs on a system Node too old for it ("SyntaxError: Unexpected token 'with'")
#2224 | closed | OTHER | Agent chat: a long message pushes Send out of the panel, and "View attached" can't be opened
#2298 | open | OTHER | AI chat (API backend): every attached view stays in the history, so requests grow until the endpoint refuses them (HTTP 413)
#2311 | open | OTHER | SolidCraft and FieldCraft on FlatPark
#2354 | open | OTHER | Feature request: macOS please provide separate ARM and x64 builds
#2374 | open | OTHER | Refactor with types: let a value's type say what it is
#2388 | closed | OTHER | Flaky test: control_server::tests::a_timed_out_request_can_be_asked_for_later fails under load
#2561 | closed | OTHER | CI: make the file-open responsiveness test deterministic
#2592 | closed | OTHER | Agent chat: the design bridge is dropped when the agent's own config already has an MCP server named `solidcraft`, as docs/mcp.md suggests
#2812 | open | OTHER | Agent chat: Ctrl+V and drag-and-drop for images, PDFs and any file
#2828 | open | OTHER | Native Android version with pen support
#2829 | open | OTHER | [RFC] Typed input and output schemas for commands
#2869 | open | OTHER | PDM: versioned document server RFC and bounded loopback prototype
#2878 | open | OTHER | docker image set not public?
#2908 | closed | OTHER | Flaky test: apply_off_thread_tests::ok_computes_off_the_window_thread_saying_so_then_applies fails under load
#2951 | open | OTHER | Feature Request: Link to Google Drive or S3
#2957 | closed | OTHER | agent_chat tests: two tests share one temp folder and fail each other under `cargo test`
#2995 | open | OTHER | PDM: authenticated local file workspace with version browsing and pinned retrieval
#3002 | open | OTHER | PDM: local CAD connection and version panel with safe working copies
#3010 | open | OTHER | AI API endpoint not working for Z.AI coding plan
#3148 | open | OTHER | [Feature Request] Advanced AI Chat System & Context Management
#3177 | open | OTHER | Auto update
#3225 | open | OTHER | Mac mini M4 problem
#3226 | open | OTHER | Tidy test layout: kernel tests by area under src/tests/, not one file per bug
#3238 | closed | OTHER | 3D Druck Service Buling
#3239 | open | OTHER | Add FieldCraft as simulation branch of SolidCraft
#3242 | closed | OTHER | MCP batch reports successful rollback after losing the original document
#3253 | open | OTHER | Missing WinGet Package
#3338 | open | OTHER | Protable version of 16.2 will not work
#3380 | open | OTHER | [RFC] Agent MCP: reliable replies, universal guidance, and long-running planned work
#3382 | open | OTHER | MCP: a feedback loop so the agent learns what each call changed and what each feature reports
#3538 | open | OTHER | [Linux Appimage] Updating from in-app function breaks systemwide update functionality.
#3654 | closed | OTHER | Control channel: an app-wide "what's running" query, across connections
#3759 | open | OTHER | Engine: a read-only selection.get command for agents and scripts
#3881 | open | OTHER | Ai change per message
#3956 | open | OTHER | CI: web-offline cross-tab update races navigation
#3983 | open | OTHER | Agent skill: help users file reproducible bug reports
#3999 | open | OTHER | DMG on OSX is boring
#4045 | open | OTHER | Model-cache tests fail when the source is built without git (source tarball)
#4131 | open | OTHER | ia agent chat connected but dont work !
#4273 | closed | OTHER | sorry I posted this in the wrong place.
#4405 | open | OTHER | Claim bot: some claim PRs fail with 403 "Resource not accessible by integration"
#4538 | open | OTHER | CLI: subcommands still don't take --help, and 'fuzz --help' starts a fuzz run (follow-up to #1818)
#4587 | open | OTHER | Agent chat: stopping an agent leaves its helper processes running where `kill` is busybox'
