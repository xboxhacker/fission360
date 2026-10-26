// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#include "FissionTorus.h"
#include <BRepPrimAPI_MakeTorus.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <gp_Trsf.hxx>

QVariantMap FissionTorus::defaults() const {
    return {{"tubeDiameter", "10"}, {"innerDiameter", "40"},
            {"origin", QVariantList{0.0, 0.0, 0.0}},
            {"operation", "join"}, {"targets", QVariantList()}};
}

bool FissionTorus::compute(RebuildContext &ctx) {
    double tube, inner;
    if (!number(ctx, "tubeDiameter", tube)) return false;
    if (!number(ctx, "innerDiameter", inner)) return false;
    if (tube <= 0 || inner <= 0) return fail("Torus diameters must be greater than zero");

    const QVariantList o = list("origin");
    const gp_Vec v(o.size() >= 3 ? o.at(0).toDouble() : 0.0,
                   o.size() >= 3 ? o.at(1).toDouble() : 0.0,
                   o.size() >= 3 ? o.at(2).toDouble() : 0.0);

    // innerDiameter = hole → major radius = (inner + tube) / 2
    const double rMajor = (inner + tube) / 2.0;
    const double rTube  = tube / 2.0;

    TopoDS_Shape torus = BRepPrimAPI_MakeTorus(rMajor, rTube).Shape();
    gp_Trsf trsf;
    trsf.SetTranslation(v);
    torus = BRepBuilderAPI_Transform(torus, trsf).Shape();
    if (torus.IsNull()) return fail("Torus failed");

    QString err;
    return ctx.applyOperation(torus, operation(), list("targets"), id(), err) || fail(err);
}