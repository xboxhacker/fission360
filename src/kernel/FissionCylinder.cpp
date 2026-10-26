// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#include "FissionCylinder.h"
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <gp_Trsf.hxx>

QVariantMap FissionCylinder::defaults() const {
    return {{"diameter", "50"}, {"height", "50"},
            {"origin", QVariantList{0.0, 0.0, 0.0}},
            {"operation", "join"}, {"targets", QVariantList()}};
}

bool FissionCylinder::compute(RebuildContext &ctx) {
    double dia, h;
    if (!number(ctx, "diameter", dia)) return false;
    if (!number(ctx, "height", h)) return false;
    if (dia <= 0 || h <= 0) return fail("Cylinder diameter and height must be greater than zero");

    const QVariantList o = list("origin");
    const gp_Vec v(o.size() >= 3 ? o.at(0).toDouble() : 0.0,
                   o.size() >= 3 ? o.at(1).toDouble() : 0.0,
                   o.size() >= 3 ? o.at(2).toDouble() : 0.0);

    TopoDS_Shape cyl = BRepPrimAPI_MakeCylinder(h, dia / 2.0).Shape();
    gp_Trsf trsf;
    trsf.SetTranslation(v);
    cyl = BRepBuilderAPI_Transform(cyl, trsf).Shape();
    if (cyl.IsNull()) return fail("Cylinder failed");

    QString err;
    return ctx.applyOperation(cyl, operation(), list("targets"), id(), err) || fail(err);
}