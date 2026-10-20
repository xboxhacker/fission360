// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#include "FissionApplication.h"
#include "app/SelfTest.h"
#include "kernel/DocumentVersionStore.h"
#include "kernel/FissionBody.h"
#include "kernel/FissionProgress.h"
#include <QBuffer>
#include <QDateTime>
#include <QDesktopServices>
#include <QEvent>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QSet>
#include <QSysInfo>
#include <QThread>
#include <QUrl>
#include <QUrlQuery>
#include <algorithm>
#include <cmath>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSettings>

FissionApplication *FissionApplication::instance() {
    static FissionApplication *app = new FissionApplication();
    return app;
}

FissionApplication::FissionApplication(QObject *parent) : QObject(parent) {
    // Kernel operations report from worker threads; the status bar is updated on this (the main) thread
    QPointer<FissionApplication> self(this);
    FissionProgress::setSink([self](const QString &id, const QString &label, double fraction, bool done) {
        if (!self) return;
        QMetaObject::invokeMethod(self, [self, id, label, fraction, done] {
            if (self) self->updateTask(id, label, fraction, done);
        }, Qt::QueuedConnection);
    });
    // Ctrl+Y redo — QML Shortcut is unreliable when the viewport / dialogs hold focus
    if (qApp) qApp->installEventFilter(this);
    m_drawingManager = new DrawingManager(this);
    m_autosaveSec = setting(QStringLiteral("autosave/intervalSec"), 120).toInt();
    connect(&m_autosaveTimer, &QTimer::timeout, this, &FissionApplication::tickAutosave);
    setAutosaveIntervalSec(m_autosaveSec);
    newDocument();
}

bool FissionApplication::eventFilter(QObject *watched, QEvent *event) {
    if (event->type() == QEvent::KeyPress) {
        auto *ke = static_cast<QKeyEvent *>(event);
        const bool ctrl = ke->modifiers() & Qt::ControlModifier;
        const bool shift = ke->modifiers() & Qt::ShiftModifier;
        const bool alt = ke->modifiers() & Qt::AltModifier;
        const bool meta = ke->modifiers() & Qt::MetaModifier;
        // Ctrl+Y (no Shift/Alt) — Windows CAD redo. Skip when typing in a text field.
        if (ctrl && !shift && !alt && !meta && ke->key() == Qt::Key_Y) {
            const QObject *focus = QGuiApplication::focusObject();
            const char *cn = focus && focus->metaObject() ? focus->metaObject()->className() : "";
            const QByteArray name(cn);
            if (name.contains("TextInput") || name.contains("TextEdit") || name.contains("TextField")
                || name.contains("SpinBox") || name.contains("ComboBox"))
                return QObject::eventFilter(watched, event);
            if (m_document && m_document->canRedo()) {
                m_document->redo();
                showMessage(QStringLiteral("Redo"));
                return true;
            }
            showMessage(QStringLiteral("Nothing to redo"));
            return true;
        }
    }
    return QObject::eventFilter(watched, event);
}

void FissionApplication::setAutosaveIntervalSec(int sec) {
    sec = std::max(0, sec);
    if (m_autosaveSec != sec) {
        m_autosaveSec = sec;
        setSetting(QStringLiteral("autosave/intervalSec"), m_autosaveSec);
        emit autosaveIntervalChanged();
    }
    if (m_autosaveSec <= 0) {
        m_autosaveTimer.stop();
    } else {
        m_autosaveTimer.start(m_autosaveSec * 1000);
    }
}

void FissionApplication::tickAutosave() {
    for (FissionDocument *d : m_documents) {
        if (!d || !d->isModified()) continue;
        if (d->writeAutosaveNow())
            emit autosaveWritten(d->documentId());
    }
}

QVariantList FissionApplication::pendingAutosaves() const {
    return FissionDocument::pendingAutosaves();
}

