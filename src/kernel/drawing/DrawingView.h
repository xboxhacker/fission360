// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#pragma once
#include <QObject>
#include <QString>
#include <QPointF>
#include <QVariantList>
#include <QVariantMap>
#include <QQmlEngine>

// A 2D projected view of a 3D design on a drawing sheet.
// Fusion API equivalent: adsk::drawing::View
class DrawingView : public QObject {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QString id READ id CONSTANT)
    Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)
    Q_PROPERTY(QString type READ type CONSTANT)        // "base" | "projected" | "section" | "detail" | "auxiliary"
    Q_PROPERTY(QPointF position READ position WRITE setPosition NOTIFY positionChanged)
    Q_PROPERTY(double scale READ scale WRITE setScale NOTIFY scaleChanged)
    Q_PROPERTY(double rotation READ rotation WRITE setRotation NOTIFY rotationChanged)
    Q_PROPERTY(double width READ width NOTIFY geometryChanged)
    Q_PROPERTY(double height READ height NOTIFY geometryChanged)
    Q_PROPERTY(QString displayStyle READ displayStyle WRITE setDisplayStyle NOTIFY styleChanged)
    Q_PROPERTY(bool showHiddenLines READ showHiddenLines WRITE setShowHiddenLines NOTIFY hiddenLinesChanged)
    Q_PROPERTY(QString tangentEdgeHandling READ tangentEdgeHandling WRITE setTangentEdgeHandling NOTIFY tangentChanged)
    Q_PROPERTY(QString orientation READ orientation WRITE setOrientation NOTIFY orientationChanged)
    Q_PROPERTY(QString parentViewId READ parentViewId WRITE setParentViewId NOTIFY parentChanged)
    Q_PROPERTY(QString projectDirection READ projectDirection WRITE setProjectDirection NOTIFY parentChanged)
    Q_PROPERTY(double detailRadius READ detailRadius WRITE setDetailRadius NOTIFY detailChanged)
    Q_PROPERTY(QPointF detailCenter READ detailCenter WRITE setDetailCenter NOTIFY detailChanged)
    Q_PROPERTY(QVariantList visibleCurves READ visibleCurves NOTIFY curvesChanged)
    Q_PROPERTY(QVariantList hiddenCurves READ hiddenCurves NOTIFY curvesChanged)
    Q_PROPERTY(QVariantList sectionCurves READ sectionCurves NOTIFY curvesChanged)
    Q_PROPERTY(bool projected READ projected NOTIFY curvesChanged)

public:
    explicit DrawingView(const QString &id, const QString &type, QObject *parent = nullptr);

    QString id() const { return m_id; }
    QString name() const { return m_name; }
    void setName(const QString &name);
    QString type() const { return m_type; }

    QPointF position() const { return m_position; }
    void setPosition(const QPointF &pos);

    double scale() const { return m_scale; }
    void setScale(double scale);

    double rotation() const { return m_rotation; }
    void setRotation(double deg);

    double width() const { return m_width; }
    double height() const { return m_height; }

    QString displayStyle() const { return m_displayStyle; }
    void setDisplayStyle(const QString &style);

    bool showHiddenLines() const { return m_showHidden; }
    void setShowHiddenLines(bool show);

    QString tangentEdgeHandling() const { return m_tangentMode; }
    void setTangentEdgeHandling(const QString &mode);

    QString orientation() const { return m_orientation; }
    void setOrientation(const QString &orient);

    QString parentViewId() const { return m_parentId; }
    void setParentViewId(const QString &id);

    QString projectDirection() const { return m_projectDir; }
    void setProjectDirection(const QString &dir);

    double detailRadius() const { return m_detailRadius; }
    void setDetailRadius(double r);
    QPointF detailCenter() const { return m_detailCenter; }
    void setDetailCenter(const QPointF &c);

    QVariantList visibleCurves() const { return m_visible; }
    QVariantList hiddenCurves() const { return m_hidden; }
    QVariantList sectionCurves() const { return m_section; }
    bool projected() const { return m_projected; }

    void setCurves(const QVariantList &visible, const QVariantList &hidden, const QVariantList &section,
                   double widthMm, double heightMm);

signals:
    void nameChanged();
    void positionChanged();
    void scaleChanged();
    void rotationChanged();
    void geometryChanged();
    void styleChanged();
    void hiddenLinesChanged();
    void tangentChanged();
    void orientationChanged();
    void parentChanged();
    void detailChanged();
    void curvesChanged();

private:
    QString m_id;
    QString m_name;
    QString m_type;
    QPointF m_position;
    double m_scale = 1.0;
    double m_rotation = 0.0;
    double m_width = 80;
    double m_height = 60;
    QString m_displayStyle = QStringLiteral("shadedWithEdges");
    bool m_showHidden = false;
    QString m_tangentMode = QStringLiteral("visible");
    QString m_orientation = QStringLiteral("front");
    QString m_parentId;
    QString m_projectDir;
    double m_detailRadius = 25;
    QPointF m_detailCenter;
    QVariantList m_visible;
    QVariantList m_hidden;
    QVariantList m_section;
    bool m_projected = false;
};
