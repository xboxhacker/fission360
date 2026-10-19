// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#include "McMasterCarrClient.h"

#include <QAbstractSocket>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkCookie>
#include <QNetworkRequest>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QSet>
#include <QStandardPaths>
#include <QUrlQuery>
#include <QtGlobal>
#include <functional>

#ifndef FISSION_VERSION
#define FISSION_VERSION "dev"
#endif

namespace {

QString chromeUserAgent() {
    return QStringLiteral(
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
        "(KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36");
}

QString findChromeExecutable() {
#ifdef Q_OS_WIN
    const QStringList candidates = {
        qEnvironmentVariable("ProgramFiles") + QStringLiteral("/Google/Chrome/Application/chrome.exe"),
        qEnvironmentVariable("ProgramFiles(x86)") + QStringLiteral("/Google/Chrome/Application/chrome.exe"),
        qEnvironmentVariable("LOCALAPPDATA") + QStringLiteral("/Google/Chrome/Application/chrome.exe"),
        QStringLiteral("C:/Program Files/Google/Chrome/Application/chrome.exe"),
        QStringLiteral("C:/Program Files (x86)/Google/Chrome/Application/chrome.exe"),
    };
    for (const QString &p : candidates) {
        if (!p.isEmpty() && QFileInfo::exists(p))
            return p;
    }
#endif
    return {};
}

QByteArray wsClientKey() {
    QByteArray raw(16, Qt::Uninitialized);
    for (int i = 0; i < raw.size(); ++i)
        raw[i] = char(QRandomGenerator::global()->generate() & 0xFF);
    return raw.toBase64();
}

QByteArray encodeWsTextFrame(const QByteArray &payload) {
    QByteArray out;
    out.append(char(0x81)); // FIN + text
    const int n = payload.size();
    // Client-to-server frames MUST be masked
    const quint32 mask = QRandomGenerator::global()->generate();
    const char maskBytes[4] = {
        char((mask >> 24) & 0xFF), char((mask >> 16) & 0xFF),
        char((mask >> 8) & 0xFF), char(mask & 0xFF)};
    if (n < 126) {
        out.append(char(0x80 | n));
    } else if (n <= 0xFFFF) {
        out.append(char(0x80 | 126));
        out.append(char((n >> 8) & 0xFF));
        out.append(char(n & 0xFF));
    } else {
        out.append(char(0x80 | 127));
        for (int i = 7; i >= 0; --i)
            out.append(char((quint64(n) >> (8 * i)) & 0xFF));
    }
    out.append(maskBytes, 4);
    for (int i = 0; i < n; ++i)
        out.append(char(payload[i] ^ maskBytes[i % 4]));
    return out;
}

// Returns decoded payload(s); leaves incomplete data in buf.
QList<QByteArray> decodeWsFrames(QByteArray &buf) {
    QList<QByteArray> out;
    while (buf.size() >= 2) {
        const uchar b0 = uchar(buf[0]);
        const uchar b1 = uchar(buf[1]);
        const bool masked = (b1 & 0x80) != 0;
        quint64 len = b1 & 0x7F;
        int hdr = 2;
        if (len == 126) {
            if (buf.size() < 4) break;
            len = (uchar(buf[2]) << 8) | uchar(buf[3]);
            hdr = 4;
        } else if (len == 127) {
            if (buf.size() < 10) break;
            len = 0;
            for (int i = 0; i < 8; ++i)
                len = (len << 8) | uchar(buf[2 + i]);
            hdr = 10;
        }
        const int maskLen = masked ? 4 : 0;
        if (quint64(buf.size()) < hdr + maskLen + len) break;
        QByteArray payload = buf.mid(hdr + maskLen, int(len));
        if (masked) {
            const char *m = buf.constData() + hdr;
            for (int i = 0; i < payload.size(); ++i)
                payload[i] = char(payload[i] ^ m[i % 4]);
        }
        buf.remove(0, hdr + maskLen + int(len));
        const int opcode = b0 & 0x0F;
        if (opcode == 0x1 || opcode == 0x2)
            out.append(payload);
        else if (opcode == 0x8)
            break; // close
        // ignore ping/pong for now
    }
    return out;
}

} // namespace

McMasterCarrClient::McMasterCarrClient(QObject *parent) : QObject(parent) {
    m_cacheRoot = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                  + QStringLiteral("/McMasterCarr");
    QDir().mkpath(m_cacheRoot);
    m_jar = new QNetworkCookieJar(this);
    m_nam.setCookieJar(m_jar);
    m_cdpPoll.setInterval(400);
    connect(&m_cdpPoll, &QTimer::timeout, this, &McMasterCarrClient::pollCdpTabs);
}

