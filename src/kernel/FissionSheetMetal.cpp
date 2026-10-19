// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#include "FissionSheetMetal.h"
#include "FileExport.h"
#include "FissionDocument.h"
#include "FissionSketch.h"
#include "GeomUtils.h"

#include <BRepAdaptor_Curve.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepAlgoAPI_Common.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepAlgoAPI_Fuse.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <BRepBuilderAPI_MakeVertex.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRepExtrema_DistShapeShape.hxx>
#include <BRepGProp.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <BRepPrimAPI_MakeRevol.hxx>
#include <BRepTools_WireExplorer.hxx>
#include <BRep_Builder.hxx>
#include <BRep_Tool.hxx>
#include <Bnd_Box.hxx>
#include <GProp_GProps.hxx>
#include <Standard_Failure.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_IndexedDataMapOfShapeListOfShape.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Compound.hxx>
#include <algorithm>
#include <cmath>

namespace {

constexpr double kDeg = 3.14159265358979323846 / 180.0;

// Outward normal of a planar face; false when the face is not flat
bool planeNormal(const TopoDS_Face &f, gp_Dir &n) {
    BRepAdaptor_Surface s(f);
    if (s.GetType() != GeomAbs_Plane) return false;
    n = s.Plane().Position().Direction();
    if (f.Orientation() == TopAbs_REVERSED) n.Reverse();
    return true;
}

double faceArea(const TopoDS_Shape &f) {
    GProp_GProps p;
    BRepGProp::SurfaceProperties(f, p);
    return p.Mass();
}

double volume(const TopoDS_Shape &s) {
    if (s.IsNull()) return 0;
    GProp_GProps p;
    BRepGProp::VolumeProperties(s, p);
    return std::fabs(p.Mass());
}

TopoDS_Shape fuse(const TopoDS_Shape &a, const TopoDS_Shape &b) {
    if (a.IsNull()) return b;
    if (b.IsNull()) return a;
    BRepAlgoAPI_Fuse op(a, b);
    if (!op.IsDone()) return {};
    return GeomUtils::unify(op.Shape());
}

TopoDS_Shape common(const TopoDS_Shape &a, const TopoDS_Shape &b) {
    if (a.IsNull() || b.IsNull()) return {};
    BRepAlgoAPI_Common op(a, b);
    if (!op.IsDone()) return {};
    return op.Shape();
}

TopoDS_Shape cut(const TopoDS_Shape &a, const TopoDS_Shape &b) {
    if (a.IsNull() || b.IsNull()) return a;
    BRepAlgoAPI_Cut op(a, b);
    if (!op.IsDone()) return {};
    return op.Shape();
}

TopoDS_Shape moved(const TopoDS_Shape &s, const gp_Trsf &t) {
    if (s.IsNull()) return s;
    return BRepBuilderAPI_Transform(s, t, true).Shape();
}

double distance(const gp_Pnt &p, const TopoDS_Shape &s) {
    if (s.IsNull()) return 1e300;
    BRepExtrema_DistShapeShape d(BRepBuilderAPI_MakeVertex(p).Vertex(), s);
    return d.IsDone() && d.NbSolution() > 0 ? d.Value() : 1e300;
}

double distance(const TopoDS_Shape &a, const TopoDS_Shape &b) {
    if (a.IsNull() || b.IsNull()) return 1e300;
    BRepExtrema_DistShapeShape d(a, b);
    return d.IsDone() && d.NbSolution() > 0 ? d.Value() : 1e300;
}

// The slab { t0 <= (x - p) . n <= t1 } of a cube of half-size L around p
TopoDS_Shape slab(const gp_Pnt &p, const gp_Dir &n, double t0, double t1, double L) {
    const gp_Ax2 ax(p, n);
    const gp_Pnt o = p.Translated(gp_Vec(n) * t0 - gp_Vec(ax.XDirection()) * L - gp_Vec(ax.YDirection()) * L);
    return BRepPrimAPI_MakeBox(gp_Ax2(o, n, ax.XDirection()), 2 * L, 2 * L, t1 - t0).Shape();
}

// The rectangle o, o + a, o + a + b, o + b as a face
TopoDS_Face rectangle(const gp_Pnt &o, const gp_Vec &a, const gp_Vec &b) {
    BRepBuilderAPI_MakePolygon poly(o, o.Translated(a), o.Translated(a + b), o.Translated(b), Standard_True);
    return BRepBuilderAPI_MakeFace(poly.Wire(), Standard_True).Face();
}

TopoDS_Shape compound(const QList<TopoDS_Shape> &shapes) {
    if (shapes.isEmpty()) return {};
    if (shapes.size() == 1) return shapes.first();
    TopoDS_Compound c;
    BRep_Builder b;
    b.MakeCompound(c);
    for (const TopoDS_Shape &s : shapes) b.Add(c, s);
    return c;
}

// A bend grown from an end face: P is a point of the bend edge on the sheet face with normal up, d the
// edge direction, out the end face's outward normal. down bends away from up.
struct BendGeom {
    TopoDS_Shape solid;
    gp_Ax1 axis;
    double signedAngle = 0;
    gp_Trsf rot;
    TopoDS_Face moved;   // the end face after the bend
    gp_Dir outRot;
};

bool makeBend(const TopoDS_Face &side, const gp_Pnt &P, const gp_Dir &d, const gp_Dir &up, const gp_Dir &out,
              double thickness, double angle, double radius, bool down, BendGeom &g) {
    const double s = down ? -1.0 : 1.0;
    const gp_Pnt C = P.Translated(gp_Vec(up) * (s > 0 ? radius : -(thickness + radius)));
    const gp_Vec u0 = gp_Vec(up) * -s;
    const double sign = gp_Vec(d).Crossed(u0).Dot(gp_Vec(out)) > 0 ? 1.0 : -1.0;
    g.axis = gp_Ax1(C, d);
    g.signedAngle = sign * angle;
    g.rot.SetRotation(g.axis, g.signedAngle);
    g.outRot = out.Transformed(g.rot);
    BRepPrimAPI_MakeRevol bend(side, g.axis, g.signedAngle);
    if (!bend.IsDone()) return false;
    g.solid = bend.Shape();
    g.moved = TopoDS::Face(moved(side, g.rot));
    return true;
}

// The two ends of a face along d (the bend line of an end face)
void extentAlong(const TopoDS_Shape &face, const gp_Dir &d, gp_Pnt &a, gp_Pnt &b) {
    double lo = 1e300, hi = -1e300;
    for (TopExp_Explorer e(face, TopAbs_VERTEX); e.More(); e.Next()) {
        const gp_Pnt p = BRep_Tool::Pnt(TopoDS::Vertex(e.Current()));
        const double t = gp_Vec(p.XYZ()).Dot(gp_Vec(d));
        if (t < lo) { lo = t; a = p; }
        if (t > hi) { hi = t; b = p; }
    }
}

// Sheet features before `self` that are active, in timeline order
QList<FissionFeature *> earlierFeatures(RebuildContext &ctx, const FissionFeature *self) {
    QList<FissionFeature *> out;
    for (FissionFeature *f : ctx.document()->timeline()->features()) {
        if (f == self) break;
        if (ctx.feature(f->id()) == f && f->isHealthy()) out << f;
    }
    return out;
}

// Unfolds the sheet body `bi` (-1: the first sheet body) by walking the sheet features in timeline order
bool unfoldSheet(RebuildContext &ctx, const FissionFeature *self, int &bi, double k, SheetFlat &r, QString &err) {
    const QList<FissionFeature *> earlier = earlierFeatures(ctx, self);
    auto bodyIndex = [&](const QString &name) {
        for (int i = 0; i < ctx.bodies.size(); ++i)
            if (ctx.bodies[i].name == name) return i;
        return -1;
    };
    QString name;
    if (bi < 0) {
        for (FissionFeature *f : earlier)
            if (auto *p = dynamic_cast<SheetMetalPart *>(f))
                if (!p->baseSheet().IsNull() && bodyIndex(p->sheetBody()) >= 0) { name = p->sheetBody(); bi = bodyIndex(name); break; }
    } else {
        if (bi >= ctx.bodies.size()) { err = QString("Body %1 no longer exists").arg(bi + 1); return false; }
        name = ctx.bodies[bi].name;
    }
    QList<SheetMetalPart *> parts;
    TopoDS_Shape base;
    for (FissionFeature *f : earlier)
        if (auto *p = dynamic_cast<SheetMetalPart *>(f)) {
            if (p->sheetBody() != name || name.isEmpty()) continue;
            if (base.IsNull() && !p->baseSheet().IsNull()) base = p->baseSheet();
            if (!base.IsNull()) parts << p;
        }
    if (base.IsNull()) { err = "Pick a body made with Flange or Contour Flange (sheet metal)"; return false; }

    r.flat = base;
    r.regions = {{base, gp_Trsf()}};
    const double tol = 1e-3;
    auto parentOf = [&](const TopoDS_Shape &s) {
        for (int i = r.regions.size() - 1; i >= 0; --i)
            if (distance(s, r.regions[i].folded) <= tol) return i;
        return -1;
    };
    for (SheetMetalPart *p : parts) {
        for (const SheetBend &b : p->bends()) {
            const int pr = parentOf(BRepBuilderAPI_MakeVertex(GeomUtils::faceCentroid(b.side)).Vertex());
            if (pr < 0) { ++r.skipped; continue; }
            const gp_Trsf MP = r.regions[pr].toFlat;
            const double allow = b.consumes ? b.allowance : b.angle * (b.radius + k * b.thickness);
            if (b.consumes && !b.keep.IsNull()) r.regions[pr].folded = common(r.regions[pr].folded, b.keep);
            gp_Trsf shift;
            shift.SetTranslation(gp_Vec(b.out) * allow);
            const gp_Trsf MU = MP * shift * b.rot.Inverted();
            TopoDS_Shape strip;
            if (allow > 1e-9) {
                BRepPrimAPI_MakePrism prism(b.side, gp_Vec(b.out) * allow);
                if (!prism.IsDone()) { err = "Could not unfold a bend"; return false; }
                strip = moved(prism.Shape(), MP);
            }
            if (!b.consumes) {
                r.flat = fuse(r.flat, strip);
                if (!b.segment.IsNull()) r.flat = fuse(r.flat, moved(b.segment, MU));
                if (r.flat.IsNull()) { err = "Could not join an unfolded bend"; return false; }
            }
            SheetFlat::Strip s;
            s.flat = strip;
            s.parentToFlat = MP;
            s.origin = GeomUtils::faceCentroid(b.side);
            s.out = b.out;
            s.axis = b.axis;
            s.signedAngle = b.signedAngle;
            s.allowance = allow;
            r.strips << s;
            gp_Pnt e0, e1;
            gp_Dir along = b.axis.Direction();
            extentAlong(b.side, along, e0, e1);
            const gp_Vec half = gp_Vec(b.out) * (allow / 2);
            r.bendLines.append({e0.Translated(half).Transformed(MP), e1.Translated(half).Transformed(MP)});
            r.regions.append({b.segment.IsNull() ? TopoDS_Shape(moved(b.side, b.rot)) : b.segment, MU});
            ++r.unfolded;
        }
        for (const TopoDS_Shape &a : p->additions()) {
            const int pr = parentOf(a);
            if (pr < 0) { ++r.skipped; continue; }
            const gp_Trsf MP = r.regions[pr].toFlat;
            r.flat = fuse(r.flat, moved(a, MP));
            if (r.flat.IsNull()) { err = "Could not join a tab to the flat pattern"; return false; }
            r.regions.append({a, MP});
        }
    }
    // The sheet normal: the largest flat face of the base
    double best = 0;
    for (TopExp_Explorer e(base, TopAbs_FACE); e.More(); e.Next()) {
        gp_Dir n;
        const TopoDS_Face f = TopoDS::Face(e.Current());
        if (!planeNormal(f, n)) continue;
        const double a = faceArea(f);
        if (a > best) { best = a; r.normal = n; }
    }
    return true;
}

}   // namespace

