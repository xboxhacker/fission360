// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
import QtQuick

// Rectangular and circular patterns of bodies — or of faces (holes/pockets) that Cut the source body
FeatureDialog {
    readonly property bool circular: feature && feature.type === "CircularPattern"
    readonly property bool faceMode: !!(props.faces && props.faces.length)
    readonly property var ops: faceMode
        ? [["cut", "Cut"], ["join", "Join"], ["newBody", "New Body"]]
        : [["join", "Join"], ["newBody", "New Body"], ["cut", "Cut"]]

    fields: circular ? [
        { kind: "selection", label: "Faces (holes)", key: "faces", sel: "faces", filter: "face" },
        { kind: "selection", label: "Bodies", key: "bodies", sel: "bodies", filter: "body,face",
          when: p => !(p.faces && p.faces.length) },
        { kind: "selection", label: "Axis", key: "axis", sel: "axis", filter: "edge,axis,sketchcurve", presets: axisPresets },
        { kind: "number", label: "Quantity", key: "count" },
        { kind: "number", label: "Total angle", key: "angle" },
        { kind: "choice", label: "Operation", key: "operation", options: ops },
        { kind: "note", text: "Pick a hole face to pattern more holes (Cut). Or leave Faces empty and pattern whole bodies." }
    ] : [
        { kind: "selection", label: "Faces (holes)", key: "faces", sel: "faces", filter: "face" },
        { kind: "selection", label: "Bodies", key: "bodies", sel: "bodies", filter: "body,face",
          when: p => !(p.faces && p.faces.length) },
        { kind: "selection", label: "Direction 1", key: "direction1", sel: "axis", filter: "edge,axis,sketchcurve", presets: axisPresets },
        { kind: "number", label: "Quantity", key: "count1" },
        { kind: "number", label: "Spacing", key: "spacing1" },
        { kind: "selection", label: "Direction 2", key: "direction2", sel: "axis", filter: "edge,axis,sketchcurve",
          presets: [["", "None"]].concat(axisPresets) },
        { kind: "number", label: "Quantity 2", key: "count2", when: p => !!p.direction2 },
        { kind: "number", label: "Spacing 2", key: "spacing2", when: p => !!p.direction2 },
        { kind: "choice", label: "Operation", key: "operation", options: ops },
        { kind: "note", text: "Pick hole/pocket faces to Cut patterned openings into the body. Bodies: empty = all." }
    ]
}
