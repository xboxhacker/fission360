// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#pragma once
#include <QObject>
#include <QHash>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QVariantList>
#include <QVariantMap>
#include <QQueue>

// Downloads and caches Poly Haven (CC0) HDRIs and material textures under
// <AppData>/FissionCAD/Fission 360/Assets/. Large files are fetched only when
// the user asks — never bundled with the app.
class RenderAssetStore : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString cacheRoot READ cacheRoot CONSTANT)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString currentJob READ currentJob NOTIFY busyChanged)
    Q_PROPERTY(double progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
public:
    explicit RenderAssetStore(QObject *parent = nullptr);
    static RenderAssetStore *instance();

    QString cacheRoot() const { return m_root; }
    bool busy() const { return m_reply != nullptr || !m_queue.isEmpty(); }
    QString currentJob() const { return m_jobLabel; }
    double progress() const { return m_progress; }
    QString lastError() const { return m_lastError; }

    // Resolutions offered for HDRIs (1k … 8k). 16k is omitted by default (very large).
    Q_INVOKABLE QVariantList hdriResolutions() const;
    Q_INVOKABLE bool isHdriCached(const QString &id, const QString &resolution = QStringLiteral("2k")) const;
    Q_INVOKABLE QString hdriLocalPath(const QString &id, const QString &resolution = QStringLiteral("2k")) const;
    // Approximate byte size from the catalog (0 if unknown); used for UI confirmation
    Q_INVOKABLE qint64 hdriApproxBytes(const QString &id, const QString &resolution) const;
    Q_INVOKABLE QString hdriDownloadUrl(const QString &id, const QString &resolution) const;
    // Starts a download; replaces any in-flight job. Emits finished/failed.
    Q_INVOKABLE bool downloadHdri(const QString &id, const QString &resolution = QStringLiteral("2k"));
    // Tonemapped JPG preview (much smaller) for thumbnails / quick sky
    Q_INVOKABLE bool isHdriPreviewCached(const QString &id) const;
    Q_INVOKABLE QString hdriPreviewLocalPath(const QString &id) const;
    Q_INVOKABLE bool downloadHdriPreview(const QString &id);

    // Material maps: albedo (diff), roughness, metalness, normal (nor_gl). Albedo required for "cached".
    Q_INVOKABLE bool isMaterialCached(const QString &id) const;
    Q_INVOKABLE QString materialAlbedoPath(const QString &id) const;
    Q_INVOKABLE QString materialRoughnessPath(const QString &id) const;
    Q_INVOKABLE QString materialMetalnessPath(const QString &id) const;
    Q_INVOKABLE QString materialNormalPath(const QString &id) const;
    Q_INVOKABLE QVariantMap materialPaths(const QString &id) const;
    // Downloads albedo + available PBR maps (rough / metal / nor_gl); missing maps are skipped
    Q_INVOKABLE bool downloadMaterial(const QString &id);
    Q_INVOKABLE QVariantList materialCatalog() const;

    Q_INVOKABLE void cancel();
    Q_INVOKABLE bool openCacheFolder() const;
    // Refresh cached-state flags on the environment / material lists after a download
    Q_INVOKABLE QVariantMap statusForHdri(const QString &id) const;

signals:
    void busyChanged();
    void progressChanged();
    void lastErrorChanged();
    void downloadFinished(const QString &kind, const QString &id, const QString &localPath);
    void downloadFailed(const QString &kind, const QString &id, const QString &error);
    void cacheChanged();

private:
    struct Job {
        QString kind;   // "hdri" | "hdriPreview" | "material" | "materialMap"
        QString id;
        QString resolution;
        QString destPath;
        QString label;
        QString mapKind;   // "diff" | "rough" | "metal" | "nor"
        bool optional = false;
        bool packFinisher = false;   // emit material finished after this job
    };
    bool startJob(const Job &job, const QUrl &url);
    void enqueue(const Job &job, const QUrl &url);
    void pumpQueue();
    void finishOk();
    void finishFail(const QString &error);
    void setProgress(double p);
    void setError(const QString &e);
    QString materialMapPath(const QString &id, const QString &suffix) const;
    QUrl materialMapUrl(const QString &id, const QString &suffix) const;
    bool mapCached(const QString &path) const;

    QString m_root;
    QNetworkAccessManager m_nam;
    QNetworkReply *m_reply = nullptr;
    Job m_job;
    QQueue<QPair<Job, QUrl>> m_queue;
    QString m_jobLabel;
    double m_progress = 0;
    QString m_lastError;
    QByteArray m_buffer;
};
