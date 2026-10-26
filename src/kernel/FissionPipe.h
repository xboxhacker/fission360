// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#pragma once
#include "FissionFeature.h"

// props:
//   innerDiameter        : expr (mm) — bore
//   outerDiameter        : expr (mm) — outer
//   length               : expr (mm)
//   origin               : [x, y, z]
//   operation, targets   : as Extrude
class FissionPipe : public FissionFeature {
    Q_OBJECT
public:
    explicit FissionPipe(QObject *parent = nullptr) : FissionFeature(parent) {}
    QString type() const override { return QStringLiteral("Pipe"); }
    QVariantMap defaults() const override;
    QStringList expressionKeys() const override { return {"innerDiameter", "outerDiameter", "length"}; }

protected:
    bool compute(RebuildContext &ctx) override;
};