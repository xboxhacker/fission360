// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#include "FissionSphere.h"
#include <BRepPrimAPI_MakeSphere.hxx>

QVariantMap FissionSphere::defaults() const {
    return {{"diameter", "50"},
            {"origin", QVariantList{0.0, 0.0, 0.0}},
            {"operation", "join"}, {"targets", QVariantList()}};
}

bool FissionSphere::compute(RebuildContext &ctx) {
    double dia;
    if (!number(ctx, "diameter", dia)) return false;
    if (dia <= 0) return fail("Sphere diameter must be greater than zero");

    const QVariantList o = list("origin");
    gp_Pnt center(o.size() >= 3 ? o.at(0).toDouble() : 0.0,
                  o.size() >= 3 ? o.at(1).toDouble() : 0.0,
                  o.size() >= 3 ? o.at(2).toDouble() : 0.0);

    TopoDS_Shape sphere = BRepPrimAPI_MakeSphere(center, dia / 2.0).Shape();
    if (sphere.IsNull()) return fail("Sphere failed");

    QString err;
    return ctx.applyOperation(sphere, operation(), list("targets"), id(), err) || fail(err);
}