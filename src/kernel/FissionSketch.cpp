// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#include "FissionSketch.h"
#include "Expression.h"
#include "FissionDocument.h"
#include "GeomUtils.h"
#include "SketchSolver.h"

#include <BRepAlgoAPI_Splitter.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakeVertex.hxx>
#include <BRepClass_FaceClassifier.hxx>
#include <BRepMesh_IncrementalMesh.hxx>
#include <BRepTools.hxx>
#include <BRep_Builder.hxx>
#include <BRep_Tool.hxx>
#include <GC_MakeArcOfCircle.hxx>
#include <GeomAPI_Interpolate.hxx>
#include <Geom_BSplineCurve.hxx>
#include <Geom_TrimmedCurve.hxx>
#include <Poly_Triangulation.hxx>
#include <ShapeAnalysis_FreeBounds.hxx>
#include <TColgp_HArray1OfPnt.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_HSequenceOfShape.hxx>
#include <TopTools_ListOfShape.hxx>
#include <TopoDS.hxx>
#include <gp_Circ.hxx>
#include <gp_Elips.hxx>
#include <gp_Pln.hxx>

FissionSketch::FissionSketch(QObject *parent) : FissionFeature(parent) {}

QVariantMap FissionSketch::defaults() const {
    return {{"plane", "XY"}, {"entities", QVariantList()}, {"constraints", QVariantList()}};
}

double FissionSketch::evaluate(const QString &expr, QString *error) const {
    if (const auto *doc = qobject_cast<const FissionDocument *>(parent())) return doc->evaluate(expr, error);
    return Expression::evaluate(expr, error);
}

static const char *const kRefKeys[] = {"a", "b", "c"};

// --- entity editing --------------------------------------------------------

int FissionSketch::addEntity(const QVariantMap &entity) {
    QVariantList ents = list("entities");
    ents.append(entity);
    set("entities", ents);
    return ents.size() - 1;
}

void FissionSketch::setEntity(int index, const QVariantMap &entity) {
    QVariantList ents = list("entities");
    if (index < 0 || index >= ents.size()) return;
    ents[index] = entity;
    set("entities", ents);
}

QVariantMap FissionSketch::entity(int index) const {
    const QVariantList ents = list("entities");
    return (index >= 0 && index < ents.size()) ? ents[index].toMap() : QVariantMap();
}

void FissionSketch::removeEntity(int index) {
    QVariantList ents = list("entities");
    if (index < 0 || index >= ents.size()) return;
    ents.removeAt(index);

    // Constraints on the removed entity go with it; later references shift down
    QVariantList cons;
    for (const QVariant &cv : list("constraints")) {
        QVariantMap c = cv.toMap();
        bool drop = false;
        for (const char *key : kRefKeys) {
            if (!c.contains(key)) continue;
            QVariantMap r = c.value(key).toMap();
            const int e = r.value("e", -1).toInt();
            if (e == index) drop = true;
            else if (e > index) { r["e"] = e - 1; c[key] = r; }
        }
        if (!drop) cons << c;
    }
    QVariantMap p = props();
    p["entities"] = ents;
    p["constraints"] = cons;
    setProps(p);
}

int FissionSketch::addRectangleLines(double x, double y, double w, double h) {
    QVariantList ents = list("entities"), cons = list("constraints");
    const int first = ents.size();
    const double cu[4] = {x, x + w, x + w, x}, cv[4] = {y, y, y + h, y + h};
    for (int k = 0; k < 4; ++k) {
        const int n = (k + 1) % 4;
        ents << QVariantMap{{"type", "line"}, {"x1", cu[k]}, {"y1", cv[k]}, {"x2", cu[n]}, {"y2", cv[n]}};
    }
    auto ref = [](int e, const char *role) { return QVariantMap{{"e", e}, {"p", role}}; };
    for (int k = 0; k < 4; ++k)
        cons << QVariantMap{{"type", "coincident"}, {"a", ref(first + k, "end")},
                            {"b", ref(first + (k + 1) % 4, "start")}};
    for (int k = 0; k < 4; ++k)
        cons << QVariantMap{{"type", k % 2 ? "vertical" : "horizontal"}, {"a", QVariantMap{{"e", first + k}}}};
    QVariantMap p = props();
    p["entities"] = ents;
    p["constraints"] = cons;
    setProps(p);
    return first;
}