bool FissionApplication::recoverAutosave(const QString &documentId) {
    FissionDocument *doc = newDocument();
    if (!doc->loadAutosave(documentId)) {
        closeDocument(m_documents.indexOf(doc));
        return false;
    }
    DocumentVersionStore::instance()->clearAutosave(documentId);
    showMessage(QStringLiteral("Recovered autosave for %1").arg(doc->name()));
    return true;
}

bool FissionApplication::deleteAutosave(const QString &documentId) {
    return DocumentVersionStore::instance()->clearAutosave(documentId);
}

QString FissionApplication::slicerName(const QString &slicer) const {
    return slicer == QLatin1String("bambu") ? QStringLiteral("Bambu Studio") : QStringLiteral("OrcaSlicer");
}

QString FissionApplication::findSlicerExecutable(const QString &slicer) const {
#ifdef Q_OS_WIN
    const bool bambu = slicer == QLatin1String("bambu");
    // A path the user set by hand (settings key orca/executable or bambu/executable) wins
    const QString saved = setting(bambu ? QStringLiteral("bambu/executable") : QStringLiteral("orca/executable")).toString();
    if (!saved.isEmpty() && QFileInfo::exists(saved))
        return saved;

    const QStringList folders = bambu
        ? QStringList{QStringLiteral("Bambu Studio"), QStringLiteral("BambuStudio")}
        : QStringList{QStringLiteral("OrcaSlicer")};
    const QStringList exes = bambu
        ? QStringList{QStringLiteral("bambu-studio.exe"), QStringLiteral("BambuStudio.exe")}
        : QStringList{QStringLiteral("orca-slicer.exe"), QStringLiteral("OrcaSlicer.exe")};
    QStringList roots{qEnvironmentVariable("ProgramFiles"), qEnvironmentVariable("ProgramFiles(x86)"),
                      qEnvironmentVariable("LOCALAPPDATA"), qEnvironmentVariable("LOCALAPPDATA") + QStringLiteral("/Programs"),
                      QStringLiteral("C:/Program Files")};
    for (const QString &root : roots) {
        if (root.isEmpty()) continue;
        for (const QString &f : folders)
            for (const QString &e : exes) {
                const QString p = root + QLatin1Char('/') + f + QLatin1Char('/') + e;
                if (QFileInfo::exists(p)) return p;
            }
    }
    // Installers register their folder; this finds custom install locations
    const QString wanted = bambu ? QStringLiteral("Bambu Studio") : QStringLiteral("OrcaSlicer");
    const QStringList hives{QStringLiteral("HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall"),
                            QStringLiteral("HKEY_CURRENT_USER\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall"),
                            QStringLiteral("HKEY_LOCAL_MACHINE\\SOFTWARE\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall")};
    for (const QString &hive : hives) {
        QSettings reg(hive, QSettings::NativeFormat);
        for (const QString &key : reg.childGroups()) {
            reg.beginGroup(key);
            const QString name = reg.value(QStringLiteral("DisplayName")).toString();
            QString dir = reg.value(QStringLiteral("InstallLocation")).toString();
            if (dir.isEmpty()) dir = QFileInfo(reg.value(QStringLiteral("DisplayIcon")).toString().section(QLatin1Char(','), 0, 0)
                                                   .remove(QLatin1Char('"'))).absolutePath();
            reg.endGroup();
            if (name.compare(wanted, Qt::CaseInsensitive) != 0 || dir.isEmpty()) continue;
            for (const QString &e : exes)
                if (QFileInfo::exists(dir + QLatin1Char('/') + e)) return QDir::fromNativeSeparators(dir + QLatin1Char('/') + e);
        }
    }
#else
    Q_UNUSED(slicer)
#endif
    return {};
}

