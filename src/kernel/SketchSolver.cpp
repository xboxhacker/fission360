// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#include "SketchSolver.h"
#include <QSet>
#include <algorithm>
#include <cmath>

namespace {

constexpr double kConverged = 1e-10;   // stop iterating
constexpr double kSatisfied = 1e-7;    // accept the solution

double wrapAngle(double a) {
    while (a > M_PI) a -= 2 * M_PI;
    while (a <= -M_PI) a += 2 * M_PI;
    return a;
}
double sgn(double x) { return x < 0 ? -1.0 : 1.0; }

QStringList scalarKeys(const QString &type) {
    if (type == "line") return {"x1", "y1", "x2", "y2"};
    if (type == "circle" || type == "polygon") return {"cx", "cy", "r"};
    if (type == "arc") return {"cx", "cy", "r", "a1", "a2"};
    if (type == "arc3") return {"x1", "y1", "xm", "ym", "x2", "y2"};
    if (type == "rectangle") return {"x", "y", "w", "h"};
    if (type == "slot") return {"x1", "y1", "x2", "y2", "w"};
    if (type == "ellipse") return {"cx", "cy", "rx", "ry"};
    if (type == "point") return {"x", "y"};
    return {};
}

bool plainNumber(const QVariant &v, double &out) {
    if (v.typeId() == QMetaType::QString) {
        bool ok = false;
        out = v.toString().trimmed().toDouble(&ok);
        return ok;
    }
    bool ok = false;
    out = v.toDouble(&ok);
    return ok;
}

struct Ref {
    int e = -1;
    QString p;
    bool isPoint() const { return !p.isEmpty(); }
};
Ref toRef(const QVariant &v) {
    const QVariantMap m = v.toMap();
    Ref r;
    r.e = m.contains("e") ? m.value("e").toInt() : -1;
    r.p = m.value("p").toString();
    return r;
}

// Solves A y = b in place (Gaussian elimination, partial pivoting)
bool solveDense(QVector<QVector<double>> A, QVector<double> b, QVector<double> &y) {
    const int n = b.size();
    for (int k = 0; k < n; ++k) {
        int piv = k;
        for (int i = k + 1; i < n; ++i)
            if (std::fabs(A[i][k]) > std::fabs(A[piv][k])) piv = i;
        if (std::fabs(A[piv][k]) < 1e-300) return false;
        std::swap(A[k], A[piv]);
        std::swap(b[k], b[piv]);
        for (int i = k + 1; i < n; ++i) {
            const double f = A[i][k] / A[k][k];
            if (f == 0.0) continue;
            for (int j = k; j < n; ++j) A[i][j] -= f * A[k][j];
            b[i] -= f * b[k];
        }
    }
    y.resize(n);
    for (int i = n - 1; i >= 0; --i) {
        double s = b[i];
        for (int j = i + 1; j < n; ++j) s -= A[i][j] * y[j];
        y[i] = s / A[i][i];
        if (!std::isfinite(y[i])) return false;
    }
    return true;
}

double norm2(const QVector<double> &v) {
    double s = 0;
    for (double x : v) s += x * x;
    return std::sqrt(s);
}
double maxAbs(const QVector<double> &v) {
    double m = 0;
    for (double x : v) m = std::max(m, std::fabs(x));
    return m;
}

} // namespace

// ---------------------------------------------------------------------------

QStringList SketchSolver::roles(const QVariantMap &entity) {
    const QString t = entity.value("type").toString();
    if (t == "line" || t == "slot") return {"start", "end"};
    if (t == "circle" || t == "ellipse" || t == "polygon") return {"center"};
    if (t == "arc") return {"center", "start", "end"};
    if (t == "arc3") return {"start", "mid", "end"};
    if (t == "point") return {"pos"};
    if (t == "spline" || t == "polyline") {
        QStringList out;
        const int n = entity.value("points").toList().size();
        for (int i = 0; i < n; ++i) out << QString("p%1").arg(i);
        return out;
    }
    return {};
}

bool SketchSolver::isDimension(const QString &type) {
    return type == "distance" || type == "pointline" || type == "radius" || type == "diameter" || type == "angle";
}