// --- Base flange -------------------------------------------------------------

bool FissionBaseFlange::compute(RebuildContext &ctx) {
    const QVariantList profiles = list("profiles");
    TopoDS_Shape profile;
    gp_Ax3 plane;
    QString err;
    const bool ok = RebuildContext::isFaceProfileList(profiles) ? ctx.resolveFaceProfiles(profiles, profile, plane, err)
                                                                : ctx.resolveProfiles(str("sketch"), profiles, profile, plane, err);
    if (!ok) return fail(err);
    double t = 0;
    if (!number(ctx, "thickness", t)) return false;
    if (t <= 0) return fail("Thickness must be greater than zero");
    const gp_Dir dir = flag("flip") ? plane.Direction().Reversed() : plane.Direction();
    try {
        BRepPrimAPI_MakePrism prism(profile, gp_Vec(dir) * t);
        if (!prism.IsDone()) return fail("Could not thicken the profile");
        m_sheet = prism.Shape();
    } catch (const Standard_Failure &e) {
        return fail(QString("Base flange failed: %1").arg(e.GetMessageString()));
    }
    const int before = ctx.bodies.size();
    ctx.addBodies(m_sheet, id());
    if (ctx.bodies.size() == before) return fail("The profile made no solid");
    m_body = ctx.bodies[before].name;
    setManipulator(plane.Location(), dir, "thickness", t);
    return true;
}

