// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#pragma once
#include "FissionFeature.h"

// props:
//   sketch, profiles      : as Extrude (sketch profiles or flat body faces)
//   axis                  : axis reference ("X", "sketchline:<id>:<n>", construction axis, edge)
//   extent                : "angle" | "full" | "symmetric"
//   angle                 : expr (deg)   flip : bool
//   operation, targets    : as Extrude
class FissionRevolve : public FissionFeature {
    Q_OBJECT
public:
    explicit FissionRevolve(QObject *parent = nullptr) : FissionFeature(parent) {}
    QString type() const override { return QStringLiteral("Revolve"); }
    QVariantMap defaults() const override;
    QStringList expressionKeys() const override { return {"angle"}; }

protected:
    bool compute(RebuildContext &ctx) override;
};
