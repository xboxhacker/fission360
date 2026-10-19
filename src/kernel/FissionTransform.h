// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#pragma once
#include "FissionFeature.h"
#include <gp_Trsf.hxx>

// Copies bodies — or patterns selected faces as cutters — with a set of transforms. props:
//   bodies    : [body indices] (empty = all bodies); ignored when faces is non-empty
//   faces     : [face refs] — build a hole/pocket tool from the faces, pattern it, Cut (default) / Join / New Body
//   operation : "join" | "cut" | "newBody"  (face patterns default to cut)
class FissionBodyCopyFeature : public FissionFeature {
    Q_OBJECT
public:
    using FissionFeature::FissionFeature;

protected:
    bool compute(RebuildContext &ctx) override;
    virtual bool transforms(RebuildContext &ctx, QList<gp_Trsf> &out) = 0;
};

// props: plane (plane ref)
class FissionMirror : public FissionBodyCopyFeature {
    Q_OBJECT
public:
    explicit FissionMirror(QObject *parent = nullptr) : FissionBodyCopyFeature(parent) {}
    QString type() const override { return QStringLiteral("Mirror"); }
    QVariantMap defaults() const override {
        return {{"bodies", QVariantList()}, {"plane", "YZ"}, {"operation", "join"}};
    }

protected:
    bool transforms(RebuildContext &ctx, QList<gp_Trsf> &out) override;
};

// props: direction1 (axis ref), count1, spacing1, direction2 (axis ref or ""), count2, spacing2
class FissionRectangularPattern : public FissionBodyCopyFeature {
    Q_OBJECT
public:
    explicit FissionRectangularPattern(QObject *parent = nullptr) : FissionBodyCopyFeature(parent) {}
    QString type() const override { return QStringLiteral("RectangularPattern"); }
    QVariantMap defaults() const override {
        return {{"bodies", QVariantList()}, {"faces", QVariantList()}, {"operation", "newBody"},
                {"direction1", "X"}, {"count1", "3"}, {"spacing1", "20 mm"},
                {"direction2", ""}, {"count2", "1"}, {"spacing2", "20 mm"}};
    }
    QStringList expressionKeys() const override { return {"count1", "spacing1", "count2", "spacing2"}; }

protected:
    bool transforms(RebuildContext &ctx, QList<gp_Trsf> &out) override;
};

// props: axis (axis ref), count, angle (total, deg; 360 = evenly around)
class FissionCircularPattern : public FissionBodyCopyFeature {
    Q_OBJECT
public:
    explicit FissionCircularPattern(QObject *parent = nullptr) : FissionBodyCopyFeature(parent) {}
    QString type() const override { return QStringLiteral("CircularPattern"); }
    QVariantMap defaults() const override {
        return {{"bodies", QVariantList()}, {"faces", QVariantList()}, {"operation", "newBody"},
                {"axis", "Z"}, {"count", "6"}, {"angle", "360 deg"}};
    }
    QStringList expressionKeys() const override { return {"count", "angle"}; }

protected:
    bool transforms(RebuildContext &ctx, QList<gp_Trsf> &out) override;
};
