// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#pragma once
#include <QObject>
#include <QString>
#include <QList>
#include <QByteArray>
#include <QJsonObject>
#include <QVariantMap>
#include <QQmlEngine>
#include "DrawingSheet.h"

// A drawing document — one or more sheets with 2D projections of a design.
// Fusion API equivalent: adsk::drawing::DrawingDocument
class DrawingDocument : public QObject {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QString id READ id CONSTANT)
    Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)
    Q_PROPERTY(QString referencedDesignId READ referencedDesignId CONSTANT)
    // Design contentRevision this drawing was last updated against
    Q_PROPERTY(qint64 sourceDesignRevision READ sourceDesignRevision NOTIFY syncChanged)
    Q_PROPERTY(bool outOfDate READ outOfDate NOTIFY syncChanged)
    Q_PROPERTY(int sheetCount READ sheetCount NOTIFY sheetsChanged)
    Q_PROPERTY(QString standard READ standard CONSTANT)
    Q_PROPERTY(QString projectionAngle READ projectionAngle CONSTANT)
    Q_PROPERTY(int activeSheetIndex READ activeSheetIndex WRITE setActiveSheetIndex NOTIFY activeSheetChanged)
    // Lightweight drawing history (view / table / annotation actions) for the drawing browser timeline
    Q_PROPERTY(QStringList history READ history NOTIFY historyChanged)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY undoChanged)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY undoChanged)

public:
    explicit DrawingDocument(const QString &id, const QString &name,
                             const QString &designId, const QString &standard,
                             const QString &projectionAngle, QObject *parent = nullptr);

    QString id() const { return m_id; }
    QString name() const { return m_name; }
    void setName(const QString &name);
    QString referencedDesignId() const { return m_designId; }
    qint64 sourceDesignRevision() const { return m_sourceRevision; }
    Q_INVOKABLE void setSourceDesignRevision(qint64 rev);
    // True when design.contentRevision != sourceDesignRevision (pass the live design revision)
    bool outOfDate() const { return m_outOfDate; }
    Q_INVOKABLE void refreshOutOfDate(qint64 designContentRevision);
    QString standard() const { return m_standard; }
    QString projectionAngle() const { return m_projectionAngle; }

    int sheetCount() const { return m_sheets.size(); }
    Q_INVOKABLE DrawingSheet *sheet(int index) const;
    Q_INVOKABLE DrawingSheet *addSheet(const QString &name, const QString &size, const QString &orientation);
    Q_INVOKABLE DrawingSheet *addCustomSheet(const QString &name, double widthMm, double heightMm,
                                             const QString &orientation);
    Q_INVOKABLE bool removeSheet(const QString &sheetId);

    int activeSheetIndex() const { return m_activeSheet; }
    void setActiveSheetIndex(int index);

    QStringList history() const { return m_history; }
    Q_INVOKABLE void pushHistory(const QString &entry);

    // One undo step. beginEdit snapshots the sheet; nested calls share that snapshot.
    bool canUndo() const { return !m_undo.isEmpty(); }
    bool canRedo() const { return !m_redo.isEmpty(); }
    Q_INVOKABLE void beginEdit();
    Q_INVOKABLE void endEdit();
    Q_INVOKABLE bool undo();
    Q_INVOKABLE bool redo();

    QJsonObject toJson() const;
    static DrawingDocument *fromJson(const QJsonObject &obj, QObject *parent = nullptr);

signals:
    void nameChanged();
    void sheetsChanged();
    void activeSheetChanged();
    void historyChanged();
    void syncChanged();
    void undoChanged();

private:
    DrawingSheet *makeSheet(const QString &name, const QString &size, double width, double height,
                            const QString &orientation);
    void pushUndoSnapshot();
    bool applySnapshot(const QByteArray &bytes);

    QString m_id;
    QString m_name;
    QString m_designId;
    qint64 m_sourceRevision = 0;
    bool m_outOfDate = false;
    QString m_standard;
    QString m_projectionAngle;
    QList<DrawingSheet *> m_sheets;
    int m_activeSheet = 0;
    QStringList m_history;
    QList<QByteArray> m_undo;
    QList<QByteArray> m_redo;
    int m_editDepth = 0;
    bool m_applying = false;
};
