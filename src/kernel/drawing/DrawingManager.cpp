// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#include "DrawingManager.h"
#include "DrawingDimension.h"
#include "DrawingProjector.h"
#include "kernel/FissionBody.h"
#include "kernel/FissionDocument.h"
#include "kernel/FissionFeature.h"
#include "kernel/FissionTimeline.h"
#include "kernel/FileExport.h"

#include <QDateTime>
#include <QFileInfo>
#include <QImage>
#include <QByteArray>
#include <QJsonObject>
#include <QPainter>
#include <QPageSize>
#include <QPdfWriter>
#include <QUrl>
#include <QUuid>
#include <QPolygonF>
#include <QtMath>
#include <algorithm>
#include <cmath>

DrawingManager::DrawingManager(QObject *parent)
    : QObject(parent)
{
}

QVariantMap DrawingManager::createDrawingInput(const QString &designDocId)
{
    QVariantMap input;
    input["designDocId"] = designDocId;
    input["standard"] = QStringLiteral("ISO");
    input["projectionAngle"] = QStringLiteral("third");
    input["sheetSize"] = QStringLiteral("A3");
    input["orientation"] = QStringLiteral("landscape");
    input["scale"] = 1.0;
    input["includeHidden"] = false;
    input["showTangentEdges"] = true;
    input["displayStyle"] = QStringLiteral("wireframe");
    input["customWidth"] = 0.0;
    input["customHeight"] = 0.0;
    input["title"] = QString();
    input["drawnBy"] = QString();
    return input;
}

QString DrawingManager::createDrawingDocument(const QVariantMap &input)
{
    QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QString name = input.value("name").toString();
    if (name.isEmpty())
        name = QStringLiteral("Drawing ") + QDateTime::currentDateTime().toString(Qt::ISODate);
    QString designId = input.value("designDocId").toString();
    QString standard = input.value("standard", "ISO").toString();
    QString projection = input.value("projectionAngle", "third").toString();

    auto *doc = new DrawingDocument(id, name, designId, standard, projection, this);
    const QString size = input.value("sheetSize", "A3").toString();
    const QString orient = input.value("orientation", "landscape").toString();
    DrawingSheet *sheet = nullptr;
    if (size == QLatin1String("custom")) {
        sheet = doc->addCustomSheet(QStringLiteral("Sheet1"),
                                    input.value("customWidth", 420).toDouble(),
                                    input.value("customHeight", 297).toDouble(),
                                    orient);
    } else {
        sheet = doc->addSheet(QStringLiteral("Sheet1"), size, orient);
    }
    if (sheet) {
        sheet->setTitle(input.value("title").toString());
        sheet->setDrawnBy(input.value("drawnBy").toString());
        sheet->setPartName(input.value("partName").toString());
        sheet->setMaterial(input.value("material").toString());
        sheet->setTitleBlockName(standard);
    }
    // Stamp against the live design revision when the caller passes contentRevision in the input
    if (input.contains(QStringLiteral("contentRevision")))
        doc->setSourceDesignRevision(input.value(QStringLiteral("contentRevision")).toLongLong());
    m_drawings.append(doc);
    emit drawingsChanged();
    return id;
}

DrawingDocument *DrawingManager::drawing(int index) const
{
    if (index < 0 || index >= m_drawings.size()) return nullptr;
    return m_drawings[index];
}

DrawingDocument *DrawingManager::drawingById(const QString &id) const
{
    for (auto *d : m_drawings)
        if (d->id() == id) return d;
    return nullptr;
}

QJsonArray DrawingManager::toJsonForDesign(const QString &designId) const
{
    QJsonArray out;
    for (DrawingDocument *d : m_drawings) {
        if (!d) continue;
        if (!designId.isEmpty() && d->referencedDesignId() != designId) continue;
        out.append(d->toJson());
    }
    return out;
}

void DrawingManager::restoreFromJson(const QJsonArray &drawings)
{
    bool changed = false;
    for (const QJsonValue &v : drawings) {
        const QJsonObject obj = v.toObject();
        const QString id = obj.value(QStringLiteral("id")).toString();
        if (id.isEmpty()) continue;
        if (drawingById(id))
            removeDrawing(id);
        DrawingDocument *doc = DrawingDocument::fromJson(obj, this);
        if (!doc) continue;
        m_drawings.append(doc);
        changed = true;
    }
    if (changed) emit drawingsChanged();
}

