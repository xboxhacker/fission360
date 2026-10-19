// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#pragma once
#include <QObject>
#include <atomic>
#include <QString>
#include <QTimer>
#include <QUrl>
#include <QQuickItem>
#include "kernel/FissionDocument.h"
#include "kernel/drawing/DrawingManager.h"

// Application singleton exposed to QML as `fissionApp`. Several designs can be open at once (one tab each);
// the active one is what the window edits.
class FissionApplication : public QObject {
    Q_OBJECT
    Q_PROPERTY(FissionDocument *activeDocument READ activeDocument NOTIFY activeDocumentChanged)
    Q_PROPERTY(QList<QObject *> documents READ documents NOTIFY documentsChanged)
    Q_PROPERTY(int activeIndex READ activeIndex WRITE setActiveIndex NOTIFY activeDocumentChanged)
    Q_PROPERTY(QString version READ version CONSTANT)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)
    // Long operations (mesh conversion, face groups, scripts' own tasks) for the status bar's progress bar:
    // busy while any runs; the text and fraction (0..1, or -1 while unknown) of the most recent one
    Q_PROPERTY(bool busy READ busy NOTIFY progressChanged)
    Q_PROPERTY(QString progressText READ progressText NOTIFY progressChanged)
    Q_PROPERTY(double progressValue READ progressValue NOTIFY progressChanged)
    Q_PROPERTY(int taskCount READ taskCount NOTIFY progressChanged)
    // Phase 9: drawings from the active design (Fusion Drawing / From Design)
    Q_PROPERTY(DrawingManager *drawingManager READ drawingManager CONSTANT)
    // Crash recovery: interval in seconds (0 = off). Default 120. Persisted via QSettings.
    Q_PROPERTY(int autosaveIntervalSec READ autosaveIntervalSec WRITE setAutosaveIntervalSec NOTIFY autosaveIntervalChanged)

public:
    static FissionApplication *instance();

    FissionDocument *activeDocument() const { return m_document; }
    QList<QObject *> documents() const;
    int activeIndex() const { return m_documents.indexOf(m_document); }
    void setActiveIndex(int index);
    QString version() const { return QStringLiteral(FISSION_VERSION); }
    QString statusMessage() const { return m_status; }
    bool busy() const { return !m_tasks.isEmpty(); }
    QString progressText() const;
    double progressValue() const;
    int taskCount() const { return m_tasks.size(); }
    DrawingManager *drawingManager() const { return m_drawingManager; }
    // Tasks of scripts and QML (kernel operations report through FissionProgress). Any thread.
    Q_INVOKABLE void beginTask(const QString &id, const QString &label);
    Q_INVOKABLE void reportProgress(const QString &id, double fraction, const QString &label = QString());
    Q_INVOKABLE void endTask(const QString &id);

    // New designs and opened files get their own tab
    Q_INVOKABLE FissionDocument *newDocument();
    Q_INVOKABLE FissionDocument *openDemo();
    Q_INVOKABLE bool openDocument(const QString &pathOrUrl);
    // Closes a tab; the last tab closed leaves an empty new design
    Q_INVOKABLE void closeDocument(int index);
    Q_INVOKABLE bool saveDocument(const QString &pathOrUrl = QString());
    Q_INVOKABLE bool exportDocument(const QString &pathOrUrl);
    Q_INVOKABLE void showMessage(const QString &message);
    Q_INVOKABLE QString toLocalPath(const QString &pathOrUrl) const;
    // The active design's file preview: the item's area of the window, cropped to 16:10 (saved with the design)
    Q_INVOKABLE void captureThumbnail(QQuickItem *item);
    // User preferences kept between sessions (QSettings)
    Q_INVOKABLE QVariant setting(const QString &key, const QVariant &fallback = QVariant()) const;
    Q_INVOKABLE void setSetting(const QString &key, const QVariant &value);
    // The GPLv3 text embedded in the resources (About dialog)
    Q_INVOKABLE QString licenseText() const;
    Q_INVOKABLE QString noticesText() const;    // THIRD_PARTY_NOTICES.md
    int autosaveIntervalSec() const { return m_autosaveSec; }
    void setAutosaveIntervalSec(int sec);
    Q_INVOKABLE QVariantList pendingAutosaves() const;
    Q_INVOKABLE bool recoverAutosave(const QString &documentId);
    Q_INVOKABLE bool deleteAutosave(const QString &documentId);
    // Phase 12: temp STL of the given bodies (empty = all) + launch the chosen slicer. Hidden bodies are
    // never sent. Slicer choice: setting "slicer/choice" = "auto" | "orca" | "bambu" ("auto" = whichever is installed).
    Q_INVOKABLE bool sendToSlicer(const QVariantList &bodyIndices = QVariantList());
    Q_INVOKABLE bool sendToOrca(const QVariantList &bodyIndices = QVariantList()) { return sendToSlicer(bodyIndices); }
    // "orca" | "bambu" | "" — the slicer that Send to Slicer would launch now
    Q_INVOKABLE QString activeSlicer() const;
    Q_INVOKABLE QString slicerName(const QString &slicer) const;    // "OrcaSlicer" / "Bambu Studio"
    Q_INVOKABLE QString findSlicerExecutable(const QString &slicer) const;   // "" when not installed
    // Hunt the disk (user folders, Program Files, drive roots) for orca-slicer.exe / bambu-studio.exe incl.
    // custom builds; runs in the background and ends with slicerSearchFinished
    Q_INVOKABLE void searchSlicers();
    Q_INVOKABLE void cancelSlicerSearch() { m_slicerSearchCancel = true; }
    Q_INVOKABLE bool slicerSearching() const { return m_slicerSearching; }
    // Use this exe (path or file:// URL) as the slicer; the kind comes from its name. Returns "orca"/"bambu"/"".
    Q_INVOKABLE QString setSlicerExecutable(const QString &pathOrUrl);
    Q_INVOKABLE void clearSlicerExecutables();   // back to auto-detect
    Q_INVOKABLE QString findOrcaExecutable() const { return findSlicerExecutable(QStringLiteral("orca")); }
    // A file or folder moved or renamed: open designs inside follow it
    void relocate(const QString &from, const QString &to);

signals:
    void activeDocumentChanged();
    void documentsChanged();
    void statusMessageChanged();
    void progressChanged();
    void autosaveIntervalChanged();
    void autosaveWritten(const QString &documentId);
    // Slicer search: each match as {slicer, path, modified}; finished once with the full list
    void slicerSearchProgress(const QString &where);
    void slicerSearchFinished(const QVariantList &results, bool cancelled);

private:
    std::atomic<bool> m_slicerSearchCancel{false};
    bool m_slicerSearching = false;
    explicit FissionApplication(QObject *parent = nullptr);
    void tickAutosave();
    // On the main thread: a task began, moved on, or finished
    void updateTask(const QString &id, const QString &label, double fraction, bool done);
    struct Task { QString id, label; double fraction = -1; };
    QList<Task> m_tasks;
    void addDocument(FissionDocument *doc);
    // An untitled, unchanged, empty design that a newly opened file may take the place of
    bool isPristine(FissionDocument *doc) const;
    QList<FissionDocument *> m_documents;
    FissionDocument *m_document = nullptr;
    QString m_status;
    DrawingManager *m_drawingManager = nullptr;
    QTimer m_autosaveTimer;
    int m_autosaveSec = 120;
};
