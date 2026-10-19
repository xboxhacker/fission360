// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#pragma once
#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QKeySequence>
#include <QObject>
#include <QPointer>
#include <QQuickWindow>
#include <QTest>
#include <QVariantList>
#include <algorithm>
#include <cstdio>

// Exposed to QML as `uiTest` for the scripted UI walkthrough (--uitest <dir>): screenshots and checks.
class UiTestDriver : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool enabled READ enabled CONSTANT)

public:
    Q_PROPERTY(QString perfFile READ perfFile CONSTANT)
    Q_PROPERTY(QString dir READ dir CONSTANT)

public:
    explicit UiTestDriver(const QString &dir, const QString &perfFile = QString(), QObject *parent = nullptr)
        : QObject(parent), m_dir(dir), m_perfFile(perfFile) {
        if (!dir.isEmpty()) QDir().mkpath(dir);
        m_clock.start();
    }
    void setWindow(QQuickWindow *w) {
        m_window = w;
        if (w)
            connect(w, &QQuickWindow::frameSwapped, this, [this] {
                const qint64 now = m_clock.elapsed();
                if (m_firstFrame < 0) m_firstFrame = now - m_markAt;
                else m_maxGap = std::max(m_maxGap, now - m_lastFrame);
                m_lastFrame = now;
                ++m_frames;
            }, Qt::DirectConnection);
    }
    bool enabled() const { return !m_dir.isEmpty(); }
    QString perfFile() const { return m_perfFile; }
    QString dir() const { return m_dir; }

    // --uiperf: wall clock; since markFrames(), the wait for the first drawn frame and the longest gap between two
    Q_INVOKABLE double ms() const { return double(m_clock.nsecsElapsed()) / 1e6; }
    Q_INVOKABLE void markFrames() { m_maxGap = 0; m_frames = 0; m_firstFrame = -1; m_markAt = m_lastFrame = m_clock.elapsed(); }
    Q_INVOKABLE double firstFrame() const { return double(m_firstFrame); }
    Q_INVOKABLE double maxFrameGap() const { return double(m_maxGap); }
    Q_INVOKABLE int frames() const { return m_frames; }
    Q_INVOKABLE void perf(const QString &what, double ms, const QString &detail = QString()) {
        std::printf("[PERF] %-40s %9.1f ms%s\n", qPrintable(what), ms, detail.isEmpty() ? "" : qPrintable("  " + detail));
        std::fflush(stdout);
    }
    Q_INVOKABLE void quit() { QCoreApplication::exit(0); }

    Q_INVOKABLE bool grab(const QString &name) {
        const QString path = QDir(m_dir).filePath(name + ".png");
        const bool ok = m_window && m_window->grabWindow().save(path);
        std::printf("[SHOT] %s%s\n", qPrintable(path), ok ? "" : " (FAILED)");
        std::fflush(stdout);
        return ok;
    }
    // Presses a key sequence ("E", "Ctrl+Alt+B") on the window, through the shortcut map like a real key press
    Q_INVOKABLE void key(const QString &sequence) {
        if (!m_window) return;
        if (!m_window->isActive()) {
            m_window->requestActivate();
            QTest::qWaitForWindowActive(m_window, 2000);
        }
        QTest::keySequence(m_window, QKeySequence(sequence));
    }
    // Real left-button mouse events at window coordinates: "press", "move" (drags while pressed), "release"
    Q_INVOKABLE void mouse(const QString &what, double x, double y) {
        if (!m_window) return;
        const QPoint p(qRound(x), qRound(y));
        if (what == "press") QTest::mousePress(m_window, Qt::LeftButton, Qt::NoModifier, p);
        else if (what == "release") QTest::mouseRelease(m_window, Qt::LeftButton, Qt::NoModifier, p);
        else QTest::mouseMove(m_window, p);
    }
    Q_INVOKABLE void check(bool ok, const QString &name, const QString &detail = QString()) {
        ok ? ++m_passes : ++m_failures;
        std::printf("[%s] %s%s\n", ok ? "PASS" : "FAIL", qPrintable(name),
                    detail.isEmpty() ? "" : qPrintable("  (" + detail + ")"));
        std::fflush(stdout);
    }
    Q_INVOKABLE void finish() {
        std::printf("\nUI walkthrough: %d passed, %d failed\n", m_passes, m_failures);
        std::fflush(stdout);
        QCoreApplication::exit(m_failures ? 1 : 0);
    }
    // Assemble PNG screenshots already written to the uitest dir into an animated GIF (lightweight walkthrough export).
    // names: ordered base names without extension (e.g. ["01_pick_plane","02_rectangle_rubberband"]).
    // Empty list = every *.png in the dir, sorted by name.
    Q_INVOKABLE bool exportGif(const QString &fileName = QStringLiteral("walkthrough.gif"),
                               const QVariantList &names = {}, int delayCs = 50, int maxWidth = 800);

private:
    QString m_dir, m_perfFile;
    QPointer<QQuickWindow> m_window;
    QElapsedTimer m_clock;
    qint64 m_lastFrame = -1, m_maxGap = 0, m_markAt = 0, m_firstFrame = -1;
    int m_frames = 0;
    int m_passes = 0, m_failures = 0;
};
