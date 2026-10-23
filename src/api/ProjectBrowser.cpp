// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#include "ProjectBrowser.h"
#include "FissionApplication.h"
#include "kernel/FissionDocument.h"
#include <QDir>
#include <QDirIterator>
#include <QDrag>
#include <QFile>
#include <QFileInfo>
#include <QLocale>
#include <QMimeData>
#include <QPixmap>
#include <QProcess>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>
#include <QUrl>

ProjectBrowser::ProjectBrowser(QObject *parent) : QObject(parent) {
    const QString fallback =
        QDir(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)).filePath("Fission 360 Projects");
    m_root = QDir::cleanPath(QSettings().value("projects/root", fallback).toString());
    QDir().mkpath(m_root);
}

void ProjectBrowser::setRoot(const QString &pathOrUrl) {
    const QUrl url(pathOrUrl);
    const QString path = QDir::cleanPath(url.isLocalFile() ? url.toLocalFile() : pathOrUrl);
    if (path.isEmpty() || path == m_root) return;
    if (!QDir().mkpath(path)) { fail(QString("Cannot use %1 as the projects folder").arg(path)); return; }
    m_root = path;
    QSettings().setValue("projects/root", m_root);
    emit rootChanged();
    emit changed();
}

void ProjectBrowser::setSessionRoot(const QString &path) {
    m_root = QDir::cleanPath(path);
    QDir().mkpath(m_root);
    emit rootChanged();
    emit changed();
}

QString ProjectBrowser::rootUrl() const { return urlOf(m_root); }
QString ProjectBrowser::urlOf(const QString &path) const { return QUrl::fromLocalFile(path).toString(); }

QString ProjectBrowser::fail(const QString &message) {
    m_error = message;
    return QString();
}

bool ProjectBrowser::isInside(const QString &path) const {
    const QString p = QDir::cleanPath(path);
    return p.compare(m_root, Qt::CaseInsensitive) == 0 || p.startsWith(m_root + "/", Qt::CaseInsensitive);
}

bool ProjectBrowser::exists(const QString &path) const { return QFileInfo::exists(path); }

QString ProjectBrowser::parentFolder(const QString &path) const {
    const QString p = QDir::cleanPath(path);
    if (p.compare(m_root, Qt::CaseInsensitive) == 0 || !isInside(p)) return m_root;
    return QFileInfo(p).absolutePath();
}

QString ProjectBrowser::relative(const QString &path) const {
    const QString r = QDir(m_root).relativeFilePath(QDir::cleanPath(path));
    return r == "." ? QString() : r;
}

QVariantList ProjectBrowser::entries(const QString &folder) const {
    const QString dir = folder.isEmpty() ? m_root : QDir::cleanPath(folder);
    QVariantList out;
    const QLocale locale;
    const QFileInfoList dirs = QDir(dir).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name | QDir::IgnoreCase);
    for (const QFileInfo &fi : dirs)
        out << QVariantMap{{"name", fi.fileName()}, {"path", fi.absoluteFilePath()}, {"url", urlOf(fi.absoluteFilePath())},
                           {"folder", true}, {"modified", locale.toString(fi.lastModified(), QLocale::ShortFormat)}, {"size", 0}};
    const QFileInfoList files = QDir(dir).entryInfoList({"*.f360"}, QDir::Files, QDir::Name | QDir::IgnoreCase);
    for (const QFileInfo &fi : files)
        out << QVariantMap{{"name", fi.completeBaseName()}, {"path", fi.absoluteFilePath()}, {"url", urlOf(fi.absoluteFilePath())},
                           {"folder", false}, {"modified", locale.toString(fi.lastModified(), QLocale::ShortFormat)},
                           {"size", fi.size()}};
    return out;
}

