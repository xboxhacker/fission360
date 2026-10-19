// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
// Linked designs: the bodies of another saved design inserted as an ImportCAD feature that remembers
// where they came from. props["link"] = {path, documentId, revision, name}; an unlinked insert drops
// "link" and keeps "linkedFrom" (the source name) for the browser.
#include "FissionDocument.h"
#include "FissionBody.h"
#include "FissionFeature.h"
#include "FissionImport.h"
#include "FissionTimeline.h"
#include "MeshUtils.h"

#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>

#include <BRep_Builder.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS_Compound.hxx>

namespace {

struct FileInfoCache {
    QDateTime modified;
    qint64 size = -1;
    QVariantMap info;
};

// Compact JSON writes keys in sorted order, and the ones we need ("contentRevision", "documentId") come
// before the large "timeline", so the head of the file is enough for all but huge body lists
QByteArray valueAfter(const QByteArray &data, const QByteArray &key) {
    const QByteArray needle = "\"" + key + "\":";
    const qsizetype at = data.indexOf(needle);
    if (at < 0) return {};
    qsizetype i = at + needle.size();
    if (i < data.size() && data[i] == '"') {
        const qsizetype end = data.indexOf('"', i + 1);
        return end < 0 ? QByteArray() : data.mid(i + 1, end - i - 1);
    }
    qsizetype end = i;
    while (end < data.size() && (QChar(data[end]).isDigit() || data[end] == '-')) ++end;
    return data.mid(i, end - i);
}

QVariantMap readInfo(const QString &path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return {};
    QByteArray head = f.read(8 << 20);
    QByteArray rev = valueAfter(head, "contentRevision"), id = valueAfter(head, "documentId");
    if ((rev.isEmpty() || id.isEmpty()) && !f.atEnd() && f.size() < (256 << 20)) {
        head += f.readAll();
        const QJsonObject root = QJsonDocument::fromJson(head).object();
        rev = QByteArray::number(root.value("contentRevision").toVariant().toLongLong());
        id = root.value("documentId").toString().toUtf8();
    }
    if (rev.isEmpty()) return {};
    return {{"documentId", QString::fromUtf8(id)},
            {"contentRevision", rev.toLongLong()},
            {"name", QFileInfo(path).completeBaseName()}};
}

// Every visible B-rep body of a design (all of them when none is visible), with names and colours in
// the order ImportCAD replays the solids
bool collectBodies(FissionDocument &src, TopoDS_Shape &out, QVariantList &parts, QString &error) {
    QList<FissionBody *> bodies;
    for (QObject *o : src.bodyObjects()) {
        auto *b = qobject_cast<FissionBody *>(o);
        if (b && !b->shape().IsNull() && !MeshUtils::isMesh(b->shape())) bodies.append(b);
    }
    QList<FissionBody *> visible;
    for (auto *b : bodies)
        if (b->isVisible()) visible.append(b);
    if (!visible.isEmpty()) bodies = visible;
    if (bodies.isEmpty()) { error = QStringLiteral("The design has no solid or surface bodies to insert"); return false; }

    BRep_Builder bb;
    TopoDS_Compound all, loose;
    bb.MakeCompound(all);
    bb.MakeCompound(loose);
    bool anyLoose = false;
    for (auto *b : bodies) {
        int k = 0;
        for (TopExp_Explorer e(b->shape(), TopAbs_SOLID); e.More(); e.Next(), ++k) {
            bb.Add(all, e.Current());
            parts << QVariantMap{{"name", k ? QString("%1 (%2)").arg(b->name()).arg(k + 1) : b->name()},
                                 {"color", b->color().name()}};
        }
        if (!k) { bb.Add(loose, b->shape()); anyLoose = true; }
    }
    if (anyLoose) bb.Add(all, loose);
    out = all;
    return true;
}

}  // namespace

QVariantMap FissionDocument::designFileInfo(const QString &path) {
    static QHash<QString, FileInfoCache> cache;
    const QFileInfo fi(path);
    if (!fi.exists()) { cache.remove(path); return {}; }
    FileInfoCache &c = cache[fi.absoluteFilePath()];
    if (c.modified != fi.lastModified() || c.size != fi.size()) {
        c.modified = fi.lastModified();
        c.size = fi.size();
        c.info = readInfo(path);
    }
    return c.info;
}