McMasterCarrClient *McMasterCarrClient::instance() {
    static McMasterCarrClient client;
    return &client;
}

void McMasterCarrClient::setPreferNoThreads(bool v) {
    if (m_preferNoThreads == v) return;
    m_preferNoThreads = v;
    emit preferNoThreadsChanged();
}

bool McMasterCarrClient::openCacheFolder() const {
    return QDesktopServices::openUrl(QUrl::fromLocalFile(m_cacheRoot));
}

void McMasterCarrClient::setBusy(bool v) {
    if (m_busy == v) return;
    m_busy = v;
    emit busyChanged();
}

void McMasterCarrClient::setStatus(const QString &s) {
    if (m_status == s) return;
    m_status = s;
    emit statusChanged();
}

void McMasterCarrClient::setError(const QString &e) {
    if (m_lastError == e) return;
    m_lastError = e;
    emit lastErrorChanged();
}

void McMasterCarrClient::setProgress(double p) {
    if (qFuzzyCompare(m_progress, p)) return;
    m_progress = p;
    emit progressChanged();
}

void McMasterCarrClient::setCurrentPartNumber(const QString &pn) {
    if (m_currentPartNumber == pn) return;
    m_currentPartNumber = pn;
    emit currentPartNumberChanged();
}

void McMasterCarrClient::finishIdle() {
    m_phase = Phase::Idle;
    setBusy(false);
    setProgress(0);
}

void McMasterCarrClient::fail(const QString &e) {
    setError(e);
    setStatus(m_browsing ? QStringLiteral("Browse another part, or reopen the same page to retry")
                         : QString());
    finishIdle();
    // Allow auto-insert to retry this PN after a failure
    m_autoInsertArmed.clear();
    if (!m_pendingPart.isEmpty()) {
        emit insertFailed(e);
        m_pendingPart.clear();
    } else {
        emit searchFinished(false);
    }
}

void McMasterCarrClient::cancel() {
    if (m_reply) {
        m_reply->abort();
        m_reply->deleteLater();
        m_reply = nullptr;
    }
    if (!m_browsing)
        finishBootstrap(false, QStringLiteral("Cancelled"));
    m_pendingQuery.clear();
    m_pendingPart.clear();
    m_autoInsertArmed.clear();
    setError(QStringLiteral("Cancelled"));
    finishIdle();
}

bool McMasterCarrClient::sessionReady() const {
    if (m_siteVersion.isEmpty()) return false;
    if (!m_sessionAt.isValid()) return false;
    // Session cookies expire; refresh every 25 minutes
    return m_sessionAt.secsTo(QDateTime::currentDateTimeUtc()) < 25 * 60;
}

bool McMasterCarrClient::looksLikeBotChallenge(const QByteArray &body) {
    const QByteArray head = body.left(200).trimmed();
    return head.startsWith("<!--") || head.contains("Access Denied")
           || (body.contains("blocked") && body.size() < 800);
}

QString McMasterCarrClient::extractPartNumber(const QString &text) {
    // Trailing '_' (image cache paths) is a word char, so do not require \b after the PN
    static const QRegularExpression re(
        QStringLiteral(R"((?i)(?<![A-Z0-9])(\d{1,6}[A-Z]\d{1,5}(?:[A-Z]\d{0,4})?)(?![A-Z0-9]))"));
    const auto m = re.match(text);
    return m.hasMatch() ? m.captured(1).toUpper() : QString();
}

QString McMasterCarrClient::partNumberFromUrl(const QString &urlText) {
    const QUrl url(urlText);
    if (!url.host().contains(QStringLiteral("mcmaster"), Qt::CaseInsensitive))
        return {};
    static const QRegularExpression fullPn(
        QStringLiteral(R"(^(?i)(\d{1,6}[A-Z]\d{1,5}(?:[A-Z]\d{0,4})?)$)"));
    // Path segments, then fragment / query (SPA-style #/91251A194/)
    QStringList segs = url.path().split(QLatin1Char('/'), Qt::SkipEmptyParts);
    const QString frag = url.fragment();
    if (!frag.isEmpty())
        segs += frag.split(QRegularExpression(QStringLiteral("[/?#&]")), Qt::SkipEmptyParts);
    {
        const QUrlQuery q(url);
        for (const auto &qi : q.queryItems()) {
            if (qi.first.compare(QStringLiteral("partnumber"), Qt::CaseInsensitive) == 0
                || qi.first.compare(QStringLiteral("part"), Qt::CaseInsensitive) == 0)
                segs.prepend(qi.second);
        }
    }
    for (const QString &seg : segs) {
        if (seg.startsWith(QLatin1String("mv"), Qt::CaseInsensitive))
            continue;
        if (seg.compare(QLatin1String("products"), Qt::CaseInsensitive) == 0
            || seg.compare(QLatin1String("Search"), Qt::CaseInsensitive) == 0
            || seg.compare(QLatin1String("WebParts"), Qt::CaseInsensitive) == 0
            || seg.compare(QLatin1String("About"), Qt::CaseInsensitive) == 0)
            return {};
        const auto m = fullPn.match(seg);
        if (m.hasMatch())
            return m.captured(1).toUpper();
    }
    // Last resort: any PN token in the full URL
    return extractPartNumber(urlText);
}