int FissionSketch::addArcThroughPoints(double x1, double y1, double xm, double ym, double x2, double y2) {
    // Circumcenter of the three points
    const double d = 2 * (x1 * (ym - y2) + xm * (y2 - y1) + x2 * (y1 - ym));
    if (std::fabs(d) < 1e-9) return -1;
    const double s1 = x1 * x1 + y1 * y1, sm = xm * xm + ym * ym, s2 = x2 * x2 + y2 * y2;
    const double cx = (s1 * (ym - y2) + sm * (y2 - y1) + s2 * (y1 - ym)) / d;
    const double cy = (s1 * (x2 - xm) + sm * (x1 - x2) + s2 * (xm - x1)) / d;
    const double r = std::hypot(x1 - cx, y1 - cy);
    auto deg = [&](double u, double v) { return std::atan2(v - cy, u - cx) * 180.0 / M_PI; };
    auto ccw = [](double from, double to) { double a = std::fmod(to - from, 360.0); return a < 0 ? a + 360.0 : a; };
    double a1 = deg(x1, y1), a2 = deg(x2, y2);
    const double am = deg(xm, ym);
    if (ccw(a1, am) > ccw(a1, a2)) std::swap(a1, a2);   // mid not on the CCW sweep: run end -> start
    if (a2 < a1) a2 += 360.0;
    auto r6 = [](double v) { return std::round(v * 1e6) / 1e6 + 0.0; };
    return addEntity({{"type", "arc"}, {"cx", r6(cx)}, {"cy", r6(cy)}, {"r", r6(r)}, {"a1", r6(a1)}, {"a2", r6(a2)}});
}

QString FissionSketch::roleAt(int entity, double u, double v) const {
    const QVariantList ents = list("entities");
    if (entity < 0 || entity >= ents.size()) return {};
    SketchSolver s;
    QString err;
    if (!s.load(ents, {}, [this](const QString &e, QString *er) { return evaluate(e, er); }, err)) return {};
    QString best;
    double bestD = 1e-4;
    for (const QString &role : SketchSolver::roles(ents[entity].toMap())) {
        SketchSolver::Point p;
        if (!s.point(entity, role, p)) continue;
        const double dist = std::hypot(p.u - u, p.v - v);
        if (dist < bestD) { bestD = dist; best = role; }
    }
    return best;
}

// --- constraints -----------------------------------------------------------

QVariantMap FissionSketch::applyConstraints(const QVariantList &entities, const QVariantList &constraints) {
    SketchSolver s;
    QString err;
    if (!s.load(entities, constraints, [this](const QString &e, QString *er) { return evaluate(e, er); }, err) ||
        !s.solve(err))
        return {{"ok", false}, {"error", err}};
    QVariantMap p = props();
    p["entities"] = s.storedEntities(entities);
    p["constraints"] = constraints;
    setProps(p);
    return {{"ok", true}};
}

QVariantMap FissionSketch::addConstraint(const QVariantMap &constraint) {
    const QVariantList ents = list("entities");
    QVariantList cons = list("constraints");
    SketchSolver current;
    QString err;
    if (!current.load(ents, cons, [this](const QString &e, QString *er) { return evaluate(e, er); }, err))
        return {{"ok", false}, {"error", err}};

    // Keep the configuration the geometry is in now (which side, inside / outside tangency, ...)
    QVariantMap c = constraint;
    const QVariantMap conf = current.configuration(c);
    for (auto it = conf.begin(); it != conf.end(); ++it)
        if (!c.contains(it.key())) c[it.key()] = it.value();
    if (SketchSolver::isDimension(c.value("type").toString()) && c.value("value").toString().isEmpty())
        c["value"] = QString::number(std::round(current.measure(c) * 1000.0) / 1000.0, 'g', 12);

    cons << c;
    QVariantMap res = applyConstraints(ents, cons);
    if (res.value("ok").toBool()) res["index"] = cons.size() - 1;
    return res;
}

QVariantMap FissionSketch::setConstraintValue(int index, const QString &expression) {
    QVariantList cons = list("constraints");
    if (index < 0 || index >= cons.size()) return {{"ok", false}, {"error", "No such constraint"}};
    QVariantMap c = cons[index].toMap();
    c["value"] = expression.trimmed();
    cons[index] = c;
    return applyConstraints(list("entities"), cons);
}

