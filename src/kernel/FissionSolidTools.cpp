// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#include "FissionSolidTools.h"
#include "FissionSketch.h"
#include "GeomUtils.h"
#include "MeshUtils.h"

#include <BRepAdaptor_CompCurve.hxx>
#include <BRepAdaptor_Curve.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepAlgoAPI_Common.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepAlgoAPI_Fuse.hxx>
#include <BRepAlgoAPI_Section.hxx>
#include <BRepBndLib.hxx>
#include <BRepBuilderAPI_GTransform.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepClass3d_SolidClassifier.hxx>
#include <BRepFeat_SplitShape.hxx>
#include <BRepOffsetAPI_DraftAngle.hxx>
#include <BRepOffsetAPI_MakeOffset.hxx>
#include <BRepOffsetAPI_MakeOffsetShape.hxx>
#include <BRepOffset_MakeOffset.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeHalfSpace.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <BRepTools.hxx>
#include <BRep_Tool.hxx>
#include <GCPnts_AbscissaPoint.hxx>
#include <GeomAPI_ProjectPointOnSurf.hxx>
#include <TopExp_Explorer.hxx>
#include <TopLoc_Location.hxx>
#include <TopoDS.hxx>
#include <gp_GTrsf.hxx>
#include <gp_Pln.hxx>
#include <algorithm>
#include <cmath>
#include <functional>