// --- Edge flange -------------------------------------------------------------

bool FissionEdgeFlange::flangeFromEdge(RebuildContext &ctx, double height, double angle, double radius, bool down,
                                       const QString &manipulatorKey) {
    int body = -1;
    TopoDS_Edge edge;
    QString err;
    if (!ctx.resolveEdge(get("edge"), body, edge, err)) return fail(err.isEmpty() ? "Pick a straight edge of a sheet" : err);
    BRepAdaptor_Curve curve(edge);
    if (curve.GetType() != GeomAbs_Line) return fail("The flange edge must be straight");

    const TopoDS_Shape shape = ctx.bodies[body].shape;
    TopTools_IndexedDataMapOfShapeListOfShape map;
    TopExp::MapShapesAndAncestors(shape, TopAbs_EDGE, TopAbs_FACE, map);
    if (!map.Contains(edge)) return fail("The edge is not on a body");
    QList<TopoDS_Face> faces;
    for (const TopoDS_Shape &f : map.FindFromKey(edge)) {
        const TopoDS_Face face = TopoDS::Face(f);
        if (!faces.contains(face)) faces << face;
    }
    if (faces.size() != 2) return fail("The edge must join two faces");
    gp_Dir n0, n1;
    if (!planeNormal(faces[0], n0) || !planeNormal(faces[1], n1)) return fail("Both faces at the edge must be flat");
    // The thin end face is the side the flange grows from; the large one is the sheet face
    const bool firstIsSide = faceArea(faces[0]) < faceArea(faces[1]);
    const TopoDS_Face side = firstIsSide ? faces[0] : faces[1];
    const gp_Dir ns = firstIsSide ? n1 : n0, nt = firstIsSide ? n0 : n1;
    const gp_Dir d = curve.Line().Direction();
    if (std::fabs(ns.Dot(nt)) > 1e-6 || std::fabs(d.Dot(ns)) > 1e-6) return fail("The faces at the edge must meet square");
    double lo = 1e300, hi = -1e300;
    for (TopExp_Explorer e(side, TopAbs_VERTEX); e.More(); e.Next()) {
        const double h = gp_Vec(BRep_Tool::Pnt(TopoDS::Vertex(e.Current())).XYZ()).Dot(gp_Vec(ns));
        lo = std::min(lo, h);
        hi = std::max(hi, h);
    }
    const double thickness = hi - lo;
    if (thickness <= 0) return fail("Could not measure the sheet thickness");

    BendGeom g;
    TopoDS_Shape result, segment;
    try {
        if (!makeBend(side, curve.Value(curve.FirstParameter()), d, ns, nt, thickness, angle, radius, down, g))
            return fail("Could not make the bend");
        result = fuse(shape, g.solid);
        if (height > 0 && !result.IsNull()) {
            BRepPrimAPI_MakePrism flange(g.moved, gp_Vec(g.outRot) * height);
            if (!flange.IsDone()) return fail("Could not make the flange");
            segment = flange.Shape();
            result = fuse(result, segment);
        }
    } catch (const Standard_Failure &e) {
        return fail(QString("Flange failed: %1").arg(e.GetMessageString()));
    }
    if (result.IsNull()) return fail("Could not join the flange to the sheet");
    ctx.bodies[body].shape = result;

    m_bend.body = ctx.bodies[body].name;
    m_bend.side = side;
    m_bend.out = nt;
    m_bend.up = ns;
    m_bend.axis = g.axis;
    m_bend.signedAngle = g.signedAngle;
    m_bend.rot = g.rot;
    m_bend.angle = angle;
    m_bend.radius = radius;
    m_bend.thickness = thickness;
    m_bend.segment = segment;
    m_valid = true;

    setManipulator(GeomUtils::faceCentroid(g.moved), g.outRot, manipulatorKey, height);
    return true;
}