void FissionSketch::removeConstraint(int index) {
    QVariantList cons = list("constraints");
    if (index < 0 || index >= cons.size()) return;
    cons.removeAt(index);
    set("constraints", cons);
}

QVariantMap FissionSketch::constraint(int index) const {
    const QVariantList cons = list("constraints");
    return (index >= 0 && index < cons.size()) ? cons[index].toMap() : QVariantMap();
}

void FissionSketch::setConstruction(int index, bool construction) {
    QVariantMap e = entity(index);
    if (e.isEmpty()) return;
    e["construction"] = construction;
    setEntity(index, e);
}

int FissionSketch::addLine(const QVariant &x1, const QVariant &y1, const QVariant &x2, const QVariant &y2) {
    return addEntity({{"type", "line"}, {"x1", x1}, {"y1", y1}, {"x2", x2}, {"y2", y2}});
}
int FissionSketch::addCircle(const QVariant &cx, const QVariant &cy, const QVariant &r) {
    return addEntity({{"type", "circle"}, {"cx", cx}, {"cy", cy}, {"r", r}});
}
int FissionSketch::addArc(const QVariant &cx, const QVariant &cy, const QVariant &r,
                          const QVariant &a1, const QVariant &a2) {
    return addEntity({{"type", "arc"}, {"cx", cx}, {"cy", cy}, {"r", r}, {"a1", a1}, {"a2", a2}});
}
int FissionSketch::addArc3(const QVariant &x1, const QVariant &y1, const QVariant &xm, const QVariant &ym,
                           const QVariant &x2, const QVariant &y2) {
    return addEntity({{"type", "arc3"}, {"x1", x1}, {"y1", y1}, {"xm", xm}, {"ym", ym}, {"x2", x2}, {"y2", y2}});
}
int FissionSketch::addRectangle(const QVariant &x, const QVariant &y, const QVariant &w, const QVariant &h) {
    return addEntity({{"type", "rectangle"}, {"x", x}, {"y", y}, {"w", w}, {"h", h}});
}
int FissionSketch::addPolygon(const QVariant &cx, const QVariant &cy, const QVariant &r, int sides) {
    return addEntity({{"type", "polygon"}, {"cx", cx}, {"cy", cy}, {"r", r}, {"sides", sides}});
}
int FissionSketch::addPolyline(const QVariantList &points, bool closed) {
    return addEntity({{"type", "polyline"}, {"points", points}, {"closed", closed}});
}
int FissionSketch::addSlot(const QVariant &x1, const QVariant &y1, const QVariant &x2, const QVariant &y2,
                           const QVariant &width) {
    return addEntity({{"type", "slot"}, {"x1", x1}, {"y1", y1}, {"x2", x2}, {"y2", y2}, {"w", width}});
}
int FissionSketch::addEllipse(const QVariant &cx, const QVariant &cy, const QVariant &rx, const QVariant &ry) {
    return addEntity({{"type", "ellipse"}, {"cx", cx}, {"cy", cy}, {"rx", rx}, {"ry", ry}});
}
int FissionSketch::addSpline(const QVariantList &points) {
    return addEntity({{"type", "spline"}, {"points", points}});
}
int FissionSketch::addPoint(const QVariant &x, const QVariant &y) {
    return addEntity({{"type", "point"}, {"x", x}, {"y", y}});
}

// --- rebuild ---------------------------------------------------------------

void FissionSketch::resetResult() {
    m_entityEdges.clear();
    m_profileEdges.clear();
    m_curves = TopoDS_Compound();
    m_profiles.clear();
    m_defaultProfiles.clear();
    m_handles.clear();
    m_annotations.clear();
    m_fully.clear();
    m_dof = 0;
}

