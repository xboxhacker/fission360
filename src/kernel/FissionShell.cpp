// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#include "FissionShell.h"
#include "GeomUtils.h"
#include <BRepAdaptor_Surface.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepAlgoAPI_Fuse.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakeSolid.hxx>
#include <BRepBuilderAPI_Sewing.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepGProp.hxx>
#include <BRepLib.hxx>
#include <BRepOffsetAPI_MakeOffset.hxx>
#include <BRepOffsetAPI_MakeOffsetShape.hxx>
#include <BRepOffsetAPI_MakeThickSolid.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <BRepTools.hxx>
#include <GProp_GProps.hxx>
#include <GeomAbs_SurfaceType.hxx>
#include <ShapeFix_Shape.hxx>
#include <Standard_Failure.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_ListOfShape.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Solid.hxx>
#include <TopoDS_Wire.hxx>
#include <cmath>

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

bool allPlanar(const TopoDS_Shape &s) {
    for (TopExp_Explorer e(s, TopAbs_FACE); e.More(); e.Next()) {
        if (BRepAdaptor_Surface(TopoDS::Face(e.Current()), Standard_False).GetType() != GeomAbs_Plane)
            return false;
    }
    return true;
}

TopoDS_Shape tryFix(const TopoDS_Shape &s) {
    if (s.IsNull()) return {};
    try {
        ShapeFix_Shape fix(s);
        fix.Perform();
        const TopoDS_Shape out = fix.Shape().IsNull() ? s : fix.Shape();
        return GeomUtils::unify(out);
    } catch (const Standard_Failure &) {
        return GeomUtils::unify(s);
    }
}

// Arc-join shells often leave knife-edge "teeth" at corners — reject those even if volume looks OK.
int tinyFaceCount(const TopoDS_Shape &s, double minArea) {
    int n = 0;
    for (TopExp_Explorer e(s, TopAbs_FACE); e.More(); e.Next()) {
        GProp_GProps props;
        BRepGProp::SurfaceProperties(TopoDS::Face(e.Current()), props);
        if (props.Mass() < minArea) ++n;
    }
    return n;
}

bool usableSolid(const TopoDS_Shape &s, double maxVolume, bool requireCheck, bool rejectTiny) {
    if (s.IsNull() || GeomUtils::solids(s).isEmpty()) return false;
    const double v = GeomUtils::volume(s);
    if (!(v > 1e-9 && v < maxVolume - 1e-9)) return false;
    if (rejectTiny && tinyFaceCount(s, 1e-2) > 0) return false;   // < 0.01 mm² faces = corner spikes
    if (!requireCheck) return true;
    return BRepCheck_Analyzer(s).IsValid();
}

TopoDS_Shape acceptShell(const TopoDS_Shape &candidate, double maxVolume, bool allowLoose) {
    const TopoDS_Shape fixed = tryFix(candidate);
    if (usableSolid(fixed, maxVolume, true, true)) return fixed;
    if (usableSolid(candidate, maxVolume, true, true)) return tryFix(candidate);
    if (!allowLoose) return {};
    // Last resort (punch fallback only): volume OK after fix, still no tiny faces
    if (usableSolid(fixed, maxVolume, false, true)) return fixed;
    return {};
}

struct ThickAttempt {
    double tol;
    bool complete;           // Intersection flag (complete intersection mode)
    GeomAbs_JoinType join;
    bool removeInt;
};

TopoDS_Shape thickSolid(const TopoDS_Shape &solid, const TopTools_ListOfShape &openings, double offset,
                        double maxVolume) {
    // Planar bodies: Intersection-first — Arc joins are what produce the jagged inner-corner teeth.
    // Curved bodies: Arc first (Intersection often fails on cylinders/fillets).
    const bool planar = allPlanar(solid);
    const ThickAttempt planarFirst[] = {
        {1e-7, true, GeomAbs_Intersection, true},
        {1e-5, true, GeomAbs_Intersection, true},
        {1e-4, true, GeomAbs_Intersection, true},
        {1e-3, true, GeomAbs_Intersection, true},
        {1e-4, false, GeomAbs_Intersection, true},
        {1e-4, true, GeomAbs_Arc, false},
    };
    const ThickAttempt curvedFirst[] = {
        {1e-4, false, GeomAbs_Arc, false},
        {1e-7, true, GeomAbs_Intersection, true},
        {1e-4, true, GeomAbs_Intersection, true},
        {1e-4, true, GeomAbs_Arc, false},
    };
    const ThickAttempt *attempts = planar ? planarFirst : curvedFirst;
    const int n = planar ? int(sizeof(planarFirst) / sizeof(planarFirst[0]))
                         : int(sizeof(curvedFirst) / sizeof(curvedFirst[0]));

    for (int i = 0; i < n; ++i) {
        const ThickAttempt &a = attempts[i];
        try {
            BRepOffsetAPI_MakeThickSolid thick;
            thick.MakeThickSolidByJoin(solid, openings, offset, a.tol, BRepOffset_Skin,
                                       a.complete ? Standard_True : Standard_False, Standard_False, a.join,
                                       a.removeInt ? Standard_True : Standard_False);
            thick.Build();
            if (!thick.IsDone()) continue;
            // Never loose-accept thick-solid — Arc junk must not ship
            const TopoDS_Shape ok = acceptShell(thick.Shape(), maxVolume, false);
            if (!ok.IsNull()) return ok;
        } catch (const Standard_Failure &) {
        }
    }
    return {};
}