bool DrawingManager::removeDrawing(const QString &id)
{
    for (int i = 0; i < m_drawings.size(); ++i) {
        if (m_drawings[i]->id() == id) {
            m_drawings[i]->deleteLater();
            m_drawings.removeAt(i);
            emit drawingsChanged();
            return true;
        }
    }
    return false;
}

bool DrawingManager::projectView(DrawingView *view, FissionDocument *design, DrawingDocument *drawingDoc)
{
    m_lastError.clear();
    if (!view) { m_lastError = "No view"; return false; }
    if (!design) { m_lastError = "No design"; return false; }

    QString orient = view->orientation();
    const QString type = view->type();
    const bool rendered = view->displayStyle() == QLatin1String("rendered");
    const bool hidden = view->showHiddenLines() && !rendered;
    DrawingProjector::Result result;

    if (type == QLatin1String("projected") && !view->parentViewId().isEmpty() && drawingDoc) {
        DrawingSheet *sheet = nullptr;
        for (int i = 0; i < drawingDoc->sheetCount(); ++i) {
            DrawingSheet *s = drawingDoc->sheet(i);
            if (s && s->viewById(view->id())) { sheet = s; break; }
        }
        DrawingView *parent = nullptr;
        if (sheet) parent = sheet->viewById(view->parentViewId());
        if (parent) {
            orient = DrawingProjector::projectedOrientation(parent->orientation(), view->projectDirection(),
                                                           drawingDoc->projectionAngle());
            view->setOrientation(orient);
        }
        result = DrawingProjector::project(design, orient, hidden);
    } else if (type == QLatin1String("section")) {
        result = DrawingProjector::projectSection(design, orient,
                                                  view->projectDirection().isEmpty() ? QStringLiteral("right")
                                                                                    : view->projectDirection());
    } else if (type == QLatin1String("detail")) {
        // Project the parent orientation (or this view's), then clip to the detail circle in view space.
        result = DrawingProjector::project(design, orient, hidden);
        if (result.ok && view->detailRadius() > 0) {
            const QPointF c = view->detailCenter();
            const double r = view->detailRadius();
            auto inside = [&](double x, double y) {
                return (x - c.x()) * (x - c.x()) + (y - c.y()) * (y - c.y()) <= r * r;
            };
            QVariantList clipped;
            for (const QVariant &v : result.visible) {
                const QVariantMap m = v.toMap();
                if (inside(m.value("x1").toDouble(), m.value("y1").toDouble())
                    || inside(m.value("x2").toDouble(), m.value("y2").toDouble()))
                    clipped.append(m);
            }
            // Keep the full projection if the crop removed everything (centre still empty).
            if (!clipped.isEmpty()) result.visible = clipped;
            result.width = std::max(result.width, r * 2);
            result.height = std::max(result.height, r * 2);
        }
    } else if (type == QLatin1String("auxiliary")) {
        result = DrawingProjector::project(design,
                                           orient.isEmpty() ? QStringLiteral("isometric") : orient,
                                           hidden);
    } else {
        result = DrawingProjector::project(design, orient, hidden);
    }

    if (!result.ok) {
        m_lastError = result.error.isEmpty() ? QStringLiteral("Projection failed") : result.error;
        return false;
    }
    view->setCurves(result.visible, result.hidden, result.section, result.width, result.height, result.edges);
    // Dimensions on this view are measured again on the new projection (a design edit changes the edges)
    if (auto *sheet = qobject_cast<DrawingSheet *>(view->parent())) {
        for (int i = 0; i < sheet->annotationCount(); ++i) {
            QVariantMap a = sheet->annotation(i);
            if (a.value("space").toString() != QLatin1String("view") || a.value("view").toString() != view->id())
                continue;
            if (!DrawingDimension::reattach(a, view->edges())) a["detached"] = true;
            sheet->updateAnnotation(a.value("id").toString(), a);
        }
    }
    if (rendered) {
        const QByteArray png = DrawingProjector::renderShadedPng(design, orient, 420);
        view->setRenderPng(QString::fromLatin1(png.toBase64()));
    } else if (!view->renderPng().isEmpty()) {
        view->setRenderPng(QString());
    }
    return true;
}

int DrawingManager::regenerateSheet(DrawingSheet *sheet, FissionDocument *design, DrawingDocument *drawingDoc)
{
    if (!sheet || !design) return 0;
    int n = 0;
    for (int i = 0; i < sheet->viewCount(); ++i) {
        if (projectView(sheet->view(i), design, drawingDoc)) ++n;
    }
    return n;
}

