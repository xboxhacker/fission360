// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#pragma once
#include "FissionFeature.h"

// props:
//   sketch, profiles   : profile to sweep (as Extrude: sketch profiles or flat body faces)
//   path               : sketch id whose curves form a single connected chain
//   orientation        : "perpendicular" (corrected Frenet) | "parallel" (fixed)
//   twist              : expr (deg) the profile turns about the path from start to end; non-zero ignores orientation
//   operation, targets : as Extrude
class FissionSweep : public FissionFeature {
    Q_OBJECT
public:
    explicit FissionSweep(QObject *parent = nullptr) : FissionFeature(parent) {}
    QString type() const override { return QStringLiteral("Sweep"); }
    QVariantMap defaults() const override {
        return {{"sketch", ""}, {"profiles", QVariantList()}, {"path", ""}, {"orientation", "perpendicular"},
                {"twist", "0 deg"}, {"operation", "join"}, {"targets", QVariantList()}};
    }
    QStringList expressionKeys() const override { return {"twist"}; }

protected:
    bool compute(RebuildContext &ctx) override;
};
