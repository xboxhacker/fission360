// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#include "RenderAssetStore.h"
#include "api/FissionApplication.h"

#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QNetworkRequest>
#include <QStandardPaths>
#include <QUrl>

namespace {

QString assetsRoot() {
    return QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)).filePath(QStringLiteral("Assets"));
}

qint64 approxHdriBytes(const QString &resolution) {
    if (resolution == "1k") return 2LL * 1024 * 1024;
    if (resolution == "2k") return 8LL * 1024 * 1024;
    if (resolution == "4k") return 30LL * 1024 * 1024;
    if (resolution == "8k") return 100LL * 1024 * 1024;
    return 0;
}

} // namespace

RenderAssetStore *RenderAssetStore::instance() {
    static RenderAssetStore store;
    return &store;
}

RenderAssetStore::RenderAssetStore(QObject *parent) : QObject(parent) {
    m_root = assetsRoot();
    QDir().mkpath(QDir(m_root).filePath(QStringLiteral("HDRIs")));
    QDir().mkpath(QDir(m_root).filePath(QStringLiteral("Materials")));
    QDir().mkpath(QDir(m_root).filePath(QStringLiteral("Previews")));
}

QVariantList RenderAssetStore::hdriResolutions() const {
    return QVariantList{
        QVariantMap{{"id", "1k"}, {"label", "1K (~2 MB)"}, {"bytes", approxHdriBytes("1k")}},
        QVariantMap{{"id", "2k"}, {"label", "2K (~8 MB)"}, {"bytes", approxHdriBytes("2k")}},
        QVariantMap{{"id", "4k"}, {"label", "4K (~30 MB)"}, {"bytes", approxHdriBytes("4k")}},
        QVariantMap{{"id", "8k"}, {"label", "8K (~100 MB)"}, {"bytes", approxHdriBytes("8k")}},
    };
}

QString RenderAssetStore::hdriLocalPath(const QString &id, const QString &resolution) const {
    return QDir(m_root).filePath(QStringLiteral("HDRIs/%1_%2.hdr").arg(id, resolution));
}

bool RenderAssetStore::isHdriCached(const QString &id, const QString &resolution) const {
    const QFileInfo fi(hdriLocalPath(id, resolution));
    return fi.isFile() && fi.size() > 1024;
}

qint64 RenderAssetStore::hdriApproxBytes(const QString &, const QString &resolution) const {
    return approxHdriBytes(resolution);
}

QString RenderAssetStore::hdriDownloadUrl(const QString &id, const QString &resolution) const {
    return QStringLiteral("https://dl.polyhaven.org/file/ph-assets/HDRIs/hdr/%1/%2_%1.hdr").arg(resolution, id);
}

QString RenderAssetStore::hdriPreviewLocalPath(const QString &id) const {
    return QDir(m_root).filePath(QStringLiteral("Previews/%1.jpg").arg(id));
}

bool RenderAssetStore::isHdriPreviewCached(const QString &id) const {
    const QFileInfo fi(hdriPreviewLocalPath(id));
    return fi.isFile() && fi.size() > 100;
}

bool RenderAssetStore::downloadHdriPreview(const QString &id) {
    if (id.isEmpty()) return false;
    if (isHdriPreviewCached(id)) {
        emit downloadFinished(QStringLiteral("hdriPreview"), id, hdriPreviewLocalPath(id));
        return true;
    }
    Job job;
    job.kind = QStringLiteral("hdriPreview");
    job.id = id;
    job.destPath = hdriPreviewLocalPath(id);
    job.label = QStringLiteral("Preview %1").arg(id);
    const QUrl url(QStringLiteral("https://dl.polyhaven.org/file/ph-assets/HDRIs/extra/Tonemapped%20JPG/%1.jpg").arg(id));
    enqueue(job, url);
    return true;
}

