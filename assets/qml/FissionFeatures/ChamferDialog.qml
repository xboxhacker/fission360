// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
import QtQuick

FeatureDialog {
    fields: [
        { kind: "bool", label: "All edges", key: "allEdges" },
        { kind: "selection", label: "Edges", key: "edges", sel: "edges", filter: "edge,face", faceEdges: true, when: p => !p.allEdges },
        { kind: "bool", label: "Tangent chain", key: "tangentChain", when: p => !p.allEdges },
        { kind: "selection", label: "Body", key: "body", sel: "body", filter: "body,face", when: p => !!p.allEdges },
        { kind: "choice", label: "Type", key: "chamferType",
          options: [["equal", "Equal distance"], ["twoDistances", "Two distances"], ["distanceAngle", "Distance and angle"]] },
        { kind: "number", label: "Distance", key: "distance" },
        { kind: "number", label: "Distance 2", key: "distance2", when: p => p.chamferType === "twoDistances" },
        { kind: "number", label: "Angle", key: "angle", when: p => p.chamferType === "distanceAngle" }
    ]
}
