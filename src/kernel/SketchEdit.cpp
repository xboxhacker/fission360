// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
// Sketch editing tools: drag, trim, extend, offset, fillet, project and DXF export
#include "FileExport.h"
#include "FissionDocument.h"
#include "FissionSketch.h"
#include "GeomUtils.h"
#include "SketchSolver.h"

#include <BRepAdaptor_Curve.hxx>
#include <BRep_Tool.hxx>
#include <GCE2d_MakeSegment.hxx>
#include <Geom2dAPI_InterCurveCurve.hxx>
#include <Geom2d_Circle.hxx>
#include <Geom2d_TrimmedCurve.hxx>
#include <GeomAPI.hxx>
#include <Geom_TrimmedCurve.hxx>
#include <Standard_Failure.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopoDS.hxx>
#include <gp_Ax2d.hxx>
#include <gp_Pln.hxx>

#include <QFile>
#include <QTextStream>
#include <algorithm>
#include <cmath>

namespace {

using P = SketchSolver::Point;
const char *const kRefs[] = {"a", "b", "c"};

double r6(double v) { return std::round(v * 1e6) / 1e6 + 0.0; }
double ccwDeg(double from, double to) {
    const double a = std::fmod(to - from, 360.0);
    return a < 0 ? a + 360.0 : a;
}
double angDeg(const P &c, const P &p) { return std::atan2(p.v - c.v, p.u - c.u) * 180.0 / M_PI; }
double dist(const P &a, const P &b) { return std::hypot(a.u - b.u, a.v - b.v); }
P add(const P &a, const P &b, double k = 1.0) { return {a.u + b.u * k, a.v + b.v * k}; }
P sub(const P &a, const P &b) { return {a.u - b.u, a.v - b.v}; }
P unit(const P &a) {
    const double l = std::hypot(a.u, a.v);
    return l > 1e-15 ? P{a.u / l, a.v / l} : P{1, 0};
}
double cross(const P &a, const P &b) { return a.u * b.v - a.v * b.u; }
double dot(const P &a, const P &b) { return a.u * b.u + a.v * b.v; }
P onCircle(const P &c, double r, double deg) {
    return {c.u + r * std::cos(deg * M_PI / 180.0), c.v + r * std::sin(deg * M_PI / 180.0)};
}

QVariantMap lineMap(const P &a, const P &b) {
    return {{"type", "line"}, {"x1", r6(a.u)}, {"y1", r6(a.v)}, {"x2", r6(b.u)}, {"y2", r6(b.v)}};
}
QVariantMap arcMap(const P &c, double r, double a1, double a2) {
    while (a1 > 180.0) { a1 -= 360.0; a2 -= 360.0; }
    while (a1 <= -180.0) { a1 += 360.0; a2 += 360.0; }
    while (a2 <= a1) a2 += 360.0;
    return {{"type", "arc"}, {"cx", r6(c.u)}, {"cy", r6(c.v)}, {"r", r6(r)}, {"a1", r6(a1)}, {"a2", r6(a2)}};
}
QVariantMap circleMap(const P &c, double r) {
    return {{"type", "circle"}, {"cx", r6(c.u)}, {"cy", r6(c.v)}, {"r", r6(r)}};
}

// Counter-clockwise arc through a, m, b: center, radius and start / end angles (deg)
bool arcThrough(const P &a, const P &m, const P &b, P &c, double &r, double &a1, double &a2) {
    const double d = 2 * (a.u * (m.v - b.v) + m.u * (b.v - a.v) + b.u * (a.v - m.v));
    if (std::fabs(d) < 1e-12) return false;
    const double sa = a.u * a.u + a.v * a.v, sm = m.u * m.u + m.v * m.v, sb = b.u * b.u + b.v * b.v;
    c = {(sa * (m.v - b.v) + sm * (b.v - a.v) + sb * (a.v - m.v)) / d,
         (sa * (b.u - m.u) + sm * (a.u - b.u) + sb * (m.u - a.u)) / d};
    r = dist(a, c);
    a1 = angDeg(c, a);
    a2 = angDeg(c, b);
    if (ccwDeg(a1, angDeg(c, m)) > ccwDeg(a1, a2)) std::swap(a1, a2);
    while (a2 <= a1) a2 += 360.0;
    return true;
}

// Intersections of an (infinite) line p + t*d with a circle
QList<P> lineCircle(const P &p, const P &d, const P &c, double r) {
    const P u = unit(d);
    const double t0 = dot(sub(c, p), u);
    const P foot = add(p, u, t0);
    const double h2 = r * r - std::pow(dist(foot, c), 2);
    if (h2 < -1e-6 * std::max(1.0, r * r)) return {};
    const double h = std::sqrt(std::max(0.0, h2));
    if (h < 1e-9) return {foot};
    return {add(foot, u, h), add(foot, u, -h)};
}
QList<P> circleCircle(const P &c1, double r1, const P &c2, double r2) {
    const double d = dist(c1, c2);
    if (d < 1e-12) return {};
    const double a = (r1 * r1 - r2 * r2 + d * d) / (2 * d);
    const double h2 = r1 * r1 - a * a;
    if (h2 < -1e-6 * std::max(1.0, r1 * r1)) return {};
    const P u = unit(sub(c2, c1)), m = add(c1, u, a);
    const double h = std::sqrt(std::max(0.0, h2));
    if (h < 1e-9) return {m};
    const P n{-u.v, u.u};
    return {add(m, n, h), add(m, n, -h)};
}

QList<gp_Pnt2d> crossings(const Handle(Geom2d_Curve) &target, const QList<QList<Handle(Geom2d_Curve)>> &curves,
                          int skip) {
    QList<gp_Pnt2d> pts;
    for (int i = 0; i < curves.size(); ++i) {
        if (i == skip) continue;
        for (const Handle(Geom2d_Curve) &c : curves[i]) {
            try {
                Geom2dAPI_InterCurveCurve x(target, c, 1e-7);
                for (int k = 1; k <= x.NbPoints(); ++k) {
                    const gp_Pnt2d p = x.Point(k);
                    if (std::none_of(pts.begin(), pts.end(), [&](const gp_Pnt2d &q) { return q.Distance(p) < 1e-7; }))
                        pts.append(p);
                }
            } catch (const Standard_Failure &) {
            }
        }
    }
    return pts;
}

}   // namespace

