// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#include "FissionTransform.h"
#include "GeomUtils.h"
#include <BRepBuilderAPI_Transform.hxx>
#include <algorithm>

bool FissionBodyCopyFeature::compute(RebuildContext &ctx) {
    QList<gp_Trsf> trsfs;
    if (!transforms(ctx, trsfs)) return false;
    if (trsfs.isEmpty()) return fail("Nothing to copy");

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
