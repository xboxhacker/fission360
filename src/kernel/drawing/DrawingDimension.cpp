// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#include "DrawingDimension.h"

#include <QtMath>
#include <algorithm>
#include <cmath>

namespace DrawingDimension {
namespace {

struct V {
    double x = 0, y = 0;
    V operator+(V o) const { return {x + o.x, y + o.y}; }
    V operator-(V o) const { return {x - o.x, y - o.y}; }
    V operator*(double s) const { return {x * s, y * s}; }
    double dot(V o) const { return x * o.x + y * o.y; }
    double cross(V o) const { return x * o.y - y * o.x; }
    double len() const { return std::hypot(x, y); }
    V unit() const { const double l = len(); return l > 1e-12 ? V{x / l, y / l} : V{1, 0}; }
    V normal() const { return {-y, x}; }
};

constexpr double kArrow = 3.0;     // arrow length on the sheet, mm
constexpr double kGap = 1.0;       // gap between the part and an extension line
constexpr double kOvershoot = 2.0; // extension line past the dimension line

V at(const QVariantMap &m, const char *x, const char *y) { return {m.value(x).toDouble(), m.value(y).toDouble()}; }
QVariantList seg(V a, V b) { return {a.x, a.y, b.x, b.y}; }
QVariantList arrow(V tip, V dir) { return {tip.x, tip.y, dir.x, dir.y}; }
QVariantMap error(const QString &why) { return {{"error", why}}; }

// Foot of the perpendicular from p onto the infinite line a-b
V foot(V p, V a, V b) {
    const V u = (b - a).unit();
    return a + u * (p - a).dot(u);
}

// Older dimensions were a plain line between two clicked points with their own text
QVariantMap normalized(const QVariantMap &ann) {
    if (ann.contains("dimType")) return ann;
    QVariantMap d = ann;
    d["dimType"] = QStringLiteral("linear");
    d["axis"] = QStringLiteral("a");
    d["p1x"] = ann.value("x"); d["p1y"] = ann.value("y");
    d["p2x"] = ann.value("x2", ann.value("x")); d["p2y"] = ann.value("y2", ann.value("y"));
    d["px"] = ann.value("x"); d["py"] = ann.value("y");
    return d;
}

// Readable text angle: never upside down
double readable(double deg) {
    while (deg > 90) deg -= 180;
    while (deg <= -90) deg += 180;
    return deg;
}

}  // namespace

QVariantMap build(const QString &tool, const QVariantMap &a, const QVariantMap &b, double placeX, double placeY,
                  double viewScale) {
    const QString ka = a.value("kind").toString(), kb = b.value("kind").toString();
    if (ka.isEmpty()) return error("Click a line, arc, circle or corner first");
    const V place{placeX, placeY};
    QVariantMap d{{"kind", "dimension"}, {"tool", tool}, {"viewScale", viewScale > 0 ? viewScale : 1.0}, {"decimals", 2},
                  {"text", ""}, {"fontSize", 3.5}, {"px", place.x}, {"py", place.y}, {"x", place.x}, {"y", place.y},
                  {"view", a.value("viewId")}};

    // Radius / diameter of one arc or circle
    if (ka == "circle" && kb.isEmpty() && tool != "linear" && tool != "aligned" && tool != "angular") {
        const bool diameter = tool == "diameter" || (tool != "radius" && a.value("full").toBool());
        d["dimType"] = diameter ? QStringLiteral("diameter") : QStringLiteral("radius");
        d["cx"] = a.value("cx"); d["cy"] = a.value("cy"); d["r"] = a.value("r");
        return d;
    }
    if (tool == "radius" || tool == "diameter") return error("Click an arc or a circle");

    // Two lines: an angle, or the distance between parallel lines
    if (ka == "line" && kb == "line") {
        const V a1 = at(a, "x1", "y1"), a2 = at(a, "x2", "y2"), b1 = at(b, "x1", "y1"), b2 = at(b, "x2", "y2");
        const V u = (a2 - a1).unit(), w = (b2 - b1).unit();
        const bool parallel = std::fabs(u.cross(w)) < 0.017;   // within 1 degree
        if (!parallel && tool != "linear" && tool != "aligned") {
            // Vertex where the lines meet; each ray points to the line's end away from it
            const double t = (b1 - a1).cross(w) / u.cross(w);
            const V vx = a1 + u * t;
            const V r1 = ((a2 - vx).len() > (a1 - vx).len() ? a2 - vx : a1 - vx).unit();
            const V r2 = ((b2 - vx).len() > (b1 - vx).len() ? b2 - vx : b1 - vx).unit();
            // The sector the click is in decides which of the four angles is meant
            const V q = place - vx;
            V d1 = r1, d2 = r2;
            for (const V s : {V{1, 1}, V{-1, 1}, V{-1, -1}, V{1, -1}}) {
                const V c1 = r1 * s.x, c2 = r2 * s.y;
                const double span = c1.cross(c2);
                if (span * c1.cross(q) >= 0 && span * q.cross(c2) >= 0) { d1 = c1; d2 = c2; break; }
            }
            d["dimType"] = QStringLiteral("angular");
            d["vx"] = vx.x; d["vy"] = vx.y;
            d["d1x"] = d1.x; d["d1y"] = d1.y; d["d2x"] = d2.x; d["d2y"] = d2.y;
            return d;
        }
        if (tool == "angular") return error("The lines are parallel; pick two lines that meet");
        const V p1 = (a1 + a2) * 0.5;
        const V p2 = parallel ? foot(p1, b1, b2) : (b1 + b2) * 0.5;
        if ((p2 - p1).len() < 1e-6) return error("The lines lie on top of each other");
        d["dimType"] = QStringLiteral("linear");
        d["axis"] = QStringLiteral("a");
        d["p1x"] = p1.x; d["p1y"] = p1.y; d["p2x"] = p2.x; d["p2y"] = p2.y;
        return d;
    }
    if (tool == "angular") return error("Click two lines that meet");

    // Distances: a single line's length, or between points, circle centres and lines
    auto anchor = [](const QVariantMap &m, const QString &k, V &p) {
        if (k == "point") { p = at(m, "x", "y"); return true; }
        if (k == "circle") { p = at(m, "cx", "cy"); return true; }
        return false;
    };
    V p1, p2;
    bool perpendicular = false;
    if (kb.isEmpty()) {
        if (ka != "line") return error("Click a second point, line or circle");
        p1 = at(a, "x1", "y1");
        p2 = at(a, "x2", "y2");
    } else if (ka == "line" || kb == "line") {
        const QVariantMap &line = ka == "line" ? a : b, &other = ka == "line" ? b : a;
        if (!anchor(other, other.value("kind").toString(), p1)) return error("Pick a point or circle and a line");
        p2 = foot(p1, at(line, "x1", "y1"), at(line, "x2", "y2"));
        perpendicular = true;
    } else {
        anchor(a, ka, p1);
        anchor(b, kb, p2);
    }
    if ((p2 - p1).len() < 1e-6) return error("Those two picks are at the same place");

    QString axis = QStringLiteral("a");
    const V span = p2 - p1;
    const bool square = std::fabs(span.x) < 1e-6 * span.len() || std::fabs(span.y) < 1e-6 * span.len();
    if (tool == "linear" || (tool == "dimension" && !perpendicular && (kb.isEmpty() ? square : true))) {
        // Horizontal when the line is placed above or below the picks, vertical when beside them
        const double outY = std::max({0.0, std::min(p1.y, p2.y) - place.y, place.y - std::max(p1.y, p2.y)});
        const double outX = std::max({0.0, std::min(p1.x, p2.x) - place.x, place.x - std::max(p1.x, p2.x)});
        axis = outY >= outX ? QStringLiteral("h") : QStringLiteral("v");
        if ((axis == "h" && std::fabs(span.x) < 1e-6) || (axis == "v" && std::fabs(span.y) < 1e-6))
            axis = axis == "h" ? QStringLiteral("v") : QStringLiteral("h");
    }
    d["dimType"] = QStringLiteral("linear");
    d["axis"] = axis;
    d["p1x"] = p1.x; d["p1y"] = p1.y; d["p2x"] = p2.x; d["p2y"] = p2.y;
    return d;
}

double value(const QVariantMap &raw) {
    const QVariantMap a = normalized(raw);
    const double scale = std::max(1e-9, a.value("viewScale", 1.0).toDouble());
    const QString t = a.value("dimType").toString();
    if (t == "radius") return a.value("r").toDouble() / scale;
    if (t == "diameter") return 2 * a.value("r").toDouble() / scale;
    if (t == "angular") {
        const V d1 = at(a, "d1x", "d1y"), d2 = at(a, "d2x", "d2y");
        return qRadiansToDegrees(std::fabs(std::atan2(d1.cross(d2), d1.dot(d2))));
    }
    const V p1 = at(a, "p1x", "p1y"), p2 = at(a, "p2x", "p2y");
    const QString axis = a.value("axis").toString();
    if (axis == "h") return std::fabs(p2.x - p1.x) / scale;
    if (axis == "v") return std::fabs(p2.y - p1.y) / scale;
    return (p2 - p1).len() / scale;
}

QString label(const QVariantMap &raw) {
    const QVariantMap a = normalized(raw);
    const QString override = a.value("text").toString();   // typed by the user, or an older dimension's text
    if (!override.isEmpty()) return override;
    QString n = QString::number(value(a), 'f', std::clamp(a.value("decimals", 2).toInt(), 0, 6));
    if (n.contains('.')) {
        while (n.endsWith('0')) n.chop(1);
        if (n.endsWith('.')) n.chop(1);
    }
    const QString t = a.value("dimType").toString();
    if (t == "radius") return "R" + n;
    if (t == "diameter") return QString(QChar(0x00D8)) + n;
    if (t == "angular") return n + QChar(0x00B0);
    return n;
}

QVariantMap geometry(const QVariantMap &raw) {
    const QVariantMap a = normalized(raw);
    const QString t = a.value("dimType").toString();
    const double size = a.value("fontSize", 3.5).toDouble();
    const V place = at(a, "px", "py");
    QVariantList lines, arcs, arrows;
    QVariantMap text{{"str", label(raw)}, {"size", size}, {"angle", 0.0}};

    if (t == "radius" || t == "diameter") {
        const V c = at(a, "cx", "cy");
        const double r = a.value("r").toDouble();
        const V v = (place - c).unit();
        const V edge = c + v * r;
        const bool outside = (place - c).len() > r;
        if (t == "radius") {
            lines << QVariant(seg(outside ? edge : c, place));
            if (!outside) lines << QVariant(seg(c, edge));
            arrows << QVariant(arrow(edge, outside ? v * -1 : v));
        } else {
            const V far = c - v * r;
            lines << QVariant(seg(far, outside ? place : edge));
            arrows << QVariant(arrow(edge, v)) << QVariant(arrow(far, v * -1));
        }
        // Centre mark
        lines << QVariant(seg(c - V{1.5, 0}, c + V{1.5, 0})) << QVariant(seg(c - V{0, 1.5}, c + V{0, 1.5}));
        const V tp = place + V{0, -size * 0.6};   // just above where it was placed
        text["x"] = tp.x; text["y"] = tp.y;
    } else if (t == "angular") {
        const V vx = at(a, "vx", "vy"), d1 = at(a, "d1x", "d1y"), d2 = at(a, "d2x", "d2y");
        const double r = std::max(5.0, (place - vx).len());
        const double a1 = std::atan2(d1.y, d1.x);
        const double sweep = std::atan2(d1.cross(d2), d1.dot(d2));
        arcs << QVariant(QVariantList{vx.x, vx.y, r, qRadiansToDegrees(a1), qRadiansToDegrees(sweep)});
        lines << QVariant(seg(vx, vx + d1 * (r + kOvershoot))) << QVariant(seg(vx, vx + d2 * (r + kOvershoot)));
        const double s = sweep >= 0 ? 1 : -1;
        const double a2 = a1 + sweep;
        arrows << QVariant(arrow(vx + V{std::cos(a1), std::sin(a1)} * r, V{std::sin(a1), -std::cos(a1)} * s))
               << QVariant(arrow(vx + V{std::cos(a2), std::sin(a2)} * r, V{-std::sin(a2), std::cos(a2)} * s));
        const double mid = a1 + sweep / 2;
        const V tp = vx + V{std::cos(mid), std::sin(mid)} * (r + size * 0.9);
        text["x"] = tp.x; text["y"] = tp.y;
    } else {
        const V p1 = at(a, "p1x", "p1y"), p2 = at(a, "p2x", "p2y");
        const QString axis = a.value("axis").toString();
        const V u = axis == "h" ? V{1, 0} : axis == "v" ? V{0, 1} : (p2 - p1).unit();
        const V n = u.normal();
        const V q1 = p1 + n * (place - p1).dot(n), q2 = p2 + n * (place - p2).dot(n);
        for (const auto &[p, q] : {std::pair{p1, q1}, std::pair{p2, q2}}) {
            const V off = q - p;
            if (off.len() < kGap) continue;
            const V dir = off.unit();
            lines << QVariant(seg(p + dir * kGap, q + dir * kOvershoot));
        }
        lines << QVariant(seg(q1, q2));
        const V along = (q2 - q1).unit();
        arrows << QVariant(arrow(q1, along * -1)) << QVariant(arrow(q2, along));
        // Text beside the line, on the side away from the part
        const V side = (place - p1).dot(n) < 0 ? n * -1 : n;
        const V tp = (q1 + q2) * 0.5 + (side.len() > 0 ? side : n) * (size * 0.75);
        text["x"] = tp.x; text["y"] = tp.y;
        text["angle"] = readable(qRadiansToDegrees(std::atan2(along.y, along.x)));
        if ((q2 - q1).len() < 2 * kArrow + 1) {
            // No room inside: arrows point in from outside
            arrows.clear();
            arrows << QVariant(arrow(q1, along)) << QVariant(arrow(q2, along * -1));
            lines << QVariant(seg(q1 - along * (kArrow + 2), q1)) << QVariant(seg(q2, q2 + along * (kArrow + 2)));
        }
    }
    return {{"lines", lines}, {"arcs", arcs}, {"arrows", arrows}, {"text", text}, {"arrowSize", kArrow}};
}

double distanceTo(const QVariantMap &ann, double x, double y) {
    const QVariantMap g = geometry(ann);
    const V p{x, y};
    double best = 1e300;
    for (const QVariant &l : g.value("lines").toList()) {
        const QVariantList s = l.toList();
        const V a{s[0].toDouble(), s[1].toDouble()}, b{s[2].toDouble(), s[3].toDouble()};
        const V ab = b - a;
        const double t = ab.dot(ab) > 0 ? std::clamp((p - a).dot(ab) / ab.dot(ab), 0.0, 1.0) : 0.0;
        best = std::min(best, (p - (a + ab * t)).len());
    }
    for (const QVariant &c : g.value("arcs").toList()) {
        const QVariantList s = c.toList();
        best = std::min(best, std::fabs((p - V{s[0].toDouble(), s[1].toDouble()}).len() - s[2].toDouble()));
    }
    const QVariantMap t = g.value("text").toMap();
    const double half = t.value("str").toString().size() * t.value("size").toDouble() * 0.35;
    best = std::min(best, std::max(0.0, (p - V{t.value("x").toDouble(), t.value("y").toDouble()}).len() - half));
    return best;
}

namespace {

constexpr const char *kPointKeys[][2] = {{"p1x", "p1y"}, {"p2x", "p2y"}, {"px", "py"}, {"x", "y"}, {"cx", "cy"},
                                          {"vx", "vy"}, {"x1", "y1"}, {"x2", "y2"}};
constexpr const char *kDirKeys[][2] = {{"d1x", "d1y"}, {"d2x", "d2y"}};

// Map every point, direction and radius of a dimension or pick between view and sheet space
QVariantMap mapped(QVariantMap m, const ViewFrame &f, bool toSheet) {
    for (const auto &k : kPointKeys) {
        if (!m.contains(k[0]) || !m.contains(k[1])) continue;
        const double x = m.value(k[0]).toDouble(), y = m.value(k[1]).toDouble();
        if (toSheet) { m[k[0]] = f.ox + x * f.s; m[k[1]] = f.oy - y * f.s; }
        else { m[k[0]] = (x - f.ox) / f.s; m[k[1]] = -(y - f.oy) / f.s; }
    }
    for (const auto &k : kDirKeys)
        if (m.contains(k[1])) m[k[1]] = -m.value(k[1]).toDouble();
    if (m.contains("r")) m["r"] = toSheet ? m.value("r").toDouble() * f.s : m.value("r").toDouble() / f.s;
    return m;
}

}  // namespace

QVariantMap toView(const QVariantMap &sheetAnn, const QVariantList &sheetPicks, const ViewFrame &f) {
    QVariantMap v = mapped(sheetAnn, f, false);
    v["space"] = QStringLiteral("view");
    v["viewScale"] = 1.0;   // values are model mm already
    QVariantList picks;
    for (const QVariant &p : sheetPicks) {
        QVariantMap m = mapped(p.toMap(), f, false);
        m.remove("viewScale");
        picks << m;
    }
    v["picks"] = picks;
    return v;
}

QVariantMap toSheet(const QVariantMap &viewAnn, const ViewFrame &f) {
    QVariantMap s = mapped(viewAnn, f, true);
    s["viewScale"] = f.s;
    s.remove("picks");
    return s;
}

bool reattach(QVariantMap &viewAnn, const QVariantList &edges) {
    const QVariantList picks = viewAnn.value("picks").toList();
    if (picks.isEmpty() || edges.isEmpty()) return false;
    double span = 1;
    for (const QVariant &v : edges) {
        const QVariantMap e = v.toMap();
        span = std::max({span, std::fabs(e.value("x1").toDouble()), std::fabs(e.value("y1").toDouble()),
                         std::fabs(e.value("x2").toDouble()), std::fabs(e.value("y2").toDouble())});
    }
    const double tol = std::max(0.5, span * 0.05);   // how far an edge may move and still be "the same"
    QVariantList found;
    for (const QVariant &pv : picks) {
        const QVariantMap p = pv.toMap();
        const QString kind = p.value("kind").toString();
        QVariantMap best;
        double bestD = tol;
        for (const QVariant &ev : edges) {
            const QVariantMap e = ev.toMap();
            const QString type = e.value("type").toString();
            const V a = at(e, "x1", "y1"), b = at(e, "x2", "y2");
            if (kind == "point") {
                QList<V> pts{a, b};
                if (type == "circle") pts << at(e, "cx", "cy");
                for (const V q : pts) {
                    const double d = (q - at(p, "x", "y")).len();
                    if (d < bestD) { bestD = d; best = {{"kind", "point"}, {"x", q.x}, {"y", q.y}}; }
                }
            } else if (kind == "circle" && type == "circle") {
                const double d = (at(e, "cx", "cy") - at(p, "cx", "cy")).len();
                if (d < bestD) {
                    bestD = d;
                    best = {{"kind", "circle"}, {"cx", e.value("cx")}, {"cy", e.value("cy")}, {"r", e.value("r")},
                            {"full", e.value("full")}, {"x1", a.x}, {"y1", a.y}, {"x2", b.x}, {"y2", b.y}};
                }
            } else if (kind == "line" && type != "circle") {
                const V pa = at(p, "x1", "y1"), pb = at(p, "x2", "y2");
                if (std::fabs((pb - pa).unit().cross((b - a).unit())) > 0.035) continue;   // not parallel (2 deg)
                const V mid = (pa + pb) * 0.5;
                const V ab = b - a;
                const double t = std::clamp((mid - a).dot(ab) / std::max(1e-12, ab.dot(ab)), 0.0, 1.0);
                const double d = (mid - (a + ab * t)).len() + std::fabs((b - a).len() - (pb - pa).len()) * 0.25;
                if (d < bestD) { bestD = d; best = {{"kind", "line"}, {"x1", a.x}, {"y1", a.y}, {"x2", b.x}, {"y2", b.y}}; }
            }
        }
        if (best.isEmpty()) return false;
        found << best;
    }
    const QVariantMap rebuilt = build(viewAnn.value("tool", "dimension").toString(), found.value(0).toMap(),
                                      found.value(1).toMap(), viewAnn.value("px").toDouble(),
                                      viewAnn.value("py").toDouble(), 1.0);
    if (rebuilt.contains("error")) return false;
    for (auto it = rebuilt.cbegin(); it != rebuilt.cend(); ++it)
        if (it.key() != "text" && it.key() != "decimals" && it.key() != "fontSize" && it.key() != "view")
            viewAnn[it.key()] = it.value();
    viewAnn["picks"] = found;
    viewAnn["detached"] = false;
    return true;
}

}  // namespace DrawingDimension