bool FissionEdgeFlange::compute(RebuildContext &ctx) {
    double height = 0, angleDeg = 0, radius = 0;
    if (!number(ctx, "height", height) || !number(ctx, "angle", angleDeg) || !number(ctx, "bendRadius", radius)) return false;
    if (height < 0) return fail("Height cannot be negative");
    if (angleDeg <= 0 || angleDeg > 180) return fail("Angle must be between 0 and 180 deg");
    if (radius < 0) return fail("Bend radius cannot be negative");
    return flangeFromEdge(ctx, height, angleDeg * kDeg, radius, flag("flip"), "height");
}

// --- Hem --------------------------------------------------------------------

bool FissionHem::compute(RebuildContext &ctx) {
    const QString kind = str("hemType");
    double length = 0, gap = 0, radius = 0, angleDeg = 180;
    if (!number(ctx, "length", length)) return false;
    if (length < 0) return fail("Length cannot be negative");
    if (kind == "flat") {
        if (!number(ctx, "gap", gap)) return false;
        if (gap <= 0) return fail("The gap must be greater than zero");
        radius = gap / 2;
    } else if (kind == "open" || kind == "rolled") {
        if (!number(ctx, "radius", radius)) return false;
        if (radius <= 0) return fail("Radius must be greater than zero");
        if (kind == "rolled") {
            if (!number(ctx, "angle", angleDeg)) return false;
            if (angleDeg <= 180 || angleDeg >= 360) return fail("A rolled hem turns between 180 and 360 deg");
        }
    } else {
        return fail(QString("Unknown hem type '%1'").arg(kind));
    }
    return flangeFromEdge(ctx, length, angleDeg * kDeg, radius, flag("flip"), "length");
}

// --- Tab --------------------------------------------------------------------

bool FissionTab::compute(RebuildContext &ctx) {
    const QVariantList profiles = list("profiles");
    TopoDS_Shape profile;
    gp_Ax3 plane;
    QString err;
    const bool ok = RebuildContext::isFaceProfileList(profiles) ? ctx.resolveFaceProfiles(profiles, profile, plane, err)
                                                                : ctx.resolveProfiles(str("sketch"), profiles, profile, plane, err);
    if (!ok) return fail(err);
    double t = 0;
    if (!number(ctx, "thickness", t)) return false;
    if (t <= 0) return fail("Thickness must be greater than zero");
    // Into the sheet: the side of the sketch plane a sheet body overlaps (sketches usually sit on a sheet face)
    QList<gp_Dir> dirs{plane.Direction().Reversed(), plane.Direction()};
    if (flag("flip")) std::swap(dirs[0], dirs[1]);
    int body = -1;
    TopoDS_Shape tab;
    try {
        QList<TopoDS_Shape> prisms;
        for (const gp_Dir &d : dirs) {
            BRepPrimAPI_MakePrism prism(profile, gp_Vec(d) * t);
            if (!prism.IsDone()) return fail("Could not thicken the tab");
            prisms << prism.Shape();
        }
        for (int pass = 0; pass < 2 && body < 0; ++pass)
            for (int di = 0; di < 2 && body < 0; ++di)
                for (int i = 0; i < ctx.bodies.size() && body < 0; ++i) {
                    const bool hit = pass == 0 ? volume(common(prisms[di], ctx.bodies[i].shape)) > 1e-9
                                               : distance(prisms[di], ctx.bodies[i].shape) <= 1e-6;
                    if (hit) { body = i; tab = prisms[di]; }
                }
        if (body < 0) return fail("The tab must touch a sheet");
        const TopoDS_Shape result = fuse(ctx.bodies[body].shape, tab);
        if (result.IsNull()) return fail("Could not join the tab to the sheet");
        ctx.bodies[body].shape = result;
    } catch (const Standard_Failure &e) {
        return fail(QString("Tab failed: %1").arg(e.GetMessageString()));
    }
    m_tab = tab;
    m_body = ctx.bodies[body].name;
    return true;
}