// --- helpers -----------------------------------------------------------------

QList<QList<Handle(Geom2d_Curve)>> FissionSketch::curves2d() const {
    QList<QList<Handle(Geom2d_Curve)>> out;
    const gp_Pln pln(m_plane);
    for (const auto &edges : m_entityEdges) {
        QList<Handle(Geom2d_Curve)> list;
        for (const TopoDS_Edge &e : edges) {
            Standard_Real f, l;
            Handle(Geom_Curve) c = BRep_Tool::Curve(e, f, l);
            if (c.IsNull()) continue;
            try {
                list.append(GeomAPI::To2d(new Geom_TrimmedCurve(c, f, l), pln));
            } catch (const Standard_Failure &) {
            }
        }
        out.append(list);
    }
    return out;
}

QVariantList FissionSketch::constraintsAfterEdit(const QVariantList &before, const QVariantList &after,
                                                 const QSet<int> &edited, const QStringList &dropWholeCurveTypes) const {
    auto eval = [this](const QString &e, QString *er) { return evaluate(e, er); };
    SketchSolver sb, sa;
    QString err;
    sb.load(before, {}, eval, err);
    sa.load(after, {}, eval, err);
    QVariantList out;
    for (const QVariant &cv : list("constraints")) {
        const QVariantMap c = cv.toMap();
        const QString type = c.value("type").toString();
        bool drop = false;
        for (const char *key : kRefs) {
            if (!c.contains(key)) continue;
            const QVariantMap r = c.value(key).toMap();
            const int e = r.value("e", -1).toInt();
            if (!edited.contains(e)) continue;
            if (r.contains("p")) {
                P p0, p1;
                const QString role = r.value("p").toString();
                if (!sb.point(e, role, p0) || !sa.point(e, role, p1) || dist(p0, p1) > 1e-6) drop = true;
            } else if (dropWholeCurveTypes.contains(type)) {
                drop = true;
            }
        }
        if (!drop) out << c;
    }
    return out;
}

void FissionSketch::setDimensionOffset(int index, double offset) {
    QVariantList cons = list("constraints");
    if (index < 0 || index >= cons.size()) return;
    QVariantMap c = cons[index].toMap();
    c["offset"] = r6(offset);
    cons[index] = c;
    set("constraints", cons);
}

// --- drag ----------------------------------------------------------------------

bool FissionSketch::dragTo(const QVariantList &targets) {
    const QVariantList ents = list("entities");
    QVariantList cons = list("constraints");
    for (const QVariant &tv : targets) {
        const QVariantMap t = tv.toMap();
        QVariantMap a{{"e", t.value("e")}};
        if (t.contains("p")) a["p"] = t.value("p");
        cons << QVariantMap{{"type", "dragto"}, {"a", a}, {"u", t.value("u")}, {"v", t.value("v")}};
    }
    SketchSolver s;
    QString err;
    if (!s.load(ents, cons, [this](const QString &e, QString *er) { return evaluate(e, er); }, err) || !s.solve(err))
        return false;
    set("entities", s.storedEntities(ents));
    return true;
}

// --- trim / extend -------------------------------------------------------------

