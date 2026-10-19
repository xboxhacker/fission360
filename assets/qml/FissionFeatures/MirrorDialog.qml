// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
import QtQuick

FeatureDialog {
    fields: [
        { kind: "selection", label: "Objects", key: "bodies", sel: "bodies", filter: "body,face", autoNext: true },
        { kind: "selection", label: "Mirror plane", key: "plane", sel: "plane", filter: "face,plane", presets: planePresets },
        { kind: "note", text: "Pick the bodies, then a plane: an origin or construction plane, or any flat face." },
        { kind: "choice", label: "Operation", key: "operation", options: [["join", "Join"], ["newBody", "New Body"]] }
    ]
}
