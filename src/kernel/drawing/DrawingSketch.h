// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#pragma once
#include <QObject>
#include <QString>
#include <QList>
#include <QJsonObject>
#include <QVariantMap>
#include <QQmlEngine>

// A 2D sketch drawn directly on a drawing sheet (not a projection of the 3D model).
// Used for title block geometry, annotation lines, custom symbols.
// Fusion API equivalent: adsk::drawing::DrawingSketch
class DrawingSketch : public QObject {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QString id READ id CONSTANT)
    Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)
    Q_PROPERTY(int curveCount READ curveCount NOTIFY curvesChanged)
public:
    explicit DrawingSketch(const QString &id, const QString &name, QObject *parent = nullptr);

    QString id() const { return m_id; }
    QString name() const { return m_name; }
    void setName(const QString &name);

    int curveCount() const { return m_curves.size(); }
    Q_INVOKABLE QVariantMap curve(int index) const;
    Q_INVOKABLE QVariantList curves() const;

    // Drawing curves (lines, arcs, circles, rectangles, slots)
    Q_INVOKABLE QString addLine(double x1, double y1, double x2, double y2);
    Q_INVOKABLE QString addCircle(double cx, double cy, double radius);
    Q_INVOKABLE QString addArc(double cx, double cy, double radius, double startAngle, double endAngle);
    Q_INVOKABLE QString addRectangle(double x, double y, double w, double h);
    Q_INVOKABLE QString addText(double x, double y, const QString &text, double fontSize = 3.5);
    Q_INVOKABLE bool removeCurve(const QString &curveId);

    QJsonObject toJson() const;
    static DrawingSketch *fromJson(const QJsonObject &obj, QObject *parent = nullptr);

signals:
    void nameChanged();
    void curvesChanged();

private:
    struct Curve {
        QString id;
        QString type; // "line" | "circle" | "arc" | "rect" | "text"
        QVariantMap params;
    };
    QString m_id;
    QString m_name;
    QList<Curve> m_curves;
};