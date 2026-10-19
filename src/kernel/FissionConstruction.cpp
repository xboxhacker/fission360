// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#include "FissionConstruction.h"
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakeVertex.hxx>
#include <gp_Pln.hxx>
#include <QVector3D>
#include <algorithm>

QVariantMap FissionConstructionPlane::defaults() const {
    return {{"mode", "offset"}, {"base", "XY"}, {"base2", "XY"}, {"offset", "10 mm"},
            {"axis", "X"}, {"angle", "45 deg"},
            {"p1", QVariantList{"0", "0", "0"}}, {"p2", QVariantList{"10", "0", "0"}},
            {"p3", QVariantList{"0", "10", "0"}}};
}

bool FissionConstructionPlane::compute(RebuildContext &ctx) {
    QString err;
    const QString mode = str("mode", "offset");
    if (mode == "offset") {
        gp_Ax3 base;
        double offset = 0;
        if (!ctx.resolvePlane(get("base"), base, err)) return fail(err);
        if (!number(ctx, "offset", offset)) return false;
        m_plane = base.Translated(gp_Vec(base.Direction()) * offset);
    } else if (mode == "angle") {
        gp_Ax3 base;
        gp_Ax1 axis;
        double angle = 0;
        if (!ctx.resolvePlane(get("base"), base, err)) return fail(err);
        if (!ctx.resolveAxis(get("axis"), axis, err)) return fail(err);
        if (!number(ctx, "angle", angle)) return false;
        m_plane = base.Rotated(axis, angle * M_PI / 180.0);
    } else if (mode == "midplane") {
        gp_Ax3 a, b;
        if (!ctx.resolvePlane(get("base"), a, err) || !ctx.resolvePlane(get("base2"), b, err)) return fail(err);
        if (!a.Direction().IsParallel(b.Direction(), 1e-6)) return fail("Midplane needs two parallel planes");
        const gp_Vec n(a.Direction());
        const double d = gp_Vec(a.Location(), b.Location()).Dot(n);
        m_plane = a.Translated(n * (d / 2));
    } else if (mode == "threePoints") {
        gp_Pnt p1, p2, p3;
        if (!ctx.resolvePoint(get("p1"), p1, err) || !ctx.resolvePoint(get("p2"), p2, err)
            || !ctx.resolvePoint(get("p3"), p3, err))
            return fail(err);
        const gp_Vec u(p1, p2), v(p1, p3);
        const gp_Vec n = u.Crossed(v);
        if (n.Magnitude() < 1e-9) return fail("Points are collinear");
        m_plane = gp_Ax3(p1, gp_Dir(n), gp_Dir(u));
    } else {
        return fail("Unknown plane mode");
    }
    const double s = ctx.modelSize() * 0.3;
    double b[4] = {-s, s, -s, s};
    const QVariantList d = list("display");
    if (d.size() == 4) {
        for (int i = 0; i < 4; ++i) b[i] = d[i].toDouble();
        if (b[1] - b[0] < 1e-3 || b[3] - b[2] < 1e-3) { b[0] = -s; b[1] = s; b[2] = -s; b[3] = s; }
    }
    std::copy(b, b + 4, m_bounds);
    m_display = BRepBuilderAPI_MakeFace(gp_Pln(m_plane), b[0], b[1], b[2], b[3]).Face();
    return true;
}

QVariantMap FissionConstructionPlane::displayInfo() const {
    if (!isHealthy() || m_display.IsNull()) return {};
    const gp_Pnt o = m_plane.Location();
    const gp_Dir x = m_plane.XDirection(), y = m_plane.YDirection();
    return {{"origin", QVector3D(o.X(), o.Y(), o.Z())},
            {"x", QVector3D(x.X(), x.Y(), x.Z())},
            {"y", QVector3D(y.X(), y.Y(), y.Z())},
            {"bounds", QVariantList{m_bounds[0], m_bounds[1], m_bounds[2], m_bounds[3]}}};
}

QVariantMap FissionConstructionAxis::defaults() const {
    return {{"mode", "twoPoints"}, {"p1", QVariantList{"0", "0", "0"}}, {"p2", QVariantList{"0", "0", "10"}},
            {"edge", QVariant()}, {"base", "XY"}, {"point", QVariantList{"0", "0", "0"}}};
}

bool FissionConstructionAxis::compute(RebuildContext &ctx) {
    QString err;
    const QString mode = str("mode", "twoPoints");
    if (mode == "twoPoints") {
        gp_Pnt a, b;
        if (!ctx.resolvePoint(get("p1"), a, err) || !ctx.resolvePoint(get("p2"), b, err)) return fail(err);
        if (a.Distance(b) < 1e-7) return fail("Points coincide");
        m_axis = gp_Ax1(a, gp_Dir(gp_Vec(a, b)));
    } else if (mode == "edge") {
        if (!ctx.resolveAxis(get("edge"), m_axis, err)) return fail(err);
    } else if (mode == "normal") {
        gp_Ax3 base;
        gp_Pnt p;
        if (!ctx.resolvePlane(get("base"), base, err) || !ctx.resolvePoint(get("point"), p, err)) return fail(err);
        m_axis = gp_Ax1(p, base.Direction());
    } else {
        return fail("Unknown axis mode");
    }
    const double len = ctx.modelSize() * 0.5;
    const gp_Vec d(m_axis.Direction());
    m_display = BRepBuilderAPI_MakeEdge(m_axis.Location().Translated(-d * len),
                                        m_axis.Location().Translated(d * len)).Edge();
    return true;
}

bool FissionConstructionPoint::compute(RebuildContext &ctx) {
    double x = 0, y = 0, z = 0;
    if (!number(ctx, "x", x) || !number(ctx, "y", y) || !number(ctx, "z", z)) return false;
    m_point = gp_Pnt(x, y, z);
    m_display = BRepBuilderAPI_MakeVertex(m_point).Vertex();
    return true;
}
