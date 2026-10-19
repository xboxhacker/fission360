// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#include "FissionTransform.h"
#include "FissionTypes.h"
#include "GeomUtils.h"
#include <BRepAdaptor_Surface.hxx>
#include <BRepAlgoAPI_Fuse.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRepLProp_SLProps.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <BRepTools.hxx>
#include <Bnd_Box.hxx>
#include <GeomAbs_SurfaceType.hxx>
#include <TopoDS.hxx>
#include <algorithm>
#include <cmath>

namespace {

// Build a solid that removes material matching the selected faces (hole wall / bottom / pocket).
// Cylindrical faces → through-body cylinder. Otherwise extrude each face along its normal through the body.
TopoDS_Shape cuttingToolFromFaces(RebuildContext &ctx, const QVariantList &faceRefs, int &bodyOut, QString &err) {
    QList<TopoDS_Face> faces;
    bodyOut = -1;
    for (const QVariant &ref : faceRefs) {
        int b = -1;
        TopoDS_Face f;
        if (!ctx.resolveFace(ref, b, f, err)) return {};
        if (bodyOut < 0) bodyOut = b;
        else if (b != bodyOut) {
            err = QStringLiteral("Pattern faces must belong to the same body");
            return {};
        }
        faces.append(f);
    }
    if (faces.isEmpty() || bodyOut < 0 || bodyOut >= ctx.bodies.size()) {
        err = QStringLiteral("Select faces to pattern");
        return {};
    }
    const TopoDS_Shape &body = ctx.bodies[bodyOut].shape;
    Bnd_Box bb = GeomUtils::boundingBox(body);
    if (bb.IsVoid()) {
        err = QStringLiteral("Body has no bounds");
        return {};
    }
    double x0, y0, z0, x1, y1, z1;
    bb.Get(x0, y0, z0, x1, y1, z1);
    const double diag = std::max(1.0, std::sqrt(bb.SquareExtent()));
    const double pad = std::max(1.0, diag * 0.02);

    auto axisRange = [&](const gp_Ax1 &ax, double &tMin, double &tMax) {
        const gp_Pnt loc = ax.Location();
        const gp_Dir dir = ax.Direction();
        tMin = 1e100;
        tMax = -1e100;
        for (int i = 0; i < 8; ++i) {
            const gp_Pnt p((i & 1) ? x1 : x0, (i & 2) ? y1 : y0, (i & 4) ? z1 : z0);
            const double t = gp_Vec(loc, p).Dot(dir);
            tMin = std::min(tMin, t);
            tMax = std::max(tMax, t);
        }
        tMin -= pad;
        tMax += pad;
    };

    for (const TopoDS_Face &f : faces) {
        BRepAdaptor_Surface s(f);
        if (s.GetType() != GeomAbs_Cylinder) continue;
        const gp_Cylinder cyl = s.Cylinder();
        const gp_Ax1 ax = cyl.Axis();
        const double r = cyl.Radius() * 1.002;
        double tMin = 0, tMax = 0;
        axisRange(ax, tMin, tMax);
        const double h = tMax - tMin;
        if (h < 1e-6 || r < 1e-6) continue;
        const gp_Ax2 ax2(ax.Location().Translated(gp_Vec(ax.Direction()) * tMin), ax.Direction());
        return BRepPrimAPI_MakeCylinder(ax2, r, h).Shape();
    }

    QList<TopoDS_Shape> tools;
    for (const TopoDS_Face &f : faces) {
        gp_Dir n(0, 0, 1);
        try {
            BRepAdaptor_Surface s(f);
            if (s.GetType() == GeomAbs_Plane) {
                n = s.Plane().Axis().Direction();
            } else {
                Standard_Real u1, u2, v1, v2;
                BRepTools::UVBounds(f, u1, u2, v1, v2);
                BRepLProp_SLProps props(s, 0.5 * (u1 + u2), 0.5 * (v1 + v2), 1, 1e-6);
                if (props.IsNormalDefined()) n = props.Normal();
            }
        } catch (...) {
            continue;
        }
        if (f.Orientation() == TopAbs_REVERSED) n.Reverse();
        gp_Ax1 ax(GeomUtils::faceCentroid(f), n);
        double tMin = 0, tMax = 0;
        axisRange(ax, tMin, tMax);
        const double hPos = std::max(pad, tMax);
        const double hNeg = std::max(pad, -tMin);
        TopoDS_Shape a = BRepPrimAPI_MakePrism(f, gp_Vec(n) * hPos).Shape();
        TopoDS_Shape b = BRepPrimAPI_MakePrism(f, gp_Vec(n) * (-hNeg)).Shape();
        if (!a.IsNull() && !b.IsNull()) {
            BRepAlgoAPI_Fuse fuse(a, b);
            if (fuse.IsDone()) tools.append(fuse.Shape());
            else {
                tools.append(a);
                tools.append(b);
            }
        } else if (!a.IsNull()) tools.append(a);
        else if (!b.IsNull()) tools.append(b);
    }
    if (tools.isEmpty()) {
        err = QStringLiteral("Could not build a cutting tool from the selected faces");
        return {};
    }
    return GeomUtils::makeCompound(tools);
}

} // namespace

