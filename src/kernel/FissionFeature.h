// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#pragma once
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVariantMap>
#include <QJsonObject>
#include <QHash>
#include <TopoDS_Shape.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <gp_Ax1.hxx>
#include <gp_Ax3.hxx>
#include <gp_Pnt.hxx>
#include "FissionTypes.h"

class FissionDocument;
class FissionFeature;

struct BodyState {
    QString name;
    TopoDS_Shape shape;
    QString sourceFeature;   // id of the feature that created the body
    QString editedBy;        // id of the feature that last changed the body
};

// State threaded through the timeline while it is replayed.
//
// References used in feature properties:
//   planes : "XY" | "XZ" | "YZ" | "feature:<id>" (construction plane or sketch) | {kind:"face", body, index, x,y,z}
//   axes   : "X" | "Y" | "Z" | "feature:<id>" (construction axis) | "sketchline:<sketchId>:<entity>"
//            | {kind:"edge", body, index, x,y,z}
//   edges  : {kind:"edge", body, index, x,y,z}   (x,y,z = midpoint hint for re-matching)
//   faces  : {kind:"face", body, index, x,y,z}   (x,y,z = centroid hint)
class RebuildContext {
public:
    explicit RebuildContext(FissionDocument *doc) : m_doc(doc) {}

    FissionDocument *document() const { return m_doc; }
    QList<BodyState> bodies;
    int featureIndex = 0;

    // Features earlier in the timeline (and not suppressed) by id
    void registerFeature(FissionFeature *f);
    FissionFeature *feature(const QString &id) const { return m_features.value(id, nullptr); }
    QList<FissionFeature *> features() const { return m_features.values(); }

    double evaluate(const QString &expr, QString *error) const;

    bool resolvePlane(const QVariant &ref, gp_Ax3 &out, QString &error) const;
    bool resolveAxis(const QVariant &ref, gp_Ax1 &out, QString &error) const;
    bool resolvePoint(const QVariant &ref, gp_Pnt &out, QString &error) const;
    bool resolveEdge(const QVariant &ref, int &body, TopoDS_Edge &out, QString &error) const;
    bool resolveFace(const QVariant &ref, int &body, TopoDS_Face &out, QString &error) const;

    // Faces of a sketch's profiles. profiles = list of indices, empty = default selection.
    bool resolveProfiles(const QString &sketchId, const QVariantList &profiles,
                         TopoDS_Shape &out, gp_Ax3 &plane, QString &error) const;
    // Coplanar flat body faces used directly as a profile; plane normal points out of the first face
    bool resolveFaceProfiles(const QVariantList &faceRefs, TopoDS_Shape &out, gp_Ax3 &plane, QString &error) const;
    static bool isFaceProfileList(const QVariantList &profiles) {
        return !profiles.isEmpty() && profiles.first().typeId() == QMetaType::QVariantMap;
    }

    double modelSize() const;   // diagonal of all bodies' bounding box (>= 100 mm)

    // Applies a feature's tool shape to the bodies. targets = body indices, empty = automatic.
    bool applyOperation(const TopoDS_Shape &tool, FeatureOperation op, const QVariantList &targets,
                        const QString &featureId, QString &error);

    // Adds every solid of a shape as a separate new body
    void addBodies(const TopoDS_Shape &shape, const QString &featureId);
    QString nextBodyName() { return QStringLiteral("Body%1").arg(++m_bodyCounter); }

private:
    FissionDocument *m_doc;
    QHash<QString, FissionFeature *> m_features;
    int m_bodyCounter = 0;
};

// Base class for everything that lives in the timeline.
// All inputs are stored in a property map so features serialize, preview and edit uniformly;
// numeric inputs may be expressions referencing user parameters.
class FissionFeature : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString id READ id CONSTANT)
    Q_PROPERTY(QString type READ type CONSTANT)
    Q_PROPERTY(QString name READ name WRITE setName NOTIFY changed)
    Q_PROPERTY(bool suppressed READ isSuppressed WRITE setSuppressed NOTIFY changed)
    Q_PROPERTY(bool visible READ isVisible WRITE setVisible NOTIFY visibilityChanged)
    Q_PROPERTY(QVariantMap props READ props WRITE setProps NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY statusChanged)
    Q_PROPERTY(bool healthy READ isHealthy NOTIFY statusChanged)
    // In-viewport drag handle from the last rebuild:
    // {origin: vector3d, direction: vector3d, key, value, angle?: {pivot, up, out, height, key, value}}
    // or empty when the feature has none
    Q_PROPERTY(QVariantMap manipulator READ manipulator NOTIFY statusChanged)

