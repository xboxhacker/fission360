// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#pragma once
#include <QObject>
#include <QString>
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

    Q_INVOKABLE QString lastError() const { return m_lastError; }

signals:
    void drawingsChanged();

private:
    QList<DrawingDocument *> m_drawings;
    QString m_lastError;
};
