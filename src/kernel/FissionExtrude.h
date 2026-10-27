// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#pragma once
#include "FissionFeature.h"

// props:
//   sketch    : sketch feature id          profiles : [indices] (empty = default regions)
//   extent    : "distance" | "symmetric" | "twoSides" | "throughAll" | "toObject"
//   distance  : expr   distance2 : expr (twoSides)   taper : expr (deg)
//   startOffset : expr (shifts the start of a distance extrude along the normal)
//   to        : face / plane ref when extent is toObject
//   flip      : bool   operation : "join" | "cut" | "intersect" | "newBody"
//   targets   : [body indices] (empty = automatic; hidden bodies are skipped)
class FissionExtrude : public FissionFeature {
    Q_OBJECT
public:
    explicit FissionExtrude(QObject *parent = nullptr) : FissionFeature(parent) {}
    QString type() const override { return QStringLiteral("Extrude"); }
    QVariantMap defaults() const override;
    QStringList expressionKeys() const override { return {"distance", "distance2", "taper", "startOffset"}; }

protected:
    bool compute(RebuildContext &ctx) override;
};