int DrawingManager::updateDrawingFromDesign(DrawingDocument *drawingDoc, FissionDocument *design)
{
    m_lastError.clear();
    if (!drawingDoc || !design) {
        m_lastError = QStringLiteral("Drawing and design are required");
        return 0;
    }
    int n = 0;
    for (int i = 0; i < drawingDoc->sheetCount(); ++i)
        n += regenerateSheet(drawingDoc->sheet(i), design, drawingDoc);
    drawingDoc->setSourceDesignRevision(design->contentRevision());
    drawingDoc->pushHistory(QStringLiteral("Updated from design (rev %1)").arg(design->contentRevision()));
    design->linkAsset(QStringLiteral("drawing"), drawingDoc->id(), drawingDoc->name());
    return n;
}

void DrawingManager::refreshDrawingSync(FissionDocument *design)
{
    if (!design) return;
    const qint64 rev = design->contentRevision();
    for (DrawingDocument *d : m_drawings) {
        if (!d) continue;
        if (d->referencedDesignId() == design->documentId()
            || d->referencedDesignId() == design->objectName()
            || d->referencedDesignId().isEmpty())
            d->refreshOutOfDate(rev);
    }
}

namespace {

DrawingView *viewOn(DrawingSheet *sheet, const QString &id) {
    if (!sheet || id.isEmpty()) return nullptr;
    for (int i = 0; i < sheet->viewCount(); ++i)
        if (DrawingView *v = sheet->view(i); v && v->id() == id) return v;
    return nullptr;
}

DrawingDimension::ViewFrame frameOf(const DrawingView *v) {
    return {v->position().x() + v->width() * v->scale() * 0.5, v->position().y() + v->height() * v->scale() * 0.5,
            v->scale()};
}

// A dimension in sheet space: one attached to a view is placed by that view's current position and scale
QVariantMap sheetSpace(DrawingSheet *sheet, const QVariantMap &ann) {
    if (ann.value("space").toString() != QLatin1String("view")) return ann;
    const DrawingView *v = viewOn(sheet, ann.value("view").toString());
    return v ? DrawingDimension::toSheet(ann, frameOf(v)) : ann;
}

// A dimension from DrawingDimension::geometry, k device units per sheet mm
void paintDimension(QPainter &p, const QVariantMap &ann, double k)
{
    const QVariantMap g = DrawingDimension::geometry(ann);
    p.save();
    p.setPen(QPen(Qt::black, std::max(0.35, 0.18 * k)));
    p.setBrush(Qt::NoBrush);
    for (const QVariant &v : g.value("lines").toList()) {
        const QVariantList s = v.toList();
        p.drawLine(QPointF(s[0].toDouble() * k, s[1].toDouble() * k), QPointF(s[2].toDouble() * k, s[3].toDouble() * k));
    }
    for (const QVariant &v : g.value("arcs").toList()) {
        const QVariantList s = v.toList();
        const double r = s[2].toDouble() * k;
        const QRectF box(s[0].toDouble() * k - r, s[1].toDouble() * k - r, 2 * r, 2 * r);
        // Qt angles are counter-clockwise in 1/16 degree; the sheet's are clockwise (y down)
        p.drawArc(box, int(-s[3].toDouble() * 16), int(-s[4].toDouble() * 16));
    }
    const double len = g.value("arrowSize", 3.0).toDouble() * k;
    p.setBrush(Qt::black);
    for (const QVariant &v : g.value("arrows").toList()) {
        const QVariantList s = v.toList();
        const QPointF tip(s[0].toDouble() * k, s[1].toDouble() * k), dir(s[2].toDouble(), s[3].toDouble());
        const QPointF back = tip - dir * len, side(-dir.y() * len * 0.3, dir.x() * len * 0.3);
        p.drawPolygon(QPolygonF{tip, back + side, back - side});
    }
    const QVariantMap t = g.value("text").toMap();
    QFont f = p.font();
    f.setPixelSize(std::max(4, int(t.value("size").toDouble() * k)));
    p.setFont(f);
    p.translate(t.value("x").toDouble() * k, t.value("y").toDouble() * k);
    p.rotate(t.value("angle").toDouble());
    const QString str = t.value("str").toString();
    const QRectF box = p.fontMetrics().boundingRect(str);
    p.drawText(QPointF(-box.width() / 2, box.height() / 3), str);
    p.restore();
}

void paintSheet(QPainter &p, DrawingSheet *sheet, double scalePxPerMm)
{
    if (!sheet) return;
    p.fillRect(QRectF(0, 0, sheet->width() * scalePxPerMm, sheet->height() * scalePxPerMm), Qt::white);
    p.setRenderHint(QPainter::Antialiasing, true);

    auto drawSegs = [&](const QVariantList &segs, const QPen &pen, DrawingView *view, double ox, double oy) {
        p.setPen(pen);
        const double s = view->scale() * scalePxPerMm;
        const double cx = ox + view->width() * view->scale() * scalePxPerMm * 0.5;
        const double cy = oy + view->height() * view->scale() * scalePxPerMm * 0.5;
        // Y is up in model mm, down on painter — flip
        for (const QVariant &v : segs) {
            const QVariantMap m = v.toMap();
            p.drawLine(QPointF(cx + m.value("x1").toDouble() * s, cy - m.value("y1").toDouble() * s),
                       QPointF(cx + m.value("x2").toDouble() * s, cy - m.value("y2").toDouble() * s));
        }
    };

    for (int i = 0; i < sheet->viewCount(); ++i) {
        DrawingView *view = sheet->view(i);
        if (!view) continue;
        const double ox = view->position().x() * scalePxPerMm;
        const double oy = view->position().y() * scalePxPerMm;
        drawSegs(view->hiddenCurves(), QPen(QColor(160, 160, 160), 0.4, Qt::DashLine), view, ox, oy);
        drawSegs(view->visibleCurves(), QPen(Qt::black, 0.7), view, ox, oy);
        drawSegs(view->sectionCurves(), QPen(Qt::black, 1.2), view, ox, oy);
        if (view->type() == QLatin1String("detail") && view->detailRadius() > 0) {
            const double s = view->scale() * scalePxPerMm;
            const double cx = ox + view->width() * view->scale() * scalePxPerMm * 0.5;
            const double cy = oy + view->height() * view->scale() * scalePxPerMm * 0.5;
            p.setPen(QPen(Qt::black, 0.8));
            p.setBrush(Qt::NoBrush);
            p.drawEllipse(QPointF(cx, cy), view->detailRadius() * s, view->detailRadius() * s);
        }
    }

    // Annotations
    for (int i = 0; i < sheet->annotationCount(); ++i) {
        const QVariantMap a = sheet->annotation(i);
        const QString kind = a.value("kind").toString();
        const double x = a.value("x").toDouble() * scalePxPerMm;
        const double y = a.value("y").toDouble() * scalePxPerMm;
        if (kind == QLatin1String("text") || kind == QLatin1String("holeNote")) {
            p.setPen(Qt::black);
            QFont f = p.font();
            f.setPointSizeF(std::max(6.0, a.value("fontSize", 3.5).toDouble() * scalePxPerMm * 0.75));
            f.setBold(a.value("bold").toBool());
            f.setItalic(a.value("italic").toBool());
            p.setFont(f);
            p.drawText(QPointF(x, y), a.value("text").toString());
        } else if (kind == QLatin1String("dimension")) {
            paintDimension(p, sheetSpace(sheet, a), scalePxPerMm);
        } else if (kind == QLatin1String("leader")) {
            p.setPen(QPen(Qt::black, 0.5));
            const double x2 = a.value("x2", a.value("x").toDouble()).toDouble() * scalePxPerMm;
            const double y2 = a.value("y2", a.value("y").toDouble()).toDouble() * scalePxPerMm;
            p.drawLine(QPointF(x, y), QPointF(x2, y2));
            const QString label = a.value("text").toString();
            if (!label.isEmpty())
                p.drawText(QPointF((x + x2) * 0.5, (y + y2) * 0.5 - 2), label);
        } else if (kind == QLatin1String("centerMark")) {
            const double r = a.value("radius", 3.0).toDouble() * scalePxPerMm;
            p.setPen(QPen(Qt::black, 0.4));
            p.drawLine(QPointF(x - r, y), QPointF(x + r, y));
            p.drawLine(QPointF(x, y - r), QPointF(x, y + r));
        }
    }

    // Images
    for (int i = 0; i < sheet->annotationCount(); ++i) {
        const QVariantMap a = sheet->annotation(i);
        if (a.value("kind").toString() != QLatin1String("image")) continue;
        QImage img(a.value("path").toString());
        if (img.isNull()) continue;
        const QRectF r(a.value("x").toDouble() * scalePxPerMm, a.value("y").toDouble() * scalePxPerMm,
                       a.value("width").toDouble() * scalePxPerMm, a.value("height").toDouble() * scalePxPerMm);
        p.drawImage(r, img);
    }

    // Tables
    for (int ti = 0; ti < sheet->tableCount(); ++ti) {
        const QVariantMap t = sheet->table(ti);
        const QVariantList rows = t.value("rows").toList();
        const QVariantList cols = t.value("colWidths").toList();
        double x = t.value("x").toDouble() * scalePxPerMm;
        double y = t.value("y").toDouble() * scalePxPerMm;
        const double rowH = 5.0 * scalePxPerMm;
        p.setPen(QPen(Qt::black, 0.5));
        QFont tf = p.font();
        tf.setPointSizeF(6);
        tf.setBold(true);
        p.setFont(tf);
        p.drawText(QPointF(x, y - 2 * scalePxPerMm), t.value("title").toString());
        tf.setBold(false);
        p.setFont(tf);
        for (int r = 0; r < rows.size(); ++r) {
            const QVariantList cells = rows[r].toList();
            double cx = x;
            for (int c = 0; c < cells.size(); ++c) {
                const double cw = (c < cols.size() ? cols[c].toDouble() : 20.0) * scalePxPerMm;
                p.drawRect(QRectF(cx, y + r * rowH, cw, rowH));
                p.drawText(QRectF(cx + 1, y + r * rowH, cw - 2, rowH), Qt::AlignVCenter | Qt::AlignLeft,
                           cells[c].toString());
                cx += cw;
            }
        }
    }

    // Title block (unless it was deleted from the sheet)
    if (!sheet->titleBlockVisible()) return;
    const double tw = 70 * scalePxPerMm, th = 28 * scalePxPerMm;
    const double tx = sheet->width() * scalePxPerMm - tw - 4 * scalePxPerMm;
    const double ty = sheet->height() * scalePxPerMm - th - 4 * scalePxPerMm;
    p.setPen(QPen(Qt::black, 0.8));
    p.setBrush(QColor(248, 248, 248));
    p.drawRect(QRectF(tx, ty, tw, th));
    p.setPen(Qt::black);
    QFont f = p.font();
    f.setPointSizeF(8);
    f.setBold(true);
    p.setFont(f);
    p.drawText(QRectF(tx + 4, ty + 2, tw - 8, 12), sheet->partName().isEmpty() ? sheet->name() : sheet->partName());
    f.setBold(false);
    f.setPointSizeF(6);
    p.setFont(f);
    p.drawText(QRectF(tx + 4, ty + 12, tw - 8, 10),
               QStringLiteral("%1  Rev %2  %3").arg(sheet->drawnBy(), sheet->revision(), sheet->date()));
    p.drawText(QRectF(tx + 4, ty + 20, tw - 8, 8),
               QStringLiteral("%1 · %2").arg(sheet->titleBlockName(), sheet->size()));
}

}   // namespace