bool FissionSketch::trimAt(int entity, double u, double v) {
    QVariantList ents = list("entities");
    if (entity < 0 || entity >= ents.size()) return false;
    SketchSolver s;
    QString err;
    if (!s.load(ents, list("constraints"), [this](const QString &e, QString *er) { return evaluate(e, er); }, err))
        return false;
    s.solve(err);
    const QVariantList num = s.entities();
    const QVariantMap e = num[entity].toMap();
    const QString type = e.value("type").toString();
    const auto curves = curves2d();
    const P click{u, v};
    QVariantList out = ents;
    auto keepFlags = [&](QVariantMap m) {
        if (ents[entity].toMap().value("construction").toBool()) m["construction"] = true;
        return m;
    };

    if (type == "line") {
        P a, b;
        s.lineEnds(entity, a, b);
        const P d = sub(b, a);
        const double len2 = dot(d, d);
        if (len2 < 1e-18) return false;
        Handle(Geom2d_TrimmedCurve) seg = GCE2d_MakeSegment(gp_Pnt2d(a.u, a.v), gp_Pnt2d(b.u, b.v)).Value();
        QList<double> ts;
        for (const gp_Pnt2d &p : crossings(seg, curves, entity)) {
            const double t = dot(sub({p.X(), p.Y()}, a), d) / len2;
            if (t > 1e-6 && t < 1 - 1e-6) ts << t;
        }
        if (ts.isEmpty()) { removeEntity(entity); return true; }
        const double tc = dot(sub(click, a), d) / len2;
        double lo = -1, hi = 2;
        for (double t : ts) {
            if (t < tc) lo = std::max(lo, t);
            else hi = std::min(hi, t);
        }
        auto at = [&](double t) { return add(a, d, t); };
        if (lo < 0) out[entity] = keepFlags(lineMap(at(hi), b));
        else if (hi > 1) out[entity] = keepFlags(lineMap(a, at(lo)));
        else {
            out[entity] = keepFlags(lineMap(a, at(lo)));
            out.append(keepFlags(lineMap(at(hi), b)));
        }
    } else if (type == "circle" || type == "arc") {
        P c;
        double r = 0;
        s.circle(entity, c, r);
        Handle(Geom2d_Circle) circ = new Geom2d_Circle(gp_Ax2d(gp_Pnt2d(c.u, c.v), gp_Dir2d(1, 0)), r);
        QList<double> angles;
        for (const gp_Pnt2d &p : crossings(circ, curves, entity)) angles << angDeg(c, {p.X(), p.Y()});
        const double ac = angDeg(c, click);
        if (type == "circle") {
            double next = 0, prev = 0, bestN = 1e9, bestP = 1e9;
            for (double a : angles) {
                if (ccwDeg(ac, a) < bestN) { bestN = ccwDeg(ac, a); next = a; }
                if (ccwDeg(a, ac) < bestP) { bestP = ccwDeg(a, ac); prev = a; }
            }
            if (angles.size() < 2) { removeEntity(entity); return true; }
            out[entity] = keepFlags(arcMap(c, r, next, next + ccwDeg(next, prev)));
        } else {
            const double a1 = e.value("a1").toDouble();
            double sweep = ccwDeg(a1, e.value("a2").toDouble());
            if (sweep < 1e-9) sweep = 360.0;
            QList<double> ss;
            for (double a : angles) {
                const double sv = ccwDeg(a1, a);
                if (sv > 1e-5 && sv < sweep - 1e-5) ss << sv;
            }
            if (ss.isEmpty()) { removeEntity(entity); return true; }
            const double sc = ccwDeg(a1, ac);
            double lo = -1, hi = 1e9;
            for (double sv : ss) {
                if (sv < sc) lo = std::max(lo, sv);
                else hi = std::min(hi, sv);
            }
            if (sc > sweep) {   // clicked outside the arc: trim the end nearer the click
                if (ccwDeg(a1 + sweep, ac) < ccwDeg(ac, a1)) { lo = *std::max_element(ss.begin(), ss.end()); hi = 1e9; }
                else { lo = -1; hi = *std::min_element(ss.begin(), ss.end()); }
            }
            if (lo < 0) out[entity] = keepFlags(arcMap(c, r, a1 + hi, a1 + sweep));
            else if (hi > sweep) out[entity] = keepFlags(arcMap(c, r, a1, a1 + lo));
            else {
                out[entity] = keepFlags(arcMap(c, r, a1, a1 + lo));
                out.append(keepFlags(arcMap(c, r, a1 + hi, a1 + sweep)));
            }
        }
    } else {
        return false;
    }

    const QVariantList cons = constraintsAfterEdit(num, out, {entity}, type == "line" ? QStringList{"midpoint", "equal"}
                                                                                       : QStringList{});
    QVariantMap p = props();
    p["entities"] = out;
    p["constraints"] = cons;
    setProps(p);
    return true;
}

bool FissionSketch::extendAt(int entity, double u, double v) {
    QVariantList ents = list("entities");
    if (entity < 0 || entity >= ents.size()) return false;
    SketchSolver s;
    QString err;
    if (!s.load(ents, list("constraints"), [this](const QString &e, QString *er) { return evaluate(e, er); }, err))
        return false;
    s.solve(err);
    const QVariantList num = s.entities();
    const QVariantMap e = num[entity].toMap();
    const QString type = e.value("type").toString();
    const auto curves = curves2d();
    const P click{u, v};
    QVariantMap updated = ents[entity].toMap();

    if (type == "line") {
        P a, b;
        s.lineEnds(entity, a, b);
        const P d = sub(b, a);
        const double len2 = dot(d, d);
        if (len2 < 1e-18) return false;
        const bool atEnd = dist(click, b) < dist(click, a);
        const double far = 1e4 / std::sqrt(len2);
        Handle(Geom2d_TrimmedCurve) ray = GCE2d_MakeSegment(gp_Pnt2d(a.u - d.u * far, a.v - d.v * far),
                                                            gp_Pnt2d(b.u + d.u * far, b.v + d.v * far)).Value();
        double best = atEnd ? 1e18 : -1e18;
        for (const gp_Pnt2d &p : crossings(ray, curves, entity)) {
            const double t = dot(sub({p.X(), p.Y()}, a), d) / len2;
            if (atEnd && t > 1 + 1e-6) best = std::min(best, t);
            if (!atEnd && t < -1e-6) best = std::max(best, t);
        }
        if (std::fabs(best) > 1e17) return false;
        const P np = add(a, d, best);
        updated[atEnd ? "x2" : "x1"] = r6(np.u);
        updated[atEnd ? "y2" : "y1"] = r6(np.v);
    } else if (type == "arc") {
        P c;
        double r = 0;
        s.circle(entity, c, r);
        const double a1 = e.value("a1").toDouble(), a2 = e.value("a2").toDouble();
        const double sweep = ccwDeg(a1, a2);
        const bool atEnd = dist(click, onCircle(c, r, a2)) < dist(click, onCircle(c, r, a1));
        Handle(Geom2d_Circle) circ = new Geom2d_Circle(gp_Ax2d(gp_Pnt2d(c.u, c.v), gp_Dir2d(1, 0)), r);
        double best = 1e18;
        for (const gp_Pnt2d &p : crossings(circ, curves, entity)) {
            const double ang = angDeg(c, {p.X(), p.Y()});
            const double step = atEnd ? ccwDeg(a2, ang) : ccwDeg(ang, a1);
            if (step > 1e-5 && step < 360.0 - sweep - 1e-5) best = std::min(best, step);
        }
        if (best > 1e17) return false;
        const double na1 = atEnd ? a1 : a1 - best, na2 = atEnd ? a2 + best : a2;
        const QVariantMap m = arcMap(c, r, na1, na2);
        for (const char *k : {"cx", "cy", "r", "a1", "a2"}) updated[k] = m.value(k);
    } else {
        return false;
    }

    QVariantList out = ents;
    out[entity] = updated;
    const QVariantList cons = constraintsAfterEdit(num, out, {entity}, type == "line" ? QStringList{"midpoint", "equal"}
                                                                                       : QStringList{});
    QVariantMap p = props();
    p["entities"] = out;
    p["constraints"] = cons;
    setProps(p);
    return true;
}

