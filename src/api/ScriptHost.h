// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#pragma once
#include <QJSValue>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QStringList>
#include <QVariantMap>

class QJSEngine;

// Scripts and Add-Ins, exposed to QML as `fissionScripts`. Scripts are JavaScript (run in the UI's engine)
// or Python (see PythonHost) with full access to the application:
//   app     - the main window: every command (startCommand, createSketch, openInspect, selectAll, ...),
//             selection, camera, viewport
//   doc     - the active FissionDocument (addFeature, addParameter, bodies, timeline, measure, save, ...)
//   fission - the application object (newDocument, openDocument, showMessage, version)
//   print(...) - adds a line to the script output
//   addCommand(id, label, tooltip, fn) - a toolbar button (UTILITIES > ADD-INS) that calls fn
// A script that runs longer than the time limit is stopped.
//
// Add-ins are scripts in <AppData>/AddIns that run when Fission 360 starts (unless switched off) and
// usually add commands.
class ScriptHost : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString folder READ folder CONSTANT)
    Q_PROPERTY(QString addInFolder READ addInFolder CONSTANT)
    Q_PROPERTY(QStringList scripts READ scripts NOTIFY scriptsChanged)
    Q_PROPERTY(QVariantList addIns READ addIns NOTIFY addInsChanged)
    Q_PROPERTY(QVariantList commands READ commands NOTIFY commandsChanged)
    Q_PROPERTY(QString pythonStatus READ pythonStatus NOTIFY addInsChanged)

public:
    explicit ScriptHost(QJSEngine *engine, QObject *parent = nullptr);

    QString folder() const;          // <AppData>/Scripts, created (with the samples) on first use
    QString addInFolder() const;     // <AppData>/AddIns, likewise
    QStringList scripts() const;     // file names (*.js, *.py) in the scripts folder
    // [{name, language, enabled, loaded, error, output}]
    QVariantList addIns() const;
    // [{id, label, tooltip, addIn}]
    QVariantList commands() const;
    QString pythonStatus() const;

    // addIn = true works on the add-ins folder
    Q_INVOKABLE QString load(const QString &name, bool addIn = false) const;
    Q_INVOKABLE bool save(const QString &name, const QString &code, bool addIn = false);
    Q_INVOKABLE bool remove(const QString &name, bool addIn = false);
    // {ok, output: [lines], error, line (1-based, 0 = unknown), result}; language "js" or "py"
    Q_INVOKABLE QVariantMap run(const QString &code, QObject *app, int timeoutMs = 30000, const QString &language = "js");
    Q_INVOKABLE QVariantMap runFile(const QString &name, QObject *app);

    // Runs every enabled add-in (once per session)
    Q_INVOKABLE void startAddIns(QObject *app);
    // (Re)runs one add-in: its old commands are dropped first
    Q_INVOKABLE QVariantMap runAddIn(const QString &name, QObject *app);
    Q_INVOKABLE void setAddInEnabled(const QString &name, bool enabled, QObject *app);
    Q_INVOKABLE QVariantMap runCommand(const QString &id);

    // For scripts: register a command (JavaScript function)
    Q_INVOKABLE void addCommand(const QString &id, const QString &label, const QString &tooltip, const QJSValue &fn,
                                const QJSValue &refresh = QJSValue());
    Q_INVOKABLE void log(const QString &line) { m_log << line; }

signals:
    void scriptsChanged();
    void addInsChanged();
    void commandsChanged();

private:
    struct Command {
        QString id, label, tooltip, addIn, language;
        QJSValue fn, refresh;
    };
    struct AddInState {
        bool loaded = false;
        QString error;
        QStringList output;
    };
    QString path(const QString &name, bool addIn) const;
    QVariantMap runJs(const QString &code, QObject *app, int timeoutMs);
    QVariantMap guardedCall(const std::function<QJSValue()> &call, int timeoutMs);
    void registerCommand(const QString &id, const QString &label, const QString &tooltip, const QString &language,
                         const QJSValue &fn);
    void dropCommands(const QString &addIn);
    QStringList disabledAddIns() const;

    QJSEngine *m_engine;
    QPointer<QObject> m_app;
    QList<Command> m_commands;
    QHash<QString, AddInState> m_addIns;
    QString m_currentAddIn;
    QStringList m_log;
    bool m_started = false;
};