// --- Bend -------------------------------------------------------------------

bool FissionBend::compute(RebuildContext &ctx) {
    const QStringList ref = str("line").split(':');
    if (ref.size() != 3 || ref[0] != "sketchline") return fail("Pick a sketch line on the sheet");
    auto *sk = qobject_cast<FissionSketch *>(ctx.feature(ref[1]));
    const int entity = ref[2].toInt();
    if (!sk || entity < 0 || entity >= sk->entityEdges().size() || sk->entityEdges()[entity].isEmpty())
        return fail("The bend line no longer exists");
    const TopoDS_Edge lineEdge = sk->entityEdges()[entity].first();
    if (BRepAdaptor_Curve(lineEdge).GetType() != GeomAbs_Line) return fail("The bend line must be a straight line");
    const gp_Pnt L0 = BRep_Tool::Pnt(TopExp::FirstVertex(lineEdge)), L1 = BRep_Tool::Pnt(TopExp::LastVertex(lineEdge));
    if (L0.Distance(L1) < 1e-6) return fail("The bend line is too short");
    const gp_Dir d(gp_Vec(L0, L1));
    const gp_Pnt M((L0.XYZ() + L1.XYZ()) / 2);

    double angleDeg = 0, radius = 0, k = 0;
    if (!number(ctx, "angle", angleDeg) || !number(ctx, "bendRadius", radius) || !number(ctx, "kFactor", k)) return false;
    if (angleDeg <= 0 || angleDeg > 180) return fail("Angle must be between 0 and 180 deg");
    if (radius < 0) return fail("Bend radius cannot be negative");
    if (k < 0 || k > 1) return fail("K-factor must be between 0 and 1");

    // The sheet face the line lies on
    int bi = -1;
    gp_Dir up;
    for (int i = 0; i < ctx.bodies.size() && bi < 0; ++i)
        for (TopExp_Explorer e(ctx.bodies[i].shape, TopAbs_FACE); e.More() && bi < 0; e.Next()) {
            const TopoDS_Face f = TopoDS::Face(e.Current());
            gp_Dir n;
            if (!planeNormal(f, n) || std::fabs(n.Dot(d)) > 1e-6) continue;
            if (distance(M, f) <= 1e-5) { bi = i; up = n; }
        }
    if (bi < 0) return fail("The bend line must lie on a flat face of a sheet");
    const TopoDS_Shape shape = ctx.bodies[bi].shape;
    // Thickness: the nearest parallel face facing the other way
    double thickness = 1e300;
    for (TopExp_Explorer e(shape, TopAbs_FACE); e.More(); e.Next()) {
        const TopoDS_Face f = TopoDS::Face(e.Current());
        gp_Dir n;
        if (!planeNormal(f, n) || n.Dot(up) > -1 + 1e-6) continue;
        const double off = gp_Vec(BRepAdaptor_Surface(f).Plane().Location(), M).Dot(gp_Vec(up));
        if (off > 1e-6) thickness = std::min(thickness, off);
    }
    if (thickness > 1e299) return fail("Could not measure the sheet thickness");

    gp_Dir out = up.Crossed(d);
    if (flag("flip")) out.Reverse();
    const double angle = angleDeg * kDeg, allow = angle * (radius + k * thickness);
    double x0, y0, z0, x1, y1, z1;
    GeomUtils::boundingBox(shape).Get(x0, y0, z0, x1, y1, z1);
    const double L = 2 * gp_Pnt(x0, y0, z0).Distance(gp_Pnt(x1, y1, z1)) + 100;

    BendGeom g;
    TopoDS_Shape result, segment;
    TopoDS_Face side;
    const TopoDS_Shape keep = slab(M, out, -L, 0, L);
    try {
        const TopoDS_Shape A = common(shape, keep), rest = common(shape, slab(M, out, allow, L, L));
        if (volume(A) < 1e-9) return fail("Nothing of the sheet stays on the fixed side of the line");
        double best = 0;
        for (TopExp_Explorer e(A, TopAbs_FACE); e.More(); e.Next()) {
            const TopoDS_Face f = TopoDS::Face(e.Current());
            gp_Dir n;
            if (!planeNormal(f, n) || n.Dot(out) < 1 - 1e-6) continue;
            if (std::fabs(gp_Vec(M, GeomUtils::faceCentroid(f)).Dot(gp_Vec(out))) > 1e-6) continue;
            const double a = faceArea(f);
            if (a > best) { best = a; side = f; }
        }
        if (side.IsNull()) return fail("The bend line must cross the sheet");
        if (!makeBend(side, M, d, up, out, thickness, angle, radius, flag("down"), g)) return fail("Could not make the bend");
        if (volume(rest) > 1e-9) {
            gp_Trsf back;
            back.SetTranslation(gp_Vec(out) * -allow);
            segment = moved(rest, g.rot * back);
        }
        result = fuse(fuse(A, g.solid), segment);
    } catch (const Standard_Failure &e) {
        return fail(QString("Bend failed: %1").arg(e.GetMessageString()));
    }
    if (result.IsNull()) return fail("Could not join the bend");
    ctx.bodies[bi].shape = result;

    m_bend.body = ctx.bodies[bi].name;
    m_bend.side = side;
    m_bend.out = out;
    m_bend.up = up;
    m_bend.axis = g.axis;
    m_bend.signedAngle = g.signedAngle;
    m_bend.rot = g.rot;
    m_bend.angle = angle;
    m_bend.radius = radius;
    m_bend.thickness = thickness;
    m_bend.segment = segment;
    m_bend.consumes = true;
    m_bend.allowance = allow;
    m_bend.keep = keep;
    m_valid = true;
    return true;
}

