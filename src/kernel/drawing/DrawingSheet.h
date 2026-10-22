// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#pragma once
#include <QObject>
#include <QString>
#include <QList>
#include <QJsonObject>
#include <QVariantMap>
#include <QVariantList>
#include <QQmlEngine>
#include "DrawingView.h"
#include "DrawingSketch.h"

// A single sheet (page) in a drawing document.
// Fusion API equivalent: adsk::drawing::Sheet
class DrawingSheet : public QObject {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QString id READ id CONSTANT)
    Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)
    Q_PROPERTY(QString size READ size CONSTANT)
    Q_PROPERTY(double width READ width CONSTANT)
    Q_PROPERTY(double height READ height CONSTANT)
    Q_PROPERTY(QString orientation READ orientation CONSTANT)
    Q_PROPERTY(double scale READ scale WRITE setScale NOTIFY scaleChanged)
    Q_PROPERTY(int viewCount READ viewCount NOTIFY viewsChanged)
    Q_PROPERTY(QList<QObject *> views READ views NOTIFY viewsChanged)
    Q_PROPERTY(int sketchCount READ sketchCount NOTIFY sketchesChanged)
    Q_PROPERTY(int annotationCount READ annotationCount NOTIFY annotationsChanged)
    Q_PROPERTY(int tableCount READ tableCount NOTIFY tablesChanged)
    Q_PROPERTY(QString titleBlockName READ titleBlockName WRITE setTitleBlockName NOTIFY titleBlockChanged)
    Q_PROPERTY(QString title READ title WRITE setTitle NOTIFY titleBlockChanged)
    Q_PROPERTY(QString drawnBy READ drawnBy WRITE setDrawnBy NOTIFY titleBlockChanged)
    Q_PROPERTY(QString revision READ revision WRITE setRevision NOTIFY titleBlockChanged)
    Q_PROPERTY(QString material READ material WRITE setMaterial NOTIFY titleBlockChanged)
    Q_PROPERTY(QString date READ date WRITE setDate NOTIFY titleBlockChanged)
    Q_PROPERTY(QString partName READ partName WRITE setPartName NOTIFY titleBlockChanged)

public:
    explicit DrawingSheet(const QString &id, const QString &name,
                          const QString &size, double width, double height,
                          const QString &orientation, double scale,
                          QObject *parent = nullptr);

    QString id() const { return m_id; }
    QString name() const { return m_name; }
    void setName(const QString &name);
    QString size() const { return m_size; }
    double width() const { return m_width; }
    double height() const { return m_height; }
    QString orientation() const { return m_orientation; }
    double scale() const { return m_scale; }
    void setScale(double scale);

    int viewCount() const { return m_views.size(); }
    QList<QObject *> views() const;
    Q_INVOKABLE DrawingView *view(int index) const;
    Q_INVOKABLE DrawingView *viewById(const QString &id) const;
    Q_INVOKABLE DrawingView *addView(const QString &type, const QVariantMap &options);
    Q_INVOKABLE bool removeView(const QString &viewId);

    int sketchCount() const { return m_sketches.size(); }
    Q_INVOKABLE DrawingSketch *sketch(int index) const;
    Q_INVOKABLE DrawingSketch *addSketch(const QString &name);

    // Annotations: dimension | text | leader | holeNote | centerMark | image
    int annotationCount() const { return m_annotations.size(); }
    Q_INVOKABLE QVariantMap annotation(int index) const;
    Q_INVOKABLE QString addAnnotation(const QVariantMap &ann);
    // Merge fields into the annotation with this id. id and kind stay put. Returns false when id is missing.
    Q_INVOKABLE bool updateAnnotation(const QString &id, const QVariantMap &fields);
    Q_INVOKABLE bool removeAnnotation(const QString &id);
    Q_INVOKABLE QVariantList annotations() const { return m_annotations; }

    // Tables: partsList | general | bend | revision
    int tableCount() const { return m_tables.size(); }
    Q_INVOKABLE QVariantMap table(int index) const;
    Q_INVOKABLE QString addTable(const QVariantMap &table);
    Q_INVOKABLE bool removeTable(const QString &id);
    Q_INVOKABLE QVariantList tables() const { return m_tables; }

    QString titleBlockName() const { return m_titleBlock; }
    void setTitleBlockName(const QString &name);
    QString title() const { return m_title; }
    void setTitle(const QString &v);
    QString drawnBy() const { return m_drawnBy; }
    void setDrawnBy(const QString &v);
    QString revision() const { return m_revision; }
    void setRevision(const QString &v);
    QString material() const { return m_material; }
    void setMaterial(const QString &v);
    QString date() const { return m_date; }
    void setDate(const QString &v);
    QString partName() const { return m_partName; }
    void setPartName(const QString &v);

    QJsonObject toJson() const;
    static DrawingSheet *fromJson(const QJsonObject &obj, QObject *parent = nullptr);

signals:
    void nameChanged();
    void scaleChanged();
    void viewsChanged();
    void sketchesChanged();
    void annotationsChanged();
    void tablesChanged();
    void titleBlockChanged();

private:
    QString m_id;
    QString m_name;
    QString m_size;
    double m_width;
    double m_height;
    QString m_orientation;
    double m_scale;
    QString m_titleBlock = QStringLiteral("ISO");
    QString m_title;
    QString m_drawnBy;
    QString m_revision = QStringLiteral("A");
    QString m_material;
    QString m_date;
    QString m_partName;
    QList<DrawingView *> m_views;
    QList<DrawingSketch *> m_sketches;
    QVariantList m_annotations;
    QVariantList m_tables;
};
