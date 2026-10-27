// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#include "FissionLoft.h"
#include "FissionSketch.h"
#include "GeomUtils.h"
#include "TimeLimit.h"
#include <BRepAdaptor_CompCurve.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeVertex.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepExtrema_DistShapeShape.hxx>
#include <BRepGProp.hxx>
#include <BRepOffsetAPI_MakePipeShell.hxx>
#include <BRepOffsetAPI_ThruSections.hxx>
#include <BRepTools.hxx>
#include <TopExp_Explorer.hxx>
#include <GeomAPI_Interpolate.hxx>
#include <Geom_BSplineCurve.hxx>
#include <GProp_GProps.hxx>
#include <Standard_Failure.hxx>
#include <TColgp_HArray1OfPnt.hxx>
#include <TopoDS_Wire.hxx>
#include <algorithm>
#include <cmath>
#include <vector>

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

static double distToShape(const gp_Pnt &p, const TopoDS_Shape &s) {
    BRepExtrema_DistShapeShape d(BRepBuilderAPI_MakeVertex(p).Vertex(), s);
    return d.IsDone() && d.NbSolution() > 0 ? d.Value() : 1e300;
}

// Pipe-shell contact refuses a rail that merely ends on each section. Only keep a result the rail actually lies on.
static TopoDS_Shape pipeWithRail(const TopoDS_Wire &spine, const TopoDS_Wire &rail, const QList<TopoDS_Wire> &wires,
                                 bool solid) {
    const struct {
        bool curv;
        BRepFill_TypeOfContact contact;
        bool correction;
    } modes[] = {
        {true, BRepFill_ContactOnBorder, true},
        {false, BRepFill_ContactOnBorder, true},
        {true, BRepFill_Contact, true},
        {false, BRepFill_Contact, true},
    };
    for (const auto &m : modes) {
        try {
            BRepOffsetAPI_MakePipeShell ps(spine);
            ps.SetMode(rail, m.curv ? Standard_True : Standard_False, m.contact);
            for (const TopoDS_Wire &w : wires) ps.Add(w, Standard_False, m.correction ? Standard_True : Standard_False);
            ps.Build();
            if (!ps.IsDone()) continue;
            if (solid && !ps.MakeSolid()) continue;
            const TopoDS_Shape shape = ps.Shape();
            if (shape.IsNull() || GeomUtils::solids(shape).isEmpty() || !(GeomUtils::volume(shape) > 1e-6)) continue;
            BRepAdaptor_CompCurve curve(rail, Standard_True);
            const double f = curve.FirstParameter(), span = curve.LastParameter() - f;
            bool onRail = true;
            for (int i = 0; i <= 8 && onRail; ++i)
                onRail = distToShape(curve.Value(f + span * i / 8.0), shape) <= 0.15;
            if (onRail) return shape;
        } catch (const Standard_Failure &) {
        }
    }
    return {};
}

static bool sectionFrame(const TopoDS_Wire &wire, gp_Pnt &centre, gp_Dir &normal) {
    GProp_GProps props;
    BRepGProp::LinearProperties(wire, props);
    centre = props.CentreOfMass();
    BRepAdaptor_CompCurve curve(wire, Standard_True);
    const double f = curve.FirstParameter(), l = curve.LastParameter();
    if (l - f < 1e-9) return false;
    gp_XYZ n(0, 0, 0);
    gp_Pnt prev = curve.Value(f);
    const int nSample = 24;
    for (int i = 1; i <= nSample; ++i) {
        const gp_Pnt p = curve.Value(f + (l - f) * i / nSample);
        n.SetX(n.X() + (prev.Y() - p.Y()) * (prev.Z() + p.Z()));
        n.SetY(n.Y() + (prev.Z() - p.Z()) * (prev.X() + p.X()));
        n.SetZ(n.Z() + (prev.X() - p.X()) * (prev.Y() + p.Y()));
        prev = p;
    }
    if (n.Modulus() < 1e-12) return false;
    normal = gp_Dir(n);
    return true;
}

// First intersection of the rail with the plane, at a parameter strictly after minU.
static bool planeHit(const BRepAdaptor_CompCurve &rail, const gp_Pnt &origin, const gp_Dir &normal, double minU,
                     double &u, gp_Pnt &hit) {
    const double f = std::max(rail.FirstParameter(), minU);
    const double l = rail.LastParameter();
    if (l <= f + 1e-12) return false;
    auto signedDist = [&](double t) { return gp_Vec(origin, rail.Value(t)).Dot(gp_Vec(normal)); };
    double tPrev = f, dPrev = signedDist(f);
    if (std::fabs(dPrev) <= 1e-5) {
        u = f;
        hit = rail.Value(f);
        return true;
    }
    const int n = 80;
    for (int i = 1; i <= n; ++i) {
        const double t = f + (l - f) * i / n;
        const double d = signedDist(t);
        if (std::fabs(d) <= 1e-5) {
            u = t;
            hit = rail.Value(t);
            return true;
        }
        if (dPrev * d < 0) {
            double a = tPrev, b = t, da = dPrev;
            for (int k = 0; k < 40; ++k) {
                const double m = 0.5 * (a + b);
                const double dm = signedDist(m);
                if (da * dm <= 0) b = m;
                else { a = m; da = dm; }
            }
            u = 0.5 * (a + b);
            hit = rail.Value(u);
            return true;
        }
        tPrev = t;
        dPrev = d;
    }
    return false;
}