bool FissionSketch::compute(RebuildContext &ctx) {
    QString err;
    if (!ctx.resolvePlane(get("plane"), m_plane, err)) return fail(err);

    BRep_Builder builder;
    builder.MakeCompound(m_curves);

    SketchSolver solver;
    if (!solver.load(list("entities"), list("constraints"),
                     [&ctx](const QString &e, QString *er) { return ctx.evaluate(e, er); }, err))
        return fail(err);
    QString solveError;
    const bool solved = solver.solve(solveError);
    const QVariantList ents = solver.entities();
    collectSolverResults(solver, ents);

    for (int i = 0; i < ents.size(); ++i) {
        const QVariantMap e = ents[i].toMap();
        QList<TopoDS_Edge> edges;
        if (!buildEntity(ctx, e, edges, err))
            return fail(QString("Entity %1 (%2): %3").arg(i + 1).arg(e.value("type").toString(), err));
        m_entityEdges.append(edges);
        for (const auto &edge : edges) {
            builder.Add(m_curves, edge);
            if (!e.value("construction").toBool()) m_profileEdges.append(edge);
        }
    }
    computeProfiles(m_profileEdges);
    return solved ? true : fail(solveError);
}

static QVariantList uv(const SketchSolver::Point &p) { return {p.u, p.v}; }

