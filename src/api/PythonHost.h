// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#pragma once
#include <QObject>
#include <QVariantMap>
#include <functional>

// Python scripts and add-ins. CPython 3.11 or newer is loaded on first use from the user's installation
// (FISSION_PYTHON, or the newest 64-bit Python in the registry) through the stable ABI (python3.dll, delay
// loaded), so Fission 360 runs without it. Scripts see the same objects as JavaScript ones:
//   app, doc, fission (the application), print(), add_command(id, label, tooltip, fn)
// Every Qt property and invokable method is reachable by name (snake_case names map to camelCase), and
// `import adsk.core, adsk.fusion` gives Application.get().activeProduct / userInterface.messageBox.
namespace PythonHost {

// "Python 3.13 (<folder>)", or why it cannot be used
QString status();
bool available(QString *why = nullptr);

// {ok, output: [lines], error, line, result}
QVariantMap run(const QString &code, QObject *app, int timeoutMs);
// Runs a command registered with add_command
QVariantMap runCommand(const QString &id, int timeoutMs);
void removeCommand(const QString &id);
// Called when a script registers a command
void setCommandHook(std::function<void(const QString &id, const QString &label, const QString &tooltip)> hook);

}   // namespace PythonHost
