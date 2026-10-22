# SolidCraft sketch and constraint parity

Public issues on [tangency-labs/solidcraft](https://github.com/tangency-labs/solidcraft/issues), compared with Fission 360’s own 2D sketch solver. Behaviour only. No SolidCraft or truck source was copied. This scan did not change sketch code. Later modeling and drawing fixes are in `CHANGELOG.md` **1.13.35**. This sketch note was not re-scanned.

Fission solves sketches with `SketchSolver` (damped minimum-norm Gauss-Newton, numeric Jacobian) on OCCT curves. It does not use the truck Rust kernel. Truck, WASM, and cargo issues are out of scope.

Prior note in `docs/solidcraft-issue-parity.md` (scan dated 2026-10-20) set aside sketch issues #3590, #3530, #3529, #3586, and #3514 as a different constraint engine. Those five are judged below.

## Method

Fetched 2026-10-21 with `gh` against `tangency-labs/solidcraft`, open and closed.

| Query | Reported total | Pages read |
|---|---|---|
| `sketch` | 1041 | 10 (GitHub search returns 1000; 41 older hits not in this page set) |
| `constraint` | 102 | all |
| `solver` | 26 | all |
| `dimension` | 113 | all |
| `profile` | 618 | all |
| `coincident` | 79 | all |
| `tangent` | 219 | all |
| `under-constrained` | 6 | all |
| `over-constrained` | 6 | all |
| `drag` | 145 | all |
| `label:area:sketch` | 107 open, 29 closed | all |

Unique issues across the keyword pages: 1390. Most `profile`, `tangent`, `drag`, and `dimension` hits are solid booleans, fillets, navigation, or drawings. Those were skipped after the title and the opening of the report. The rows below are the sketch-solver, sketch-tool, and sketch-import reports whose text was actually read (search body excerpt, or the full short report).

Fission files read for the verdicts:

- `src/kernel/SketchSolver.h`, `src/kernel/SketchSolver.cpp` (constraint residuals, DOF, conflict)
- `src/kernel/FissionSketch.h`, `src/kernel/FissionSketch.cpp` (entities, profiles, dimension value, annotations)
- `src/kernel/SketchEdit.cpp` (drag, trim, extend, offset, fillet, project)
- `src/kernel/FissionFeature.cpp` (`resolvePlane`, nearest-face rematch)
- `assets/qml/FissionApplication/Viewport.qml` (constraint picks, dimension build, drag)
- `assets/qml/FissionApplication/Toolbar.qml` (sketch tools)
- `assets/qml/FissionSketch/SketchPanel.qml` (DOF line, offset and fillet fields)
- `src/ui/BRepRenderer.cpp` (sketch-origin snap)
- `CHANGELOG.md` (solver introduction, profile-index fallback, multi-profile join)

`LIKELY-SAME` is used only where that code or the changelog shows the same user-visible gap.

## What Fission sketches do

Geometric constraints in `SketchSolver.h`: coincident (point–point, point–line, point–circle/arc), horizontal, vertical, parallel, perpendicular, collinear, tangent (line–circle/arc, circle–circle inner/outer), equal (two lines or two circles/arcs), concentric, midpoint (point and one line), symmetric (two points and a sketch line), fix. Dimensions: distance (aligned / horizontal / vertical), point–line, radius, diameter, angle. The sketch origin is `{e: -1, p: "origin"}`. A conflict fails the sketch (`The sketch constraints conflict and cannot all be satisfied`) and `addConstraint` writes nothing.

The ribbon matches that set (`Toolbar.qml`): Dimension plus coincident, horizontal, vertical, parallel, perpendicular, collinear, tangent, equal, concentric, midpoint, symmetry, fix. Curves colour by DOF (changelog: blue under-constrained, white fully constrained). The sketch panel shows the remaining DOF.

Tools: line, center line, rectangle (four constrained lines), center rectangle, circle, 3-point arc stored as a center arc, polygon, slot, ellipse, fit-point spline, point, trim, extend, offset, fillet (two lines), project, move/copy, construction, DXF export. Legacy `rectangle` and `arc3` entities still build; the changelog says a `rectangle` entity cannot be constrained.

Not implemented: driven dimensions, arc length, circle-edge dimensions, sketch text, SVG/DXF import, sketch pattern, sketch mirror, rigid group, 3D sketch, pierce.

## Issue by issue

| # | Title | Verdict | Evidence |
|---|---|---|---|
| [#30](https://github.com/tangency-labs/solidcraft/issues/30) | Profiles picked in the Extrude dialog are stored by index | POSSIBLY-SAME | Extrude still stores profile indices. Changelog 1.13.21 falls back to the sketch’s default closed profiles when an index dies, which can attach the feature to a different region. Fission project is not rebuilt, so the “projected outline got new ids” half does not apply. |
| [#40](https://github.com/tangency-labs/solidcraft/issues/40) | Offsets of projected geometry get new ids on recompute | ALREADY-HANDLED | `offsetEntities` copies curves once and does not regenerate them on rebuild (`SketchEdit.cpp`). |
| [#44](https://github.com/tangency-labs/solidcraft/issues/44) | Sketch: dimension placement and editing | ALREADY-HANDLED | Placement click sets orient (`Viewport.qml` `buildConstraint`). `setDimensionOffset` stores the label offset. Double-click opens the value editor (changelog). Driven dimensions in parentheses are #320. |
| [#45](https://github.com/tangency-labs/solidcraft/issues/45) | Trim / extend / offset robustness (splines, ellipses) | LIKELY-SAME | `trimAt` returns false unless the entity is a line, circle, or arc. `offsetEntities` walks lines and arcs, and grows circle, polygon, and slot. Spline and ellipse are skipped. |
| [#46](https://github.com/tangency-labs/solidcraft/issues/46) | Sketch fillet of line–arc, arc–arc, chains | LIKELY-SAME | `filletCorner` requires two lines (`SketchEdit.cpp`). |
| [#47](https://github.com/tangency-labs/solidcraft/issues/47) | Project / Include, followed through later edits | LIKELY-SAME | `projectRef` copies edges or a face boundary and sets `fixed` and `projected`. Later body edits do not update those curves. Silhouettes and other components are not projected. |
| [#81](https://github.com/tangency-labs/solidcraft/issues/81) | Angle constraints don't work as expected | ALREADY-HANDLED | Dimension on two non-parallel lines builds an angle (`Viewport.qml` `buildConstraint`). Select-then-press-D is #116. |
| [#116](https://github.com/tangency-labs/solidcraft/issues/116) | Use the selection that already exists when applying a constraint | LIKELY-SAME | `constraintClick` starts from an empty pick list. The current selection is not consumed. |
| [#145](https://github.com/tangency-labs/solidcraft/issues/145) | Solver allocates dense matrices and can abort | LIKELY-SAME | `SketchSolver::jacobian` and `solve` build a dense Jacobian and a dense `J Jᵀ` with no entity cap. |
| [#162](https://github.com/tangency-labs/solidcraft/issues/162) | Offset has no constraint; no circle-to-circle distance | LIKELY-SAME | Offset distance is evaluated to a number, then baked (`Viewport.qml`). No residual measures the gap between two circles or a point and a circumference (`SketchSolver.cpp` `addResidual`). |
| [#203](https://github.com/tangency-labs/solidcraft/issues/203) | Numeric solver flips to the mirror solution | POSSIBLY-SAME | The solver is still iterative Gauss-Newton (`SketchSolver.h`). `configuration()` stores side, inner/outer tangency, and angle sign. Aligned distance is an unsigned length, so a large edit can still swing the sketch. |
| [#320](https://github.com/tangency-labs/solidcraft/issues/320) | Over-constraint should become a driven dimension | LIKELY-SAME | `addConstraint` solves immediately and, on failure, changes nothing (`FissionSketch.h`). There is no driven flag. |
| [#517](https://github.com/tangency-labs/solidcraft/issues/517) | Offset should keep an editable dimension | LIKELY-SAME | The panel has a distance field (`SketchPanel.qml`), but `offsetEntities` stores plain coordinates and only coincident constraints at the new joints. |
| [#588](https://github.com/tangency-labs/solidcraft/issues/588) | Tangent does not split a profile | POSSIBLY-SAME | Profiles are OCCT splits of built edges (`FissionSketch::computeProfiles`). A tangency that only touches at a point does not cut a face into two regions. |
| [#590](https://github.com/tangency-labs/solidcraft/issues/590) | Drag breaks a tangent square and marks it over-constrained | ALREADY-HANDLED | `dragTo` adds a `dragto` residual and writes entities only when `solve` succeeds (`SketchEdit.cpp`). A failed drag leaves the constraints in place. |
| [#687](https://github.com/tangency-labs/solidcraft/issues/687) | Clover of equal rectangles refused as over-constrained | UNKNOWN | No Fission repro. Redundant residuals that are already satisfied pass `kSatisfied` (`SketchSolver::solve`). A slightly inconsistent snap could still fail. |
| [#696](https://github.com/tangency-labs/solidcraft/issues/696) | Sketch Mirror, Pattern, and Offset dialogs | LIKELY-SAME | Toolbar has solid Mirror and Pattern, and a sketch Offset click-tool. No sketch mirror or sketch pattern command (`Toolbar.qml`). |
| [#754](https://github.com/tangency-labs/solidcraft/issues/754) | Missing arc-length dimension | LIKELY-SAME | `SketchSolver::isDimension` is distance, pointline, radius, diameter, angle. |
| [#755](https://github.com/tangency-labs/solidcraft/issues/755) | Spline/arc loop crossed at the joints finds no profiles | POSSIBLY-SAME | A bounded face is kept only when the splitter does not touch the oversized border (`computeProfiles`). A junction that does not meet produces no region. |
| [#939](https://github.com/tangency-labs/solidcraft/issues/939) | OFFSET: operation failed | UNKNOWN | Report has no public recipe in the excerpt. Spline and ellipse offsets return no curves (`offsetEntities`). |
| [#1026](https://github.com/tangency-labs/solidcraft/issues/1026) | Fillet refused when a construction line also ends in the corner | ALREADY-HANDLED | Fillet uses the two picked lines. It does not require the corner to have only those two curves. |
| [#1075](https://github.com/tangency-labs/solidcraft/issues/1075) | Symmetry has no point symmetry and cannot use origin axes | LIKELY-SAME | Symmetric residual is two points and a sketch line (`SketchSolver.cpp`). Origin axes are not sketch curves. The origin point can be snapped (`BRepRenderer.cpp` `consider` of the plane origin). |
| [#1208](https://github.com/tangency-labs/solidcraft/issues/1208) | Sketch fillet refuses a line–arc corner | LIKELY-SAME | Same two-line check as #46. |
| [#1359](https://github.com/tangency-labs/solidcraft/issues/1359) | Infinite construction lines | LIKELY-SAME | Construction is a flag on finite entities (`FissionSketch.h`). No infinite line type. |
| [#1485](https://github.com/tangency-labs/solidcraft/issues/1485) | Tangent-arc tool misses trimmed ends | LIKELY-SAME | No tangent-arc creation tool. The arc tool is `addArcThroughPoints` (`FissionSketch.h`). Fillet tangency exists only between two lines. |
| [#1544](https://github.com/tangency-labs/solidcraft/issues/1544) | Mirror entities in sketch mode | LIKELY-SAME | No sketch mirror. Body Mirror is a solid feature. |
| [#1796](https://github.com/tangency-labs/solidcraft/issues/1796) | Symmetry cannot center a line between two fixed lines | LIKELY-SAME | The tool signature is point, point, line (`Viewport.qml` `constraintSignatures`). Two lines cannot be the symmetric pair. |
| [#1803](https://github.com/tangency-labs/solidcraft/issues/1803) | Dimension cannot pick a circle edge (line-to-circle distance) | LIKELY-SAME | Dimension signatures are circle (radius/diameter), line, line+line, line+point, point+point (`Viewport.qml`). No line+circle distance. |
| [#1824](https://github.com/tangency-labs/solidcraft/issues/1824) | Fillet only accepts the corner point when several lines meet | ALREADY-HANDLED | The tool asks for two lines, not the shared vertex (`filletCorner`). |
| [#1942](https://github.com/tangency-labs/solidcraft/issues/1942) | How to draw polygon, polyline | ALREADY-HANDLED | Polygon and spline tools are on the sketch toolbar. Polyline remains an entity type (`FissionSketch.h`). |
| [#1976](https://github.com/tangency-labs/solidcraft/issues/1976) | Constrained point drag stops before the feasible cursor target | LIKELY-SAME | `dragto` requires the point to sit on the cursor. If that residual conflicts, `dragTo` returns false and writes nothing. There is no closest-point projection. |
| [#2000](https://github.com/tangency-labs/solidcraft/issues/2000) | Show open and untied profile ends | LIKELY-SAME | No open-end markup in the sketch kernel or QML searched. Closed regions come from `computeProfiles` only. |
| [#2006](https://github.com/tangency-labs/solidcraft/issues/2006) | Crash in `sketch.drag` | NOT-APPLICABLE | Truck command crash. Fission drag is `FissionSketch::dragTo`. |
| [#2031](https://github.com/tangency-labs/solidcraft/issues/2031) | Move of sketch objects doesn't work great | POSSIBLY-SAME | `moveEntities` drops constraints that tie moved curves to curves left behind (`FissionSketch.h`). That is a rigid move with constraints broken, which is a different failure than a solve that will not move. |
| [#2087](https://github.com/tangency-labs/solidcraft/issues/2087) | Horizontal/Vertical cannot pick a line midpoint | LIKELY-SAME | `constraintCandidates` skips `role === "mid"` (`Viewport.qml`). Midpoints are draw-snaps and handles, not constraint picks. |
| [#2166](https://github.com/tangency-labs/solidcraft/issues/2166) | 3-point arc midpoint is not kept | POSSIBLY-SAME | The tool stores a center arc (`addArcThroughPoints`). Solver roles for `arc` are center, start, end. There is no constrained mid point. |
| [#2355](https://github.com/tangency-labs/solidcraft/issues/2355) | Sketch solver diagnostics and profiling | NOT-APPLICABLE | Truck diagnostics. |
| [#2364](https://github.com/tangency-labs/solidcraft/issues/2364) | Cannot dimension between sketch geometry and an axis | LIKELY-SAME | Dimension picks are sketch points and curves. A face sketch’s origin is the face centroid (`RebuildContext::resolvePlane`), not the model axis. |
| [#2366](https://github.com/tangency-labs/solidcraft/issues/2366) | Dimension input should accept expressions | ALREADY-HANDLED | Dimension `value` is an expression evaluated on solve (`SketchSolver::load`). Changelog: typed sketch expressions are kept. |
| [#2424](https://github.com/tangency-labs/solidcraft/issues/2424) | Spline offset creates no end constraints | LIKELY-SAME | Splines never enter `offsetEntities` (only line, arc, circle, polygon, slot). |
| [#2621](https://github.com/tangency-labs/solidcraft/issues/2621) | Moving a sketch drags some lines like a string | LIKELY-SAME | If dragging both ends fails, `moveSketchDrag` drags only the nearer end (`Viewport.qml`). |
| [#2919](https://github.com/tangency-labs/solidcraft/issues/2919) | New dimensions default to 10 mm | ALREADY-HANDLED | An empty dimension value is filled from `measure()` (`FissionSketch.cpp` `addConstraint`). |
| [#2940](https://github.com/tangency-labs/solidcraft/issues/2940) | Misbehavior of symmetric constraint | UNKNOWN | Symmetric residual exists. The report does not give a recipe that the residual is known to miss. |
| [#2961](https://github.com/tangency-labs/solidcraft/issues/2961) | Symmetry fails after a dimension | UNKNOWN | Same residual. No Fission failure recorded in the changelog. |
| [#3167](https://github.com/tangency-labs/solidcraft/issues/3167) | Typing a dimension before switching orient keeps the aligned value | ALREADY-HANDLED | Orient is chosen by the placement click, then the editor opens (`createConstraint`). |
| [#3320](https://github.com/tangency-labs/solidcraft/issues/3320) | Cannot reference a plane axis for dimensions | LIKELY-SAME | Same pick limit as #2364. |
| [#3360](https://github.com/tangency-labs/solidcraft/issues/3360) | Constrained edge drag lags and the release misses the cursor | POSSIBLY-SAME | Each move rebuilds a numeric Jacobian (`SketchSolver::solve`, up to 200 iterations). Nothing in the code drops the last successful solve on mouse-up. |
| [#3451](https://github.com/tangency-labs/solidcraft/issues/3451) | Coincident needs external geometry | LIKELY-SAME | Coincident targets are sketch points, lines, circles, arcs, or the sketch origin. Another sketch or a body edge is available only after a one-shot project (#47). |
| [#3514](https://github.com/tangency-labs/solidcraft/issues/3514) | Sketch rectangular pattern 15×15 over-constrains | NOT-APPLICABLE | No sketch pattern, so this truck pattern conflict cannot occur. Missing pattern is #696. |
| [#3529](https://github.com/tangency-labs/solidcraft/issues/3529) | Cannot line-snap to SVG sketch geometry | LIKELY-SAME | No SVG import in `src/` (search for svg/dxf import found none). DXF export exists (`exportDxf`). |
| [#3586](https://github.com/tangency-labs/solidcraft/issues/3586) | `sketch.pattern.rectangular` over-constrains | NOT-APPLICABLE | Same as #3514. |
| [#3630](https://github.com/tangency-labs/solidcraft/issues/3630) | Extrude of a filleted sketch is invisible | UNKNOWN | Sketch fillet builds a real arc with tangent and radius constraints. No Fission extrude of that profile is known to vanish. |
| [#3637](https://github.com/tangency-labs/solidcraft/issues/3637) | Multiple profiles per sketch | ALREADY-HANDLED | `computeProfiles` keeps every bounded face and even-depth defaults. Changelog 1.13.22 joins several profiles in one Extrude. |
| [#3786](https://github.com/tangency-labs/solidcraft/issues/3786) | Dimensioning one shape moves the others | ALREADY-HANDLED | A distance residual touches only its two points. The Gauss-Newton step moves variables with a non-zero Jacobian column (`SketchSolver::solve`). |
| [#3805](https://github.com/tangency-labs/solidcraft/issues/3805) | Midpoint of two lines errors | LIKELY-SAME | Midpoint accepts a point and one line, not two lines (`SketchSolver.cpp`). |
| [#3812](https://github.com/tangency-labs/solidcraft/issues/3812) | Dimension callout stays in millimetres under inch units | ALREADY-HANDLED | The dimension editor formats with `withUnit` (`Viewport.qml`). Changelog: inch input and labels. |
| [#3917](https://github.com/tangency-labs/solidcraft/issues/3917) | Trim of an offset curve is undone by the next solve | ALREADY-HANDLED | Offset curves are ordinary entities, not recomputed from a source (#40). |
| [#3923](https://github.com/tangency-labs/solidcraft/issues/3923) | Adding a constraint that does not move geometry still rebuilds | POSSIBLY-SAME | `addConstraint` always solves and writes the sketch. No “geometry unchanged, skip timeline” check was found. |
| [#4064](https://github.com/tangency-labs/solidcraft/issues/4064) | Cannot dimension a point or circle to a circle/arc edge | LIKELY-SAME | Same signature limit as #1803. Centres can be dimensioned as points. |
| [#4069](https://github.com/tangency-labs/solidcraft/issues/4069) | Group entities (rigid block) | LIKELY-SAME | Unknown constraint types fail (`addResidual` “unknown constraint type”). No group residual. |
| [#4097](https://github.com/tangency-labs/solidcraft/issues/4097) | Sketch fillet drops the parameter on a driven corner | LIKELY-SAME | `filletCorner` replaces the corner ends with numeric coordinates from the solved geometry (`moved()` writes `r6` numbers). The new radius constraint keeps the radius expression. |
| [#4193](https://github.com/tangency-labs/solidcraft/issues/4193) | Sketch mirror cannot use an origin axis | LIKELY-SAME | No sketch mirror tool (#1544). |
| [#4352](https://github.com/tangency-labs/solidcraft/issues/4352) | Dimensioning: aligned vs horizontal vs vertical | ALREADY-HANDLED | Two points plus a placement click choose horizontal, vertical, or aligned (`buildConstraint`). |
| [#4385](https://github.com/tangency-labs/solidcraft/issues/4385) | Two-sided offset shows only one distance | LIKELY-SAME | One distance, one side, baked (`offsetEntities`). No second driving dimension. |
| [#4447](https://github.com/tangency-labs/solidcraft/issues/4447) | Full 3D constraints (SolveSpace) | NOT-APPLICABLE | Solver is one sketch plane (`SketchSolver.h`). |
| [#4464](https://github.com/tangency-labs/solidcraft/issues/4464) | Offset removes turning freedom from the source lines | ALREADY-HANDLED | Offset does not add constraints back to the source curves (`offsetEntities`). The copy also does not follow the source (#517). |
| [#4585](https://github.com/tangency-labs/solidcraft/issues/4585) | Sketch with 1 DOF will not move once it is complex | LIKELY-SAME | A 1-DOF linkage cannot put a dragged point on an arbitrary cursor position. `dragTo` then fails the whole gesture. Spline fit points are a separate path: `moveSketchDrag` writes `points` with `setEntity` and does not call `dragTo`. |
| [#4591](https://github.com/tangency-labs/solidcraft/issues/4591) | Closed sketch geometry not detected | UNKNOWN | No geometry in the report. Profile detection exists (`computeProfiles`). |
| [#4615](https://github.com/tangency-labs/solidcraft/issues/4615) | Extend does nothing when nothing is in the way | LIKELY-SAME | `extendAt` lengthens to the next curve and returns false when there is nothing to meet (`FissionSketch.h`). |

### Same evidence, grouped

**SVG / DXF / canvas into a sketch — LIKELY-SAME.** No importer. Export only (`FissionSketch::exportDxf`).

| # | Title |
|---|---|
| [#67](https://github.com/tangency-labs/solidcraft/issues/67) | Import DXF and Import SVG fail on macOS |
| [#2052](https://github.com/tangency-labs/solidcraft/issues/2052) | DXF import issue |
| [#2605](https://github.com/tangency-labs/solidcraft/issues/2605) | SVG import palette missing |
| [#3229](https://github.com/tangency-labs/solidcraft/issues/3229) | Import and calibrate reference images in sketches |
| [#3443](https://github.com/tangency-labs/solidcraft/issues/3443) | Move and scale SVG during insertion |
| [#3931](https://github.com/tangency-labs/solidcraft/issues/3931) | Canvas to sketch |
| [#4274](https://github.com/tangency-labs/solidcraft/issues/4274) | Importing images as a sketch reference |

**Sketch text — LIKELY-SAME.** No text entity on `FissionSketch`. Drawing text is a separate sheet tool.

| # | Title |
|---|---|
| [#236](https://github.com/tangency-labs/solidcraft/issues/236) | Text in sketch cannot be moved |
| [#513](https://github.com/tangency-labs/solidcraft/issues/513) | Sketch text font |
| [#727](https://github.com/tangency-labs/solidcraft/issues/727) | Parametric sketch text and letter counters |
| [#1362](https://github.com/tangency-labs/solidcraft/issues/1362) | Sketch text height, place, and angle are numbers only |
| [#1772](https://github.com/tangency-labs/solidcraft/issues/1772) | Text parameters for sketch text |

**3D sketch — NOT-APPLICABLE.** Fission sketches are planar. These reports are about SolidCraft’s 3D sketch mode.

| # | Title |
|---|---|
| [#741](https://github.com/tangency-labs/solidcraft/issues/741) | Profiles from planar loops of 3D sketch curves |
| [#3599](https://github.com/tangency-labs/solidcraft/issues/3599) | Pierce constraint |
| [#3639](https://github.com/tangency-labs/solidcraft/issues/3639) | 3D sketch points stay on a plane |
| [#3855](https://github.com/tangency-labs/solidcraft/issues/3855) | 3D sketch only lines and fit splines |
| [#4160](https://github.com/tangency-labs/solidcraft/issues/4160) | Fillet failed on a 3D sketch |

**Create Sketch on a face refused — NOT-APPLICABLE (43 titles in the fetched `sketch` pages, including #3530, #3590, #3488, #4078, #4325, #4395, #4578, #4612).** The SolidCraft message is a truck face certificate (“this face can't keep an attachment”). Fission attaches with `resolvePlane`: a planar face, origin at the face centroid, and after topology changes the nearest face with the same kind (`matchSubShape` in `FissionFeature.cpp`). That can follow the wrong face. It does not emit the truck refusal.

**Solid-kernel reports that only mention a sketch, spline, profile, or tangent as the source of a body — NOT-APPLICABLE.** Examples read and skipped: #3540 (coincident cylindrical cut), #4083 (solid fillet after a sketch resize), #4141, #4142, #4143, #4332 (chamfer of a spline rim), #4558 (join after a sketch edit). Same for the bulk of the 618 `profile` and 219 `tangent` hits (booleans, blends, lofts).

## Verdict counts

Counted rows above, with each grouped issue counted once. The 43 Create Sketch refusals and the skipped solid-kernel / navigation hits are not in this tally.

| Verdict | Count |
|---|---|
| LIKELY-SAME | 46 |
| ALREADY-HANDLED | 16 |
| POSSIBLY-SAME | 8 |
| NOT-APPLICABLE | 10 |
| UNKNOWN | 6 |
| **Total in the tables** | **86** |

LIKELY-SAME includes the 7 import issues and the 5 sketch-text issues. NOT-APPLICABLE in the tally is #2006, #2355, #3514, #3586, #4447, and the five 3D-sketch issues. The 43 Create Sketch refusals are a separate NOT-APPLICABLE set and are not in the 86.

## Sketch gaps, in order

1. **Drag ignores remaining freedom.** A constrained drag must hit the cursor exactly or it does nothing (#4585, #1976). A blocked line drag moves one end (#2621). Spline points are written directly and are not solved with `dragto`.

2. **Over-constraint is a hard failure.** No driven dimension, and no toggle (#320). A conflicting dimension is rejected and the sketch is unchanged.

3. **Dimensions do not cover circles as edges, arc length, or model axes.** Radius and diameter work. Point-to-circumference, circle-to-circle gap, and arc length do not (#4064, #1803, #162, #754). Axes are not dimension targets (#2364, #3320).

4. **Offset, project, and fillet are one-shot.** Offset does not keep a driving dimension, has no second side, and skips splines and ellipses (#517, #4385, #2424, #45). Project does not follow the body (#47, #3451). Fillet is two lines and replaces expression-driven corners with numbers (#46, #1208, #4097). Extend will not run past the click when no curve is in the way (#4615).

5. **No sketch pattern, mirror, or rigid group.** Symmetry is two points about a sketch line, not two lines about an origin axis (#696, #4193, #4069, #1796, #1075, #3805). Horizontal and vertical cannot pick a midpoint (#2087).

6. **Ellipse, polygon, and slot are single entities.** Their edges are not lines or arcs, so tangent, equal, concentric, and radius do not apply (`isCirc` is circle or arc only). Ellipse radii are axis-aligned (`rx`, `ry`, no angle). Trim and extend ignore them.

7. **No SVG, DXF, or image import, and no sketch text.** DXF export exists.

8. **The numeric solver has no size cap and is not constructive.** Dense `J Jᵀ` (#145). Stored signs reduce mirror flips; they do not lock every branch (#203).

9. **Profile presentation.** Several profiles and holes already work (#3637). Open ends are not drawn (#2000). Reports of a closed loop that will not shade (#4591, #755) are not confirmed on Fission geometry.
