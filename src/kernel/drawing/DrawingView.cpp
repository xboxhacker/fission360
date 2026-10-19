// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#include "DrawingView.h"
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
    static const QStringList valid = {"shaded", "wireframe", "shadedWithEdges"};
    if (valid.contains(style) && m_displayStyle != style) {
        m_displayStyle = style;
        emit styleChanged();
    }
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
    static const QStringList valid = {"front", "top", "right", "back", "bottom", "left", "home", "isometric"};
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