void FissionSketch::collectSolverResults(const SketchSolver &s, const QVariantList &ents) {
    m_dof = s.degreesOfFreedom();
    m_fully.resize(ents.size());
    for (int i = 0; i < ents.size(); ++i) {
        m_fully[i] = s.entityFullyConstrained(i);
        const QVariantMap e = ents[i].toMap();
        const bool construction = e.value("construction").toBool();
        auto handle = [&](const QString &role, const SketchSolver::Point &p) {
            m_handles << QVariantMap{{"entity", i}, {"role", role}, {"u", p.u}, {"v", p.v},
                                     {"construction", construction}};
        };
        for (const QString &role : SketchSolver::roles(e)) {
            SketchSolver::Point p;
            if (s.point(i, role, p)) handle(role, p);
        }
        SketchSolver::Point a, b;
        if (s.lineEnds(i, a, b)) handle("mid", {(a.u + b.u) / 2, (a.v + b.v) / 2});
    }

    using P = SketchSolver::Point;
    auto refPoint = [&](const QVariant &r, P &out) {
        const QVariantMap m = r.toMap();
        return s.point(m.value("e", -1).toInt(), m.value("p").toString(), out);
    };
    auto refEntity = [](const QVariant &r) { return r.toMap().value("e", -1).toInt(); };
    // A point that represents a whole curve: line midpoint, top of a circle
    auto curveAnchor = [&](int e, P &out) {
        P a, b, c;
        double r;
        if (s.lineEnds(e, a, b)) { out = {(a.u + b.u) / 2, (a.v + b.v) / 2}; return true; }
        if (s.circle(e, c, r)) {
            if (s.entityType(e) == "arc") {
                P p0, p1;
                s.point(e, "start", p0);
                s.point(e, "end", p1);
                double mu = (p0.u + p1.u) / 2 - c.u, mv = (p0.v + p1.v) / 2 - c.v;
                const double len = std::hypot(mu, mv);
                if (len < 1e-9) { mu = -(p0.v - c.v); mv = p0.u - c.u; }
                const double l2 = std::hypot(mu, mv);
                out = {c.u + r * mu / l2, c.v + r * mv / l2};
                return true;
            }
            out = {c.u, c.v + r};
            return true;
        }
        const QVariantMap em = ents.value(e).toMap();
        const QStringList roles = SketchSolver::roles(em);
        return !roles.isEmpty() && s.point(e, roles.first(), out);
    };

    const QVariantList cons = list("constraints");
    for (int i = 0; i < cons.size(); ++i) {
        const QVariantMap c = cons[i].toMap();
        const QString type = c.value("type").toString();
        QVariantMap a{{"index", i}, {"type", type}};
        P p, q, c0;
        double r = 0;
        if (SketchSolver::isDimension(type)) {
            const QString value = c.value("value").toString();
            bool plain = false;
            value.toDouble(&plain);
            a["value"] = value;
            a["expression"] = !plain;
            a["measured"] = s.measure(c);
            if (c.contains("offset")) a["offset"] = c.value("offset").toDouble();
            if (type == "distance" && refPoint(c.value("a"), p) && refPoint(c.value("b"), q)) {
                a["geom"] = "linear";
                a["orient"] = c.value("orient", "aligned");
                a["p1"] = uv(p);
                a["p2"] = uv(q);
            } else if (type == "pointline") {
                QVariant pr = c.value("a"), lr = c.value("b");
                if (!pr.toMap().contains("p")) std::swap(pr, lr);
                P l0, l1;
                if (!refPoint(pr, p) || !s.lineEnds(refEntity(lr), l0, l1)) continue;
                const double du = l1.u - l0.u, dv = l1.v - l0.v, len2 = du * du + dv * dv;
                const double t = len2 > 0 ? ((p.u - l0.u) * du + (p.v - l0.v) * dv) / len2 : 0;
                a["geom"] = "linear";
                a["orient"] = "aligned";
                a["p1"] = uv(p);
                a["p2"] = uv({l0.u + t * du, l0.v + t * dv});
            } else if ((type == "radius" || type == "diameter") && s.circle(refEntity(c.value("a")), c0, r)) {
                a["geom"] = "radial";
                a["center"] = uv(c0);
                a["r"] = r;
            } else if (type == "angle") {
                P a0, a1, b0, b1;
                if (!s.lineEnds(refEntity(c.value("a")), a0, a1) || !s.lineEnds(refEntity(c.value("b")), b0, b1)) continue;
                // Vertex at the line intersection (or between the lines when parallel)
                const double d1u = a1.u - a0.u, d1v = a1.v - a0.v, d2u = b1.u - b0.u, d2v = b1.v - b0.v;
                const double den = d1u * d2v - d1v * d2u;
                P v;
                if (std::fabs(den) > 1e-12) {
                    const double t = ((b0.u - a0.u) * d2v - (b0.v - a0.v) * d2u) / den;
                    v = {a0.u + t * d1u, a0.v + t * d1v};
                } else {
                    v = {(a0.u + a1.u + b0.u + b1.u) / 4, (a0.v + a1.v + b0.v + b1.v) / 4};
                }
                // Each ray points from the vertex toward the line's farther end
                auto ray = [&](const P &e0, const P &e1) {
                    const P end = std::hypot(e0.u - v.u, e0.v - v.v) > std::hypot(e1.u - v.u, e1.v - v.v) ? e0 : e1;
                    return uv({end.u - v.u, end.v - v.v});
                };
                a["geom"] = "angular";
                a["vertex"] = uv(v);
                a["d1"] = ray(a0, a1);
                a["d2"] = ray(b0, b1);
            } else {
                continue;
            }
        } else {
            QVariantList anchors;
            auto addRef = [&](const QVariant &ref) {
                if (!ref.isValid()) return;
                P pt;
                if (ref.toMap().contains("p") ? refPoint(ref, pt) : curveAnchor(refEntity(ref), pt)) anchors.append(QVariant(uv(pt)));
            };
            if (type == "coincident" || type == "concentric") {
                if (c.value("a").toMap().contains("p")) addRef(c.value("a"));
                else if (c.value("b").toMap().contains("p")) addRef(c.value("b"));
                else if (s.circle(refEntity(c.value("a")), c0, r)) anchors.append(QVariant(uv(c0)));
            } else if (type == "tangent") {
                // Touching point: foot of the circle center on the line, or along the center line
                QVariant lr = c.value("a"), cr = c.value("b");
                if (s.entityType(refEntity(lr)) != "line") std::swap(lr, cr);
                P l0, l1;
                if (s.lineEnds(refEntity(lr), l0, l1) && s.circle(refEntity(cr), c0, r)) {
                    const double du = l1.u - l0.u, dv = l1.v - l0.v, len2 = du * du + dv * dv;
                    const double t = len2 > 0 ? ((c0.u - l0.u) * du + (c0.v - l0.v) * dv) / len2 : 0;
                    anchors.append(QVariant(uv({l0.u + t * du, l0.v + t * dv})));
                } else if (s.circle(refEntity(c.value("a")), p, r) && s.circle(refEntity(c.value("b")), q, r)) {
                    double r1 = 0;
                    s.circle(refEntity(c.value("a")), p, r1);
                    const double du = q.u - p.u, dv = q.v - p.v, len = std::hypot(du, dv);
                    const double k = c.value("inner").toBool() && c.value("sign").toDouble() < 0 ? -r1 : r1;
                    if (len > 1e-9) anchors.append(QVariant(uv({p.u + k * du / len, p.v + k * dv / len})));
                }
            } else {
                for (const char *key : kRefKeys) addRef(c.value(key));
            }
            if (anchors.isEmpty()) continue;
            a["anchors"] = anchors;
        }
        m_annotations << a;
    }
}