// --- chains and offset -----------------------------------------------------------

namespace {
// Stored start / end of a line or arc
bool openEnds(const SketchSolver &s, int e, P &a, P &b) {
    const QString t = s.entityType(e);
    if (t == "line") return s.lineEnds(e, a, b);
    if (t == "arc") return s.point(e, "start", a) && s.point(e, "end", b);
    return false;
}
}   // namespace

QVariantList FissionSketch::chainOf(int entity) const {
    SketchSolver s;
    QString err;
    if (!s.load(list("entities"), list("constraints"), [this](const QString &e, QString *er) { return evaluate(e, er); },
                err))
        return {entity};
    s.solve(err);
    P a, b;
    if (!openEnds(s, entity, a, b)) return {entity};
    QList<int> chain{entity}, todo{entity};
    while (!todo.isEmpty()) {
        const int e = todo.takeLast();
        openEnds(s, e, a, b);
        for (int o = 0; o < s.entityCount(); ++o) {
            P c, d;
            if (chain.contains(o) || !openEnds(s, o, c, d)) continue;
            if (dist(a, c) < 1e-6 || dist(a, d) < 1e-6 || dist(b, c) < 1e-6 || dist(b, d) < 1e-6) {
                chain.append(o);
                todo.append(o);
            }
        }
    }
    QVariantList out;
    for (int i : chain) out << i;
    return out;
}

