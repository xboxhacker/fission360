// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#include "FissionSolidTools.h"
#include "FissionSketch.h"
#include "GeomUtils.h"
#include "TimeLimit.h"
#include "MeshUtils.h"

#include <BRepAdaptor_CompCurve.hxx>
#include <BRepAdaptor_Curve.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepAlgoAPI_Common.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepAlgoAPI_Defeaturing.hxx>
#include <BRepAlgoAPI_Fuse.hxx>
#include <BRepAlgoAPI_Section.hxx>
#include <BRepFilletAPI_MakeChamfer.hxx>
#include <BRepBndLib.hxx>
#include <BRepBuilderAPI_Copy.hxx>
#include <BRepBuilderAPI_GTransform.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <gp_Trsf.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepClass3d_SolidClassifier.hxx>
#include <BRepFeat_SplitShape.hxx>
#include <BRepOffsetAPI_DraftAngle.hxx>
#include <IntAna_QuadQuadGeo.hxx>
#include <BRepOffsetAPI_MakeOffset.hxx>
#include <BRepOffsetAPI_MakeOffsetShape.hxx>
#include <BRepOffsetAPI_MakeThickSolid.hxx>
#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <BRepOffset_MakeOffset.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeHalfSpace.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <BRepLib.hxx>
#include <BRepTools.hxx>
#include <memory>
#include <QMap>
#include <BRep_Tool.hxx>
#include <GCPnts_AbscissaPoint.hxx>
#include <GeomAPI_ProjectPointOnSurf.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_IndexedDataMapOfShapeListOfShape.hxx>
#include <TopTools_ListIteratorOfListOfShape.hxx>
#include <TopTools_ListOfShape.hxx>
#include <TopLoc_Location.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Solid.hxx>
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

} // namespace

bool primitiveFrame(RebuildContext &ctx, const QVariant &planeRef, const QVariantList &origin,
                    gp_Ax3 &placed, QString &error, gp_Ax3 *base) {
    const bool missing = !planeRef.isValid() || planeRef.isNull()
        || (planeRef.typeId() == QMetaType::QString && planeRef.toString().trimmed().isEmpty());
    if (missing) { error = "Select a plane or a flat face"; return false; }
    gp_Ax3 plane;
    if (!ctx.resolvePlane(planeRef, plane, error)) return false;
    auto coord = [&](int i) {
        if (i >= origin.size()) return 0.0;
        const QVariant v = origin.at(i);
        if (v.typeId() == QMetaType::QString) {
            QString err;
            const double n = ctx.evaluate(v.toString(), &err);
            return err.isEmpty() ? n : 0.0;
        }
        return v.toDouble();
    };
    if (base) *base = plane;
    gp_Pnt loc = plane.Location();
    loc.Translate(gp_Vec(plane.XDirection()) * coord(0) + gp_Vec(plane.YDirection()) * coord(1)
                  + gp_Vec(plane.Direction()) * coord(2));
    placed = gp_Ax3(loc, plane.Direction(), plane.XDirection());
    return true;
}

TopoDS_Shape placeOnFrame(const TopoDS_Shape &shape, const gp_Ax3 &frame) {
    gp_Trsf trsf;
    trsf.SetDisplacement(gp_Ax3(), frame);
    return BRepBuilderAPI_Transform(shape, trsf, Standard_True).Shape();
}

