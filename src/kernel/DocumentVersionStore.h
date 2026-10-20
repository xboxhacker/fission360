// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#pragma once
#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <QJsonArray>

// Crash autosave under AppData, plus helpers for version history embedded inside .f360 files.
// Older builds wrote "<path>.f360.versions/" sidecars — migrateSidecarIntoFile() folds those in.
class DocumentVersionStore : public QObject {
    Q_OBJECT
public:
    static DocumentVersionStore *instance();

    Q_INVOKABLE QString autosaveDir() const;
    Q_INVOKABLE QString autosavePathFor(const QString &documentId) const;

    // Keep at most this many named versions per design (oldest dropped). Default 8.
    Q_INVOKABLE int maxVersions() const { return m_maxVersions; }
    Q_INVOKABLE void setMaxVersions(int n);

    // Pack / unpack a design snapshot for the embedded "history" array (no nested history)
    static QJsonObject makeHistoryEntry(int versionNumber, const QByteArray &designJsonUtf8,
                                        const QString &label = QString());
    static QByteArray payloadFromEntry(const QJsonObject &entry);
    static QVariantList historyMetaList(const QJsonArray &history);   // UI rows without payload

    // Import legacy "<design>.f360.versions/" into an in-memory history array; returns true if any imported.
    // Does not delete the sidecar until removeSidecar() is called after a successful save.
    static bool migrateSidecarInto(const QString &designPath, QJsonArray &history, int maxVersions);
    static QString versionsDirFor(const QString &designPath);
    static bool removeSidecar(const QString &designPath);

    Q_INVOKABLE bool writeAutosave(const QString &documentId, const QByteArray &jsonUtf8,
                                   const QString &displayName, const QString &originalPath);
    Q_INVOKABLE bool clearAutosave(const QString &documentId);
    // [{documentId, name, originalPath, path, modified}]
    Q_INVOKABLE QVariantList pendingAutosaves() const;
    Q_INVOKABLE QByteArray loadAutosave(const QString &documentId) const;

private:
    explicit DocumentVersionStore(QObject *parent = nullptr);
    int m_maxVersions = 8;
};
