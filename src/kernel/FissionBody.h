// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#pragma once
#include <QObject>
#include <QColor>
#include <QHash>
#include <QVariantMap>
#include <TopoDS_Shape.hxx>

// A body produced by the last rebuild. Objects are reused across rebuilds (by position)
// so QML bindings and visibility survive parameter edits.
class FissionBody : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString name READ name NOTIFY shapeChanged)
    Q_PROPERTY(bool visible READ isVisible WRITE setVisible NOTIFY visibilityChanged)
    Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY visibilityChanged)
    Q_PROPERTY(double volume READ volume NOTIFY shapeChanged)
    Q_PROPERTY(double area READ area NOTIFY shapeChanged)
    Q_PROPERTY(int faceCount READ faceCount NOTIFY shapeChanged)
    Q_PROPERTY(int edgeCount READ edgeCount NOTIFY shapeChanged)
    Q_PROPERTY(bool isMesh READ isMesh NOTIFY shapeChanged)
    Q_PROPERTY(double opacity READ opacity WRITE setOpacity NOTIFY visibilityChanged)
    // Appearance library id (e.g. "aluminum"); empty = custom colour / PBR values below
    Q_PROPERTY(QString appearanceId READ appearanceId WRITE setAppearanceId NOTIFY appearanceChanged)
    Q_PROPERTY(double roughness READ roughness WRITE setRoughness NOTIFY appearanceChanged)
    Q_PROPERTY(double metalness READ metalness WRITE setMetalness NOTIFY appearanceChanged)
    // Optional PBR maps (local paths from RenderAssetStore material download)
    Q_PROPERTY(QString texturePath READ texturePath WRITE setTexturePath NOTIFY appearanceChanged)
    Q_PROPERTY(QString roughnessPath READ roughnessPath WRITE setRoughnessPath NOTIFY appearanceChanged)
    Q_PROPERTY(QString metalnessPath READ metalnessPath WRITE setMetalnessPath NOTIFY appearanceChanged)
    Q_PROPERTY(QString normalPath READ normalPath WRITE setNormalPath NOTIFY appearanceChanged)
    // faceIndex → override map (subset of appearance fields); empty = use body defaults
    Q_PROPERTY(QVariantMap faceAppearances READ faceAppearancesMap NOTIFY appearanceChanged)
    // Mesh bodies: nodes, triangles, groups, openEdges, nonManifoldEdges, pieces, closed, oriented, area,
    // volume, minEdge, maxEdge (computed on first use)
    Q_PROPERTY(QVariantMap meshStats READ meshStats NOTIFY shapeChanged)
    // Id of the timeline feature that created the body
    Q_PROPERTY(QString sourceFeature READ sourceFeature NOTIFY shapeChanged)
    // Id of the timeline feature that last changed the body (fillet, chamfer, press/pull, …)
    Q_PROPERTY(QString editedBy READ editedBy NOTIFY shapeChanged)

public:
    explicit FissionBody(QObject *parent = nullptr) : QObject(parent) {}

    QString name() const { return m_name; }
    QString sourceFeature() const { return m_sourceFeature; }
    void setSourceFeature(const QString &id) { m_sourceFeature = id; }
    QString editedBy() const { return m_editedBy; }
    void setEditedBy(const QString &id) { m_editedBy = id; }
    const TopoDS_Shape &shape() const { return m_shape; }
    void setShape(const QString &name, const TopoDS_Shape &shape);

    bool isVisible() const { return m_visible; }
    void setVisible(bool v);
    QColor color() const { return m_color; }
    void setColor(const QColor &c);
    double opacity() const { return m_opacity; }
    void setOpacity(double o);
    QString appearanceId() const { return m_appearanceId; }
    void setAppearanceId(const QString &id);
    // Looks up the library entry and copies colour / PBR onto this body
    Q_INVOKABLE void applyAppearance(const QString &id);
    double roughness() const { return m_roughness; }
    void setRoughness(double r);
    double metalness() const { return m_metalness; }
    void setMetalness(double m);
    QString texturePath() const { return m_texturePath; }
    void setTexturePath(const QString &path);
    QString roughnessPath() const { return m_roughnessPath; }
    void setRoughnessPath(const QString &path);
    QString metalnessPath() const { return m_metalnessPath; }
    void setMetalnessPath(const QString &path);
    QString normalPath() const { return m_normalPath; }
    void setNormalPath(const QString &path);

    // Per-face overrides (Fusion-style face appearance). faceIndex is 0-based GeomUtils::faceMap order.
    Q_INVOKABLE void applyAppearanceToFace(int faceIndex, const QString &id);
    // Merge PBR map paths into an existing face override (creates one if needed)
    Q_INVOKABLE void setFaceMaps(int faceIndex, const QString &albedo, const QString &roughness,
                                 const QString &metalness, const QString &normal);
    Q_INVOKABLE void clearFaceAppearance(int faceIndex);
    Q_INVOKABLE void clearAllFaceAppearances();
    Q_INVOKABLE QVariantMap faceAppearance(int faceIndex) const;
    QVariantMap faceAppearancesMap() const;
    void setFaceAppearancesFromJson(const QVariantMap &map);
    QHash<int, QVariantMap> faceAppearances() const { return m_faceAppearances; }

    // Effective look for a face (override merged onto body defaults)
    struct EffectiveLook {
        QColor color;
        double roughness = 0.55;
        double metalness = 0.0;
        double opacity = 1.0;
        QString texturePath;
        QString roughnessPath;
        QString metalnessPath;
        QString normalPath;
    };
    EffectiveLook effectiveLook(int faceIndex) const;

    bool isMesh() const;
    QVariantMap meshStats() const;

    double volume() const;
    double area() const;
    int faceCount() const;
    int edgeCount() const;

    // Mass properties for the properties palette, computed once per shape
    struct Geometry {
        double volume = 0;
        double centroid[3] = {0, 0, 0};
        double box[6] = {0, 0, 0, 0, 0, 0};
        bool hasBox = false;
    };
    const Geometry &geometry() const;
    bool box(double out[6]) const;   // false for an empty shape

signals:
    void shapeChanged();
    void visibilityChanged();
    void appearanceChanged();

private:
    QString m_name;
    QString m_sourceFeature;
    QString m_editedBy;
    TopoDS_Shape m_shape;
    bool m_visible = true;
    QColor m_color = QColor(170, 178, 189);
    double m_opacity = 1.0;
    QString m_appearanceId;
    double m_roughness = 0.55;
    double m_metalness = 0.0;
    QString m_texturePath;
    QString m_roughnessPath;
    QString m_metalnessPath;
    QString m_normalPath;
    QHash<int, QVariantMap> m_faceAppearances;
    void clearCaches();
    mutable double m_volume = -1, m_area = -1;
    mutable int m_faceCount = -1, m_edgeCount = -1, m_isMesh = -1, m_boxState = -1;
    mutable double m_box[6] = {0, 0, 0, 0, 0, 0};
    mutable QVariantMap m_meshStats;
    mutable Geometry m_geometry;
    mutable bool m_hasGeometry = false;
};