QString FissionApplication::activeSlicer() const {
    const QString choice = setting(QStringLiteral("slicer/choice"), QStringLiteral("auto")).toString();
    if (choice == QLatin1String("orca") || choice == QLatin1String("bambu"))
        return choice;
    // Auto: whichever is installed (OrcaSlicer first when both are)
    for (const char *s : {"orca", "bambu"})
        if (!findSlicerExecutable(QLatin1String(s)).isEmpty()) return QLatin1String(s);
    return {};
}

namespace {
QString slicerKindOf(const QString &path) {
    const QString name = QFileInfo(path).fileName().toLower();
    return name.contains(QLatin1String("bambu")) ? QStringLiteral("bambu") : QStringLiteral("orca");
}
}

QString FissionApplication::setSlicerExecutable(const QString &pathOrUrl) {
    QString path = pathOrUrl.startsWith(QLatin1String("file:"), Qt::CaseInsensitive)
                       ? QUrl(pathOrUrl).toLocalFile() : pathOrUrl;
    path = QDir::fromNativeSeparators(path);
    if (path.isEmpty() || !QFileInfo(path).isFile()) {
        showMessage(QStringLiteral("That file does not exist"));
        return {};
    }
    // The file name tells which slicer it is; a custom-named build counts as OrcaSlicer
    const QString kind = slicerKindOf(path);
    setSetting(kind == QLatin1String("bambu") ? QStringLiteral("bambu/executable") : QStringLiteral("orca/executable"), path);
    setSetting(QStringLiteral("slicer/choice"), kind);
    showMessage(QStringLiteral("Slicer set to %1: %2").arg(slicerName(kind), QDir::toNativeSeparators(path)));
    return kind;
}

void FissionApplication::clearSlicerExecutables() {
    QSettings s;
    s.remove(QStringLiteral("orca/executable"));
    s.remove(QStringLiteral("bambu/executable"));
    s.setValue(QStringLiteral("slicer/choice"), QStringLiteral("auto"));
}