bool DrawingManager::exportPdf(DrawingSheet *sheet, const QString &path)
{
    m_lastError.clear();
    if (!sheet) { m_lastError = "No sheet"; return false; }
    QPdfWriter pdf(path);
    pdf.setPageSize(QPageSize(QSizeF(sheet->width(), sheet->height()), QPageSize::Millimeter));
    pdf.setPageMargins(QMarginsF(0, 0, 0, 0));
    pdf.setTitle(sheet->name());
    QPainter p(&pdf);
    if (!p.isActive()) { m_lastError = "Could not open PDF"; return false; }
    // QPdfWriter paints in points (1/72"); convert mm → points
    const double ptPerMm = 72.0 / 25.4;
    paintSheet(p, sheet, ptPerMm);
    p.end();
    return true;
}

bool DrawingManager::exportDxf(DrawingSheet *sheet, const QString &path)
{
    m_lastError.clear();
    if (!sheet) { m_lastError = "No sheet"; return false; }
    FileExport::DxfWriter dxf{gp_Ax3()};
    if (!dxf.open(path, {{"VISIBLE", 7}, {"HIDDEN", 8}, {"SECTION", 1}, {"ANNO", 3}, {"TITLE", 4}}, &m_lastError))
        return false;

    // The sheet is in mm from the top left with y down; DXF has y up, so Y = sheet height - y everywhere.
    // (Views and notes used to be written with different conventions and did not line up.)
    const double H = sheet->height();
    for (int i = 0; i < sheet->viewCount(); ++i) {
        DrawingView *view = sheet->view(i);
        if (!view) continue;
        auto emitSegs = [&](const QVariantList &segs, const QString &layer) {
            for (const QVariant &v : segs) {
                const QVariantMap m = v.toMap();
                const QPointF a = view->toSheet(m.value("x1").toDouble(), m.value("y1").toDouble());
                const QPointF b = view->toSheet(m.value("x2").toDouble(), m.value("y2").toDouble());
                dxf.addLine2d(a.x(), H - a.y(), b.x(), H - b.y(), layer);
            }
        };
        emitSegs(view->visibleCurves(), QStringLiteral("VISIBLE"));
        if (view->showHiddenLines()) emitSegs(view->hiddenCurves(), QStringLiteral("HIDDEN"));
        emitSegs(view->sectionCurves(), QStringLiteral("SECTION"));
    }
    for (int i = 0; i < sheet->annotationCount(); ++i) {
        const QVariantMap a = sheet->annotation(i);
        const QString kind = a.value("kind").toString();
        if (kind == QLatin1String("text") || kind == QLatin1String("holeNote")) {
            dxf.addText2d(a.value("x").toDouble(), H - a.value("y").toDouble(), a.value("text").toString(),
                          a.value("fontSize", 3.5).toDouble(), QStringLiteral("ANNO"));
        } else if (kind == QLatin1String("dimension")) {
            const QVariantMap g = DrawingDimension::geometry(sheetSpace(sheet, a));
            for (const QVariant &v : g.value("lines").toList()) {
                const QVariantList s = v.toList();
                dxf.addLine2d(s[0].toDouble(), H - s[1].toDouble(), s[2].toDouble(), H - s[3].toDouble(),
                              QStringLiteral("ANNO"));
            }
            for (const QVariant &v : g.value("arcs").toList()) {   // arcs as short lines (the writer has no ARC)
                const QVariantList s = v.toList();
                const double cx = s[0].toDouble(), cy = s[1].toDouble(), r = s[2].toDouble();
                const double a0 = qDegreesToRadians(s[3].toDouble()), sw = qDegreesToRadians(s[4].toDouble());
                const int n = std::max(4, int(std::fabs(sw) * 12));
                for (int k = 0; k < n; ++k) {
                    const double t0 = a0 + sw * k / n, t1 = a0 + sw * (k + 1) / n;
                    dxf.addLine2d(cx + r * std::cos(t0), H - (cy + r * std::sin(t0)), cx + r * std::cos(t1),
                                  H - (cy + r * std::sin(t1)), QStringLiteral("ANNO"));
                }
            }
            const double len = g.value("arrowSize", 3.0).toDouble();
            for (const QVariant &v : g.value("arrows").toList()) {
                const QVariantList s = v.toList();
                const double tx = s[0].toDouble(), ty = s[1].toDouble(), dx = s[2].toDouble(), dy = s[3].toDouble();
                const double bx = tx - dx * len, by = ty - dy * len, sx = -dy * len * 0.3, sy = dx * len * 0.3;
                dxf.addLine2d(tx, H - ty, bx + sx, H - (by + sy), QStringLiteral("ANNO"));
                dxf.addLine2d(tx, H - ty, bx - sx, H - (by - sy), QStringLiteral("ANNO"));
            }
            const QVariantMap t = g.value("text").toMap();
            dxf.addText2d(t.value("x").toDouble(), H - t.value("y").toDouble(), t.value("str").toString(),
                          t.value("size").toDouble(), QStringLiteral("ANNO"));
        } else if (kind == QLatin1String("leader")) {
            dxf.addLine2d(a.value("x").toDouble(), H - a.value("y").toDouble(),
                          a.value("x2", a.value("x").toDouble()).toDouble(),
                          H - a.value("y2", a.value("y").toDouble()).toDouble(), QStringLiteral("ANNO"));
        }
    }
    if (sheet->titleBlockVisible())
        dxf.addText2d(sheet->width() - 68, 8, sheet->partName().isEmpty() ? sheet->name() : sheet->partName(),
                      3.5, QStringLiteral("TITLE"));
    return dxf.close(&m_lastError);
}

