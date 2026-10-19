// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
import QtQuick

FeatureDialog {
    fields: [
        { kind: "selection", label: "Faces", key: "faces", sel: "faces", filter: "face" },
        { kind: "selection", label: "Body", key: "body", sel: "body", filter: "body,face",
          when: p => !(p.faces && p.faces.length) },
        { kind: "number", label: "Thickness", key: "thickness" },
        { kind: "choice", label: "Direction", key: "direction", options: [["inside", "Inside"], ["outside", "Outside"]] },
        { kind: "note", text: "Selected faces are removed; with no faces the body is hollowed into a closed shell." }
    ]
}