bool RenderAssetStore::downloadHdri(const QString &id, const QString &resolution) {
    if (id.isEmpty() || resolution.isEmpty()) return false;
    if (isHdriCached(id, resolution)) {
        emit downloadFinished(QStringLiteral("hdri"), id, hdriLocalPath(id, resolution));
        emit cacheChanged();
        return true;
    }
    Job job;
    job.kind = QStringLiteral("hdri");
    job.id = id;
    job.resolution = resolution;
    job.destPath = hdriLocalPath(id, resolution);
    job.label = QStringLiteral("HDRI %1 (%2)").arg(id, resolution);
    enqueue(job, QUrl(hdriDownloadUrl(id, resolution)));
    return true;
}

QString RenderAssetStore::materialMapPath(const QString &id, const QString &suffix) const {
    return QDir(m_root).filePath(QStringLiteral("Materials/%1_%2_1k.jpg").arg(id, suffix));
}

QUrl RenderAssetStore::materialMapUrl(const QString &id, const QString &suffix) const {
    return QUrl(QStringLiteral("https://dl.polyhaven.org/file/ph-assets/Textures/jpg/1k/%1/%1_%2_1k.jpg")
                    .arg(id, suffix));
}

bool RenderAssetStore::mapCached(const QString &path) const {
    const QFileInfo fi(path);
    return fi.isFile() && fi.size() > 100;
}

QString RenderAssetStore::materialAlbedoPath(const QString &id) const {
    return materialMapPath(id, QStringLiteral("diff"));
}

QString RenderAssetStore::materialRoughnessPath(const QString &id) const {
    return materialMapPath(id, QStringLiteral("rough"));
}

QString RenderAssetStore::materialMetalnessPath(const QString &id) const {
    return materialMapPath(id, QStringLiteral("metal"));
}

QString RenderAssetStore::materialNormalPath(const QString &id) const {
    return materialMapPath(id, QStringLiteral("nor_gl"));
}

bool RenderAssetStore::isMaterialCached(const QString &id) const {
    return mapCached(materialAlbedoPath(id));
}

QVariantMap RenderAssetStore::materialPaths(const QString &id) const {
    auto pathIf = [&](const QString &p) { return mapCached(p) ? p : QString(); };
    return QVariantMap{
        {"id", id},
        {"albedo", pathIf(materialAlbedoPath(id))},
        {"roughness", pathIf(materialRoughnessPath(id))},
        {"metalness", pathIf(materialMetalnessPath(id))},
        {"normal", pathIf(materialNormalPath(id))},
        {"cached", isMaterialCached(id)},
    };
}

bool RenderAssetStore::downloadMaterial(const QString &id) {
    if (id.isEmpty()) return false;
    // Queue albedo (required) then optional PBR maps; last job emits "material" finished
    struct Spec { const char *suffix; const char *label; bool optional; };
    const Spec specs[] = {
        {"diff", "albedo", false},
        {"rough", "roughness", true},
        {"metal", "metalness", true},
        {"nor_gl", "normal", true},
    };
    QList<QPair<Job, QUrl>> batch;
    for (int i = 0; i < 4; ++i) {
        const QString path = materialMapPath(id, specs[i].suffix);
        if (mapCached(path)) continue;
        Job job;
        job.kind = QStringLiteral("materialMap");
        job.id = id;
        job.mapKind = specs[i].suffix;
        job.destPath = path;
        job.label = QStringLiteral("Material %1 (%2)").arg(id, specs[i].label);
        job.optional = specs[i].optional;
        job.packFinisher = false;
        batch.append({job, materialMapUrl(id, specs[i].suffix)});
    }
    if (batch.isEmpty()) {
        emit downloadFinished(QStringLiteral("material"), id, materialAlbedoPath(id));
        emit cacheChanged();
        return true;
    }
    batch.last().first.packFinisher = true;
    for (const auto &item : batch)
        enqueue(item.first, item.second);
    return true;
}

