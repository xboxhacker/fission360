// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#pragma once
#include <QString>
#include <functional>

// Progress of long operations (mesh conversion, face groups, imports) for the window's status bar. Any
// thread may report; the sink the application installs marshals to the main thread. A task is identified
// by a string so several may run at once. fraction: 0..1, or below 0 when it is not known.
namespace FissionProgress {

using Sink = std::function<void(const QString &id, const QString &label, double fraction, bool done)>;
void setSink(Sink sink);

void begin(const QString &id, const QString &label);
void report(const QString &id, double fraction, const QString &label = QString());
void end(const QString &id);

// A (fraction, what) callback for kernel operations (MeshUtils::Progress): reports under `id`, mapping the
// operation's 0..1 onto [from, to] of the task; `what` names the step, e.g. "Fitting surfaces"
std::function<void(double, const QString &)> callback(const QString &id, double from = 0, double to = 1);

}   // namespace FissionProgress
