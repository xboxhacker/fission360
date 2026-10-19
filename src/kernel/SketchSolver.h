// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#pragma once
#include <QHash>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>
#include <functional>

// Geometric constraint solver for the entities of one sketch.
//
// Every numeric entity field (plain number) is an unknown; fields holding an expression, and all
// fields of an entity with a "fix" constraint, are held constant. The solver moves the unknowns as
// little as possible (minimum-norm Gauss-Newton with Levenberg damping) until every constraint holds.
//
// Constraint maps, stored in the sketch's "constraints" list:
//   {type, a, b, c, value, sign, inner}
//   a / b / c reference geometry: {e: entity} for a whole curve, or {e, p: role} for a point.
//   Point roles: line start|end, circle/ellipse/polygon center, arc center|start|end,
//                arc3 start|mid|end, slot start|end, point pos, spline/polyline p0, p1, ...
//                {e: -1, p: "origin"} is the fixed sketch origin.
//
//   geometric : coincident (pt, pt | curve), horizontal / vertical (line | pt, pt), parallel,
//               perpendicular, collinear (line, line), tangent (line | circle, circle | line),
//               equal (line, line | circle, circle), concentric (circle, circle),
//               midpoint (pt, line), symmetric (pt, pt, line), fix (any)
//   dimensions: distance (pt, pt; orient "aligned" | "horizontal" | "vertical"), pointline (pt, line),
//               radius, diameter (circle | arc), angle (line, line). value = expression (mm / deg).
//   sign / inner record which of the mirror-image solutions was chosen when the constraint was made.
//   dragto    : (pt) moves the point to (u, v); (circle | arc) puts (u, v) on the curve. Never stored —
//               FissionSketch::dragTo adds them for one solve while geometry is dragged.
// Entities with fixed:true (projected geometry) are held constant like a fix constraint.
class SketchSolver {
public:
    using Evaluator = std::function<double(const QString &expr, QString *error)>;

    struct Point { double u = 0, v = 0; };

    bool load(const QVariantList &entities, const QVariantList &constraints, const Evaluator &eval, QString &error);
    bool solve(QString &error);

    // Entities with every scalar replaced by its current value
    QVariantList entities() const;
    // `original` with the unknowns replaced by their solved values (expressions are kept)
    QVariantList storedEntities(const QVariantList &original) const;

    int degreesOfFreedom() const { return m_dof; }
    bool entityFullyConstrained(int entity) const { return m_fully.value(entity, false); }

    bool point(int entity, const QString &role, Point &out) const;
    bool lineEnds(int entity, Point &a, Point &b) const;
    bool circle(int entity, Point &center, double &radius) const;
    QString entityType(int entity) const;
    int entityCount() const { return m_ents.size(); }

    // Point roles of an entity type (for picking and snapping)
    static QStringList roles(const QVariantMap &entity);
    static bool isDimension(const QString &type);

    // Current value of a dimension (mm or deg) and the signs that describe the current configuration
    double measure(const QVariantMap &constraint) const;
    QVariantMap configuration(const QVariantMap &constraint) const;

private:
    struct Ent {
        QVariantMap src;
        QString type;
        QStringList keys;          // "x1", "points/2/0", ...
        QVector<double> vals;
        QVector<bool> free;
    };
    struct Residual {
        int count;
        std::function<void(double *)> f;
    };

    double val(int e, const QString &key) const;
    bool addResidual(const QVariantMap &c, QString &error);
    void residuals(QVector<double> &out) const;
    void jacobian(QVector<QVector<double>> &J);
    void setX(const QVector<double> &x);
    QVector<double> x() const;
    void analyze();

    QVector<Ent> m_ents;
    QVector<QPair<int, int>> m_vars;     // (entity, slot)
    QVector<Residual> m_res;
    int m_resCount = 0;
    int m_dof = 0;
    QHash<int, bool> m_fully;
};