QString McMasterCarrClient::pickStepPath(const QVariantMap &productContent, bool preferNoThreads) {
    const QVariantMap cad = productContent.value(QStringLiteral("cadControlDat")).toMap();
    const QVariantMap avail = cad.value(QStringLiteral("AvailableCAD")).toMap();
    const QVariantList threeD = avail.value(QStringLiteral("ThreeDDownloads")).toList();
    QString step, stepNoThreads;
    for (const QVariant &v : threeD) {
        const QVariantMap m = v.toMap();
        const QString name = m.value(QStringLiteral("DisplayName")).toString();
        const QString path = m.value(QStringLiteral("FilePath")).toString();
        if (name.compare(QStringLiteral("3-D STEP no threads"), Qt::CaseInsensitive) == 0)
            stepNoThreads = path;
        else if (name.compare(QStringLiteral("3-D STEP"), Qt::CaseInsensitive) == 0)
            step = path;
    }
    if (preferNoThreads && !stepNoThreads.isEmpty()) return stepNoThreads;
    if (!step.isEmpty()) return step;
    return stepNoThreads;
}

QNetworkRequest McMasterCarrClient::apiRequest(const QUrl &url) const {
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader, chromeUserAgent());
    req.setRawHeader("Accept", "application/json, text/javascript, */*; q=0.01");
    req.setRawHeader("X-Requested-With", "XMLHttpRequest");
    req.setRawHeader("Referer", "https://www.mcmaster.com/");
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    return req;
}

void McMasterCarrClient::applyCookies(const QVariantList &cookies) {
    QList<QNetworkCookie> list;
    for (const QVariant &v : cookies) {
        const QVariantMap m = v.toMap();
        QNetworkCookie c(m.value(QStringLiteral("name")).toString().toUtf8(),
                         m.value(QStringLiteral("value")).toString().toUtf8());
        QString domain = m.value(QStringLiteral("domain")).toString();
        if (domain.startsWith('.'))
            domain = domain.mid(1);
        c.setDomain(domain.isEmpty() ? QStringLiteral("mcmaster.com") : domain);
        c.setPath(m.value(QStringLiteral("path")).toString().isEmpty()
                      ? QStringLiteral("/")
                      : m.value(QStringLiteral("path")).toString());
        c.setSecure(m.value(QStringLiteral("secure")).toBool());
        c.setHttpOnly(m.value(QStringLiteral("httpOnly")).toBool());
        list.append(c);
    }
    m_jar->setCookiesFromUrl(list, QUrl(QStringLiteral("https://www.mcmaster.com/")));
    m_sessionAt = QDateTime::currentDateTimeUtc();
}

void McMasterCarrClient::search(const QString &query) {
    const QString q = query.trimmed();
    if (q.isEmpty()) {
        setError(QStringLiteral("Enter a part number or search terms"));
        emit searchFinished(false);
        return;
    }
    if (m_busy) {
        setError(QStringLiteral("Already busy"));
        return;
    }
    m_pendingQuery = q;
    m_pendingPart.clear();
    setError({});
    setBusy(true);
    setStatus(QStringLiteral("Connecting to McMaster-Carr..."));
    ensureSession([this](bool ok) {
        if (!ok) return;
        runSearch(m_pendingQuery);
    });
}

void McMasterCarrClient::insertPart(const QString &partNumber) {
    const QString pn = extractPartNumber(partNumber).isEmpty()
                           ? partNumber.trimmed().toUpper()
                           : extractPartNumber(partNumber);
    if (pn.isEmpty()) {
        setError(QStringLiteral("Missing part number"));
        emit insertFailed(m_lastError);
        return;
    }
    if (m_busy) {
        setError(QStringLiteral("Already busy"));
        emit insertFailed(m_lastError);
        return;
    }
    m_pendingPart = pn;
    m_pendingTitle.clear();
    setError({});
    setBusy(true);
    setStatus(QStringLiteral("Connecting to McMaster-Carr..."));
    ensureSession([this](bool ok) {
        if (!ok) return;
        runResolve(m_pendingPart);
    });
}

