// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#include "DrawingView.h"
#include <QJsonArray>
#include <algorithm>
#include <cmath>

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
                            double widthMm, double heightMm, const QVariantList &edges)
{
    m_visible = visible;
    m_edges = edges;
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
        {QStringLiteral("edges"), QJsonArray::fromVariantList(m_edges)},
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
                    obj.value(QStringLiteral("height")).toDouble(),
                    obj.value(QStringLiteral("edges")).toArray().toVariantList());
    return view;
}

QPointF DrawingView::toSheet(double x, double y) const
{
    // Curves are about the view centre with y up; the sheet is in mm from the top left with y down
    return QPointF(m_position.x() + m_width * m_scale * 0.5 + x * m_scale,
                   m_position.y() + m_height * m_scale * 0.5 - y * m_scale);
}

QVariantMap DrawingView::pickEdge(double sheetX, double sheetY, double tolMm) const
{
    if (m_edges.isEmpty() || m_scale <= 0) return {};
    const double cx = m_position.x() + m_width * m_scale * 0.5;
    const double cy = m_position.y() + m_height * m_scale * 0.5;
    const double lx = (sheetX - cx) / m_scale, ly = -(sheetY - cy) / m_scale;
    const double tol = tolMm / m_scale;

    // Nearest segment, and through its "e" the edge
    int best = -1;
    double bestD = tol;
    auto scan = [&](const QVariantList &segs) {
        for (const QVariant &v : segs) {
            const QVariantMap s = v.toMap();
            if (!s.contains(QStringLiteral("e"))) continue;
            const double x1 = s.value("x1").toDouble(), y1 = s.value("y1").toDouble();
            const double dx = s.value("x2").toDouble() - x1, dy = s.value("y2").toDouble() - y1;
            const double len2 = dx * dx + dy * dy;
            const double t = len2 > 0 ? std::clamp(((lx - x1) * dx + (ly - y1) * dy) / len2, 0.0, 1.0) : 0.0;
            const double d = std::hypot(lx - (x1 + t * dx), ly - (y1 + t * dy));
            if (d < bestD) { bestD = d; best = s.value("e").toInt(); }
        }
    };
    scan(m_visible);
    if (m_showHidden) scan(m_hidden);

    // End points and circle centres snap first, a little tighter than the edges
    QPointF snap;
    double snapD = tol * 0.7;
    bool snapped = false;
    for (const QVariant &v : m_edges) {
        const QVariantMap e = v.toMap();
        if (e.value("hidden").toBool() && !m_showHidden) continue;
        QList<QPointF> pts{{e.value("x1").toDouble(), e.value("y1").toDouble()},
                           {e.value("x2").toDouble(), e.value("y2").toDouble()}};
        if (e.value("type").toString() == QLatin1String("circle"))
            pts << QPointF(e.value("cx").toDouble(), e.value("cy").toDouble());
        for (const QPointF &p : pts) {
            const double d = std::hypot(lx - p.x(), ly - p.y());
            if (d < snapD) { snapD = d; snap = p; snapped = true; }
        }
    }
    if (snapped) {
        const QPointF s = toSheet(snap.x(), snap.y());
        return {{"kind", "point"}, {"viewId", m_id}, {"x", s.x()}, {"y", s.y()}};
    }
    if (best < 0 || best >= m_edges.size()) return {};

    const QVariantMap e = m_edges[best].toMap();
    const QString type = e.value("type").toString();
    QVariantMap out{{"edge", best}, {"viewId", m_id}};
    const QPointF a = toSheet(e.value("x1").toDouble(), e.value("y1").toDouble());
    const QPointF b = toSheet(e.value("x2").toDouble(), e.value("y2").toDouble());
    out["x1"] = a.x(); out["y1"] = a.y(); out["x2"] = b.x(); out["y2"] = b.y();
    if (type == QLatin1String("circle")) {
        const QPointF c = toSheet(e.value("cx").toDouble(), e.value("cy").toDouble());
        out["kind"] = QStringLiteral("circle");
        out["cx"] = c.x(); out["cy"] = c.y();
        out["r"] = e.value("r").toDouble() * m_scale;
        out["full"] = e.value("full").toBool();
    } else {
        // A free-form curve is measured by its chord, like a line between its end points
        out["kind"] = QStringLiteral("line");
    }
    return out;
}