QVariantMap FissionSketch::moveEntities(const QVariantList &indices, double du, double dv, double angle, double pu,
                                        double pv, bool copy) {
    QVariantList ents = list("entities");
    const QVariantList cons = list("constraints");
    QList<int> moved;
    for (const QVariant &v : indices) {
        const int i = v.toInt();
        if (i < 0 || i >= ents.size() || moved.contains(i)) continue;
        const QVariantMap e = ents[i].toMap();
        if (e.value("fixed").toBool() || e.value("projected").toBool()) continue;
        moved << i;
    }
    if (moved.isEmpty()) return {{"ok", false}, {"error", "Select sketch curves to move"}};

    const double c = std::cos(angle * M_PI / 180.0), s = std::sin(angle * M_PI / 180.0);
    const bool rotating = std::fabs(angle) > 1e-12;
    QString err;
    auto num = [&](const QVariantMap &e, const char *key) {
        const QVariant v = e.value(key);
        if (v.typeId() == QMetaType::QString) return evaluate(v.toString(), &err);
        return v.toDouble();
    };
    auto map = [&](double u, double v) {
        const double x = u - pu, y = v - pv;
        return P{r6(pu + c * x - s * y + du), r6(pv + s * x + c * y + dv)};
    };
    auto movePt = [&](QVariantMap &e, const char *ku, const char *kv) {
        const P p = map(num(e, ku), num(e, kv));
        e[ku] = p.u;
        e[kv] = p.v;
    };
    auto transformed = [&](QVariantMap e) {
        const QString t = e.value("type").toString();
        if (t == "line" || t == "slot") { movePt(e, "x1", "y1"); movePt(e, "x2", "y2"); }
        else if (t == "circle" || t == "polygon" || t == "ellipse") movePt(e, "cx", "cy");
        else if (t == "arc") {
            movePt(e, "cx", "cy");
            e["a1"] = r6(num(e, "a1") + angle);
            e["a2"] = r6(num(e, "a2") + angle);
        } else if (t == "arc3") { movePt(e, "x1", "y1"); movePt(e, "xm", "ym"); movePt(e, "x2", "y2"); }
        else if (t == "point") movePt(e, "x", "y");
        else if (t == "rectangle") {
            const double x = num(e, "x"), y = num(e, "y"), w = num(e, "w"), h = num(e, "h");
            if (!rotating) { const P p = map(x, y); e["x"] = p.u; e["y"] = p.v; }
            else {
                QVariantList pts;
                for (const P &q : {map(x, y), map(x + w, y), map(x + w, y + h), map(x, y + h)}) pts << QVariant(QVariantList{q.u, q.v});
                QVariantMap poly{{"type", "polyline"}, {"points", pts}, {"closed", true}};
                if (e.contains("construction")) poly["construction"] = e["construction"];
                return poly;
            }
        } else if (t == "spline" || t == "polyline") {
            QVariantList pts;
            for (const QVariant &pv2 : e.value("points").toList()) {
                const QVariantList q = pv2.toList();
                if (q.size() < 2) continue;
                const QVariantMap tmp{{"u", q[0]}, {"v", q[1]}};
                const P p = map(num(tmp, "u"), num(tmp, "v"));
                pts << QVariant(QVariantList{p.u, p.v});
            }
            e["points"] = pts;
        }
        return e;
    };

    // Constraints that only make sense for the curves where they were
    auto refsOf = [](const QVariantMap &con) {
        QList<int> out;
        for (const char *key : kRefs)
            if (con.contains(key)) out << con.value(key).toMap().value("e", -1).toInt();
        return out;
    };
    auto inner = [&](const QVariantMap &con) {
        const QList<int> refs = refsOf(con);
        if (refs.isEmpty()) return false;
        for (int e : refs)
            if (!moved.contains(e)) return false;
        const QString type = con.value("type").toString();
        if (type == "fix") return false;
        if (rotating && (type == "horizontal" || type == "vertical" ||
                         (type == "distance" && !con.value("orient").toString().isEmpty() &&
                          con.value("orient").toString() != "aligned")))
            return false;
        return true;
    };

    QVariantList newCons;
    QVariantList result;
    if (copy) {
        QHash<int, int> remap;
        for (int i : moved) {
            remap[i] = ents.size();
            result << ents.size();
            ents << transformed(ents[i].toMap());
        }
        newCons = cons;
        for (const QVariant &cv : cons) {
            QVariantMap con = cv.toMap();
            if (!inner(con)) continue;
            for (const char *key : kRefs) {
                if (!con.contains(key)) continue;
                QVariantMap r = con.value(key).toMap();
                r["e"] = remap.value(r.value("e").toInt());
                con[key] = r;
            }
            newCons << con;
        }
    } else {
        for (int i : moved) {
            ents[i] = transformed(ents[i].toMap());
            result << i;
        }
        for (const QVariant &cv : cons) {
            const QVariantMap con = cv.toMap();
            const QList<int> refs = refsOf(con);
            const bool touches = std::any_of(refs.begin(), refs.end(), [&](int e) { return moved.contains(e); });
            if (!touches || inner(con)) newCons << con;
        }
    }
    if (!err.isEmpty()) return {{"ok", false}, {"error", err}};
    QVariantMap res = applyConstraints(ents, newCons);
    if (res.value("ok").toBool()) res["entities"] = result;
    return res;
}

