// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#include "FissionHole.h"
#include "FissionDocument.h"
#include "FissionSketch.h"
#include "GeomUtils.h"

#include <BRepAdaptor_Curve.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRep_Tool.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <gp_Pln.hxx>
#include <QVector3D>
#include <optional>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepLib.hxx>
#include <BRepOffsetAPI_MakePipeShell.hxx>
#include <BRepPrimAPI_MakeRevol.hxx>
#include <BRepTools.hxx>
#include <Geom2d_Line.hxx>
#include <Geom_CylindricalSurface.hxx>
#include <gp_Cylinder.hxx>
#include <cmath>

namespace ThreadUtils {

double coarsePitch(double d) {
    double best = 0.35, bestD = 1e300;
    for (const ThreadSize &s : threadSizes("isoCoarse")) {
        const double dd = std::fabs(s.major - d);
        if (dd < bestD) { bestD = dd; best = s.pitch; }
    }
    return best;
}

namespace {

// ISO 273 clearance holes (fine, medium, coarse) by nominal diameter
bool metricClearance(double d, double out[3]) {
    static const double table[][4] = {
        {1.6, 1.7, 1.8, 2.0}, {2, 2.2, 2.4, 2.6}, {2.5, 2.7, 2.9, 3.1}, {3, 3.2, 3.4, 3.6}, {4, 4.3, 4.5, 4.8},
        {5, 5.3, 5.5, 5.8}, {6, 6.4, 6.6, 7.0}, {8, 8.4, 9.0, 10.0}, {10, 10.5, 11.0, 12.0}, {12, 13.0, 13.5, 14.5},
        {14, 15.0, 15.5, 16.5}, {16, 17.0, 17.5, 18.5}, {18, 19.0, 20.0, 21.0}, {20, 21.0, 22.0, 24.0},
        {22, 23.0, 24.0, 26.0}, {24, 25.0, 26.0, 28.0}, {27, 28.0, 30.0, 32.0}, {30, 31.0, 33.0, 35.0},
        {33, 34.0, 36.0, 38.0}, {36, 37.0, 39.0, 42.0}, {39, 40.0, 42.0, 45.0}, {42, 43.0, 45.0, 48.0},
        {48, 50.0, 52.0, 56.0}, {56, 58.0, 62.0, 66.0}, {64, 66.0, 70.0, 74.0}};
    for (const auto &row : table)
        if (std::fabs(row[0] - d) < 1e-6) {
            out[0] = row[1]; out[1] = row[2]; out[2] = row[3];
            return true;
        }
    out[0] = d * 1.06; out[1] = d * 1.1; out[2] = d * 1.2;
    return false;
}

QList<ThreadSize> buildTable() {
    QList<ThreadSize> t;
    auto metric = [&](const char *std, double d, double p, bool fine) {
        ThreadSize s;
        s.standard = std;
        s.name = fine ? QString("M%1x%2").arg(d, 0, 'g', 4).arg(p, 0, 'g', 3) : QString("M%1").arg(d, 0, 'g', 4);
        s.major = d;
        s.pitch = p;
        metricClearance(d, s.clearance);
        t << s;
    };
    static const double coarse[][2] = {{1.6, 0.35}, {2, 0.4}, {2.5, 0.45}, {3, 0.5}, {4, 0.7}, {5, 0.8}, {6, 1.0},
                                       {8, 1.25}, {10, 1.5}, {12, 1.75}, {14, 2.0}, {16, 2.0}, {18, 2.5}, {20, 2.5},
                                       {22, 2.5}, {24, 3.0}, {27, 3.0}, {30, 3.5}, {33, 3.5}, {36, 4.0}, {39, 4.0},
                                       {42, 4.5}, {48, 5.0}, {56, 5.5}, {64, 6.0}};
    for (const auto &r : coarse) metric("isoCoarse", r[0], r[1], false);
    static const double fine[][2] = {{3, 0.35}, {4, 0.5}, {5, 0.5}, {6, 0.75}, {8, 1.0}, {10, 1.25}, {10, 1.0},
                                     {12, 1.5}, {12, 1.25}, {14, 1.5}, {16, 1.5}, {18, 1.5}, {20, 1.5}, {20, 2.0},
                                     {22, 1.5}, {24, 2.0}, {27, 2.0}, {30, 2.0}, {33, 2.0}, {36, 3.0}, {39, 3.0},
                                     {42, 3.0}, {48, 3.0}};
    for (const auto &r : fine) metric("isoFine", r[0], r[1], true);

    // Unified threads: size, major (in), threads per inch, clearance close / normal / loose (in)
    struct Inch { const char *size; double major; int tpi; double c[3]; };
    auto inch = [&](const char *std, const Inch &r) {
        ThreadSize s;
        s.standard = std;
        s.name = QString("%1-%2").arg(r.size).arg(r.tpi);
        s.major = r.major * 25.4;
        s.pitch = 25.4 / r.tpi;
        for (int k = 0; k < 3; ++k) s.clearance[k] = r.c[k] * 25.4;
        t << s;
    };
    static const Inch unc[] = {
        {"#2", 0.086, 56, {0.094, 0.102, 0.116}}, {"#4", 0.112, 40, {0.116, 0.125, 0.140}},
        {"#5", 0.125, 40, {0.128, 0.136, 0.150}}, {"#6", 0.138, 32, {0.144, 0.149, 0.165}},
        {"#8", 0.164, 32, {0.170, 0.177, 0.191}}, {"#10", 0.190, 24, {0.196, 0.201, 0.221}},
        {"#12", 0.216, 24, {0.221, 0.228, 0.250}}, {"1/4", 0.250, 20, {0.257, 0.266, 0.281}},
        {"5/16", 0.3125, 18, {0.323, 0.332, 0.344}}, {"3/8", 0.375, 16, {0.386, 0.397, 0.406}},
        {"7/16", 0.4375, 14, {0.453, 0.469, 0.484}}, {"1/2", 0.500, 13, {0.516, 0.531, 0.562}},
        {"9/16", 0.5625, 12, {0.578, 0.594, 0.625}}, {"5/8", 0.625, 11, {0.641, 0.656, 0.688}},
        {"3/4", 0.750, 10, {0.766, 0.781, 0.812}}, {"7/8", 0.875, 9, {0.891, 0.906, 0.938}},
        {"1", 1.000, 8, {1.016, 1.031, 1.062}}};
    for (const Inch &r : unc) inch("unc", r);
    static const Inch unf[] = {
        {"#2", 0.086, 64, {0.094, 0.102, 0.116}}, {"#4", 0.112, 48, {0.116, 0.125, 0.140}},
        {"#6", 0.138, 40, {0.144, 0.149, 0.165}}, {"#8", 0.164, 36, {0.170, 0.177, 0.191}},
        {"#10", 0.190, 32, {0.196, 0.201, 0.221}}, {"#12", 0.216, 28, {0.221, 0.228, 0.250}},
        {"1/4", 0.250, 28, {0.257, 0.266, 0.281}}, {"5/16", 0.3125, 24, {0.323, 0.332, 0.344}},
        {"3/8", 0.375, 24, {0.386, 0.397, 0.406}}, {"7/16", 0.4375, 20, {0.453, 0.469, 0.484}},
        {"1/2", 0.500, 20, {0.516, 0.531, 0.562}}, {"9/16", 0.5625, 18, {0.578, 0.594, 0.625}},
        {"5/8", 0.625, 18, {0.641, 0.656, 0.688}}, {"3/4", 0.750, 16, {0.766, 0.781, 0.812}},
        {"7/8", 0.875, 14, {0.891, 0.906, 0.938}}, {"1", 1.000, 12, {1.016, 1.031, 1.062}}};
    for (const Inch &r : unf) inch("unf", r);
    return t;
}

const QList<ThreadSize> &allThreads() {
    static const QList<ThreadSize> table = buildTable();
    return table;
}

}   // namespace

QString ThreadSize::designation() const {
    if (standard == "unc") return name + " UNC";
    if (standard == "unf") return name + " UNF";
    return standard == "isoCoarse" ? QString("M%1x%2").arg(major, 0, 'g', 4).arg(pitch, 0, 'g', 3) : name;
}

QList<ThreadSize> threadSizes(const QString &standard) {
    QList<ThreadSize> out;
    for (const ThreadSize &s : allThreads())
        if (s.standard == standard) out << s;
    return out;
}

bool findThread(const QString &standard, const QString &name, ThreadSize &out) {
    const QString want = name.trimmed().toUpper().remove(' ');
    for (const ThreadSize &s : allThreads())
        if (s.standard == standard && s.name.toUpper() == want) { out = s; return true; }
    return false;
}

bool nearestThread(const QString &standard, double major, ThreadSize &out) {
    double best = 1e300;
    for (const ThreadSize &s : allThreads()) {
        if (s.standard != standard) continue;
        const double d = std::fabs(s.major - major);
        if (d < best) { best = d; out = s; }
    }
    return best < 1e300;
}

int fitIndex(const QString &fit) { return fit == "close" ? 0 : fit == "loose" ? 2 : 1; }

TopoDS_Shape threadTool(const gp_Ax3 &axis, double radius, double length, double pitch, bool internal) {
    if (radius <= 0 || length <= 0 || pitch <= 0) return {};
    const double depth = 0.6134 * pitch;                 // ISO 68 thread depth
    const double half = depth * std::tan(M_PI / 6.0);    // 60° flank angle
    const double eps = 0.05 * pitch;                     // start the groove just outside the material
    const gp_Pnt o = axis.Location();
    const gp_Vec X(axis.XDirection()), Z(axis.Direction());
    auto P = [&](double r, double z) { return o.Translated(X * r + Z * z); };
    const double rs = internal ? radius - eps : radius + eps;
    const double rt = internal ? radius + depth : radius - depth;
    BRepBuilderAPI_MakePolygon poly(P(rs, -half - eps * 0.6), P(rs, half + eps * 0.6), P(rt, 0), Standard_True);
    const TopoDS_Wire profile = poly.Wire();

    // Helix as a straight line in the (angle, height) parameter space of a cylinder
    Handle(Geom_CylindricalSurface) cyl = new Geom_CylindricalSurface(axis, radius);
    const double turns = length / pitch;
    const gp_Dir2d d(2 * M_PI, pitch);
    Handle(Geom2d_Line) line = new Geom2d_Line(gp_Pnt2d(0, 0), d);
    const double sMax = turns * std::sqrt(4 * M_PI * M_PI + pitch * pitch);
    TopoDS_Edge helix = BRepBuilderAPI_MakeEdge(line, cyl, 0, sMax).Edge();
    BRepLib::BuildCurves3d(helix);
    const TopoDS_Wire path = BRepBuilderAPI_MakeWire(helix).Wire();

    BRepOffsetAPI_MakePipeShell pipe(path);
    pipe.SetMode(axis.Direction());   // fixed binormal keeps the profile in a plane through the axis
    pipe.Add(profile, Standard_False, Standard_False);
    pipe.Build();
    if (!pipe.IsDone()) return {};
    if (!pipe.MakeSolid()) return {};
    return pipe.Shape();
}

}   // namespace ThreadUtils