namespace {

bool readBodies(const RebuildContext &ctx, const QVariantList &refs, QList<int> &out, QString &err,
                bool emptyMeansAll) {
    for (const QVariant &v : refs) {
        const int i = v.toInt();
        if (i < 0 || i >= ctx.bodies.size()) { err = QString("Body %1 no longer exists").arg(i + 1); return false; }
        if (MeshUtils::isMesh(ctx.bodies[i].shape)) {
            err = ctx.bodies[i].name + " is a mesh body: use the MESH tools or Convert Mesh first";
            return false;
        }
        if (!out.contains(i)) out << i;
    }
    if (out.isEmpty() && emptyMeansAll)
        for (int i = 0; i < ctx.bodies.size(); ++i)
            if (!MeshUtils::isMesh(ctx.bodies[i].shape)) out << i;
    if (out.isEmpty()) { err = "Select one or more bodies"; return false; }
    std::sort(out.begin(), out.end());
    return true;
}

bool readFaces(const RebuildContext &ctx, const QVariantList &refs, int &body, QList<TopoDS_Face> &faces, QString &err) {
    body = -1;
    for (const QVariant &r : refs) {
        int b = -1;
        TopoDS_Face f;
        if (!ctx.resolveFace(r, b, f, err)) return false;
        if (body >= 0 && b != body) { err = "All faces must belong to the same body"; return false; }
        body = b;
        faces << f;
    }
    if (faces.isEmpty()) { err = "Select one or more faces"; return false; }
    return true;
}

TopoDS_Face bigPlane(const gp_Ax3 &ax, double size) {
    return BRepBuilderAPI_MakeFace(gp_Pln(ax), -size, size, -size, size).Face();
}

// Solid filling the side of the plane its normal points to
TopoDS_Shape halfSpace(const gp_Ax3 &ax, double size) {
    const gp_Pnt ref = ax.Location().Translated(gp_Vec(ax.Direction()));
    return BRepPrimAPI_MakeHalfSpace(bigPlane(ax, size), ref).Solid();
}

gp_Dir outwardNormal(const TopoDS_Face &f) {
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

// Replaces body `index` by the solids of `shape`; further lumps become new bodies right after it
void replaceBody(RebuildContext &ctx, int index, const TopoDS_Shape &shape, const QString &featureId) {
    const QList<TopoDS_Shape> parts = GeomUtils::solids(shape);
    if (parts.isEmpty()) { ctx.bodies.removeAt(index); return; }
    ctx.bodies[index].shape = parts.first();
    for (int k = 1; k < parts.size(); ++k) ctx.bodies.insert(index + k, {ctx.nextBodyName(), parts[k], featureId});
}

TopoDS_Shape prism(const TopoDS_Shape &base, const gp_Vec &v) {
    BRepPrimAPI_MakePrism mk(base, v, Standard_True);
    mk.Build();
    return mk.IsDone() ? mk.Shape() : TopoDS_Shape();
}

TopoDS_Shape translated(const TopoDS_Shape &s, const gp_Vec &v) {
    gp_Trsf t;
    t.SetTranslation(v);
    return BRepBuilderAPI_Transform(s, t, Standard_True).Shape();
}

TopoDS_Shape fuse(const TopoDS_Shape &a, const TopoDS_Shape &b) {
    BRepAlgoAPI_Fuse f(a, b);
    return f.IsDone() && !f.HasErrors() ? f.Shape() : TopoDS_Shape();
}
TopoDS_Shape cut(const TopoDS_Shape &a, const TopoDS_Shape &b) {
    BRepAlgoAPI_Cut c(a, b);
    return c.IsDone() && !c.HasErrors() ? c.Shape() : TopoDS_Shape();
}
TopoDS_Shape common(const TopoDS_Shape &a, const TopoDS_Shape &b) {
    BRepAlgoAPI_Common c(a, b);
    return c.IsDone() && !c.HasErrors() ? c.Shape() : TopoDS_Shape();
}

// Draft pull direction: a plane or flat face (its normal), or for non-planar references the axis of a
// cylindrical / conical / revolved face, a straight edge, a construction axis or a sketch line. The
// neutral plane goes through the reference, square to the pull.
bool resolvePull(const RebuildContext &ctx, const QVariant &ref, gp_Ax3 &out, QString &err) {
    if (ref.typeId() == QMetaType::QVariantMap && ref.toMap().value("kind").toString() == "face") {
        int b = -1;
        TopoDS_Face f;
        if (!ctx.resolveFace(ref, b, f, err)) return false;
        BRepAdaptor_Surface s(f);
        switch (s.GetType()) {
        case GeomAbs_Plane: break;
        case GeomAbs_Cylinder: out = gp_Ax3(s.Cylinder().Axis().Location(), s.Cylinder().Axis().Direction()); return true;
        case GeomAbs_Cone: out = gp_Ax3(s.Cone().Axis().Location(), s.Cone().Axis().Direction()); return true;
        case GeomAbs_SurfaceOfRevolution: out = gp_Ax3(s.AxeOfRevolution().Location(), s.AxeOfRevolution().Direction()); return true;
        default: err = "This face sets no pull direction: pick a flat or round face, an edge or an axis"; return false;
        }
        return ctx.resolvePlane(ref, out, err);
    }
    const bool axisLike = (ref.typeId() == QMetaType::QVariantMap && ref.toMap().value("kind").toString() == "edge")
                          || ref.toString() == "X" || ref.toString() == "Y" || ref.toString() == "Z"
                          || ref.toString().startsWith("sketchline:");
    if (ref.toString().startsWith("feature:")) {
        // A construction plane, sketch or construction axis
        if (ctx.resolvePlane(ref, out, err)) return true;
        gp_Ax1 a;
        QString axisErr;
        if (!ctx.resolveAxis(ref, a, axisErr)) return false;
        out = gp_Ax3(a.Location(), a.Direction());
        err.clear();
        return true;
    }
    if (axisLike) {
        gp_Ax1 a;
        if (!ctx.resolveAxis(ref, a, err)) return false;
        out = gp_Ax3(a.Location(), a.Direction());
        return true;
    }
    return ctx.resolvePlane(ref, out, err);
}

// Joint origin (Fusion's snap points): Z of `out` is the joint direction
bool resolveJointOrigin(const RebuildContext &ctx, const QVariant &ref, gp_Ax3 &out, int &body, QString &err) {
    const QVariantMap m = ref.toMap();
    const QString kind = m.value("kind").toString();
    if (kind == "edge") {
        TopoDS_Edge e;
        if (!ctx.resolveEdge(ref, body, e, err)) return false;
        BRepAdaptor_Curve c(e);
        if (c.GetType() == GeomAbs_Circle) { out = gp_Ax3(c.Circle().Position()); return true; }
        if (c.GetType() == GeomAbs_Ellipse) { out = gp_Ax3(c.Ellipse().Position()); return true; }
        gp_Pnt p;
        gp_Vec d;
        c.D1((c.FirstParameter() + c.LastParameter()) / 2, p, d);
        if (d.Magnitude() < 1e-12) { err = "This edge sets no direction"; return false; }
        out = gp_Ax3(p, gp_Dir(d));
        return true;
    }
    if (kind != "face") { err = "Pick a face or an edge"; return false; }
    TopoDS_Face f;
    if (!ctx.resolveFace(ref, body, f, err)) return false;
    const QVariantList hit = m.value("hit").toList();
    const bool hasHit = hit.size() == 3;
    gp_Pnt p = hasHit ? gp_Pnt(hit[0].toDouble(), hit[1].toDouble(), hit[2].toDouble()) : GeomUtils::faceCentroid(f);
    BRepAdaptor_Surface s(f);
    auto onAxis = [&](const gp_Ax1 &a) {
        const gp_Dir d = a.Direction();
        const double t = gp_Vec(a.Location(), p).Dot(gp_Vec(d));
        out = gp_Ax3(a.Location().Translated(gp_Vec(d) * t), d);
    };
    switch (s.GetType()) {
    case GeomAbs_Cylinder: onAxis(s.Cylinder().Axis()); return true;
    case GeomAbs_Cone: onAxis(s.Cone().Axis()); return true;
    case GeomAbs_Sphere: out = gp_Ax3(s.Sphere().Location(), outwardNormal(f)); return true;
    case GeomAbs_Torus: onAxis(s.Torus().Axis()); return true;
    case GeomAbs_Plane: {
        gp_Dir n = s.Plane().Axis().Direction();
        if (f.Orientation() == TopAbs_REVERSED) n.Reverse();
        out = gp_Ax3(p, n);
        return true;
    }
    default: break;
    }
    if (!hasHit) { out = gp_Ax3(p, outwardNormal(f)); return true; }
    GeomAPI_ProjectPointOnSurf proj(p, BRep_Tool::Surface(f));
    if (proj.NbPoints() == 0) { out = gp_Ax3(p, outwardNormal(f)); return true; }
    double u = 0, v = 0;
    proj.LowerDistanceParameters(u, v);
    gp_Pnt q;
    gp_Vec du, dv;
    s.D1(u, v, q, du, dv);
    const gp_Vec n = du.Crossed(dv);
    if (n.Magnitude() < 1e-12) { out = gp_Ax3(p, outwardNormal(f)); return true; }
    gp_Dir d(n);
    if (f.Orientation() == TopAbs_REVERSED) d.Reverse();
    out = gp_Ax3(q, d);
    return true;
}

// Meshes and very large converted bodies only get a new placement; B-rep geometry is moved for real
TopoDS_Shape placed(const TopoDS_Shape &src, const gp_Trsf &m) {
    int faces = 0;
    for (TopExp_Explorer e(src, TopAbs_FACE); e.More() && faces <= 2000; e.Next()) ++faces;
    return (MeshUtils::isMesh(src) || faces > 2000) ? src.Moved(TopLoc_Location(m))
                                                     : BRepBuilderAPI_Transform(src, m, Standard_True).Shape();
}

TopoDS_Shape allBodies(const RebuildContext &ctx) {
    QList<TopoDS_Shape> s;
    for (const auto &b : ctx.bodies)
        if (!MeshUtils::isMesh(b.shape)) s << b.shape;
    return GeomUtils::makeCompound(s);
}

}   // namespace

// ---------------------------------------------------------------------------

bool FissionRemove::compute(RebuildContext &ctx) {
    QList<int> idx;
    for (const QVariant &v : list("bodies")) {
        const int i = v.toInt();
        if (i < 0 || i >= ctx.bodies.size()) return fail(QString("Body %1 no longer exists").arg(i + 1));
        if (!idx.contains(i)) idx << i;
    }
    if (idx.isEmpty()) return fail("Select the bodies to remove");
    std::sort(idx.begin(), idx.end(), std::greater<int>());
    for (int i : idx) ctx.bodies.removeAt(i);
    return true;
}

bool FissionCombine::compute(RebuildContext &ctx) {
    const int target = integer("target", -1);
    if (target < 0 || target >= ctx.bodies.size()) return fail("Select the target body");
    QList<int> tools;
    for (const QVariant &v : list("tools")) {
        const int i = v.toInt();
        if (i == target) continue;
        if (i < 0 || i >= ctx.bodies.size()) return fail(QString("Body %1 no longer exists").arg(i + 1));
        if (!tools.contains(i)) tools << i;
    }
    if (tools.isEmpty()) return fail("Select one or more tool bodies");

    QList<TopoDS_Shape> toolShapes;
    for (int i : tools) toolShapes << ctx.bodies[i].shape;
    const TopoDS_Shape &t0 = ctx.bodies[target].shape;
    const QString op = str("operation", "join");
    TopoDS_Shape result;
    if (op == "join") {
        TopTools_ListOfShape args, tl;
        args.Append(t0);
        for (const auto &s : toolShapes) tl.Append(s);
        BRepAlgoAPI_Fuse f;
        f.SetArguments(args);
        f.SetTools(tl);
        f.Build();
        if (f.IsDone() && !f.HasErrors()) result = f.Shape();
    } else if (op == "cut") {
        result = cut(t0, GeomUtils::makeCompound(toolShapes));
    } else {
        result = common(t0, GeomUtils::makeCompound(toolShapes));
    }
    if (result.IsNull()) return fail("Combine failed");
    result = GeomUtils::unify(result);
    if (GeomUtils::solids(result).isEmpty()) return fail(op == "intersect" ? "The bodies do not overlap" : "Combine left nothing");

    int t = target;
    if (!flag("keepTools")) {
        std::sort(tools.begin(), tools.end(), std::greater<int>());
        for (int i : tools) {
            ctx.bodies.removeAt(i);
            if (i < t) --t;
        }
    }
    replaceBody(ctx, t, result, id());
    return true;
}

bool FissionMove::compute(RebuildContext &ctx) {
    QString err;
    QList<int> idx;
    for (const QVariant &b : list("bodies")) {
        const int i = b.toInt();
        if (i < 0 || i >= ctx.bodies.size()) return fail(QString("Body %1 no longer exists").arg(i + 1));
        if (!idx.contains(i)) idx << i;
    }
    if (idx.isEmpty()) return fail("Select one or more bodies");
    std::sort(idx.begin(), idx.end());

    auto vec = [&](const char *key, const char *what, gp_XYZ &out) {
        const QVariantList l = list(key);
        double v[3] = {0, 0, 0};
        for (int k = 0; k < 3 && k < l.size(); ++k) {
            v[k] = ctx.evaluate(l[k].toString(), &err);
            if (!err.isEmpty()) { err = QString(what) + ": " + err; return false; }
        }
        out.SetCoord(v[0], v[1], v[2]);
        return true;
    };
    const QString type = str("moveType", "");
    gp_Trsf m;
    gp_XYZ t;
    if (type == "free" || type == "translate" || type.isEmpty()) {
        if (!vec("translate", "Distance", t)) return fail(err);
        m.SetTranslation(gp_Vec(t));
    }
    if (type == "free") {
        gp_XYZ rot, pivot;
        if (!vec("rotation", "Angle", rot) || !vec("pivot", "Pivot", pivot)) return fail(err);
        const gp_Pnt p(pivot);
        gp_Trsf rx, ry, rz;
        rx.SetRotation(gp_Ax1(p, gp::DX()), rot.X() * M_PI / 180.0);
        ry.SetRotation(gp_Ax1(p, gp::DY()), rot.Y() * M_PI / 180.0);
        rz.SetRotation(gp_Ax1(p, gp::DZ()), rot.Z() * M_PI / 180.0);
        m.Multiply(rz);
        m.Multiply(ry);
        m.Multiply(rx);
    } else if (type == "rotate" || type.isEmpty()) {
        double angle = 0;
        if (!number(ctx, "angle", angle)) return false;
        if (std::fabs(angle) > 1e-12) {
            gp_Ax1 axis;
            if (!ctx.resolveAxis(get("axis"), axis, err)) return fail(err);
            gp_Trsf rot;
            rot.SetRotation(axis, angle * M_PI / 180.0);
            m.Multiply(rot);
        }
    } else if (type == "pointToPoint" || type == "pointToPosition") {
        gp_XYZ from, to;
        if (!vec("fromPoint", "From point", from)) return fail(err);
        if (!vec(type == "pointToPoint" ? "toPoint" : "translate", type == "pointToPoint" ? "To point" : "Position", to))
            return fail(err);
        m.SetTranslation(gp_Vec(to - from));
    } else if (type != "translate") {
        return fail("Unknown move type " + type);
    }

    const bool copy = flag("copy");
    for (int i : idx) {
        const TopoDS_Shape s = placed(ctx.bodies[i].shape, m);
        if (copy) ctx.bodies.append({ctx.nextBodyName(), s, id()});
        else ctx.bodies[i].shape = s;
    }
    return true;
}

bool FissionJoint::compute(RebuildContext &ctx) {
    QList<int> idx;
    for (const QVariant &b : list("bodies")) {
        const int i = b.toInt();
        if (i < 0 || i >= ctx.bodies.size()) return fail(QString("Body %1 no longer exists").arg(i + 1));
        if (!idx.contains(i)) idx << i;
    }
    if (idx.isEmpty()) return fail("Select the bodies that move (the component)");
    const bool asBuilt = flag("asBuilt");

    QString err;
    gp_Ax3 to;
    int toBody = -1;
    if (get("to").toMap().isEmpty()) return fail(asBuilt ? "Pick the joint origin" : "Pick the origin on the part it joins to");
    if (!resolveJointOrigin(ctx, get("to"), to, toBody, err)) return fail((asBuilt ? "Joint origin: " : "Target origin: ") + err);

    gp_Trsf place;
    if (!asBuilt) {
        if (idx.contains(toBody)) return fail("The target origin must be on a body that does not move");
        if (get("from").toMap().isEmpty()) return fail("Pick the origin on the moving part");
        gp_Ax3 from;
        int fromBody = -1;
        if (!resolveJointOrigin(ctx, get("from"), from, fromBody, err)) return fail("Moving origin: " + err);
        if (!idx.contains(fromBody)) return fail("The moving origin must be on one of the moving bodies");
        const gp_Dir z = flag("flip") ? to.Direction() : to.Direction().Reversed();
        place.SetDisplacement(from, gp_Ax3(to.Location(), z, to.XDirection()));
    }

    double angle = 0, offset = 0;
    if (!number(ctx, "angle", angle) || !number(ctx, "offset", offset)) return false;
    const gp_Ax1 axis(to.Location(), to.Direction());
    setManipulator(to.Location(), to.Direction(), "offset", offset);

    gp_Trsf m, rot;
    m.SetTranslation(gp_Vec(to.Direction()) * offset);
    rot.SetRotation(axis, angle * M_PI / 180.0);
    m.Multiply(rot);
    m.Multiply(place);
    for (int i : idx) ctx.bodies[i].shape = placed(ctx.bodies[i].shape, m);
    return true;
}

bool FissionScale::compute(RebuildContext &ctx) {
    QString err;
    QList<int> idx;
    if (!readBodies(ctx, list("bodies"), idx, err, false)) return fail(err);
    gp_Pnt p(0, 0, 0);
    if (!ctx.resolvePoint(get("point"), p, err)) return fail(err);
    if (str("scaleType", "uniform") == "uniform") {
        double s = 1;
        if (!number(ctx, "scale", s)) return false;
        if (s <= 1e-9) return fail("Scale factor must be positive");
        gp_Trsf t;
        t.SetScale(p, s);
        for (int i : idx) ctx.bodies[i].shape = BRepBuilderAPI_Transform(ctx.bodies[i].shape, t, Standard_True).Shape();
        return true;
    }
    double sx = 1, sy = 1, sz = 1;
    if (!number(ctx, "scaleX", sx) || !number(ctx, "scaleY", sy) || !number(ctx, "scaleZ", sz)) return false;
    if (sx <= 1e-9 || sy <= 1e-9 || sz <= 1e-9) return fail("Scale factors must be positive");
    gp_GTrsf g;
    g.SetVectorialPart(gp_Mat(sx, 0, 0, 0, sy, 0, 0, 0, sz));
    g.SetTranslationPart(gp_XYZ(p.X() * (1 - sx), p.Y() * (1 - sy), p.Z() * (1 - sz)));
    for (int i : idx) {
        BRepBuilderAPI_GTransform gt(ctx.bodies[i].shape, g, Standard_True);
        if (!gt.IsDone()) return fail("Non-uniform scale failed");
        ctx.bodies[i].shape = gt.Shape();
    }
    return true;
}

bool FissionSplitBody::compute(RebuildContext &ctx) {
    QString err;
    QList<int> idx;
    if (!readBodies(ctx, list("bodies"), idx, err, false)) return fail(err);
    const int toolBody = integer("toolBody", -1);
    TopoDS_Shape tool;
    if (toolBody >= 0) {
        if (toolBody >= ctx.bodies.size()) return fail("The splitting body no longer exists");
        tool = ctx.bodies[toolBody].shape;
    } else {
        gp_Ax3 ax;
        if (!ctx.resolvePlane(get("tool"), ax, err)) return fail(err);
        tool = halfSpace(ax, ctx.modelSize() * 10);
    }
    int split = 0;
    for (int k = idx.size() - 1; k >= 0; --k) {
        const int i = idx[k];
        if (i == toolBody) continue;
        const TopoDS_Shape inside = common(ctx.bodies[i].shape, tool);
        const TopoDS_Shape outside = cut(ctx.bodies[i].shape, tool);
        if (inside.IsNull() || outside.IsNull()) return fail("Split failed");
        const QList<TopoDS_Shape> parts = GeomUtils::solids(outside) + GeomUtils::solids(inside);
        if (parts.size() < 2) continue;
        replaceBody(ctx, i, GeomUtils::makeCompound(parts), id());
        ++split;
    }
    if (!split) return fail("The splitting tool does not cross the selected bodies");
    return true;
}

bool FissionSplitFace::compute(RebuildContext &ctx) {
    QString err;
    int body = -1;
    QList<TopoDS_Face> faces;
    if (!readFaces(ctx, list("faces"), body, faces, err)) return fail(err);
    gp_Ax3 ax;
    if (!ctx.resolvePlane(get("tool"), ax, err)) return fail(err);
    const TopoDS_Face plane = bigPlane(ax, ctx.modelSize() * 10);
    BRepFeat_SplitShape splitter(ctx.bodies[body].shape);
    int added = 0;
    for (const TopoDS_Face &f : faces) {
        BRepAlgoAPI_Section sec(f, plane, Standard_False);
        sec.ComputePCurveOn1(Standard_True);
        sec.Approximation(Standard_True);
        sec.Build();
        if (!sec.IsDone()) continue;
        for (TopExp_Explorer e(sec.Shape(), TopAbs_EDGE); e.More(); e.Next()) {
            splitter.Add(TopoDS::Edge(e.Current()), f);
            ++added;
        }
    }
    if (!added) return fail("The plane does not cross the selected faces");
    splitter.Build();
    if (!splitter.IsDone()) return fail("Split Face failed");
    ctx.bodies[body].shape = splitter.Shape();
    return true;
}

bool FissionOffsetFace::compute(RebuildContext &ctx) {
    QString err;
    double d = 0;
    if (!number(ctx, "distance", d)) return false;
    int body = -1;
    QList<TopoDS_Face> faces;
    if (!readFaces(ctx, list("faces"), body, faces, err)) return fail(err);
    // Press Pull handle: on the first face, along its outward normal
    setManipulator(GeomUtils::faceCentroid(faces.first()), outwardNormal(faces.first()), "distance", d);
    if (std::fabs(d) < 1e-9) return true;
    const TopoDS_Shape solid = ctx.bodies[body].shape;
    const double v0 = GeomUtils::volume(solid);

    // Exact: offset only these faces and let the neighbours extend (works for curved faces too)
    TopoDS_Shape result;
    try {
        BRepOffset_MakeOffset mo;
        mo.Initialize(solid, 0.0, 1e-5, BRepOffset_Skin, Standard_True, Standard_False, GeomAbs_Intersection);
        for (const TopoDS_Face &f : faces) mo.SetOffsetOnFace(f, d);
        mo.MakeOffsetShape();
        if (mo.IsDone() && !GeomUtils::solids(mo.Shape()).isEmpty() && BRepCheck_Analyzer(mo.Shape()).IsValid()) {
            const double v1 = GeomUtils::volume(mo.Shape());
            if (d > 0 ? v1 > v0 + 1e-9 : v1 < v0 - 1e-9) result = mo.Shape();
        }
    } catch (const Standard_Failure &) {
    }

    // Fallback for flat faces: add or remove a prism under each face
    if (result.IsNull()) {
        TopoDS_Shape r = solid;
        for (const TopoDS_Face &f : faces) {
            if (BRepAdaptor_Surface(f).GetType() != GeomAbs_Plane) return fail("Offset Face failed on a curved face");
            const gp_Vec n(outwardNormal(f));
            const TopoDS_Shape p = prism(f, n * d);
            if (p.IsNull()) return fail("Offset Face failed");
            r = d > 0 ? fuse(r, p) : cut(r, p);
            if (r.IsNull()) return fail("Offset Face failed");
        }
        result = GeomUtils::unify(r);
    }
    replaceBody(ctx, body, result, id());
    return true;
}

bool FissionOffset::compute(RebuildContext &ctx) {
    QString err;
    double d = 0;
    if (!number(ctx, "distance", d)) return false;
    const QVariantList faceRefs = list("faces");
    QList<TopoDS_Shape> sources;
    if (!faceRefs.isEmpty()) {
        for (const QVariant &r : faceRefs) {
            int b = -1;
            TopoDS_Face f;
            if (!ctx.resolveFace(r, b, f, err)) return fail(err);
            sources << f;
        }
    } else {
        QList<int> idx;
        if (!readBodies(ctx, list("bodies"), idx, err, false)) return fail("Select faces or bodies to offset");
        for (int i : idx) sources << ctx.bodies[i].shape;
    }
    for (const TopoDS_Shape &s : sources) {
        BRepOffsetAPI_MakeOffsetShape mk;
        mk.PerformByJoin(s, d, 1e-5);
        if (!mk.IsDone()) return fail("Offset failed (the distance may be too large for the curvature)");
        ctx.addBodies(mk.Shape(), id());
    }
    return true;
}

bool FissionReplaceFace::compute(RebuildContext &ctx) {
    QString err;
    int body = -1;
    QList<TopoDS_Face> faces;
    if (!readFaces(ctx, list("faces"), body, faces, err)) return fail(err);
    if (str("plane").isEmpty() && get("plane").typeId() != QMetaType::QVariantMap) return fail("Select the new face (a plane)");
    gp_Ax3 ax;
    if (!ctx.resolvePlane(get("plane"), ax, err)) return fail(err);
    const double L = ctx.modelSize() * 4;
    TopoDS_Shape r = ctx.bodies[body].shape;
    for (const TopoDS_Face &f : faces) {
        const gp_Dir n = outwardNormal(f);
        gp_Dir pn = ax.Direction();
        const double dot = pn.Dot(n);
        if (std::fabs(dot) < 1e-3) return fail("The new plane is parallel to the face's normal");
        if (dot < 0) pn.Reverse();
        const TopoDS_Shape beyond = halfSpace(gp_Ax3(ax.Location(), pn), L * 2);
        const TopoDS_Shape grow = cut(prism(f, gp_Vec(n) * L), beyond);
        const TopoDS_Shape column = prism(translated(f, gp_Vec(n) * -L), gp_Vec(n) * (2 * L));
        const TopoDS_Shape trim = common(column, beyond);
        if (grow.IsNull() || trim.IsNull()) return fail("Replace Face failed");
        r = cut(fuse(r, grow), trim);
        if (r.IsNull()) return fail("Replace Face failed");
    }
    replaceBody(ctx, body, GeomUtils::unify(r), id());
    return true;
}

bool FissionDraft::compute(RebuildContext &ctx) {
    QString err;
    int body = -1;
    QList<TopoDS_Face> faces;
    if (!readFaces(ctx, list("faces"), body, faces, err)) return fail(err);
    gp_Ax3 ax;
    if (!resolvePull(ctx, get("plane"), ax, err)) return fail(err);
    double angle = 0;
    if (!number(ctx, "angle", angle)) return false;
    const double rad = angle * M_PI / 180.0;
    gp_Dir dir = ax.Direction();
    if (flag("flip")) dir.Reverse();
    const gp_Pln neutral(ax.Location(), ax.Direction());

    auto draftFaces = [&](const TopoDS_Shape &shape, const QList<TopoDS_Face> &which, const gp_Dir &pull,
                          TopoDS_Shape &out) -> bool {
        BRepOffsetAPI_DraftAngle dr(shape);
        for (const TopoDS_Face &f : which) {
            dr.Add(f, pull, rad, neutral);
            if (!dr.AddDone()) { err = "Draft cannot be applied to one of the faces (is it parallel to the pull direction?)"; return false; }
        }
        dr.Build();
        if (!dr.IsDone()) { err = "Draft failed"; return false; }
        out = dr.Shape();
        return true;
    };

    TopoDS_Shape result;
    if (str("draftType", "face") != "parting") {
        if (!draftFaces(ctx.bodies[body].shape, faces, dir, result)) return fail(err);
    } else {
        // Split at the parting plane, draft each half away from it, then join the halves again
        const TopoDS_Shape hs = halfSpace(ax, ctx.modelSize() * 10);
        const TopoDS_Shape upper = common(ctx.bodies[body].shape, hs);
        const TopoDS_Shape lower = cut(ctx.bodies[body].shape, hs);
        if (upper.IsNull() || lower.IsNull()) return fail("Parting line draft failed");
        auto matching = [&](const TopoDS_Shape &half) {
            QList<TopoDS_Face> out;
            for (TopExp_Explorer e(half, TopAbs_FACE); e.More(); e.Next()) {
                const TopoDS_Face hf = TopoDS::Face(e.Current());
                const gp_Pnt c = GeomUtils::faceCentroid(hf);
                for (const TopoDS_Face &sel : faces) {
                    GeomAPI_ProjectPointOnSurf proj(c, BRep_Tool::Surface(sel));
                    if (proj.NbPoints() && proj.LowerDistance() < 1e-5 && outwardNormal(hf).IsParallel(outwardNormal(sel), 1e-3)) {
                        out << hf;
                        break;
                    }
                }
            }
            return out;
        };
        TopoDS_Shape a = upper, b = lower;
        const QList<TopoDS_Face> fa = matching(upper), fb = matching(lower);
        if (!fa.isEmpty() && !draftFaces(upper, fa, ax.Direction(), a)) return fail(err);
        if (!fb.isEmpty() && !draftFaces(lower, fb, ax.Direction().Reversed(), b)) return fail(err);
        result = fuse(a, b);
        if (result.IsNull()) return fail("Parting line draft failed");
        result = GeomUtils::unify(result);
    }
    replaceBody(ctx, body, result, id());
    return true;
}

bool FissionPathPattern::transforms(RebuildContext &ctx, QList<gp_Trsf> &out) {
    auto *sk = qobject_cast<FissionSketch *>(ctx.feature(str("path")));
    if (!sk) return fail("Select a path (a sketch curve)");
    QString err;
    const TopoDS_Wire w = sk->pathWire(err);
    if (w.IsNull()) return fail(err);
    BRepAdaptor_CompCurve c(w);
    const double total = GCPnts_AbscissaPoint::Length(c);
    double count = 0;
    if (!number(ctx, "count", count)) return false;
    const int n = int(std::lround(count));
    if (n < 2 || n > 1000) return fail("Quantity must be between 2 and 1000");
    double dist = total;
    if (!str("distance").trimmed().isEmpty() && !number(ctx, "distance", dist)) return false;
    dist = std::min(std::fabs(dist), total);
    const double u0 = c.FirstParameter();
    gp_Pnt p0;
    gp_Vec t0;
    c.D1(u0, p0, t0);
    const bool follow = str("orientation") == "path";
    for (int i = 1; i < n; ++i) {
        GCPnts_AbscissaPoint ap(c, dist * i / (n - 1), u0);
        if (!ap.IsDone()) return fail("Could not place an instance on the path");
        gp_Pnt p;
        gp_Vec t;
        c.D1(ap.Parameter(), p, t);
        gp_Trsf tr;
        tr.SetTranslation(gp_Vec(p0, p));
        if (follow) {
            const gp_Vec axis = t0.Crossed(t);
            if (axis.Magnitude() > 1e-9 * t0.Magnitude() * t.Magnitude()) {
                gp_Trsf r;
                r.SetRotation(gp_Ax1(p0, gp_Dir(axis)), t0.AngleWithRef(t, axis));
                tr.Multiply(r);
            }
        }
        out << tr;
    }
    return true;
}

bool FissionRib::compute(RebuildContext &ctx) {
    auto *sk = qobject_cast<FissionSketch *>(ctx.feature(str("sketch")));
    if (!sk) return fail("Select a sketch with the curves for the " + type().toLower());
    double t = 0;
    if (!number(ctx, "thickness", t)) return false;
    if (t <= 0) return fail("Thickness must be positive");
    const bool toNext = str("extent", "toNext") == "toNext";
    const double L = ctx.modelSize() * 4;
    double depth = L;
    if (!toNext && !number(ctx, "depth", depth)) return false;
    if (depth <= 0) return fail("Depth must be positive");

    const gp_Ax3 pl = sk->plane();
    const gp_Dir n = pl.Direction();
    gp_Dir dir = n;
    if (!bothSides() && !ctx.bodies.isEmpty()) {
        Bnd_Box all;
        for (const auto &b : ctx.bodies) all.Add(GeomUtils::boundingBox(b.shape));
        double x0, y0, z0, x1, y1, z1;
        all.Get(x0, y0, z0, x1, y1, z1);
        const gp_Pnt c((x0 + x1) / 2, (y0 + y1) / 2, (z0 + z1) / 2);
        if (gp_Vec(pl.Location(), c).Dot(gp_Vec(n)) < 0) dir.Reverse();
    }
    if (flag("flip")) dir.Reverse();

    const QVariantList ents = sk->get("entities").toList();
    const auto &entityEdges = sk->entityEdges();
    QList<TopoDS_Shape> tools;
    QList<gp_Pnt> seeds;
    Bnd_Box limit;   // walls never run past the bodies or the sketch curves
    for (const auto &b : ctx.bodies) {
        if (MeshUtils::isMesh(b.shape)) limit.Add(GeomUtils::boundingBox(b.shape));
        else BRepBndLib::AddOptimal(b.shape, limit, Standard_False, Standard_False);
    }
    for (int i = 0; i < entityEdges.size(); ++i) {
        if (i < ents.size() && ents[i].toMap().value("construction").toBool()) continue;
        for (const TopoDS_Edge &e : entityEdges[i]) {
            Bnd_Box eb;
            BRepBndLib::AddOptimal(e, eb, Standard_False, Standard_False);
            double a0, b0, c0, a1, b1, c1;
            eb.Get(a0, b0, c0, a1, b1, c1);
            limit.Update(a0 - t / 2, b0 - t / 2, c0 - t / 2, a1 + t / 2, b1 + t / 2, c1 + t / 2);
            BRepAdaptor_Curve bc(e);
            TopoDS_Face strip;
            if (bc.GetType() == GeomAbs_Line) {
                gp_Pnt a = bc.Value(bc.FirstParameter()), b = bc.Value(bc.LastParameter());
                gp_Vec ld(a, b);
                if (ld.Magnitude() < 1e-9) continue;
                ld.Normalize();
                // Ends run on until they meet the body walls
                if (toNext) { a.Translate(ld * -L); b.Translate(ld * L); }
                const gp_Vec side = gp_Vec(n).Crossed(ld) * (t / 2);
                BRepBuilderAPI_MakePolygon poly(a.Translated(side), b.Translated(side), b.Translated(-side),
                                                a.Translated(-side), Standard_True);
                strip = BRepBuilderAPI_MakeFace(poly.Wire(), Standard_True).Face();
            } else {
                BRepOffsetAPI_MakeOffset off(BRepBuilderAPI_MakeWire(e).Wire(), GeomAbs_Arc, Standard_False);
                off.Perform(t / 2);
                if (!off.IsDone()) return fail("Could not thicken a curve");
                strip = BRepBuilderAPI_MakeFace(TopoDS::Wire(off.Shape()), Standard_True).Face();
            }
            const gp_Pnt mid = bc.Value((bc.FirstParameter() + bc.LastParameter()) / 2);
            TopoDS_Shape tool;
            if (bothSides()) {
                tool = prism(translated(strip, gp_Vec(n) * -depth), gp_Vec(n) * (2 * depth));
                seeds << mid;
            } else {
                tool = prism(strip, gp_Vec(dir) * depth);
                seeds << mid.Translated(gp_Vec(dir) * std::min(0.05, depth / 4));
            }
            if (tool.IsNull()) return fail("Could not build the " + type().toLower());
            tools << tool;
        }
    }
    if (tools.isEmpty()) return fail("The sketch has no curves");

    QList<TopoDS_Shape> kept;
    if (toNext && !ctx.bodies.isEmpty()) {
        // Keep only the part of each wall between the curve and the body
        const TopoDS_Shape bodies = allBodies(ctx);
        double x0, y0, z0, x1, y1, z1;
        limit.Get(x0, y0, z0, x1, y1, z1);
        const TopoDS_Shape box = BRepPrimAPI_MakeBox(gp_Pnt(x0, y0, z0), gp_Pnt(x1, y1, z1)).Shape();
        for (int k = 0; k < tools.size(); ++k) {
            const TopoDS_Shape outside = cut(common(tools[k], box), bodies);
            if (outside.IsNull()) return fail(type() + " failed");
            for (const TopoDS_Shape &piece : GeomUtils::solids(outside)) {
                BRepClass3d_SolidClassifier cls(piece, seeds[k], 1e-6);
                if (cls.State() == TopAbs_IN || cls.State() == TopAbs_ON) kept << piece;
            }
        }
        if (kept.isEmpty()) return fail("The " + type().toLower() + " does not reach a body; use a distance");
    } else {
        kept = tools;
    }
    QString err;
    if (!ctx.applyOperation(GeomUtils::makeCompound(kept), FeatureOperation::Join, {}, id(), err)) return fail(err);
    return true;
}
