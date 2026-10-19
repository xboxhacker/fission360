// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#pragma once
#include "FissionFeature.h"
#include <QList>
#include <QVector>
#include <TopoDS_Face.hxx>
#include <TopoDS_Wire.hxx>
#include <TopoDS_Compound.hxx>
#include <Geom2d_Curve.hxx>
#include <QSet>

class SketchSolver;

// A 2D sketch on a plane. props:
//   plane    : plane reference (see RebuildContext)
//   entities : list of maps; coordinates are sketch-plane (u, v) values or expressions
//     {type:"line", x1,y1,x2,y2}            {type:"circle", cx,cy,r}
//     {type:"arc", cx,cy,r,a1,a2}  (deg)    {type:"arc3", x1,y1,xm,ym,x2,y2}
//     {type:"rectangle", x,y,w,h}           {type:"polygon", cx,cy,r,sides}
//     {type:"polyline", points:[[x,y]..], closed}
//     {type:"slot", x1,y1,x2,y2,w}          {type:"ellipse", cx,cy,rx,ry}
//     {type:"spline", points:[[x,y]..]}     {type:"point", x,y}
//   Any entity may carry construction:true to exclude it from profiles.
//   constraints : geometric constraints and driving dimensions (format in SketchSolver.h).
//                 Every rebuild solves them before the curves are built.
class FissionSketch : public FissionFeature {
    Q_OBJECT
    Q_PROPERTY(int entityCount READ entityCount NOTIFY changed)
    Q_PROPERTY(int constraintCount READ constraintCount NOTIFY changed)
    Q_PROPERTY(int profileCount READ profileCount NOTIFY statusChanged)

public:
    explicit FissionSketch(QObject *parent = nullptr);
    QString type() const override { return QStringLiteral("Sketch"); }
    QVariantMap defaults() const override;

    Q_INVOKABLE int addLine(const QVariant &x1, const QVariant &y1, const QVariant &x2, const QVariant &y2);
    Q_INVOKABLE int addCircle(const QVariant &cx, const QVariant &cy, const QVariant &r);
    Q_INVOKABLE int addArc(const QVariant &cx, const QVariant &cy, const QVariant &r,
                           const QVariant &startDeg, const QVariant &endDeg);
    Q_INVOKABLE int addArc3(const QVariant &x1, const QVariant &y1, const QVariant &xm, const QVariant &ym,
                            const QVariant &x2, const QVariant &y2);
    Q_INVOKABLE int addRectangle(const QVariant &x, const QVariant &y, const QVariant &w, const QVariant &h);
    Q_INVOKABLE int addPolygon(const QVariant &cx, const QVariant &cy, const QVariant &r, int sides);
    Q_INVOKABLE int addPolyline(const QVariantList &points, bool closed);
    Q_INVOKABLE int addSlot(const QVariant &x1, const QVariant &y1, const QVariant &x2, const QVariant &y2,
                            const QVariant &width);
    Q_INVOKABLE int addEllipse(const QVariant &cx, const QVariant &cy, const QVariant &rx, const QVariant &ry);
    Q_INVOKABLE int addSpline(const QVariantList &points);
    Q_INVOKABLE int addPoint(const QVariant &x, const QVariant &y);
    Q_INVOKABLE int addEntity(const QVariantMap &entity);
    Q_INVOKABLE void setEntity(int index, const QVariantMap &entity);
    Q_INVOKABLE QVariantMap entity(int index) const;
    Q_INVOKABLE void removeEntity(int index);
    Q_INVOKABLE void setConstruction(int index, bool construction);
    int entityCount() const { return list("entities").size(); }

    // Four lines held together by coincident + horizontal / vertical constraints; returns the first line
    Q_INVOKABLE int addRectangleLines(double x, double y, double w, double h);
    // Three-point arc stored as a counter-clockwise center arc (so it can be constrained); -1 if collinear
    Q_INVOKABLE int addArcThroughPoints(double x1, double y1, double xm, double ym, double x2, double y2);
    // Point role of an entity at (u, v) in the stored geometry, or "" if none is there
    Q_INVOKABLE QString roleAt(int entity, double u, double v) const;

    // Constraint edits solve immediately; on failure nothing changes. Return {ok, error, index}.
    Q_INVOKABLE QVariantMap addConstraint(const QVariantMap &constraint);
    Q_INVOKABLE QVariantMap setConstraintValue(int index, const QString &expression);
    Q_INVOKABLE void removeConstraint(int index);
    Q_INVOKABLE QVariantMap constraint(int index) const;
    int constraintCount() const { return list("constraints").size(); }

    // Dimension label placement, stored as the constraint's "offset": linear = distance of the dimension
    // line from the measured points (sketch units), radial = label direction (deg), angular = arc radius
    Q_INVOKABLE void setDimensionOffset(int index, double offset);

