// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#include "FissionBox.h"
#include "FissionSolidTools.h"
#include <BRepPrimAPI_MakeBox.hxx>

QVariantMap FissionBox::defaults() const {
    return {{"width", "50"}, {"length", "50"}, {"depth", "50"},
            {"plane", ""}, {"origin", QVariantList{0.0, 0.0, 0.0}},
            {"operation", "newBody"}, {"targets", QVariantList()}};
}

bool FissionBox::compute(RebuildContext &ctx) {
    double w, l, d;
    if (!number(ctx, "width", w)) return false;
    if (!number(ctx, "length", l)) return false;
    if (!number(ctx, "depth", d)) return false;
    if (w <= 0 || l <= 0 || d <= 0) return fail("Box dimensions must be greater than zero");

    gp_Ax3 frame, base;
    QString err;
    if (!primitiveFrame(ctx, get("plane"), list("origin"), frame, err, &base)) return fail(err);

    BRepPrimAPI_MakeBox box(w, l, d);
    box.Build();
    if (!box.IsDone()) return fail("Box failed");
    const TopoDS_Shape placed = placeOnFrame(box.Shape(), frame);
    if (placed.IsNull()) return fail("Box placement failed");
    setHandles(base, {{frame.Location(), frame.XDirection(), "width", w},
                      {frame.Location(), frame.YDirection(), "length", l},
                      {frame.Location(), frame.Direction(), "depth", d}});
    return ctx.applyOperation(placed, operation(), list("targets"), id(), err) || fail(err);
}