QVariantList RenderAssetStore::materialCatalog() const {
    const struct {
        const char *id, *name, *appearanceId;
    } items[] = {
        {"metal_plate", "Metal Plate", "steel"},
        {"metal_0003", "Brushed Metal", "aluminum"},
        {"plastic", "Plastic", "plastic_white"},
        {"rubber_001", "Rubber", "rubber_black"},
        {"wood_cabinet_worn_long", "Worn Wood", "wood_oak"},
        {"wood_table_001", "Wood Table", "wood_oak"},
        {"concrete_floor_worn_001", "Worn Concrete", "concrete"},
        {"rock_boulder_cracked", "Cracked Rock", "concrete"},
        {"marble_01", "Marble", "marble"},
        {"fabric_pattern_05", "Fabric", "fabric_grey"},
        {"ceramic_tile_02", "Ceramic Tile", "ceramic_white"},
        {"christmas_tree_ornament_01", "Glass Ornament", "glass_clear"},
    };
    QVariantList out;
    for (const auto &it : items) {
        const QVariantMap paths = materialPaths(it.id);
        out.append(QVariantMap{
            {"id", it.id},
            {"name", it.name},
            {"appearanceId", it.appearanceId},
            {"cached", paths["cached"]},
            {"localPath", materialAlbedoPath(it.id)},
            {"albedoPath", paths["albedo"]},
            {"roughnessPath", paths["roughness"]},
            {"metalnessPath", paths["metalness"]},
            {"normalPath", paths["normal"]},
            {"license", "CC0"},
            {"credit", "Textures: Poly Haven (CC0) — https://polyhaven.com"},
            {"pageUrl", QStringLiteral("https://polyhaven.com/a/%1").arg(it.id)},
        });
    }
    return out;
}

QVariantMap RenderAssetStore::statusForHdri(const QString &id) const {
    QVariantMap m{{"id", id}};
    QVariantList cached;
    for (const QString &r : {"1k", "2k", "4k", "8k"})
        if (isHdriCached(id, r))
            cached.append(QVariantMap{{"resolution", r}, {"path", hdriLocalPath(id, r)}});
    m["cached"] = cached;
    m["previewCached"] = isHdriPreviewCached(id);
    m["previewPath"] = hdriPreviewLocalPath(id);
    QString best;
    for (const QString &r : {"8k", "4k", "2k", "1k"})
        if (isHdriCached(id, r)) { best = r; break; }
    m["bestResolution"] = best;
    m["bestPath"] = best.isEmpty() ? QString() : hdriLocalPath(id, best);
    return m;
}

void RenderAssetStore::enqueue(const Job &job, const QUrl &url) {
    m_queue.enqueue({job, url});
    pumpQueue();
}

void RenderAssetStore::pumpQueue() {
    if (m_reply || m_queue.isEmpty()) return;
    const auto next = m_queue.dequeue();
    startJob(next.first, next.second);
}