bool FissionSketch::buildEntity(RebuildContext &ctx, const QVariantMap &e, QList<TopoDS_Edge> &edges, QString &error) {
    bool ok = true;
    auto val = [&](const QVariant &v) -> double {
        if (!ok) return 0.0;
        if (v.typeId() == QMetaType::Double || v.typeId() == QMetaType::Int) return v.toDouble();
        QString err;
        double d = ctx.evaluate(v.toString(), &err);
        if (!err.isEmpty()) { error = err; ok = false; }
        return d;
    };
    auto num = [&](const char *key) { return val(e.value(key)); };

    const gp_Pnt O = m_plane.Location();
    const gp_Dir X = m_plane.XDirection(), Y = m_plane.YDirection(), N = m_plane.Direction();
    auto P = [&](double u, double v) { return O.Translated(gp_Vec(X) * u + gp_Vec(Y) * v); };
    auto points = [&](const QVariantList &pts) {
        QList<gp_Pnt> out;
        for (const QVariant &p : pts) {
            const QVariantList xy = p.toList();
            if (xy.size() < 2) { error = "Point needs x and y"; ok = false; return out; }
            out.append(P(val(xy[0]), val(xy[1])));
        }
        return out;
    };
    auto line = [&](const gp_Pnt &a, const gp_Pnt &b) {
        if (a.Distance(b) < 1e-7) { error = "Zero-length line"; ok = false; return; }
        edges.append(BRepBuilderAPI_MakeEdge(a, b).Edge());
    };
    auto arc3 = [&](const gp_Pnt &a, const gp_Pnt &m, const gp_Pnt &b) {
        GC_MakeArcOfCircle mk(a, m, b);
        if (!mk.IsDone()) { error = "Arc points are collinear"; ok = false; return; }
        edges.append(BRepBuilderAPI_MakeEdge(mk.Value()).Edge());
    };

    const QString type = e.value("type").toString();
    if (type == "line") {
        const double x1 = num("x1"), y1 = num("y1"), x2 = num("x2"), y2 = num("y2");
        if (ok) line(P(x1, y1), P(x2, y2));
    } else if (type == "circle") {
        const double cx = num("cx"), cy = num("cy"), r = num("r");
        if (ok && r <= 0) { error = "Radius must be positive"; ok = false; }
        if (ok) edges.append(BRepBuilderAPI_MakeEdge(gp_Circ(gp_Ax2(P(cx, cy), N, X), r)).Edge());
    } else if (type == "arc") {
        const double cx = num("cx"), cy = num("cy"), r = num("r");
        double a1 = num("a1"), a2 = num("a2");
        if (ok && r <= 0) { error = "Radius must be positive"; ok = false; }
        if (ok) {
            while (a2 <= a1) a2 += 360.0;
            gp_Circ c(gp_Ax2(P(cx, cy), N, X), r);
            edges.append(BRepBuilderAPI_MakeEdge(c, a1 * M_PI / 180.0, a2 * M_PI / 180.0).Edge());
        }
    } else if (type == "arc3") {
        const double x1 = num("x1"), y1 = num("y1"), xm = num("xm"), ym = num("ym"), x2 = num("x2"), y2 = num("y2");
        if (ok) arc3(P(x1, y1), P(xm, ym), P(x2, y2));
    } else if (type == "rectangle") {
        const double x = num("x"), y = num("y"), w = num("w"), h = num("h");
        if (ok && (std::fabs(w) < 1e-7 || std::fabs(h) < 1e-7)) { error = "Rectangle has zero size"; ok = false; }
        if (ok) {
            const gp_Pnt a = P(x, y), b = P(x + w, y), c = P(x + w, y + h), d = P(x, y + h);
            line(a, b); line(b, c); line(c, d); line(d, a);
        }
    } else if (type == "polygon") {
        const double cx = num("cx"), cy = num("cy"), r = num("r");
        const int n = e.value("sides", 6).toInt();
        if (ok && (n < 3 || r <= 0)) { error = "Polygon needs >= 3 sides and a positive radius"; ok = false; }
        if (ok) {
            QList<gp_Pnt> v;
            for (int i = 0; i < n; ++i) {
                const double a = M_PI / 2 + 2 * M_PI * i / n;
                v.append(P(cx + r * std::cos(a), cy + r * std::sin(a)));
            }
            for (int i = 0; i < n && ok; ++i) line(v[i], v[(i + 1) % n]);
        }
    } else if (type == "polyline") {
        const QList<gp_Pnt> v = points(e.value("points").toList());
        if (ok && v.size() < 2) { error = "Polyline needs at least 2 points"; ok = false; }
        for (int i = 0; ok && i + 1 < v.size(); ++i) line(v[i], v[i + 1]);
        if (ok && e.value("closed").toBool() && v.size() > 2) line(v.last(), v.first());
    } else if (type == "slot") {
        const double x1 = num("x1"), y1 = num("y1"), x2 = num("x2"), y2 = num("y2"), w = num("w");
        if (ok && w <= 0) { error = "Slot width must be positive"; ok = false; }
        if (ok && std::hypot(x2 - x1, y2 - y1) < 1e-7) { error = "Slot centers coincide"; ok = false; }
        if (ok) {
            const double r = w / 2, len = std::hypot(x2 - x1, y2 - y1);
            const double du = (x2 - x1) / len, dv = (y2 - y1) / len;   // slot direction
            const double nu = -dv, nv = du;                             // in-plane normal
            const gp_Pnt a1 = P(x1 + r * nu, y1 + r * nv), a2 = P(x2 + r * nu, y2 + r * nv);
            const gp_Pnt b2 = P(x2 - r * nu, y2 - r * nv), b1 = P(x1 - r * nu, y1 - r * nv);
            line(a1, a2);
            arc3(a2, P(x2 + r * du, y2 + r * dv), b2);
            line(b2, b1);
            arc3(b1, P(x1 - r * du, y1 - r * dv), a1);
        }
    } else if (type == "ellipse") {
        const double cx = num("cx"), cy = num("cy"), rx = num("rx"), ry = num("ry");
        if (ok && (rx <= 0 || ry <= 0)) { error = "Ellipse radii must be positive"; ok = false; }
        if (ok) {
            // gp_Elips requires major >= minor; rotate the frame when the Y radius is larger
            gp_Elips el = rx >= ry ? gp_Elips(gp_Ax2(P(cx, cy), N, X), rx, ry)
                                   : gp_Elips(gp_Ax2(P(cx, cy), N, Y), ry, rx);
            edges.append(BRepBuilderAPI_MakeEdge(el).Edge());
        }
    } else if (type == "spline") {
        const QList<gp_Pnt> v = points(e.value("points").toList());
        if (ok && v.size() < 2) { error = "Spline needs at least 2 points"; ok = false; }
        if (ok) {
            Handle(TColgp_HArray1OfPnt) arr = new TColgp_HArray1OfPnt(1, v.size());
            for (int i = 0; i < v.size(); ++i) arr->SetValue(i + 1, v[i]);
            GeomAPI_Interpolate interp(arr, Standard_False, 1e-7);
            interp.Perform();
            if (!interp.IsDone()) { error = "Spline interpolation failed"; ok = false; }
            else edges.append(BRepBuilderAPI_MakeEdge(interp.Curve()).Edge());
        }
    } else if (type == "point") {
        const double x = num("x"), y = num("y");
        if (ok) {
            BRep_Builder b;
            b.Add(m_curves, BRepBuilderAPI_MakeVertex(P(x, y)).Vertex());
        }
    } else {
        error = "Unknown entity type";
        ok = false;
    }
    return ok;
}

