// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#pragma once
#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

// Sidecar version history + crash autosave for .f360 designs.
// Versions live next to the file in "<name>.f360.versions/" (capped, compressed) so the
// main file stays small and low-end disks aren't filled. Autosave lives under AppData.
class DocumentVersionStore : public QObject {
    Q_OBJECT
public:
    static DocumentVersionStore *instance();

    // Directory next to a saved design that holds v0001.f360 … (qCompress payloads)
    Q_INVOKABLE QString versionsDirFor(const QString &designPath) const;
    Q_INVOKABLE QString autosaveDir() const;
    Q_INVOKABLE QString autosavePathFor(const QString &documentId) const;

    // Keep at most this many named versions per design (oldest dropped). Default 8 — light on disk.
    Q_INVOKABLE int maxVersions() const { return m_maxVersions; }
    Q_INVOKABLE void setMaxVersions(int n);

    // Write a version snapshot (compressed JSON bytes). Returns version number or 0 on failure.
    Q_INVOKABLE int pushVersion(const QString &designPath, int versionNumber, const QByteArray &jsonUtf8,
                                const QString &label = QString());
    Q_INVOKABLE QVariantList listVersions(const QString &designPath) const;
    // Load snapshot JSON for revert; empty on failure
    Q_INVOKABLE QByteArray loadVersion(const QString &designPath, int versionNumber) const;
    Q_INVOKABLE bool deleteVersion(const QString &designPath, int versionNumber);

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
