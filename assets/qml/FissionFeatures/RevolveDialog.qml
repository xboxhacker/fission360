// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
import QtQuick

FeatureDialog {
    fields: [
        { kind: "selection", label: "Profile", key: "profiles", sel: "profiles", filter: "profile,face" },
        { kind: "selection", label: "Axis", key: "axis", sel: "axis", filter: "sketchcurve,edge,axis", presets: axisPresets },
        { kind: "choice", label: "Extent", key: "extent",
          options: [["full", "Full"], ["angle", "Angle"], ["symmetric", "Symmetric"]] },
        { kind: "number", label: "Angle", key: "angle", when: p => p.extent !== "full" },
        { kind: "bool", label: "Flip", key: "flip", when: p => p.extent === "angle" },
        { kind: "choice", label: "Operation", key: "operation", options: operations },
        { kind: "note", text: "The profile can be sketch profiles or a flat body face. Pick a sketch line, a model edge or a construction axis as the axis." }
    ]
}
