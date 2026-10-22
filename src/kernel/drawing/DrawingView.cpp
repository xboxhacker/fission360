// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#include "DrawingView.h"
#include <QJsonArray>
#include <algorithm>

DrawingView::DrawingView(const QString &id, const QString &type, QObject *parent)
    : QObject(parent)
    , m_id(id)
    , m_type(type)
    , m_name(type.left(1).toUpper() + type.mid(1) + QStringLiteral(" View"))
{
}

void DrawingView::setName(const QString &name)
{
    if (m_name != name) {
        m_name = name;
        emit nameChanged();
    }
}

void DrawingView::setPosition(const QPointF &pos)
{
    if (m_position != pos) {
        m_position = pos;
        emit positionChanged();
    }
}

void DrawingView::setScale(double scale)
{
    if (!qFuzzyCompare(m_scale, scale) && scale > 0) {
        m_scale = scale;
        emit scaleChanged();
        emit geometryChanged();
    }
}

void DrawingView::setRotation(double deg)
{
    if (!qFuzzyCompare(m_rotation, deg)) {
        m_rotation = deg;
        emit rotationChanged();
    }
}

void DrawingView::setDisplayStyle(const QString &style)
{
    static const QStringList valid = {"shaded", "wireframe", "shadedWithEdges", "rendered"};
    if (valid.contains(style) && m_displayStyle != style) {
        m_displayStyle = style;
        emit styleChanged();
    }
}

void DrawingView::setRenderPng(const QString &pngB64)
{
    if (m_renderPng == pngB64) return;
    m_renderPng = pngB64;
    emit renderChanged();
}

void DrawingView::setShowHiddenLines(bool show)
{
    if (m_showHidden != show) {
        m_showHidden = show;
        emit hiddenLinesChanged();
    }
}

void DrawingView::setTangentEdgeHandling(const QString &mode)
{
    static const QStringList valid = {"visible", "removed", "phantom"};
    if (valid.contains(mode) && m_tangentMode != mode) {
        m_tangentMode = mode;
        emit tangentChanged();
    }
}

void DrawingView::setOrientation(const QString &orient)
{
    static const QStringList valid = {"front", "top", "right", "back", "bottom", "left", "home", "isometric",
                                      "isoFrontTopRight", "isoFrontTopLeft", "isoFrontBottomRight", "isoFrontBottomLeft",
                                      "isoBackTopRight", "isoBackTopLeft", "isoBackBottomRight", "isoBackBottomLeft"};
    if (valid.contains(orient) && m_orientation != orient) {
        m_orientation = orient;
        emit orientationChanged();
    }
}

void DrawingView::setParentViewId(const QString &id)
{
    if (m_parentId != id) {
        m_parentId = id;
        emit parentChanged();
    }
}

void DrawingView::setProjectDirection(const QString &dir)
{
    if (m_projectDir != dir) {
        m_projectDir = dir;
        emit parentChanged();
    }
}

void DrawingView::setDetailRadius(double r)
{
    if (!qFuzzyCompare(m_detailRadius, r) && r > 0) {
        m_detailRadius = r;
        emit detailChanged();
    }
}

void DrawingView::setDetailCenter(const QPointF &c)
{
    if (m_detailCenter != c) {
        m_detailCenter = c;
        emit detailChanged();
    }
}

void DrawingView::setCurves(const QVariantList &visible, const QVariantList &hidden, const QVariantList &section,
                            double widthMm, double heightMm)
{
    m_visible = visible;
    m_hidden = hidden;
    m_section = section;
    m_width = std::max(20.0, widthMm > 0 ? widthMm : 80.0);
    m_height = std::max(20.0, heightMm > 0 ? heightMm : 60.0);
    m_projected = !visible.isEmpty() || !hidden.isEmpty();
    emit curvesChanged();
    emit geometryChanged();
}

