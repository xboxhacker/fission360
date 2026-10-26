// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
pragma Singleton
import QtQuick

QtObject {
    readonly property color window: "#e9ebee"
    readonly property color panel: "#f7f8f9"
    readonly property color panelAlt: "#eef0f2"
    readonly property color header: "#3c4249"
    readonly property color headerText: "#f2f4f6"
    readonly property color border: "#c9ced4"
    readonly property color text: "#24292e"
    readonly property color textDim: "#6a737d"
    readonly property color accent: "#0696d7"
    readonly property color accentLight: "#d6eefa"
    readonly property color hover: "#e1e6eb"
    readonly property color error: "#d13b3b"
    readonly property color warning: "#e08a1e"
    readonly property color success: "#2e9e5b"
    readonly property color sketch: "#3f8fe0"
    readonly property color construction: "#e09232"
    readonly property color manipulator: "#f07c1a"

    readonly property int fontSize: 12
    readonly property int smallFont: 11
    readonly property string fontFamily: "Segoe UI"

    function featureColor(type) {
        switch (type) {
        case "Sketch": return sketch
        case "ConstructionPlane": case "ConstructionAxis": case "ConstructionPoint": return construction
        case "Fillet": case "Chamfer": case "Shell": return "#7a5bc4"
        case "Mirror": case "RectangularPattern": case "CircularPattern": return "#2e9e8f"
        default: return "#d07a2a"
        }
    }

    // Fusion imports keep Fusion's type in props.fusionType. Steps with no Fission feature are ImportCAD
    // pass-throughs, so the glyph comes from the Fusion type, not from ImportCAD (Insert STEP).
    function featureGlyph(type, fusionType, name) {
        if (fusionType && fusionType.length) return fusionGlyph(fusionType, name || "")
        return fissionGlyph(type)
    }

    function fusionGlyph(raw, name) {
        switch (raw.replace("adsk::fusion::", "")) {
        case "Sketch": return "sketch"
        case "ExtrudeFeature": return "extrude"
        case "RevolveFeature": return "revolve"
        case "FilletFeature": return "fillet"
        case "ChamferFeature": return "chamfer"
        case "ShellFeature": return "shell"
        case "HoleFeature": return "hole"
        case "SweepFeature": return "sweep"
        case "LoftFeature": return "loft"
        case "MirrorFeature": return "mirror"
        case "RectangularPatternFeature": return "rectpattern"
        case "CircularPatternFeature": return "circpattern"
        case "PathPatternFeature": return "pathpattern"
        case "ThreadFeature": return "thread"
        case "CombineFeature": return "combine"
        case "MoveFeature": return "move"
        case "ScaleFeature": return "scale"
        case "SplitBodyFeature": return "splitbody"
        case "SplitFaceFeature": return "splitface"
        case "OffsetFacesFeature": return "offsetface"
        case "OffsetFeature": return "offsetbody"
        case "ReplaceFaceFeature": return "replaceface"
        case "DraftFeature": return "draft"
        case "RibFeature": return "rib"
        case "WebFeature": return "web"
        case "ConstructionPlane": return "plane"
        case "ConstructionAxis": return "axis"
        case "ConstructionPoint": return "point"
        case "Joint": case "AsBuiltJoint": return "joint"
        case "FlangeFeature": return "edgeflange"
        case "HemFeature": return "hem"
        case "BendFeature": return "bend"
        case "UnfoldFeature": return "unfold"
        case "RefoldFeature": return "refold"
        case "FlatPattern": return "flatpattern"
        case "MeshReduceFeature": return "reduce"
        case "MeshSmoothFeature": return "smooth"
        case "MeshRemeshFeature": return "remesh"
        case "MeshRepairFeature": return "meshrepair"
        case "MeshPlaneCutFeature": return "planecut"
        case "MeshShellFeature": return "shell"
        case "MeshCombineFeature": return "combine"
        case "MeshReverseNormalFeature": return "reversenormals"
        case "MeshConvertFeature": return "tobrep"
        case "MeshGenerateFaceGroupsFeature": return "facegroups"
        // Fusion-only steps
        case "DeleteFaceFeature": return "deleteface"
        case "Occurrence": return "component"
        case "Snapshot": return "position"
        case "TimelineGroup": return "group"
        case "BoundaryFillFeature": return "boundaryfill"
        case "FormFeature": return "form"
        case "ThickenFeature": return "thicken"
        case "Feature":   // features with no public API class of their own
            if (/mesh\s*section/i.test(name)) return "meshsection"
            if (/^base\s*mesh/i.test(name)) return "basemesh"
            return "basefeature"
        default: return "fusionfeature"
        }
    }

    function fissionGlyph(type) {
        switch (type) {
        case "Sketch": return "sketch"
        case "Extrude": return "extrude"
        case "Revolve": return "revolve"
        case "Fillet": return "fillet"
        case "Chamfer": return "chamfer"
        case "Loft": return "loft"
        case "Sweep": return "sweep"
        case "Shell": return "shell"
        case "Mirror": return "mirror"
        case "RectangularPattern": return "rectpattern"
        case "CircularPattern": return "circpattern"
        case "ConstructionPlane": return "plane"
        case "ConstructionAxis": return "axis"
        case "ConstructionPoint": return "point"
        case "Hole": return "hole"
        case "Thread": return "thread"
        case "Combine": return "combine"
        case "Move": return "move"
        case "Scale": return "scale"
        case "SplitBody": return "splitbody"
        case "SplitFace": return "splitface"
        case "OffsetFace": return "offsetface"
        case "Offset": return "offsetbody"
        case "ReplaceFace": return "replaceface"
        case "Draft": return "draft"
        case "Joint": return "joint"
        case "PathPattern": return "pathpattern"
        case "Rib": return "rib"
        case "Web": return "web"
        case "InsertMesh": return "insertmesh"
        case "MeshConvert": return "tobrep"
        case "MeshReverse": return "reversenormals"
        case "MeshSmooth": return "smooth"
        case "MeshReduce": return "reduce"
        case "MeshPlaneCut": return "planecut"
        case "MeshRepair": return "meshrepair"
        case "MeshFaceGroups": return "facegroups"
        case "MeshRemesh": return "remesh"
        case "MeshShell": return "shell"
        case "MeshCombine": return "combine"
        case "Remove": return "remove"
        case "DeleteFace": return "deleteface"
        case "ImportCAD": return "importcad"
        case "BaseFlange": return "flange"
        case "EdgeFlange": return "edgeflange"
        case "FlatPattern": return "flatpattern"
        case "Hem": return "hem"
        case "Tab": return "tab"
        case "Bend": return "bend"
        case "ContourFlange": return "contourflange"
        case "Unfold": return "unfold"
        case "Refold": return "refold"
        case "Box": return "box"
        case "Cylinder": return "cylinder"
        case "Sphere": return "sphere"
        case "Torus": return "torus"
        case "Coil": return "coil"
        case "Pipe": return "pipe"
        default: return "box"
        }
    }
}