public:
    explicit FissionFeature(QObject *parent = nullptr);

    QString id() const { return m_id; }
    virtual QString type() const = 0;
    QString name() const { return m_name; }
    void setName(const QString &name);
    bool isSuppressed() const { return m_suppressed; }
    void setSuppressed(bool s);
    bool isVisible() const { return m_visible; }
    void setVisible(bool v);

    QVariantMap props() const { return m_props; }
    void setProps(const QVariantMap &props);
    Q_INVOKABLE QVariant get(const QString &key) const { return m_props.value(key); }
    Q_INVOKABLE void set(const QString &key, const QVariant &value);

    QString error() const { return m_error; }
    bool isHealthy() const { return m_error.isEmpty(); }
    QVariantMap manipulator() const { return m_manipulator; }

    // Keys whose values are numeric expressions (shown in the Parameters dialog)
    Q_INVOKABLE virtual QStringList expressionKeys() const { return {}; }
    virtual QVariantMap defaults() const { return {}; }

    // Replays the feature. Returns false and sets error() on failure.
    bool run(RebuildContext &ctx);
    // Unique across all features; changes whenever the feature does, so rebuilds can reuse earlier results
    quint64 revision() const { return m_revision; }
    void touch();

    // Geometry drawn as an overlay (sketch curves, construction planes); null if none
    virtual TopoDS_Shape overlayShape() const { return TopoDS_Shape(); }
    virtual bool isConstruction() const { return false; }

    QJsonObject toJson() const;
    static FissionFeature *create(const QString &type, QObject *parent = nullptr);
    static FissionFeature *fromJson(const QJsonObject &obj, QObject *parent = nullptr);
    static QStringList types();

signals:
    void changed();
    void statusChanged();
    void visibilityChanged();

protected:
    virtual bool compute(RebuildContext &ctx) = 0;
    virtual void resetResult() {}

    // Typed property helpers
    QString str(const QString &key, const QString &def = QString()) const;
    bool flag(const QString &key, bool def = false) const;
    int integer(const QString &key, int def = 0) const;
    QVariantList list(const QString &key) const { return m_props.value(key).toList(); }
    // Evaluates a numeric/expression property; on failure records the error and returns false
    bool number(RebuildContext &ctx, const QString &key, double &out);
    FeatureOperation operation() const;

    bool fail(const QString &message) { m_error = message; return false; }
    // value = current numeric value of `key`, measured from origin along direction
    void setManipulator(const gp_Pnt &origin, const gp_Dir &direction, const QString &key, double value);
    // Adds an angle knob to the manipulator (call after setManipulator): a side line from `pivot` along `up`
    // for `height`, leaning against `out` by `degrees` (positive = toward the inside)
    void setAngleManipulator(const gp_Pnt &pivot, const gp_Dir &up, const gp_Dir &out, double height,
                             const QString &key, double degrees);
    // Adds a point handle (call after setManipulator) dragged in the plane through `origin` with `normal`;
    // the dropped point is written to `key` as [x, y, z]. snaps = [[x, y, z, label], ...]
    void setMoveManipulator(const gp_Pnt &origin, const gp_Dir &normal, const QString &key, const QVariantList &snaps);

private:
    // Fusion import: props snapshot (encoded BRep after this step) / snapshotStepB64 / snapshotEmpty replace
    // the bodies; snapshotBodies [{name, box [xmin, ymin, zmin, xmax, ymax, zmax] mm}] names them
    bool applyFusionSnapshot(RebuildContext &ctx);
    TopoDS_Shape m_snapshotShape;
    QStringList m_snapshotNames;   // per solid, from the STEP itself
    quint64 m_snapshotRevision = ~quint64(0);
    QString m_id;
    QString m_name;
    bool m_suppressed = false;
    bool m_visible = true;
    QVariantMap m_props;
    QString m_error;
    QVariantMap m_manipulator;
    quint64 m_revision = 0;
    friend class FissionDocument;
};