QJsonObject DrawingView::toJson() const
{
    return QJsonObject{
        {QStringLiteral("id"), m_id},
        {QStringLiteral("name"), m_name},
        {QStringLiteral("type"), m_type},
        {QStringLiteral("x"), m_position.x()},
        {QStringLiteral("y"), m_position.y()},
        {QStringLiteral("scale"), m_scale},
        {QStringLiteral("rotation"), m_rotation},
        {QStringLiteral("width"), m_width},
        {QStringLiteral("height"), m_height},
        {QStringLiteral("displayStyle"), m_displayStyle},
        {QStringLiteral("renderPng"), m_renderPng},
        {QStringLiteral("showHiddenLines"), m_showHidden},
        {QStringLiteral("tangentEdgeHandling"), m_tangentMode},
        {QStringLiteral("orientation"), m_orientation},
        {QStringLiteral("parentViewId"), m_parentId},
        {QStringLiteral("projectDirection"), m_projectDir},
        {QStringLiteral("detailRadius"), m_detailRadius},
        {QStringLiteral("detailCenterX"), m_detailCenter.x()},
        {QStringLiteral("detailCenterY"), m_detailCenter.y()},
        {QStringLiteral("visibleCurves"), QJsonArray::fromVariantList(m_visible)},
        {QStringLiteral("hiddenCurves"), QJsonArray::fromVariantList(m_hidden)},
        {QStringLiteral("sectionCurves"), QJsonArray::fromVariantList(m_section)},
    };
}

DrawingView *DrawingView::fromJson(const QJsonObject &obj, QObject *parent)
{
    const QString id = obj.value(QStringLiteral("id")).toString();
    const QString type = obj.value(QStringLiteral("type")).toString();
    if (id.isEmpty() || type.isEmpty()) return nullptr;
    auto *view = new DrawingView(id, type, parent);
    if (obj.contains(QStringLiteral("name")))
        view->setName(obj.value(QStringLiteral("name")).toString());
    view->setPosition(QPointF(obj.value(QStringLiteral("x")).toDouble(), obj.value(QStringLiteral("y")).toDouble()));
    if (obj.contains(QStringLiteral("scale")))
        view->setScale(obj.value(QStringLiteral("scale")).toDouble(1.0));
    if (obj.contains(QStringLiteral("rotation")))
        view->setRotation(obj.value(QStringLiteral("rotation")).toDouble());
    if (obj.contains(QStringLiteral("displayStyle")))
        view->setDisplayStyle(obj.value(QStringLiteral("displayStyle")).toString());
    if (obj.contains(QStringLiteral("renderPng")))
        view->setRenderPng(obj.value(QStringLiteral("renderPng")).toString());
    if (obj.contains(QStringLiteral("showHiddenLines")))
        view->setShowHiddenLines(obj.value(QStringLiteral("showHiddenLines")).toBool());
    if (obj.contains(QStringLiteral("tangentEdgeHandling")))
        view->setTangentEdgeHandling(obj.value(QStringLiteral("tangentEdgeHandling")).toString());
    if (obj.contains(QStringLiteral("orientation")))
        view->setOrientation(obj.value(QStringLiteral("orientation")).toString());
    if (obj.contains(QStringLiteral("parentViewId")))
        view->setParentViewId(obj.value(QStringLiteral("parentViewId")).toString());
    if (obj.contains(QStringLiteral("projectDirection")))
        view->setProjectDirection(obj.value(QStringLiteral("projectDirection")).toString());
    if (obj.contains(QStringLiteral("detailRadius")))
        view->setDetailRadius(obj.value(QStringLiteral("detailRadius")).toDouble());
    if (obj.contains(QStringLiteral("detailCenterX")) || obj.contains(QStringLiteral("detailCenterY")))
        view->setDetailCenter(QPointF(obj.value(QStringLiteral("detailCenterX")).toDouble(),
                                      obj.value(QStringLiteral("detailCenterY")).toDouble()));
    view->setCurves(obj.value(QStringLiteral("visibleCurves")).toArray().toVariantList(),
                    obj.value(QStringLiteral("hiddenCurves")).toArray().toVariantList(),
                    obj.value(QStringLiteral("sectionCurves")).toArray().toVariantList(),
                    obj.value(QStringLiteral("width")).toDouble(),
                    obj.value(QStringLiteral("height")).toDouble());
    return view;
}
