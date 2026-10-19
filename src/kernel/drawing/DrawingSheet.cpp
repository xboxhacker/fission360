// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#include "DrawingSheet.h"
#include <QDate>
#include <QUuid>

DrawingSheet::DrawingSheet(const QString &id, const QString &name,
                           const QString &size, double width, double height,
                           const QString &orientation, double scale,
                           QObject *parent)
    : QObject(parent)
    , m_id(id)
    , m_name(name)
    , m_size(size)
    , m_width(width)
    , m_height(height)
    , m_orientation(orientation)
    , m_scale(scale)
    , m_date(QDate::currentDate().toString(Qt::ISODate))
{
}

void DrawingSheet::setName(const QString &name)
{
    if (m_name != name) {
        m_name = name;
        emit nameChanged();
    }
}

void DrawingSheet::setScale(double scale)
{
    if (qFuzzyCompare(m_scale, scale)) return;
    m_scale = scale;
    emit scaleChanged();
}

DrawingView *DrawingSheet::view(int index) const
{
    if (index < 0 || index >= m_views.size()) return nullptr;
    return m_views[index];
}

DrawingView *DrawingSheet::viewById(const QString &id) const
{
    for (DrawingView *v : m_views)
        if (v->id() == id) return v;
    return nullptr;
}

DrawingView *DrawingSheet::addView(const QString &type, const QVariantMap &options)
{
    QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    auto *view = new DrawingView(id, type, this);

    if (options.contains("position"))
        view->setPosition(options["position"].toPointF());
    if (options.contains("scale"))
        view->setScale(options["scale"].toDouble());
    if (options.contains("orientation"))
        view->setOrientation(options["orientation"].toString());
    if (options.contains("displayStyle"))
        view->setDisplayStyle(options["displayStyle"].toString());
    if (options.contains("showHiddenLines"))
        view->setShowHiddenLines(options["showHiddenLines"].toBool());
    if (options.contains("parentViewId"))
        view->setParentViewId(options["parentViewId"].toString());
    if (options.contains("projectDirection"))
        view->setProjectDirection(options["projectDirection"].toString());
    if (options.contains("detailRadius"))
        view->setDetailRadius(options["detailRadius"].toDouble());
    if (options.contains("detailCenter"))
        view->setDetailCenter(options["detailCenter"].toPointF());
    if (options.contains("name"))
        view->setName(options["name"].toString());

    m_views.append(view);
    emit viewsChanged();
    return view;
}

bool DrawingSheet::removeView(const QString &viewId)
{
    for (int i = 0; i < m_views.size(); ++i) {
        if (m_views[i]->id() == viewId) {
            m_views[i]->deleteLater();
            m_views.removeAt(i);
            emit viewsChanged();
            return true;
        }
    }
    return false;
}

DrawingSketch *DrawingSheet::sketch(int index) const
{
    if (index < 0 || index >= m_sketches.size()) return nullptr;
    return m_sketches[index];
}

DrawingSketch *DrawingSheet::addSketch(const QString &name)
{
    QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    auto *sketch = new DrawingSketch(id, name, this);
    m_sketches.append(sketch);
    emit sketchesChanged();
    return sketch;
}

QVariantMap DrawingSheet::annotation(int index) const
{
    if (index < 0 || index >= m_annotations.size()) return {};
    return m_annotations[index].toMap();
}

QString DrawingSheet::addAnnotation(const QVariantMap &ann)
{
    QVariantMap a = ann;
    if (!a.contains("id"))
        a["id"] = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_annotations.append(a);
    emit annotationsChanged();
    return a.value("id").toString();
}

bool DrawingSheet::removeAnnotation(const QString &id)
{
    for (int i = 0; i < m_annotations.size(); ++i) {
        if (m_annotations[i].toMap().value("id").toString() == id) {
            m_annotations.removeAt(i);
            emit annotationsChanged();
            return true;
        }
    }
    return false;
}

QVariantMap DrawingSheet::table(int index) const
{
    if (index < 0 || index >= m_tables.size()) return {};
    return m_tables[index].toMap();
}

QString DrawingSheet::addTable(const QVariantMap &table)
{
    QVariantMap t = table;
    if (!t.contains("id"))
        t["id"] = QUuid::createUuid().toString(QUuid::WithoutBraces);
    if (!t.contains("kind"))
        t["kind"] = QStringLiteral("general");
    m_tables.append(t);
    emit tablesChanged();
    return t.value("id").toString();
}

bool DrawingSheet::removeTable(const QString &id)
{
    for (int i = 0; i < m_tables.size(); ++i) {
        if (m_tables[i].toMap().value("id").toString() == id) {
            m_tables.removeAt(i);
            emit tablesChanged();
            return true;
        }
    }
    return false;
}

void DrawingSheet::setTitleBlockName(const QString &name)
{
    if (m_titleBlock != name) {
        m_titleBlock = name;
        emit titleBlockChanged();
    }
}

void DrawingSheet::setTitle(const QString &v)
{
    if (m_title != v) { m_title = v; emit titleBlockChanged(); }
}
void DrawingSheet::setDrawnBy(const QString &v)
{
    if (m_drawnBy != v) { m_drawnBy = v; emit titleBlockChanged(); }
}
void DrawingSheet::setRevision(const QString &v)
{
    if (m_revision != v) { m_revision = v; emit titleBlockChanged(); }
}
void DrawingSheet::setMaterial(const QString &v)
{
    if (m_material != v) { m_material = v; emit titleBlockChanged(); }
}
void DrawingSheet::setDate(const QString &v)
{
    if (m_date != v) { m_date = v; emit titleBlockChanged(); }
}
void DrawingSheet::setPartName(const QString &v)
{
    if (m_partName != v) { m_partName = v; emit titleBlockChanged(); }
}