namespace {

// Replaces body `index` by the solids of `shape`; further lumps become new bodies right after it
void replaceBody(RebuildContext &ctx, int index, const TopoDS_Shape &shape, const QString &featureId) {
    const QList<TopoDS_Shape> parts = GeomUtils::solids(shape);
    if (parts.isEmpty()) { ctx.bodies.removeAt(index); return; }
    ctx.bodies[index].shape = parts.first();
    ctx.bodies[index].editedBy = featureId;
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
        f.SetRunParallel(Standard_True);
        Handle(GeomUtils::TimeLimit) limit = new GeomUtils::TimeLimit(20000);
        f.Build(limit->Start());
        if (limit->expired())
            return fail("Combine took longer than 20 s and was stopped (try fewer tools, or Join as New Body)");
        if (f.IsDone() && !f.HasErrors()) result = f.Shape();
    } else if (op == "cut") {
        BRepAlgoAPI_Cut c;
        TopTools_ListOfShape args, tl;
        args.Append(t0);
        tl.Append(GeomUtils::makeCompound(toolShapes));
        c.SetArguments(args);
        c.SetTools(tl);
        c.SetRunParallel(Standard_True);
        Handle(GeomUtils::TimeLimit) limit = new GeomUtils::TimeLimit(20000);
        c.Build(limit->Start());
        if (limit->expired())
            return fail("Combine took longer than 20 s and was stopped");
        if (c.IsDone() && !c.HasErrors()) result = c.Shape();
    } else {
        BRepAlgoAPI_Common c;
        TopTools_ListOfShape args, tl;
        args.Append(t0);
        tl.Append(GeomUtils::makeCompound(toolShapes));
        c.SetArguments(args);
        c.SetTools(tl);
        c.SetRunParallel(Standard_True);
        Handle(GeomUtils::TimeLimit) limit = new GeomUtils::TimeLimit(20000);
        c.Build(limit->Start());
        if (limit->expired())
            return fail("Combine took longer than 20 s and was stopped");
        if (c.IsDone() && !c.HasErrors()) result = c.Shape();
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

// Analytic distance only. A generic point-to-surface projection can spin forever on a cone.
double distanceToSurface(const TopoDS_Face &face, const gp_Pnt &world) {
    BRepAdaptor_Surface s(face);
    switch (s.GetType()) {
    case GeomAbs_Plane:
        return s.Plane().Distance(world);
    case GeomAbs_Cylinder: {
        const gp_Cylinder c = s.Cylinder();
        const gp_Vec fromAxis(c.Location(), world);
        const gp_Vec axis(c.Axis().Direction());
        const gp_Vec radial = fromAxis - axis * fromAxis.Dot(axis);
        return std::abs(radial.Magnitude() - c.Radius());
    }
    default:
        return 1e100;
    }
}

double distanceToFaces(const QList<TopoDS_Face> &faces, const gp_Pnt &world) {
    double best = 1e100;
    for (const TopoDS_Face &f : faces) best = std::min(best, distanceToSurface(f, world));
    return best;
}

// Seam-split halves of one wall share a surface. A plane and a cylinder do not.
bool sameSurface(const TopoDS_Face &a, const TopoDS_Face &b) {
    const BRepAdaptor_Surface sa(a), sb(b);
    if (sa.GetType() != sb.GetType()) return false;
    if (sa.GetType() == GeomAbs_Plane) {
        if (!sa.Plane().Axis().Direction().IsParallel(sb.Plane().Axis().Direction(), 1e-4)) return false;
        return std::abs(sa.Plane().Distance(sb.Plane().Location())) < 1e-4;
    }
    if (sa.GetType() == GeomAbs_Cylinder) {
        const gp_Cylinder ca = sa.Cylinder(), cb = sb.Cylinder();
        if (std::abs(ca.Radius() - cb.Radius()) > 1e-4) return false;
        if (!ca.Axis().Direction().IsParallel(cb.Axis().Direction(), 1e-4)) return false;
        const gp_Vec gap(ca.Location(), cb.Location());
        const gp_Vec axis(ca.Axis().Direction());
        return (gap - axis * gap.Dot(axis)).Magnitude() < 1e-3;
    }
    TopLoc_Location la, lb;
    const Handle(Geom_Surface) ua = BRep_Tool::Surface(a, la);
    const Handle(Geom_Surface) ub = BRep_Tool::Surface(b, lb);
    return !ua.IsNull() && ua == ub && la.IsEqual(lb);
}

double polarAngle(const gp_Ax3 &ax, const gp_Pnt &p) {
    const gp_Vec d(ax.Location(), p);
    const gp_Vec radial = d - gp_Vec(ax.Direction()) * d.Dot(gp_Vec(ax.Direction()));
    return std::atan2(radial.Dot(gp_Vec(ax.YDirection())), radial.Dot(gp_Vec(ax.XDirection())));
}

// Normal at a point on the face. Face midpoints of a split wall point different ways.
gp_Dir normalNear(const TopoDS_Face &face, const gp_Pnt &world) {
    const BRepAdaptor_Surface s(face);
    double u = 0, v = 0;
    bool have = false;
    if (s.GetType() == GeomAbs_Plane) {
        const gp_Vec d(s.Plane().Location(), world);
        u = d.Dot(gp_Vec(s.Plane().XAxis().Direction()));
        v = d.Dot(gp_Vec(s.Plane().YAxis().Direction()));
        have = true;
    } else if (s.GetType() == GeomAbs_Cylinder) {
        const gp_Vec d(s.Cylinder().Location(), world);
        v = d.Dot(gp_Vec(s.Cylinder().Axis().Direction()));
        u = polarAngle(s.Cylinder().Position(), world);
        have = true;
    } else if (s.GetType() == GeomAbs_Cone) {
        const double sa = std::sin(s.Cone().SemiAngle());
        if (std::abs(sa) > 1e-6) {
            const gp_Vec d(s.Cone().Location(), world);
            const gp_Vec axis(s.Cone().Axis().Direction());
            const gp_Vec radial = d - axis * d.Dot(axis);
            u = polarAngle(s.Cone().Position(), world);
            v = (radial.Magnitude() - s.Cone().RefRadius()) / sa;
            have = true;
        }
    }
    if (!have) return outwardNormal(face);
    gp_Pnt p;
    gp_Vec du, dv;
    s.D1(u, v, p, du, dv);
    const gp_Vec n = du.Crossed(dv);
    if (n.Magnitude() < 1e-12) return outwardNormal(face);
    gp_Dir dir(n);
    if (face.Orientation() == TopAbs_REVERSED) dir.Reverse();
    return dir;
}

TopoDS_Face faceOn(const QList<TopoDS_Face> &faces, const gp_Pnt &world) {
    for (const TopoDS_Face &f : faces)
        if (distanceToSurface(f, world) <= 1e-3) return f;
    return faces.first();
}

// A round chamfer is a cone between two faces. Pulling it changes those two legs, and a pull
// that reaches the corner takes the chamfer off.
// notChamfer is set when a face is a cone that does not run between two walls (a taper, a countersink):
// those are offset like any other face.
bool pullChamferFaces(const TopoDS_Shape &solid, const QList<TopoDS_Face> &chamfers, double distance,
                      TopoDS_Shape &out, QString &error, bool &notChamfer) {
    notChamfer = false;
    struct Job {
        QList<TopoDS_Face> faces;   // both halves when the cone is split on a seam
        QList<TopoDS_Face> sideA;   // one wall, including seam-split halves
        QList<TopoDS_Face> sideB;
        double legA = 0;
        double legB = 0;
        gp_Pnt center;
        gp_Pnt onA;
        gp_Pnt onB;
        TopoDS_Face coneOnA;
        TopoDS_Face coneOnB;
    };
    QList<Job> jobs;
    TopTools_IndexedDataMapOfShapeListOfShape edgeFaces;
    TopExp::MapShapesAndAncestors(solid, TopAbs_EDGE, TopAbs_FACE, edgeFaces);

    auto adjacent = [&](const TopoDS_Face &face) {
        QList<TopoDS_Face> out;
        for (TopExp_Explorer ex(face, TopAbs_EDGE); ex.More(); ex.Next()) {
            const TopTools_ListOfShape *adj = edgeFaces.Seek(ex.Current());
            if (!adj) continue;
            for (TopTools_ListIteratorOfListOfShape it(*adj); it.More(); it.Next()) {
                const TopoDS_Face other = TopoDS::Face(it.Value());
                if (other.IsSame(face)) continue;
                bool seen = false;
                for (const TopoDS_Face &n : out)
                    if (n.IsSame(other)) seen = true;
                if (!seen) out.append(other);
            }
        }
        return out;
    };
    auto listed = [](const QList<TopoDS_Face> &faces, const TopoDS_Face &face) {
        for (const TopoDS_Face &n : faces)
            if (n.IsSame(face)) return true;
        return false;
    };

    QList<TopoDS_Face> claimed;
    for (const TopoDS_Face &chamfer : chamfers) {
        if (listed(claimed, chamfer)) continue;
        if (BRepAdaptor_Surface(chamfer).GetType() != GeomAbs_Cone) {
            error = QStringLiteral("Only a round chamfer can be pulled this way");
            notChamfer = true;
            return false;
        }
        // A circular chamfer is often two cone faces meeting at a seam. Pull both.
        QList<TopoDS_Face> group;
        group.append(chamfer);
        for (int i = 0; i < group.size() && group.size() < 8; ++i) {
            for (const TopoDS_Face &other : adjacent(group[i])) {
                if (BRepAdaptor_Surface(other).GetType() != GeomAbs_Cone || listed(group, other)) continue;
                group.append(other);
            }
        }
        struct SideGroup { QList<TopoDS_Face> faces; double length = 0; };
        QList<SideGroup> groups;
        for (const TopoDS_Face &part : group) {
            claimed.append(part);
            for (const TopoDS_Face &other : adjacent(part)) {
                if (BRepAdaptor_Surface(other).GetType() == GeomAbs_Cone) continue;
                int slot = -1;
                for (int i = 0; i < groups.size(); ++i)
                    if (sameSurface(groups[i].faces.first(), other)) slot = i;
                if (slot < 0) {
                    groups.append(SideGroup{});
                    slot = groups.size() - 1;
                }
                if (!listed(groups[slot].faces, other)) groups[slot].faces.append(other);
            }
        }
        // The two long boundaries are the walls the chamfer runs between. A flat that only
        // ends the chamfer shares a short edge and is not a third wall.
        for (const TopoDS_Face &part : group) {
            for (TopExp_Explorer ex(part, TopAbs_EDGE); ex.More(); ex.Next()) {
                const TopoDS_Edge edge = TopoDS::Edge(ex.Current());
                if (BRep_Tool::Degenerated(edge)) continue;
                const gp_Pnt mid = GeomUtils::edgeMidpoint(edge);
                const double len = GCPnts_AbscissaPoint::Length(BRepAdaptor_Curve(edge));
                int best = -1;
                double bestD = 1e-3;
                for (int i = 0; i < groups.size(); ++i) {
                    const double d = distanceToFaces(groups[i].faces, mid);
                    if (d < bestD) { bestD = d; best = i; }
                }
                if (best >= 0) groups[best].length += len;
            }
        }
        std::sort(groups.begin(), groups.end(), [](const SideGroup &a, const SideGroup &b) { return a.length > b.length; });
        if (groups.size() < 2 || groups[1].length < 0.2
            || (groups.size() > 2 && groups[2].length > groups[1].length * 0.5)) {
            error = QStringLiteral("This face is not a single chamfer");
            notChamfer = true;
            return false;
        }
        Job job;
        job.faces = group;
        job.sideA = groups[0].faces;
        job.sideB = groups[1].faces;
        job.center = GeomUtils::faceCentroid(chamfer);
        job.legA = job.legB = -1;
        for (const TopoDS_Face &part : group) {
            for (TopExp_Explorer ex(part, TopAbs_EDGE); ex.More(); ex.Next()) {
                const gp_Pnt mid = GeomUtils::edgeMidpoint(TopoDS::Edge(ex.Current()));
                const double da = distanceToFaces(job.sideA, mid);
                const double db = distanceToFaces(job.sideB, mid);
                // The edge lying on one neighbour is the far end of the leg along the other.
                // Seam edges of a split cone sit on neither face, so they are not the leg.
                if (db <= 1e-3 && da > 1e-3) { job.legA = da; job.onB = mid; job.coneOnB = part; }
                if (da <= 1e-3 && db > 1e-3) { job.legB = db; job.onA = mid; job.coneOnA = part; }
            }
        }
        if (job.legA < 0 || job.legB < 0) {
            error = QStringLiteral("Could not measure this chamfer");
            notChamfer = true;
            return false;
        }
        const auto shrink = [&](const QList<TopoDS_Face> &side, const TopoDS_Face &cone, const gp_Pnt &at, double leg) {
            const double phi = normalNear(cone, at).Angle(normalNear(faceOn(side, at), at));
            const double halfPi = 1.5707963267948966;
            const double acute = phi > halfPi ? (2 * halfPi - phi) : phi;
            const double s = std::sin(acute);
            if (s < 0.05) return -1.0;
            return leg - distance / s;
        };
        job.legA = shrink(job.sideA, job.coneOnA, job.onA, job.legA);
        job.legB = shrink(job.sideB, job.coneOnB, job.onB, job.legB);
        if (job.legA < -0.05 || job.legB < -0.05) {
            // Past the corner: the chamfer comes off. A still-positive other leg is not a chamfer.
            if (job.legA > 0.05 || job.legB > 0.05) {
                error = QStringLiteral("Pull is larger than this chamfer");
                return false;
            }
            job.legA = job.legB = 0;
        }
        jobs.append(job);
    }
    if (jobs.isEmpty()) { error = QStringLiteral("Select a chamfer face"); return false; }

    BRepAlgoAPI_Defeaturing defeat;
    defeat.SetShape(solid);
    for (const Job &job : jobs)
        for (const TopoDS_Face &face : job.faces) defeat.AddFaceToRemove(face);
    defeat.SetToFillHistory(Standard_False);
    Handle(GeomUtils::TimeLimit) limit = new GeomUtils::TimeLimit(2000);
    defeat.Build(limit->Start());
    if (limit->expired() || !defeat.IsDone() || defeat.Shape().IsNull()) {
        error = QStringLiteral("Could not pull this chamfer");
        return false;
    }
    TopoDS_Shape bare = defeat.Shape();
    const bool anyLeft = std::any_of(jobs.begin(), jobs.end(), [](const Job &j) { return j.legA > 0.05 || j.legB > 0.05; });
    if (!anyLeft) {
        out = bare;
        return !GeomUtils::solids(out).isEmpty();
    }

    BRepFilletAPI_MakeChamfer chamfer(bare);
    int added = 0;
    for (const Job &job : jobs) {
        if (job.legA <= 0.05 && job.legB <= 0.05) continue;
        TopoDS_Edge restored;
        double best = 1e100;
        for (TopExp_Explorer ex(bare, TopAbs_EDGE); ex.More(); ex.Next()) {
            const TopoDS_Edge edge = TopoDS::Edge(ex.Current());
            if (BRep_Tool::Degenerated(edge)) continue;
            const gp_Pnt mid = GeomUtils::edgeMidpoint(edge);
            if (distanceToFaces(job.sideA, mid) > 0.05 || distanceToFaces(job.sideB, mid) > 0.05) continue;
            const double rank = mid.Distance(job.center);
            if (rank < best) { best = rank; restored = edge; }
        }
        if (restored.IsNull()) {
            error = QStringLiteral("Could not pull this chamfer");
            return false;
        }
        const double a = std::max(0.0, job.legA);
        const double b = std::max(0.0, job.legB);
        if (std::abs(a - b) <= 0.05 || std::min(a, b) <= 0.05)
            chamfer.Add(std::max(a, b), restored);
        else {
            TopoDS_Face side;
            for (TopExp_Explorer fx(bare, TopAbs_FACE); fx.More(); fx.Next()) {
                const TopoDS_Face f = TopoDS::Face(fx.Current());
                if (distanceToFaces(job.sideA, GeomUtils::faceCentroid(f)) < 0.05) { side = f; break; }
            }
            if (side.IsNull()) chamfer.Add(a, restored);
            else chamfer.Add(a, b, restored, side);
        }
        ++added;
    }
    if (!added) { out = bare; return !GeomUtils::solids(out).isEmpty(); }
    chamfer.Build();
    if (!chamfer.IsDone()) { error = QStringLiteral("Pull is too large for this chamfer"); return false; }
    out = chamfer.Shape();
    return !GeomUtils::solids(out).isEmpty();
}

// A copy of a solid with the given faces found on the copy, for work in a worker thread that may outlive
// the feature
struct CopiedFaces {
    TopoDS_Shape solid;
    QList<TopoDS_Face> faces;
};
CopiedFaces copyWithFaces(const TopoDS_Shape &solid, const QList<TopoDS_Face> &faces) {
    CopiedFaces c;
    c.solid = GeomUtils::copyWithFaces(solid, faces, c.faces);
    return c;
}

// The material between a face of `solid` and the same face moved `distance` along its outward normal: a prism
// for a flat face, the face thickened for a curved one. Null when OCCT cannot build it.
TopoDS_Shape faceSlab(const TopoDS_Shape &solid, const TopoDS_Face &f, double distance) {
    if (BRepAdaptor_Surface(f, Standard_False).GetType() == GeomAbs_Plane)
        return prism(f, gp_Vec(outwardNormal(f)) * distance);
    const auto thicken = [&f](double d) -> TopoDS_Shape {
        BRepOffsetAPI_MakeThickSolid thick;
        thick.MakeThickSolidBySimple(f, d);
        if (!thick.IsDone() || GeomUtils::solids(thick.Shape()).isEmpty()) return {};
        // The thickened cones of the spool hub came out inside out (negative volume), which no boolean can use
        QList<TopoDS_Shape> parts;
        for (const TopoDS_Shape &part : GeomUtils::solids(thick.Shape())) {
            TopoDS_Solid solid = TopoDS::Solid(part);
            BRepLib::OrientClosedSolid(solid);
            parts << solid;
        }
        return GeomUtils::makeCompound(parts);
    };
    const TopoDS_Shape slab = thicken(distance);
    if (slab.IsNull()) return {};
    // A pushed-out slab lies outside the solid, a pulled-in one inside; the thickening may go the other way
    GProp_GProps props;
    BRepGProp::VolumeProperties(slab, props);
    BRepClass3d_SolidClassifier where(solid, props.CentreOfMass(), 1e-7);
    const bool inside = where.State() == TopAbs_IN;
    return inside == (distance < 0) ? slab : thicken(-distance);
}

// OCCT's offset either finishes within a second or two or runs for half a minute (spool hub), so it gets less
constexpr qint64 kOccOffsetMs = 3000;
constexpr qint64 kOffsetFaceMs = 6000;   // the slab fallback

bool offsetFacesOnSolid(const TopoDS_Shape &solid, const QList<TopoDS_Face> &faces, double distance,
                        TopoDS_Shape &out, QString &error) {
    if (faces.isEmpty()) { error = QStringLiteral("Select one or more faces"); return false; }
    if (std::fabs(distance) < 1e-9) { out = solid; return true; }
    const double v0 = GeomUtils::volume(solid);
    double area = 0;
    for (const TopoDS_Face &f : faces) area += GeomUtils::area(f);
    // The volume moved the right way, and (for the slab fallback) by roughly the faces' area times the distance
    const auto plausible = [v0, area, distance](const TopoDS_Shape &s, bool sized) {
        if (s.IsNull() || GeomUtils::solids(s).isEmpty()) return false;
        const double dv = GeomUtils::volume(s) - v0;
        if (!(distance > 0 ? dv > 1e-6 : dv < -1e-6)) return false;
        const double expected = area * std::fabs(distance);
        return !sized || (std::fabs(dv) > expected * 0.2 && std::fabs(dv) < expected * 3 + 1e-6);
    };

    // A round chamfer is pulled by rebuilding it between its two walls. Other cones (a taper, a
    // countersink) are offset like any other face below.
    const bool cones = std::all_of(faces.begin(), faces.end(),
                                   [](const TopoDS_Face &f) { return BRepAdaptor_Surface(f).GetType() == GeomAbs_Cone; });
    if (cones) {
        bool notChamfer = false;
        bool pulled = false;
        try {
            pulled = pullChamferFaces(solid, faces, distance, out, error, notChamfer);
        } catch (...) {
            error = QStringLiteral("Could not pull this chamfer");
        }
        if (pulled && !GeomUtils::solids(out).isEmpty() && std::isfinite(GeomUtils::volume(out))
            && (GeomUtils::faceMap(out).Extent() > 200 || BRepCheck_Analyzer(out).IsValid()))
            return true;
        // A chamfer that cannot be rebuilt (a frustum between two flats looks like one) is still offset
        // like any other face; its own message is kept in case that fails too
        if (notChamfer) error.clear();
    }

    // OCCT's offset with only these faces moved. It walks every face of the solid and has locked the window
    // on the spool hub, so it runs on a copy in a worker thread with a hard limit.
    {
        const CopiedFaces c = copyWithFaces(solid, faces);
        auto result = std::make_shared<TopoDS_Shape>();
        const bool finished = GeomUtils::runWithTimeLimit(kOccOffsetMs, [c, distance, plausible, result] {
            BRepOffset_MakeOffset mo;
            mo.Initialize(c.solid, 0.0, 1e-5, BRepOffset_Skin, Standard_True, Standard_False, GeomAbs_Intersection);
            for (const TopoDS_Face &f : c.faces) mo.SetOffsetOnFace(f, distance);
            Handle(GeomUtils::TimeLimit) limit = new GeomUtils::TimeLimit(kOccOffsetMs);
            mo.MakeOffsetShape(limit->Start());
            if (!limit->expired() && mo.IsDone() && plausible(mo.Shape(), false) && BRepCheck_Analyzer(mo.Shape()).IsValid())
                *result = mo.Shape();
        });
        if (finished && !result->IsNull()) {
            out = *result;
            return true;
        }
    }

    // Replace-face fallback: add (or take away) the slab between each face and its moved copy. The sides of a
    // slab run along the face normals, which is exact where the neighbouring faces meet the face square.
    const CopiedFaces c = copyWithFaces(solid, faces);
    struct Result {
        TopoDS_Shape shape;
        QString error;
    };
    auto result = std::make_shared<Result>();
    const bool finished = GeomUtils::runWithTimeLimit(kOffsetFaceMs, [c, distance, plausible, result] {
        TopoDS_Shape r = c.solid;
        for (const TopoDS_Face &f : c.faces) {
            const TopoDS_Shape slab = faceSlab(c.solid, f, distance);
            if (slab.IsNull()) {
                result->error = QStringLiteral("Could not offset this face");
                return;
            }
            r = distance > 0 ? fuse(r, slab) : cut(r, slab);
            if (r.IsNull()) {
                result->error = QStringLiteral("Offset Face failed");
                return;
            }
        }
        r = GeomUtils::unify(r);
        if (!plausible(r, true)) {
            result->error = QStringLiteral("Offset Face failed (the distance may be too large for this face)");
            return;
        }
        result->shape = r;
    });
    if (!finished) {
        error = QStringLiteral("Offset Face took longer than 9 s and was stopped");
        return false;
    }
    if (result->shape.IsNull()) {
        if (error.isEmpty()) error = result->error.isEmpty() ? QStringLiteral("Could not pull this face") : result->error;
        return false;
    }
    out = result->shape;
    error.clear();
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
    TopoDS_Shape result;
    QString pullErr;
    if (!offsetFacesOnSolid(ctx.bodies[body].shape, faces, d, result, pullErr))
        return fail(pullErr.isEmpty() ? QStringLiteral("Could not pull this face") : pullErr);
    if (std::fabs(d) < 1e-9) return true;
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

constexpr int kDeleteFaceMs = 15000;

bool FissionDeleteFace::compute(RebuildContext &ctx) {
    QString err;
    QMap<int, QList<TopoDS_Face>> byBody;
    for (const QVariant &r : list("faces")) {
        int b = -1;
        TopoDS_Face f;
        if (!ctx.resolveFace(r, b, f, err)) return fail(err);
        byBody[b] << f;
    }
    if (byBody.isEmpty()) return fail("Select one or more faces");
    // Highest body first: a body that splits into lumps inserts new bodies after itself
    for (auto it = byBody.constEnd(); it != byBody.constBegin();) {
        --it;
        const TopoDS_Shape shape = ctx.bodies[it.key()].shape;
        if (MeshUtils::isMesh(shape)) return fail("Delete Face works on solid bodies, not meshes");
        // OCCT's defeaturing only checks for cancel between its stages, and on helical (thread) faces one
        // stage ran for minutes. It works on a copy in a worker thread; after kDeleteFaceMs the feature gives
        // up and the worker finishes on its own copy in the background.
        QList<TopoDS_Face> onCopy;
        const TopoDS_Shape copy = GeomUtils::copyWithFaces(shape, it.value(), onCopy);
        TopTools_ListOfShape faces;
        for (const TopoDS_Face &f : onCopy) faces.Append(f);
        struct Result {
            TopoDS_Shape out;
            QString error;
        };
        auto job = std::make_shared<Result>();
        const bool finished = GeomUtils::runWithTimeLimit(kDeleteFaceMs, [job, copy, faces] {
            try {
                BRepAlgoAPI_Defeaturing defeat;
                defeat.SetShape(copy);
                for (TopTools_ListOfShape::Iterator f(faces); f.More(); f.Next()) defeat.AddFaceToRemove(f.Value());
                defeat.SetToFillHistory(Standard_False);
                defeat.SetRunParallel(Standard_True);
                // Asked to stop too, so a worker the feature gave up on does not keep a core busy for minutes
                Handle(GeomUtils::TimeLimit) limit = new GeomUtils::TimeLimit(kDeleteFaceMs);
                defeat.Build(limit->Start());
                if (!defeat.IsDone() || defeat.Shape().IsNull() || GeomUtils::solids(defeat.Shape()).isEmpty())
                    job->error = QStringLiteral("The faces around it cannot close the gap, so this face cannot be deleted");
                else if (!BRepCheck_Analyzer(defeat.Shape()).IsValid())
                    job->error = QStringLiteral("Deleting this face leaves an invalid solid");
                else
                    job->out = defeat.Shape();
            } catch (...) {
                job->error = QStringLiteral("Delete Face failed in the geometry kernel");
            }
        });
        if (!finished)
            return fail("Delete Face took longer than 15 s and was stopped (OCCT cannot heal these faces quickly)");
        if (!job->error.isEmpty()) return fail(job->error);
        replaceBody(ctx, it.key(), job->out, id());
    }
    return true;
}

// Draft of one flat face without OCCT's draft: the face's plane is turned by `rad` about the line where it
// meets the neutral plane, so that it leans towards the pull, and the body is cut or filled to the turned
// plane over the face's own outline (as Replace Face does). OCCT's draft has to re-intersect the turned face
// with every neighbour and fails on imported bodies whose side faces meet chamfers and blends; here the
// neighbours only grow or shrink by the thin wedge.
TopoDS_Shape draftFlatFace(const TopoDS_Shape &solid, const TopoDS_Face &f, const gp_Dir &pull, const gp_Pln &neutral,
                      double rad) {
    const BRepAdaptor_Surface surface(f, Standard_False);
    if (surface.GetType() != GeomAbs_Plane || std::fabs(rad) < 1e-9) return {};
    const gp_Dir n = outwardNormal(f);
    IntAna_QuadQuadGeo meet(surface.Plane(), neutral, 1e-9, 1e-9);
    if (!meet.IsDone() || meet.TypeInter() != IntAna_Line || meet.NbSolutions() < 1) return {};
    const gp_Lin hinge = meet.Line(1);
    // Turn so the face leans towards the pull: its normal gains a component along the pull
    gp_Dir turned = n.Rotated(hinge.Position(), rad);
    if (n.Rotated(hinge.Position(), -rad).Dot(pull) > turned.Dot(pull)) turned = n.Rotated(hinge.Position(), -rad);
    // How far the turned plane gets from the face: the band the wedge lives in
    double reach = 0;
    for (TopExp_Explorer v(f, TopAbs_VERTEX); v.More(); v.Next())
        reach = std::max(reach, hinge.Distance(BRep_Tool::Pnt(TopoDS::Vertex(v.Current()))));
    const double depth = reach * std::tan(std::fabs(rad)) * 1.5 + 0.05;
    // The cutting plane is centred on the face: the hinge line's own origin can be far away
    const gp_Pnt c = GeomUtils::faceCentroid(f);
    const gp_Pnt mid = hinge.Location().Translated(gp_Vec(hinge.Direction()) * gp_Vec(hinge.Location(), c).Dot(gp_Vec(hinge.Direction())));
    const gp_Ax3 plane(mid, turned);
    const TopoDS_Shape beyond = halfSpace(plane, (reach + depth) * 4 + 1);
    const TopoDS_Shape grow = cut(prism(f, gp_Vec(n) * depth), beyond);
    const TopoDS_Shape column = prism(translated(f, gp_Vec(n) * -depth), gp_Vec(n) * (2 * depth));
    const TopoDS_Shape trim = column.IsNull() ? TopoDS_Shape() : common(column, beyond);
    if (grow.IsNull() || trim.IsNull()) return {};
    const TopoDS_Shape grown = fuse(solid, grow);
    const TopoDS_Shape r = grown.IsNull() ? TopoDS_Shape() : cut(grown, trim);
    if (r.IsNull() || GeomUtils::solids(r).isEmpty()) return {};
    return GeomUtils::unify(r);
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

    // Flat faces OCCT cannot draft are turned one by one (draftFlatFace); each turned face is found again on
    // the new body by its centre, since the booleans renumber the faces
    auto tiltFaces = [&](const TopoDS_Shape &shape, const QList<TopoDS_Face> &which, const gp_Dir &pull,
                         TopoDS_Shape &out) -> bool {
        TopoDS_Shape r = shape;
        for (const TopoDS_Face &f : which) {
            const gp_Pnt c = GeomUtils::faceCentroid(f);
            TopoDS_Face now;
            double best = 1e-3;
            for (TopExp_Explorer e(r, TopAbs_FACE); e.More(); e.Next()) {
                const TopoDS_Face cand = TopoDS::Face(e.Current());
                if (BRepAdaptor_Surface(cand, Standard_False).GetType() != GeomAbs_Plane) continue;
                const double d = BRepAdaptor_Surface(cand, Standard_False).Plane().Distance(c);
                if (d < best && outwardNormal(cand).IsParallel(outwardNormal(f), 1e-6)
                    && outwardNormal(cand).Dot(outwardNormal(f)) > 0) {
                    best = d;
                    now = cand;
                }
            }
            if (now.IsNull()) return false;
            r = draftFlatFace(r, now, pull, neutral, rad);
            if (r.IsNull()) return false;
        }
        if (!BRepCheck_Analyzer(r).IsValid()) return false;
        out = r;
        return true;
    };
    auto draftFaces = [&](const TopoDS_Shape &shape, const QList<TopoDS_Face> &which, const gp_Dir &pull,
                          TopoDS_Shape &out) -> bool {
        bool flat = true;
        for (const TopoDS_Face &f : which)
            if (BRepAdaptor_Surface(f, Standard_False).GetType() != GeomAbs_Plane) flat = false;
        try {
            BRepOffsetAPI_DraftAngle dr(shape);
            for (const TopoDS_Face &f : which) {
                dr.Add(f, pull, rad, neutral);
                if (!dr.AddDone()) {
                    err = "Draft cannot be applied to one of the faces (is it parallel to the pull direction?)";
                    return flat && tiltFaces(shape, which, pull, out);
                }
            }
            dr.Build();
            if (dr.IsDone() && !GeomUtils::solids(dr.Shape()).isEmpty()) {
                out = dr.Shape();
                return true;
            }
        } catch (const Standard_Failure &) {
        }
        err = "Draft failed";
        return flat && tiltFaces(shape, which, pull, out);
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

    // Viewport angle knob: pivot on the first drafted face, height along the pull, lean against "out"
    {
        const TopoDS_Face &face = faces.first();
        const gp_Pnt pivot = GeomUtils::faceCentroid(face);
        gp_Vec outV(GeomUtils::shapeCentroid(ctx.bodies[body].shape), pivot);
        outV -= gp_Vec(dir) * outV.Dot(gp_Vec(dir));
        if (outV.Magnitude() < 1e-9) outV = gp_Vec(ax.XDirection());
        const gp_Dir out(outV);
        const Bnd_Box bb = GeomUtils::boundingBox(face);
        double xmin, ymin, zmin, xmax, ymax, zmax;
        bb.Get(xmin, ymin, zmin, xmax, ymax, zmax);
        const double height = std::max(8.0, gp_Pnt(xmin, ymin, zmin).Distance(gp_Pnt(xmax, ymax, zmax)) * 0.45);
        // Distance arrow along the pull is visual only; the angle knob writes "angle"
        setManipulator(pivot, dir, QStringLiteral("distance"), height);
        setAngleManipulator(pivot, dir, out, height, QStringLiteral("angle"), angle);
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