void FissionApplication::searchSlicers() {
    if (m_slicerSearching) return;
    m_slicerSearching = true;
    m_slicerSearchCancel = false;

    struct Root { QString path; int depth; };
    QList<Root> roots;
    auto add = [&](const QString &p, int depth) { if (!p.isEmpty() && QFileInfo(p).isDir()) roots.append({p, depth}); };
    for (auto loc : {QStandardPaths::DocumentsLocation, QStandardPaths::DesktopLocation, QStandardPaths::DownloadLocation})
        add(QStandardPaths::writableLocation(loc), 8);
    add(QDir::homePath(), 6);
    const QString localAppData = qEnvironmentVariable("LOCALAPPDATA");
    add(localAppData + QStringLiteral("/Programs"), 3);
    add(localAppData + QStringLiteral("/OrcaSlicer"), 3);
    add(qEnvironmentVariable("ProgramFiles"), 3);
    add(qEnvironmentVariable("ProgramFiles(x86)"), 3);
    for (const QFileInfo &d : QDir::drives()) add(d.absoluteFilePath(), 4);

    QPointer<FissionApplication> self(this);
    std::atomic<bool> *cancel = &m_slicerSearchCancel;
    QThread *t = QThread::create([self, roots, cancel]() {
        const QStringList wanted{QStringLiteral("orca-slicer.exe"), QStringLiteral("OrcaSlicer.exe"),
                                 QStringLiteral("bambu-studio.exe"), QStringLiteral("BambuStudio.exe")};
        // Folders that are huge or can never hold a slicer
        static const QSet<QString> skip{
            QStringLiteral("node_modules"), QStringLiteral("cmakefiles"), QStringLiteral("$recycle.bin"),
            QStringLiteral("windows"), QStringLiteral("programdata"), QStringLiteral("appdata"),
            QStringLiteral("system volume information"), QStringLiteral("winsxs"), QStringLiteral("deps"),
            QStringLiteral("_deps"), QStringLiteral("resources"), QStringLiteral("temp"), QStringLiteral("recovery")};
        QSet<QString> visited, found;
        QVariantList results;
        int counter = 0;
        for (const Root &root : roots) {
            struct Item { QString dir; int depth; };
            QList<Item> stack{{root.path, root.depth}};
            while (!stack.isEmpty() && !cancel->load()) {
                const Item it = stack.takeLast();
                const QString key = QDir(it.dir).canonicalPath().toLower();
                if (visited.contains(key)) continue;
                visited.insert(key);
                if (++counter % 150 == 0) {
                    const QString where = it.dir;
                    QMetaObject::invokeMethod(self.data(), [self, where] {
                        if (self) emit self->slicerSearchProgress(QDir::toNativeSeparators(where));
                    }, Qt::QueuedConnection);
                }
                QDir dir(it.dir);
                for (const QString &f : dir.entryList(wanted, QDir::Files | QDir::NoDotAndDotDot)) {
                    const QFileInfo fi(dir.filePath(f));
                    const QString canon = fi.canonicalFilePath().toLower();
                    if (found.contains(canon) || canon.contains(QLatin1String("/cmakefiles/"))) continue;
                    found.insert(canon);
                    QVariantMap m;
                    m[QStringLiteral("slicer")] = slicerKindOf(f);
                    m[QStringLiteral("path")] = QDir::toNativeSeparators(fi.absoluteFilePath());
                    m[QStringLiteral("modified")] = fi.lastModified().toString(QStringLiteral("yyyy-MM-dd HH:mm"));
                    m[QStringLiteral("time")] = fi.lastModified().toMSecsSinceEpoch();
                    results.append(m);
                }
                if (it.depth <= 0) continue;
                for (const QString &sub : dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
                    const QString low = sub.toLower();
                    if (skip.contains(low)) continue;
                    // Drive roots: the Users tree is already covered by the user-folder roots
                    if (it.depth == root.depth && root.depth == 4 && low == QLatin1String("users")) continue;
                    stack.append({dir.filePath(sub), it.depth - 1});
                }
            }
            if (cancel->load()) break;
        }
        std::sort(results.begin(), results.end(), [](const QVariant &a, const QVariant &b) {
            return a.toMap().value(QStringLiteral("time")).toLongLong() > b.toMap().value(QStringLiteral("time")).toLongLong();
        });
        const bool wasCancelled = cancel->load();
        if (self)
            QMetaObject::invokeMethod(self.data(), [self, results, wasCancelled] {
                if (!self) return;
                self->m_slicerSearching = false;
                emit self->slicerSearchFinished(results, wasCancelled);
            }, Qt::QueuedConnection);
    });
    connect(t, &QThread::finished, t, &QObject::deleteLater);
    t->start();
}

bool FissionApplication::sendToSlicer(const QVariantList &bodyIndices) {
    if (!m_document) {
        showMessage(QStringLiteral("No design is open"));
        return false;
    }
    const QString slicer = activeSlicer();
    const QString exe = slicer.isEmpty() ? QString() : findSlicerExecutable(slicer);
    if (exe.isEmpty()) {
        showMessage(slicer.isEmpty()
            ? QStringLiteral("No slicer found. Install OrcaSlicer or Bambu Studio (Display Settings ▸ Slicer).")
            : QStringLiteral("%1 was not found. Install it, or pick another slicer in Display Settings.").arg(slicerName(slicer)));
        return false;
    }
    // Only visible bodies are ever sent: hidden means "do not print"
    QVariantList candidates = bodyIndices;
    if (candidates.isEmpty())
        for (int i = 0; i < m_document->bodyCount(); ++i) candidates.append(i);
    QVariantList bodies;
    int hidden = 0;
    for (const QVariant &v : candidates) {
        auto *b = m_document->body(v.toInt());
        if (!b) continue;
        if (b->isVisible()) bodies.append(v);
        else ++hidden;
    }
    if (bodies.isEmpty()) {
        showMessage(hidden ? QStringLiteral("The selected bodies are hidden — hidden bodies are not sent to the slicer")
                           : QStringLiteral("No visible bodies to send to the slicer"));
        return false;
    }
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::TempLocation)
                        + QStringLiteral("/Fission360-Slicer");
    QDir().mkpath(dir);
    // STL avoids the "project from another version" warning that 3MF often triggers
    const QString path = dir + QStringLiteral("/fission-%1.stl")
                             .arg(QDateTime::currentMSecsSinceEpoch());
    if (m_document->exportBodies(path, bodies, QStringLiteral("stl")).isEmpty()) {
        showMessage(QStringLiteral("Send to %1 failed: %2").arg(slicerName(slicer), m_document->lastError()));
        return false;
    }
    if (!QProcess::startDetached(exe, {QDir::toNativeSeparators(path)})) {
        showMessage(QStringLiteral("Could not launch %1").arg(slicerName(slicer)));
        return false;
    }
    showMessage(QStringLiteral("Opened in %1 (%2 visible bod%3%4)")
                    .arg(slicerName(slicer)).arg(bodies.size()).arg(bodies.size() == 1 ? QStringLiteral("y") : QStringLiteral("ies"))
                    .arg(hidden ? QStringLiteral(", %1 hidden skipped").arg(hidden) : QString()));
    return true;
}