// --- Contour flange ---------------------------------------------------------

bool FissionContourFlange::compute(RebuildContext &ctx) {
    auto *sk = qobject_cast<FissionSketch *>(ctx.feature(str("sketch")));
    if (!sk) return fail("Pick a sketch with an open chain of lines");
    QString err;
    const TopoDS_Wire wire = sk->pathWire(err);
    if (wire.IsNull()) return fail(err.isEmpty() ? "The sketch needs one open chain of lines" : err);
    QList<gp_Pnt> q;
    for (BRepTools_WireExplorer e(wire); e.More(); e.Next()) {
        const TopoDS_Edge edge = e.Current();
        if (BRepAdaptor_Curve(edge).GetType() != GeomAbs_Line) return fail("A contour flange is made of straight lines");
        const gp_Pnt a = BRep_Tool::Pnt(TopExp::FirstVertex(edge, Standard_True));
        const gp_Pnt b = BRep_Tool::Pnt(TopExp::LastVertex(edge, Standard_True));
        if (q.isEmpty()) q << a;
        q << b;
    }
    if (q.size() < 2) return fail("The sketch needs at least one line");
    if (q.first().Distance(q.last()) < 1e-6) return fail("The chain of lines must be open");
    double D = 0, T = 0, R = 0;
    if (!number(ctx, "distance", D) || !number(ctx, "thickness", T) || !number(ctx, "bendRadius", R)) return false;
    if (D <= 0) return fail("Distance must be greater than zero");
    if (T <= 0) return fail("Thickness must be greater than zero");
    if (R < 0) return fail("Bend radius cannot be negative");
    const gp_Dir w = flag("reverse") ? sk->plane().Direction().Reversed() : sk->plane().Direction();

    const int n = q.size() - 1;
    QList<gp_Dir> dir, side;   // per line: direction and the thickness side
    for (int i = 0; i < n; ++i) {
        if (q[i].Distance(q[i + 1]) < 1e-6) return fail("A line of the contour is too short");
        dir << gp_Dir(gp_Vec(q[i], q[i + 1]));
        gp_Dir l = w.Crossed(dir[i]);
        if (flag("flip")) l.Reverse();
        side << l;
    }
    QList<double> setback(n + 1, 0.0), turn(n + 1, 0.0);
    QList<bool> towardT(n + 1, false);
    for (int i = 1; i < n; ++i) {
        turn[i] = dir[i - 1].Angle(dir[i]);
        if (turn[i] < 1e-6) return fail("Neighbouring lines must not be in line");
        if (turn[i] > 179 * kDeg) return fail("A corner turns back too sharply");
        towardT[i] = dir[i].Dot(side[i - 1]) > 0;
        setback[i] = (towardT[i] ? R + T : R) * std::tan(turn[i] / 2);
    }
    QList<double> len;
    for (int i = 0; i < n; ++i) {
        len << q[i].Distance(q[i + 1]) - setback[i] - setback[i + 1];
        if (len[i] < -1e-9) return fail("The lines are too short for the bend radius");
    }

    TopoDS_Shape result;
    try {
        auto plate = [&](int i) -> TopoDS_Shape {
            if (len[i] < 1e-9) return {};
            const gp_Pnt s = q[i].Translated(gp_Vec(dir[i]) * setback[i]);
            BRepPrimAPI_MakePrism p(rectangle(s, gp_Vec(side[i]) * T, gp_Vec(w) * D), gp_Vec(dir[i]) * len[i]);
            return p.IsDone() ? p.Shape() : TopoDS_Shape();
        };
        m_first = plate(0);
        if (m_first.IsNull()) {
            // The first line is used up by its corner: a thin start face keeps the base sheet
            const gp_Pnt s = q[0].Translated(gp_Vec(dir[0]) * setback[0]);
            BRepPrimAPI_MakePrism p(rectangle(s, gp_Vec(side[0]) * T, gp_Vec(w) * D), gp_Vec(dir[0]) * 1e-3);
            m_first = p.Shape();
        }
        result = m_first;
        for (int i = 1; i < n; ++i) {
            // The end face of line i-1's plate and the edge of it on the turn side
            const gp_Pnt e = q[i].Translated(gp_Vec(dir[i - 1]) * -setback[i]);
            const TopoDS_Face end = rectangle(e, gp_Vec(side[i - 1]) * T, gp_Vec(w) * D);
            const gp_Dir up = towardT[i] ? side[i - 1] : side[i - 1].Reversed();
            const gp_Pnt P = towardT[i] ? e.Translated(gp_Vec(side[i - 1]) * T) : e;
            BendGeom g;
            if (!makeBend(end, P, w, up, dir[i - 1], T, turn[i], R, false, g)) return fail("Could not make a corner bend");
            if (g.outRot.Dot(dir[i]) < 1 - 1e-6) return fail("Could not line a corner bend up with the next line");
            const TopoDS_Shape next = plate(i);
            result = fuse(fuse(result, g.solid), next);
            if (result.IsNull()) return fail("Could not join a corner bend");
            SheetBend b;
            b.side = end;
            b.out = dir[i - 1];
            b.up = up;
            b.axis = g.axis;
            b.signedAngle = g.signedAngle;
            b.rot = g.rot;
            b.angle = turn[i];
            b.radius = R;
            b.thickness = T;
            b.segment = next;
            m_bends << b;
        }
    } catch (const Standard_Failure &e) {
        return fail(QString("Contour flange failed: %1").arg(e.GetMessageString()));
    }
    const int before = ctx.bodies.size();
    ctx.addBodies(result, id());
    if (ctx.bodies.size() == before) return fail("The contour made no solid");
    m_body = ctx.bodies[before].name;
    for (SheetBend &b : m_bends) b.body = m_body;
    setManipulator(q.first(), w, "distance", D);
    return true;
}