static gp_Pnt interiorPoint(const TopoDS_Face &face) {
    BRepMesh_IncrementalMesh mesh(face, 0.5, Standard_False, 0.5);
    TopLoc_Location loc;
    Handle(Poly_Triangulation) tri = BRep_Tool::Triangulation(face, loc);
    if (tri.IsNull() || tri->NbTriangles() == 0) return GeomUtils::faceCentroid(face);
    // Centroid of the largest triangle is safely inside the region
    double bestArea = -1;
    gp_Pnt best;
    for (int i = 1; i <= tri->NbTriangles(); ++i) {
        int a, b, c;
        tri->Triangle(i).Get(a, b, c);
        const gp_Pnt pa = tri->Node(a), pb = tri->Node(b), pc = tri->Node(c);
        const double area = gp_Vec(pa, pb).Crossed(gp_Vec(pa, pc)).Magnitude();
        if (area > bestArea) {
            bestArea = area;
            best = gp_Pnt((pa.XYZ() + pb.XYZ() + pc.XYZ()) / 3.0);
        }
    }
    return best.Transformed(loc.Transformation());
}

void FissionSketch::computeProfiles(const QList<TopoDS_Edge> &edges) {
    if (edges.isEmpty()) return;

    // Split a face larger than the sketch by every curve; the bounded pieces are the profiles
    Bnd_Box box;
    for (const auto &e : edges) box.Add(GeomUtils::boundingBox(e));
    const double size = std::sqrt(box.SquareExtent()) + 10.0;
    double x0, y0, z0, x1, y1, z1;
    box.Get(x0, y0, z0, x1, y1, z1);
    const gp_Vec toCenter(m_plane.Location(), gp_Pnt((x0 + x1) / 2, (y0 + y1) / 2, (z0 + z1) / 2));
    const double uc = toCenter.Dot(gp_Vec(m_plane.XDirection()));
    const double vc = toCenter.Dot(gp_Vec(m_plane.YDirection()));
    const double umin = uc - size, umax = uc + size, vmin = vc - size, vmax = vc + size;
    const TopoDS_Face base = BRepBuilderAPI_MakeFace(gp_Pln(m_plane), umin, umax, vmin, vmax).Face();

    TopTools_ListOfShape args, tools;
    args.Append(base);
    for (const auto &e : edges) tools.Append(e);
    BRepAlgoAPI_Splitter splitter;
    splitter.SetArguments(args);
    splitter.SetTools(tools);
    splitter.Build();
    if (!splitter.IsDone() || splitter.HasErrors()) return;

    const double eps = size * 1e-6;
    for (TopExp_Explorer exp(splitter.Shape(), TopAbs_FACE); exp.More(); exp.Next()) {
        const TopoDS_Face f = TopoDS::Face(exp.Current());
        double fu0, fu1, fv0, fv1;
        BRepTools::UVBounds(f, fu0, fu1, fv0, fv1);
        const bool touchesBorder = fu0 <= umin + eps || fu1 >= umax - eps || fv0 <= vmin + eps || fv1 >= vmax - eps;
        if (!touchesBorder) m_profiles.append(f);
    }

    // A profile enclosed by another profile's outer boundary is a hole (e.g. the disk inside a ring);
    // the default selection keeps regions at even nesting depth.
    QList<TopoDS_Face> outerFaces;
    for (const auto &f : m_profiles)
        outerFaces.append(BRepBuilderAPI_MakeFace(gp_Pln(m_plane), BRepTools::OuterWire(f), Standard_True).Face());
    for (int i = 0; i < m_profiles.size(); ++i) {
        const gp_Pnt p = interiorPoint(m_profiles[i]);
        int depth = 0;
        for (int j = 0; j < m_profiles.size(); ++j) {
            if (i == j) continue;
            BRepClass_FaceClassifier cls(outerFaces[j], p, 1e-6);
            if (cls.State() == TopAbs_IN) ++depth;
        }
        if (depth % 2 == 0) m_defaultProfiles.append(i);
    }
}

