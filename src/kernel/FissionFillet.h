// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#pragma once
#include "FissionFeature.h"

// props:
//   edges  : [edge refs]           radius : expr
//   radiusType : "constant" | "variable"   radius2 : expr (end radius of a variable fillet; the radius
//                changes linearly from radius to radius2 along each tangent chain)
//   body   : body index used with allEdges
//   allEdges : bool (fillet every edge of `body`)
class FissionFillet : public FissionFeature {
    Q_OBJECT
public:
    explicit FissionFillet(QObject *parent = nullptr) : FissionFeature(parent) {}
    QString type() const override { return QStringLiteral("Fillet"); }
    QVariantMap defaults() const override {
        return {{"edges", QVariantList()}, {"radius", "2 mm"}, {"radiusType", "constant"}, {"radius2", "4 mm"},
                {"body", 0}, {"allEdges", false}, {"tangentChain", true}};
    }
    QStringList expressionKeys() const override { return {"radius", "radius2"}; }

protected:
    bool compute(RebuildContext &ctx) override;
};