// Nearest positive hit of the ray centre + t * dir with the polygon.
static bool rayHit(const std::vector<gp_Pnt> &poly, const gp_Pnt &centre, const gp_Vec &dir, const gp_Dir &normal,
                   gp_Pnt &hit) {
    double best = 1e300;
    bool ok = false;
    const gp_Vec n(normal);
    for (size_t i = 0; i < poly.size(); ++i) {
        const gp_Pnt &a = poly[i];
        const gp_Vec ab(a, poly[(i + 1) % poly.size()]);
        const double denom = dir.Crossed(ab).Dot(n);
        if (std::fabs(denom) < 1e-12) continue;
        const gp_Vec ca(centre, a);
        const double t = ca.Crossed(ab).Dot(n) / denom;
        const double u = ca.Crossed(dir).Dot(n) / denom;
        if (t > 1e-9 && u >= -1e-6 && u <= 1.0 + 1e-6 && t < best) {
            best = t;
            hit = gp_Pnt(centre.XYZ() + dir.XYZ() * t);
            ok = true;
        }
    }
    return ok;
}

static std::vector<gp_Pnt> outlineOf(const TopoDS_Wire &wire) {
    BRepAdaptor_CompCurve curve(wire, Standard_True);
    const double f = curve.FirstParameter(), l = curve.LastParameter();
    std::vector<gp_Pnt> pts;
    const int n = 64;
    for (int i = 0; i < n; ++i) pts.push_back(curve.Value(f + (l - f) * i / n));
    std::vector<gp_Pnt> corners;
    const double angTol = 2.0 * M_PI / 180.0;
    for (size_t i = 0; i < pts.size(); ++i) {
        const gp_Pnt &prev = pts[(i + pts.size() - 1) % pts.size()];
        const gp_Pnt &next = pts[(i + 1) % pts.size()];
        const gp_Vec a(prev, pts[i]), b(pts[i], next);
        if (a.Magnitude() < 1e-8 || b.Magnitude() < 1e-8) continue;
        if (a.Angle(b) < angTol) continue;
        corners.push_back(pts[i]);
    }
    return corners.size() >= 3 ? corners : pts;
}

static TopoDS_Wire wireFromPoints(const std::vector<gp_Pnt> &pts) {
    BRepBuilderAPI_MakeWire mk;
    int added = 0;
    for (size_t i = 0; i < pts.size(); ++i) {
        const gp_Pnt &a = pts[i];
        const gp_Pnt &b = pts[(i + 1) % pts.size()];
        if (a.Distance(b) < 1e-6) continue;
        BRepBuilderAPI_MakeEdge edge(a, b);
        if (!edge.IsDone()) return {};
        mk.Add(edge.Edge());
        if (!mk.IsDone()) return {};
        ++added;
    }
    return added >= 3 && mk.IsDone() ? mk.Wire() : TopoDS_Wire();
}