void McMasterCarrClient::ensureSession(const std::function<void(bool)> &then) {
    if (sessionReady()) {
        then(true);
        return;
    }
    m_phase = Phase::Bootstrapping;
    bootstrapCookies([this, then](bool ok) {
        if (!ok) {
            fail(m_lastError.isEmpty()
                     ? QStringLiteral("Could not open a McMaster-Carr session. Is Google Chrome installed?")
                     : m_lastError);
            then(false);
            return;
        }
        discoverSiteVersion([this, then](bool ok2) {
            if (!ok2) {
                fail(m_lastError.isEmpty() ? QStringLiteral("Could not read the McMaster site version")
                                           : m_lastError);
                then(false);
                return;
            }
            then(true);
        });
    });
}

void McMasterCarrClient::bootstrapCookies(const std::function<void(bool)> &then) {
#ifdef Q_OS_WIN
    if (findChromeExecutable().isEmpty()) {
        setError(QStringLiteral("Google Chrome was not found (needed to open a McMaster session)"));
        then(false);
        return;
    }
    m_bootstrapThen = then;
    setStatus(QStringLiteral("Connecting to McMaster-Carr..."));
    m_chromeUserData = QDir::temp().filePath(
        QStringLiteral("fission-mcmaster-%1").arg(QDateTime::currentMSecsSinceEpoch()));
    QDir().mkpath(m_chromeUserData);
    m_cdpPort = 9220 + int(QDateTime::currentMSecsSinceEpoch() % 400);
    startChrome(QStringLiteral("https://www.mcmaster.com/"), true);
    m_cdpPoll.start();
    QTimer::singleShot(20000, this, [this] {
        if (m_phase == Phase::Bootstrapping && m_bootstrapThen)
            finishBootstrap(false, QStringLiteral("Timed out waiting for McMaster session"));
    });
#else
    Q_UNUSED(then);
    setError(QStringLiteral("McMaster-Carr insert is only supported on Windows (needs Chrome)"));
    then(false);
#endif
}

void McMasterCarrClient::startChrome(const QString &url, bool headless) {
    stopChromeProcess(false);
    m_chrome = new QProcess(this);
    m_chrome->setProgram(findChromeExecutable());
    QStringList args;
    if (headless)
        args << QStringLiteral("--headless=new") << QStringLiteral("--disable-gpu");
    args << QStringLiteral("--remote-debugging-port=%1").arg(m_cdpPort)
         << QStringLiteral("--user-data-dir=%1").arg(m_chromeUserData)
         << url;
    m_chrome->setArguments(args);
    m_chrome->start();
}

void McMasterCarrClient::stopChromeProcess(bool removeUserData) {
    m_cdpPoll.stop();
    if (m_cdp) {
        m_cdp->disconnect();
        m_cdp->deleteLater();
        m_cdp = nullptr;
    }
    m_cdpHandshakeDone = false;
    m_cdpBuf.clear();
    if (m_chrome) {
        m_chrome->disconnect(this);
        m_chrome->kill();
        m_chrome->waitForFinished(2000);
        m_chrome->deleteLater();
        m_chrome = nullptr;
    }
    if (removeUserData && !m_chromeUserData.isEmpty()) {
        QDir(m_chromeUserData).removeRecursively();
        m_chromeUserData.clear();
    }
}

void McMasterCarrClient::startCatalogBrowser() {
    // Embedded WebEngine popup owns the visible UI; this only arms auto-insert
    // and warms a headless Chrome cookie session for ProductContent / STEP.
    if (m_browsing) {
        if (!sessionReady())
            ensureSession([](bool) {});
        return;
    }
    m_browsing = true;
    emit browsingChanged();
    setError({});
    setCurrentPartNumber({});
    m_autoInsertArmed.clear();
    setStatus(QStringLiteral("Browse McMaster-Carr — open a part to insert STEP"));
    ensureSession([this](bool ok) {
        if (!m_browsing) return;
        if (ok)
            setStatus(QStringLiteral("Browse McMaster-Carr — open a part to insert STEP"));
    });
}

void McMasterCarrClient::stopCatalogBrowser() {
    const bool was = m_browsing;
    m_browsing = false;
    if (was)
        emit browsingChanged();
    setCurrentPartNumber({});
    m_autoInsertArmed.clear();
    // Do not kill an in-flight STEP download; cancel() handles that from the dialog
    if (m_phase == Phase::Bootstrapping)
        finishBootstrap(false, QStringLiteral("Cancelled"));
}

void McMasterCarrClient::onCatalogUrl(const QString &url) {
    if (!m_browsing) return;
    tryAutoInsertFromBrowseUrl(url);
}