QVariantList FissionSketch::offsetEntities(const QVariantList &indices, double distance, double u, double v) {
    if (distance <= 0) return {};
    QVariantList ents = list("entities"), cons = list("constraints");
    SketchSolver s;
    QString err;
    if (!s.load(ents, cons, [this](const QString &e, QString *er) { return evaluate(e, er); }, err)) return {};
    s.solve(err);
    const QVariantList num = s.entities();
    const P click{u, v};
    QVariantList added;
    QList<int> path;

    // Closed single entities grow or shrink toward the click
    for (const QVariant &iv : indices) {
        const int i = iv.toInt();
        if (i < 0 || i >= num.size()) continue;
        const QVariantMap m = num[i].toMap();
        const QString t = m.value("type").toString();
        if (t == "line" || t == "arc") { if (!path.contains(i)) path << i; continue; }
        if (t == "circle" || t == "polygon") {
            const P c{m.value("cx").toDouble(), m.value("cy").toDouble()};
            const double r = m.value("r").toDouble();
            const int n = std::max(3, m.value("sides", 6).toInt());
            const double k = t == "polygon" ? 1.0 / std::cos(M_PI / n) : 1.0;
            const bool inside = dist(click, c) < (t == "polygon" ? r * std::cos(M_PI / n) : r);
            const double nr = r + (inside ? -distance : distance) * k;
            if (nr <= 1e-9) return {};
            QVariantMap out = t == "circle" ? circleMap(c, nr) : m;
            if (t == "polygon") { out["r"] = r6(nr); out.remove("construction"); }
            added << ents.size();
            ents << out;
        } else if (t == "slot") {
            const P a{m.value("x1").toDouble(), m.value("y1").toDouble()}, b{m.value("x2").toDouble(), m.value("y2").toDouble()};
            const double w = m.value("w").toDouble();
            const P d = sub(b, a);
            const double tt = std::clamp(dot(sub(click, a), d) / std::max(1e-18, dot(d, d)), 0.0, 1.0);
            const bool inside = dist(click, add(a, d, tt)) < w / 2;
            const double nw = w + (inside ? -2 : 2) * distance;
            if (nw <= 1e-9) return {};
            QVariantMap out = m;
            out["w"] = r6(nw);
            out.remove("construction");
            added << ents.size();
            ents << out;
        }
    }

    // Lines and arcs: walk each connected chain, offset every piece to the same side, then rejoin the corners
    struct Piece { int e; bool line; bool fwd; P t0, t1; P c; double r = 0; };
    QList<int> remaining = path;
    while (!remaining.isEmpty()) {
        auto ends = [&](int e, P &a, P &b) { return openEnds(s, e, a, b); };
        auto sharedEnd = [&](const P &p, int self) {
            for (int o : remaining) {
                P a, b;
                if (o != self && ends(o, a, b) && (dist(p, a) < 1e-6 || dist(p, b) < 1e-6)) return true;
            }
            return false;
        };
        int start = remaining.first();
        bool startFwd = true;
        for (int e : remaining) {
            P a, b;
            ends(e, a, b);
            if (!sharedEnd(a, e)) { start = e; startFwd = true; break; }
            if (!sharedEnd(b, e)) { start = e; startFwd = false; break; }
        }
        QList<Piece> chain;
        auto makePiece = [&](int e, bool fwd) {
            Piece pc;
            pc.e = e;
            pc.fwd = fwd;
            pc.line = s.entityType(e) == "line";
            P a, b;
            ends(e, a, b);
            pc.t0 = fwd ? a : b;
            pc.t1 = fwd ? b : a;
            if (!pc.line) s.circle(e, pc.c, pc.r);
            return pc;
        };
        chain << makePiece(start, startFwd);
        remaining.removeAll(start);
        for (bool grew = true; grew;) {
            grew = false;
            const P tip = chain.last().t1;
            for (int o : remaining) {
                P a, b;
                ends(o, a, b);
                if (dist(a, tip) < 1e-6 || dist(b, tip) < 1e-6) {
                    chain << makePiece(o, dist(a, tip) < 1e-6);
                    remaining.removeAll(o);
                    grew = true;
                    break;
                }
            }
        }
        const bool closed = chain.size() > 1 && dist(chain.last().t1, chain.first().t0) < 1e-6;

        // Side: left (+1) or right (-1) of the walking direction, from the piece nearest the click
        double bestD = 1e18;
        int side = 1;
        for (const Piece &pc : chain) {
            double d;
            bool left;
            if (pc.line) {
                const P dir = sub(pc.t1, pc.t0);
                const double t = std::clamp(dot(sub(click, pc.t0), dir) / std::max(1e-18, dot(dir, dir)), 0.0, 1.0);
                d = dist(click, add(pc.t0, dir, t));
                left = cross(dir, sub(click, pc.t0)) > 0;
            } else {
                d = std::fabs(dist(click, pc.c) - pc.r);
                const bool inside = dist(click, pc.c) < pc.r;
                left = pc.fwd ? inside : !inside;
            }
            if (d < bestD) { bestD = d; side = left ? 1 : -1; }
        }

        // Offset pieces
        QList<Piece> off = chain;
        for (Piece &pc : off) {
            if (pc.line) {
                const P dir = unit(sub(pc.t1, pc.t0)), n{-dir.v, dir.u};
                pc.t0 = add(pc.t0, n, side * distance);
                pc.t1 = add(pc.t1, n, side * distance);
            } else {
                const double nr = pc.fwd ? pc.r - side * distance : pc.r + side * distance;
                if (nr <= 1e-9) return {};
                pc.t0 = add(pc.c, sub(pc.t0, pc.c), nr / pc.r);
                pc.t1 = add(pc.c, sub(pc.t1, pc.c), nr / pc.r);
                pc.r = nr;
            }
        }
        // Rejoin consecutive pieces at the intersection nearest the old joint
        QList<bool> joined;
        const int njoints = closed ? off.size() : off.size() - 1;
        for (int k = 0; k < njoints; ++k) {
            Piece &p1 = off[k];
            Piece &p2 = off[(k + 1) % off.size()];
            const P guess{(p1.t1.u + p2.t0.u) / 2, (p1.t1.v + p2.t0.v) / 2};
            QList<P> cands;
            if (p1.line && p2.line) {
                const P d1 = sub(p1.t1, p1.t0), d2 = sub(p2.t1, p2.t0);
                const double den = cross(d1, d2);
                if (std::fabs(den) < 1e-12 * std::sqrt(dot(d1, d1) * dot(d2, d2))) cands << guess;
                else cands << add(p1.t0, d1, cross(sub(p2.t0, p1.t0), d2) / den);
            } else if (p1.line || p2.line) {
                const Piece &ln = p1.line ? p1 : p2, &ci = p1.line ? p2 : p1;
                cands = lineCircle(ln.t0, sub(ln.t1, ln.t0), ci.c, ci.r);
            } else {
                cands = circleCircle(p1.c, p1.r, p2.c, p2.r);
            }
            if (cands.isEmpty()) { joined << false; continue; }
            P best = cands.first();
            for (const P &q : cands) if (dist(q, guess) < dist(best, guess)) best = q;
            p1.t1 = best;
            p2.t0 = best;
            joined << true;
        }

        const int first = ents.size();
        for (const Piece &pc : off) {
            if (pc.line) {
                ents << (pc.fwd ? lineMap(pc.t0, pc.t1) : lineMap(pc.t1, pc.t0));
            } else {
                const P a = pc.fwd ? pc.t0 : pc.t1, b = pc.fwd ? pc.t1 : pc.t0;
                const QVariantMap orig = num[pc.e].toMap();
                const double sweep = ccwDeg(orig.value("a1").toDouble(), orig.value("a2").toDouble());
                double a1 = angDeg(pc.c, a), a2 = angDeg(pc.c, b);
                if (ccwDeg(a1, a2) < 1e-9 && sweep > 180) a2 = a1 + 360;   // full turn
                ents << arcMap(pc.c, pc.r, a1, a1 + ccwDeg(a1, a2));
            }
            added << ents.size() - 1;
        }
        for (int k = 0; k < joined.size(); ++k) {
            if (!joined[k]) continue;
            const int i1 = first + k, i2 = first + (k + 1) % off.size();
            const QString r1 = off[k].fwd ? "end" : "start", r2 = off[(k + 1) % off.size()].fwd ? "start" : "end";
            cons << QVariantMap{{"type", "coincident"}, {"a", QVariantMap{{"e", i1}, {"p", r1}}},
                                {"b", QVariantMap{{"e", i2}, {"p", r2}}}};
        }
    }

    if (added.isEmpty()) return {};
    QVariantMap p = props();
    p["entities"] = ents;
    p["constraints"] = cons;
    setProps(p);
    return added;
}

