// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#include "FissionPipe.h"
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <gp_Trsf.hxx>

QVariantMap FissionPipe::defaults() const {
    return {{"innerDiameter", "20"}, {"outerDiameter", "30"}, {"length", "50"},
            {"origin", QVariantList{0.0, 0.0, 0.0}},
            {"operation", "join"}, {"targets", QVariantList()}};
}

bool FissionPipe::compute(RebuildContext &ctx) {
    double in, out, len;
    if (!number(ctx, "innerDiameter", in)) return false;
    if (!number(ctx, "outerDiameter", out)) return false;
    if (!number(ctx, "length", len)) return false;
    if (len <= 0 || out <= 0) return fail("Pipe outer diameter and length must be positive");
    if (in >= out) return fail("Pipe inner diameter must be smaller than outer diameter");
    if (in < 0) return fail("Pipe inner diameter must be zero or positive");

    const QVariantList o = list("origin");
    const gp_Vec v(o.size() >= 3 ? o.at(0).toDouble() : 0.0,
                   o.size() >= 3 ? o.at(1).toDouble() : 0.0,
                   o.size() >= 3 ? o.at(2).toDouble() : 0.0);
    gp_Trsf trsf;
    trsf.SetTranslation(v);

    TopoDS_Shape outer = BRepBuilderAPI_Transform(BRepPrimAPI_MakeCylinder(len, out / 2.0).Shape(), trsf).Shape();
    TopoDS_Shape inner = in > 0
        ? BRepBuilderAPI_Transform(BRepPrimAPI_MakeCylinder(len, in / 2.0).Shape(), trsf).Shape()
        : TopoDS_Shape();
    TopoDS_Shape pipe = inner.IsNull() ? outer : BRepAlgoAPI_Cut(outer, inner).Shape();
    if (pipe.IsNull()) return fail("Pipe cut failed");

    QString err;
    return ctx.applyOperation(pipe, operation(), list("targets"), id(), err) || fail(err);
}