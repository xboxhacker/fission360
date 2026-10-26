// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#include "FissionPipe.h"
#include "FissionSolidTools.h"
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepAlgoAPI_Cut.hxx>

QVariantMap FissionPipe::defaults() const {
    return {{"innerDiameter", "20"}, {"outerDiameter", "30"}, {"length", "50"},
            {"plane", ""}, {"origin", QVariantList{0.0, 0.0, 0.0}},
            {"operation", "newBody"}, {"targets", QVariantList()}};
}

bool FissionPipe::compute(RebuildContext &ctx) {
    double in, out, len;
    if (!number(ctx, "innerDiameter", in)) return false;
    if (!number(ctx, "outerDiameter", out)) return false;
    if (!number(ctx, "length", len)) return false;
    if (len <= 0 || out <= 0) return fail("Pipe outer diameter and length must be positive");
    if (in >= out) return fail("Pipe inner diameter must be smaller than outer diameter");
    if (in < 0) return fail("Pipe inner diameter must be zero or positive");

    gp_Ax3 frame, base;
    QString err;
    if (!primitiveFrame(ctx, get("plane"), list("origin"), frame, err, &base)) return fail(err);

    // MakeCylinder takes radius, then height. The bore is a hair longer so the cut goes through both ends.
    TopoDS_Shape outer = BRepPrimAPI_MakeCylinder(out / 2.0, len).Shape();
    TopoDS_Shape pipe = outer;
    if (in > 0) {
        const gp_Ax2 bore(gp_Pnt(0, 0, -0.05), gp_Dir(0, 0, 1));
        TopoDS_Shape inner = BRepPrimAPI_MakeCylinder(bore, in / 2.0, len + 0.1).Shape();
        BRepAlgoAPI_Cut cut(outer, inner);
        if (!cut.IsDone()) return fail("Pipe cut failed");
        pipe = cut.Shape();
    }
    pipe = placeOnFrame(pipe, frame);
    if (pipe.IsNull()) return fail("Pipe placement failed");
    setHandles(base, {{frame.Location(), frame.XDirection(), "outerDiameter", out, 0.5},
                      {frame.Location(), frame.YDirection(), "innerDiameter", in, 0.5},
                      {frame.Location(), frame.Direction(), "length", len}});
    return ctx.applyOperation(pipe, operation(), list("targets"), id(), err) || fail(err);
}
