// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#include "FissionExtrude.h"
#include "FissionSolidTools.h"
#include "GeomUtils.h"
#include <BRepAdaptor_Curve.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRepOffsetAPI_DraftAngle.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <BRepTools.hxx>
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

namespace {
gp_Dir faceOutward(const TopoDS_Face &f) {
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
}

bool FissionExtrude::compute(RebuildContext &ctx) {
    QString err;
    TopoDS_Shape profile;
    gp_Ax3 plane;
    const QVariantList profiles = list("profiles");
    if (RebuildContext::isFaceProfileList(profiles)) {
        int body = -1;
        QList<TopoDS_Face> faces;
        bool curved = false;
        for (const QVariant &ref : profiles) {
            int b = -1;
            TopoDS_Face face;
            if (!ctx.resolveFace(ref, b, face, err)) return fail(err);
            if (body >= 0 && b != body) return fail("Faces pulled together must be on one body");
            body = b;
            if (BRepAdaptor_Surface(face).GetType() != GeomAbs_Plane) curved = true;
            faces.append(face);
        }
        if (curved) {
            double d1 = 0, taper = 0;
            if (!number(ctx, "distance", d1) || !number(ctx, "taper", taper)) return false;
            if (std::fabs(taper) > 1e-9) return fail("Taper does not apply when pulling a round face");
            if (str("extent", "distance") != QLatin1String("distance"))
                return fail("Pull a round face with a distance");
            const double signedDist = flag("flip") ? -d1 : d1;
            setManipulator(GeomUtils::faceCentroid(faces.first()), faceOutward(faces.first()), "distance", signedDist);
            TopoDS_Shape pulled;
            if (!offsetFacesOnSolid(ctx.bodies[body].shape, faces, signedDist, pulled, err))
                return fail(err.isEmpty() ? QStringLiteral("Could not pull this face") : err);
            if (std::fabs(signedDist) < 1e-9) return true;
            const QList<TopoDS_Shape> parts = GeomUtils::solids(pulled);
            if (parts.isEmpty()) return fail("Could not pull this face");
            ctx.bodies[body].shape = parts.first();
            return true;
        }
    }
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
