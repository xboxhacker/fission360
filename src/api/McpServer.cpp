// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#include "McpServer.h"
#include "CommandRegistry.h"
#include "FissionApplication.h"
#include "kernel/FissionDocument.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QVariantList>

#include <cstdio>
#include <iostream>

#ifdef Q_OS_WIN
#include <fcntl.h>
#include <io.h>
#endif

namespace {

QJsonValue toJson(const QVariant &v) {
    return QJsonValue::fromVariant(v);
}

QVariantMap jsonRpcError(const QVariant &id, int code, const QString &message) {
    return QVariantMap{{QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
                       {QStringLiteral("id"), id},
                       {QStringLiteral("error"),
                        QVariantMap{{QStringLiteral("code"), code}, {QStringLiteral("message"), message}}}};
}

QVariantMap jsonRpcResult(const QVariant &id, const QVariant &result) {
    return QVariantMap{{QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
                       {QStringLiteral("id"), id},
                       {QStringLiteral("result"), result}};
}

QVariantList toolDefs() {
    auto tool = [](const QString &name, const QString &desc, const QVariantMap &schema,
                   bool readOnly = false) {
        QVariantMap annotations{{QStringLiteral("readOnlyHint"), readOnly},
                                {QStringLiteral("destructiveHint"), !readOnly},
                                {QStringLiteral("openWorldHint"), true}};
        return QVariantMap{{QStringLiteral("name"), name},
                           {QStringLiteral("description"), desc},
                           {QStringLiteral("inputSchema"), schema},
                           {QStringLiteral("annotations"), annotations}};
    };
    const QVariantMap emptySchema{{QStringLiteral("type"), QStringLiteral("object")},
                                  {QStringLiteral("properties"), QVariantMap()}};
    QVariantList tools;
    tools << tool(QStringLiteral("list_commands"),
                  QStringLiteral("List Fission command-registry entries (id, label, group, params)."),
                  QVariantMap{{QStringLiteral("type"), QStringLiteral("object")},
                              {QStringLiteral("properties"),
                               QVariantMap{{QStringLiteral("filter"),
                                            QVariantMap{{QStringLiteral("type"), QStringLiteral("string")}}}}}},
                  true);
    tools << tool(QStringLiteral("execute"),
                  QStringLiteral("Run a registered command by id with JSON params."),
                  QVariantMap{
                      {QStringLiteral("type"), QStringLiteral("object")},
                      {QStringLiteral("properties"),
                       QVariantMap{{QStringLiteral("command"),
                                    QVariantMap{{QStringLiteral("type"), QStringLiteral("string")}}},
                                   {QStringLiteral("params"),
                                    QVariantMap{{QStringLiteral("type"), QStringLiteral("object")}}}}},
                      {QStringLiteral("required"), QVariantList{QStringLiteral("command")}}});
    tools << tool(QStringLiteral("batch"),
                  QStringLiteral("Run commands in order; stop on first error (no auto-rollback yet)."),
                  QVariantMap{
                      {QStringLiteral("type"), QStringLiteral("object")},
                      {QStringLiteral("properties"),
                       QVariantMap{{QStringLiteral("commands"),
                                    QVariantMap{{QStringLiteral("type"), QStringLiteral("array")}}}}},
                      {QStringLiteral("required"), QVariantList{QStringLiteral("commands")}}});
    tools << tool(QStringLiteral("inspect_design"),
                  QStringLiteral("Bodies, parameters, and timeline summary for the active design."),
                  emptySchema, true);
    tools << tool(QStringLiteral("measure"),
                  QStringLiteral("Measure bodies (volume/area/CoM/bbox). Optional bodies: [indices]."),
                  QVariantMap{{QStringLiteral("type"), QStringLiteral("object")},
                              {QStringLiteral("properties"),
                               QVariantMap{{QStringLiteral("bodies"),
                                            QVariantMap{{QStringLiteral("type"), QStringLiteral("array")}}}}}},
                  true);
    tools << tool(QStringLiteral("export"),
                  QStringLiteral("Export the active design (STEP/STL/3MF/…)."),
                  QVariantMap{
                      {QStringLiteral("type"), QStringLiteral("object")},
                      {QStringLiteral("properties"),
                       QVariantMap{{QStringLiteral("path"),
                                    QVariantMap{{QStringLiteral("type"), QStringLiteral("string")}}},
                                   {QStringLiteral("format"),
                                    QVariantMap{{QStringLiteral("type"), QStringLiteral("string")}}}}},
                      {QStringLiteral("required"), QVariantList{QStringLiteral("path")}}});
    tools << tool(QStringLiteral("new_design"), QStringLiteral("Start an empty design."), emptySchema);
    tools << tool(QStringLiteral("open"), QStringLiteral("Open a .f360 / STEP / mesh file."),
                  QVariantMap{
                      {QStringLiteral("type"), QStringLiteral("object")},
                      {QStringLiteral("properties"),
                       QVariantMap{{QStringLiteral("path"),
                                    QVariantMap{{QStringLiteral("type"), QStringLiteral("string")}}}}},
                      {QStringLiteral("required"), QVariantList{QStringLiteral("path")}}});
    tools << tool(QStringLiteral("save"), QStringLiteral("Save the active design."),
                  QVariantMap{{QStringLiteral("type"), QStringLiteral("object")},
                              {QStringLiteral("properties"),
                               QVariantMap{{QStringLiteral("path"),
                                            QVariantMap{{QStringLiteral("type"), QStringLiteral("string")}}}}}});
    tools << tool(QStringLiteral("undo"), QStringLiteral("Undo one change."), emptySchema);
    tools << tool(QStringLiteral("redo"), QStringLiteral("Redo one change."), emptySchema);
    tools << tool(QStringLiteral("set_parameter"),
                  QStringLiteral("Create or change a user parameter and rebuild."),
                  QVariantMap{
                      {QStringLiteral("type"), QStringLiteral("object")},
                      {QStringLiteral("properties"),
                       QVariantMap{{QStringLiteral("name"),
                                    QVariantMap{{QStringLiteral("type"), QStringLiteral("string")}}},
                                   {QStringLiteral("expression"),
                                    QVariantMap{{QStringLiteral("type"), QStringLiteral("string")}}},
                                   {QStringLiteral("value"),
                                    QVariantMap{{QStringLiteral("type"), QStringLiteral("string")}}},
                                   {QStringLiteral("unit"),
                                    QVariantMap{{QStringLiteral("type"), QStringLiteral("string")}}}}},
                      {QStringLiteral("required"), QVariantList{QStringLiteral("name")}}});
    return tools;
}

} // namespace

QString McpServer::resolvePath(const QString &path) {
    if (path.isEmpty()) return {};
    return QFileInfo(path).absoluteFilePath();
}

bool McpServer::pathAllowed(const QString &path, const Options &opts, FissionApplication *app) {
    if (opts.allowAnyPath) return true;
    const QString abs = resolvePath(path);
    if (abs.isEmpty()) return false;
    QStringList roots = opts.allowPaths;
    roots << QDir::tempPath();
    roots << QDir::homePath();
    if (app && app->activeDocument() && !app->activeDocument()->filePath().isEmpty())
        roots << QFileInfo(app->activeDocument()->filePath()).absolutePath();
    for (const QString &r : roots) {
        if (r.isEmpty()) continue;
        const QString root = QFileInfo(r).absoluteFilePath();
        if (abs.startsWith(root, Qt::CaseInsensitive)) return true;
    }
    return false;
}

QByteArray McpServer::readMessage() {
    // Prefer Content-Length framing; fall back to one JSON object per line.
    std::string first;
    if (!std::getline(std::cin, first)) return {};
    // Strip CR
    if (!first.empty() && first.back() == '\r') first.pop_back();

    if (first.rfind("Content-Length:", 0) == 0 || first.rfind("content-length:", 0) == 0) {
        int len = 0;
        const auto colon = first.find(':');
        if (colon != std::string::npos)
            len = std::atoi(first.c_str() + colon + 1);
        // Read remaining headers until blank line
        std::string line;
        while (std::getline(std::cin, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.empty()) break;
            if (line.rfind("Content-Length:", 0) == 0 || line.rfind("content-length:", 0) == 0) {
                const auto c = line.find(':');
                if (c != std::string::npos) len = std::atoi(line.c_str() + c + 1);
            }
        }
        if (len <= 0 || len > 64 * 1024 * 1024) return {};
        QByteArray body(len, Qt::Uninitialized);
        int got = 0;
        while (got < len) {
            const int n = static_cast<int>(std::fread(body.data() + got, 1, size_t(len - got), stdin));
            if (n <= 0) break;
            got += n;
        }
        if (got != len) return {};
        return body;
    }

    // NDJSON line
    return QByteArray::fromStdString(first);
}

void McpServer::writeMessage(const QByteArray &json) {
    const QByteArray header = QByteArray("Content-Length: ") + QByteArray::number(json.size()) + "\r\n\r\n";
    std::fwrite(header.constData(), 1, size_t(header.size()), stdout);
    std::fwrite(json.constData(), 1, size_t(json.size()), stdout);
    std::fflush(stdout);
}

QVariantMap McpServer::toolResult(const QVariant &structured, bool isError) {
    const QByteArray text = QJsonDocument::fromVariant(structured).toJson(QJsonDocument::Compact);
    QVariantMap contentItem{{QStringLiteral("type"), QStringLiteral("text")},
                            {QStringLiteral("text"), QString::fromUtf8(text)}};
    QVariantMap out{{QStringLiteral("content"), QVariantList{contentItem}}};
    if (isError) out.insert(QStringLiteral("isError"), true);
    return out;
}

QVariantMap McpServer::callTool(const QString &name, const QVariantMap &args, FissionApplication *app,
                                const Options &opts) {
    auto *reg = CommandRegistry::instance();

    if (name == QLatin1String("list_commands")) {
        return toolResult(QVariantMap{{QStringLiteral("commands"),
                                       reg->listCommands(args.value(QStringLiteral("filter")).toString())}});
    }
    if (name == QLatin1String("execute")) {
        const QString cmd = args.value(QStringLiteral("command")).toString();
        const QVariantMap params = args.value(QStringLiteral("params")).toMap();
        // Path checks for file commands
        if ((cmd == QLatin1String("doc.open") || cmd == QLatin1String("doc.save")
             || cmd == QLatin1String("doc.export"))
            && params.contains(QStringLiteral("path"))) {
            const QString p = params.value(QStringLiteral("path")).toString();
            if (!pathAllowed(p, opts, app))
                return toolResult(QVariantMap{{QStringLiteral("error"),
                                               QStringLiteral("Path not allowed: ") + p
                                                   + QStringLiteral(" (use --allow-path)")}},
                                  true);
        }
        const QVariantMap r = reg->execute(cmd, params);
        return toolResult(r, !r.value(QStringLiteral("ok")).toBool());
    }
    if (name == QLatin1String("batch")) {
        const QVariantList steps = args.value(QStringLiteral("commands")).toList();
        QVariantList results;
        for (int i = 0; i < steps.size(); ++i) {
            const QVariantMap step = steps[i].toMap();
            const QString cmd = step.value(QStringLiteral("command")).toString();
            const QVariantMap params = step.value(QStringLiteral("params")).toMap();
            const QVariantMap r = reg->execute(cmd, params);
            results.append(QVariantMap{{QStringLiteral("index"), i},
                                       {QStringLiteral("command"), cmd},
                                       {QStringLiteral("result"), r}});
            if (!r.value(QStringLiteral("ok")).toBool()) {
                return toolResult(QVariantMap{{QStringLiteral("ok"), false},
                                              {QStringLiteral("failed"),
                                               QVariantMap{{QStringLiteral("index"), i},
                                                           {QStringLiteral("command"), cmd},
                                                           {QStringLiteral("error"),
                                                            r.value(QStringLiteral("error"))}}},
                                              {QStringLiteral("results"), results}},
                                  true);
            }
        }
        return toolResult(QVariantMap{{QStringLiteral("ok"), true}, {QStringLiteral("results"), results}});
    }
    if (name == QLatin1String("inspect_design"))
        return toolResult(reg->execute(QStringLiteral("doc.inspect"), {}));
    if (name == QLatin1String("measure"))
        return toolResult(reg->execute(QStringLiteral("measure.bodies"), args));
    if (name == QLatin1String("export")) {
        const QString p = args.value(QStringLiteral("path")).toString();
        if (!pathAllowed(p, opts, app))
            return toolResult(QVariantMap{{QStringLiteral("error"), QStringLiteral("Path not allowed")}}, true);
        const QVariantMap r = reg->execute(QStringLiteral("doc.export"), args);
        return toolResult(r, !r.value(QStringLiteral("ok")).toBool());
    }
    if (name == QLatin1String("new_design"))
        return toolResult(reg->execute(QStringLiteral("doc.new"), {}));
    if (name == QLatin1String("open")) {
        const QString p = args.value(QStringLiteral("path")).toString();
        if (!pathAllowed(p, opts, app))
            return toolResult(QVariantMap{{QStringLiteral("error"), QStringLiteral("Path not allowed")}}, true);
        const QVariantMap r = reg->execute(QStringLiteral("doc.open"), args);
        return toolResult(r, !r.value(QStringLiteral("ok")).toBool());
    }
    if (name == QLatin1String("save")) {
        if (args.contains(QStringLiteral("path"))
            && !pathAllowed(args.value(QStringLiteral("path")).toString(), opts, app))
            return toolResult(QVariantMap{{QStringLiteral("error"), QStringLiteral("Path not allowed")}}, true);
        const QVariantMap r = reg->execute(QStringLiteral("doc.save"), args);
        return toolResult(r, !r.value(QStringLiteral("ok")).toBool());
    }
    if (name == QLatin1String("undo"))
        return toolResult(reg->execute(QStringLiteral("edit.undo"), {}));
    if (name == QLatin1String("redo"))
        return toolResult(reg->execute(QStringLiteral("edit.redo"), {}));
    if (name == QLatin1String("set_parameter")) {
        const QVariantMap r = reg->execute(QStringLiteral("param.set"), args);
        return toolResult(r, !r.value(QStringLiteral("ok")).toBool());
    }
    return toolResult(QVariantMap{{QStringLiteral("error"), QStringLiteral("Unknown tool: ") + name}}, true);
}

QVariantMap McpServer::handleRequest(const QVariantMap &req, FissionApplication *app, const Options &opts) {
    const QString method = req.value(QStringLiteral("method")).toString();
    const QVariant id = req.value(QStringLiteral("id"));
    const QVariantMap params = req.value(QStringLiteral("params")).toMap();
    const bool isNotification = !req.contains(QStringLiteral("id"));

    if (method == QLatin1String("initialize")) {
        QVariantMap caps{{QStringLiteral("tools"), QVariantMap{{QStringLiteral("listChanged"), false}}}};
        QVariantMap serverInfo{{QStringLiteral("name"), QStringLiteral("fission360")},
                               {QStringLiteral("version"), app ? app->version() : QString()}};
        return jsonRpcResult(id,
                             QVariantMap{{QStringLiteral("protocolVersion"), QStringLiteral("2024-11-05")},
                                         {QStringLiteral("capabilities"), caps},
                                         {QStringLiteral("serverInfo"), serverInfo},
                                         {QStringLiteral("instructions"),
                                          QStringLiteral("Fission 360 headless MCP. Use list_commands then "
                                                         "execute with dotted ids (doc.*, feature.add, "
                                                         "measure.bodies, param.set, edit.undo). Paths "
                                                         "must be under home, temp, the open design folder, "
                                                         "or --allow-path.")}});
    }
    if (method == QLatin1String("notifications/initialized") || method.startsWith(QLatin1String("notifications/"))) {
        return {}; // no response for notifications
    }
    if (method == QLatin1String("ping"))
        return jsonRpcResult(id, QVariantMap());
    if (method == QLatin1String("tools/list"))
        return jsonRpcResult(id, QVariantMap{{QStringLiteral("tools"), toolDefs()}});
    if (method == QLatin1String("tools/call")) {
        const QString name = params.value(QStringLiteral("name")).toString();
        const QVariantMap args = params.value(QStringLiteral("arguments")).toMap();
        return jsonRpcResult(id, callTool(name, args, app, opts));
    }
    if (method == QLatin1String("resources/list"))
        return jsonRpcResult(id, QVariantMap{{QStringLiteral("resources"), QVariantList()}});
    if (method == QLatin1String("prompts/list"))
        return jsonRpcResult(id, QVariantMap{{QStringLiteral("prompts"), QVariantList()}});

    if (isNotification) return {};
    return jsonRpcError(id, -32601, QStringLiteral("Method not found: ") + method);
}

int McpServer::run(FissionApplication *app, const Options &opts) {
#ifdef Q_OS_WIN
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
#endif
    // stderr only for diagnostics — stdout is protocol
    std::fprintf(stderr, "Fission 360 MCP %s (stdio). Allow: home, temp",
                 qPrintable(app ? app->version() : QString()));
    for (const QString &p : opts.allowPaths)
        std::fprintf(stderr, ", %s", qPrintable(p));
    if (opts.allowAnyPath) std::fprintf(stderr, ", *");
    std::fprintf(stderr, "\n");
    std::fflush(stderr);

    CommandRegistry::instance()->registerBuiltins(app);
    if (!opts.openPath.isEmpty()) {
        if (!pathAllowed(opts.openPath, opts, app) && !opts.allowAnyPath) {
            std::fprintf(stderr, "Refusing --in path outside allow list: %s\n", qPrintable(opts.openPath));
            return 2;
        }
        if (!app->openDocument(opts.openPath)) {
            std::fprintf(stderr, "Failed to open %s: %s\n", qPrintable(opts.openPath),
                         qPrintable(app->activeDocument() ? app->activeDocument()->lastError() : QString()));
            return 2;
        }
    }

    while (true) {
        const QByteArray raw = readMessage();
        if (raw.isEmpty()) break;
        QJsonParseError err;
        const QJsonDocument doc = QJsonDocument::fromJson(raw, &err);
        if (err.error != QJsonParseError::NoError || !doc.isObject()) {
            const QVariantMap bad = jsonRpcError(QVariant(), -32700, QStringLiteral("Parse error"));
            writeMessage(QJsonDocument::fromVariant(bad).toJson(QJsonDocument::Compact));
            continue;
        }
        const QVariantMap req = doc.object().toVariantMap();
        // shutdown
        if (req.value(QStringLiteral("method")).toString() == QLatin1String("shutdown")) {
            writeMessage(QJsonDocument::fromVariant(jsonRpcResult(req.value(QStringLiteral("id")), QVariant()))
                             .toJson(QJsonDocument::Compact));
            continue;
        }
        if (req.value(QStringLiteral("method")).toString() == QLatin1String("exit"))
            return 0;

        const QVariantMap resp = handleRequest(req, app, opts);
        if (resp.isEmpty()) continue; // notification
        writeMessage(QJsonDocument::fromVariant(resp).toJson(QJsonDocument::Compact));
    }
    return 0;
}