QVariantList FissionDocument::threadSizes(const QString &standard) const {
    QVariantList out;
    for (const ThreadUtils::ThreadSize &s : ThreadUtils::threadSizes(standard))
        out << QVariantMap{{"name", s.name}, {"major", s.major}, {"pitch", s.pitch}, {"tapDrill", s.tapDrill()},
                           {"close", s.clearance[0]}, {"normal", s.clearance[1]}, {"loose", s.clearance[2]}};
    return out;
}

namespace {

gp_Dir towardBodies(const RebuildContext &ctx, const gp_Pnt &at, gp_Dir dir) {
    if (ctx.bodies.isEmpty()) return dir;
    Bnd_Box all;
    for (const auto &b : ctx.bodies) all.Add(GeomUtils::boundingBox(b.shape));
    double x0, y0, z0, x1, y1, z1;
    all.Get(x0, y0, z0, x1, y1, z1);
    const gp_Pnt c((x0 + x1) / 2, (y0 + y1) / 2, (z0 + z1) / 2);
    if (gp_Vec(at, c).Dot(gp_Vec(dir)) < 0) dir.Reverse();
    return dir;
}

}   // namespace

QVariantList FissionHole::faceSnaps(const RebuildContext &ctx, const gp_Ax3 &plane) const {
    QVariantList out;
    auto add = [&](const gp_Pnt &p, const QString &label) {
        for (const QVariant &v : out) {
            const QVariantList q = v.toList();
            if (gp_Pnt(q[0].toDouble(), q[1].toDouble(), q[2].toDouble()).Distance(p) < 1e-6) return;
        }
        out << QVariant(QVariantList{p.X(), p.Y(), p.Z(), label});
    };
    const gp_Pln pln(plane);
    int body = -1;
    TopoDS_Face face;
    QString err;
    if (ctx.resolveFace(get("face"), body, face, err)) {
        add(GeomUtils::faceCentroid(face), "Face center");
        for (TopExp_Explorer e(face, TopAbs_EDGE); e.More(); e.Next()) {
            BRepAdaptor_Curve c(TopoDS::Edge(e.Current()));
            if (c.GetType() == GeomAbs_Circle && pln.Distance(c.Circle().Location()) < 1e-6)
                add(c.Circle().Location(), "Circle center");
        }
        for (TopExp_Explorer e(face, TopAbs_VERTEX); e.More(); e.Next())
            add(BRep_Tool::Pnt(TopoDS::Vertex(e.Current())), "Vertex");
    }
    // Points of shown sketches lying in the face plane
    for (FissionFeature *f : ctx.features()) {
        auto *sk = qobject_cast<FissionSketch *>(f);
        if (!sk || !sk->isVisible()) continue;
        const gp_Ax3 sp = sk->plane();
        if (!sp.Direction().IsParallel(plane.Direction(), 1e-6) || pln.Distance(sp.Location()) > 1e-6) continue;
        for (const QVariant &hv : sk->handles()) {
            const QVariantMap h = hv.toMap();
            add(sp.Location().Translated(gp_Vec(sp.XDirection()) * h.value("u").toDouble()
                                         + gp_Vec(sp.YDirection()) * h.value("v").toDouble()),
                sk->name() + " point");
        }
    }
    return out;
}