double SketchSolver::val(int e, const QString &key) const {
    const Ent &en = m_ents[e];
    const int i = en.keys.indexOf(key);
    return i >= 0 ? en.vals[i] : 0.0;
}

QString SketchSolver::entityType(int entity) const {
    return entity >= 0 && entity < m_ents.size() ? m_ents[entity].type : QString();
}

bool SketchSolver::point(int e, const QString &role, Point &out) const {
    if (e == -1 && role == "origin") { out = {0, 0}; return true; }
    if (e < 0 || e >= m_ents.size()) return false;
    const QString &t = m_ents[e].type;
    auto P = [&](const char *ku, const char *kv) { out = {val(e, ku), val(e, kv)}; return true; };
    if (t == "line" || t == "slot") {
        if (role == "start") return P("x1", "y1");
        if (role == "end") return P("x2", "y2");
    } else if (t == "circle" || t == "ellipse" || t == "polygon") {
        if (role == "center") return P("cx", "cy");
    } else if (t == "arc") {
        const double cx = val(e, "cx"), cy = val(e, "cy"), r = val(e, "r");
        if (role == "center") return P("cx", "cy");
        const double a = (role == "start" ? val(e, "a1") : val(e, "a2")) * M_PI / 180.0;
        if (role == "start" || role == "end") { out = {cx + r * std::cos(a), cy + r * std::sin(a)}; return true; }
    } else if (t == "arc3") {
        if (role == "start") return P("x1", "y1");
        if (role == "mid") return P("xm", "ym");
        if (role == "end") return P("x2", "y2");
    } else if (t == "point") {
        if (role == "pos") return P("x", "y");
    } else if (t == "spline" || t == "polyline") {
        if (role.startsWith('p')) {
            bool ok = false;
            const int i = role.mid(1).toInt(&ok);
            const QString ku = QString("points/%1/0").arg(i), kv = QString("points/%1/1").arg(i);
            if (ok && m_ents[e].keys.contains(ku)) { out = {val(e, ku), val(e, kv)}; return true; }
        }
    }
    return false;
}

bool SketchSolver::lineEnds(int e, Point &a, Point &b) const {
    if (entityType(e) != "line") return false;
    return point(e, "start", a) && point(e, "end", b);
}

bool SketchSolver::circle(int e, Point &center, double &radius) const {
    const QString t = entityType(e);
    if (t != "circle" && t != "arc") return false;
    center = {val(e, "cx"), val(e, "cy")};
    radius = val(e, "r");
    return true;
}

// ---------------------------------------------------------------------------

bool SketchSolver::load(const QVariantList &entities, const QVariantList &constraints, const Evaluator &eval,
                        QString &error) {
    m_ents.clear();
    m_vars.clear();
    m_res.clear();
    m_resCount = 0;
    m_fully.clear();

    QSet<int> fixed;
    for (const QVariant &cv : constraints) {
        const QVariantMap c = cv.toMap();
        if (c.value("type").toString() == "fix") fixed.insert(toRef(c.value("a")).e);
    }

    for (int i = 0; i < entities.size(); ++i) {
        const QVariantMap e = entities[i].toMap();
        if (e.value("fixed").toBool()) fixed.insert(i);
        Ent en;
        en.src = e;
        en.type = e.value("type").toString();
        en.keys = scalarKeys(en.type);
        QList<QVariant> values;
        for (const QString &k : en.keys) values << e.value(k);
        if (en.type == "spline" || en.type == "polyline") {
            const QVariantList pts = e.value("points").toList();
            for (int j = 0; j < pts.size(); ++j) {
                const QVariantList xy = pts[j].toList();
                for (int k = 0; k < 2; ++k) {
                    en.keys << QString("points/%1/%2").arg(j).arg(k);
                    values << (k < xy.size() ? xy[k] : QVariant(0.0));
                }
            }
        }
        for (const QVariant &v : values) {
            double d = 0;
            const bool number = plainNumber(v, d);
            if (!number) {
                QString err;
                d = eval(v.toString(), &err);
                if (!err.isEmpty()) {
                    error = QString("Entity %1 (%2): %3").arg(i + 1).arg(en.type, err);
                    return false;
                }
            }
            en.vals << d;
            en.free << (number && !fixed.contains(i));
        }
        m_ents << en;
    }
    for (int i = 0; i < m_ents.size(); ++i)
        for (int s = 0; s < m_ents[i].vals.size(); ++s)
            if (m_ents[i].free[s]) m_vars << qMakePair(i, s);

    for (int i = 0; i < constraints.size(); ++i) {
        QVariantMap c = constraints[i].toMap();
        if (c.value("type").toString() == "fix") continue;
        if (isDimension(c.value("type").toString())) {
            QString err;
            const double v = eval(c.value("value").toString(), &err);
            if (!err.isEmpty()) { error = QString("Dimension %1: %2").arg(i + 1).arg(err); return false; }
            c["__value"] = v;
        }
        QString err;
        if (!addResidual(c, err)) {
            error = QString("Constraint %1 (%2): %3").arg(i + 1).arg(c.value("type").toString(), err);
            return false;
        }
    }
    return true;
}

