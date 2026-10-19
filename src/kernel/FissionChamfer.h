// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#pragma once
#include "FissionFeature.h"

// props:
//   edges, body, allEdges : as Fillet
//   chamferType : "equal" | "twoDistances" | "distanceAngle"
//   distance, distance2 : expr     angle : expr (deg)
class FissionChamfer : public FissionFeature {
    Q_OBJECT
public:
    explicit FissionChamfer(QObject *parent = nullptr) : FissionFeature(parent) {}
    QString type() const override { return QStringLiteral("Chamfer"); }
    QVariantMap defaults() const override {
        return {{"edges", QVariantList()}, {"body", 0}, {"allEdges", false}, {"tangentChain", true}, {"chamferType", "equal"},
                {"distance", "1 mm"}, {"distance2", "2 mm"}, {"angle", "45 deg"}};
    }
    QStringList expressionKeys() const override { return {"distance", "distance2", "angle"}; }

protected:
    bool compute(RebuildContext &ctx) override;
};