QVariantList ProjectBrowser::folders() const {
    QVariantList out{QVariantMap{{"path", m_root}, {"label", QFileInfo(m_root).fileName()}}};
    QStringList found;
    QDirIterator it(m_root, QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (it.hasNext()) found << QDir::cleanPath(it.next());
    found.sort(Qt::CaseInsensitive);
    for (const QString &p : found)
        out << QVariantMap{{"path", p}, {"label", QFileInfo(m_root).fileName() + "/" + QDir(m_root).relativeFilePath(p)}};
    return out;
}

bool ProjectBrowser::startDrag(const QString &path) {
    const QFileInfo fi(path);
    if (!fi.isFile()) return false;
    auto *mime = new QMimeData;
    mime->setUrls({QUrl::fromLocalFile(fi.absoluteFilePath())});
    auto *drag = new QDrag(this);
    drag->setMimeData(mime);
    const QString thumb = thumbnail(path);
    QPixmap pix;
    if (thumb.startsWith(QLatin1String("data:image/png;base64,"))
        && pix.loadFromData(QByteArray::fromBase64(thumb.mid(22).toLatin1()), "PNG")) {
        pix = pix.scaled(120, 75, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        drag->setPixmap(pix);
        drag->setHotSpot(QPoint(pix.width() / 2, pix.height() / 2));
    }
    return drag->exec(Qt::CopyAction) != Qt::IgnoreAction;
}

QString ProjectBrowser::thumbnail(const QString &path) {
    const QFileInfo fi(path);
    if (!fi.isFile()) return QString();
    const QString key = fi.absoluteFilePath();
    auto it = m_thumbs.constFind(key);
    if (it != m_thumbs.constEnd() && it->stamp == fi.lastModified()) return it->data;
    QString data;
    QFile f(key);
    if (f.open(QIODevice::ReadOnly)) {
        // Current files put the preview first (files from Fission before 1.10.6 have it further down);
        // read in chunks until its value closes rather than loading big STEP data
        QByteArray head;
        qsizetype from = -1;
        while (head.size() < (64 << 20) && !f.atEnd()) {
            head += f.read(256 * 1024);
            if (from < 0) {
                for (const char *t : {"\"thumbnail\": \"", "\"thumbnail\":\""}) {
                    const qsizetype at = head.indexOf(t);
                    if (at >= 0) { from = at + qsizetype(qstrlen(t)); break; }
                }
                if (from < 0) continue;
            }
            const qsizetype to = head.indexOf('"', from);
            if (to < 0) continue;
            if (to > from)
                data = QStringLiteral("data:image/png;base64,") + QString::fromLatin1(head.mid(from, to - from));
            break;
        }
    }
    // Older Fusion exports (1.3–1.4) kept a name.f360.png sidecar next to the file
    if (data.isEmpty() && QFileInfo::exists(key + QStringLiteral(".png")))
        data = QUrl::fromLocalFile(key + QStringLiteral(".png")).toString();
    m_thumbs.insert(key, {fi.lastModified(), data});
    return data;
}

QString ProjectBrowser::uniquePath(const QString &folder, const QString &base, const QString &suffix) const {
    QDir d(folder);
    QString name = base + suffix;
    for (int n = 2; d.exists(name); ++n) name = QString("%1 (%2)%3").arg(base).arg(n).arg(suffix);
    return QDir::cleanPath(d.filePath(name));
}

QString ProjectBrowser::makeFolder(const QString &parent, const QString &name) {
    const QString dir = parent.isEmpty() ? m_root : QDir::cleanPath(parent);
    const QString wanted = name.trimmed().isEmpty() ? QString("New Folder") : name.trimmed();
    if (wanted.contains(QRegularExpression(R"([\\/:*?"<>|])"))) return fail(QString("A folder name cannot contain \\ / : * ? \" < > |"));
    const QString path = uniquePath(dir, wanted, QString());
    if (!QDir().mkpath(path)) return fail(QString("Could not create %1").arg(path));
    emit changed();
    return path;
}

QString ProjectBrowser::rename(const QString &path, const QString &name) {
    const QFileInfo fi(path);
    if (!fi.exists()) return fail("The item no longer exists");
    QString wanted = name.trimmed();
    if (wanted.isEmpty()) return fail("Enter a name");
    if (wanted.contains(QRegularExpression(R"([\\/:*?"<>|])"))) return fail(QString("A name cannot contain \\ / : * ? \" < > |"));
    if (fi.isFile() && !wanted.endsWith(".f360", Qt::CaseInsensitive)) wanted += ".f360";
    const QString to = QDir::cleanPath(fi.dir().filePath(wanted));
    if (to.compare(QDir::cleanPath(fi.absoluteFilePath()), Qt::CaseInsensitive) != 0 && QFileInfo::exists(to))
        return fail(QString("%1 already exists").arg(wanted));
    if (!QDir().rename(fi.absoluteFilePath(), to)) return fail(QString("Could not rename %1").arg(fi.fileName()));
    FissionApplication::instance()->relocate(fi.absoluteFilePath(), to);
    emit changed();
    return to;
}

