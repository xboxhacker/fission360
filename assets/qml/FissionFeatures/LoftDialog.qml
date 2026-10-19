// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
import QtQuick

FeatureDialog {
    fields: [
        { kind: "selection", label: "Sections", key: "sections", sel: "sections", filter: "profile,face" },
        { kind: "note", text: "Pick faces and sketch profiles in loft order. A face is lofted from its whole outer boundary." },
        { kind: "choice", label: "Guide", key: "guide",
          options: [["none", "None"], ["rail", "Rail"], ["centerline", "Centerline"]] },
        { kind: "selection", label: "Rail", key: "rail", sel: "path", filter: "sketchcurve", when: p => p.guide === "rail" },
        { kind: "selection", label: "Centerline", key: "centerline", sel: "path", filter: "sketchcurve", when: p => p.guide === "centerline" },
        { kind: "note", text: "A rail is a sketch of its own whose curves touch every section's outline; the sections turn and scale to follow it.",
          when: p => p.guide === "rail" },
        { kind: "note", text: "A centerline is a sketch of its own whose curves pass through every section; the loft follows it.",
          when: p => p.guide === "centerline" },
        { kind: "bool", label: "Ruled", key: "ruled", when: p => !p.guide || p.guide === "none" },
        { kind: "bool", label: "Solid", key: "solid" },
        { kind: "choice", label: "Operation", key: "operation", options: operations }
    ]
}
