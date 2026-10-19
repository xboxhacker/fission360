// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
import QtQuick

// Construction planes, axes and points
FeatureDialog {
    readonly property string type: feature ? feature.type : ""

    readonly property var planeFields: [
        { kind: "choice", label: "Type", key: "mode",
          options: [["offset", "Offset plane"], ["angle", "Plane at angle"], ["midplane", "Midplane"], ["threePoints", "Through 3 points"]] },
        { kind: "selection", label: "Plane", key: "base", sel: "plane", filter: "face,plane", presets: planePresets,
          when: p => p.mode !== "threePoints" },
        { kind: "number", label: "Distance", key: "offset", when: p => p.mode === "offset" },
        { kind: "selection", label: "Axis", key: "axis", sel: "axis", filter: "edge,axis,sketchcurve", presets: axisPresets,
          when: p => p.mode === "angle" },
        { kind: "number", label: "Angle", key: "angle", when: p => p.mode === "angle" },
        { kind: "selection", label: "Plane 2", key: "base2", sel: "plane", filter: "face,plane", presets: planePresets,
          when: p => p.mode === "midplane" },
        { kind: "vector", label: "Point 1", key: "p1", when: p => p.mode === "threePoints" },
        { kind: "vector", label: "Point 2", key: "p2", when: p => p.mode === "threePoints" },
        { kind: "vector", label: "Point 3", key: "p3", when: p => p.mode === "threePoints" }
    ]
    readonly property var axisFields: [
        { kind: "choice", label: "Type", key: "mode",
          options: [["twoPoints", "Through two points"], ["edge", "Along edge"], ["normal", "Normal to plane at point"]] },
        { kind: "vector", label: "Point 1", key: "p1", when: p => p.mode === "twoPoints" },
        { kind: "vector", label: "Point 2", key: "p2", when: p => p.mode === "twoPoints" },
        { kind: "selection", label: "Edge", key: "edge", sel: "edge", filter: "edge", when: p => p.mode === "edge" },
        { kind: "selection", label: "Plane", key: "base", sel: "plane", filter: "face,plane", presets: planePresets,
          when: p => p.mode === "normal" },
        { kind: "vector", label: "Point", key: "point", when: p => p.mode === "normal" }
    ]
    readonly property var pointFields: [
        { kind: "number", label: "X", key: "x" },
        { kind: "number", label: "Y", key: "y" },
        { kind: "number", label: "Z", key: "z" }
    ]

    fields: type === "ConstructionPlane" ? planeFields : type === "ConstructionAxis" ? axisFields : pointFields
}
