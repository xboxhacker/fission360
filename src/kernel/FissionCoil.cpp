// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#include "FissionCoil.h"
#include "FissionSolidTools.h"
#include <BRepOffsetAPI_MakePipe.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <GeomFill_Trihedron.hxx>
#include <GeomAPI_Interpolate.hxx>
#include <Geom_Circle.hxx>
#include <Geom_Curve.hxx>
#include <TColgp_HArray1OfPnt.hxx>
#include <TColStd_HArray1OfReal.hxx>
#include <cmath>

QVariantMap FissionCoil::defaults() const {
    return {{"diameter", "40"}, {"tubeDiameter", "8"}, {"pitch", "10"}, {"height", "50"},
            {"plane", ""}, {"origin", QVariantList{0.0, 0.0, 0.0}},
            {"operation", "newBody"}, {"targets", QVariantList()}};
}

bool FissionCoil::compute(RebuildContext &ctx) {
    double dia, tube, pitch, h;
    if (!number(ctx, "diameter", dia)) return false;
    if (!number(ctx, "tubeDiameter", tube)) return false;
    if (!number(ctx, "pitch", pitch)) return false;
    if (!number(ctx, "height", h)) return false;
    if (dia <= 0 || tube <= 0 || pitch <= 0 || h <= 0)
        return fail("Coil diameter, tube diameter, pitch and height must be positive");
    if (tube >= dia) return fail("Tube diameter must be smaller than the coil diameter");

    gp_Ax3 frame, base;
    QString err;
    if (!primitiveFrame(ctx, get("plane"), list("origin"), frame, err, &base)) return fail(err);

    const double rHelix = dia / 2.0;
    const double rTube  = tube / 2.0;
    const double totalAngle = 2.0 * M_PI * (h / pitch);
    const int steps = std::max(100, static_cast<int>(std::ceil(totalAngle * 10.0))); // about every 6°

    Handle(TColgp_HArray1OfPnt) pts = new TColgp_HArray1OfPnt(1, steps + 1);
    Handle(TColStd_HArray1OfReal) params = new TColStd_HArray1OfReal(1, steps + 1);
    for (int i = 0; i <= steps; ++i) {
        const double t = totalAngle * i / steps;
        pts->SetValue(i + 1, gp_Pnt(rHelix * std::cos(t), rHelix * std::sin(t), pitch * t / (2.0 * M_PI)));
        params->SetValue(i + 1, t);
    }
    // Parameter is the helix angle, so the end tangents are dP/d(angle). Scaling is left off:
    // OCCT's scaler guesses the magnitude from the first three points and throws a long helix off the cylinder.
    auto helixTangent = [&](double t) {
        return gp_Vec(-rHelix * std::sin(t), rHelix * std::cos(t), pitch / (2.0 * M_PI));
    };
    GeomAPI_Interpolate interp(pts, params, Standard_False, 1e-7);
    interp.Load(helixTangent(0), helixTangent(totalAngle), Standard_False);
    interp.Perform();
    if (!interp.IsDone()) return fail("Coil spine interpolation failed");

    const Handle(Geom_Curve) curve = interp.Curve();
    gp_Pnt start;
    gp_Vec startTan;
    curve->D1(curve->FirstParameter(), start, startTan);
    if (startTan.Magnitude() < 1e-9) return fail("Coil spine has no direction");

    TopoDS_Wire spine = BRepBuilderAPI_MakeWire(BRepBuilderAPI_MakeEdge(curve).Edge()).Wire();

    // A disk standing square to the helix. The pipe carries that disk rigidly, so the tube stays
    // round. A pipe shell with a fixed binormal flattens the tube wherever the helix runs along it.
    Handle(Geom_Circle) circle = new Geom_Circle(gp_Ax2(start, gp_Dir(startTan)), rTube);
    TopoDS_Wire profile = BRepBuilderAPI_MakeWire(BRepBuilderAPI_MakeEdge(circle).Edge()).Wire();
    TopoDS_Face disk = BRepBuilderAPI_MakeFace(profile).Face();
    if (disk.IsNull()) return fail("Coil profile failed");

    BRepOffsetAPI_MakePipe pipe(spine, disk, GeomFill_IsCorrectedFrenet, Standard_False);
    pipe.Build();
    if (!pipe.IsDone()) return fail("Coil sweep build failed");

    TopoDS_Shape shape = placeOnFrame(pipe.Shape(), frame);
    if (shape.IsNull()) return fail("Coil placement failed");
    const gp_Pnt rim = frame.Location().Translated(gp_Vec(frame.XDirection()) * rHelix);
    setHandles(base, {{frame.Location(), frame.XDirection(), "diameter", dia, 0.5},
                      {rim, frame.XDirection(), "tubeDiameter", tube, 0.5},
                      {frame.Location(), frame.Direction(), "height", h},
                      {rim, frame.Direction(), "pitch", pitch}});
    return ctx.applyOperation(shape, operation(), list("targets"), id(), err) || fail(err);
}
