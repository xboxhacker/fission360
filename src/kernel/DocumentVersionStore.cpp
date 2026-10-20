// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#include "DocumentVersionStore.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
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

QString DocumentVersionStore::versionsDirFor(const QString &designPath) {
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

QJsonObject DocumentVersionStore::makeHistoryEntry(int versionNumber, const QByteArray &designJsonUtf8,
                                                   const QString &label) {
    if (versionNumber < 1 || designJsonUtf8.isEmpty()) return {};
    return QJsonObject{{QStringLiteral("version"), versionNumber},
                       {QStringLiteral("label"), label.isEmpty() ? QStringLiteral("v%1").arg(versionNumber) : label},
                       {QStringLiteral("savedAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},
                       {QStringLiteral("bytes"), designJsonUtf8.size()},
                       {QStringLiteral("payload"), QString::fromLatin1(qCompress(designJsonUtf8, 6).toBase64())}};
}

QByteArray DocumentVersionStore::payloadFromEntry(const QJsonObject &entry) {
    const QByteArray b64 = entry.value(QStringLiteral("payload")).toString().toLatin1();
    if (b64.isEmpty()) return {};
    return qUncompress(QByteArray::fromBase64(b64));
}

QVariantList DocumentVersionStore::historyMetaList(const QJsonArray &history) {
    QVariantList out;
    for (const QJsonValue &v : history) {
        const QJsonObject o = v.toObject();
        if (!o.contains(QStringLiteral("version"))) continue;
        out.append(QVariantMap{{QStringLiteral("version"), o.value(QStringLiteral("version")).toInt()},
                               {QStringLiteral("label"), o.value(QStringLiteral("label")).toString()},
                               {QStringLiteral("savedAt"), o.value(QStringLiteral("savedAt")).toString()},
                               {QStringLiteral("bytes"), o.value(QStringLiteral("bytes")).toInt()}});
    }
    return out;
}

bool DocumentVersionStore::migrateSidecarInto(const QString &designPath, QJsonArray &history, int maxVersions) {
    const QDir dir(versionsDirFor(designPath));
    if (!dir.exists()) return false;
    QSet<int> have;
    for (const QJsonValue &v : history)
        have.insert(v.toObject().value(QStringLiteral("version")).toInt());

    bool imported = false;
    const QStringList metas = dir.entryList({QStringLiteral("v*.meta.json")}, QDir::Files, QDir::Name);
    for (const QString &name : metas) {
        QFile mf(dir.filePath(name));
        if (!mf.open(QIODevice::ReadOnly)) continue;
        const QJsonObject meta = QJsonDocument::fromJson(mf.readAll()).object();
        const int ver = meta.value(QStringLiteral("version")).toInt();
        if (ver < 1 || have.contains(ver)) continue;
        const QString stem = QStringLiteral("v%1").arg(ver, 4, 10, QChar('0'));
        QFile zf(dir.filePath(stem + QStringLiteral(".f360z")));
        if (!zf.open(QIODevice::ReadOnly)) continue;
        const QByteArray raw = qUncompress(zf.readAll());
        if (raw.isEmpty()) continue;
        QJsonObject entry = makeHistoryEntry(ver, raw, meta.value(QStringLiteral("label")).toString());
        if (meta.contains(QStringLiteral("savedAt")))
            entry.insert(QStringLiteral("savedAt"), meta.value(QStringLiteral("savedAt")));
        history.append(entry);
        have.insert(ver);
        imported = true;
    }
    // Keep chronological order and cap
    QList<QJsonObject> sorted;
    for (const QJsonValue &v : history) sorted.append(v.toObject());
    std::sort(sorted.begin(), sorted.end(), [](const QJsonObject &a, const QJsonObject &b) {
        return a.value(QStringLiteral("version")).toInt() < b.value(QStringLiteral("version")).toInt();
    });
    while (sorted.size() > maxVersions) sorted.removeFirst();
    history = QJsonArray();
    for (const QJsonObject &o : sorted) history.append(o);
    return imported;
}

bool DocumentVersionStore::removeSidecar(const QString &designPath) {
    const QString dir = versionsDirFor(designPath);
    if (dir.isEmpty() || !QDir(dir).exists()) return false;
    return QDir(dir).removeRecursively();
}

bool DocumentVersionStore::writeAutosave(const QString &documentId, const QByteArray &jsonUtf8,
                                         const QString &displayName, const QString &originalPath) {
    if (documentId.isEmpty() || jsonUtf8.isEmpty()) return false;
    QDir().mkpath(autosaveDir());
    const QString path = autosavePathFor(documentId);
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
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