bool DrawingManager::exportPng(DrawingSheet *sheet, const QString &path, int dpi)
{
    m_lastError.clear();
    if (!sheet) { m_lastError = "No sheet"; return false; }
    const double pxPerMm = dpi / 25.4;
    QImage img(int(sheet->width() * pxPerMm), int(sheet->height() * pxPerMm), QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::white);
    QPainter p(&img);
    paintSheet(p, sheet, pxPerMm);
    p.end();
    if (!img.save(path)) {
        m_lastError = QStringLiteral("Could not write %1").arg(path);
        return false;
    }
    return true;
}

bool DrawingManager::exportTiff(DrawingSheet *sheet, const QString &path, int dpi)
{
    m_lastError.clear();
    if (!sheet) { m_lastError = "No sheet"; return false; }
    const double pxPerMm = dpi / 25.4;
    QImage img(int(sheet->width() * pxPerMm), int(sheet->height() * pxPerMm), QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::white);
    QPainter p(&img);
    paintSheet(p, sheet, pxPerMm);
    p.end();
    if (!img.save(path, "TIFF")) {
        // Fall back to PNG bytes with a .tif name if the TIFF plugin is missing
        if (!img.save(path, "PNG")) {
            m_lastError = QStringLiteral("Could not write %1").arg(path);
            return false;
        }
    }
    return true;
}

