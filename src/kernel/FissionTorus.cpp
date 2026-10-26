// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#include "FissionTorus.h"
#include "FissionSolidTools.h"
#include <BRepPrimAPI_MakeTorus.hxx>

QVariantMap FissionTorus::defaults() const {
    return {{"tubeDiameter", "10"}, {"innerDiameter", "40"},
            {"plane", ""}, {"origin", QVariantList{0.0, 0.0, 0.0}},
            {"operation", "newBody"}, {"targets", QVariantList()}};
}

bool FissionTorus::compute(RebuildContext &ctx) {
    double tube, inner;
    if (!number(ctx, "tubeDiameter", tube)) return false;
    if (!number(ctx, "innerDiameter", inner)) return false;
    if (tube <= 0 || inner <= 0) return fail("Torus diameters must be greater than zero");

    gp_Ax3 frame, base;
    QString err;
    if (!primitiveFrame(ctx, get("plane"), list("origin"), frame, err, &base)) return fail(err);

    // innerDiameter = hole → major radius = (inner + tube) / 2. The hole axis is the plane normal.
    const double rMajor = (inner + tube) / 2.0;
    const double rTube  = tube / 2.0;
    TopoDS_Shape torus = placeOnFrame(BRepPrimAPI_MakeTorus(rMajor, rTube).Shape(), frame);
    if (torus.IsNull()) return fail("Torus failed");
    const gp_Pnt tubeCenter = frame.Location().Translated(gp_Vec(frame.XDirection()) * rMajor);
    setHandles(base, {{frame.Location(), frame.XDirection(), "innerDiameter", inner, 0.5},
                      {tubeCenter, frame.XDirection(), "tubeDiameter", tube, 0.5}});
    return ctx.applyOperation(torus, operation(), list("targets"), id(), err) || fail(err);
}