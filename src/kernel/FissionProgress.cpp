// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#include "FissionProgress.h"

#include <QElapsedTimer>
#include <QHash>
#include <QMutex>
#include <QSet>
#include <cstdio>
#include <QMutexLocker>
#include <algorithm>

namespace FissionProgress {

namespace {
QMutex g_mutex;
Sink g_sink;
QSet<QString> g_started;   // tasks between begin and end (for FISSION_TRACE_IO)

// FISSION_TRACE_IO=1 prints every stage of a task with the time since it began (open / save profiling)
void trace(const QString &id, const QString &label, bool begin, bool done) {
    static const bool on = qEnvironmentVariableIsSet("FISSION_TRACE_IO");
    if (!on) return;
    static QHash<QString, QElapsedTimer> clocks;
    static QHash<QString, qint64> last;
    if (begin) { clocks[id].start(); last[id] = 0; }
    const qint64 now = clocks.value(id).isValid() ? clocks[id].elapsed() : 0;
    std::fprintf(stderr, "[io] %-6s %7lld ms (+%6lld)  %s\n", qPrintable(id), now, now - last.value(id),
                 done ? "done" : qPrintable(label));
    std::fflush(stderr);
    last[id] = now;
}

void emitTo(const QString &id, const QString &label, double fraction, bool done) {
    Sink sink;
    {
        QMutexLocker lock(&g_mutex);
        sink = g_sink;
        trace(id, label, !done && !g_started.contains(id), done);
        if (done) g_started.remove(id); else g_started.insert(id);
    }
    if (sink) sink(id, label, fraction, done);
}
}   // namespace

void setSink(Sink sink) {
    QMutexLocker lock(&g_mutex);
    g_sink = std::move(sink);
}

void begin(const QString &id, const QString &label) { emitTo(id, label, -1, false); }

void report(const QString &id, double fraction, const QString &label) { emitTo(id, label, fraction, false); }

void end(const QString &id) { emitTo(id, QString(), 1, true); }

std::function<void(double, const QString &)> callback(const QString &id, double from, double to) {
    return [id, from, to](double fraction, const QString &what) {
        report(id, fraction < 0 ? -1 : from + (to - from) * std::clamp(fraction, 0.0, 1.0), what);
    };
}

}   // namespace FissionProgress
