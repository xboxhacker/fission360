// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#pragma once
#include "FissionFeature.h"

// Construction plane. props.mode:
//   "offset"      : base (plane ref), offset (expr)
//   "angle"       : base (plane ref), axis (axis ref), angle (expr, deg)
//   "midplane"    : base, base2 (plane refs)
//   "threePoints" : p1, p2, p3 ([x,y,z] exprs or point refs)
// props.display: [umin, umax, vmin, vmax] extent of the drawn rectangle in plane coordinates
//   (missing = a square sized to the model). Display only; dragging an edge in the view sets it.
class FissionConstructionPlane : public FissionFeature {
    Q_OBJECT
public:
    explicit FissionConstructionPlane(QObject *parent = nullptr) : FissionFeature(parent) {}
    QString type() const override { return QStringLiteral("ConstructionPlane"); }
    QVariantMap defaults() const override;
    QStringList expressionKeys() const override { return {"offset", "angle"}; }
    bool isConstruction() const override { return true; }
    const gp_Ax3 &plane() const { return m_plane; }
    TopoDS_Shape overlayShape() const override { return m_display; }
    // {origin, x, y (vector3d), bounds: [umin, umax, vmin, vmax]} of the drawn rectangle; empty when unhealthy
    Q_INVOKABLE QVariantMap displayInfo() const;

protected:
    bool compute(RebuildContext &ctx) override;

private:
    gp_Ax3 m_plane;
    TopoDS_Shape m_display;
    double m_bounds[4] = {0, 0, 0, 0};
};

// Construction axis. props.mode:
//   "twoPoints" : p1, p2       "edge" : edge (edge ref)
//   "normal"    : base (plane ref), point ([x,y,z] in world)
class FissionConstructionAxis : public FissionFeature {
    Q_OBJECT
public:
    explicit FissionConstructionAxis(QObject *parent = nullptr) : FissionFeature(parent) {}
    QString type() const override { return QStringLiteral("ConstructionAxis"); }
    QVariantMap defaults() const override;
    bool isConstruction() const override { return true; }
    const gp_Ax1 &axis() const { return m_axis; }
    TopoDS_Shape overlayShape() const override { return m_display; }

protected:
    bool compute(RebuildContext &ctx) override;

private:
    gp_Ax1 m_axis;
    TopoDS_Shape m_display;
};

// Construction point. props: x, y, z (exprs)
class FissionConstructionPoint : public FissionFeature {
    Q_OBJECT
public:
    explicit FissionConstructionPoint(QObject *parent = nullptr) : FissionFeature(parent) {}
    QString type() const override { return QStringLiteral("ConstructionPoint"); }
    QVariantMap defaults() const override { return {{"x", "0"}, {"y", "0"}, {"z", "0"}}; }
    QStringList expressionKeys() const override { return {"x", "y", "z"}; }
    bool isConstruction() const override { return true; }
    const gp_Pnt &point() const { return m_point; }
    TopoDS_Shape overlayShape() const override { return m_display; }

protected:
    bool compute(RebuildContext &ctx) override;

private:
    gp_Pnt m_point;
    TopoDS_Shape m_display;
};
