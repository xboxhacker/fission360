// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#pragma once
#include "FissionFeature.h"

// props:
//   tubeDiameter         : expr (mm) — wire / tube diameter
//   innerDiameter        : expr (mm) — hole diameter through the centre
//   origin               : [x, y, z]
//   operation, targets   : as Extrude
class FissionTorus : public FissionFeature {
    Q_OBJECT
public:
    explicit FissionTorus(QObject *parent = nullptr) : FissionFeature(parent) {}
    QString type() const override { return QStringLiteral("Torus"); }
    QVariantMap defaults() const override;
    QStringList expressionKeys() const override { return {"tubeDiameter", "innerDiameter"}; }

protected:
    bool compute(RebuildContext &ctx) override;
};