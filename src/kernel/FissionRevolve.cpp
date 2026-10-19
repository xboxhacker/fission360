// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#include "FissionRevolve.h"
#include <BRepBuilderAPI_Transform.hxx>
#include <BRepPrimAPI_MakeRevol.hxx>

QVariantMap FissionRevolve::defaults() const {
    return {{"sketch", ""}, {"profiles", QVariantList()}, {"axis", "Y"}, {"extent", "full"},
            {"angle", "360 deg"}, {"flip", false}, {"operation", "join"}, {"targets", QVariantList()}};
}

bool FissionRevolve::compute(RebuildContext &ctx) {
    QString err;
    TopoDS_Shape profile;
    gp_Ax3 plane;
    const QVariantList profiles = list("profiles");
    const bool ok = RebuildContext::isFaceProfileList(profiles)
                        ? ctx.resolveFaceProfiles(profiles, profile, plane, err)
                        : ctx.resolveProfiles(str("sketch"), profiles, profile, plane, err);
    if (!ok) return fail(err);

    gp_Ax1 axis;
    if (!ctx.resolveAxis(get("axis"), axis, err)) return fail(err);
    if (flag("flip")) axis.Reverse();

    const QString extent = str("extent", "full");
    double angle = 360.0;
    if (extent != "full" && !number(ctx, "angle", angle)) return false;
    if (std::fabs(angle) < 1e-7) return fail("Revolve angle is zero");
    angle = std::clamp(angle, -360.0, 360.0);

    TopoDS_Shape base = profile;
    if (extent == "symmetric") {
        gp_Trsf t;
        t.SetRotation(axis, -angle * M_PI / 180.0);
        base = BRepBuilderAPI_Transform(profile, t, Standard_True).Shape();
        angle *= 2;
    }

    BRepPrimAPI_MakeRevol rev(base, axis, std::min(std::fabs(angle), 360.0) * M_PI / 180.0 * (angle < 0 ? -1 : 1),
                              Standard_True);
    rev.Build();
    if (!rev.IsDone()) return fail("Revolve failed (profile may cross the axis)");

    if (!ctx.applyOperation(rev.Shape(), operation(), list("targets"), id(), err)) return fail(err);
    return true;
}