void McMasterCarrClient::tryAutoInsertFromBrowseUrl(const QString &tabUrl) {
    const QString pn = partNumberFromUrl(tabUrl);
    setCurrentPartNumber(pn);
    if (pn.isEmpty()) {
        m_autoInsertArmed.clear();
        return;
    }
    if (m_busy || pn == m_autoInsertArmed)
        return;
    if (!sessionReady()) {
        setStatus(QStringLiteral("Connecting to McMaster-Carr..."));
        ensureSession([this, pn](bool ok) {
            if (!ok || !m_browsing) return;
            if (m_currentPartNumber == pn && m_autoInsertArmed != pn)
                tryAutoInsertFromBrowseUrl(QStringLiteral("https://www.mcmaster.com/%1/").arg(pn));
        });
        return;
    }
    m_autoInsertArmed = pn;
    emit productPageDetected(pn);
    setStatus(QStringLiteral("Part %1 — selecting 3-D STEP...").arg(pn));
    insertPart(pn);
}

void McMasterCarrClient::pollCdpTabs() {
    if (m_phase != Phase::Bootstrapping) {
        m_cdpPoll.stop();
        return;
    }
    auto *reply = m_nam.get(QNetworkRequest(QUrl(QStringLiteral("http://127.0.0.1:%1/json/list").arg(m_cdpPort))));
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        if (m_phase != Phase::Bootstrapping) return;
        if (reply->error() != QNetworkReply::NoError) return;
        const QJsonArray tabs = QJsonDocument::fromJson(reply->readAll()).array();
        for (const QJsonValue &v : tabs) {
            const QJsonObject o = v.toObject();
            const QString tabUrl = o.value(QStringLiteral("url")).toString();
            const QString ws = o.value(QStringLiteral("webSocketDebuggerUrl")).toString();
            if (tabUrl.startsWith(QStringLiteral("https://www.mcmaster.com")) && !ws.isEmpty()
                && !tabUrl.contains(QStringLiteral("ServiceWorker"))) {
                m_cdpPoll.stop();
                connectCdp(QUrl(ws));
                return;
            }
        }
    });
}

void McMasterCarrClient::connectCdp(const QUrl &wsUrl) {
    if (m_cdp) {
        m_cdp->disconnect();
        m_cdp->deleteLater();
        m_cdp = nullptr;
    }
    m_cdpBuf.clear();
    m_cdpHandshakeDone = false;
    m_cdp = new QTcpSocket(this);
    connect(m_cdp, &QTcpSocket::readyRead, this, &McMasterCarrClient::onCdpReadyRead);
    connect(m_cdp, &QTcpSocket::errorOccurred, this, [this](QAbstractSocket::SocketError) {
        finishBootstrap(false, QStringLiteral("CDP connection failed: %1").arg(m_cdp ? m_cdp->errorString() : QString()));
    });
    const QString key = QString::fromLatin1(wsClientKey());
    m_cdp->connectToHost(wsUrl.host(), quint16(wsUrl.port(80)));
    connect(m_cdp, &QTcpSocket::connected, this, [this, wsUrl, key] {
        const QByteArray path = wsUrl.path(QUrl::FullyEncoded).toUtf8()
                                + (wsUrl.hasQuery() ? ("?" + wsUrl.query(QUrl::FullyEncoded).toUtf8()) : QByteArray());
        QByteArray req;
        req += "GET " + path + " HTTP/1.1\r\n";
        req += "Host: " + wsUrl.host().toUtf8() + ":" + QByteArray::number(wsUrl.port()) + "\r\n";
        req += "Upgrade: websocket\r\n";
        req += "Connection: Upgrade\r\n";
        req += "Sec-WebSocket-Key: " + key.toLatin1() + "\r\n";
        req += "Sec-WebSocket-Version: 13\r\n\r\n";
        m_cdp->write(req);
        m_cdp->setProperty("wsKey", key);
    });
}

void McMasterCarrClient::cdpSend(int id, const QString &method, const QJsonObject &params) {
    if (!m_cdp || !m_cdpHandshakeDone) return;
    QJsonObject msg{{QStringLiteral("id"), id}, {QStringLiteral("method"), method}};
    if (!params.isEmpty())
        msg.insert(QStringLiteral("params"), params);
    m_cdp->write(encodeWsTextFrame(QJsonDocument(msg).toJson(QJsonDocument::Compact)));
}

