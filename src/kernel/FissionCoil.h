// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#pragma once
#include "FissionFeature.h"

// props:
//   diameter             : expr (mm) — helix centre-line diameter
//   tubeDiameter         : expr (mm) — wire / tube diameter
//   pitch                : expr (mm) — distance between adjacent turns
//   height               : expr (mm) — total coil height
//   origin               : [x, y, z]
//   operation, targets   : as Extrude
class FissionCoil : public FissionFeature {
    Q_OBJECT
public:
    explicit FissionCoil(QObject *parent = nullptr) : FissionFeature(parent) {}
    QString type() const override { return QStringLiteral("Coil"); }
    QVariantMap defaults() const override;
    QStringList expressionKeys() const override { return {"diameter", "tubeDiameter", "pitch", "height"}; }

protected:
    bool compute(RebuildContext &ctx) override;
};