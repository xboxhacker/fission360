// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#include "DrawingSketch.h"
#include <QUuid>

DrawingSketch::DrawingSketch(const QString &id, const QString &name, QObject *parent)
    : QObject(parent)
    , m_id(id)
    , m_name(name)
{
}

void DrawingSketch::setName(const QString &name)
{
    if (m_name != name) {
        m_name = name;
        emit nameChanged();
    }
}

QVariantMap DrawingSketch::curve(int index) const
{
    if (index < 0 || index >= m_curves.size()) return {};
    QVariantMap m = m_curves[index].params;
    m["id"] = m_curves[index].id;
    m["type"] = m_curves[index].type;
    return m;
}

QVariantList DrawingSketch::curves() const
{
    QVariantList out;
    for (int i = 0; i < m_curves.size(); ++i) out.append(curve(i));
    return out;
}

QString DrawingSketch::addLine(double x1, double y1, double x2, double y2)
{
    QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QVariantMap p;
    p["x1"] = x1; p["y1"] = y1; p["x2"] = x2; p["y2"] = y2;
    m_curves.append({id, "line", p});
    emit curvesChanged();
    return id;
}

QString DrawingSketch::addCircle(double cx, double cy, double radius)
{
    QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QVariantMap p;
    p["cx"] = cx; p["cy"] = cy; p["radius"] = radius;
    m_curves.append({id, "circle", p});
    emit curvesChanged();
    return id;
}

QString DrawingSketch::addArc(double cx, double cy, double radius, double startAngle, double endAngle)
{
    QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QVariantMap p;
    p["cx"] = cx; p["cy"] = cy; p["radius"] = radius;
    p["startAngle"] = startAngle; p["endAngle"] = endAngle;
    m_curves.append({id, "arc", p});
    emit curvesChanged();
    return id;
}

QString DrawingSketch::addRectangle(double x, double y, double w, double h)
{
    QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QVariantMap p;
    p["x"] = x; p["y"] = y; p["w"] = w; p["h"] = h;
    m_curves.append({id, "rect", p});
    emit curvesChanged();
    return id;
}

QString DrawingSketch::addText(double x, double y, const QString &text, double fontSize)
{
    QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QVariantMap p;
    p["x"] = x; p["y"] = y; p["text"] = text; p["fontSize"] = fontSize;
    m_curves.append({id, "text", p});
    emit curvesChanged();
    return id;
}

bool DrawingSketch::removeCurve(const QString &curveId)
{
    for (int i = 0; i < m_curves.size(); ++i) {
        if (m_curves[i].id == curveId) {
            m_curves.removeAt(i);
            emit curvesChanged();
            return true;
        }
    }
    return false;
}