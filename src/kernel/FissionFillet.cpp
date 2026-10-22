// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#include "FissionFillet.h"
#include "FissionChamfer.h"
#include "GeomUtils.h"
#include <BRepFilletAPI_MakeChamfer.hxx>
#include <BRepFilletAPI_MakeFillet.hxx>
#include <TopExp.hxx>
#include <TopTools_IndexedDataMapOfShapeListOfShape.hxx>
#include <TopoDS.hxx>
#include <QMap>
#include <cmath>

// Groups the referenced edges by body. With allEdges, every edge of `body` is used.
static bool collectEdges(RebuildContext &ctx, bool allEdges, int body, const QVariantList &refs,
                         QMap<int, QList<TopoDS_Edge>> &out, QString &error) {
    if (allEdges) {
        if (body < 0 || body >= ctx.bodies.size()) { error = "Body no longer exists"; return false; }
        const TopTools_IndexedMapOfShape map = GeomUtils::edgeMap(ctx.bodies[body].shape);
        for (int i = 1; i <= map.Extent(); ++i) out[body].append(TopoDS::Edge(map(i)));
        return true;
    }
    if (refs.isEmpty()) { error = "Select edges"; return false; }
    for (const QVariant &ref : refs) {
        int b = -1;
        TopoDS_Edge e;
        if (!ctx.resolveEdge(ref, b, e, error)) return false;
        bool dup = false;
        for (const auto &x : out[b]) dup = dup || x.IsSame(e);
        if (!dup) out[b].append(e);
    }
    return true;
}

// Drag handle on the first edge, pointing away from the material
static void edgeHandle(const RebuildContext &ctx, const QMap<int, QList<TopoDS_Edge>> &edges, gp_Pnt &origin, gp_Dir &dir) {
    const auto it = edges.constBegin();
    const TopoDS_Edge &e = it.value().first();
    origin = GeomUtils::edgeMidpoint(e);
    dir = GeomUtils::edgeOutwardBisector(ctx.bodies[it.key()].shape, e);
}

bool FissionFillet::compute(RebuildContext &ctx) {
    double radius = 0, radius2 = 0;
    if (!number(ctx, "radius", radius)) return false;
    if (radius <= 0) return fail("Radius must be positive");
    const bool variable = str("radiusType", "constant") == "variable";
    if (variable) {
        if (!number(ctx, "radius2", radius2)) return false;
        if (radius2 <= 0) return fail("End radius must be positive");
    }

    QString err;
    QMap<int, QList<TopoDS_Edge>> edges;
    if (!collectEdges(ctx, flag("allEdges"), integer("body"), list("edges"), edges, err)) return fail(err);
    if (!edges.isEmpty() && !edges.first().isEmpty()) {
        gp_Pnt o;
        gp_Dir d;
        edgeHandle(ctx, edges, o, d);
        setManipulator(o, d, "radius", radius);
    }

    for (auto it = edges.begin(); it != edges.end(); ++it) {
        BRepFilletAPI_MakeFillet fillet(ctx.bodies[it.key()].shape);
        for (const auto &e : it.value()) {
            // A variable fillet runs from the start radius to the end radius along the whole tangent chain
            if (!variable) fillet.Add(radius, e);
            else if (fillet.Contour(e) == 0) fillet.Add(radius, radius2, e);
        }
        fillet.Build();
        if (!fillet.IsDone()) return fail("Fillet failed (radius may be too large for the selected edges)");
        const TopoDS_Shape result = fillet.Shape();
        const double before = GeomUtils::volume(ctx.bodies[it.key()].shape);
        const double after = GeomUtils::volume(result);
        // MakeFillet reports success for a radius the edges cannot hold, and the shape it returns can be
        // a self-intersecting solid whose volume is orders of magnitude too large. A fillet only removes material.
        if (result.IsNull() || GeomUtils::solids(result).isEmpty() || !std::isfinite(after) || !(after > 1e-9)
            || after > before * 1.01)
            return fail("Fillet failed (radius may be too large for the selected edges)");
        ctx.bodies[it.key()].shape = result;
        ctx.bodies[it.key()].editedBy = id();
    }
    return true;
}

bool FissionChamfer::compute(RebuildContext &ctx) {
    const QString kind = str("chamferType", "equal");
    double d1 = 0, d2 = 0, angle = 45;
    if (!number(ctx, "distance", d1)) return false;
    if (kind == "twoDistances" && !number(ctx, "distance2", d2)) return false;
    if (kind == "distanceAngle" && !number(ctx, "angle", angle)) return false;
    if (d1 <= 0 || (kind == "twoDistances" && d2 <= 0)) return fail("Distance must be positive");
    if (kind == "distanceAngle" && (angle <= 0 || angle >= 90)) return fail("Angle must be between 0 and 90 degrees");

    QString err;
    QMap<int, QList<TopoDS_Edge>> edges;
    if (!collectEdges(ctx, flag("allEdges"), integer("body"), list("edges"), edges, err)) return fail(err);
    if (!edges.isEmpty() && !edges.first().isEmpty()) {
        gp_Pnt o;
        gp_Dir d;
        edgeHandle(ctx, edges, o, d);
        setManipulator(o, d, "distance", d1);
    }

    for (auto it = edges.begin(); it != edges.end(); ++it) {
        const TopoDS_Shape &shape = ctx.bodies[it.key()].shape;
        TopTools_IndexedDataMapOfShapeListOfShape edgeFaces;
        TopExp::MapShapesAndAncestors(shape, TopAbs_EDGE, TopAbs_FACE, edgeFaces);

        BRepFilletAPI_MakeChamfer chamfer(shape);
        for (const auto &e : it.value()) {
            if (kind == "equal") {
                chamfer.Add(d1, e);
                continue;
            }
            const TopTools_ListOfShape *faces = edgeFaces.Seek(e);
            if (!faces || faces->IsEmpty()) return fail("Chamfer edge has no adjacent face");
            const TopoDS_Face face = TopoDS::Face(faces->First());
            if (kind == "twoDistances") chamfer.Add(d1, d2, e, face);
            else chamfer.AddDA(d1, angle * M_PI / 180.0, e, face);
        }
        chamfer.Build();
        if (!chamfer.IsDone()) return fail("Chamfer failed (distance may be too large)");
        ctx.bodies[it.key()].shape = chamfer.Shape();
        ctx.bodies[it.key()].editedBy = id();
    }
    return true;
}
