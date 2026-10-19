// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
import QtQuick

FeatureDialog {
    fields: [
        { kind: "selection", label: "Profile", key: "profiles", sel: "profiles", filter: "profile,face" },
        { kind: "selection", label: "Path", key: "path", sel: "path", filter: "sketchcurve" },
        { kind: "note", text: "The path is every curve of the picked sketch, chained end to end. The profile can be sketch profiles or a flat body face." },
        { kind: "choice", label: "Orientation", key: "orientation",
          options: [["perpendicular", "Perpendicular"], ["parallel", "Parallel"]] },
        { kind: "number", label: "Twist angle", key: "twist" },
        { kind: "choice", label: "Operation", key: "operation", options: operations }
    ]
}
