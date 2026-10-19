// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#pragma once
#include "FissionFeature.h"

// props:
//   sections  : in order, each either
//               {kind:"profile", sketch: id, profile: index (-1 = first default profile)}
//               or a face ref {kind:"face", body, index, x, y, z} (the face's whole outer boundary)
//   solid     : bool     ruled : bool (no guide only)
//   guide     : "none" | "rail" | "centerline"
//   rail      : sketch id whose curves form one chain touching every section's outline (the sections turn
//               and scale to keep touching it; the spine runs through the section centres)
//   centerline: sketch id whose curves form one chain through every section (the sections follow it)
//   operation, targets : as Extrude
class FissionLoft : public FissionFeature {
    Q_OBJECT
public:
    explicit FissionLoft(QObject *parent = nullptr) : FissionFeature(parent) {}
    QString type() const override { return QStringLiteral("Loft"); }
    QVariantMap defaults() const override {
        return {{"sections", QVariantList()}, {"solid", true}, {"ruled", false}, {"guide", "none"},
                {"rail", ""}, {"centerline", ""}, {"operation", "join"}, {"targets", QVariantList()}};
    }

protected:
    bool compute(RebuildContext &ctx) override;
};