// --- sketch fillet ----------------------------------------------------------------

QVariantMap FissionSketch::filletCorner(int l1, int l2, const QString &radius) {
    auto bad = [](const QString &m) { return QVariantMap{{"ok", false}, {"error", m}}; };
    QVariantList ents = list("entities");
    SketchSolver s;
    QString err;
    if (!s.load(ents, list("constraints"), [this](const QString &e, QString *er) { return evaluate(e, er); }, err))
        return bad(err);
    s.solve(err);
    if (l1 == l2 || s.entityType(l1) != "line" || s.entityType(l2) != "line") return bad("Select two lines to fillet");
    QString rerr;
    const double R = evaluate(radius, &rerr);
    if (!rerr.isEmpty()) return bad(rerr);
    if (R <= 0) return bad("The fillet radius must be positive");

    P a0, a1, b0, b1;
    s.lineEnds(l1, a0, a1);
    s.lineEnds(l2, b0, b1);
    const P d1 = sub(a1, a0), d2 = sub(b1, b0);
    const double den = cross(d1, d2);
    if (std::fabs(den) < 1e-9 * std::sqrt(dot(d1, d1) * dot(d2, d2))) return bad("The lines are parallel");
    const P corner = add(a0, d1, cross(sub(b0, a0), d2) / den);
    const bool aStart = dist(a0, corner) < dist(a1, corner), bStart = dist(b0, corner) < dist(b1, corner);
    const P farA = aStart ? a1 : a0, farB = bStart ? b1 : b0;
    const P u1 = unit(sub(farA, corner)), u2 = unit(sub(farB, corner));
    const double phi = std::acos(std::clamp(dot(u1, u2), -1.0, 1.0));
    if (phi < 1e-6 || phi > M_PI - 1e-6) return bad("The lines don't form a corner");
    const double tl = R / std::tan(phi / 2);
    if (tl >= dist(farA, corner) - 1e-9 || tl >= dist(farB, corner) - 1e-9)
        return bad("The radius is too large for these lines");
    const P t1 = add(corner, u1, tl), t2 = add(corner, u2, tl);
    const P center = add(corner, unit(add(u1, u2)), R / std::sin(phi / 2));
    double s1 = angDeg(center, t1), s2 = angDeg(center, t2);
    const bool startAtT1 = ccwDeg(s1, s2) <= 180.0;
    if (!startAtT1) std::swap(s1, s2);

    const QVariantList num = s.entities();
    QVariantList out = ents;
    auto moved = [&](int e, bool start, const P &p) {
        QVariantMap m = num[e].toMap();
        m[start ? "x1" : "x2"] = r6(p.u);
        m[start ? "y1" : "y2"] = r6(p.v);
        return m;
    };
    out[l1] = moved(l1, aStart, t1);
    out[l2] = moved(l2, bStart, t2);
    QVariantMap arc = arcMap(center, R, s1, s1 + ccwDeg(s1, s2));
    if (ents[l1].toMap().value("construction").toBool()) arc["construction"] = true;
    const int arcIndex = out.size();
    out << arc;

    QVariantMap p = props();
    p["entities"] = out;
    p["constraints"] = constraintsAfterEdit(num, out, {l1, l2}, {"midpoint", "equal"});
    setProps(p);

    const QString roleT1 = startAtT1 ? "start" : "end", roleT2 = startAtT1 ? "end" : "start";
    auto ref = [](int e, const QString &role) {
        QVariantMap m{{"e", e}};
        if (!role.isEmpty()) m["p"] = role;
        return m;
    };
    const QList<QVariantMap> ties{
        {{"type", "coincident"}, {"a", ref(arcIndex, roleT1)}, {"b", ref(l1, aStart ? "start" : "end")}},
        {{"type", "coincident"}, {"a", ref(arcIndex, roleT2)}, {"b", ref(l2, bStart ? "start" : "end")}},
        {{"type", "tangent"}, {"a", ref(l1, {})}, {"b", ref(arcIndex, {})}},
        {{"type", "tangent"}, {"a", ref(l2, {})}, {"b", ref(arcIndex, {})}},
        {{"type", "radius"}, {"a", ref(arcIndex, {})}, {"value", radius.trimmed()}},
    };
    QString tieError;
    for (const QVariantMap &c : ties) {
        const QVariantMap res = addConstraint(c);
        if (!res.value("ok").toBool() && tieError.isEmpty()) tieError = res.value("error").toString();
    }
    QVariantMap res{{"ok", true}, {"index", arcIndex}};
    if (!tieError.isEmpty()) res["warning"] = tieError;
    return res;
}

// --- project ----------------------------------------------------------------------