bool FissionHole::compute(RebuildContext &ctx) {
    QString err;
    QList<gp_Pnt> centers;
    gp_Dir drill(0, 0, -1);
    QVariantList moveSnaps;
    std::optional<gp_Ax3> movePlane;

    auto *sk = qobject_cast<FissionSketch *>(ctx.feature(str("sketch")));
    if (sk) {
        const gp_Ax3 pl = sk->plane();
        const QVariantList ents = sk->get("entities").toList();
        for (const QVariant &hv : sk->handles()) {
            const QVariantMap h = hv.toMap();
            const int e = h.value("entity").toInt();
            const QVariantMap ent = ents.value(e).toMap();
            if (ent.value("type") != "point" || ent.value("construction").toBool()) continue;
            centers << pl.Location().Translated(gp_Vec(pl.XDirection()) * h.value("u").toDouble()
                                                + gp_Vec(pl.YDirection()) * h.value("v").toDouble());
        }
        if (centers.isEmpty()) return fail("The sketch has no points: add sketch points where the holes go");
        drill = towardBodies(ctx, pl.Location(), pl.Direction());
    } else {
        if (get("face").typeId() != QMetaType::QVariantMap) return fail("Click a flat face to place the hole");
        gp_Ax3 ax;
        if (!ctx.resolvePlane(get("face"), ax, err)) return fail(err);
        drill = ax.Direction().Reversed();
        gp_Pnt c = ax.Location();
        const QVariantList cv = list("center");
        if (cv.size() == 3) {
            double xyz[3];
            for (int k = 0; k < 3; ++k) {
                xyz[k] = ctx.evaluate(cv[k].toString(), &err);
                if (!err.isEmpty()) return fail("Position: " + err);
            }
            c = gp_Pnt(xyz[0], xyz[1], xyz[2]);
        }
        const gp_Vec n(ax.Direction());
        c.Translate(n * -gp_Vec(ax.Location(), c).Dot(n));   // onto the face plane
        centers << c;
        moveSnaps = faceSnaps(ctx, ax);
        movePlane = ax;
    }
    if (flag("flip")) drill.Reverse();

    double D = 0, depth = 0, tip = 0;
    // A standard size sets the diameter: the tap drill (basic minor) when tapped, else the clearance hole
    const QString standard = str("standard", "custom");
    ThreadUtils::ThreadSize size;
    const bool sized = standard != "custom";
    if (sized) {
        if (!ThreadUtils::findThread(standard, str("size"), size)) return fail("Pick a size for the standard");
        D = flag("tapped") ? size.tapDrill() : size.clearance[ThreadUtils::fitIndex(str("fit", "normal"))];
        m_designation = flag("tapped") ? size.designation() + " tapped"
                                       : QString("%1 clearance, %2 \u00d8%3 mm").arg(size.name, str("fit", "normal"))
                                             .arg(D, 0, 'f', 2);
    } else if (!number(ctx, "diameter", D)) {
        return false;
    }
    if (D <= 0) return fail("Diameter must be positive");
    const bool all = str("extent", "distance") == "all";
    if (all) {
        depth = ctx.modelSize() * 4;
    } else {
        if (!number(ctx, "depth", depth) || !number(ctx, "tipAngle", tip)) return false;
        if (depth <= 0) return fail("Depth must be positive");
        if (tip < 0 || tip >= 180) return fail("Tip angle must be between 0 (flat) and 180 degrees");
    }
    const double R = D / 2, e = 0.05;
    QList<QPair<double, double>> rz{{0, -e}};
    double threadStart = 0;
    const QString kind = str("holeType", "simple");
    if (kind == "counterbore") {
        double Dc = 0, dc = 0;
        if (!number(ctx, "cboreDiameter", Dc) || !number(ctx, "cboreDepth", dc)) return false;
        if (Dc <= D) return fail("Counterbore diameter must be larger than the hole");
        if (dc <= 0 || dc >= depth) return fail("Counterbore depth must be between 0 and the hole depth");
        rz << qMakePair(Dc / 2, -e) << qMakePair(Dc / 2, dc) << qMakePair(R, dc);
        threadStart = dc;
    } else if (kind == "countersink") {
        double Ds = 0, a = 0;
        if (!number(ctx, "csinkDiameter", Ds) || !number(ctx, "csinkAngle", a)) return false;
        if (Ds <= D) return fail("Countersink diameter must be larger than the hole");
        if (a <= 0 || a >= 180) return fail("Countersink angle must be between 0 and 180 degrees");
        const double zc = (Ds / 2 - R) / std::tan(a * M_PI / 360.0);
        if (zc >= depth) return fail("The countersink is deeper than the hole");
        rz << qMakePair(Ds / 2, -e) << qMakePair(Ds / 2, 0.0) << qMakePair(R, zc);
        threadStart = zc;
    } else {
        rz << qMakePair(R, -e);
    }
    rz << qMakePair(R, depth);
    if (!all && tip > 0) rz << qMakePair(0.0, depth + R / std::tan(tip * M_PI / 360.0));
    else rz << qMakePair(0.0, depth);

    QList<TopoDS_Shape> holes, threads;
    double pitch = 0;
    const bool cutThread = flag("tapped") && flag("modeled", true);
    if (flag("tapped")) {
        pitch = sized ? size.pitch : ThreadUtils::coarsePitch(D);
        if (!sized && !str("pitch").trimmed().isEmpty() && !number(ctx, "pitch", pitch)) return false;
        if (pitch <= 0) return fail("Pitch must be positive");
    }
    for (const gp_Pnt &c : centers) {
        const gp_Ax3 frame(c, drill);
        const gp_Vec X(frame.XDirection()), Z(drill);
        BRepBuilderAPI_MakePolygon poly;
        for (const auto &p : rz) poly.Add(c.Translated(X * p.first + Z * p.second));
        poly.Close();
        const TopoDS_Face f = BRepBuilderAPI_MakeFace(poly.Wire(), Standard_True).Face();
        BRepPrimAPI_MakeRevol rev(f, gp_Ax1(c, drill), 2 * M_PI, Standard_True);
        rev.Build();
        if (!rev.IsDone()) return fail("Could not build the hole");
        holes << rev.Shape();
        if (cutThread) {
            // Runs out above the face; stops a pitch short of the bottom like a tap
            const double z0 = threadStart - pitch;
            double end = depth - pitch;
            if (all) {
                Bnd_Box bb;
                for (const auto &b : ctx.bodies) bb.Add(GeomUtils::boundingBox(b.shape));
                double x0, y0, zz0, x1, y1, zz1;
                bb.Get(x0, y0, zz0, x1, y1, zz1);
                end = 0;
                for (double x : {x0, x1})
                    for (double y : {y0, y1})
                        for (double z : {zz0, zz1}) end = std::max(end, gp_Vec(c, gp_Pnt(x, y, z)).Dot(Z) + pitch);
            }
            const double len = end - z0;
            if (len < pitch) return fail("The hole is too shallow for a thread of this pitch");
            const TopoDS_Shape t = ThreadUtils::threadTool(gp_Ax3(c.Translated(Z * z0), drill, frame.XDirection()),
                                                           R, len, pitch, true);
            if (t.IsNull()) return fail("Could not build the thread");
            threads << t;
        }
    }
    setManipulator(centers.first(), drill, "depth", depth);
    if (movePlane) setMoveManipulator(centers.first(), movePlane->Direction(), "center", moveSnaps);
    if (!ctx.applyOperation(GeomUtils::makeCompound(holes), FeatureOperation::Cut, {}, id(), err)) return fail(err);
    if (!threads.isEmpty() && !ctx.applyOperation(GeomUtils::makeCompound(threads), FeatureOperation::Cut, {}, id(), err))
        return fail("Thread: " + err);
    return true;
}

