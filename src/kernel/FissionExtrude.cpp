// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#include "FissionExtrude.h"
#include "GeomUtils.h"
#include <BRepAdaptor_Curve.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRepOffsetAPI_DraftAngle.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <gp_Pln.hxx>

QVariantMap FissionExtrude::defaults() const {
    return {{"sketch", ""}, {"profiles", QVariantList()}, {"extent", "distance"},
            {"distance", "10 mm"}, {"distance2", "10 mm"}, {"taper", "0 deg"},
            {"flip", false}, {"operation", "join"}, {"targets", QVariantList()}};
}

// Prism of `profile` from offset `from` to `to` along dir (both signed distances)
static TopoDS_Shape prism(const TopoDS_Shape &profile, const gp_Dir &dir, double from, double to) {
    TopoDS_Shape base = profile;
    if (std::fabs(from) > 1e-9) {
        gp_Trsf t;
        t.SetTranslation(gp_Vec(dir) * from);
        base = BRepBuilderAPI_Transform(profile, t, Standard_True).Shape();
    }
    BRepPrimAPI_MakePrism mk(base, gp_Vec(dir) * (to - from), Standard_True);
    mk.Build();
    return mk.IsDone() ? mk.Shape() : TopoDS_Shape();
}

bool FissionExtrude::compute(RebuildContext &ctx) {
    QString err;
    TopoDS_Shape profile;
    gp_Ax3 plane;
    const QVariantList profiles = list("profiles");
    const bool ok = RebuildContext::isFaceProfileList(profiles)
                        ? ctx.resolveFaceProfiles(profiles, profile, plane, err)
                        : ctx.resolveProfiles(str("sketch"), profiles, profile, plane, err);
    if (!ok) return fail(err);

    double d1 = 0, d2 = 0, taper = 0;
    if (!number(ctx, "distance", d1) || !number(ctx, "taper", taper)) return false;

    gp_Dir dir = plane.Direction();
    if (flag("flip")) dir.Reverse();
    const QString extent = str("extent", "distance");
    if (extent != "throughAll") setManipulator(GeomUtils::shapeCentroid(profile), dir, "distance", d1);
    if (d1 < 0) { dir.Reverse(); d1 = -d1; }
    if (extent == "distance") {
        // Taper knob: on the profile outline furthest along the sketch X direction, at the far end of the side
        const gp_Vec X(plane.XDirection());
        const gp_Pnt c = GeomUtils::shapeCentroid(profile);
        gp_Pnt pivot = c;
        double best = -1e300;
        for (TopExp_Explorer exp(profile, TopAbs_EDGE); exp.More(); exp.Next()) {
            BRepAdaptor_Curve curve(TopoDS::Edge(exp.Current()));
            for (int k = 0; k <= 32; ++k) {
                const gp_Pnt p = curve.Value(curve.FirstParameter() + (curve.LastParameter() - curve.FirstParameter()) * k / 32);
                const double u = gp_Vec(c, p).Dot(X);
                if (u > best) { best = u; pivot = p; }
            }
        }
        setAngleManipulator(pivot, dir, gp_Dir(X), d1, "taper", taper);
    }

    double from = 0, to = d1;
    if (extent == "symmetric") {
        from = -d1; to = d1;
    } else if (extent == "twoSides") {
        if (!number(ctx, "distance2", d2)) return false;
        from = -d2; to = d1;
    } else if (extent == "throughAll") {
        const double all = ctx.modelSize() * 4;
        to = all;
        if (operation() == FeatureOperation::Cut) from = 0;
    }
    if (std::fabs(to - from) < 1e-7) return fail("Extrude distance is zero");

    TopoDS_Shape shape = prism(profile, dir, from, to);
    if (shape.IsNull()) return fail("Extrude failed");

    if (std::fabs(taper) > 1e-9) {
        // Draft the side faces about the sketch plane; caps are left untouched
        if (extent != "distance") return fail("Taper is only supported for one-sided extrudes");
        const gp_Pln neutral(plane.Location(), dir);
        BRepOffsetAPI_DraftAngle draft(shape);
        for (TopExp_Explorer exp(shape, TopAbs_FACE); exp.More(); exp.Next()) {
            const TopoDS_Face f = TopoDS::Face(exp.Current());
            BRepAdaptor_Surface s(f);
            if (s.GetType() == GeomAbs_Plane && s.Plane().Axis().Direction().IsParallel(dir, 1e-6)) continue;
            // Positive taper narrows the body away from the sketch plane
            draft.Add(f, dir, taper * M_PI / 180.0, neutral);
            if (!draft.AddDone()) return fail("Taper angle cannot be applied to this profile");
        }
        draft.Build();
        if (!draft.IsDone()) return fail("Taper failed");
        shape = draft.Shape();
    }

    if (!ctx.applyOperation(shape, operation(), list("targets"), id(), err)) return fail(err);
    return true;
}