int FissionSketch::projectRef(const QVariantMap &ref) {
    auto *doc = qobject_cast<FissionDocument *>(parent());
    if (!doc) return 0;
    const QString kind = ref.value("kind").toString();
    const TopoDS_Shape shape = doc->displayShape(ref.value("body", -1).toInt());
    if (shape.IsNull()) return 0;
    const int index = ref.value("index", -1).toInt();
    QList<TopoDS_Edge> edges;
    if (kind == "edge") {
        const TopTools_IndexedMapOfShape map = GeomUtils::edgeMap(shape);
        if (index < 0 || index >= map.Extent()) return 0;
        edges << TopoDS::Edge(map(index + 1));
    } else if (kind == "face") {
        const TopTools_IndexedMapOfShape map = GeomUtils::faceMap(shape);
        if (index < 0 || index >= map.Extent()) return 0;
        TopTools_IndexedMapOfShape faceEdges;
        for (TopExp_Explorer ex(map(index + 1), TopAbs_EDGE); ex.More(); ex.Next()) faceEdges.Add(ex.Current());
        for (int i = 1; i <= faceEdges.Extent(); ++i) edges << TopoDS::Edge(faceEdges(i));
    } else {
        return 0;
    }

    const gp_Pnt O = m_plane.Location();
    const gp_Vec X(m_plane.XDirection()), Y(m_plane.YDirection());
    const gp_Dir N = m_plane.Direction();
    auto toUV = [&](const gp_Pnt &p) { const gp_Vec r(O, p); return P{r.Dot(X), r.Dot(Y)}; };

    QVariantList ents = list("entities");
    const int before = ents.size();
    auto push = [&](QVariantMap m) {
        m["fixed"] = true;
        m["projected"] = true;
        ents << m;
    };
    for (const TopoDS_Edge &edge : edges) {
        if (BRep_Tool::Degenerated(edge)) continue;
        try {
            BRepAdaptor_Curve c(edge);
            const double f = c.FirstParameter(), l = c.LastParameter();
            const P a = toUV(c.Value(f)), b = toUV(c.Value(l)), m = toUV(c.Value((f + l) / 2));
            if (c.GetType() == GeomAbs_Line) {
                if (dist(a, b) < 1e-6) push({{"type", "point"}, {"x", r6(a.u)}, {"y", r6(a.v)}});
                else push(lineMap(a, b));
                continue;
            }
            if (c.GetType() == GeomAbs_Circle && std::fabs(c.Circle().Axis().Direction().Dot(N)) > 1 - 1e-9) {
                const P cen = toUV(c.Circle().Location());
                const double r = c.Circle().Radius();
                if (dist(a, b) < 1e-7) push(circleMap(cen, r));
                else {
                    P cc;
                    double rr, s1, s2;
                    if (arcThrough(a, m, b, cc, rr, s1, s2)) push(arcMap(cc, rr, s1, s2));
                }
                continue;
            }
            // Anything else becomes a spline through sampled points (or a line / point when it projects flat)
            QVariantList pts;
            QList<P> uvs;
            const int n = 24;
            for (int i = 0; i <= n; ++i) uvs << toUV(c.Value(f + (l - f) * i / n));
            double spread = 0, off = 0;
            const P dir = sub(uvs.last(), uvs.first());
            for (const P &q : uvs) {
                spread = std::max(spread, dist(q, uvs.first()));
                if (dot(dir, dir) > 1e-18) off = std::max(off, std::fabs(cross(unit(dir), sub(q, uvs.first()))));
            }
            if (spread < 1e-6) { push({{"type", "point"}, {"x", r6(a.u)}, {"y", r6(a.v)}}); continue; }
            if (off < 1e-6 && dist(a, b) > 1e-6) { push(lineMap(a, b)); continue; }
            for (const P &q : uvs) pts << QVariant(QVariantList{r6(q.u), r6(q.v)});
            if (dist(uvs.first(), uvs.last()) < 1e-7) pts.removeLast();
            push({{"type", "spline"}, {"points", pts}});
        } catch (const Standard_Failure &) {
        }
    }
    if (ents.size() == before) return 0;
    set("entities", ents);
    return ents.size() - before;
}

// --- DXF export --------------------------------------------------------------------

bool FissionSketch::exportDxf(const QString &path, QString *error) const {
    const gp_Vec X(m_plane.XDirection()), Y(m_plane.YDirection());
    FileExport::DxfWriter dxf(gp_Ax3(m_plane.Location(), gp_Dir(X.Crossed(Y)), m_plane.XDirection()));
    if (!dxf.open(path, {{"0", 7}, {"CONSTRUCTION", 30}}, error)) return false;
    const QVariantList ents = list("entities");
    for (int i = 0; i < ents.size(); ++i) {
        const QVariantMap e = ents[i].toMap();
        const QString layer = e.value("construction").toBool() ? "CONSTRUCTION" : "0";
        if (e.value("type").toString() == "point") {
            for (const QVariant &hv : m_handles) {
                const QVariantMap h = hv.toMap();
                if (h.value("entity").toInt() != i || h.value("role").toString() != "pos") continue;
                dxf.addPoint(h["u"].toDouble(), h["v"].toDouble(), layer);
            }
            continue;
        }
        if (i >= m_entityEdges.size()) continue;
        for (const TopoDS_Edge &edge : m_entityEdges[i]) dxf.addEdge(edge, layer);
    }
    return dxf.close(error);
}
