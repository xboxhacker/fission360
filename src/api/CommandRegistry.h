// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#pragma once
#include <QObject>
#include <QString>
#include <QVariantMap>
#include <QVariantList>
#include <functional>

class FissionApplication;

// Named command table shared by scripts, CLI, and MCP. Handlers run against the active
// FissionApplication / document — no dialogs. Ids are dotted (e.g. "doc.export", "feature.add").
class CommandRegistry : public QObject {
    Q_OBJECT
    Q_PROPERTY(int commandCount READ commandCount NOTIFY commandsChanged)

public:
    using Handler = std::function<QVariantMap(const QVariantMap &args)>;

    struct Entry {
        QString id;
        QString label;
        QString group;        // Document | Feature | Measure | Edit | System
        QString description;
        QString paramsDoc;    // short JSON-ish hint for agents
        bool readOnly = false;
        Handler handler;
    };

    static CommandRegistry *instance();

    int commandCount() const { return m_entries.size(); }
    Q_INVOKABLE QVariantList listCommands(const QString &filter = QString()) const;
    Q_INVOKABLE QVariantMap execute(const QString &id, const QVariantMap &args = {});
    Q_INVOKABLE bool hasCommand(const QString &id) const;
    Q_INVOKABLE QVariantMap commandInfo(const QString &id) const;

    void registerCommand(const Entry &entry);
    void registerCommand(const QString &id, const QString &label, const QString &group,
                         const QString &description, bool readOnly, const QString &paramsDoc,
                         Handler handler);

    // Built-in headless commands (doc / feature / measure / edit). Call once at startup.
    void registerBuiltins(FissionApplication *app);

signals:
    void commandsChanged();

private:
    explicit CommandRegistry(QObject *parent = nullptr);
    QVariantMap ok(const QVariantMap &result = {}) const;
    QVariantMap fail(const QString &error) const;
    FissionApplication *app() const { return m_app; }

    QList<Entry> m_entries;
    FissionApplication *m_app = nullptr;
};
