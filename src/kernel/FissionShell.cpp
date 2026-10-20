// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#include "FissionShell.h"
#include "GeomUtils.h"
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepBuilderAPI_MakeSolid.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepLib.hxx>
#include <BRepOffsetAPI_MakeOffsetShape.hxx>
#include <BRepOffsetAPI_MakeThickSolid.hxx>
#include <Standard_Failure.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Solid.hxx>

namespace {

bool usableSolid(const TopoDS_Shape &s, double maxVolume) {
    if (s.IsNull() || GeomUtils::solids(s).isEmpty()) return false;
    if (!BRepCheck_Analyzer(s).IsValid()) return false;
    const double v = GeomUtils::volume(s);
    return v > 1e-9 && v < maxVolume - 1e-9;
}

// Default mode (arc joins) handles curved bodies; complete-intersection mode handles planar bodies where
// faces shrink away under the offset (narrow ridges / slivers) or more than three faces meet at a vertex.
TopoDS_Shape thickSolid(const TopoDS_Shape &solid, const TopTools_ListOfShape &openings, double offset,
                        double maxVolume) {
    struct Attempt { double tol; bool complete; };
    for (const Attempt &a : {Attempt{1e-4, false}, Attempt{1e-7, true}, Attempt{1e-4, true}}) {
        try {
            BRepOffsetAPI_MakeThickSolid thick;
            thick.MakeThickSolidByJoin(solid, openings, offset, a.tol, BRepOffset_Skin,
                                       a.complete ? Standard_True : Standard_False, Standard_False,
                                       a.complete ? GeomAbs_Intersection : GeomAbs_Arc,
                                       a.complete ? Standard_True : Standard_False);
            thick.Build();
            if (thick.IsDone() && usableSolid(thick.Shape(), maxVolume)) return thick.Shape();
        } catch (const Standard_Failure &) {
        }
    }
    return {};
}

TopoDS_Shape offsetSolid(const TopoDS_Shape &solid, double offset) {
    struct Attempt { double tol; bool complete; };
    for (const Attempt &a : {Attempt{1e-4, false}, Attempt{1e-7, true}}) {
        try {
            BRepOffsetAPI_MakeOffsetShape off;
            off.PerformByJoin(solid, offset, a.tol, BRepOffset_Skin, a.complete ? Standard_True : Standard_False,
                              Standard_False, a.complete ? GeomAbs_Intersection : GeomAbs_Arc,
                              a.complete ? Standard_True : Standard_False);
            if (!off.IsDone()) continue;
            for (TopExp_Explorer exp(off.Shape(), TopAbs_SHELL); exp.More(); exp.Next()) {
                TopoDS_Solid inner = BRepBuilderAPI_MakeSolid(TopoDS::Shell(exp.Current())).Solid();
                BRepLib::OrientClosedSolid(inner);
                if (!inner.IsNull()) return inner;
            }
        } catch (const Standard_Failure &) {
        }
    }
    return {};
}

} // namespace

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
    const double v0 = GeomUtils::volume(solid);
    // Outside shells add material; only inside shells must end up smaller than the body
    const double maxVolume = offset < 0 ? v0 : 1e300;

    if (!openings.IsEmpty()) {
        const TopoDS_Shape result = thickSolid(solid, openings, offset, maxVolume);
        if (result.IsNull()) return fail("Shell failed: the walls could not be offset (try a thinner wall)");
        ctx.bodies[body].shape = result;
        return true;
    }

    // No openings: hollow out with an offset copy of the solid
    const TopoDS_Shape inner = offsetSolid(solid, offset);
    if (inner.IsNull()) return fail("Shell failed: the walls could not be offset (try a thinner wall)");
    try {
        if (offset < 0) {
            BRepAlgoAPI_Cut cut(solid, inner);
            if (!cut.IsDone()) return fail("Shell failed");
            ctx.bodies[body].shape = cut.Shape();
        } else {
            BRepAlgoAPI_Cut cut(inner, solid);
            if (!cut.IsDone()) return fail("Shell failed");
            ctx.bodies[body].shape = cut.Shape();
        }
    } catch (const Standard_Failure &) {
        return fail("Shell failed");
    }
    return true;
}
