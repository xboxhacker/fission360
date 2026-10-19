// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#include "DocumentVersionStore.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <algorithm>

DocumentVersionStore *DocumentVersionStore::instance() {
    static DocumentVersionStore store;
    return &store;
}

DocumentVersionStore::DocumentVersionStore(QObject *parent) : QObject(parent) {}

void DocumentVersionStore::setMaxVersions(int n) {
    m_maxVersions = std::clamp(n, 1, 50);
}

QString DocumentVersionStore::versionsDirFor(const QString &designPath) const {
    if (designPath.isEmpty()) return {};
    return designPath + QStringLiteral(".versions");
}

QString DocumentVersionStore::autosaveDir() const {
    const QString root = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return QDir(root).filePath(QStringLiteral("Autosave"));
}

QString DocumentVersionStore::autosavePathFor(const QString &documentId) const {
    if (documentId.isEmpty()) return {};
    return QDir(autosaveDir()).filePath(documentId + QStringLiteral(".f360.autosave"));
}

int DocumentVersionStore::pushVersion(const QString &designPath, int versionNumber, const QByteArray &jsonUtf8,
                                      const QString &label) {
    if (designPath.isEmpty() || versionNumber < 1 || jsonUtf8.isEmpty()) return 0;
    const QString dir = versionsDirFor(designPath);
    if (!QDir().mkpath(dir)) return 0;
    const QString path = QDir(dir).filePath(QStringLiteral("v%1.f360z").arg(versionNumber, 4, 10, QChar('0')));
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return 0;
    // Lightweight: qCompress (zlib) — no second copy of the live file
    f.write(qCompress(jsonUtf8, 6));
    f.close();

    QJsonObject meta{{"version", versionNumber},
                     {"label", label},
                     {"savedAt", QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},
                     {"bytes", jsonUtf8.size()}};
    QFile mf(QDir(dir).filePath(QStringLiteral("v%1.meta.json").arg(versionNumber, 4, 10, QChar('0'))));
    if (mf.open(QIODevice::WriteOnly | QIODevice::Truncate))
        mf.write(QJsonDocument(meta).toJson(QJsonDocument::Compact));

    // Cap history
    const QVariantList all = listVersions(designPath);
    for (int i = 0; i < all.size() - m_maxVersions; ++i)
        deleteVersion(designPath, all[i].toMap().value(QStringLiteral("version")).toInt());
    return versionNumber;
}

QVariantList DocumentVersionStore::listVersions(const QString &designPath) const {
    QVariantList out;
    const QDir dir(versionsDirFor(designPath));
    if (!dir.exists()) return out;
    const QStringList metas = dir.entryList({QStringLiteral("v*.meta.json")}, QDir::Files, QDir::Name);
    for (const QString &name : metas) {
        QFile f(dir.filePath(name));
        if (!f.open(QIODevice::ReadOnly)) continue;
        const QJsonObject o = QJsonDocument::fromJson(f.readAll()).object();
        if (o.contains(QStringLiteral("version")))
            out.append(o.toVariantMap());
    }
    return out;
}

QByteArray DocumentVersionStore::loadVersion(const QString &designPath, int versionNumber) const {
    const QString path = QDir(versionsDirFor(designPath))
                             .filePath(QStringLiteral("v%1.f360z").arg(versionNumber, 4, 10, QChar('0')));
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return {};
    return qUncompress(f.readAll());
}

bool DocumentVersionStore::deleteVersion(const QString &designPath, int versionNumber) {
    const QDir dir(versionsDirFor(designPath));
    const QString stem = QStringLiteral("v%1").arg(versionNumber, 4, 10, QChar('0'));
    bool ok = QFile::remove(dir.filePath(stem + QStringLiteral(".f360z")));
    QFile::remove(dir.filePath(stem + QStringLiteral(".meta.json")));
    return ok;
}

bool DocumentVersionStore::writeAutosave(const QString &documentId, const QByteArray &jsonUtf8,
                                         const QString &displayName, const QString &originalPath) {
    if (documentId.isEmpty() || jsonUtf8.isEmpty()) return false;
    QDir().mkpath(autosaveDir());
    const QString path = autosavePathFor(documentId);
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    // Small header JSON line then compressed body — recovery without loading huge files twice
    QJsonObject header{{"documentId", documentId},
                       {"name", displayName},
                       {"originalPath", originalPath},
                       {"savedAt", QDateTime::currentDateTimeUtc().toString(Qt::ISODate)}};
    f.write(QJsonDocument(header).toJson(QJsonDocument::Compact));
    f.write("\n", 1);
    f.write(qCompress(jsonUtf8, 5));
    return true;
}

bool DocumentVersionStore::clearAutosave(const QString &documentId) {
    return QFile::remove(autosavePathFor(documentId));
}

QVariantList DocumentVersionStore::pendingAutosaves() const {
    QVariantList out;
    const QDir dir(autosaveDir());
    if (!dir.exists()) return out;
    for (const QFileInfo &fi : dir.entryInfoList({QStringLiteral("*.f360.autosave")}, QDir::Files)) {
        QFile f(fi.absoluteFilePath());
        if (!f.open(QIODevice::ReadOnly)) continue;
        const QByteArray line = f.readLine();
        const QJsonObject h = QJsonDocument::fromJson(line).object();
        out.append(QVariantMap{
            {"documentId", h.value(QStringLiteral("documentId")).toString()},
            {"name", h.value(QStringLiteral("name")).toString()},
            {"originalPath", h.value(QStringLiteral("originalPath")).toString()},
            {"savedAt", h.value(QStringLiteral("savedAt")).toString()},
            {"path", fi.absoluteFilePath()},
            {"modified", fi.lastModified().toString(Qt::ISODate)},
        });
    }
    return out;
}

QByteArray DocumentVersionStore::loadAutosave(const QString &documentId) const {
    QFile f(autosavePathFor(documentId));
    if (!f.open(QIODevice::ReadOnly)) return {};
    f.readLine(); // header
    return qUncompress(f.readAll());
}
