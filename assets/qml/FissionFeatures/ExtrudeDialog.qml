// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
import QtQuick

FeatureDialog {
    fields: [
        { kind: "selection", label: "Profile", key: "profiles", sel: "profiles", filter: "profile,face" },
        { kind: "choice", label: "Extent", key: "extent",
          options: [["distance", "Distance"], ["symmetric", "Symmetric"], ["twoSides", "Two Sides"],
                    ["throughAll", "All"], ["toObject", "To Object"]] },
        { kind: "number", label: "Distance", key: "distance",
          when: p => p.extent !== "throughAll" && p.extent !== "toObject" },
        { kind: "number", label: "Distance 2", key: "distance2", when: p => p.extent === "twoSides" },
        { kind: "number", label: "Start offset", key: "startOffset",
          when: p => p.extent === "distance" || p.extent === "toObject" || p.extent === "throughAll" },
        { kind: "selection", label: "To", key: "to", sel: "plane", filter: "face,plane",
          when: p => p.extent === "toObject" },
        { kind: "number", label: "Taper angle", key: "taper", when: p => p.extent === "distance" },
        { kind: "bool", label: "Flip", key: "flip", when: p => p.extent !== "symmetric" },
        { kind: "choice", label: "Operation", key: "operation", options: operations },
        { kind: "selection", label: "Objects", key: "targets", sel: "bodies", filter: "body,face",
          when: p => p.operation !== "newBody" },
        { kind: "note", text: "Click a face to pull it. Shift-click or Ctrl-click adds another face to the same pull. "
                              + "Click a different face to set the distance out to that face. "
                              + "Objects: bodies to combine with (empty = every body the tool touches)." }
    ]
}
