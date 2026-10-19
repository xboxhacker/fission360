// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
import QtQuick

FeatureDialog {
    fields: [
        { kind: "bool", label: "All edges", key: "allEdges" },
        { kind: "selection", label: "Edges", key: "edges", sel: "edges", filter: "edge,face", faceEdges: true, when: p => !p.allEdges },
        { kind: "bool", label: "Tangent chain", key: "tangentChain", when: p => !p.allEdges },
        { kind: "selection", label: "Body", key: "body", sel: "body", filter: "body,face", when: p => !!p.allEdges },
        { kind: "choice", label: "Radius type", key: "radiusType", options: [["constant", "Constant"], ["variable", "Variable"]] },
        { kind: "number", label: "Radius", key: "radius", when: p => p.radiusType !== "variable" },
        { kind: "number", label: "Start radius", key: "radius", when: p => p.radiusType === "variable" },
        { kind: "number", label: "End radius", key: "radius2", when: p => p.radiusType === "variable" },
        { kind: "note", text: "The radius changes evenly from start to end along each tangent chain.", when: p => p.radiusType === "variable" }
    ]
}
