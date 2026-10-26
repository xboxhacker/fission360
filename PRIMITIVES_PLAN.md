# Fission 360 — Primitive solid bodies (Box / Cylinder / Sphere / Torus / Coil / Pipe) — DONE (1.13.42)

Six parametric **primitive solid bodies**, each a new `FissionFeature` subclass with a live
spec dialog, placed at an origin and applied as **join / cut / intersect** like every other
solid feature.

| Tool | Kernel class | OCCT builder | Spec-dialog fields |
|------|--------------|--------------|--------------------|
| Box | `FissionBox` | `BRepPrimAPI_MakeBox(origin,w,l,d)` | width, depth, height |
| Cylinder | `FissionCylinder` | `BRepPrimAPI_MakeCylinder(h,r)` + translate | radius, height |
| Sphere | `FissionSphere` | `BRepPrimAPI_MakeSphere(center,r)` | radius |
| Torus | `FissionTorus` | `BRepPrimAPI_MakeTorus(rMajor,rTube)` + translate | major radius, tube radius |
| Coil | `FissionCoil` | `GeomAPI_Interpolate` helix + `MakePipeShell` sweep of a circle | coil diameter, tube diameter, pitch, height |
| Pipe | `FissionPipe` | `MakePipeShell` sweep of a circle along a straight spine | outer diameter, wall thickness, length |

Every primitive shares the same contract:

- **`defaults()`** returns **string** expressions (e.g. `"50"`), not numbers —
  `number(ctx, key, var)` parses them.
- **`origin`** is a `point3` stored as `QVariantList{x,y,z}`; a `gp_Trsf`
  translation moves the built solid into place (primitives default to the local origin).
- **`compute()`** builds the solid, then calls
  `ctx.applyOperation(shape, operation(), targets, id(), err)` — the same call Extrude uses —
  so a primitive can **join / cut / intersect** onto the selected body or faces.

## Where they plug in

- **Kernel** — six `.h/.cpp` pairs in `src/kernel/`, registered in `FissionFeature.cpp`
  `create()` and `types()`.
- **Toolbar** — one `cmd()` line each under the **SOLID CREATE** menu.
- **Dialog** — `FeaturePanel.qml` `dialogFor()` routes the type to a data-driven
  `SpecDialog` entry (fields with `kind` "number" / "point3" / "choice").

## Gotchas baked in

- `MakeCylinder(height, radius)` and `MakeTorus(rMajor, rTube)` take **no point** — the origin
  is applied by translating the shape, not by an origin constructor arg.
- `GeomAPI_Interpolate` takes a **heap** `Handle(TColgp_HArray1OfPnt)` (fill with `SetValue`),
  then `.Perform()` / `.IsDone()`. A stack `TColgp_Array1OfPnt` is the overload mismatch.
- `MakePipeShell::SetMode(gp_Dir)` — the **fixed-binormal** mode — keeps the tube in a plane
  through the axis (no twist); the same construction the Hole thread uses. A wire/Frenet mode
  would twist it. `Build()` returns `void` → check `.IsDone()`.
- `MakeEdge(curve).Edge()` returns a `TopoDS_Edge` (not `.Shape()`).

## Status

- All six compile, registered, and wired to the toolbar + spec dialog.
- Build `cmake --build build-rel --config Release` is green (Release `.exe` produced).
- Smoke-tested: window opens clean, no QML errors.
- Bumped to **1.13.42**; README + CHANGELOG updated.