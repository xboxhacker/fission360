// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#include "FissionBox.h"
#include <BRepPrimAPI_MakeBox.hxx>

QVariantMap FissionBox::defaults() const {
    return {{"width", "50"}, {"length", "50"}, {"depth", "50"},
            {"origin", QVariantList{0.0, 0.0, 0.0}},
            {"operation", "join"}, {"targets", QVariantList()}};
}

bool FissionBox::compute(RebuildContext &ctx) {
    double w, l, d;
    if (!number(ctx, "width", w)) return false;
    if (!number(ctx, "length", l)) return false;
    if (!number(ctx, "depth", d)) return false;
    if (w <= 0 || l <= 0 || d <= 0) return fail("Box dimensions must be greater than zero");

    const QVariantList o = list("origin");
    const gp_Pnt origin(o.size() >= 3 ? o.at(0).toDouble() : 0.0,
                        o.size() >= 3 ? o.at(1).toDouble() : 0.0,
                        o.size() >= 3 ? o.at(2).toDouble() : 0.0);

    BRepPrimAPI_MakeBox box(origin, w, l, d);
    box.Build();
    if (!box.IsDone()) return fail("Box failed");

    QString err;
    return ctx.applyOperation(box.Shape(), operation(), list("targets"), id(), err) || fail(err);
}