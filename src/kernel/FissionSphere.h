// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#pragma once
#include "FissionFeature.h"

// props:
//   diameter             : expr (mm)
//   origin               : [x, y, z]
//   operation, targets   : as Extrude
class FissionSphere : public FissionFeature {
    Q_OBJECT
public:
    explicit FissionSphere(QObject *parent = nullptr) : FissionFeature(parent) {}
    QString type() const override { return QStringLiteral("Sphere"); }
    QVariantMap defaults() const override;
    QStringList expressionKeys() const override { return {"diameter"}; }

protected:
    bool compute(RebuildContext &ctx) override;
};