bool FissionSketch::lineAxis(int entity, gp_Ax1 &axis) const {
    if (entity < 0 || entity >= m_entityEdges.size()) return false;
    const QVariantMap e = this->entity(entity);
    if (e.value("type").toString() != "line" || m_entityEdges[entity].isEmpty()) return false;
    const TopoDS_Edge &edge = m_entityEdges[entity].first();
    const gp_Pnt a = BRep_Tool::Pnt(TopExp::FirstVertex(edge)), b = BRep_Tool::Pnt(TopExp::LastVertex(edge));
    axis = gp_Ax1(a, gp_Dir(gp_Vec(a, b)));
    return true;
}

TopoDS_Wire FissionSketch::pathWire(QString &error) const {
    if (m_profileEdges.isEmpty()) { error = "Path sketch has no curves"; return TopoDS_Wire(); }
    Handle(TopTools_HSequenceOfShape) in = new TopTools_HSequenceOfShape;
    for (const auto &e : m_profileEdges) in->Append(e);
    Handle(TopTools_HSequenceOfShape) wires;
    ShapeAnalysis_FreeBounds::ConnectEdgesToWires(in, 1e-4, Standard_False, wires);
    if (wires.IsNull() || wires->Length() != 1) {
        error = "Path must be a single connected chain of curves";
        return TopoDS_Wire();
    }
    return TopoDS::Wire(wires->Value(1));
}
