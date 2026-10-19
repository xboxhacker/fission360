// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#include "DrawingProjector.h"
#include "kernel/FissionDocument.h"
#include "kernel/FissionBody.h"

#include <BRep_Builder.hxx>
#include <BRep_Tool.hxx>
#include <BRepAdaptor_Curve.hxx>
#include <GCPnts_QuasiUniformDeflection.hxx>
#include <Geom_Curve.hxx>
#include <HLRAlgo_Projector.hxx>
#include <HLRBRep_Algo.hxx>
#include <HLRBRep_HLRToShape.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Compound.hxx>
#include <gp_Ax2.hxx>
#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>
#include <cmath>

namespace DrawingProjector {
namespace {

TopoDS_Shape compoundOf(FissionDocument *design) {
    if (!design) return {};
    BRep_Builder bb;
    TopoDS_Compound comp;
    bb.MakeCompound(comp);
    int n = 0;
    for (int i = 0; i < design->bodyCount(); ++i) {
        FissionBody *b = design->body(i);
        if (!b || b->shape().IsNull()) continue;
        bb.Add(comp, b->shape());
        ++n;
    }
    return n ? TopoDS_Shape(comp) : TopoDS_Shape();
}

void edgesToSegments(const TopoDS_Shape &s, const gp_Ax2 &ax, QVariantList &out) {
    if (s.IsNull()) return;
    const gp_Pnt o = ax.Location();
    const gp_Dir xd = ax.XDirection(), yd = ax.YDirection();
    for (TopExp_Explorer ex(s, TopAbs_EDGE); ex.More(); ex.Next()) {
        const TopoDS_Edge edge = TopoDS::Edge(ex.Current());
        if (BRep_Tool::Degenerated(edge)) continue;
        BRepAdaptor_Curve c(edge);
        const double f = c.FirstParameter(), l = c.LastParameter();
        if (l <= f) continue;
        const double chord = std::max(0.05, c.Value(l).Distance(c.Value(f)) * 0.02 + 0.05);
        GCPnts_QuasiUniformDeflection discret(c, chord);
        if (!discret.IsDone() || discret.NbPoints() < 2) {
            // Fallback: endpoints only
            Standard_Real a, b;
            Handle(Geom_Curve) gc = BRep_Tool::Curve(edge, a, b);
            if (gc.IsNull()) continue;
            const gp_Pnt p0 = gc->Value(a), p1 = gc->Value(b);
            const gp_XYZ d0 = p0.XYZ() - o.XYZ(), d1 = p1.XYZ() - o.XYZ();
            QVariantMap seg;
            seg["x1"] = d0.Dot(xd.XYZ());
            seg["y1"] = d0.Dot(yd.XYZ());
            seg["x2"] = d1.Dot(xd.XYZ());
            seg["y2"] = d1.Dot(yd.XYZ());
            out.append(seg);
            continue;
        }
        for (int i = 1; i < discret.NbPoints(); ++i) {
            const gp_Pnt p0 = discret.Value(i), p1 = discret.Value(i + 1);
            const gp_XYZ d0 = p0.XYZ() - o.XYZ(), d1 = p1.XYZ() - o.XYZ();
            QVariantMap seg;
            seg["x1"] = d0.Dot(xd.XYZ());
            seg["y1"] = d0.Dot(yd.XYZ());
            seg["x2"] = d1.Dot(xd.XYZ());
            seg["y2"] = d1.Dot(yd.XYZ());
            out.append(seg);
        }
    }
}

void boundsOf(const QVariantList &segs, double &w, double &h) {
    double loX = 1e300, loY = 1e300, hiX = -1e300, hiY = -1e300;
    for (const QVariant &v : segs) {
        const QVariantMap m = v.toMap();
        for (const char *k : {"x1", "x2"}) {
            const double x = m.value(k).toDouble();
            loX = std::min(loX, x);
            hiX = std::max(hiX, x);
        }
        for (const char *k : {"y1", "y2"}) {
            const double y = m.value(k).toDouble();
            loY = std::min(loY, y);
            hiY = std::max(hiY, y);
        }
    }
    if (loX > hiX) { w = h = 0; return; }
    w = hiX - loX;
    h = hiY - loY;
}

// Shift segments so the bbox centre is at the origin (views place their own frame).
void centreSegments(QVariantList &segs) {
    double loX = 1e300, loY = 1e300, hiX = -1e300, hiY = -1e300;
    for (const QVariant &v : segs) {
        const QVariantMap m = v.toMap();
        loX = std::min({loX, m.value("x1").toDouble(), m.value("x2").toDouble()});
        hiX = std::max({hiX, m.value("x1").toDouble(), m.value("x2").toDouble()});
        loY = std::min({loY, m.value("y1").toDouble(), m.value("y2").toDouble()});
        hiY = std::max({hiY, m.value("y1").toDouble(), m.value("y2").toDouble()});
    }
    if (loX > hiX) return;
    const double cx = 0.5 * (loX + hiX), cy = 0.5 * (loY + hiY);
    for (QVariant &v : segs) {
        QVariantMap m = v.toMap();
        m["x1"] = m.value("x1").toDouble() - cx;
        m["x2"] = m.value("x2").toDouble() - cx;
        m["y1"] = m.value("y1").toDouble() - cy;
        m["y2"] = m.value("y2").toDouble() - cy;
        v = m;
    }
}

}   // namespace

gp_Ax2 viewAxis(const QString &orientation) {
    // Looking toward -Z of the Ax2; X right, Y up on the sheet
    if (orientation == "top") return gp_Ax2(gp_Pnt(0, 0, 0), gp_Dir(0, 0, 1), gp_Dir(1, 0, 0));
    if (orientation == "bottom") return gp_Ax2(gp_Pnt(0, 0, 0), gp_Dir(0, 0, -1), gp_Dir(1, 0, 0));
    if (orientation == "right") return gp_Ax2(gp_Pnt(0, 0, 0), gp_Dir(1, 0, 0), gp_Dir(0, 0, 1));
    if (orientation == "left") return gp_Ax2(gp_Pnt(0, 0, 0), gp_Dir(-1, 0, 0), gp_Dir(0, 0, 1));
    if (orientation == "back") return gp_Ax2(gp_Pnt(0, 0, 0), gp_Dir(0, 1, 0), gp_Dir(-1, 0, 0));
    if (orientation == "home" || orientation == "isometric")
        return gp_Ax2(gp_Pnt(0, 0, 0), gp_Dir(1, 1, 1), gp_Dir(1, 0, -1));
    // front (default): looking along +Y toward -Y, X right, Z up
    return gp_Ax2(gp_Pnt(0, 0, 0), gp_Dir(0, -1, 0), gp_Dir(1, 0, 0));
}

Result projectShape(const TopoDS_Shape &shape, const QString &orientation, bool showHidden) {
    Result r;
    if (shape.IsNull()) { r.error = "Nothing to project"; return r; }
    const gp_Ax2 ax = viewAxis(orientation);
    try {
        Handle(HLRBRep_Algo) algo = new HLRBRep_Algo();
        algo->Add(shape);
        algo->Projector(HLRAlgo_Projector(ax));
        algo->Update();
        algo->Hide();
        HLRBRep_HLRToShape toShape(algo);
        edgesToSegments(toShape.VCompound(), ax, r.visible);
        edgesToSegments(toShape.Rg1LineVCompound(), ax, r.visible);
        edgesToSegments(toShape.OutLineVCompound(), ax, r.visible);
        if (showHidden) {
            edgesToSegments(toShape.HCompound(), ax, r.hidden);
            edgesToSegments(toShape.Rg1LineHCompound(), ax, r.hidden);
            edgesToSegments(toShape.OutLineHCompound(), ax, r.hidden);
        }
    } catch (const Standard_Failure &e) {
        r.error = QString("Projection failed: %1").arg(e.GetMessageString());
        return r;
    }
    centreSegments(r.visible);
    centreSegments(r.hidden);
    boundsOf(r.visible, r.width, r.height);
    if (r.width <= 0 && !r.hidden.isEmpty()) boundsOf(r.hidden, r.width, r.height);
    r.ok = !r.visible.isEmpty() || !r.hidden.isEmpty();
    if (!r.ok) r.error = "The projection produced no edges";
    return r;
}

Result project(FissionDocument *design, const QString &orientation, bool showHidden) {
    return projectShape(compoundOf(design), orientation, showHidden);
}

QString projectedOrientation(const QString &parentOrientation, const QString &direction,
                             const QString &projectionAngle) {
    // Sheet directions relative to the parent view. Third-angle (default) matches US practice;
    // first-angle flips the left/right and up/down pairs.
    const bool third = projectionAngle != QLatin1String("first");
    const QString dir = direction.toLower();
    auto flip = [&](const QString &a, const QString &b) { return third ? a : b; };

    if (parentOrientation == "front") {
        if (dir == "right") return flip("right", "left");
        if (dir == "left") return flip("left", "right");
        if (dir == "up") return flip("top", "bottom");
        if (dir == "down") return flip("bottom", "top");
    }
    if (parentOrientation == "top") {
        if (dir == "right") return flip("right", "left");
        if (dir == "left") return flip("left", "right");
        if (dir == "up") return flip("back", "front");
        if (dir == "down") return flip("front", "back");
    }
    if (parentOrientation == "right") {
        if (dir == "right") return flip("back", "front");
        if (dir == "left") return flip("front", "back");
        if (dir == "up") return flip("top", "bottom");
        if (dir == "down") return flip("bottom", "top");
    }
    if (parentOrientation == "left") {
        if (dir == "right") return flip("front", "back");
        if (dir == "left") return flip("back", "front");
        if (dir == "up") return flip("top", "bottom");
        if (dir == "down") return flip("bottom", "top");
    }
    if (parentOrientation == "back") {
        if (dir == "right") return flip("left", "right");
        if (dir == "left") return flip("right", "left");
        if (dir == "up") return flip("top", "bottom");
        if (dir == "down") return flip("bottom", "top");
    }
    if (parentOrientation == "bottom") {
        if (dir == "right") return flip("right", "left");
        if (dir == "left") return flip("left", "right");
        if (dir == "up") return flip("front", "back");
        if (dir == "down") return flip("back", "front");
    }
    return parentOrientation;
}

Result projectSection(FissionDocument *design, const QString &parentOrientation,
                      const QString &direction, double cutOffsetMm) {
    // Full HLR of the parent view, plus a cutting-plane line through the middle for the UI.
    Result r = project(design, parentOrientation, false);
    if (!r.ok) return r;
    const QString look = projectedOrientation(parentOrientation, direction, "third");
    Result lookProj = project(design, look, false);
    if (lookProj.ok) {
        r = lookProj;
        // Mark as section by adding a centre cutting line in view space
        QVariantMap cut;
        const double half = std::max(r.width, r.height) * 0.6;
        if (direction == "up" || direction == "down") {
            cut["x1"] = -half; cut["y1"] = cutOffsetMm; cut["x2"] = half; cut["y2"] = cutOffsetMm;
        } else {
            cut["x1"] = cutOffsetMm; cut["y1"] = -half; cut["x2"] = cutOffsetMm; cut["y2"] = half;
        }
        r.section.append(cut);
    }
    return r;
}

}   // namespace DrawingProjector
