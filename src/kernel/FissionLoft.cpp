// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#include "FissionLoft.h"
#include "FissionSketch.h"
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepOffsetAPI_MakePipeShell.hxx>
#include <BRepOffsetAPI_ThruSections.hxx>
#include <BRepTools.hxx>
#include <GeomAPI_Interpolate.hxx>
#include <Geom_BSplineCurve.hxx>
#include <TColgp_HArray1OfPnt.hxx>
#include <TopoDS_Wire.hxx>

// Smooth curve through the section centres, used as the spine when only a rail guides the loft
static TopoDS_Wire centreSpine(const QList<TopoDS_Wire> &wires, QString &error) {
    Handle(TColgp_HArray1OfPnt) pts = new TColgp_HArray1OfPnt(1, int(wires.size()));
    for (int i = 0; i < wires.size(); ++i) {
        GProp_GProps props;
        BRepGProp::LinearProperties(wires[i], props);
        pts->SetValue(i + 1, props.CentreOfMass());
    }
    for (int i = 2; i <= pts->Length(); ++i)
        if (pts->Value(i).Distance(pts->Value(i - 1)) < 1e-6) {
            error = "Two sections share a centre; add a centerline";
            return {};
        }
    GeomAPI_Interpolate interp(pts, Standard_False, 1e-6);
    interp.Perform();
    if (!interp.IsDone()) {
        error = "No spine fits through the section centres";
        return {};
    }
    return BRepBuilderAPI_MakeWire(BRepBuilderAPI_MakeEdge(interp.Curve()).Edge()).Wire();
}

bool FissionLoft::compute(RebuildContext &ctx) {
    const QVariantList sections = list("sections");
    if (sections.size() < 2) return fail("Select at least two profiles or faces");

    QList<TopoDS_Wire> wires;
    for (int i = 0; i < sections.size(); ++i) {
        const QVariantMap s = sections[i].toMap();
        const QString label = QString("Section %1").arg(i + 1);
        TopoDS_Wire wire;
        if (s.value("kind").toString() == "face") {
            int body = -1;
            TopoDS_Face face;
            QString err;
            if (!ctx.resolveFace(s, body, face, err)) return fail(label + ": " + err);
            wire = BRepTools::OuterWire(face);
        } else {
            auto *sketch = qobject_cast<FissionSketch *>(ctx.feature(s.value("sketch").toString()));
            if (!sketch) return fail(label + ": sketch not found");
            if (!sketch->isHealthy()) return fail(label + ": sketch '" + sketch->name() + "' has errors");
            int profile = s.value("profile", -1).toInt();
            if (profile < 0) profile = sketch->defaultProfiles().isEmpty() ? -1 : sketch->defaultProfiles().first();
            if (profile < 0 || profile >= sketch->profileCount())
                return fail(label + ": sketch '" + sketch->name() + "' has no such profile");
            wire = BRepTools::OuterWire(sketch->profiles()[profile]);
        }
        if (wire.IsNull()) return fail(label + " has no closed outline");
        wires << wire;
    }

    // Guide curves: every curve of the picked sketch, chained (like a sweep path)
    const QString guide = str("guide", "none");
    auto guideWire = [&](const QString &key, const QString &what, TopoDS_Wire &out) {
        auto *sk = qobject_cast<FissionSketch *>(ctx.feature(str(key)));
        if (!sk) return fail("Select a " + what + " sketch");
        for (const QVariant &v : sections)
            if (v.toMap().value("sketch").toString() == sk->id())
                return fail("The " + what + " must be in its own sketch, not a section's");
        QString err;
        out = sk->pathWire(err);
        if (out.IsNull()) return fail(what + ": " + err);
        return true;
    };

    TopoDS_Shape result;
    if (guide == "rail" || guide == "centerline") {
        TopoDS_Wire spine, rail;
        QString err;
        if (guide == "centerline") {
            if (!guideWire("centerline", "centerline", spine)) return false;
        } else {
            if (!guideWire("rail", "rail", rail)) return false;
            spine = centreSpine(wires, err);
            if (spine.IsNull()) return fail(err);
        }
        BRepOffsetAPI_MakePipeShell ps(spine);
        // On a rail the sections are turned and scaled so their outline keeps touching it
        if (!rail.IsNull()) ps.SetMode(rail, Standard_False, BRepFill_ContactOnBorder);
        for (const TopoDS_Wire &w : wires) ps.Add(w, Standard_False, Standard_False);
        ps.Build();
        if (!ps.IsDone()) return fail("Loft failed (the " + guide + " must run through every section, in order)");
        if (flag("solid", true)) ps.MakeSolid();
        result = ps.Shape();
    } else {
        BRepOffsetAPI_ThruSections loft(flag("solid", true), flag("ruled"), 1e-6);
        for (const TopoDS_Wire &w : wires) loft.AddWire(w);
        loft.CheckCompatibility(Standard_True);
        loft.Build();
        if (!loft.IsDone()) return fail("Loft failed (profiles may be incompatible)");
        result = loft.Shape();
    }

    QString err;
    if (!ctx.applyOperation(result, operation(), list("targets"), id(), err)) return fail(err);
    return true;
}