QString ProjectBrowser::move(const QString &path, const QString &folder) {
    const QFileInfo fi(path);
    if (!fi.exists()) return fail("The item no longer exists");
    const QString from = QDir::cleanPath(fi.absoluteFilePath()), dest = QDir::cleanPath(folder);
    if (!QFileInfo(dest).isDir()) return fail("The destination folder does not exist");
    if (fi.isDir() && (dest.compare(from, Qt::CaseInsensitive) == 0 || dest.startsWith(from + "/", Qt::CaseInsensitive)))
        return fail("A folder cannot move into itself");
    if (QFileInfo(from).absolutePath().compare(dest, Qt::CaseInsensitive) == 0) return from;
    const QString to = fi.isDir() ? uniquePath(dest, fi.fileName(), QString())
                                  : uniquePath(dest, fi.completeBaseName(), "." + fi.suffix());
    if (!QDir().rename(from, to)) return fail(QString("Could not move %1").arg(fi.fileName()));
    FissionApplication::instance()->relocate(from, to);
    emit changed();
    return to;
}

QString ProjectBrowser::duplicate(const QString &path) {
    const QFileInfo fi(path);
    if (!fi.isFile()) return fail("Only designs can be duplicated");
    const QString to = uniquePath(fi.absolutePath(), fi.completeBaseName() + " copy", "." + fi.suffix());
    if (!QFile::copy(fi.absoluteFilePath(), to)) return fail(QString("Could not copy %1").arg(fi.fileName()));
    emit changed();
    return to;
}

bool ProjectBrowser::remove(const QString &path) {
    const QFileInfo fi(path);
    if (!fi.exists()) { fail("The item no longer exists"); return false; }
    if (QDir::cleanPath(fi.absoluteFilePath()).compare(m_root, Qt::CaseInsensitive) == 0) {
        fail("The projects folder itself cannot be deleted");
        return false;
    }
    // The recycle bin first, so a mistaken delete can be undone in Explorer
    bool ok = QFile::moveToTrash(fi.absoluteFilePath());
    if (!ok) ok = fi.isDir() ? QDir(fi.absoluteFilePath()).removeRecursively() : QFile::remove(fi.absoluteFilePath());
    if (!ok) { fail(QString("Could not delete %1").arg(fi.fileName())); return false; }
    m_thumbs.remove(fi.absoluteFilePath());
    emit changed();
    return true;
}

bool ProjectBrowser::exportDesign(const QString &path, const QString &destPathOrUrl) {
    const QUrl url(destPathOrUrl);
    const QString dest = url.isLocalFile() ? url.toLocalFile() : destPathOrUrl;
    FissionDocument doc;
    if (!doc.load(path)) { fail(doc.lastError()); return false; }
    if (doc.exportFile(dest).isEmpty()) { fail(doc.lastError()); return false; }
    return true;
}

void ProjectBrowser::reveal(const QString &path) const {
#ifdef Q_OS_WIN
    QProcess::startDetached("explorer.exe", {"/select,", QDir::toNativeSeparators(path)});
#else
    QProcess::startDetached("xdg-open", {QFileInfo(path).absolutePath()});
#endif
}