// Sections turned and scaled about their centres so each station's outline passes through the rail.
// MakePipeShell's contact law fails when the rail ends on a section instead of crossing it.
static TopoDS_Shape sectionsAlongRail(const QList<TopoDS_Wire> &wires, const TopoDS_Wire &rail, bool solid,
                                      QString &error) {
    BRepAdaptor_CompCurve railCurve(rail, Standard_True);
    QList<TopoDS_Wire> stations;
    double uCursor = railCurve.FirstParameter() - 1.0;
    const QString miss = QStringLiteral("Loft failed (the rail must run through every section, in order)");
    for (int s = 0; s + 1 < wires.size(); ++s) {
        gp_Pnt ca, cb;
        gp_Dir na, nb;
        if (!sectionFrame(wires[s], ca, na) || !sectionFrame(wires[s + 1], cb, nb)) {
            error = QStringLiteral("A section has no closed outline");
            return {};
        }
        const gp_Vec travel(ca, cb);
        if (travel.Magnitude() < 1e-6) {
            error = QStringLiteral("Two sections share a centre; add a centerline");
            return {};
        }
        if (na.Dot(travel) < 0) na.Reverse();
        if (nb.Dot(travel) < 0) nb.Reverse();
        double ua = 0, ub = 0;
        gp_Pnt ha, hb;
        if (!planeHit(railCurve, ca, na, uCursor, ua, ha) || distToShape(ha, wires[s]) > 0.05
            || !planeHit(railCurve, cb, nb, ua + 1e-9, ub, hb) || distToShape(hb, wires[s + 1]) > 0.05) {
            error = miss;
            return {};
        }
        uCursor = ub;
        const std::vector<gp_Pnt> outlineA = outlineOf(wires[s]);
        const std::vector<gp_Pnt> outlineB = outlineOf(wires[s + 1]);
        gp_Vec zeroA(ca, ha);
        zeroA = zeroA - gp_Vec(na) * zeroA.Dot(gp_Vec(na));
        gp_Vec zeroB(cb, hb);
        zeroB = zeroB - gp_Vec(nb) * zeroB.Dot(gp_Vec(nb));
        if (zeroA.Magnitude() < 1e-9 || zeroB.Magnitude() < 1e-9) {
            error = miss;
            return {};
        }
        zeroA.Normalize();
        zeroB.Normalize();
        const gp_Vec yA = gp_Vec(na).Crossed(zeroA);
        const gp_Vec yB = gp_Vec(nb).Crossed(zeroB);
        const int nAng = 32;
        const int steps = 4;
        for (int step = 0; step <= steps; ++step) {
            if (step == 0 && !stations.isEmpty()) continue;
            const double t = double(step) / steps;
            const gp_Pnt c(ca.XYZ() * (1.0 - t) + cb.XYZ() * t);
            gp_Vec nv = gp_Vec(na) * (1.0 - t) + gp_Vec(nb) * t;
            if (nv.Magnitude() < 1e-9) nv = gp_Vec(na);
            const gp_Dir n(nv);
            double ui = 0;
            gp_Pnt hi;
            if (!planeHit(railCurve, c, n, ua - 1e-6, ui, hi) || ui > ub + 1e-4) {
                error = miss;
                return {};
            }
            gp_Vec zero = zeroA * (1.0 - t) + zeroB * t;
            zero = zero - gp_Vec(n) * zero.Dot(gp_Vec(n));
            if (zero.Magnitude() < 1e-9) zero = zeroA;
            zero.Normalize();
            const gp_Vec y = gp_Vec(n).Crossed(zero);
            std::vector<gp_Pnt> pts;
            pts.reserve(nAng);
            for (int i = 0; i < nAng; ++i) {
                const double ang = 2.0 * M_PI * i / nAng;
                gp_Pnt pa, pb;
                const gp_Vec dirA = zeroA * std::cos(ang) + yA * std::sin(ang);
                const gp_Vec dirB = zeroB * std::cos(ang) + yB * std::sin(ang);
                if (!rayHit(outlineA, ca, dirA, na, pa) || !rayHit(outlineB, cb, dirB, nb, pb)) {
                    error = QStringLiteral("Loft failed (profiles may be incompatible)");
                    return {};
                }
                gp_Pnt p(pa.XYZ() * (1.0 - t) + pb.XYZ() * t);
                gp_Vec inPlane(c, p);
                inPlane = inPlane - gp_Vec(n) * inPlane.Dot(gp_Vec(n));
                pts.push_back(gp_Pnt(c.XYZ() + inPlane.XYZ()));
            }
            gp_Vec from(c, pts.front());
            from = from - gp_Vec(n) * from.Dot(gp_Vec(n));
            gp_Vec to(c, hi);
            to = to - gp_Vec(n) * to.Dot(gp_Vec(n));
            if (from.Magnitude() < 1e-9 || to.Magnitude() < 1e-9) {
                error = miss;
                return {};
            }
            gp_Trsf rot, sc;
            rot.SetRotation(gp_Ax1(c, n), from.AngleWithRef(to, gp_Vec(n)));
            sc.SetScale(c, to.Magnitude() / from.Magnitude());
            const gp_Trsf tr = sc * rot;
            for (gp_Pnt &p : pts) p.Transform(tr);
            std::vector<gp_Pnt> corners;
            const double angTol = 2.0 * M_PI / 180.0;
            for (size_t i = 0; i < pts.size(); ++i) {
                const gp_Vec a(pts[(i + pts.size() - 1) % pts.size()], pts[i]);
                const gp_Vec b(pts[i], pts[(i + 1) % pts.size()]);
                if (a.Magnitude() < 1e-8 || b.Magnitude() < 1e-8) continue;
                if (a.Angle(b) < angTol) continue;
                corners.push_back(pts[i]);
            }
            if (corners.size() >= 3) pts.swap(corners);
            gp_XYZ area(0, 0, 0);
            for (size_t i = 0; i < pts.size(); ++i) {
                const gp_Pnt &a = pts[i];
                const gp_Pnt &b = pts[(i + 1) % pts.size()];
                area.SetX(area.X() + (a.Y() - b.Y()) * (a.Z() + b.Z()));
                area.SetY(area.Y() + (a.Z() - b.Z()) * (a.X() + b.X()));
                area.SetZ(area.Z() + (a.X() - b.X()) * (a.Y() + b.Y()));
            }
            if (gp_Vec(area).Dot(gp_Vec(n)) < 0) std::reverse(pts.begin(), pts.end());
            const TopoDS_Wire station = wireFromPoints(pts);
            if (station.IsNull()) {
                error = miss;
                return {};
            }
            stations << station;
        }
    }
    try {
        BRepOffsetAPI_ThruSections loft(solid, Standard_False, 1e-6);
        for (const TopoDS_Wire &w : stations) loft.AddWire(w);
        loft.CheckCompatibility(Standard_True);
        loft.Build();
        if (!loft.IsDone() || loft.Shape().IsNull()) {
            error = miss;
            return {};
        }
        return loft.Shape();
    } catch (const Standard_Failure &) {
        error = miss;
        return {};
    }
}

