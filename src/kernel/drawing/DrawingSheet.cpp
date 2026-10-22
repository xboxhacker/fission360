// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#include "DrawingSheet.h"
#include <QDate>
#include <QJsonArray>
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

QList<QObject *> DrawingSheet::views() const
{
    QList<QObject *> out;
    out.reserve(m_views.size());
    for (DrawingView *v : m_views) out.append(v);
    return out;
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

bool DrawingSheet::updateAnnotation(const QString &id, const QVariantMap &fields)
{
    if (id.isEmpty()) return false;
    for (int i = 0; i < m_annotations.size(); ++i) {
        QVariantMap a = m_annotations[i].toMap();
        if (a.value(QStringLiteral("id")).toString() != id) continue;
        bool changed = false;
        for (auto it = fields.cbegin(); it != fields.cend(); ++it) {
            if (it.key() == QLatin1String("id") || it.key() == QLatin1String("kind")) continue;
            if (a.value(it.key()) == it.value()) continue;
            a.insert(it.key(), it.value());
            changed = true;
        }
        if (changed) {
            m_annotations[i] = a;
            emit annotationsChanged();
        }
        return true;
    }
    return false;
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

QJsonObject DrawingSheet::toJson() const
{
    QJsonArray views;
    for (DrawingView *v : m_views)
        if (v) views.append(v->toJson());
    QJsonArray sketches;
    for (DrawingSketch *s : m_sketches)
        if (s) sketches.append(s->toJson());
    return QJsonObject{
        {QStringLiteral("id"), m_id},
        {QStringLiteral("name"), m_name},
        {QStringLiteral("size"), m_size},
        {QStringLiteral("width"), m_width},
        {QStringLiteral("height"), m_height},
        {QStringLiteral("orientation"), m_orientation},
        {QStringLiteral("scale"), m_scale},
        {QStringLiteral("titleBlock"), m_titleBlock},
        {QStringLiteral("title"), m_title},
        {QStringLiteral("drawnBy"), m_drawnBy},
        {QStringLiteral("revision"), m_revision},
        {QStringLiteral("material"), m_material},
        {QStringLiteral("date"), m_date},
        {QStringLiteral("partName"), m_partName},
        {QStringLiteral("views"), views},
        {QStringLiteral("sketches"), sketches},
        {QStringLiteral("annotations"), QJsonArray::fromVariantList(m_annotations)},
        {QStringLiteral("tables"), QJsonArray::fromVariantList(m_tables)},
    };
}

DrawingSheet *DrawingSheet::fromJson(const QJsonObject &obj, QObject *parent)
{
    const QString id = obj.value(QStringLiteral("id")).toString();
    if (id.isEmpty()) return nullptr;
    const double width = obj.value(QStringLiteral("width")).toDouble();
    const double height = obj.value(QStringLiteral("height")).toDouble();
    auto *sheet = new DrawingSheet(id,
                                   obj.value(QStringLiteral("name")).toString(QStringLiteral("Sheet")),
                                   obj.value(QStringLiteral("size")).toString(QStringLiteral("A3")),
                                   width > 0 ? width : 420.0,
                                   height > 0 ? height : 297.0,
                                   obj.value(QStringLiteral("orientation")).toString(QStringLiteral("landscape")),
                                   obj.value(QStringLiteral("scale")).toDouble(1.0) > 0
                                       ? obj.value(QStringLiteral("scale")).toDouble(1.0) : 1.0,
                                   parent);
    sheet->setTitleBlockName(obj.value(QStringLiteral("titleBlock")).toString(sheet->titleBlockName()));
    sheet->setTitle(obj.value(QStringLiteral("title")).toString());
    sheet->setDrawnBy(obj.value(QStringLiteral("drawnBy")).toString());
    if (obj.contains(QStringLiteral("revision")))
        sheet->setRevision(obj.value(QStringLiteral("revision")).toString());
    sheet->setMaterial(obj.value(QStringLiteral("material")).toString());
    if (obj.contains(QStringLiteral("date")))
        sheet->setDate(obj.value(QStringLiteral("date")).toString());
    sheet->setPartName(obj.value(QStringLiteral("partName")).toString());
    for (const QJsonValue &v : obj.value(QStringLiteral("views")).toArray()) {
        if (DrawingView *view = DrawingView::fromJson(v.toObject(), sheet))
            sheet->m_views.append(view);
    }
    for (const QJsonValue &v : obj.value(QStringLiteral("sketches")).toArray()) {
        if (DrawingSketch *sk = DrawingSketch::fromJson(v.toObject(), sheet))
            sheet->m_sketches.append(sk);
    }
    sheet->m_annotations = obj.value(QStringLiteral("annotations")).toArray().toVariantList();
    sheet->m_tables = obj.value(QStringLiteral("tables")).toArray().toVariantList();
    return sheet;
}