void McMasterCarrClient::onCdpReadyRead() {
    if (!m_cdp) return;
    m_cdpBuf.append(m_cdp->readAll());
    if (!m_cdpHandshakeDone) {
        const int end = m_cdpBuf.indexOf("\r\n\r\n");
        if (end < 0) return;
        const QByteArray header = m_cdpBuf.left(end);
        m_cdpBuf.remove(0, end + 4);
        if (!header.contains("101")) {
            finishBootstrap(false, QStringLiteral("WebSocket upgrade failed"));
            return;
        }
        m_cdpHandshakeDone = true;
        cdpSend(m_cdpNextId++, QStringLiteral("Network.enable"));
        m_cdpCookiesId = m_cdpNextId++;
        // Give Akamai a moment to set the cat cookie
        QTimer::singleShot(1500, this, [this] {
            if (m_phase == Phase::Bootstrapping)
                cdpSend(m_cdpCookiesId, QStringLiteral("Network.getAllCookies"));
        });
        return;
    }
    const QList<QByteArray> frames = decodeWsFrames(m_cdpBuf);
    for (const QByteArray &frame : frames) {
        const QJsonObject msg = QJsonDocument::fromJson(frame).object();
        if (msg.value(QStringLiteral("id")).toInt() != m_cdpCookiesId)
            continue;
        const QJsonArray cookies = msg.value(QStringLiteral("result")).toObject()
                                       .value(QStringLiteral("cookies")).toArray();
        QVariantList list;
        bool hasCat = false;
        for (const QJsonValue &v : cookies) {
            const QJsonObject o = v.toObject();
            const QString domain = o.value(QStringLiteral("domain")).toString();
            if (!domain.contains(QStringLiteral("mcmaster"), Qt::CaseInsensitive))
                continue;
            QVariantMap m;
            m.insert(QStringLiteral("name"), o.value(QStringLiteral("name")).toString());
            m.insert(QStringLiteral("value"), o.value(QStringLiteral("value")).toString());
            m.insert(QStringLiteral("domain"), domain);
            m.insert(QStringLiteral("path"), o.value(QStringLiteral("path")).toString());
            m.insert(QStringLiteral("secure"), o.value(QStringLiteral("secure")).toBool());
            m.insert(QStringLiteral("httpOnly"), o.value(QStringLiteral("httpOnly")).toBool());
            if (m.value(QStringLiteral("name")).toString() == QLatin1String("cat"))
                hasCat = true;
            list.append(m);
        }
        if (list.isEmpty() || !hasCat) {
            // Retry once more
            QTimer::singleShot(1500, this, [this] {
                if (m_phase == Phase::Bootstrapping)
                    cdpSend(m_cdpCookiesId = m_cdpNextId++, QStringLiteral("Network.getAllCookies"));
            });
            return;
        }
        applyCookies(list);
        finishBootstrap(true);
        return;
    }
}

void McMasterCarrClient::finishBootstrap(bool ok, const QString &error) {
    if (!m_bootstrapThen)
        return;
    m_cdpPoll.stop();
    if (m_cdp) {
        m_cdp->disconnect();
        m_cdp->deleteLater();
        m_cdp = nullptr;
    }
    m_cdpHandshakeDone = false;
    m_cdpBuf.clear();
    stopChromeProcess(true);
    if (m_phase == Phase::Bootstrapping)
        m_phase = Phase::Idle;
    auto then = m_bootstrapThen;
    m_bootstrapThen = {};
    if (!ok && !error.isEmpty())
        setError(error);
    if (then)
        then(ok);
}

void McMasterCarrClient::discoverSiteVersion(const std::function<void(bool)> &then) {
    // Prefer version from the volver cookie
    for (const QNetworkCookie &c : m_jar->cookiesForUrl(QUrl(QStringLiteral("https://www.mcmaster.com/")))) {
        if (c.name() == "volver") {
            const QString v = QString::fromUtf8(c.value());
            if (v.startsWith(QLatin1String("mv"))) {
                m_siteVersion = v;
                then(true);
                return;
            }
        }
    }
    setStatus(QStringLiteral("Reading site version..."));
    auto *reply = m_nam.get(apiRequest(QUrl(QStringLiteral("https://www.mcmaster.com/"))));
    connect(reply, &QNetworkReply::finished, this, [this, reply, then] {
        reply->deleteLater();
        const QByteArray body = reply->readAll();
        static const QRegularExpression re(QStringLiteral(R"(mv\d{6,})"));
        const auto m = re.match(QString::fromUtf8(body));
        if (!m.hasMatch()) {
            setError(QStringLiteral("Could not find McMaster site version"));
            then(false);
            return;
        }
        m_siteVersion = m.captured(0);
        then(true);
    });
}

