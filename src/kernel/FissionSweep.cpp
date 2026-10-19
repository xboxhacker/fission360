// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#include "FissionSweep.h"
#include "FissionSketch.h"
#include "GeomUtils.h"
#include <BRepAdaptor_CompCurve.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepOffsetAPI_MakePipe.hxx>
#include <BRepOffsetAPI_MakePipeShell.hxx>
#include <BRepTools.hxx>
#include <BRep_Builder.hxx>
#include <GCPnts_AbscissaPoint.hxx>
#include <GCPnts_UniformAbscissa.hxx>
#include <GeomAPI_PointsToBSpline.hxx>
#include <Geom_BSplineCurve.hxx>
#include <TColgp_Array1OfPnt.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Compound.hxx>
#include <cmath>
#include <vector>

// A curve circling the path at `radius`, turning `twistDeg` from start to end. Used as the auxiliary spine
// of a pipe shell it turns the section by the same amount; the frame along the path is rotation-minimizing
// (double reflection), so a twist of zero sweeps like Perpendicular.
static TopoDS_Wire twistSpine(const TopoDS_Wire &path, const gp_Dir &startX, double radius, double twistDeg,
                              QString &error) {
    BRepAdaptor_CompCurve curve(path, Standard_True);
    const int n = 96;
    GCPnts_UniformAbscissa abscissa(curve, n);
    if (!abscissa.IsDone() || abscissa.NbPoints() < 2) {
        error = "The sweep path cannot be sampled";
        return {};
    }
    const int count = abscissa.NbPoints();
    std::vector<gp_Pnt> pts(count);
    std::vector<gp_Vec> tan(count);
    for (int i = 0; i < count; ++i) {
        gp_Vec d1;
        curve.D1(abscissa.Parameter(i + 1), pts[i], d1);
        if (d1.Magnitude() < 1e-12) d1 = i > 0 ? tan[i - 1] : gp_Vec(0, 0, 1);
        tan[i] = d1.Normalized();
    }
    gp_Vec r = gp_Vec(startX) - tan[0] * gp_Vec(startX).Dot(tan[0]);
    if (r.Magnitude() < 1e-9) {
        r = tan[0].Crossed(std::fabs(tan[0].Z()) < 0.9 ? gp_Vec(0, 0, 1) : gp_Vec(1, 0, 0));
    }
    r.Normalize();
    TColgp_Array1OfPnt aux(1, count);
    for (int i = 0; i < count; ++i) {
        if (i > 0) {
            // Double reflection: carries the reference direction along without spinning it
            const gp_Vec v1(pts[i - 1], pts[i]);
            const double c1 = v1.SquareMagnitude();
            if (c1 > 1e-18) {
                const gp_Vec rL = r - v1 * (2.0 / c1 * v1.Dot(r));
                const gp_Vec tL = tan[i - 1] - v1 * (2.0 / c1 * v1.Dot(tan[i - 1]));
                const gp_Vec v2 = tan[i] - tL;
                const double c2 = v2.SquareMagnitude();
                r = c2 > 1e-18 ? rL - v2 * (2.0 / c2 * v2.Dot(rL)) : rL;
            }
            r = r - tan[i] * r.Dot(tan[i]);
            if (r.Magnitude() < 1e-12) { error = "The sweep path folds back on itself"; return {}; }
            r.Normalize();
        }
        const double a = twistDeg * M_PI / 180.0 * i / (count - 1);
        const gp_Vec b = tan[i].Crossed(r);
        aux.SetValue(i + 1, pts[i].Translated((r * std::cos(a) + b * std::sin(a)) * radius));
    }
    GeomAPI_PointsToBSpline fit(aux, 3, 8, GeomAbs_C2, 1e-4);
    if (!fit.IsDone()) {
        error = "The twist guide could not be built";
        return {};
    }
    BRepBuilderAPI_MakeEdge edge(fit.Curve());
    if (!edge.IsDone()) {
        error = "The twist guide could not be built";
        return {};
    }
    return BRepBuilderAPI_MakeWire(edge.Edge()).Wire();
}

static TopoDS_Shape pipeShell(const TopoDS_Wire &path, const TopoDS_Wire &aux, const TopoDS_Wire &section) {
    BRepOffsetAPI_MakePipeShell ps(path);
    ps.SetMode(aux, Standard_True, BRepFill_NoContact);
    ps.Add(section, Standard_False, Standard_False);
    ps.Build();
    if (!ps.IsDone()) return {};
    ps.MakeSolid();
    return ps.Shape();
}

bool FissionSweep::compute(RebuildContext &ctx) {
    QString err;
    TopoDS_Shape profile;
    gp_Ax3 plane;
    const QVariantList profiles = list("profiles");
    const bool ok = RebuildContext::isFaceProfileList(profiles)
                        ? ctx.resolveFaceProfiles(profiles, profile, plane, err)
                        : ctx.resolveProfiles(str("sketch"), profiles, profile, plane, err);
    if (!ok) return fail(err);

    auto *pathSketch = qobject_cast<FissionSketch *>(ctx.feature(str("path")));
    if (!pathSketch) return fail("Select a path sketch");
    if (pathSketch->id() == str("sketch")) return fail("Profile and path must be in different sketches");
    const TopoDS_Wire path = pathSketch->pathWire(err);
    if (path.IsNull()) return fail(err);

    double twist = 0;
    if (!number(ctx, "twist", twist)) return false;
    TopoDS_Shape result;
    if (std::fabs(twist) < 1e-9) {
        const GeomFill_Trihedron mode = str("orientation") == "parallel" ? GeomFill_IsFixed : GeomFill_IsCorrectedFrenet;
        BRepOffsetAPI_MakePipe pipe(path, profile, mode, Standard_False);
        pipe.Build();
        if (!pipe.IsDone()) return fail("Sweep failed (path may self-intersect or curve too tightly)");
        result = pipe.Shape();
    } else {
        // Twisted: each face's outer loop is swept with the turning guide, holes are swept the same way and cut
        const double radius = std::max(1.0, GeomUtils::boundingBox(profile).CornerMin().Distance(
                                                GeomUtils::boundingBox(profile).CornerMax()));
        const TopoDS_Wire aux = twistSpine(path, plane.XDirection(), radius, twist, err);
        if (aux.IsNull()) return fail(err);
        BRep_Builder bb;
        TopoDS_Compound all;
        bb.MakeCompound(all);
        for (TopExp_Explorer fe(profile, TopAbs_FACE); fe.More(); fe.Next()) {
            const TopoDS_Face face = TopoDS::Face(fe.Current());
            const TopoDS_Wire outer = BRepTools::OuterWire(face);
            TopoDS_Shape solid = pipeShell(path, aux, outer);
            if (solid.IsNull()) return fail("Twisted sweep failed (try a smaller twist or a smoother path)");
            for (TopExp_Explorer we(face, TopAbs_WIRE); we.More(); we.Next()) {
                if (we.Current().IsSame(outer)) continue;
                const TopoDS_Shape hole = pipeShell(path, aux, TopoDS::Wire(we.Current()));
                if (hole.IsNull()) continue;
                BRepAlgoAPI_Cut cut(solid, hole);
                if (cut.IsDone()) solid = cut.Shape();
            }
            bb.Add(all, solid);
        }
        result = all;
    }

    if (!ctx.applyOperation(result, operation(), list("targets"), id(), err)) return fail(err);
    return true;
}