bool FissionLoft::compute(RebuildContext &ctx) {
    const QVariantList sections = list("sections");
    if (sections.size() < 2) return fail("Select at least two profiles or faces");

    QList<TopoDS_Wire> wires;
    for (int i = 0; i < sections.size(); ++i) {
        const QVariantMap s = sections[i].toMap();
        const QString label = QString("Section %1").arg(i + 1);
        TopoDS_Wire wire;
        TopoDS_Face sectionFace;
        if (s.value("kind").toString() == "face") {
            int body = -1;
            QString err;
            if (!ctx.resolveFace(s, body, sectionFace, err)) return fail(label + ": " + err);
            wire = BRepTools::OuterWire(sectionFace);
        } else {
            auto *sketch = qobject_cast<FissionSketch *>(ctx.feature(s.value("sketch").toString()));
            if (!sketch) return fail(label + ": sketch not found");
            if (!sketch->isHealthy()) return fail(label + ": sketch '" + sketch->name() + "' has errors");
            int profile = s.value("profile", -1).toInt();
            if (profile < 0) profile = sketch->defaultProfiles().isEmpty() ? -1 : sketch->defaultProfiles().first();
            if (profile < 0 || profile >= sketch->profileCount())
                return fail(label + ": sketch '" + sketch->name() + "' has no such profile");
            sectionFace = sketch->profiles()[profile];
            wire = BRepTools::OuterWire(sectionFace);
        }
        if (wire.IsNull()) return fail(label + " has no closed outline");
        // A ring (profile with a hole) used to loft as a solid disc with no error. Refuse it until
        // inner wires are lofted as cuts.
        if (!sectionFace.IsNull()) {
            int wiresOnFace = 0;
            for (TopExp_Explorer exp(sectionFace, TopAbs_WIRE); exp.More(); exp.Next()) ++wiresOnFace;
            if (wiresOnFace > 1)
                return fail(label + " has a hole — loft of ring sections is not supported yet");
        }
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
            BRepOffsetAPI_MakePipeShell ps(spine);
            for (const TopoDS_Wire &w : wires) ps.Add(w, Standard_False, Standard_False);
            ps.Build();
            if (!ps.IsDone()) return fail("Loft failed (the centerline must run through every section, in order)");
            if (flag("solid", true)) ps.MakeSolid();
            result = ps.Shape();
        } else {
            if (!guideWire("rail", "rail", rail)) return false;
            // Contact mode never sees a rail that stops on the first and last sections. Follow the rail by
            // scaling each station so its outline passes through the rail, and only then loft.
            QString spineErr;
            spine = centreSpine(wires, spineErr);
            if (!spine.IsNull()) result = pipeWithRail(spine, rail, wires, flag("solid", true));
            if (result.IsNull()) {
                result = sectionsAlongRail(wires, rail, flag("solid", true), err);
                if (result.IsNull()) return fail(err.isEmpty() ? QStringLiteral("Loft failed (the rail must run through every section, in order)") : err);
            }
        }
    } else {
        BRepOffsetAPI_ThruSections loft(flag("solid", true), flag("ruled"), 1e-6);
        for (const TopoDS_Wire &w : wires) loft.AddWire(w);
        loft.CheckCompatibility(Standard_True);
        Handle(GeomUtils::TimeLimit) limit = new GeomUtils::TimeLimit(20000);
        loft.Build(limit->Start());
        if (limit->expired()) return fail("Loft took longer than 20 s and was stopped");
        if (!loft.IsDone()) return fail("Loft failed (profiles may be incompatible)");
        result = loft.Shape();
    }

    QString err;
    if (!ctx.applyOperation(result, operation(), list("targets"), id(), err)) return fail(err);
    return true;
}