void McMasterCarrClient::runSearch(const QString &query) {
    m_phase = Phase::Searching;
    setStatus(QStringLiteral("Searching..."));
    setProgress(-1);
    QUrl url(QStringLiteral("https://www.mcmaster.com/%1/Search/Suggest.aspx").arg(m_siteVersion));
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("arg"), query);
    q.addQueryItem(QStringLiteral("suggester"), QStringLiteral("completionfuzzypn"));
    q.addQueryItem(QStringLiteral("count"), QStringLiteral("24"));
    q.addQueryItem(QStringLiteral("ftdecider"), QStringLiteral("src"));
    q.addQueryItem(QStringLiteral("acdecider"), QStringLiteral("src"));
    q.addQueryItem(QStringLiteral("sort"), QStringLiteral("pn"));
    url.setQuery(q);
    if (m_reply) {
        m_reply->abort();
        m_reply->deleteLater();
    }
    m_reply = m_nam.get(apiRequest(url));
    connect(m_reply, &QNetworkReply::finished, this, [this] {
        auto *reply = m_reply;
        m_reply = nullptr;
        reply->deleteLater();
        const QByteArray body = reply->readAll();
        if (reply->error() != QNetworkReply::NoError || looksLikeBotChallenge(body)) {
            m_sessionAt = {};
            fail(QStringLiteral("McMaster search was blocked. Try again."));
            return;
        }
        const QJsonObject root = QJsonDocument::fromJson(body).object();
        const QJsonArray coll = root.value(QStringLiteral("SrchSuggRsltsDat")).toObject()
                                    .value(QStringLiteral("SrchSuggColl")).toArray();
        QVariantList results;
        QSet<QString> seen;
        for (const QJsonValue &v : coll) {
            const QJsonObject o = v.toObject();
            const QString name = o.value(QStringLiteral("MixedCaseName")).toString();
            const QString request = o.value(QStringLiteral("RequestText")).toString();
            QString pn = extractPartNumber(request);
            if (pn.isEmpty()) pn = extractPartNumber(name);
            if (pn.isEmpty()) {
                const QJsonArray imgs = o.value(QStringLiteral("ImagePaths")).toObject()
                                            .value(QStringLiteral("FullImagePaths")).toArray();
                if (!imgs.isEmpty())
                    pn = extractPartNumber(imgs.at(0).toString());
            }
            if (pn.isEmpty() || seen.contains(pn)) continue;
            seen.insert(pn);
            QString image;
            const QJsonArray imgs = o.value(QStringLiteral("ImagePaths")).toObject()
                                        .value(QStringLiteral("FullImagePaths")).toArray();
            for (const QJsonValue &img : imgs) {
                const QString p = img.toString();
                if (p.contains(QStringLiteral("@100p")) || p.contains(QStringLiteral("@200p"))) {
                    image = p;
                    break;
                }
            }
            if (image.isEmpty() && !imgs.isEmpty())
                image = imgs.at(0).toString();
            if (image.startsWith('/'))
                image = QStringLiteral("https://www.mcmaster.com") + image;

            QString title = name;
            const int dash = name.indexOf(QStringLiteral(" - "));
            if (dash > 0)
                title = name.mid(dash + 3).trimmed();
            else if (o.value(QStringLiteral("IsPartNumber")).toBool())
                title = name;

            QVariantMap row;
            row.insert(QStringLiteral("partNumber"), pn);
            row.insert(QStringLiteral("title"), title);
            row.insert(QStringLiteral("name"), name);
            row.insert(QStringLiteral("imageUrl"), image);
            row.insert(QStringLiteral("isPartNumber"), o.value(QStringLiteral("IsPartNumber")).toBool());
            results.append(row);
        }

        // If the query itself looks like a PN, put it first even when Suggest is sparse
        const QString direct = extractPartNumber(m_pendingQuery);
        if (!direct.isEmpty() && !seen.contains(direct)) {
            QVariantMap row;
            row.insert(QStringLiteral("partNumber"), direct);
            row.insert(QStringLiteral("title"), direct);
            row.insert(QStringLiteral("name"), direct);
            row.insert(QStringLiteral("imageUrl"), QString());
            row.insert(QStringLiteral("isPartNumber"), true);
            results.prepend(row);
        }

        m_results = results;
        emit resultsChanged();
        setStatus(results.isEmpty() ? QStringLiteral("No parts found")
                                    : QStringLiteral("%1 result(s)").arg(results.size()));
        finishIdle();
        emit searchFinished(!results.isEmpty());
    });
}

