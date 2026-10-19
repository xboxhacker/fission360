// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
import QtQuick

// Rectangular and circular patterns of bodies
FeatureDialog {
    readonly property bool circular: feature && feature.type === "CircularPattern"
    readonly property var ops: [["join", "Join"], ["newBody", "New Body"]]

    fields: circular ? [
        { kind: "selection", label: "Objects", key: "bodies", sel: "bodies", filter: "body,face" },
        { kind: "selection", label: "Axis", key: "axis", sel: "axis", filter: "edge,axis,sketchcurve", presets: axisPresets },
        { kind: "number", label: "Quantity", key: "count" },
        { kind: "number", label: "Total angle", key: "angle" },
        { kind: "choice", label: "Operation", key: "operation", options: ops }
    ] : [
        { kind: "selection", label: "Objects", key: "bodies", sel: "bodies", filter: "body,face" },
        { kind: "selection", label: "Direction 1", key: "direction1", sel: "axis", filter: "edge,axis,sketchcurve", presets: axisPresets },
        { kind: "number", label: "Quantity", key: "count1" },
        { kind: "number", label: "Spacing", key: "spacing1" },
        { kind: "selection", label: "Direction 2", key: "direction2", sel: "axis", filter: "edge,axis,sketchcurve",
          presets: [["", "None"]].concat(axisPresets) },
        { kind: "number", label: "Quantity 2", key: "count2", when: p => !!p.direction2 },
        { kind: "number", label: "Spacing 2", key: "spacing2", when: p => !!p.direction2 },
        { kind: "choice", label: "Operation", key: "operation", options: ops },
        { kind: "note", text: "Objects: empty = all bodies." }
    ]
}