QString DrawingManager::addPartsList(DrawingSheet *sheet, FissionDocument *design, const QPointF &pos)
{
    m_lastError.clear();
    if (!sheet) { m_lastError = "No sheet"; return {}; }
    QVariantList rows;
    rows << QVariantList{QStringLiteral("Item"), QStringLiteral("Qty"), QStringLiteral("Name"), QStringLiteral("Material")};
    int item = 1;
    if (design) {
        for (int i = 0; i < design->bodyCount(); ++i) {
            FissionBody *b = design->body(i);
            if (!b) continue;
            rows << QVariantList{QString::number(item++), QStringLiteral("1"), b->name(),
                                 sheet->material().isEmpty() ? QStringLiteral("-") : sheet->material()};
        }
    }
    if (rows.size() == 1)
        rows << QVariantList{QStringLiteral("1"), QStringLiteral("1"), sheet->partName().isEmpty() ? QStringLiteral("Part") : sheet->partName(),
                             sheet->material().isEmpty() ? QStringLiteral("-") : sheet->material()};
    return sheet->addTable({{"kind", "partsList"}, {"title", "Parts List"}, {"x", pos.x()}, {"y", pos.y()},
                            {"colWidths", QVariantList{12, 12, 40, 30}}, {"rows", rows}});
}