void FissionApplication::updateTask(const QString &id, const QString &label, double fraction, bool done) {
    int at = -1;
    for (int i = 0; i < m_tasks.size(); ++i)
        if (m_tasks[i].id == id) at = i;
    if (done) {
        if (at < 0) return;
        m_tasks.removeAt(at);
    } else {
        if (at < 0) {
            m_tasks.append({id, label, fraction});
        } else {
            if (!label.isEmpty()) m_tasks[at].label = label;
            m_tasks[at].fraction = fraction;
        }
    }
    emit progressChanged();
}

QString FissionApplication::progressText() const {
    if (m_tasks.isEmpty()) return QString();
    const Task &t = m_tasks.last();
    QString text = t.label;
    if (t.fraction >= 0) text += QString(" %1%").arg(int(std::floor(t.fraction * 100)));
    if (m_tasks.size() > 1) text += QString(" (+%1 more)").arg(m_tasks.size() - 1);
    return text;
}

double FissionApplication::progressValue() const {
    return m_tasks.isEmpty() ? -1 : m_tasks.last().fraction;
}

void FissionApplication::beginTask(const QString &id, const QString &label) { FissionProgress::begin(id, label); }

void FissionApplication::reportProgress(const QString &id, double fraction, const QString &label) {
    FissionProgress::report(id, fraction, label);
}

void FissionApplication::endTask(const QString &id) { FissionProgress::end(id); }

QString FissionApplication::toLocalPath(const QString &pathOrUrl) const {
    const QUrl url(pathOrUrl);
    return url.isLocalFile() ? url.toLocalFile() : pathOrUrl;
}

