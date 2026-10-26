// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#include "FissionCoil.h"
#include <BRepOffsetAPI_MakePipeShell.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <GeomAPI_Interpolate.hxx>
#include <Geom_Circle.hxx>
#include <TColgp_HArray1OfPnt.hxx>
#include <gp_Trsf.hxx>
#include <cmath>

QVariantMap FissionCoil::defaults() const {
    return {{"diameter", "40"}, {"tubeDiameter", "8"}, {"pitch", "10"}, {"height", "50"},
            {"origin", QVariantList{0.0, 0.0, 0.0}},
            {"operation", "join"}, {"targets", QVariantList()}};
}

bool FissionCoil::compute(RebuildContext &ctx) {
    double dia, tube, pitch, h;
    if (!number(ctx, "diameter", dia)) return false;
    if (!number(ctx, "tubeDiameter", tube)) return false;
    if (!number(ctx, "pitch", pitch)) return false;
    if (!number(ctx, "height", h)) return false;
    if (dia <= 0 || tube <= 0 || pitch <= 0 || h <= 0)
        return fail("Coil diameter, tube diameter, pitch and height must be positive");

    const QVariantList o = list("origin");
    const double ox = o.size() >= 1 ? o.at(0).toDouble() : 0.0;
    const double oy = o.size() >= 2 ? o.at(1).toDouble() : 0.0;
    const double oz = o.size() >= 3 ? o.at(2).toDouble() : 0.0;

    const double rHelix = dia / 2.0;
    const double rTube  = tube / 2.0;
    const double totalAngle = 2.0 * M_PI * (h / pitch);
    const int steps = std::max(100, static_cast<int>(totalAngle * 10.0));

    Handle(TColgp_HArray1OfPnt) pts = new TColgp_HArray1OfPnt(1, steps + 1);
    for (int i = 0; i <= steps; ++i) {
        const double t = totalAngle * i / steps;
        pts->SetValue(i + 1, gp_Pnt(rHelix * std::cos(t), rHelix * std::sin(t), pitch * t / (2.0 * M_PI)));
    }
    GeomAPI_Interpolate interp(pts, Standard_False, 1e-6);
    interp.Perform();
    if (!interp.IsDone()) return fail("Coil spine interpolation failed");

    TopoDS_Wire spine = BRepBuilderAPI_MakeWire(BRepBuilderAPI_MakeEdge(interp.Curve()).Edge()).Wire();

    const gp_Pnt center(rHelix, 0.0, 0.0);
    const gp_Ax2 profileAx(center, gp_Dir(0, 1, 0));
    Handle(Geom_Circle) circle = new Geom_Circle(profileAx, rTube);
    TopoDS_Wire profile = BRepBuilderAPI_MakeWire(BRepBuilderAPI_MakeEdge(circle).Edge()).Wire();

    BRepOffsetAPI_MakePipeShell pipe(spine);
    pipe.SetMode(gp_Dir(0, 1, 0));   // fixed binormal keeps the tube in a plane through the axis (as in Hole)
    pipe.Add(profile, Standard_False, Standard_False);
    pipe.Build();
    if (!pipe.IsDone()) return fail("Coil sweep build failed");
    if (!pipe.MakeSolid()) return fail("Coil solid creation failed");

    TopoDS_Shape shape = pipe.Shape();
    gp_Trsf trsf;
    trsf.SetTranslation(gp_Vec(ox, oy, oz));
    shape = BRepBuilderAPI_Transform(shape, trsf).Shape();
    if (shape.IsNull()) return fail("Coil transform failed");

    QString err;
    return ctx.applyOperation(shape, operation(), list("targets"), id(), err) || fail(err);
}