QString DrawingManager::addRevisionTable(DrawingSheet *sheet, const QPointF &pos)
{
    m_lastError.clear();
    if (!sheet) { m_lastError = "No sheet"; return {}; }
    QVariantList rows;
    rows << QVariantList{QStringLiteral("Rev"), QStringLiteral("Date"), QStringLiteral("Description"), QStringLiteral("By")};
    rows << QVariantList{sheet->revision(), sheet->date(), QStringLiteral("Initial release"), sheet->drawnBy()};
    return sheet->addTable({{"kind", "revision"}, {"title", "Revisions"}, {"x", pos.x()}, {"y", pos.y()},
                            {"colWidths", QVariantList{12, 24, 50, 20}}, {"rows", rows}});
}

QString DrawingManager::addBendTable(DrawingSheet *sheet, FissionDocument *design, const QPointF &pos)
{
    m_lastError.clear();
    if (!sheet) { m_lastError = "No sheet"; return {}; }
    QVariantList rows;
    rows << QVariantList{QStringLiteral("#"), QStringLiteral("Feature"), QStringLiteral("Angle"), QStringLiteral("Radius")};
    int n = 1;
    if (design) {
        for (FissionFeature *f : design->timeline()->features()) {
            if (!f) continue;
            const QString t = f->type();
            if (t != QLatin1String("EdgeFlange") && t != QLatin1String("Bend") && t != QLatin1String("ContourFlange")
                && t != QLatin1String("Hem"))
                continue;
            rows << QVariantList{QString::number(n++), t,
                                 f->get("angle").toString().isEmpty() ? QStringLiteral("-") : f->get("angle").toString(),
                                 f->get("radius").toString().isEmpty() ? QStringLiteral("-") : f->get("radius").toString()};
        }
    }
    if (rows.size() == 1)
        rows << QVariantList{QStringLiteral("—"), QStringLiteral("(no bends)"), QStringLiteral("-"), QStringLiteral("-")};
    return sheet->addTable({{"kind", "bend"}, {"title", "Bend Table"}, {"x", pos.x()}, {"y", pos.y()},
                            {"colWidths", QVariantList{10, 36, 24, 24}}, {"rows", rows}});
}