bool SketchSolver::addResidual(const QVariantMap &c, QString &error) {
    const QString type = c.value("type").toString();
    Ref a = toRef(c.value("a")), b = toRef(c.value("b"));
    const Ref cc = toRef(c.value("c"));
    const double value = c.value("__value").toDouble();
    const double sign = c.contains("sign") ? sgn(c.value("sign").toDouble()) : 1.0;
    const bool inner = c.value("inner").toBool();

    auto validPoint = [&](const Ref &r) { Point p; return r.isPoint() && point(r.e, r.p, p); };
    auto isLine = [&](const Ref &r) { return !r.isPoint() && entityType(r.e) == "line"; };
    auto isCirc = [&](const Ref &r) {
        const QString t = entityType(r.e);
        return !r.isPoint() && (t == "circle" || t == "arc");
    };
    auto add = [&](int count, std::function<void(double *)> f) {
        m_res.append({count, std::move(f)});
        m_resCount += count;
        return true;
    };
    auto fail = [&](const QString &msg) { error = msg; return false; };

    // Accessors reading the current values
    auto P = [this](const Ref &r) { Point p; point(r.e, r.p, p); return p; };
    auto L = [this](int e, Point &p0, Point &dir, double &len) {
        Point p1;
        lineEnds(e, p0, p1);
        dir = {p1.u - p0.u, p1.v - p0.v};
        len = std::hypot(dir.u, dir.v);
        if (len > 1e-12) { dir.u /= len; dir.v /= len; }
    };
    auto C = [this](int e, Point &c, double &r) { circle(e, c, r); };

    if (type == "coincident") {
        if (!a.isPoint() && b.isPoint()) std::swap(a, b);
        if (!validPoint(a)) return fail("first item must be a point");
        if (validPoint(b))
            return add(2, [=](double *o) { const Point p = P(a), q = P(b); o[0] = p.u - q.u; o[1] = p.v - q.v; });
        if (isLine(b))
            return add(1, [=](double *o) {
                Point p0, d; double len; L(b.e, p0, d, len);
                const Point p = P(a);
                o[0] = d.u * (p.v - p0.v) - d.v * (p.u - p0.u);
            });
        if (isCirc(b))
            return add(1, [=](double *o) {
                Point c0; double r; C(b.e, c0, r);
                const Point p = P(a);
                o[0] = std::hypot(p.u - c0.u, p.v - c0.v) - r;
            });
        return fail("second item must be a point or a line, circle or arc");
    }
    if (type == "horizontal" || type == "vertical") {
        const bool h = type == "horizontal";
        if (isLine(a))
            return add(1, [=](double *o) { Point p0, p1; lineEnds(a.e, p0, p1); o[0] = h ? p1.v - p0.v : p1.u - p0.u; });
        if (validPoint(a) && validPoint(b))
            return add(1, [=](double *o) { const Point p = P(a), q = P(b); o[0] = h ? q.v - p.v : q.u - p.u; });
        return fail("select a line or two points");
    }
    if (type == "parallel" || type == "perpendicular" || type == "collinear") {
        if (!isLine(a) || !isLine(b)) return fail("select two lines");
        if (type == "parallel")
            return add(1, [=](double *o) {
                Point p, d1, d2; double l;
                L(a.e, p, d1, l); L(b.e, p, d2, l);
                o[0] = d1.u * d2.v - d1.v * d2.u;
            });
        if (type == "perpendicular")
            return add(1, [=](double *o) {
                Point p, d1, d2; double l;
                L(a.e, p, d1, l); L(b.e, p, d2, l);
                o[0] = d1.u * d2.u + d1.v * d2.v;
            });
        return add(2, [=](double *o) {
            Point p0, d; double l; L(a.e, p0, d, l);
            Point q0, q1; lineEnds(b.e, q0, q1);
            o[0] = d.u * (q0.v - p0.v) - d.v * (q0.u - p0.u);
            o[1] = d.u * (q1.v - p0.v) - d.v * (q1.u - p0.u);
        });
    }
    if (type == "tangent") {
        if (isCirc(a) && isLine(b)) std::swap(a, b);
        if (isLine(a) && isCirc(b))
            return add(1, [=](double *o) {
                Point p0, d; double l; L(a.e, p0, d, l);
                Point c0; double r; C(b.e, c0, r);
                o[0] = sign * (d.u * (c0.v - p0.v) - d.v * (c0.u - p0.u)) - r;
            });
        if (isCirc(a) && isCirc(b))
            return add(1, [=](double *o) {
                Point c1, c2; double r1, r2;
                C(a.e, c1, r1); C(b.e, c2, r2);
                const double d = std::hypot(c1.u - c2.u, c1.v - c2.v);
                o[0] = inner ? d - sign * (r1 - r2) : d - (r1 + r2);
            });
        return fail("select a line and a circle/arc, or two circles/arcs");
    }
    if (type == "equal") {
        if (isLine(a) && isLine(b))
            return add(1, [=](double *o) {
                Point p, d; double l1, l2;
                L(a.e, p, d, l1); L(b.e, p, d, l2);
                o[0] = l1 - l2;
            });
        if (isCirc(a) && isCirc(b))
            return add(1, [=](double *o) { Point c; double r1, r2; C(a.e, c, r1); C(b.e, c, r2); o[0] = r1 - r2; });
        return fail("select two lines or two circles/arcs");
    }
    if (type == "concentric") {
        if (!isCirc(a) || !isCirc(b)) return fail("select two circles or arcs");
        return add(2, [=](double *o) {
            Point c1, c2; double r;
            C(a.e, c1, r); C(b.e, c2, r);
            o[0] = c1.u - c2.u; o[1] = c1.v - c2.v;
        });
    }
    if (type == "midpoint") {
        if (isLine(a) && validPoint(b)) std::swap(a, b);
        if (!validPoint(a) || !isLine(b)) return fail("select a point and a line");
        return add(2, [=](double *o) {
            Point p0, p1; lineEnds(b.e, p0, p1);
            const Point p = P(a);
            o[0] = p.u - (p0.u + p1.u) / 2; o[1] = p.v - (p0.v + p1.v) / 2;
        });
    }
    if (type == "symmetric") {
        if (!validPoint(a) || !validPoint(b) || !isLine(cc)) return fail("select two points and a symmetry line");
        return add(2, [=](double *o) {
            Point l0, d; double l; L(cc.e, l0, d, l);
            const Point p = P(a), q = P(b);
            const double mu = (p.u + q.u) / 2, mv = (p.v + q.v) / 2;
            o[0] = d.u * (mv - l0.v) - d.v * (mu - l0.u);
            o[1] = d.u * (q.u - p.u) + d.v * (q.v - p.v);
        });
    }
    if (type == "distance") {
        if (!validPoint(a) || !validPoint(b)) return fail("select two points");
        const QString orient = c.value("orient", "aligned").toString();
        return add(1, [=](double *o) {
            const Point p = P(a), q = P(b);
            if (orient == "horizontal") o[0] = sign * (q.u - p.u) - value;
            else if (orient == "vertical") o[0] = sign * (q.v - p.v) - value;
            else o[0] = std::hypot(q.u - p.u, q.v - p.v) - value;
        });
    }
    if (type == "pointline") {
        if (isLine(a) && validPoint(b)) std::swap(a, b);
        if (!validPoint(a) || !isLine(b)) return fail("select a point and a line");
        return add(1, [=](double *o) {
            Point p0, d; double l; L(b.e, p0, d, l);
            const Point p = P(a);
            o[0] = sign * (d.u * (p.v - p0.v) - d.v * (p.u - p0.u)) - value;
        });
    }
    if (type == "radius" || type == "diameter") {
        if (!isCirc(a)) return fail("select a circle or arc");
        const double k = type == "diameter" ? 2.0 : 1.0;
        return add(1, [=](double *o) { Point c0; double r; C(a.e, c0, r); o[0] = k * r - value; });
    }
    if (type == "angle") {
        if (!isLine(a) || !isLine(b)) return fail("select two lines");
        return add(1, [=](double *o) {
            Point p, d1, d2; double l;
            L(a.e, p, d1, l); L(b.e, p, d2, l);
            const double ang = std::atan2(d1.u * d2.v - d1.v * d2.u, d1.u * d2.u + d1.v * d2.v);
            o[0] = wrapAngle(ang - sign * value * M_PI / 180.0) * 10.0;   // ~10 mm lever arm
        });
    }
    if (type == "dragto") {
        const double tu = c.value("u").toDouble(), tv = c.value("v").toDouble();
        if (validPoint(a))
            return add(2, [=](double *o) { const Point p = P(a); o[0] = p.u - tu; o[1] = p.v - tv; });
        if (isCirc(a))
            return add(1, [=](double *o) { Point c0; double r; C(a.e, c0, r); o[0] = std::hypot(tu - c0.u, tv - c0.v) - r; });
        return fail("drag target must be a point or a circle / arc");
    }
    return fail("unknown constraint type");
}

