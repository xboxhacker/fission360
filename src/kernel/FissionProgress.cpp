// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#include "FissionProgress.h"

#include <QMutex>
#include <QMutexLocker>
#include <algorithm>

namespace FissionProgress {

namespace {
QMutex g_mutex;
Sink g_sink;

void emitTo(const QString &id, const QString &label, double fraction, bool done) {
    Sink sink;
    {
        QMutexLocker lock(&g_mutex);
        sink = g_sink;
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