void McMasterCarrClient::runResolve(const QString &partNumber) {
    m_phase = Phase::Resolving;
    setStatus(QStringLiteral("Looking up CAD for %1...").arg(partNumber));
    setProgress(-1);

    QJsonArray events;
    events.append(QJsonObject{
        {QStringLiteral("type"), QStringLiteral("ORDERINGMASTERPARTNUMBER")},
        {QStringLiteral("selectionType"), QStringLiteral("SELECTION")},
        {QStringLiteral("values"),
         QJsonObject{{QStringLiteral("entities"), partNumber},
                     {QStringLiteral("preselectedComponentPartNumber"), QString()}}},
    });
    QUrl url(QStringLiteral("https://www.mcmaster.com/%1/WebParts/Navigate/ProductContent.aspx")
                 .arg(m_siteVersion));
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("partNumber"), partNumber);
    q.addQueryItem(QStringLiteral("clientNavigationEvents"),
                   QString::fromUtf8(QJsonDocument(events).toJson(QJsonDocument::Compact)));
    q.addQueryItem(QStringLiteral("features"),
                   QStringLiteral("enablemnitpns,enablerewriteetoeventchain,rtrvcadviarps"));
    url.setQuery(q);

    if (m_reply) {
        m_reply->abort();
        m_reply->deleteLater();
    }
    m_reply = m_nam.get(apiRequest(url));
    connect(m_reply, &QNetworkReply::finished, this, [this, partNumber] {
        auto *reply = m_reply;
        m_reply = nullptr;
        reply->deleteLater();
        const QByteArray body = reply->readAll();
        if (reply->error() != QNetworkReply::NoError || looksLikeBotChallenge(body)) {
            m_sessionAt = {};
            fail(QStringLiteral("Could not load CAD options for %1").arg(partNumber));
            return;
        }
        const QJsonObject root = QJsonDocument::fromJson(body).object();
        const QString stepPath = pickStepPath(root.toVariantMap(), m_preferNoThreads);
        if (stepPath.isEmpty()) {
            fail(QStringLiteral("No 3-D STEP download is available for %1").arg(partNumber));
            return;
        }
        // Title from filename: /.../PN_Title.STEP
        QString title = QFileInfo(stepPath).completeBaseName();
        if (title.startsWith(partNumber, Qt::CaseInsensitive)) {
            title = title.mid(partNumber.size());
            if (title.startsWith('_')) title = title.mid(1);
            title.replace(QStringLiteral("NO THREADS_"), QString());
        }
        m_pendingTitle = title;
        runDownload(stepPath, partNumber, title);
    });
}

void McMasterCarrClient::runDownload(const QString &filePath, const QString &partNumber,
                                     const QString &title) {
    m_phase = Phase::Downloading;
    setStatus(QStringLiteral("Downloading STEP..."));
    setProgress(0);
    m_buffer.clear();

    QString urlPath = filePath;
    if (urlPath.startsWith('/'))
        urlPath = QStringLiteral("https://www.mcmaster.com") + urlPath;
    QUrl url(urlPath);

    const QString local = m_cacheRoot + QLatin1Char('/') + partNumber
                          + (m_preferNoThreads ? QStringLiteral("_nothreads") : QString())
                          + QStringLiteral(".step");
    if (QFileInfo::exists(local) && QFileInfo(local).size() > 1024) {
        setStatus(m_browsing
                      ? QStringLiteral("Inserted %1 (cached) — open another part or click Done")
                            .arg(partNumber)
                      : QStringLiteral("Using cached STEP"));
        finishIdle();
        emit stepReady(local, partNumber, title);
        m_pendingPart.clear();
        return;
    }

    QNetworkRequest req = apiRequest(url);
    req.setRawHeader("Accept", "application/STEP, */*");
    req.setRawHeader("Referer", QStringLiteral("https://www.mcmaster.com/%1/").arg(partNumber).toUtf8());

    if (m_reply) {
        m_reply->abort();
        m_reply->deleteLater();
    }
    m_reply = m_nam.get(req);
    connect(m_reply, &QNetworkReply::downloadProgress, this, [this](qint64 rec, qint64 total) {
        setProgress(total > 0 ? double(rec) / double(total) : -1.0);
    });
    connect(m_reply, &QNetworkReply::readyRead, this, [this] {
        if (m_reply) m_buffer.append(m_reply->readAll());
    });
    connect(m_reply, &QNetworkReply::finished, this, [this, local, partNumber, title] {
        auto *reply = m_reply;
        m_reply = nullptr;
        reply->deleteLater();
        m_buffer.append(reply->readAll());
        const int http = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (reply->error() != QNetworkReply::NoError || http == 403 || http == 404
            || looksLikeBotChallenge(m_buffer) || m_buffer.size() < 256) {
            m_sessionAt = {};
            m_buffer.clear();
            fail(QStringLiteral("STEP download failed for %1 (HTTP %2)").arg(partNumber).arg(http));
            return;
        }
        QDir().mkpath(QFileInfo(local).absolutePath());
        QFile out(local);
        if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            fail(out.errorString());
            return;
        }
        out.write(m_buffer);
        out.close();
        m_buffer.clear();
        setStatus(m_browsing
                      ? QStringLiteral("Inserted %1 — open another part or click Done")
                            .arg(partNumber)
                      : QStringLiteral("Downloaded %1").arg(partNumber));
        finishIdle();
        emit stepReady(local, partNumber, title);
        m_pendingPart.clear();
    });
}