void FissionApplication::captureThumbnail(QQuickItem *item) {
    if (!item || !item->window() || !m_document) return;
    const QImage shot = item->window()->grabWindow();
    if (shot.isNull()) return;
    const qreal dpr = shot.devicePixelRatio() > 0 ? shot.devicePixelRatio() : 1;
    const QRectF r = item->mapRectToScene(QRectF(0, 0, item->width(), item->height()));
    QRect area(qRound(r.x() * dpr), qRound(r.y() * dpr), qRound(r.width() * dpr), qRound(r.height() * dpr));
    area = area.intersected(shot.rect());
    if (area.width() < 16 || area.height() < 16) return;
    // Centre crop to 16:10 clear of the view cube and the navigation bar, then 320 x 200
    const int w = std::min(area.width() * 7 / 10, area.height() * 3 / 4 * 16 / 10), h = w * 10 / 16;
    area = QRect(area.center().x() - w / 2, area.center().y() - h * 11 / 20, w, h);
    const QImage thumb = shot.copy(area).scaled(320, 200, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    QByteArray png;
    QBuffer buf(&png);
    buf.open(QIODevice::WriteOnly);
    if (thumb.save(&buf, "PNG")) m_document->setThumbnail(png);
}

void FissionApplication::relocate(const QString &from, const QString &to) {
    const QString f = QDir::cleanPath(from), t = QDir::cleanPath(to);
    for (auto *d : m_documents) {
        const QString p = QDir::cleanPath(d->filePath());
        if (p.isEmpty()) continue;
        if (p.compare(f, Qt::CaseInsensitive) == 0) d->setFilePath(t);
        else if (p.startsWith(f + "/", Qt::CaseInsensitive)) d->setFilePath(t + p.mid(f.size()));
    }
}

QVariant FissionApplication::setting(const QString &key, const QVariant &fallback) const {
    return QSettings().value(key, fallback);
}

void FissionApplication::setSetting(const QString &key, const QVariant &value) { QSettings().setValue(key, value); }

QString FissionApplication::licenseText() const {
    QFile f(QStringLiteral(":/qml/LICENSE"));
    return f.open(QIODevice::ReadOnly) ? QString::fromUtf8(f.readAll()) : QString();
}

QString FissionApplication::noticesText() const {
    QFile f(QStringLiteral(":/qml/THIRD_PARTY_NOTICES.md"));
    return f.open(QIODevice::ReadOnly) ? QString::fromUtf8(f.readAll()) : QString();
}

void FissionApplication::showMessage(const QString &message) {
    m_status = message;
    emit statusMessageChanged();
}

QList<QObject *> FissionApplication::documents() const {
    QList<QObject *> out;
    for (auto *d : m_documents) out << d;
    return out;
}

void FissionApplication::setActiveIndex(int index) {
    if (index < 0 || index >= m_documents.size() || m_documents[index] == m_document) return;
    m_document = m_documents[index];
    emit activeDocumentChanged();
}

void FissionApplication::addDocument(FissionDocument *doc) {
    static int serial = 0;
    doc->setObjectName(QString("design%1").arg(++serial));   // stable key for per-tab UI state
    m_documents.append(doc);
    m_document = doc;
    emit documentsChanged();
    emit activeDocumentChanged();
}

bool FissionApplication::isPristine(FissionDocument *doc) const {
    return doc && doc->filePath().isEmpty() && !doc->isModified() && doc->timeline()->count() == 0;
}

FissionDocument *FissionApplication::newDocument() {
    auto *doc = new FissionDocument(this);
    // Untitled, Untitled 2, ... so the tabs can be told apart
    QStringList names;
    for (auto *d : m_documents) names << d->name();
    QString name = doc->name();
    for (int n = 2; names.contains(name); ++n) name = QString("%1 %2").arg(doc->name()).arg(n);
    doc->setName(name);
    addDocument(doc);
    showMessage("New design");
    return doc;
}

FissionDocument *FissionApplication::openDemo() {
    auto *doc = newDocument();
    SelfTest::buildDemo(doc);
    showMessage("Opened the demo bracket");
    return doc;
}

bool FissionApplication::openDocument(const QString &pathOrUrl) {
    const QString path = toLocalPath(pathOrUrl);
    for (int i = 0; i < m_documents.size(); ++i)
        if (!m_documents[i]->filePath().isEmpty()
            && QFileInfo(m_documents[i]->filePath()).canonicalFilePath() == QFileInfo(path).canonicalFilePath()) {
            setActiveIndex(i);
            showMessage(QString("%1 is already open").arg(QFileInfo(path).fileName()));
            return true;
        }
    auto *doc = new FissionDocument(this);
    if (!doc->load(path)) {
        showMessage(QString("Could not open %1: %2").arg(path, doc->lastError()));
        doc->deleteLater();
        return false;
    }
    // Opening into the empty startup design replaces it instead of leaving an empty tab behind
    if (isPristine(m_document)) {
        const int i = m_documents.indexOf(m_document);
        FissionDocument *old = m_document;
        doc->setObjectName(old->objectName() + "o");
        m_documents[i] = doc;
        m_document = doc;
        emit documentsChanged();
        emit activeDocumentChanged();
        old->deleteLater();
    } else {
        addDocument(doc);
    }
    showMessage(QString("Opened %1").arg(QFileInfo(path).fileName()));
    return true;
}

void FissionApplication::closeDocument(int index) {
    if (index < 0 || index >= m_documents.size()) return;
    FissionDocument *doc = m_documents.takeAt(index);
    if (m_documents.isEmpty()) {
        doc->deleteLater();
        emit documentsChanged();
        newDocument();
        return;
    }
    if (doc == m_document) m_document = m_documents[std::min<int>(index, m_documents.size() - 1)];
    emit documentsChanged();
    emit activeDocumentChanged();
    doc->deleteLater();
}

bool FissionApplication::saveDocument(const QString &pathOrUrl) {
    QString path = toLocalPath(pathOrUrl);
    if (path.isEmpty()) path = m_document->filePath();
    if (path.isEmpty()) {
        showMessage("Choose a file name with Save As");
        return false;
    }
    if (QFileInfo(path).suffix().isEmpty()) path += ".f360";
    if (!m_document->save(path)) {
        showMessage(QString("Save failed: %1").arg(m_document->lastError()));
        return false;
    }
    showMessage(QString("Saved %1").arg(QFileInfo(path).fileName()));
    return true;
}

bool FissionApplication::exportDocument(const QString &pathOrUrl) {
    const QString path = toLocalPath(pathOrUrl);
    if (m_document->exportFile(path).isEmpty()) {
        showMessage(QString("Export failed: %1").arg(m_document->lastError()));
        return false;
    }
    showMessage(QString("Exported %1").arg(QFileInfo(path).fileName()));
    return true;
}

int FissionApplication::compareVersions(const QString &aIn, const QString &bIn) const {
    auto norm = [](QString s) {
        s = s.trimmed();
        if (s.startsWith(QLatin1Char('v'), Qt::CaseInsensitive)) s = s.mid(1);
        // Drop pre-release / build metadata for ordering
        const int cut = s.indexOf(QRegularExpression(QStringLiteral("[+-]")));
        if (cut >= 0) s = s.left(cut);
        return s;
    };
    const QStringList aa = norm(aIn).split(QLatin1Char('.'), Qt::SkipEmptyParts);
    const QStringList bb = norm(bIn).split(QLatin1Char('.'), Qt::SkipEmptyParts);
    const int n = std::max(aa.size(), bb.size());
    for (int i = 0; i < n; ++i) {
        const int av = i < aa.size() ? aa[i].toInt() : 0;
        const int bv = i < bb.size() ? bb[i].toInt() : 0;
        if (av < bv) return -1;
        if (av > bv) return 1;
    }
    return 0;
}

void FissionApplication::skipUpdateVersion(const QString &version) {
    setSetting(QStringLiteral("updates/skipVersion"), version.trimmed());
}

void FissionApplication::openUpdateUrl(const QString &url) {
    if (url.isEmpty()) return;
    QDesktopServices::openUrl(QUrl(url));
}

void FissionApplication::reportBug() {
    QStringList featureErrs;
    if (m_document) {
        for (auto *f : m_document->timeline()->features()) {
            if (f && !f->isHealthy())
                featureErrs << QStringLiteral("%1: %2").arg(f->name(), f->error());
        }
    }
    const QString body = QStringLiteral(
                             "### What happened\n\n"
                             "<!-- Short description of the bug -->\n\n"
                             "### Steps to reproduce\n\n"
                             "1.\n2.\n3.\n\n"
                             "### Expected\n\n\n"
                             "### Actual\n\n\n"
                             "### Environment\n"
                             "- Fission 360: %1\n"
                             "- OS: %2 (%3)\n"
                             "- Qt: %4\n"
                             "- Last status: %5\n"
                             "- Unhealthy features: %6\n")
                             .arg(QStringLiteral(FISSION_VERSION),
                                  QSysInfo::prettyProductName(),
                                  QSysInfo::currentCpuArchitecture(),
                                  QString::fromLatin1(qVersion()),
                                  m_status.isEmpty() ? QStringLiteral("(none)") : m_status,
                                  featureErrs.isEmpty() ? QStringLiteral("(none)") : featureErrs.join(QStringLiteral("; ")));

    QUrl url(QStringLiteral("https://github.com/xboxhacker/fission360/issues/new"));
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("title"), QStringLiteral("Bug: "));
    q.addQueryItem(QStringLiteral("body"), body);
    q.addQueryItem(QStringLiteral("labels"), QStringLiteral("bug"));
    url.setQuery(q);
    if (!QDesktopServices::openUrl(url))
        showMessage(QStringLiteral("Could not open the browser — report at github.com/xboxhacker/fission360/issues"));
    else
        showMessage(QStringLiteral("Opened a new GitHub issue form"));
}