double SketchSolver::measure(const QVariantMap &c) const {
    const QString type = c.value("type").toString();
    Ref a = toRef(c.value("a")), b = toRef(c.value("b"));
    Point p, q, c0, p0, p1;
    double r = 0;
    if (type == "distance") {
        if (!point(a.e, a.p, p) || !point(b.e, b.p, q)) return 0;
        const QString orient = c.value("orient", "aligned").toString();
        if (orient == "horizontal") return std::fabs(q.u - p.u);
        if (orient == "vertical") return std::fabs(q.v - p.v);
        return std::hypot(q.u - p.u, q.v - p.v);
    }
    if (type == "pointline") {
        if (!a.isPoint()) std::swap(a, b);
        if (!point(a.e, a.p, p) || !lineEnds(b.e, p0, p1)) return 0;
        const double len = std::hypot(p1.u - p0.u, p1.v - p0.v);
        return len > 1e-12 ? std::fabs((p1.u - p0.u) * (p.v - p0.v) - (p1.v - p0.v) * (p.u - p0.u)) / len : 0;
    }
    if (type == "radius" || type == "diameter") {
        if (!circle(a.e, c0, r)) return 0;
        return type == "diameter" ? 2 * r : r;
    }
    if (type == "angle") {
        Point q0, q1;
        if (!lineEnds(a.e, p0, p1) || !lineEnds(b.e, q0, q1)) return 0;
        const double ux = p1.u - p0.u, uy = p1.v - p0.v, vx = q1.u - q0.u, vy = q1.v - q0.v;
        return std::fabs(std::atan2(ux * vy - uy * vx, ux * vx + uy * vy)) * 180.0 / M_PI;
    }
    return 0;
}