bool RenderAssetStore::startJob(const Job &job, const QUrl &url) {
    if (!url.isValid()) {
        setError(QStringLiteral("Invalid download URL"));
        emit downloadFailed(job.kind, job.id, m_lastError);
        pumpQueue();
        return false;
    }
    m_job = job;
    m_jobLabel = job.label;
    m_buffer.clear();
    setProgress(0);
    setError({});
    emit busyChanged();

    if (auto *app = FissionApplication::instance())
        app->beginTask(QStringLiteral("asset-download"), job.label);

    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Fission360/%1").arg(FISSION_VERSION));
    m_reply = m_nam.get(req);
    connect(m_reply, &QNetworkReply::downloadProgress, this, [this](qint64 rec, qint64 total) {
        if (total > 0) setProgress(double(rec) / double(total));
        else setProgress(-1);
        if (auto *app = FissionApplication::instance())
            app->reportProgress(QStringLiteral("asset-download"), m_progress, m_jobLabel);
    });
    connect(m_reply, &QNetworkReply::readyRead, this, [this] {
        if (m_reply) m_buffer.append(m_reply->readAll());
    });
    connect(m_reply, &QNetworkReply::finished, this, [this] {
        if (!m_reply) return;
        const QNetworkReply::NetworkError err = m_reply->error();
        const QString errStr = m_reply->errorString();
        const int http = m_reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        m_buffer.append(m_reply->readAll());
        m_reply->deleteLater();
        m_reply = nullptr;
        if (err != QNetworkReply::NoError || http == 404) {
            if (m_job.optional) {
                // Missing optional PBR map — skip and continue the pack
                if (auto *app = FissionApplication::instance())
                    app->endTask(QStringLiteral("asset-download"));
                m_jobLabel.clear();
                emit busyChanged();
                if (m_job.packFinisher) {
                    emit downloadFinished(QStringLiteral("material"), m_job.id, materialAlbedoPath(m_job.id));
                    emit cacheChanged();
                }
                pumpQueue();
                return;
            }
            finishFail(errStr.isEmpty() ? QStringLiteral("HTTP %1").arg(http) : errStr);
            return;
        }
        if (m_buffer.size() < 64) {
            if (m_job.optional) {
                if (auto *app = FissionApplication::instance())
                    app->endTask(QStringLiteral("asset-download"));
                m_jobLabel.clear();
                emit busyChanged();
                if (m_job.packFinisher) {
                    emit downloadFinished(QStringLiteral("material"), m_job.id, materialAlbedoPath(m_job.id));
                    emit cacheChanged();
                }
                pumpQueue();
                return;
            }
            finishFail(QStringLiteral("Download was empty or too small"));
            return;
        }
        QDir().mkpath(QFileInfo(m_job.destPath).absolutePath());
        QFile out(m_job.destPath);
        if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            finishFail(out.errorString());
            return;
        }
        out.write(m_buffer);
        out.close();
        m_buffer.clear();
        finishOk();
    });
    return true;
}

void RenderAssetStore::finishOk() {
    setProgress(1);
    if (auto *app = FissionApplication::instance()) {
        app->endTask(QStringLiteral("asset-download"));
        if (m_job.kind != QStringLiteral("materialMap") || m_job.packFinisher)
            app->showMessage(QStringLiteral("Downloaded %1").arg(m_jobLabel));
    }
    const Job done = m_job;
    m_jobLabel.clear();
    emit busyChanged();
    if (done.kind == QStringLiteral("materialMap")) {
        emit downloadFinished(QStringLiteral("materialMap"), done.id, done.destPath);
        if (done.packFinisher) {
            emit downloadFinished(QStringLiteral("material"), done.id, materialAlbedoPath(done.id));
            emit cacheChanged();
        }
    } else {
        emit downloadFinished(done.kind, done.id, done.destPath);
        emit cacheChanged();
    }
    pumpQueue();
}

void RenderAssetStore::finishFail(const QString &error) {
    setError(error);
    if (auto *app = FissionApplication::instance()) {
        app->endTask(QStringLiteral("asset-download"));
        app->showMessage(QStringLiteral("Download failed: %1").arg(error));
    }
    const Job done = m_job;
    m_jobLabel.clear();
    m_queue.clear();
    emit busyChanged();
    emit downloadFailed(done.kind, done.id, error);
}

void RenderAssetStore::cancel() {
    m_queue.clear();
    if (!m_reply) return;
    m_reply->abort();
    m_reply->deleteLater();
    m_reply = nullptr;
    m_buffer.clear();
    m_jobLabel.clear();
    if (auto *app = FissionApplication::instance())
        app->endTask(QStringLiteral("asset-download"));
    emit busyChanged();
}

bool RenderAssetStore::openCacheFolder() const {
    return QDesktopServices::openUrl(QUrl::fromLocalFile(m_root));
}

void RenderAssetStore::setProgress(double p) {
    if (qFuzzyCompare(m_progress, p)) return;
    m_progress = p;
    emit progressChanged();
}

void RenderAssetStore::setError(const QString &e) {
    if (m_lastError == e) return;
    m_lastError = e;
    emit lastErrorChanged();
}