bool FissionBodyCopyFeature::compute(RebuildContext &ctx) {
    QList<gp_Trsf> trsfs;
    if (!transforms(ctx, trsfs)) return false;
    if (trsfs.isEmpty()) return fail("Nothing to copy");

    const QVariantList faceRefs = list("faces");
    if (!faceRefs.isEmpty()) {
        int body = -1;
        QString err;
        const TopoDS_Shape tool = cuttingToolFromFaces(ctx, faceRefs, body, err);
        if (tool.IsNull()) return fail(err);
        QList<TopoDS_Shape> copies;
        for (const gp_Trsf &t : trsfs)
            copies.append(BRepBuilderAPI_Transform(tool, t, Standard_True).Shape());
        // Face patterns default to Cut so patterned holes remove material from the source body
        const FeatureOperation op = operationFromString(str("operation", "cut"));
        if (op == FeatureOperation::NewBody) {
            for (const auto &c : copies) ctx.bodies.append({ctx.nextBodyName(), c, id()});
            return true;
        }
        if (!ctx.applyOperation(GeomUtils::makeCompound(copies), op, {body}, id(), err))
            return fail(err);
        return true;
    }

    QList<int> sources;
    for (const QVariant &v : list("bodies")) sources.append(v.toInt());
    if (sources.isEmpty())
        for (int i = 0; i < ctx.bodies.size(); ++i) sources.append(i);
    if (sources.isEmpty()) return fail("There are no bodies to copy");
    std::sort(sources.begin(), sources.end());
    sources.erase(std::unique(sources.begin(), sources.end()), sources.end());
    for (int i : sources)
        if (i < 0 || i >= ctx.bodies.size()) return fail(QString("Body %1 no longer exists").arg(i));

    const bool join = str("operation", "join") == "join";
    QString err;
    // Highest index first: joining may insert bodies after the source, which must not shift pending sources
    for (int k = sources.size() - 1; k >= 0; --k) {
        const int src = sources[k];
        QList<TopoDS_Shape> copies;
        for (const gp_Trsf &t : trsfs)
            copies.append(BRepBuilderAPI_Transform(ctx.bodies[src].shape, t, Standard_True).Shape());
        if (join) {
            if (!ctx.applyOperation(GeomUtils::makeCompound(copies), FeatureOperation::Join, {src}, id(), err))
                return fail(err);
        } else {
            for (const auto &c : copies) ctx.bodies.append({ctx.nextBodyName(), c, id()});
        }
    }
    return true;
}

bool FissionMirror::transforms(RebuildContext &ctx, QList<gp_Trsf> &out) {
    gp_Ax3 plane;
    QString err;
    if (!ctx.resolvePlane(get("plane"), plane, err)) return fail(err);
    gp_Trsf t;
    t.SetMirror(plane.Ax2());
    out.append(t);
    return true;
}

static bool countValue(double v, int &out) {
    out = static_cast<int>(std::lround(v));
    return out >= 1 && out <= 1000;
}

bool FissionRectangularPattern::transforms(RebuildContext &ctx, QList<gp_Trsf> &out) {
    QString err;
    double c1 = 1, s1 = 0, c2 = 1, s2 = 0;
    if (!number(ctx, "count1", c1) || !number(ctx, "spacing1", s1)) return false;
    int n1 = 1, n2 = 1;
    if (!countValue(c1, n1)) return fail("Quantity must be between 1 and 1000");
    gp_Ax1 a1, a2;
    if (!ctx.resolveAxis(get("direction1"), a1, err)) return fail(err);

    const bool twoDirs = !str("direction2").isEmpty();
    if (twoDirs) {
        if (!number(ctx, "count2", c2) || !number(ctx, "spacing2", s2)) return false;
        if (!countValue(c2, n2)) return fail("Quantity must be between 1 and 1000");
        if (!ctx.resolveAxis(get("direction2"), a2, err)) return fail(err);
    }
    for (int i = 0; i < n1; ++i) {
        for (int j = 0; j < n2; ++j) {
            if (i == 0 && j == 0) continue;
            gp_Vec v = gp_Vec(a1.Direction()) * (s1 * i);
            if (twoDirs) v += gp_Vec(a2.Direction()) * (s2 * j);
            gp_Trsf t;
            t.SetTranslation(v);
            out.append(t);
        }
    }
    return true;
}

bool FissionCircularPattern::transforms(RebuildContext &ctx, QList<gp_Trsf> &out) {
    QString err;
    double c = 1, angle = 360;
    if (!number(ctx, "count", c) || !number(ctx, "angle", angle)) return false;
    int n = 1;
    if (!countValue(c, n) || n < 2) return fail("Quantity must be between 2 and 1000");
    gp_Ax1 axis;
    if (!ctx.resolveAxis(get("axis"), axis, err)) return fail(err);
    // Full circle spaces instances evenly; a partial angle places the last instance at the end angle
    const double step = std::fabs(angle) >= 360.0 - 1e-9 ? 360.0 / n : angle / (n - 1);
    for (int i = 1; i < n; ++i) {
        gp_Trsf t;
        t.SetRotation(axis, step * i * M_PI / 180.0);
        out.append(t);
    }
    return true;
}