QVariantMap SketchSolver::configuration(const QVariantMap &c) const {
    const QString type = c.value("type").toString();
    Ref a = toRef(c.value("a")), b = toRef(c.value("b"));
    Point p, q, p0, p1, c1, c2;
    double r1 = 0, r2 = 0;
    QVariantMap out;
    auto signedDist = [&](int line, const Point &pt) {
        lineEnds(line, p0, p1);
        const double len = std::hypot(p1.u - p0.u, p1.v - p0.v);
        return len > 1e-12 ? ((p1.u - p0.u) * (pt.v - p0.v) - (p1.v - p0.v) * (pt.u - p0.u)) / len : 0.0;
    };
    if (type == "distance") {
        const QString orient = c.value("orient", "aligned").toString();
        if (point(a.e, a.p, p) && point(b.e, b.p, q) && orient != "aligned")
            out["sign"] = sgn(orient == "horizontal" ? q.u - p.u : q.v - p.v);
    } else if (type == "pointline") {
        if (!a.isPoint()) std::swap(a, b);
        if (point(a.e, a.p, p)) out["sign"] = sgn(signedDist(b.e, p));
    } else if (type == "angle") {
        Point q0, q1;
        if (lineEnds(a.e, p0, p1) && lineEnds(b.e, q0, q1)) {
            const double ux = p1.u - p0.u, uy = p1.v - p0.v, vx = q1.u - q0.u, vy = q1.v - q0.v;
            out["sign"] = sgn(std::atan2(ux * vy - uy * vx, ux * vx + uy * vy));
        }
    } else if (type == "tangent") {
        if (circle(a.e, c1, r1) && entityType(b.e) == "line") std::swap(a, b);
        if (entityType(a.e) == "line" && circle(b.e, c2, r2)) {
            out["sign"] = sgn(signedDist(a.e, c2));
        } else if (circle(a.e, c1, r1) && circle(b.e, c2, r2)) {
            const double d = std::hypot(c1.u - c2.u, c1.v - c2.v);
            const bool inner = d < std::max(r1, r2);
            out["inner"] = inner;
            if (inner) out["sign"] = sgn(r1 - r2);
        }
    }
    return out;
}

