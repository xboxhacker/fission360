// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#include "FissionShell.h"
#include "GeomUtils.h"
#include <BRepAdaptor_Surface.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepAlgoAPI_Fuse.hxx>
#include <BRepBuilderAPI_MakeSolid.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepLib.hxx>
#include <BRepOffsetAPI_MakeOffsetShape.hxx>
#include <BRepOffsetAPI_MakeThickSolid.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <BRepTools.hxx>
#include <ShapeFix_Shape.hxx>
#include <Standard_Failure.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_ListIteratorOfShape.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Solid.hxx>

namespace {

gp_Dir outwardNormal(const TopoDS_Face &f) {
    BRepAdaptor_Surface s(f);
    double u0, u1, v0, v1;
    BRepTools::UVBounds(f, u0, u1, v0, v1);
    gp_Pnt p;
    gp_Vec du, dv;
    s.D1((u0 + u1) / 2, (v0 + v1) / 2, p, du, dv);
    const gp_Vec n = du.Crossed(dv);
    if (n.Magnitude() < 1e-12) return gp_Dir(0, 0, 1);
    gp_Dir d(n);
    if (f.Orientation() == TopAbs_REVERSED) d.Reverse();
    return d;
}

TopoDS_Shape tryFix(const TopoDS_Shape &s) {
    if (s.IsNull()) return {};
    try {
        ShapeFix_Shape fix(s);
        fix.Perform();
        return fix.Shape().IsNull() ? s : fix.Shape();
    } catch (const Standard_Failure &) {
        return s;
    }
}

bool usableSolid(const TopoDS_Shape &s, double maxVolume, bool requireCheck) {
    if (s.IsNull() || GeomUtils::solids(s).isEmpty()) return false;
    const double v = GeomUtils::volume(s);
    if (!(v > 1e-9 && v < maxVolume - 1e-9)) return false;
    if (!requireCheck) return true;
    return BRepCheck_Analyzer(s).IsValid();
}

TopoDS_Shape acceptShell(const TopoDS_Shape &candidate, double maxVolume) {
    if (usableSolid(candidate, maxVolume, true)) return candidate;
    const TopoDS_Shape fixed = tryFix(candidate);
    if (usableSolid(fixed, maxVolume, true)) return fixed;
    // Prefer a volume-correct solid over a perfect checker when OCCT's thick-solid
    // leaves harmless seam warnings (common on tapered planar roofs).
    if (usableSolid(fixed, maxVolume, false)) return fixed;
    if (usableSolid(candidate, maxVolume, false)) return candidate;
    return {};
}

// Default mode (arc joins) for curved bodies; complete-intersection for planar bodies where
// faces vanish under the offset or more than three faces meet at a vertex.
TopoDS_Shape thickSolid(const TopoDS_Shape &solid, const TopTools_ListOfShape &openings, double offset,
                        double maxVolume) {
    struct Attempt { double tol; bool complete; };
    for (const Attempt &a : {Attempt{1e-4, false}, Attempt{1e-7, true}, Attempt{1e-4, true},
                             Attempt{1e-3, true}, Attempt{1e-5, true}}) {
        try {
            BRepOffsetAPI_MakeThickSolid thick;
            thick.MakeThickSolidByJoin(solid, openings, offset, a.tol, BRepOffset_Skin,
                                       a.complete ? Standard_True : Standard_False, Standard_False,
                                       a.complete ? GeomAbs_Intersection : GeomAbs_Arc,
                                       a.complete ? Standard_True : Standard_False);
            thick.Build();
            if (!thick.IsDone()) continue;
            const TopoDS_Shape ok = acceptShell(thick.Shape(), maxVolume);
            if (!ok.IsNull()) return ok;
        } catch (const Standard_Failure &) {
        }
    }
    return {};
}

TopoDS_Shape offsetSolid(const TopoDS_Shape &solid, double offset) {
    struct Attempt { double tol; bool complete; };
    for (const Attempt &a : {Attempt{1e-4, false}, Attempt{1e-7, true}, Attempt{1e-4, true}}) {
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

// Fallback when MakeThickSolid fails (tapered roofs, multi-face apexes): hollow by
// solid-offset cut, then punch each opening face through the wall thickness.
TopoDS_Shape punchOpenings(const TopoDS_Shape &solid, const TopTools_ListOfShape &openings, double offset,
                           double maxVolume) {
    if (offset >= 0) return {};   // outside shells need thick-solid; punch is for inside openings
    const TopoDS_Shape inner = offsetSolid(solid, offset);
    if (inner.IsNull()) return {};
    TopoDS_Shape hollow;
    try {
        BRepAlgoAPI_Cut cut(solid, inner);
        if (!cut.IsDone()) return {};
        hollow = cut.Shape();
    } catch (const Standard_Failure &) {
        return {};
    }
    const double depth = std::fabs(offset) * 2.0 + 1.0;   // clear the wall into the cavity
    for (TopTools_ListIteratorOfShape it(openings); it.More(); it.Next()) {
        const TopoDS_Face f = TopoDS::Face(it.Value());
        try {
            const gp_Dir n = outwardNormal(f);
            // Slight outward bite + inward punch so the wall at the opening is fully removed
            BRepPrimAPI_MakePrism outPrism(f, gp_Vec(n) * 0.25);
            BRepPrimAPI_MakePrism inPrism(f, gp_Vec(n.Reversed()) * depth);
            TopoDS_Shape tool = inPrism.Shape();
            if (outPrism.IsDone()) {
                BRepAlgoAPI_Fuse fuse(tool, outPrism.Shape());
                if (fuse.IsDone() && !fuse.Shape().IsNull()) tool = fuse.Shape();
            }
            BRepAlgoAPI_Cut cut(hollow, tool);
            if (!cut.IsDone()) return {};
            hollow = cut.Shape();
        } catch (const Standard_Failure &) {
            return {};
        }
    }
    return acceptShell(hollow, maxVolume);
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
    // Merge coplanar faces left by face-extrude / taper joins — thick-solid is picky about seams
    const TopoDS_Shape solid = GeomUtils::unify(ctx.bodies[body].shape);
    const double v0 = GeomUtils::volume(solid);
    const double maxVolume = offset < 0 ? v0 : 1e300;

    if (!openings.IsEmpty()) {
        // Re-resolve openings on the unified solid (same centroids) so face identity matches
        TopTools_ListOfShape unifiedOpenings;
        for (const QVariant &ref : refs) {
            int b = -1;
            TopoDS_Face f;
            // Temporarily resolve against the live body, then match by centroid on unified
            if (!ctx.resolveFace(ref, b, f, err)) return fail(err);
            const gp_Pnt want = GeomUtils::faceCentroid(f);
            TopoDS_Face matched;
            double best = 1e300;
            for (TopExp_Explorer e(solid, TopAbs_FACE); e.More(); e.Next()) {
                const TopoDS_Face cand = TopoDS::Face(e.Current());
                const double d = GeomUtils::faceCentroid(cand).Distance(want);
                if (d < best) {
                    best = d;
                    matched = cand;
                }
            }
            if (matched.IsNull() || best > 1.0) return fail("Shell face no longer exists on the body");
            unifiedOpenings.Append(matched);
        }

        TopoDS_Shape result = thickSolid(solid, unifiedOpenings, offset, maxVolume);
        if (result.IsNull()) result = punchOpenings(solid, unifiedOpenings, offset, maxVolume);
        if (result.IsNull()) return fail("Shell failed: the walls could not be offset (try a thinner wall)");
        ctx.bodies[body].shape = result;
        return true;
    }

    const TopoDS_Shape inner = offsetSolid(solid, offset);
    if (inner.IsNull()) return fail("Shell failed: the walls could not be offset (try a thinner wall)");
    try {
        if (offset < 0) {
            BRepAlgoAPI_Cut cut(solid, inner);
            if (!cut.IsDone()) return fail("Shell failed");
            const TopoDS_Shape ok = acceptShell(cut.Shape(), maxVolume);
            ctx.bodies[body].shape = ok.IsNull() ? cut.Shape() : ok;
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