    // --- editing tools (SketchEdit.cpp); geometry comes from the last rebuild -------------------------
    // Moves geometry while every constraint holds. targets: [{e, p, u, v}] puts point (e, p) at (u, v);
    // [{e, u, v}] puts (u, v) on circle / arc e. Returns false (and changes nothing) if impossible.
    Q_INVOKABLE bool dragTo(const QVariantList &targets);
    // Trim: removes the piece of line / circle / arc `entity` between the intersections around (u, v),
    // or the whole curve when nothing crosses it. Extend: lengthens the line / arc end nearest (u, v) to
    // the next curve it meets. Both return false when there is nothing to do.
    Q_INVOKABLE bool trimAt(int entity, double u, double v);
    Q_INVOKABLE bool extendAt(int entity, double u, double v);
    // Curves joined end to end with `entity` (including it)
    Q_INVOKABLE QVariantList chainOf(int entity) const;
    // Offset copies of the curves, `distance` toward the side containing (u, v); returns the new indices
    Q_INVOKABLE QVariantList offsetEntities(const QVariantList &entities, double distance, double u, double v);
    // Move / Copy of sketch curves: rotates by `angle` degrees about (pu, pv), then shifts by (du, dv).
    // Constraints tying moved curves to curves left behind (or to a direction, when rotating) are dropped.
    // copy adds moved copies instead. Returns {ok, error, entities (the moved or added indices)}
    Q_INVOKABLE QVariantMap moveEntities(const QVariantList &entities, double du, double dv, double angle,
                                         double pu, double pv, bool copy);
    // Rounds the corner between two lines with a tangent arc (tangent, coincident and radius constraints).
    // Returns {ok, error, index (arc)}
    Q_INVOKABLE QVariantMap filletCorner(int line1, int line2, const QString &radius);
    // Copies body edges ({kind:"edge"}) or a face's boundary ({kind:"face"}) onto the sketch plane as
    // fixed, "projected" curves. Returns the number of curves added.
    Q_INVOKABLE int projectRef(const QVariantMap &ref);
    // Writes the sketch curves as an ASCII DXF (R12) in sketch coordinates; construction curves go on
    // layer CONSTRUCTION
    bool exportDxf(const QString &path, QString *error = nullptr) const;
    Q_INVOKABLE bool exportDxfFile(const QString &path) const { return exportDxf(path); }

    // Results of the last solve: [{entity, role, u, v, construction}] snap/pick points (lines add "mid"),
    // annotation data for every constraint, remaining degrees of freedom
    Q_INVOKABLE QVariantList handles() const { return m_handles; }
    Q_INVOKABLE QVariantList annotations() const { return m_annotations; }
    Q_INVOKABLE int degreesOfFreedom() const { return m_dof; }
    Q_INVOKABLE bool entityFullyConstrained(int entity) const { return m_fully.value(entity, false); }

    // Results of the last rebuild
    const gp_Ax3 &plane() const { return m_plane; }
    int profileCount() const { return m_profiles.size(); }
    const QList<TopoDS_Face> &profiles() const { return m_profiles; }
    const QList<int> &defaultProfiles() const { return m_defaultProfiles; }
    const QList<QList<TopoDS_Edge>> &entityEdges() const { return m_entityEdges; }
    bool lineAxis(int entity, gp_Ax1 &axis) const;
    // All non-construction curves chained into one wire (sweep paths)
    TopoDS_Wire pathWire(QString &error) const;
    TopoDS_Shape overlayShape() const override { return m_curves; }

protected:
    bool compute(RebuildContext &ctx) override;
    void resetResult() override;

private:
    // Sketch-plane 2D curves of every entity's edges (from the last rebuild)
    QList<QList<Handle(Geom2d_Curve)>> curves2d() const;
    // Drops constraints whose point references (e, role) moved by more than tol between the two lists
    QVariantList constraintsAfterEdit(const QVariantList &before, const QVariantList &after,
                                      const QSet<int> &edited, const QStringList &dropWholeCurveTypes) const;
    bool buildEntity(RebuildContext &ctx, const QVariantMap &e, QList<TopoDS_Edge> &edges, QString &error);
    void computeProfiles(const QList<TopoDS_Edge> &edges);
    void collectSolverResults(const SketchSolver &solver, const QVariantList &entities);
    QVariantMap applyConstraints(const QVariantList &entities, const QVariantList &constraints);
    double evaluate(const QString &expr, QString *error) const;

    QVariantList m_handles;
    QVariantList m_annotations;
    QVector<bool> m_fully;
    int m_dof = 0;

    gp_Ax3 m_plane;
    QList<QList<TopoDS_Edge>> m_entityEdges;
    QList<TopoDS_Edge> m_profileEdges;
    TopoDS_Compound m_curves;
    QList<TopoDS_Face> m_profiles;
    QList<int> m_defaultProfiles;
};
