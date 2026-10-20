// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#pragma once
#include <QString>
#include <QStringList>
#include <QVariantMap>

class FissionApplication;

// Headless Model Context Protocol server over stdio (JSON-RPC 2.0).
// Supports Content-Length framing (MCP / LSP) and newline-delimited JSON.
// Tools wrap CommandRegistry — same surface scripts/CLI will use.
class McpServer {
public:
    struct Options {
        QString openPath;           // optional design to load at start
        QStringList allowPaths;     // extra folders agents may read/write (--allow-path)
        bool allowAnyPath = false;
    };

    // Blocks on stdin until EOF / shutdown. Returns process exit code.
    static int run(FissionApplication *app, const Options &opts = {});

private:
    static QByteArray readMessage();
    static void writeMessage(const QByteArray &json);
    static QVariantMap handleRequest(const QVariantMap &req, FissionApplication *app, const Options &opts);
    static QVariantMap toolResult(const QVariant &structured, bool isError = false);
    static QVariantMap callTool(const QString &name, const QVariantMap &args, FissionApplication *app,
                                const Options &opts);
    static bool pathAllowed(const QString &path, const Options &opts, FissionApplication *app);
    static QString resolvePath(const QString &path);
};
