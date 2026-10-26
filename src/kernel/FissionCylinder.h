// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#pragma once
#include "FissionFeature.h"

// props:
//   diameter, height     : expr (mm)
//   origin               : [x, y, z]
//   operation, targets   : as Extrude
class FissionCylinder : public FissionFeature {
    Q_OBJECT
public:
    explicit FissionCylinder(QObject *parent = nullptr) : FissionFeature(parent) {}
    QString type() const override { return QStringLiteral("Cylinder"); }
    QVariantMap defaults() const override;
    QStringList expressionKeys() const override { return {"diameter", "height"}; }

protected:
    bool compute(RebuildContext &ctx) override;
};