// ---------------------------------------------------------------------------

void SketchSolver::residuals(QVector<double> &out) const {
    out.resize(m_resCount);
    int k = 0;
    for (const Residual &r : m_res) {
        r.f(out.data() + k);
        k += r.count;
    }
}

QVector<double> SketchSolver::x() const {
    QVector<double> v;
    v.reserve(m_vars.size());
    for (const auto &[e, s] : m_vars) v << m_ents[e].vals[s];
    return v;
}

void SketchSolver::setX(const QVector<double> &v) {
    for (int i = 0; i < m_vars.size(); ++i) m_ents[m_vars[i].first].vals[m_vars[i].second] = v[i];
}

void SketchSolver::jacobian(QVector<QVector<double>> &J) {
    const int n = m_vars.size(), m = m_resCount;
    J.assign(m, QVector<double>(n, 0.0));
    QVector<double> rp, rm;
    for (int j = 0; j < n; ++j) {
        double &slot = m_ents[m_vars[j].first].vals[m_vars[j].second];
        const double x0 = slot, h = 1e-6 * std::max(1.0, std::fabs(x0));
        slot = x0 + h;
        residuals(rp);
        slot = x0 - h;
        residuals(rm);
        slot = x0;
        for (int i = 0; i < m; ++i) J[i][j] = (rp[i] - rm[i]) / (2 * h);
    }
}

