// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#include "FissionShell.h"
#include "GeomUtils.h"
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepBuilderAPI_MakeSolid.hxx>
#include <BRepLib.hxx>
#include <BRepOffsetAPI_MakeOffsetShape.hxx>
#include <BRepOffsetAPI_MakeThickSolid.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Solid.hxx>

bool FissionShell::compute(RebuildContext &ctx) {
    double t = 0;
    if (!number(ctx, "thickness", t)) return false;
    if (t <= 0) return fail("Thickness must be positive");
    const double offset = str("direction", "inside") == "outside" ? t : -t;

    QString err;
    const QVariantList refs = list("faces");
    int body = integer("body");
    TopTools_ListOfShape openings;
    for (const QVariant &ref : refs) {
        int b = -1;
        TopoDS_Face f;
        if (!ctx.resolveFace(ref, b, f, err)) return fail(err);
        if (!openings.IsEmpty() && b != body) return fail("All shell faces must belong to the same body");
        body = b;
        openings.Append(f);
    }
    if (body < 0 || body >= ctx.bodies.size()) return fail("Body no longer exists");
    const TopoDS_Shape &solid = ctx.bodies[body].shape;

    if (!openings.IsEmpty()) {
        BRepOffsetAPI_MakeThickSolid thick;
        thick.MakeThickSolidByJoin(solid, openings, offset, 1e-4);
        thick.Build();
        if (!thick.IsDone()) return fail("Shell failed (thickness may be too large)");
        ctx.bodies[body].shape = thick.Shape();
        return true;
    }

    // No openings: hollow out with an offset copy of the solid
    BRepOffsetAPI_MakeOffsetShape off;
    off.PerformByJoin(solid, offset, 1e-4);
    if (!off.IsDone()) return fail("Shell failed (thickness may be too large)");
    TopoDS_Solid inner;
    for (TopExp_Explorer exp(off.Shape(), TopAbs_SHELL); exp.More(); exp.Next()) {
        inner = BRepBuilderAPI_MakeSolid(TopoDS::Shell(exp.Current())).Solid();
        BRepLib::OrientClosedSolid(inner);
        break;
    }
    if (inner.IsNull()) return fail("Shell failed");
    if (offset < 0) {
        BRepAlgoAPI_Cut cut(solid, inner);
        if (!cut.IsDone()) return fail("Shell failed");
        ctx.bodies[body].shape = cut.Shape();
    } else {
        BRepAlgoAPI_Cut cut(inner, solid);
        if (!cut.IsDone()) return fail("Shell failed");
        ctx.bodies[body].shape = cut.Shape();
    }
    return true;
}