// --- Flat pattern -----------------------------------------------------------

namespace {
QString flatSummary(const SheetFlat &f) {
    double x0, y0, z0, x1, y1, z1;
    GeomUtils::boundingBox(f.flat).Get(x0, y0, z0, x1, y1, z1);
    // Size in the sheet plane: the two largest box extents
    QList<double> ext{x1 - x0, y1 - y0, z1 - z0};
    std::sort(ext.begin(), ext.end());
    QString s = QString("%1 bend%2 unfolded, %3 x %4 mm").arg(f.unfolded).arg(f.unfolded == 1 ? "" : "s")
                    .arg(ext[2], 0, 'f', 2).arg(ext[1], 0, 'f', 2);
    if (f.skipped) s += QString(" (%1 part%2 could not be placed)").arg(f.skipped).arg(f.skipped == 1 ? "" : "s");
    return s;
}
}   // namespace

bool FissionFlatPattern::compute(RebuildContext &ctx) {
    double k = 0;
    if (!number(ctx, "kFactor", k)) return false;
    if (k < 0 || k > 1) return fail("K-factor must be between 0 and 1");
    int bi = integer("body", -1);
    SheetFlat f;
    QString err;
    try {
        if (!unfoldSheet(ctx, this, bi, k, f, err)) return fail(err);
    } catch (const Standard_Failure &e) {
        return fail(QString("Flat pattern failed: %1").arg(e.GetMessageString()));
    }
    // Laid out beside the folded body
    double fx0, fy0, fz0, fx1, fy1, fz1, x0, y0, z0, x1, y1, z1;
    GeomUtils::boundingBox(ctx.bodies[bi].shape).Get(fx0, fy0, fz0, fx1, fy1, fz1);
    GeomUtils::boundingBox(f.flat).Get(x0, y0, z0, x1, y1, z1);
    gp_Trsf shift;
    shift.SetTranslation(gp_Vec(fx1 - x0 + std::max(5.0, 0.15 * (fx1 - fx0)), 0, 0));
    f.flat = moved(f.flat, shift);
    for (auto &l : f.bendLines) l = {l.first.Transformed(shift), l.second.Transformed(shift)};
    ctx.bodies.append({ctx.bodies[bi].name + " Flat", f.flat, id()});
    m_summary = flatSummary(f);
    m_flat = f;
    return true;
}

// --- Unfold / Refold ---------------------------------------------------------

bool FissionUnfold::compute(RebuildContext &ctx) {
    double k = 0;
    if (!number(ctx, "kFactor", k)) return false;
    if (k < 0 || k > 1) return fail("K-factor must be between 0 and 1");
    int bi = integer("body", -1);
    SheetFlat f;
    QString err;
    try {
        if (!unfoldSheet(ctx, this, bi, k, f, err)) return fail(err);
    } catch (const Standard_Failure &e) {
        return fail(QString("Unfold failed: %1").arg(e.GetMessageString()));
    }
    m_folded = ctx.bodies[bi].shape;
    m_body = ctx.bodies[bi].name;
    ctx.bodies[bi].shape = f.flat;
    m_summary = flatSummary(f);
    m_flat = f;
    return true;
}

