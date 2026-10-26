// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#include "FissionCylinder.h"
#include "FissionSolidTools.h"
#include <BRepPrimAPI_MakeCylinder.hxx>

QVariantMap FissionCylinder::defaults() const {
    return {{"diameter", "50"}, {"height", "50"},
            {"plane", ""}, {"origin", QVariantList{0.0, 0.0, 0.0}},
            {"operation", "newBody"}, {"targets", QVariantList()}};
}

bool FissionCylinder::compute(RebuildContext &ctx) {
    double dia, h;
    if (!number(ctx, "diameter", dia)) return false;
    if (!number(ctx, "height", h)) return false;
    if (dia <= 0 || h <= 0) return fail("Cylinder diameter and height must be greater than zero");

    gp_Ax3 frame, base;
    QString err;
    if (!primitiveFrame(ctx, get("plane"), list("origin"), frame, err, &base)) return fail(err);

    // Radius, then height. The axis is the plane normal.
    TopoDS_Shape cyl = placeOnFrame(BRepPrimAPI_MakeCylinder(dia / 2.0, h).Shape(), frame);
    if (cyl.IsNull()) return fail("Cylinder failed");
    setHandles(base, {{frame.Location(), frame.XDirection(), "diameter", dia, 0.5},
                      {frame.Location(), frame.Direction(), "height", h}});
    return ctx.applyOperation(cyl, operation(), list("targets"), id(), err) || fail(err);
}