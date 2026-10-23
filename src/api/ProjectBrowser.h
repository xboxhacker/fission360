// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#pragma once
#include <QDateTime>
#include <QHash>
#include <QObject>
#include <QVariantList>

// The projects folder: where designs are saved, browsed with sub folders and thumbnails, and managed
// (rename, move, duplicate, delete, export) from the projects panel
class ProjectBrowser : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString root READ root WRITE setRoot NOTIFY rootChanged)
    Q_PROPERTY(QString rootUrl READ rootUrl NOTIFY rootChanged)

public:
    explicit ProjectBrowser(QObject *parent = nullptr);

    QString root() const { return m_root; }
    void setRoot(const QString &pathOrUrl);
    // For this run only (the UI walkthrough), leaving the saved choice alone
    void setSessionRoot(const QString &path);
    QString rootUrl() const;

    // Folders first, then designs (.f360), each {name, path, url, folder, modified, size}
    Q_INVOKABLE QVariantList entries(const QString &folder) const;
    // Every folder under the root, as {path, label} with the root first
    Q_INVOKABLE QVariantList folders() const;
    // "data:image/png;base64,..." from the design file, or "" when it has none
    Q_INVOKABLE QString thumbnail(const QString &path);
    Q_INVOKABLE bool isInside(const QString &path) const;
    Q_INVOKABLE bool exists(const QString &path) const;
    Q_INVOKABLE QString parentFolder(const QString &path) const;
    Q_INVOKABLE QString relative(const QString &path) const;
    Q_INVOKABLE QString urlOf(const QString &path) const;

    // Each returns the new path, or "" with lastError() set
    Q_INVOKABLE QString makeFolder(const QString &parent, const QString &name = QString());
    Q_INVOKABLE QString rename(const QString &path, const QString &name);
    Q_INVOKABLE QString move(const QString &path, const QString &folder);
    Q_INVOKABLE QString duplicate(const QString &path);
    Q_INVOKABLE bool remove(const QString &path);
    Q_INVOKABLE bool exportDesign(const QString &path, const QString &destPathOrUrl);
    Q_INVOKABLE void reveal(const QString &path) const;
    // OS drag of a design file (a file:// URL, so the viewport and Explorer both accept it), with its
    // preview under the cursor. Blocks until the drop; returns true when something accepted it.
    Q_INVOKABLE bool startDrag(const QString &path);
    Q_INVOKABLE QString lastError() const { return m_error; }

signals:
    void rootChanged();
    void changed();

private:
    QString fail(const QString &message);
    QString uniquePath(const QString &folder, const QString &base, const QString &suffix) const;

    QString m_root;
    QString m_error;
    struct Thumb { QDateTime stamp; QString data; };
    QHash<QString, Thumb> m_thumbs;
};
