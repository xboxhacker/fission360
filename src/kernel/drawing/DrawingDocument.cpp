// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#include "DrawingDocument.h"
#include <QMap>
#include <QSizeF>
#include <QUuid>
#include <algorithm>

DrawingDocument::DrawingDocument(const QString &id, const QString &name,
                                 const QString &designId, const QString &standard,
                                 const QString &projectionAngle, QObject *parent)
    : QObject(parent)
    , m_id(id)
    , m_name(name)
    , m_designId(designId)
    , m_standard(standard)
    , m_projectionAngle(projectionAngle)
{
}

void DrawingDocument::setName(const QString &name)
{
    if (m_name != name) {
        m_name = name;
        emit nameChanged();
    }
}

DrawingSheet *DrawingDocument::sheet(int index) const
{
    if (index < 0 || index >= m_sheets.size()) return nullptr;
    return m_sheets[index];
}

DrawingSheet *DrawingDocument::makeSheet(const QString &name, const QString &size, double width,
                                         double height, const QString &orientation)
{
    QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    auto *s = new DrawingSheet(id, name, size, width, height, orientation, 1.0, this);
    m_sheets.append(s);
    emit sheetsChanged();
    return s;
}

DrawingSheet *DrawingDocument::addSheet(const QString &name, const QString &size, const QString &orientation)
{
    static const QMap<QString, QSizeF> sheetSizes = {
        {"A0", QSizeF(841, 1189)}, {"A1", QSizeF(594, 841)},
        {"A2", QSizeF(420, 594)},  {"A3", QSizeF(297, 420)},
        {"A4", QSizeF(210, 297)},  {"A",  QSizeF(216, 279)},
        {"B",  QSizeF(279, 432)},  {"C",  QSizeF(432, 559)},
        {"D",  QSizeF(559, 864)},  {"E",  QSizeF(864, 1118)}
    };

    QSizeF dims = sheetSizes.value(size, QSizeF(297, 420));
    const bool landscape = (orientation == "landscape");
    const double shortSide = std::min(dims.width(), dims.height());
    const double longSide = std::max(dims.width(), dims.height());
    return makeSheet(name, size,
                     landscape ? longSide : shortSide,
                     landscape ? shortSide : longSide,
                     orientation);
}

DrawingSheet *DrawingDocument::addCustomSheet(const QString &name, double widthMm, double heightMm,
                                              const QString &orientation)
{
    double w = std::max(10.0, widthMm), h = std::max(10.0, heightMm);
    if (orientation == "landscape" && w < h) std::swap(w, h);
    if (orientation == "portrait" && h < w) std::swap(w, h);
    return makeSheet(name, QStringLiteral("custom"), w, h, orientation);
}

bool DrawingDocument::removeSheet(const QString &sheetId)
{
    for (int i = 0; i < m_sheets.size(); ++i) {
        if (m_sheets[i]->id() == sheetId) {
            m_sheets[i]->deleteLater();
            m_sheets.removeAt(i);
            if (m_activeSheet >= m_sheets.size())
                m_activeSheet = std::max(0, int(m_sheets.size()) - 1);
            emit sheetsChanged();
            emit activeSheetChanged();
            return true;
        }
    }
    return false;
}

void DrawingDocument::setActiveSheetIndex(int index)
{
    if (index < 0 || index >= m_sheets.size() || index == m_activeSheet) return;
    m_activeSheet = index;
    emit activeSheetChanged();
}

void DrawingDocument::pushHistory(const QString &entry)
{
    if (entry.isEmpty()) return;
    m_history.append(entry);
    emit historyChanged();
}

void DrawingDocument::setSourceDesignRevision(qint64 rev)
{
    if (m_sourceRevision == rev && !m_outOfDate) return;
    m_sourceRevision = rev;
    m_outOfDate = false;
    emit syncChanged();
}

void DrawingDocument::refreshOutOfDate(qint64 designContentRevision)
{
    const bool stale = (m_sourceRevision != designContentRevision) || m_sourceRevision == 0;
    if (m_outOfDate == stale) return;
    m_outOfDate = stale;
    emit syncChanged();
}