bool FissionThread::compute(RebuildContext &ctx) {
    QString err;
    if (get("face").typeId() != QMetaType::QVariantMap) return fail("Select a cylindrical face");
    int body = -1;
    TopoDS_Face face;
    if (!ctx.resolveFace(get("face"), body, face, err)) return fail(err);
    BRepAdaptor_Surface s(face);
    if (s.GetType() != GeomAbs_Cylinder) return fail("Select a cylindrical face");
    const gp_Cylinder cyl = s.Cylinder();
    const double R = cyl.Radius();
    double u0, u1, v0, v1;
    BRepTools::UVBounds(face, u0, u1, v0, v1);

    // Outward face normal pointing away from the axis = a shaft (external thread)
    gp_Pnt p;
    gp_Vec du, dv;
    s.D1((u0 + u1) / 2, (v0 + v1) / 2, p, du, dv);
    gp_Vec n = du.Crossed(dv);
    if (face.Orientation() == TopAbs_REVERSED) n.Reverse();
    const gp_Ax1 ax = cyl.Axis();
    const gp_Vec radial = gp_Vec(ax.Location(), p) - gp_Vec(ax.Direction()) * gp_Vec(ax.Location(), p).Dot(gp_Vec(ax.Direction()));
    const bool internal = n.Dot(radial) < 0;

    double pitch = ThreadUtils::coarsePitch(2 * R);
    const QString standard = str("standard", "custom");
    ThreadUtils::ThreadSize size;
    const bool sized = standard != "custom";
    if (sized) {
        // An explicit size, or the one that fits this face: its major diameter (a hole is the minor one)
        if (str("size").trimmed().isEmpty()) {
            const double guess = internal ? 2 * R + 1.0825 * pitch : 2 * R;
            if (!ThreadUtils::nearestThread(standard, guess, size)) return fail("No sizes in this standard");
            if (internal) ThreadUtils::nearestThread(standard, 2 * R + 1.0825 * size.pitch, size);
        } else if (!ThreadUtils::findThread(standard, str("size"), size)) {
            return fail("Unknown size " + str("size"));
        }
        pitch = size.pitch;
    } else if (!str("pitch").trimmed().isEmpty() && !number(ctx, "pitch", pitch)) {
        return false;
    }
    if (pitch <= 0) return fail("Pitch must be positive");
    double offset = 0, length = v1 - v0;
    if (!number(ctx, "offset", offset)) return false;
    if (!str("length").trimmed().isEmpty() && !number(ctx, "length", length)) return false;
    length = std::min(length, v1 - v0 - offset);
    if (length <= pitch * 0.5) return fail("The thread is shorter than half a pitch");
    m_designation = sized ? QString("%1 %2").arg(size.designation(), internal ? "internal" : "external")
                          : QString("M%1x%2 %3").arg(2 * R, 0, 'g', 4).arg(pitch, 0, 'g', 3).arg(internal ? "internal" : "external");
    if (!flag("modeled", true)) return true;

    const gp_Ax3 pos = cyl.Position();
    const gp_Ax3 start(pos.Location().Translated(gp_Vec(pos.Direction()) * (v0 + offset)), pos.Direction(), pos.XDirection());
    const TopoDS_Shape tool = ThreadUtils::threadTool(start, R, length, pitch, internal);
    if (tool.IsNull()) return fail("Could not build the thread");
    BRepAlgoAPI_Cut cut(ctx.bodies[body].shape, tool);
    if (!cut.IsDone() || cut.HasErrors()) return fail("Thread failed");
    const QList<TopoDS_Shape> parts = GeomUtils::solids(cut.Shape());
    if (parts.isEmpty()) return fail("Thread failed");
    ctx.bodies[body].shape = parts.first();
    return true;
}