bool SketchSolver::solve(QString &error) {
    const int n = m_vars.size(), m = m_resCount;
    QVector<double> r;
    residuals(r);
    if (m > 0 && n > 0) {
        double lambda = 1e-10;
        QVector<QVector<double>> J;
        for (int it = 0; it < 200 && maxAbs(r) > kConverged; ++it) {
            jacobian(J);
            QVector<QVector<double>> JJt(m, QVector<double>(m, 0.0));
            for (int i = 0; i < m; ++i)
                for (int k = i; k < m; ++k) {
                    double s = 0;
                    for (int j = 0; j < n; ++j) s += J[i][j] * J[k][j];
                    JJt[i][k] = JJt[k][i] = s;
                }
            const QVector<double> x0 = x();
            const double r0 = norm2(r);
            bool improved = false;
            for (int tries = 0; tries < 16 && !improved; ++tries) {
                QVector<QVector<double>> A = JJt;
                for (int i = 0; i < m; ++i) A[i][i] += lambda * std::max(1.0, JJt[i][i]);
                QVector<double> y;
                if (solveDense(A, r, y)) {
                    // Minimum-norm step: dx = -J^T (J J^T + lambda I)^-1 r
                    QVector<double> xn = x0;
                    for (int j = 0; j < n; ++j) {
                        double s = 0;
                        for (int i = 0; i < m; ++i) s += J[i][j] * y[i];
                        xn[j] -= s;
                    }
                    setX(xn);
                    QVector<double> rn;
                    residuals(rn);
                    if (norm2(rn) < r0) {
                        r = rn;
                        lambda = std::max(lambda * 0.1, 1e-12);
                        improved = true;
                        break;
                    }
                }
                setX(x0);
                lambda *= 10;
            }
            if (!improved) break;
        }
    }
    analyze();
    if (maxAbs(r) > kSatisfied) {
        error = "The sketch constraints conflict and cannot all be satisfied";
        return false;
    }
    return true;
}

void SketchSolver::analyze() {
    const int n = m_vars.size();
    QVector<QVector<double>> J;
    if (m_resCount > 0 && n > 0) jacobian(J);

    // Orthonormal basis of the Jacobian's row space; a variable is determined
    // when its unit vector lies in that space.
    QVector<QVector<double>> basis;
    for (const QVector<double> &row : J) {
        QVector<double> v = row;
        const double rowNorm = norm2(row);
        if (rowNorm < 1e-12) continue;
        for (int pass = 0; pass < 2; ++pass)
            for (const auto &q : basis) {
                double d = 0;
                for (int j = 0; j < n; ++j) d += v[j] * q[j];
                for (int j = 0; j < n; ++j) v[j] -= d * q[j];
            }
        const double len = norm2(v);
        if (len > 1e-7 * rowNorm) {
            for (double &x : v) x /= len;
            basis << v;
        }
    }
    m_dof = n - basis.size();

    QVector<bool> determined(n, false);
    for (int j = 0; j < n; ++j) {
        double s = 0;
        for (const auto &q : basis) s += q[j] * q[j];
        determined[j] = s > 1 - 1e-6;
    }
    for (int e = 0; e < m_ents.size(); ++e) m_fully[e] = true;
    for (int j = 0; j < n; ++j)
        if (!determined[j]) m_fully[m_vars[j].first] = false;
}

// Writes the entity's values into map; onlyFree keeps expression / fixed fields untouched
static void writeValues(QVariantMap &map, const QStringList &keys, const QVector<double> &vals,
                        const QVector<bool> &free, bool onlyFree, bool round) {
    QVariantList pts = map.value("points").toList();
    bool ptsChanged = false;
    for (int s = 0; s < keys.size(); ++s) {
        if (onlyFree && !free[s]) continue;
        double v = vals[s];
        if (round) v = std::round(v * 1e6) / 1e6 + 0.0;
        const QString &k = keys[s];
        if (k.startsWith("points/")) {
            const QStringList parts = k.split('/');
            const int j = parts[1].toInt(), c = parts[2].toInt();
            QVariantList xy = pts.value(j).toList();
            while (xy.size() < 2) xy << 0.0;
            xy[c] = v;
            pts[j] = xy;
            ptsChanged = true;
        } else {
            map[k] = v;
        }
    }
    if (ptsChanged) map["points"] = pts;
}

QVariantList SketchSolver::entities() const {
    QVariantList out;
    for (const Ent &en : m_ents) {
        QVariantMap m = en.src;
        writeValues(m, en.keys, en.vals, en.free, false, false);
        out << m;
    }
    return out;
}

QVariantList SketchSolver::storedEntities(const QVariantList &original) const {
    QVariantList out = original;
    for (int i = 0; i < m_ents.size() && i < out.size(); ++i) {
        QVariantMap m = out[i].toMap();
        writeValues(m, m_ents[i].keys, m_ents[i].vals, m_ents[i].free, true, true);
        out[i] = m;
    }
    return out;
}
