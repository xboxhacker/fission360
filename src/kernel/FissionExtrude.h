// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#pragma once
#include "FissionFeature.h"

// props:
//   sketch    : sketch feature id          profiles : [indices] (empty = default regions)
//   extent    : "distance" | "symmetric" | "twoSides" | "throughAll"
//   distance  : expr   distance2 : expr (twoSides)   taper : expr (deg)
//   flip      : bool   operation : "join" | "cut" | "intersect" | "newBody"
//   targets   : [body indices] (empty = automatic)
class FissionExtrude : public FissionFeature {
    Q_OBJECT
public:
    explicit FissionExtrude(QObject *parent = nullptr) : FissionFeature(parent) {}
    QString type() const override { return QStringLiteral("Extrude"); }
    QVariantMap defaults() const override;
    QStringList expressionKeys() const override { return {"distance", "distance2", "taper"}; }

protected:
    bool compute(RebuildContext &ctx) override;
};