QString DrawingManager::addImage(DrawingSheet *sheet, const QString &path, const QPointF &pos, double widthMm)
{
    m_lastError.clear();
    if (!sheet) { m_lastError = "No sheet"; return {}; }
    const QString local = path.startsWith(QLatin1String("file:")) ? QUrl(path).toLocalFile() : path;
    if (!QFileInfo::exists(local)) { m_lastError = "Image not found"; return {}; }
    QImage img(local);
    if (img.isNull()) { m_lastError = "Could not read the image"; return {}; }
    const double aspect = img.height() > 0 ? double(img.height()) / double(img.width()) : 1.0;
    return sheet->addAnnotation({{"kind", "image"}, {"path", local}, {"x", pos.x()}, {"y", pos.y()},
                                 {"width", widthMm}, {"height", widthMm * aspect}});
}

QVariantMap DrawingManager::buildDimension(const QString &tool, const QVariantMap &a, const QVariantMap &b,
                                           double placeX, double placeY, double viewScale) const
{
    return DrawingDimension::build(tool, a, b, placeX, placeY, viewScale);
}

QVariantMap DrawingManager::dimensionGeometry(DrawingSheet *sheet, const QVariantMap &annotation) const
{
    QVariantMap g = DrawingDimension::geometry(sheetSpace(sheet, annotation));
    g["detached"] = annotation.value("detached").toBool();
    return g;
}

QVariantMap DrawingManager::attachDimension(DrawingSheet *sheet, const QVariantMap &annotation,
                                            const QVariantList &picks) const
{
    const DrawingView *v = viewOn(sheet, annotation.value("view").toString());
    if (!v) return annotation;   // not on a view: stays where it was placed on the paper
    return DrawingDimension::toView(annotation, picks, frameOf(v));
}

bool DrawingManager::moveDimension(DrawingSheet *sheet, const QString &id, double x, double y) const
{
    if (!sheet) return false;
    for (int i = 0; i < sheet->annotationCount(); ++i) {
        const QVariantMap a = sheet->annotation(i);
        if (a.value("id").toString() != id) continue;
        double px = x, py = y;
        if (a.value("space").toString() == QLatin1String("view")) {
            const DrawingView *v = viewOn(sheet, a.value("view").toString());
            if (!v) return false;
            const DrawingDimension::ViewFrame f = frameOf(v);
            px = (x - f.ox) / f.s;
            py = -(y - f.oy) / f.s;
        }
        return sheet->updateAnnotation(id, {{"px", px}, {"py", py}, {"x", px}, {"y", py}});
    }
    return false;
}

QString DrawingManager::dimensionLabel(const QVariantMap &annotation) const
{
    return DrawingDimension::label(annotation);
}

QString DrawingManager::dimensionAt(DrawingSheet *sheet, double x, double y, double tolMm) const
{
    if (!sheet) return {};
    QString best;
    double bestD = tolMm;
    for (int i = 0; i < sheet->annotationCount(); ++i) {
        const QVariantMap a = sheet->annotation(i);
        if (a.value("kind").toString() != QLatin1String("dimension")) continue;
        const double d = DrawingDimension::distanceTo(sheetSpace(sheet, a), x, y);
        if (d < bestD) { bestD = d; best = a.value("id").toString(); }
    }
    return best;
}

QVariantMap DrawingManager::pickSheetEdge(DrawingSheet *sheet, double x, double y, double tolMm) const
{
    if (!sheet) return {};
    // Points beat edges, then the nearest view whose frame holds the click
    QVariantMap edge;
    for (int i = 0; i < sheet->viewCount(); ++i) {
        DrawingView *view = sheet->view(i);
        if (!view) continue;
        const QVariantMap hit = view->pickEdge(x, y, tolMm);
        if (hit.isEmpty()) continue;
        if (hit.value("kind").toString() == QLatin1String("point")) {
            QVariantMap out = hit;
            out["viewScale"] = view->scale();
            return out;
        }
        if (edge.isEmpty()) { edge = hit; edge["viewScale"] = view->scale(); }
    }
    return edge;
}
