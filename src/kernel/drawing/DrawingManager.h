// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#pragma once
#include <QObject>
#include <QString>
#include <QJsonArray>
#include <QVariantMap>
#include <QQmlEngine>
#include "DrawingDocument.h"

class FissionDocument;
class DrawingView;
class DrawingSheet;

// Entry point for the Drawing workspace.
// Fusion API equivalent: adsk::fusion::DrawingManager
class DrawingManager : public QObject {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(int drawingCount READ drawingCount NOTIFY drawingsChanged)
public:
    explicit DrawingManager(QObject *parent = nullptr);

    Q_INVOKABLE QVariantMap createDrawingInput(const QString &designDocId);
    Q_INVOKABLE QString createDrawingDocument(const QVariantMap &input);

    int drawingCount() const { return m_drawings.size(); }
    Q_INVOKABLE DrawingDocument *drawing(int index) const;
    Q_INVOKABLE DrawingDocument *drawingById(const QString &id) const;
    Q_INVOKABLE bool removeDrawing(const QString &id);

    // Drawings whose referenced design is designId (empty id includes every open drawing).
    QJsonArray toJsonForDesign(const QString &designId) const;
    // Replace drawings that share an id. Entries without an id are skipped.
    void restoreFromJson(const QJsonArray &drawings);

    // Project the active design onto a view (base / projected / section / detail / auxiliary).
    Q_INVOKABLE bool projectView(DrawingView *view, FissionDocument *design, DrawingDocument *drawingDoc = nullptr);

    // Regenerate every view on a sheet from the design.
    Q_INVOKABLE int regenerateSheet(DrawingSheet *sheet, FissionDocument *design, DrawingDocument *drawingDoc = nullptr);
    // Rebuild all sheets from the design and stamp sourceDesignRevision (clears out-of-date).
    Q_INVOKABLE int updateDrawingFromDesign(DrawingDocument *drawingDoc, FissionDocument *design);
    // Refresh outOfDate flags for every open drawing against the design's contentRevision.
    Q_INVOKABLE void refreshDrawingSync(FissionDocument *design);

    // Export the active sheet to PDF or DXF (paper coordinates in mm).
    Q_INVOKABLE bool exportPdf(DrawingSheet *sheet, const QString &path);
    Q_INVOKABLE bool exportDxf(DrawingSheet *sheet, const QString &path);
    Q_INVOKABLE bool exportPng(DrawingSheet *sheet, const QString &path, int dpi = 150);
    Q_INVOKABLE bool exportTiff(DrawingSheet *sheet, const QString &path, int dpi = 150);

    // Build a parts-list / revision / bend table and place it on the sheet.
    Q_INVOKABLE QString addPartsList(DrawingSheet *sheet, FissionDocument *design, const QPointF &pos = QPointF(20, 20));
    Q_INVOKABLE QString addRevisionTable(DrawingSheet *sheet, const QPointF &pos = QPointF(20, 20));
    Q_INVOKABLE QString addBendTable(DrawingSheet *sheet, FissionDocument *design, const QPointF &pos = QPointF(20, 20));
    Q_INVOKABLE QString addImage(DrawingSheet *sheet, const QString &path, const QPointF &pos, double widthMm = 40);

    // Dimensions (DrawingDimension): build one from picks, what to draw for it, its text, and the one
    // nearest a sheet point within tolMm ("" when none)
    Q_INVOKABLE QVariantMap buildDimension(const QString &tool, const QVariantMap &a, const QVariantMap &b,
                                           double placeX, double placeY, double viewScale) const;
    Q_INVOKABLE QVariantMap dimensionGeometry(DrawingSheet *sheet, const QVariantMap &annotation) const;
    // A dimension built from picks on one view, attached to it: stored in that view's model mm so it moves
    // with the view and is re-measured when the view is projected again
    Q_INVOKABLE QVariantMap attachDimension(DrawingSheet *sheet, const QVariantMap &annotation,
                                            const QVariantList &picks) const;
    // Drag a dimension's line / text to a sheet point
    Q_INVOKABLE bool moveDimension(DrawingSheet *sheet, const QString &id, double x, double y) const;
    Q_INVOKABLE QString dimensionLabel(const QVariantMap &annotation) const;
    Q_INVOKABLE QString dimensionAt(DrawingSheet *sheet, double x, double y, double tolMm) const;
    // The model edge or point under a sheet point, across all views of the sheet (DrawingView::pickEdge)
    Q_INVOKABLE QVariantMap pickSheetEdge(DrawingSheet *sheet, double x, double y, double tolMm) const;
    Q_INVOKABLE QString lastError() const { return m_lastError; }

signals:
    void drawingsChanged();

private:
    QList<DrawingDocument *> m_drawings;
    QString m_lastError;
};