TopoDS_Shape offsetSolid(const TopoDS_Shape &solid, double offset) {
    struct Attempt {
        double tol;
        bool complete;
        GeomAbs_JoinType join;
    };
    // Same priority: Intersection for clean corners on planar offsets
    const Attempt attempts[] = {
        {1e-7, true, GeomAbs_Intersection},
        {1e-4, true, GeomAbs_Intersection},
        {1e-4, false, GeomAbs_Arc},
    };
    for (const Attempt &a : attempts) {
        try {
            BRepOffsetAPI_MakeOffsetShape off;
            off.PerformByJoin(solid, offset, a.tol, BRepOffset_Skin, a.complete ? Standard_True : Standard_False,
                              Standard_False, a.join, a.complete ? Standard_True : Standard_False);
            if (!off.IsDone()) continue;
            for (TopExp_Explorer exp(off.Shape(), TopAbs_SHELL); exp.More(); exp.Next()) {
                TopoDS_Solid inner = BRepBuilderAPI_MakeSolid(TopoDS::Shell(exp.Current())).Solid();
                BRepLib::OrientClosedSolid(inner);
                if (!inner.IsNull() && GeomUtils::volume(inner) > 1e-9) return GeomUtils::unify(inner);
            }
        } catch (const Standard_Failure &) {
        }
    }
    return {};
}

// Grow a planar opening face in-plane so the punch clears adjacent walls at corners
TopoDS_Shape punchTool(const TopoDS_Face &f, double wall, double margin) {
    const gp_Dir n = outwardNormal(f);
    TopoDS_Shape faceShape = f;
    // 2D offset of the outer wire when the face is planar — grows the cut past sharp corners
    try {
        if (BRepAdaptor_Surface(f, Standard_False).GetType() == GeomAbs_Plane) {
            TopoDS_Wire outer;
            for (TopExp_Explorer w(f, TopAbs_WIRE); w.More(); w.Next()) {
                outer = TopoDS::Wire(w.Current());
                break;
            }
            if (!outer.IsNull()) {
                BRepOffsetAPI_MakeOffset off(outer, GeomAbs_Intersection);
                off.Perform(margin);
                if (off.IsDone() && !off.Shape().IsNull()) {
                    TopoDS_Wire grown;
                    if (off.Shape().ShapeType() == TopAbs_WIRE) grown = TopoDS::Wire(off.Shape());
                    else {
                        for (TopExp_Explorer e(off.Shape(), TopAbs_WIRE); e.More(); e.Next()) {
                            grown = TopoDS::Wire(e.Current());
                            break;
                        }
                    }
                    if (!grown.IsNull()) {
                        BRepBuilderAPI_MakeFace mk(grown);
                        if (mk.IsDone()) faceShape = mk.Face();
                    }
                }
            }
        }
    } catch (const Standard_Failure &) {
        faceShape = f;
    }

    const double depth = wall * 2.0 + 1.0;
    BRepPrimAPI_MakePrism inPrism(faceShape, gp_Vec(n.Reversed()) * depth);
    BRepPrimAPI_MakePrism outPrism(faceShape, gp_Vec(n) * std::max(0.5, margin));
    TopoDS_Shape tool = inPrism.Shape();
    if (outPrism.IsDone()) {
        BRepAlgoAPI_Fuse fuse(tool, outPrism.Shape());
        if (fuse.IsDone() && !fuse.Shape().IsNull()) tool = fuse.Shape();
    }
    return tool;
}

TopoDS_Shape sewSolid(const TopoDS_Shape &s) {
    try {
        BRepBuilderAPI_Sewing sew(1e-4);
        for (TopExp_Explorer e(s, TopAbs_FACE); e.More(); e.Next()) sew.Add(e.Current());
        sew.Perform();
        TopoDS_Shape sewn = sew.SewedShape();
        if (sewn.IsNull()) return s;
        for (TopExp_Explorer sh(sewn, TopAbs_SHELL); sh.More(); sh.Next()) {
            TopoDS_Solid solid = BRepBuilderAPI_MakeSolid(TopoDS::Shell(sh.Current())).Solid();
            BRepLib::OrientClosedSolid(solid);
            if (!solid.IsNull() && GeomUtils::volume(solid) > 1e-9) return solid;
        }
        return sewn;
    } catch (const Standard_Failure &) {
        return s;
    }
}

// Fallback when MakeThickSolid fails: hollow by solid-offset cut, then punch openings with a grown face.
TopoDS_Shape punchOpenings(const TopoDS_Shape &solid, const TopTools_ListOfShape &openings, double offset,
                           double maxVolume) {
    if (offset >= 0) return {};
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
    const double wall = std::fabs(offset);
    for (TopTools_ListOfShape::Iterator it(openings); it.More(); it.Next()) {
        try {
            const TopoDS_Shape tool = punchTool(TopoDS::Face(it.Value()), wall, wall * 0.25 + 0.25);
            if (tool.IsNull()) return {};
            BRepAlgoAPI_Cut cut(hollow, tool);
            if (!cut.IsDone()) return {};
            hollow = cut.Shape();
        } catch (const Standard_Failure &) {
            return {};
        }
    }
    hollow = sewSolid(hollow);
    TopoDS_Shape ok = acceptShell(hollow, maxVolume, false);
    if (ok.IsNull()) ok = acceptShell(hollow, maxVolume, true);
    return ok;
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
    const TopoDS_Shape solid = GeomUtils::unify(ctx.bodies[body].shape);
    const double v0 = GeomUtils::volume(solid);
    const double maxVolume = offset < 0 ? v0 : 1e300;

    if (!openings.IsEmpty()) {
        TopTools_ListOfShape unifiedOpenings;
        for (const QVariant &ref : refs) {
            int b = -1;
            TopoDS_Face f;
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
            const TopoDS_Shape ok = acceptShell(cut.Shape(), maxVolume, true);
            ctx.bodies[body].shape = ok.IsNull() ? tryFix(cut.Shape()) : ok;
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
