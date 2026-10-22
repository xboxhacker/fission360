// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#include "DrawingDocument.h"
#include <QJsonArray>
#include <QJsonDocument>
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

void DrawingDocument::pushUndoSnapshot()
{
    const QByteArray bytes = QJsonDocument(toJson()).toJson(QJsonDocument::Compact);
    if (!m_undo.isEmpty() && m_undo.last() == bytes) return;
    m_undo.append(bytes);
    while (m_undo.size() > 40) m_undo.removeFirst();
    if (!m_redo.isEmpty()) m_redo.clear();
    emit undoChanged();
}

void DrawingDocument::beginEdit()
{
    if (m_applying) return;
    if (m_editDepth++ == 0) pushUndoSnapshot();
}

void DrawingDocument::endEdit()
{
    if (m_editDepth > 0) --m_editDepth;
}

bool DrawingDocument::applySnapshot(const QByteArray &bytes)
{
    const QJsonObject obj = QJsonDocument::fromJson(bytes).object();
    if (obj.value(QStringLiteral("id")).toString() != m_id) return false;
    QList<DrawingSheet *> next;
    for (const QJsonValue &v : obj.value(QStringLiteral("sheets")).toArray()) {
        if (DrawingSheet *sheet = DrawingSheet::fromJson(v.toObject(), this))
            next.append(sheet);
    }
    const QJsonArray sheetJson = obj.value(QStringLiteral("sheets")).toArray();
    if (next.isEmpty() && !sheetJson.isEmpty()) {
        qDeleteAll(next);
        return false;
    }

    m_applying = true;
    const QList<DrawingSheet *> old = m_sheets;
    m_sheets = next;
    if (!m_sheets.isEmpty())
        m_activeSheet = std::clamp(obj.value(QStringLiteral("activeSheet")).toInt(0), 0, int(m_sheets.size()) - 1);
    else
        m_activeSheet = 0;
    const QString name = obj.value(QStringLiteral("name")).toString(m_name);
    if (m_name != name) m_name = name;
    if (obj.contains(QStringLiteral("sourceDesignRevision")))
        m_sourceRevision = obj.value(QStringLiteral("sourceDesignRevision")).toVariant().toLongLong();
    m_history.clear();
    for (const QJsonValue &v : obj.value(QStringLiteral("history")).toArray()) {
        const QString entry = v.toString();
        if (!entry.isEmpty()) m_history.append(entry);
    }
    emit sheetsChanged();
    emit activeSheetChanged();
    emit nameChanged();
    emit historyChanged();
    emit syncChanged();
    for (DrawingSheet *sheet : old) sheet->deleteLater();
    m_applying = false;
    return true;
}

bool DrawingDocument::undo()
{
    if (m_undo.isEmpty() || m_applying || m_editDepth > 0) return false;
    const QByteArray current = QJsonDocument(toJson()).toJson(QJsonDocument::Compact);
    QByteArray snap = m_undo.takeLast();
    while (!m_undo.isEmpty() && snap == current) snap = m_undo.takeLast();
    m_redo.append(current);
    while (m_redo.size() > 40) m_redo.removeFirst();
    const bool ok = applySnapshot(snap);
    if (!ok) m_undo.append(snap);
    emit undoChanged();
    return ok;
}

bool DrawingDocument::redo()
{
    if (m_redo.isEmpty() || m_applying || m_editDepth > 0) return false;
    const QByteArray current = QJsonDocument(toJson()).toJson(QJsonDocument::Compact);
    const QByteArray snap = m_redo.takeLast();
    m_undo.append(current);
    while (m_undo.size() > 40) m_undo.removeFirst();
    const bool ok = applySnapshot(snap);
    if (!ok) m_redo.append(snap);
    emit undoChanged();
    return ok;
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

QJsonObject DrawingDocument::toJson() const
{
    QJsonArray sheets;
    for (DrawingSheet *s : m_sheets)
        if (s) sheets.append(s->toJson());
    QJsonArray history;
    for (const QString &entry : m_history) history.append(entry);
    return QJsonObject{
        {QStringLiteral("id"), m_id},
        {QStringLiteral("name"), m_name},
        {QStringLiteral("designId"), m_designId},
        {QStringLiteral("sourceDesignRevision"), m_sourceRevision},
        {QStringLiteral("standard"), m_standard},
        {QStringLiteral("projectionAngle"), m_projectionAngle},
        {QStringLiteral("activeSheet"), m_activeSheet},
        {QStringLiteral("history"), history},
        {QStringLiteral("sheets"), sheets},
    };
}

DrawingDocument *DrawingDocument::fromJson(const QJsonObject &obj, QObject *parent)
{
    const QString id = obj.value(QStringLiteral("id")).toString();
    if (id.isEmpty()) return nullptr;
    auto *doc = new DrawingDocument(id,
                                   obj.value(QStringLiteral("name")).toString(QStringLiteral("Drawing")),
                                   obj.value(QStringLiteral("designId")).toString(),
                                   obj.value(QStringLiteral("standard")).toString(QStringLiteral("ISO")),
                                   obj.value(QStringLiteral("projectionAngle")).toString(QStringLiteral("third")),
                                   parent);
    if (obj.contains(QStringLiteral("sourceDesignRevision")))
        doc->setSourceDesignRevision(obj.value(QStringLiteral("sourceDesignRevision")).toVariant().toLongLong());
    for (const QJsonValue &v : obj.value(QStringLiteral("sheets")).toArray()) {
        if (DrawingSheet *sheet = DrawingSheet::fromJson(v.toObject(), doc))
            doc->m_sheets.append(sheet);
    }
    if (!doc->m_sheets.isEmpty()) {
        const int active = obj.value(QStringLiteral("activeSheet")).toInt(0);
        doc->m_activeSheet = std::clamp(active, 0, int(doc->m_sheets.size()) - 1);
    }
    for (const QJsonValue &v : obj.value(QStringLiteral("history")).toArray()) {
        const QString entry = v.toString();
        if (!entry.isEmpty()) doc->m_history.append(entry);
    }
    return doc;
}
