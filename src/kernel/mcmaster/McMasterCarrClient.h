// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#pragma once
#include <QObject>
#include <QDateTime>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkCookieJar>
#include <QNetworkReply>
#include <QVariantList>
#include <QVariantMap>
#include <QProcess>
#include <QTimer>
#include <QTcpSocket>
#include <functional>

// McMaster-Carr STEP insert: the UI embeds a Chromium WebEngine view for browsing.
// Headless Chrome (CDP) bootstraps Akamai session cookies; Qt networking then runs
// ProductContent + STEP download. onCatalogUrl() auto-inserts when a product page opens.
class McMasterCarrClient : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
    Q_PROPERTY(double progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(QVariantList results READ results NOTIFY resultsChanged)
    Q_PROPERTY(bool preferNoThreads READ preferNoThreads WRITE setPreferNoThreads NOTIFY preferNoThreadsChanged)
    Q_PROPERTY(bool browsing READ browsing NOTIFY browsingChanged)
    Q_PROPERTY(QString currentPartNumber READ currentPartNumber NOTIFY currentPartNumberChanged)
public:
    explicit McMasterCarrClient(QObject *parent = nullptr);
    static McMasterCarrClient *instance();

    bool busy() const { return m_busy; }
    QString status() const { return m_status; }
    QString lastError() const { return m_lastError; }
    double progress() const { return m_progress; }
    QVariantList results() const { return m_results; }
    bool preferNoThreads() const { return m_preferNoThreads; }
    void setPreferNoThreads(bool v);
    bool browsing() const { return m_browsing; }
    QString currentPartNumber() const { return m_currentPartNumber; }

    Q_INVOKABLE void search(const QString &query);
    Q_INVOKABLE void cancel();
    Q_INVOKABLE void insertPart(const QString &partNumber);
    // Embedded catalog popup: prepare Chrome session; watch product URLs
    Q_INVOKABLE void startCatalogBrowser();
    Q_INVOKABLE void stopCatalogBrowser();
    Q_INVOKABLE void onCatalogUrl(const QString &url);
    Q_INVOKABLE QString cacheRoot() const { return m_cacheRoot; }
    Q_INVOKABLE bool openCacheFolder() const;

    static QString extractPartNumber(const QString &text);
    static QString partNumberFromUrl(const QString &urlText);
    static QString pickStepPath(const QVariantMap &productContent, bool preferNoThreads);
    static bool looksLikeBotChallenge(const QByteArray &body);

signals:
    void busyChanged();
    void statusChanged();
    void lastErrorChanged();
    void progressChanged();
    void resultsChanged();
    void preferNoThreadsChanged();
    void browsingChanged();
    void currentPartNumberChanged();
    void searchFinished(bool ok);
    void stepReady(const QString &localPath, const QString &partNumber, const QString &title);
    void insertFailed(const QString &error);
    void productPageDetected(const QString &partNumber);

private:
    enum class Phase { Idle, Bootstrapping, Searching, Resolving, Downloading };

    void setBusy(bool v);
    void setStatus(const QString &s);
    void setError(const QString &e);
    void setProgress(double p);
    void setCurrentPartNumber(const QString &pn);
    void fail(const QString &e);
    void finishIdle();

    void ensureSession(const std::function<void(bool)> &then);
    void bootstrapCookies(const std::function<void(bool)> &then);
    void discoverSiteVersion(const std::function<void(bool)> &then);
    void runSearch(const QString &query);
    void runResolve(const QString &partNumber);
    void runDownload(const QString &filePath, const QString &partNumber, const QString &title);

    QNetworkRequest apiRequest(const QUrl &url) const;
    void applyCookies(const QVariantList &cookies);
    bool sessionReady() const;

    void startChrome(const QString &url, bool headless);
    void stopChromeProcess(bool removeUserData);
    void pollCdpTabs();
    void connectCdp(const QUrl &wsUrl);
    void cdpSend(int id, const QString &method, const QJsonObject &params = QJsonObject());
    void onCdpReadyRead();
    void finishBootstrap(bool ok, const QString &error = {});
    void tryAutoInsertFromBrowseUrl(const QString &tabUrl);

    QString m_cacheRoot;
    QString m_siteVersion;
    QNetworkAccessManager m_nam;
    QNetworkCookieJar *m_jar = nullptr;
    QNetworkReply *m_reply = nullptr;
    Phase m_phase = Phase::Idle;
    bool m_busy = false;
    bool m_preferNoThreads = false;
    bool m_browsing = false;
    QString m_status;
    QString m_lastError;
    double m_progress = 0;
    QVariantList m_results;
    QString m_pendingQuery;
    QString m_pendingPart;
    QString m_pendingTitle;
    QString m_currentPartNumber;
    QString m_autoInsertArmed;
    QByteArray m_buffer;

    QProcess *m_chrome = nullptr;
    QString m_chromeUserData;
    int m_cdpPort = 0;
    QTcpSocket *m_cdp = nullptr;
    QByteArray m_cdpBuf;
    bool m_cdpHandshakeDone = false;
    int m_cdpNextId = 1;
    int m_cdpCookiesId = -1;
    QTimer m_cdpPoll;
    std::function<void(bool)> m_bootstrapThen;
    QDateTime m_sessionAt;
};