bool FissionRefold::compute(RebuildContext &ctx) {
    FissionUnfold *u = nullptr;
    const QString want = str("unfold");
    for (FissionFeature *f : earlierFeatures(ctx, this))
        if (auto *c = qobject_cast<FissionUnfold *>(f))
            if (want.isEmpty() || c->id() == want) u = c;
    if (!u || u->folded().IsNull()) return fail("There is no unfolded sheet to refold");
    int bi = -1;
    for (int i = 0; i < ctx.bodies.size(); ++i)
        if (ctx.bodies[i].name == u->bodyName()) bi = i;
    if (bi < 0) return fail("The unfolded sheet no longer exists");

    const SheetFlat &flat = u->flat();
    const TopoDS_Shape now = ctx.bodies[bi].shape;
    TopoDS_Shape result = u->folded();
    int cuts = 0, adds = 0;
    try {
        const TopoDS_Shape removed = cut(flat.flat, now), added = cut(now, flat.flat);
        double x0, y0, z0, x1, y1, z1;
        GeomUtils::boundingBox(u->folded()).Get(x0, y0, z0, x1, y1, z1);
        const double L = 2 * gp_Pnt(x0, y0, z0).Distance(gp_Pnt(x1, y1, z1)) + 100;
        // Pieces in flat coordinates back to folded ones: rigid regions as they are, bend strips slice by slice
        auto refold = [&](const TopoDS_Shape &piece) {
            QList<TopoDS_Shape> out;
            if (volume(piece) < 1e-9) return out;
            for (const SheetFlat::Region &r : flat.regions) {
                if (volume(r.folded) < 1e-9) continue;
                const TopoDS_Shape pc = common(piece, moved(r.folded, r.toFlat));
                if (volume(pc) > 1e-9) out << moved(pc, r.toFlat.Inverted());
            }
            for (const SheetFlat::Strip &s : flat.strips) {
                if (s.flat.IsNull()) continue;
                const TopoDS_Shape pc = common(piece, s.flat);
                if (volume(pc) < 1e-9) continue;
                const TopoDS_Shape local = moved(pc, s.parentToFlat.Inverted());
                const int slices = std::max(1, int(std::ceil(std::fabs(s.signedAngle) / (3 * kDeg))));
                for (int i = 0; i < slices; ++i) {
                    const double t0 = s.allowance * i / slices, t1 = s.allowance * (i + 1) / slices, tm = (t0 + t1) / 2;
                    const TopoDS_Shape sl = common(local, slab(s.origin, s.out, t0, t1, L));
                    if (volume(sl) < 1e-12) continue;
                    gp_Trsf back, turn;
                    back.SetTranslation(gp_Vec(s.out) * -tm);
                    turn.SetRotation(s.axis, s.signedAngle * tm / s.allowance);
                    out << moved(sl, turn * back);
                }
            }
            return out;
        };
        const QList<TopoDS_Shape> cutPieces = refold(removed), addPieces = refold(added);
        cuts = cutPieces.size();
        adds = addPieces.size();
        if (cuts) result = cut(result, compound(cutPieces));
        if (adds && !result.IsNull()) result = fuse(result, compound(addPieces));
        if (result.IsNull()) return fail("Could not fold the changes back");
        result = GeomUtils::unify(result);
    } catch (const Standard_Failure &e) {
        return fail(QString("Refold failed: %1").arg(e.GetMessageString()));
    }
    ctx.bodies[bi].shape = result;
    m_summary = cuts + adds ? QString("%1 cut%2 and %3 addition%4 folded back").arg(cuts).arg(cuts == 1 ? "" : "s")
                                  .arg(adds).arg(adds == 1 ? "" : "s")
                            : QString("Folded back with no changes");
    return true;
}

// --- Sheet metal rule and DXF -----------------------------------------------

void FissionDocument::ensureSheetMetalRule() {
    if (!parameter("SheetThickness")) addParameter("SheetThickness", "1 mm", "mm", "Sheet metal rule: thickness");
    if (!parameter("BendRadius")) addParameter("BendRadius", "1 mm", "mm", "Sheet metal rule: inner bend radius");
    if (!parameter("KFactor")) addParameter("KFactor", "0.44", "", "Sheet metal rule: neutral axis position (0..1)");
}

QString FissionDocument::exportFlatPatternDxf(const QString &featureId, const QString &path) {
    const SheetFlat *flat = nullptr;
    for (FissionFeature *f : timeline()->features()) {
        if (!f->isHealthy() || f->isSuppressed()) continue;
        if (!featureId.isEmpty() && f->id() != featureId) continue;
        if (auto *fp = qobject_cast<FissionFlatPattern *>(f)) flat = &fp->flat();
        else if (auto *uf = qobject_cast<FissionUnfold *>(f)) flat = &uf->flat();
    }
    if (!flat || flat->flat.IsNull()) { m_lastError = "Create a flat pattern first"; return QString(); }
    // The outline is the flat's largest face looking along the sheet normal (or against it)
    TopoDS_Face top;
    double best = 0;
    for (TopExp_Explorer e(flat->flat, TopAbs_FACE); e.More(); e.Next()) {
        const TopoDS_Face f = TopoDS::Face(e.Current());
        gp_Dir n;
        if (!planeNormal(f, n) || std::fabs(n.Dot(flat->normal)) < 1 - 1e-6) continue;
        const double a = faceArea(f);
        if (a > best) { best = a; top = f; }
    }
    if (top.IsNull()) { m_lastError = "The flat pattern has no flat face"; return QString(); }
    gp_Dir n;
    planeNormal(top, n);
    const gp_Ax3 pl = BRepAdaptor_Surface(top).Plane().Position();
    FileExport::DxfWriter dxf(gp_Ax3(pl.Location(), n, pl.XDirection()));
    QString err;
    if (!dxf.open(path, {{"0", 7}, {"BEND", 1}}, &err)) { m_lastError = err; return QString(); }
    for (TopExp_Explorer e(top, TopAbs_EDGE); e.More(); e.Next())
        if (!BRep_Tool::Degenerated(TopoDS::Edge(e.Current()))) dxf.addEdge(TopoDS::Edge(e.Current()), "0");
    for (const auto &l : flat->bendLines)
        if (l.first.Distance(l.second) > 1e-6) dxf.addEdge(BRepBuilderAPI_MakeEdge(l.first, l.second).Edge(), "BEND");
    if (!dxf.close(&err)) { m_lastError = err; return QString(); }
    return path;
}