void FissionApplication::checkForUpdates(bool force) {
    if (m_updateCheckBusy) return;
    if (!force && setting(QStringLiteral("updates/checkEnabled"), true).toBool() == false) {
        emit updateCheckFinished(false, QString());
        return;
    }
    m_updateCheckBusy = true;
    QNetworkRequest req(QUrl(QStringLiteral("https://api.github.com/repos/xboxhacker/fission360/releases/latest")));
    req.setHeader(QNetworkRequest::UserAgentHeader,
                  QStringLiteral("Fission360/%1 (update-check)").arg(QStringLiteral(FISSION_VERSION)));
    req.setRawHeader("Accept", "application/vnd.github+json");
    // Avoid hanging the UI forever on offline networks
    req.setTransferTimeout(12000);
    auto *reply = m_updateNam.get(req);
    QPointer<FissionApplication> self(this);
    connect(reply, &QNetworkReply::finished, this, [self, reply, force] {
        reply->deleteLater();
        if (!self) return;
        self->m_updateCheckBusy = false;
        if (reply->error() != QNetworkReply::NoError) {
            emit self->updateCheckFinished(false, QString());
            return;
        }
        const QJsonObject obj = QJsonDocument::fromJson(reply->readAll()).object();
        QString tag = obj.value(QStringLiteral("tag_name")).toString();
        if (tag.startsWith(QLatin1Char('v'), Qt::CaseInsensitive)) tag = tag.mid(1);
        const QString url = obj.value(QStringLiteral("html_url")).toString();
        QString summary = obj.value(QStringLiteral("name")).toString();
        if (summary.isEmpty()) summary = QStringLiteral("Fission 360 %1").arg(tag);
        const QString body = obj.value(QStringLiteral("body")).toString().trimmed();
        if (!body.isEmpty()) {
            QString first = body.section(QLatin1Char('\n'), 0, 2).trimmed();
            if (first.size() > 280) first = first.left(277) + QStringLiteral("…");
            if (!first.isEmpty()) summary += QStringLiteral("\n\n") + first;
        }
        if (tag.isEmpty() || url.isEmpty()) {
            emit self->updateCheckFinished(false, QString());
            return;
        }
        const QString current = QStringLiteral(FISSION_VERSION);
        const bool newer = self->compareVersions(current, tag) < 0;
        if (!newer) {
            emit self->updateCheckFinished(false, tag);
            return;
        }
        const QString skipped = self->setting(QStringLiteral("updates/skipVersion")).toString();
        if (!force && !skipped.isEmpty() && self->compareVersions(skipped, tag) >= 0) {
            emit self->updateCheckFinished(false, tag);
            return;
        }
        emit self->updateAvailable(tag, url, summary);
        emit self->updateCheckFinished(true, tag);
    });
}
