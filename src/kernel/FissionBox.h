// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#pragma once
#include "FissionFeature.h"

// props:
//   width, length, depth : expr (mm)
//   origin               : [x, y, z]
//   operation, targets   : as Extrude
class FissionBox : public FissionFeature {
    Q_OBJECT
public:
    explicit FissionBox(QObject *parent = nullptr) : FissionFeature(parent) {}
    QString type() const override { return QStringLiteral("Box"); }
    QVariantMap defaults() const override;
    QStringList expressionKeys() const override { return {"width", "length", "depth"}; }

protected:
    bool compute(RebuildContext &ctx) override;
};