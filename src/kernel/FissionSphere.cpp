// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#include "FissionSphere.h"
#include "FissionSolidTools.h"
#include <BRepPrimAPI_MakeSphere.hxx>

QVariantMap FissionSphere::defaults() const {
    return {{"diameter", "50"},
            {"plane", ""}, {"origin", QVariantList{0.0, 0.0, 0.0}},
            {"operation", "newBody"}, {"targets", QVariantList()}};
}

bool FissionSphere::compute(RebuildContext &ctx) {
    double dia;
    if (!number(ctx, "diameter", dia)) return false;
    if (dia <= 0) return fail("Sphere diameter must be greater than zero");

    gp_Ax3 frame, base;
    QString err;
    if (!primitiveFrame(ctx, get("plane"), list("origin"), frame, err, &base)) return fail(err);

    TopoDS_Shape sphere = placeOnFrame(BRepPrimAPI_MakeSphere(dia / 2.0).Shape(), frame);
    if (sphere.IsNull()) return fail("Sphere failed");
    setHandles(base, {{frame.Location(), frame.XDirection(), "diameter", dia, 0.5}});
    return ctx.applyOperation(sphere, operation(), list("targets"), id(), err) || fail(err);
}