QString FissionDocument::insertLinkedDesign(const QString &path) {
    const QFileInfo fi(path);
    if (!fi.exists()) { m_lastError = "File not found: " + path; return {}; }
    if (!m_filePath.isEmpty() && QFileInfo(m_filePath) == fi) {
        m_lastError = QStringLiteral("A design cannot be inserted into itself");
        return {};
    }
    FissionDocument src;
    if (!src.load(fi.absoluteFilePath())) { m_lastError = "Could not open " + fi.fileName() + ": " + src.lastError(); return {}; }
    if (src.documentId() == m_documentId) {
        m_lastError = QStringLiteral("A design cannot be inserted into itself");
        return {};
    }
    TopoDS_Shape shape;
    QVariantList parts;
    if (!collectBodies(src, shape, parts, m_lastError)) return {};

    const int before = bodyCount();
    const QVariantMap link{{"path", fi.absoluteFilePath()}, {"documentId", src.documentId()},
                           {"revision", src.contentRevision()}, {"name", fi.completeBaseName()}};
    FissionFeature *f = addFeature("ImportCAD", {{"data", FissionImportCAD::encode(shape)}, {"fileName", fi.fileName()},
                                                 {"format", "Fission 360"}, {"parts", parts}, {"link", link}},
                                   fi.completeBaseName());
    if (!f) return {};
    if (!f->error().isEmpty()) m_lastError = f->error();
    for (int i = 0; i < parts.size() && before + i < bodyCount(); ++i) {
        const QColor c(parts[i].toMap().value("color").toString());
        if (c.isValid()) body(before + i)->setColor(c);
    }
    emit linkedDesignsChanged();
    return f->id();
}

QVariantList FissionDocument::linkedDesigns() const {
    QVariantList out;
    for (auto *f : m_timeline->features()) {
        if (f->type() != QLatin1String("ImportCAD")) continue;
        const QVariantMap link = f->get("link").toMap();
        if (link.isEmpty()) continue;
        const QString path = link.value("path").toString();
        const QVariantMap info = designFileInfo(path);
        const bool missing = info.isEmpty();
        const qint64 rev = link.value("revision").toLongLong();
        const qint64 srcRev = info.value("contentRevision").toLongLong();
        const QString srcId = info.value("documentId").toString();
        out << QVariantMap{{"featureId", f->id()},
                           {"name", f->name()},
                           {"sourceName", link.value("name")},
                           {"path", path},
                           {"revision", rev},
                           {"sourceRevision", srcRev},
                           {"missing", missing},
                           {"outOfDate", !missing && (srcRev != rev
                                                      || (!srcId.isEmpty() && srcId != link.value("documentId").toString()))}};
    }
    return out;
}

bool FissionDocument::updateLinkedDesign(const QString &featureId) {
    FissionFeature *f = featureById(featureId);
    if (!f) { m_lastError = QStringLiteral("The linked insert no longer exists"); return false; }
    QVariantMap link = f->get("link").toMap();
    if (link.isEmpty()) { m_lastError = QStringLiteral("This insert is not linked"); return false; }
    FissionDocument src;
    const QString path = link.value("path").toString();
    if (!src.load(path)) { m_lastError = "Could not open " + QFileInfo(path).fileName() + ": " + src.lastError(); return false; }
    TopoDS_Shape shape;
    QVariantList parts;
    if (!collectBodies(src, shape, parts, m_lastError)) return false;

    checkpoint();
    link["revision"] = src.contentRevision();
    link["documentId"] = src.documentId();
    m_restoring = true;   // one rebuild below instead of one per property
    f->set("data", FissionImportCAD::encode(shape));
    f->set("parts", parts);
    f->set("link", link);
    m_restoring = false;
    setModified(true);
    rebuild();
    int k = 0;
    for (auto *b : m_bodies) {
        if (b->sourceFeature() != featureId) continue;
        const QColor c(parts.value(k++).toMap().value("color").toString());
        if (c.isValid()) b->setColor(c);
    }
    emit linkedDesignsChanged();
    if (!f->error().isEmpty()) { m_lastError = f->error(); return false; }
    return true;
}

void FissionDocument::unlinkDesign(const QString &featureId) {
    FissionFeature *f = featureById(featureId);
    if (!f) return;
    const QVariantMap link = f->get("link").toMap();
    if (link.isEmpty()) return;
    checkpoint();
    QVariantMap props = f->props();
    props.remove("link");
    props.insert("linkedFrom", link.value("name"));
    f->m_props = props;
    emit f->changed();
    setModified(true);
    emit linkedDesignsChanged();
}
