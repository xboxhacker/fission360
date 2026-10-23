// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
import QtQuick

// Forms for the MODIFY / CREATE tools that need no custom UI beyond the generic field kinds
FeatureDialog {
    id: sd
    readonly property string type: feature ? feature.type : ""
    readonly property var bodyFilter: "body,face"
    readonly property var planeSel: ({ kind: "selection", sel: "plane", filter: "face,plane", presets: planePresets })

    function plane(label, key) { return Object.assign({}, planeSel, { label: label, key: key }) }

    readonly property var fromSpec: ({ kind: "point3", label: "From point", key: "fromPoint", prompt: "Click the point to move from",
                                       then: "toPoint", when: p => p.moveType === "pointToPoint" || p.moveType === "pointToPosition",
                                       alsoSet: p => p.moveType === "pointToPosition" ? "translate" : "toPoint" })
    function moveHint(f) {
        const t = f ? f.get("moveType") : ""
        if (t === "free") return "Drag an arrow to slide along it, a square to slide in its plane, a ring to turn about "
                                 + "the pivot (Shift = 15\u00b0 steps), or the white ball to drag freely."
        if (t === "translate") return "Drag the arrows or squares, or type the distances."
        if (t === "rotate") return "Pick an axis (an edge, construction axis or sketch line) and type the angle."
        if (t === "pointToPoint") return "Pick a point on the bodies (vertex, circle center, midpoint or face), then the point it moves to."
        if (t === "pointToPosition") return "Pick a point on the bodies, then type the position it moves to."
        return ""
    }

    function jointHint(f) {
        if (!f) return ""
        const m = f.get("motion")
        const drive = m === "revolute" ? "Angle turns it about the joint axis. "
                    : m === "slider" ? "Offset Z slides it along the joint axis. "
                    : m === "cylindrical" ? "Angle turns it, Offset Z slides it. "
                    : "Angle and Offset Z fine-tune the locked position. "
        const pick = f.get("asBuilt")
                ? "Pick the parts that move, then the joint origin: they stay where they are. "
                : "Pick the parts that move, a snap point on them, then the snap point on the part they join to. "
        return pick + "Snap points: a flat face where you click (Z = its normal), a round face (its axis), a circular "
                    + "edge (its centre) or a straight edge (its midpoint). " + drive
    }

    // Hole / Thread standards
    readonly property var threadStandards: [["custom", "Custom"], ["isoCoarse", "ISO metric coarse"], ["isoFine", "ISO metric fine"],
                                           ["unc", "Inch UNC"], ["unf", "Inch UNF"]]
    function sized(p) { return !!p.standard && p.standard !== "custom" }
    function sizeOptions(standard) {
        if (!app || !app.doc || !standard || standard === "custom") return []
        return app.doc.threadSizes(standard).map(s => [s.name, s.name + "   \u00d8" + s.major.toFixed(2) + " mm, pitch " + s.pitch.toFixed(3)])
    }
    // A new standard starts on a common size (about 6 mm) unless the current size exists in it
    function pickStandard(v, allowEmpty) {
        if (!feature || v === "custom") return
        const list = app.doc.threadSizes(v)
        const cur = feature.get("size")
        if (allowEmpty && !cur) return
        if (list.some(s => s.name === cur)) return
        const s = list.find(x => x.major >= 6) || list[0]
        if (s) feature.set("size", s.name)
    }

    readonly property var specs: ({
        Hole: [
            { kind: "selection", label: "Face", key: "face", sel: "face", filter: "face", hitKey: "center",
              when: p => !p.sketch },
            { kind: "selection", label: "Sketch points", key: "sketch", sel: "sketch", filter: "sketchcurve,profile",
              when: p => !p.face },
            { kind: "choice", label: "Hole type", key: "holeType",
              options: [["simple", "Simple"], ["counterbore", "Counterbore"], ["countersink", "Countersink"]] },
            { kind: "choice", label: "Standard", key: "standard", options: threadStandards, onPick: v => sd.pickStandard(v, false) },
            { kind: "choice", label: "Size", key: "size", optionsOf: p => sd.sizeOptions(p.standard), when: p => sd.sized(p) },
            { kind: "choice", label: "Fit", key: "fit", options: [["close", "Close"], ["normal", "Normal"], ["loose", "Loose"]],
              when: p => sd.sized(p) && !p.tapped },
            { kind: "note", textOf: f => f && f.designation ? f.designation : "" },
            { kind: "number", label: "Diameter", key: "diameter", when: p => !sd.sized(p) },
            { kind: "number", label: "C'bore dia.", key: "cboreDiameter", when: p => p.holeType === "counterbore" },
            { kind: "number", label: "C'bore depth", key: "cboreDepth", when: p => p.holeType === "counterbore" },
            { kind: "number", label: "C'sink dia.", key: "csinkDiameter", when: p => p.holeType === "countersink" },
            { kind: "number", label: "C'sink angle", key: "csinkAngle", when: p => p.holeType === "countersink" },
            { kind: "choice", label: "Extents", key: "extent", options: [["distance", "Distance"], ["all", "All"]] },
            { kind: "number", label: "Depth", key: "depth", when: p => p.extent !== "all" },
            { kind: "number", label: "Drill point", key: "tipAngle", when: p => p.extent !== "all" },
            { kind: "bool", label: "Tapped", key: "tapped" },
            { kind: "number", label: "Pitch", key: "pitch", when: p => !!p.tapped && !sd.sized(p) },
            { kind: "bool", label: "Modeled", key: "modeled", when: p => !!p.tapped },
            { kind: "bool", label: "Flip", key: "flip" },
            { kind: "note", text: "Click a flat face where the hole goes, then drag the round handle to move it over the face: "
                                  + "it snaps to the face center, circles (other holes) and corners on the face, and points of "
                                  + "shown sketches on that face. Or pick a sketch whose points mark several holes. "
                                  + "Drill point 0 = flat bottom. Custom: empty pitch = ISO metric coarse. A standard size sets "
                                  + "the diameter: the tap drill when tapped, otherwise the clearance hole for the fit." }
        ],
        Thread: [
            { kind: "selection", label: "Face", key: "face", sel: "face", filter: "face" },
            { kind: "choice", label: "Standard", key: "standard", options: threadStandards, onPick: v => sd.pickStandard(v, true) },
            { kind: "choice", label: "Size", key: "size", optionsOf: p => [["", "Fit the face"]].concat(sd.sizeOptions(p.standard)),
              when: p => sd.sized(p) },
            { kind: "number", label: "Pitch", key: "pitch", when: p => !sd.sized(p) },
            { kind: "number", label: "Length", key: "length" },
            { kind: "number", label: "Offset", key: "offset" },
            { kind: "bool", label: "Modeled", key: "modeled" },
            { kind: "note", textOf: f => f && f.designation ? "Designation: " + f.designation : "" },
            { kind: "note", text: "Pick a cylindrical face (shaft or hole). Empty pitch = ISO metric coarse, empty length = whole face." }
        ],
        Combine: [
            { kind: "selection", label: "Target body", key: "target", sel: "body", filter: bodyFilter, autoNext: true },
            { kind: "selection", label: "Tool bodies", key: "tools", sel: "bodies", filter: bodyFilter },
            { kind: "choice", label: "Operation", key: "operation", options: [["join", "Join"], ["cut", "Cut"], ["intersect", "Intersect"]] },
            { kind: "bool", label: "Keep tools", key: "keepTools" }
        ],
        Move: [
            { kind: "selection", label: "Objects", key: "bodies", sel: "bodies", filter: bodyFilter },
            { kind: "types", label: "Move type", key: "moveType",
              options: [["free", "Free move: drag the arrows, squares and rings", "movefree"],
                        ["translate", "Translate: X / Y / Z distances", "movetranslate"],
                        ["rotate", "Rotate: about an axis", "moverotate"],
                        ["pointToPoint", "Point to point: pick a point, then where it goes", "movepointtopoint"],
                        ["pointToPosition", "Point to position: pick a point, then type where it goes", "movepointtoposition"]],
              onPick: v => { if (v === "pointToPoint" || v === "pointToPosition") sd.pickPoint3(sd.fromSpec) } },
            { kind: "point3", label: "Pivot", key: "pivot", prompt: "Click the pivot point", when: p => p.moveType === "free" },
            { kind: "vector", key: "translate", labels: ["X distance", "Y distance", "Z distance"],
              when: p => p.moveType === "free" || p.moveType === "translate" || !p.moveType },
            { kind: "vector", key: "rotation", labels: ["X angle", "Y angle", "Z angle"], when: p => p.moveType === "free" },
            { kind: "selection", label: "Rotate axis", key: "axis", sel: "axis", filter: "edge,axis,sketchcurve", presets: axisPresets,
              when: p => p.moveType === "rotate" || !p.moveType },
            { kind: "number", label: "Angle", key: "angle", when: p => p.moveType === "rotate" || !p.moveType },
            fromSpec,
            { kind: "point3", label: "To point", key: "toPoint", prompt: "Click where the point goes",
              when: p => p.moveType === "pointToPoint" },
            { kind: "vector", key: "translate", labels: ["X position", "Y position", "Z position"],
              when: p => p.moveType === "pointToPosition" },
            { kind: "bool", label: "Create copy", key: "copy" },
            { kind: "note", textOf: f => moveHint(f) }
        ],
        Scale: [
            { kind: "selection", label: "Objects", key: "bodies", sel: "bodies", filter: bodyFilter },
            { kind: "selection", label: "Point", key: "point", sel: "point", filter: "point", presets: [["", "Origin"]] },
            { kind: "choice", label: "Scale type", key: "scaleType", options: [["uniform", "Uniform"], ["nonUniform", "Non-uniform"]] },
            { kind: "number", label: "Scale factor", key: "scale", unit: "", when: p => p.scaleType !== "nonUniform" },
            { kind: "number", label: "X scale", key: "scaleX", unit: "", when: p => p.scaleType === "nonUniform" },
            { kind: "number", label: "Y scale", key: "scaleY", unit: "", when: p => p.scaleType === "nonUniform" },
            { kind: "number", label: "Z scale", key: "scaleZ", unit: "", when: p => p.scaleType === "nonUniform" }
        ],
        SplitBody: [
            { kind: "selection", label: "Body to split", key: "bodies", sel: "bodies", filter: bodyFilter, autoNext: true },
            plane("Splitting plane", "tool"),
            { kind: "selection", label: "Or tool body", key: "toolBody", sel: "body", filter: bodyFilter, presets: [[-1, "None"]] },
            { kind: "note", text: "Each piece becomes its own body." }
        ],
        SplitFace: [
            { kind: "selection", label: "Faces", key: "faces", sel: "faces", filter: "face", autoNext: true },
            plane("Splitting tool", "tool")
        ],
        OffsetFace: [
            { kind: "selection", label: "Faces", key: "faces", sel: "faces", filter: "face" },
            { kind: "number", label: "Distance", key: "distance" }
        ],
        Offset: [
            { kind: "selection", label: "Faces", key: "faces", sel: "faces", filter: "face", when: p => !(p.bodies && p.bodies.length) },
            { kind: "selection", label: "Or bodies", key: "bodies", sel: "bodies", filter: "body", when: p => !(p.faces && p.faces.length) },
            { kind: "number", label: "Distance", key: "distance" },
            { kind: "note", text: "Creates offset copies as new bodies; negative distances go inward." }
        ],
        ReplaceFace: [
            { kind: "selection", label: "Source faces", key: "faces", sel: "faces", filter: "face", autoNext: true },
            plane("Target plane", "plane")
        ],
        Draft: [
            { kind: "choice", label: "Type", key: "draftType", options: [["face", "Fixed plane"], ["parting", "Parting line"]] },
            { kind: "selection", label: "Pull direction", key: "plane", sel: "pull", filter: "face,plane,edge,axis,sketchcurve",
              presets: planePresets, autoNext: true, inputView: true },
            { kind: "selection", label: "Faces", key: "faces", sel: "faces", filter: "face" },
            { kind: "number", label: "Angle", key: "angle" },
            { kind: "bool", label: "Flip pull", key: "flip" },
            { kind: "note", text: "Pick what sets the pull direction first, then the faces to tilt: a flat face or plane (its normal), "
                                  + "a round face (its axis), a straight edge, an axis or a sketch line. "
                                  + "With two faces picked before starting, the first is the pull direction." }
        ],
        Joint: [
            { kind: "selection", label: "Component", key: "bodies", sel: "bodies", filter: bodyFilter, autoNext: true },
            { kind: "selection", label: "Moving origin", key: "from", sel: "origin", filter: "face,edge", autoNext: true,
              inputView: true, when: p => !p.asBuilt },
            { kind: "selection", label: "Target origin", key: "to", sel: "origin", filter: "face,edge", inputView: true,
              when: p => !p.asBuilt },
            { kind: "selection", label: "Joint origin", key: "to", sel: "origin", filter: "face,edge", inputView: true,
              when: p => !!p.asBuilt },
            { kind: "types", label: "Motion", key: "motion",
              options: [["rigid", "Rigid: locked in place", "joint"],
                        ["revolute", "Revolute: turns about the joint axis", "moverotate"],
                        ["slider", "Slider: slides along the joint axis", "movetranslate"],
                        ["cylindrical", "Cylindrical: turns and slides", "revolve"]] },
            { kind: "number", label: "Angle", key: "angle" },
            { kind: "number", label: "Offset Z", key: "offset" },
            { kind: "bool", label: "Flip", key: "flip", when: p => !p.asBuilt },
            { kind: "note", textOf: f => sd.jointHint(f) }
        ],
        PathPattern: [
            { kind: "selection", label: "Objects", key: "bodies", sel: "bodies", filter: bodyFilter, autoNext: true },
            { kind: "selection", label: "Path", key: "path", sel: "path", filter: "sketchcurve" },
            { kind: "number", label: "Quantity", key: "count", unit: "" },
            { kind: "number", label: "Distance", key: "distance" },
            { kind: "choice", label: "Orientation", key: "orientation", options: [["identical", "Identical"], ["path", "Path direction"]] },
            { kind: "choice", label: "Operation", key: "operation", options: [["join", "Join"], ["newBody", "New Body"]] },
            { kind: "note", text: "Empty distance = spread over the whole path." }
        ],
        Rib: [
            { kind: "selection", label: "Curves", key: "sketch", sel: "sketch", filter: "sketchcurve" },
            { kind: "number", label: "Thickness", key: "thickness" },
            { kind: "choice", label: "Depth", key: "extent", options: [["toNext", "To next"], ["distance", "Distance"]] },
            { kind: "number", label: "Distance", key: "depth", when: p => p.extent === "distance" },
            { kind: "bool", label: "Flip", key: "flip" }
        ],
        Web: [
            { kind: "selection", label: "Curves", key: "sketch", sel: "sketch", filter: "sketchcurve" },
            { kind: "number", label: "Thickness", key: "thickness" },
            { kind: "choice", label: "Depth", key: "extent", options: [["toNext", "To next"], ["distance", "Symmetric distance"]] },
            { kind: "number", label: "Distance", key: "depth", when: p => p.extent === "distance" }
        ],
        InsertMesh: [
            { kind: "note", textOf: f => f ? "File: " + f.props.fileName + "  (units: " + f.props.unit + ")" : "" },
            { kind: "note", text: "The triangles are stored in the design, so the file is no longer needed." }
        ],
        MeshConvert: [
            { kind: "choice", label: "Direction", key: "direction", options: [["toBRep", "Mesh \u2192 Solid"], ["toMesh", "Solid \u2192 Mesh"]] },
            { kind: "selection", label: "Bodies", key: "bodies", sel: "bodies", filter: "body,face" },
            { kind: "choice", label: "Refinement", key: "refinement", when: p => p.direction === "toMesh",
              options: [["coarse", "Coarse"], ["medium", "Medium"], ["fine", "Fine"]] },
            { kind: "choice", label: "Method", key: "method", when: p => p.direction !== "toMesh",
              options: [["prismatic", "Prismatic"], ["faceted", "Faceted"]] },
            { kind: "note", textOf: f => f && f.summary ? "Result: " + f.summary : "" },
            { kind: "note", text: "Pick the bodies to convert. Prismatic fits planes, cylinders, cones and spheres with true "
                                  + "round edges; on a mesh with face groups each group is fitted as one patch. Faceted keeps "
                                  + "the flat triangles (coplanar ones merged). Both need a closed mesh; open meshes get one "
                                  + "face per triangle." }
        ],
        MeshFaceGroups: [
            { kind: "selection", label: "Mesh bodies", key: "bodies", sel: "bodies", filter: "body,face" },
            { kind: "choice", label: "Type", key: "method", options: [["accurate", "Accurate"], ["fast", "Fast"]] },
            { kind: "number", label: "Boundary tolerance", key: "tolerance", unit: "", when: p => p.method !== "fast" },
            { kind: "number", label: "Angle threshold", key: "angle", when: p => p.method === "fast" },
            { kind: "number", label: "Min group size %", key: "minSize", unit: "" },
            { kind: "note", textOf: f => f && f.summary ? "Result: " + f.summary : "" },
            { kind: "note", text: "Splits the mesh into face groups, shown in colour. Accurate fits planes, cylinders, "
                                  + "cones and spheres; Boundary tolerance is a fraction of the body size (Fusion: 0 to "
                                  + "0.01, default 0.001) — how far vertices may sit from a primitive. Fast splits where "
                                  + "neighbouring triangles bend more than the angle. Min group size is Fast-only. "
                                  + "Convert Mesh (Prismatic) makes one face from each group." }
        ],
        MeshRemesh: [
            { kind: "selection", label: "Mesh bodies", key: "bodies", sel: "bodies", filter: "body,face" },
            { kind: "number", label: "Target edge length", key: "edgeLength" },
            { kind: "bool", label: "Adaptive", key: "adaptive" },
            { kind: "number", label: "Iterations", key: "iterations", unit: "" },
            { kind: "number", label: "Keep edges sharper than", key: "sharpAngle" },
            { kind: "note", textOf: f => f && f.summary ? "Result: " + f.summary : "" },
            { kind: "note", text: "Rebuilds the triangles at an even size close to the target length. Adaptive uses smaller "
                                  + "triangles where the surface bends. Sharp edges and open boundaries stay in place." }
        ],
        ImportCAD: [
            { kind: "note", textOf: f => f ? f.props.format + " file: " + f.props.fileName : "" },
            { kind: "note", text: "The geometry is stored in the design, so the file is no longer needed. Each solid is a body; "
                                  + "loose surfaces are kept together as one body." }
        ],
        BaseFlange: [
            { kind: "selection", label: "Profile", key: "profiles", sel: "profiles", filter: "profile" },
            { kind: "number", label: "Thickness", key: "thickness" },
            { kind: "bool", label: "Flip", key: "flip" },
            { kind: "note", text: "Thickens a closed sketch profile into a sheet. Thickness defaults to the sheet metal rule "
                                  + "(parameter SheetThickness)." }
        ],
        EdgeFlange: [
            { kind: "selection", label: "Edge", key: "edge", sel: "edge", filter: "edge" },
            { kind: "number", label: "Height", key: "height" },
            { kind: "number", label: "Angle", key: "angle" },
            { kind: "number", label: "Bend radius", key: "bendRadius" },
            { kind: "bool", label: "Bend down", key: "flip" },
            { kind: "note", text: "Pick a straight edge along the top or bottom of a sheet. Height is the flat length after the "
                                  + "bend; the bend radius defaults to the sheet metal rule (BendRadius)." }
        ],
        FlatPattern: [
            { kind: "selection", label: "Sheet body", key: "body", sel: "body", filter: "body,face", presets: [[-1, "First sheet"]] },
            { kind: "number", label: "K-factor", key: "kFactor", unit: "" },
            { kind: "note", textOf: f => f && f.summary ? f.summary : "" },
            { kind: "note", text: "Unfolds every bend of the sheet, flanges on flanges included, into the sheet plane (bend "
                                  + "length = angle \u00d7 (R + K \u00d7 T)) and lays the result out beside the model as a new "
                                  + "body. Export it with Flat Pattern \u2192 Export DXF." }
        ],
        Hem: [
            { kind: "selection", label: "Edge", key: "edge", sel: "edge", filter: "edge" },
            { kind: "choice", label: "Type", key: "hemType", options: [["flat", "Flat"], ["open", "Open"], ["rolled", "Rolled"]] },
            { kind: "number", label: "Gap", key: "gap", when: p => p.hemType === "flat" },
            { kind: "number", label: "Radius", key: "radius", when: p => p.hemType !== "flat" },
            { kind: "number", label: "Angle", key: "angle", when: p => p.hemType === "rolled" },
            { kind: "number", label: "Length", key: "length" },
            { kind: "bool", label: "Fold down", key: "flip" },
            { kind: "note", text: "Folds a sheet edge back on itself. Flat leaves the gap between the sheet and the fold; Open "
                                  + "folds round the radius; Rolled curls the edge through the angle (180\u2013360 deg)." }
        ],
        Tab: [
            { kind: "selection", label: "Profile", key: "profiles", sel: "profiles", filter: "profile" },
            { kind: "number", label: "Thickness", key: "thickness" },
            { kind: "bool", label: "Flip", key: "flip" },
            { kind: "note", text: "Adds a sketch profile to the sheet it touches, in the sheet's plane. Sketch on a sheet face; "
                                  + "the tab grows into the sheet. Flip uses the other side of the sketch plane." }
        ],
        Bend: [
            { kind: "selection", label: "Bend line", key: "line", sel: "axis", filter: "sketchcurve" },
            { kind: "number", label: "Angle", key: "angle" },
            { kind: "number", label: "Bend radius", key: "bendRadius" },
            { kind: "number", label: "K-factor", key: "kFactor", unit: "" },
            { kind: "bool", label: "Other side moves", key: "flip" },
            { kind: "bool", label: "Bend down", key: "down" },
            { kind: "note", text: "Pick a sketch line drawn on a sheet face. The sheet folds along it and keeps its flat "
                                  + "length: the bend uses up sheet past the line." }
        ],
        ContourFlange: [
            { kind: "selection", label: "Profile lines", key: "sketch", sel: "path", filter: "sketchcurve" },
            { kind: "number", label: "Distance", key: "distance" },
            { kind: "number", label: "Thickness", key: "thickness" },
            { kind: "number", label: "Bend radius", key: "bendRadius" },
            { kind: "bool", label: "Thickness side", key: "flip" },
            { kind: "bool", label: "Reverse direction", key: "reverse" },
            { kind: "note", text: "Pick a sketch with one open chain of lines. It becomes a sheet with a bend at every corner, "
                                  + "run out along the sketch normal by the distance." }
        ],
        Unfold: [
            { kind: "selection", label: "Sheet body", key: "body", sel: "body", filter: "body,face", presets: [[-1, "First sheet"]] },
            { kind: "number", label: "K-factor", key: "kFactor", unit: "" },
            { kind: "note", textOf: f => f && f.summary ? f.summary : "" },
            { kind: "note", text: "Lays the sheet flat in place so holes and cuts can run across bends. Add Refold after them "
                                  + "to fold the sheet back with the changes." }
        ],
        Refold: [
            { kind: "note", textOf: f => f && f.summary ? f.summary : "" },
            { kind: "note", text: "Folds back the sheet of the last Unfold. Cuts and additions made while it was flat follow "
                                  + "the bends." }
        ],
        DeleteFace: [
            { kind: "selection", label: "Faces", key: "faces", sel: "faces", filter: "face" },
            { kind: "note", text: "The faces are removed and the faces around them grow to close the solid (holes, fillets, chamfers, bosses)." }
        ],
        Remove: [
            { kind: "selection", label: "Bodies", key: "bodies", sel: "bodies", filter: "body,face" },
            { kind: "note", text: "The bodies are removed from this point of the timeline on; delete or roll back this feature to bring them back." }
        ],
        MeshReverse: [
            { kind: "selection", label: "Mesh bodies", key: "bodies", sel: "bodies", filter: "body,face" }
        ],
        MeshSmooth: [
            { kind: "selection", label: "Mesh bodies", key: "bodies", sel: "bodies", filter: "body,face" },
            { kind: "number", label: "Iterations", key: "iterations", unit: "" },
            { kind: "number", label: "Smoothness", key: "smoothness", unit: "" }
        ],
        MeshReduce: [
            { kind: "selection", label: "Mesh bodies", key: "bodies", sel: "bodies", filter: "body,face" },
            { kind: "number", label: "Keep %", key: "proportion", unit: "" }
        ],
        MeshRepair: [
            { kind: "selection", label: "Mesh bodies", key: "bodies", sel: "bodies", filter: "body,face" },
            { kind: "number", label: "Weld tolerance", key: "tolerance" },
            { kind: "bool", label: "Orient normals", key: "orient" },
            { kind: "bool", label: "Fill holes", key: "fillHoles" },
            { kind: "note", textOf: f => f && f.summary ? f.summary : "" },
            { kind: "note", text: "Welds nearby vertices, removes repeated triangles, makes the normals of each piece agree "
                                  + "(pointing out of closed pieces) and closes open boundaries." }
        ],
        MeshShell: [
            { kind: "selection", label: "Mesh bodies", key: "bodies", sel: "bodies", filter: "body,face" },
            { kind: "number", label: "Thickness", key: "thickness" },
            { kind: "choice", label: "Direction", key: "direction", options: [["inside", "Inside"], ["outside", "Outside"]] },
            { kind: "note", text: "A closed mesh is hollowed; an open one is thickened into a closed wall. Drag the arrow on the body to set the thickness." }
        ],
        MeshCombine: [
            { kind: "selection", label: "Mesh bodies", key: "bodies", sel: "bodies", filter: "body,face" },
            { kind: "note", text: "The first body keeps the triangles of all of them; the others are removed." }
        ],
        MeshPlaneCut: [
            { kind: "selection", label: "Mesh bodies", key: "bodies", sel: "bodies", filter: "body,face" },
            plane("Cutting plane", "plane"),
            { kind: "number", label: "Offset", key: "offset" },
            { kind: "bool", label: "Flip", key: "flip" },
            { kind: "bool", label: "Fill hole", key: "fill" },
            { kind: "note", text: "Removes the part in front of the plane (along its normal). Drag the arrow to move the plane." }
        ]
    })

    fields: specs[type] || []
}
