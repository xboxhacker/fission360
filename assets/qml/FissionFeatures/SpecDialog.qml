// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
import QtQuick

// Forms for the MODIFY / CREATE tools that need no custom UI beyond the generic field kinds
FeatureDialog {
    id: sd
    readonly property string type: feature ? feature.type : ""
    readonly property var bodyFilter: "body,face"
    readonly property var planeSel: ({ kind: "selection", sel: "plane", filter: "face,plane", presets: planePresets })

    function plane(label, key, extra) { return Object.assign({}, planeSel, extra || {}, { label: label, key: key }) }
    readonly property var startField: ({ kind: "vector", label: "Start", key: "origin", labels: ["U", "V", "W"] })
    readonly property var startNote: ({ kind: "note", text: "Click the plane to set the start point. Drag an arrow, or type in its box, to change a size." })

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

    readonly property var primitiveOp: [["newBody", "New"], ["join", "Join"], ["cut", "Cut"], ["intersect", "Intersect"]]

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
        Box: [
            sd.plane("Plane", "plane", { placeStart: true }),
            { kind: "number", label: "Width", key: "width" },
            { kind: "number", label: "Length", key: "length" },
            { kind: "number", label: "Depth", key: "depth" },
            sd.startField,
            { kind: "choice", label: "Operation", key: "operation", options: sd.primitiveOp },
            sd.startNote
        ],
        Cylinder: [
            sd.plane("Plane", "plane", { placeStart: true }),
            { kind: "number", label: "Diameter", key: "diameter" },
            { kind: "number", label: "Height", key: "height" },
            sd.startField,
            { kind: "choice", label: "Operation", key: "operation", options: sd.primitiveOp },
            sd.startNote
        ],
        Sphere: [
            sd.plane("Plane", "plane", { placeStart: true }),
            { kind: "number", label: "Diameter", key: "diameter" },
            sd.startField,
            { kind: "choice", label: "Operation", key: "operation", options: sd.primitiveOp },
            sd.startNote
        ],
        Torus: [
            sd.plane("Plane", "plane", { placeStart: true }),
            { kind: "number", label: "Inner diameter", key: "innerDiameter" },
            { kind: "number", label: "Tube diameter", key: "tubeDiameter" },
            sd.startField,
            { kind: "choice", label: "Operation", key: "operation", options: sd.primitiveOp },
            sd.startNote
        ],
        Coil: [
            sd.plane("Plane", "plane", { placeStart: true }),
            { kind: "number", label: "Diameter", key: "diameter" },
            { kind: "number", label: "Tube diameter", key: "tubeDiameter" },
            { kind: "number", label: "Pitch", key: "pitch" },
            { kind: "number", label: "Height", key: "height" },
            sd.startField,
            { kind: "choice", label: "Operation", key: "operation", options: sd.primitiveOp },
            sd.startNote
        ],
        Pipe: [
            sd.plane("Plane", "plane", { placeStart: true }),
            { kind: "number", label: "Outer diameter", key: "outerDiameter" },
            { kind: "number", label: "Inner diameter", key: "innerDiameter" },
            { kind: "number", label: "Length", key: "length" },
            sd.startField,
            { kind: "choice", label: "Operation", key: "operation", options: sd.primitiveOp },
            sd.startNote
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
            { kind: "note", textOf: f => sd.moveHint(f) }
        ],
        Scale: [
            { kind: "selection", label: "Bodies", key: "bodies", sel: "bodies", filter: bodyFilter },
            { kind: "vector", key: "scale", labels: ["Scale X", "Scale Y", "Scale Z"], unit: "" },
            { kind: "bool", label: "Non-uniform", key: "nonUniform" }
        ],
        SplitBody: [
            { kind: "selection", label: "Body", key: "body", sel: "body", filter: bodyFilter },
            { kind: "selection", label: "Split with", key: "faces", sel: "faces", filter: "face" },
            { kind: "note", text: "The body is split along the picked faces. Drag the arrow to flip which side is removed." }
        ],
        SplitFace: [
            { kind: "selection", label: "Body", key: "body", sel: "body", filter: bodyFilter },
            { kind: "selection", label: "Faces to split", key: "faces", sel: "faces", filter: "face" },
            { kind: "selection", label: "Cut with", key: "cutFaces", sel: "faces", filter: "face" }
        ],
        OffsetFace: [
            { kind: "selection", label: "Body", key: "body", sel: "body", filter: bodyFilter },
            { kind: "selection", label: "Faces", key: "faces", sel: "faces", filter: "face" },
            { kind: "number", label: "Offset", key: "offset" }
        ],
        Offset: [
            { kind: "selection", label: "Body", key: "body", sel: "body", filter: bodyFilter },
            { kind: "number", label: "Offset", key: "offset" },
            { kind: "note", textOf: f => f && f.summary ? f.summary : "" },
            { kind: "note", text: "Offsets every face of the body outward. Drag the arrow to set the offset. "
                                  + "It reports faces that would flip or self-intersect before it commits." }
        ],
        Draft: [
            { kind: "selection", label: "Body", key: "body", sel: "body", filter: bodyFilter },
            { kind: "selection", label: "Faces", key: "faces", sel: "faces", filter: "face" },
            { kind: "number", label: "Angle", key: "angle" },
            { kind: "selection", label: "Neutral plane", key: "plane", sel: "plane", filter: "face,plane" },
            { kind: "note", text: "Drafts the picked faces about the neutral plane. Drag the angle handle to set the angle." }
        ],
        ReplaceFace: [
            { kind: "selection", label: "Body", key: "body", sel: "body", filter: bodyFilter },
            { kind: "selection", label: "Faces to replace", key: "faces", sel: "faces", filter: "face" },
            { kind: "note", text: "Replaces the picked faces with the nearest face of another body." }
        ],
        Joint: [
            { kind: "selection", label: "Moving bodies", key: "bodies", sel: "bodies", filter: bodyFilter },
            { kind: "selection", label: "Base body", key: "base", sel: "body", filter: bodyFilter },
            { kind: "choice", label: "Motion", key: "motion",
              options: [["revolute", "Revolute"], ["slider", "Slider"], ["cylindrical", "Cylindrical"], ["fixed", "Fixed"]] },
            { kind: "point3", label: "Joint origin", key: "origin", prompt: "Click the joint origin" },
            { kind: "selection", label: "Joint axis", key: "axis", sel: "axis", filter: "edge,axis,sketchcurve", presets: axisPresets },
            { kind: "number", label: "Angle", key: "angle" },
            { kind: "number", label: "Offset Z", key: "offsetZ" },
            { kind: "bool", label: "As-built joint", key: "asBuilt" },
            { kind: "note", textOf: f => sd.jointHint(f) }
        ],
        PathPattern: [
            { kind: "selection", label: "Body", key: "body", sel: "body", filter: bodyFilter },
            { kind: "selection", label: "Path", key: "path", sel: "path", filter: "sketchcurve,edge,axis" },
            { kind: "number", label: "Count", key: "count", unit: "" },
            { kind: "number", label: "Spacing", key: "spacing" },
            { kind: "bool", label: "Occur along path", key: "occurs" },
            { kind: "note", text: "Patterns the feature along the path. Drag the arrow to set the count." }
        ],
        Rib: [
            { kind: "selection", label: "Body", key: "body", sel: "body", filter: bodyFilter },
            { kind: "selection", label: "Faces", key: "faces", sel: "faces", filter: "face" },
            { kind: "number", label: "Thickness", key: "thickness" },
            { kind: "number", label: "Depth", key: "depth" }
        ],
        Web: [
            { kind: "selection", label: "Body", key: "body", sel: "body", filter: bodyFilter },
            { kind: "selection", label: "Faces", key: "faces", sel: "faces", filter: "face" },
            { kind: "number", label: "Thickness", key: "thickness" },
            { kind: "number", label: "Depth", key: "depth" }
        ],
        BaseFlange: [
            { kind: "selection", label: "Sheet body", key: "body", sel: "body", filter: "body,face", presets: [[-1, "First sheet"]] },
            { kind: "number", label: "Thickness", key: "thickness" },
            { kind: "number", label: "Bend radius", key: "bendRadius" },
            { kind: "number", label: "Offset", key: "offset" }
        ],
        EdgeFlange: [
            { kind: "selection", label: "Edge", key: "face", sel: "face", filter: "edge,face" },
            { kind: "number", label: "Length", key: "length" },
            { kind: "number", label: "Thickness", key: "thickness" },
            { kind: "number", label: "Bend radius", key: "bendRadius" },
            { kind: "number", label: "Angle", key: "angle" },
            { kind: "choice", label: "Bend", key: "bend", options: [["none", "None"], ["inside", "Inside"], ["outside", "Outside"]] }
        ],
        Hem: [
            { kind: "selection", label: "Edge", key: "face", sel: "face", filter: "edge,face" },
            { kind: "number", label: "Length", key: "length" },
            { kind: "number", label: "Thickness", key: "thickness" },
            { kind: "number", label: "Bend radius", key: "bendRadius" },
            { kind: "number", label: "Angle", key: "angle" }
        ],
        Tab: [
            { kind: "selection", label: "Edge", key: "face", sel: "face", filter: "edge,face" },
            { kind: "number", label: "Length", key: "length" },
            { kind: "number", label: "Thickness", key: "thickness" },
            { kind: "number", label: "Bend radius", key: "bendRadius" },
            { kind: "choice", label: "Shape", key: "shape", options: [["rect", "Rectangle"], ["tri", "Triangle"]] }
        ],
        Bend: [
            { kind: "selection", label: "Edge", key: "face", sel: "face", filter: "edge,face" },
            { kind: "number", label: "Bend radius", key: "bendRadius" },
            { kind: "number", label: "Angle", key: "angle" },
            { kind: "choice", label: "Bend", key: "bend", options: [["inside", "Inside"], ["outside", "Outside"]] },
            { kind: "note", text: "Bends the sheet along the picked edge. Drag the angle handle to set the bend angle." }
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