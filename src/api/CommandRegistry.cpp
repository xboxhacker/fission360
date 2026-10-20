// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#include "CommandRegistry.h"
#include "FissionApplication.h"
#include "kernel/FissionBody.h"
#include "kernel/FissionDocument.h"
#include "kernel/FissionFeature.h"
#include "kernel/FissionParameter.h"

CommandRegistry *CommandRegistry::instance() {
    static CommandRegistry *r = new CommandRegistry();
    return r;
}

CommandRegistry::CommandRegistry(QObject *parent) : QObject(parent) {}

QVariantMap CommandRegistry::ok(const QVariantMap &result) const {
    QVariantMap m = result;
    m.insert(QStringLiteral("ok"), true);
    return m;
}

QVariantMap CommandRegistry::fail(const QString &error) const {
    return QVariantMap{{QStringLiteral("ok"), false}, {QStringLiteral("error"), error}};
}

void CommandRegistry::registerCommand(const Entry &entry) {
    for (int i = 0; i < m_entries.size(); ++i) {
        if (m_entries[i].id == entry.id) {
            m_entries[i] = entry;
            emit commandsChanged();
            return;
        }
    }
    m_entries.append(entry);
    emit commandsChanged();
}

void CommandRegistry::registerCommand(const QString &id, const QString &label, const QString &group,
                                      const QString &description, bool readOnly, const QString &paramsDoc,
                                      Handler handler) {
    Entry e;
    e.id = id;
    e.label = label;
    e.group = group;
    e.description = description;
    e.readOnly = readOnly;
    e.paramsDoc = paramsDoc;
    e.handler = std::move(handler);
    registerCommand(e);
}

bool CommandRegistry::hasCommand(const QString &id) const {
    for (const auto &e : m_entries)
        if (e.id == id) return true;
    return false;
}

QVariantMap CommandRegistry::commandInfo(const QString &id) const {
    for (const auto &e : m_entries) {
        if (e.id != id) continue;
        return QVariantMap{{QStringLiteral("id"), e.id},
                           {QStringLiteral("label"), e.label},
                           {QStringLiteral("group"), e.group},
                           {QStringLiteral("description"), e.description},
                           {QStringLiteral("params"), e.paramsDoc},
                           {QStringLiteral("readOnly"), e.readOnly}};
    }
    return {};
}

QVariantList CommandRegistry::listCommands(const QString &filter) const {
    const QString f = filter.trimmed().toLower();
    QVariantList out;
    for (const auto &e : m_entries) {
        if (!f.isEmpty()) {
            const QString blob = (e.id + QLatin1Char(' ') + e.label + QLatin1Char(' ') + e.group
                                  + QLatin1Char(' ') + e.description)
                                     .toLower();
            if (!blob.contains(f)) continue;
        }
        out.append(QVariantMap{{QStringLiteral("id"), e.id},
                               {QStringLiteral("label"), e.label},
                               {QStringLiteral("group"), e.group},
                               {QStringLiteral("description"), e.description},
                               {QStringLiteral("params"), e.paramsDoc},
                               {QStringLiteral("readOnly"), e.readOnly}});
    }
    return out;
}

QVariantMap CommandRegistry::execute(const QString &id, const QVariantMap &args) {
    for (const auto &e : m_entries) {
        if (e.id != id) continue;
        if (!e.handler) return fail(QStringLiteral("Command has no handler: ") + id);
        try {
            return e.handler(args);
        } catch (const std::exception &ex) {
            return fail(QString::fromUtf8(ex.what()));
        } catch (...) {
            return fail(QStringLiteral("Command threw: ") + id);
        }
    }
    return fail(QStringLiteral("Unknown command: ") + id);
}

void CommandRegistry::registerBuiltins(FissionApplication *application) {
    m_app = application;
    auto doc = [this]() -> FissionDocument * {
        return m_app ? m_app->activeDocument() : nullptr;
    };

    registerCommand(QStringLiteral("system.version"), QStringLiteral("Version"), QStringLiteral("System"),
                    QStringLiteral("Fission 360 version string"), true, QString(),
                    [this](const QVariantMap &) {
                        return ok({{QStringLiteral("version"), m_app ? m_app->version() : QString()}});
                    });

    registerCommand(QStringLiteral("doc.new"), QStringLiteral("New Design"), QStringLiteral("Document"),
                    QStringLiteral("Replace the active tab with an empty design"), false, QString(),
                    [this](const QVariantMap &) {
                        if (!m_app) return fail(QStringLiteral("No application"));
                        m_app->newDocument();
                        return ok({{QStringLiteral("name"), m_app->activeDocument()
                                                                ? m_app->activeDocument()->name()
                                                                : QString()}});
                    });

    registerCommand(QStringLiteral("doc.open"), QStringLiteral("Open Design"), QStringLiteral("Document"),
                    QStringLiteral("Open a .f360 / STEP / mesh file"), false,
                    QStringLiteral("{path}"),
                    [this](const QVariantMap &a) {
                        if (!m_app) return fail(QStringLiteral("No application"));
                        const QString path = a.value(QStringLiteral("path")).toString();
                        if (path.isEmpty()) return fail(QStringLiteral("path required"));
                        if (!m_app->openDocument(path))
                            return fail(m_app->activeDocument() ? m_app->activeDocument()->lastError()
                                                                : QStringLiteral("Open failed"));
                        auto *d = m_app->activeDocument();
                        return ok({{QStringLiteral("path"), d ? d->filePath() : path},
                                   {QStringLiteral("bodies"), d ? d->bodyCount() : 0}});
                    });

    registerCommand(QStringLiteral("doc.save"), QStringLiteral("Save Design"), QStringLiteral("Document"),
                    QStringLiteral("Save the active design (.f360)"), false,
                    QStringLiteral("{path?}"),
                    [this](const QVariantMap &a) {
                        if (!m_app) return fail(QStringLiteral("No application"));
                        const QString path = a.value(QStringLiteral("path")).toString();
                        if (!m_app->saveDocument(path))
                            return fail(m_app->activeDocument() ? m_app->activeDocument()->lastError()
                                                                : QStringLiteral("Save failed"));
                        auto *d = m_app->activeDocument();
                        return ok({{QStringLiteral("path"), d ? d->filePath() : path}});
                    });

    registerCommand(QStringLiteral("doc.export"), QStringLiteral("Export"), QStringLiteral("Document"),
                    QStringLiteral("Export STEP / STL / 3MF / IGES / OBJ / …"), false,
                    QStringLiteral("{path, format?}"),
                    [this, doc](const QVariantMap &a) {
                        auto *d = doc();
                        if (!d) return fail(QStringLiteral("No document"));
                        const QString path = a.value(QStringLiteral("path")).toString();
                        if (path.isEmpty()) return fail(QStringLiteral("path required"));
                        const QString format = a.value(QStringLiteral("format")).toString();
                        const QString out = d->exportFile(path, format);
                        if (out.isEmpty()) return fail(d->lastError().isEmpty() ? QStringLiteral("Export failed")
                                                                               : d->lastError());
                        return ok({{QStringLiteral("path"), out}});
                    });

    registerCommand(QStringLiteral("doc.rebuild"), QStringLiteral("Rebuild"), QStringLiteral("Document"),
                    QStringLiteral("Recompute the timeline"), false, QString(),
                    [doc, this](const QVariantMap &) {
                        auto *d = doc();
                        if (!d) return fail(QStringLiteral("No document"));
                        d->rebuild();
                        return ok({{QStringLiteral("bodies"), d->bodyCount()}});
                    });

    registerCommand(QStringLiteral("doc.inspect"), QStringLiteral("Inspect Design"), QStringLiteral("Document"),
                    QStringLiteral("Bodies, parameters, timeline summary"), true, QString(),
                    [this, doc](const QVariantMap &) {
                        auto *d = doc();
                        if (!d) return fail(QStringLiteral("No document"));
                        QVariantList bodies;
                        for (int i = 0; i < d->bodyCount(); ++i) {
                            QVariantMap bp = d->bodyProperties(i);
                            bp.insert(QStringLiteral("index"), i);
                            if (auto *b = d->body(i)) {
                                bp.insert(QStringLiteral("name"), b->name());
                                bp.insert(QStringLiteral("visible"), b->isVisible());
                            }
                            bodies.append(bp);
                        }
                        QVariantList params;
                        for (QObject *o : d->parameterObjects()) {
                            auto *p = qobject_cast<FissionParameter *>(o);
                            if (!p) continue;
                            params.append(QVariantMap{{QStringLiteral("name"), p->name()},
                                                      {QStringLiteral("expression"), p->expression()},
                                                      {QStringLiteral("unit"), p->unit()},
                                                      {QStringLiteral("value"), p->value()}});
                        }
                        QVariantList features = d->featureList();
                        return ok({{QStringLiteral("name"), d->name()},
                                   {QStringLiteral("path"), d->filePath()},
                                   {QStringLiteral("modified"), d->isModified()},
                                   {QStringLiteral("bodies"), bodies},
                                   {QStringLiteral("parameters"), params},
                                   {QStringLiteral("features"), features},
                                   {QStringLiteral("bodyCount"), d->bodyCount()},
                                   {QStringLiteral("featureCount"), features.size()}});
                    });

    registerCommand(QStringLiteral("measure.bodies"), QStringLiteral("Measure Bodies"), QStringLiteral("Measure"),
                    QStringLiteral("Volume / area / CoM / bbox for bodies"), true,
                    QStringLiteral("{bodies?: [index, …]}  empty = all"),
                    [this, doc](const QVariantMap &a) {
                        auto *d = doc();
                        if (!d) return fail(QStringLiteral("No document"));
                        QVariantList indices = a.value(QStringLiteral("bodies")).toList();
                        if (indices.isEmpty()) {
                            for (int i = 0; i < d->bodyCount(); ++i) indices.append(i);
                        }
                        QVariantList items;
                        double vol = 0, area = 0;
                        for (const QVariant &v : indices) {
                            const int i = v.toInt();
                            QVariantMap bp = d->bodyProperties(i);
                            bp.insert(QStringLiteral("index"), i);
                            items.append(bp);
                            vol += bp.value(QStringLiteral("volume")).toDouble();
                            area += bp.value(QStringLiteral("area")).toDouble();
                        }
                        return ok({{QStringLiteral("items"), items},
                                   {QStringLiteral("volume_mm3"), vol},
                                   {QStringLiteral("area_mm2"), area}});
                    });

    registerCommand(QStringLiteral("measure.selection"), QStringLiteral("Measure Refs"), QStringLiteral("Measure"),
                    QStringLiteral("Measure face/edge/vertex refs (same shape as UI measure)"), true,
                    QStringLiteral("{refs: [{…}]}"),
                    [this, doc](const QVariantMap &a) {
                        auto *d = doc();
                        if (!d) return fail(QStringLiteral("No document"));
                        const QVariantList refs = a.value(QStringLiteral("refs")).toList();
                        if (refs.isEmpty()) return fail(QStringLiteral("refs required"));
                        return ok(d->measure(refs));
                    });

    registerCommand(QStringLiteral("edit.undo"), QStringLiteral("Undo"), QStringLiteral("Edit"),
                    QStringLiteral("Undo one change"), false, QString(),
                    [this, doc](const QVariantMap &) {
                        auto *d = doc();
                        if (!d) return fail(QStringLiteral("No document"));
                        if (!d->canUndo()) return fail(QStringLiteral("Nothing to undo"));
                        d->undo();
                        return ok();
                    });

    registerCommand(QStringLiteral("edit.redo"), QStringLiteral("Redo"), QStringLiteral("Edit"),
                    QStringLiteral("Redo one change"), false, QString(),
                    [this, doc](const QVariantMap &) {
                        auto *d = doc();
                        if (!d) return fail(QStringLiteral("No document"));
                        if (!d->canRedo()) return fail(QStringLiteral("Nothing to redo"));
                        d->redo();
                        return ok();
                    });

    registerCommand(QStringLiteral("param.set"), QStringLiteral("Set Parameter"), QStringLiteral("Document"),
                    QStringLiteral("Create or change a user parameter and rebuild"), false,
                    QStringLiteral("{name, expression|value, unit?, comment?}"),
                    [this, doc](const QVariantMap &a) {
                        auto *d = doc();
                        if (!d) return fail(QStringLiteral("No document"));
                        const QString name = a.value(QStringLiteral("name")).toString();
                        if (name.isEmpty()) return fail(QStringLiteral("name required"));
                        QString expr = a.value(QStringLiteral("expression")).toString();
                        if (expr.isEmpty() && a.contains(QStringLiteral("value")))
                            expr = a.value(QStringLiteral("value")).toString();
                        if (expr.isEmpty()) return fail(QStringLiteral("expression or value required"));
                        const QString unit = a.value(QStringLiteral("unit")).toString();
                        const QString comment = a.value(QStringLiteral("comment")).toString();
                        FissionParameter *p = d->parameter(name);
                        if (p) {
                            p->setExpression(expr);
                            if (!unit.isEmpty()) p->setUnit(unit);
                            if (!comment.isEmpty()) p->setComment(comment);
                        } else {
                            p = d->addParameter(name, expr, unit, comment);
                            if (!p) return fail(d->lastError().isEmpty() ? QStringLiteral("addParameter failed")
                                                                        : d->lastError());
                        }
                        d->rebuild();
                        return ok({{QStringLiteral("name"), p->name()},
                                   {QStringLiteral("value"), p->value()},
                                   {QStringLiteral("expression"), p->expression()}});
                    });

    registerCommand(QStringLiteral("feature.add"), QStringLiteral("Add Feature"), QStringLiteral("Feature"),
                    QStringLiteral("Add a timeline feature by type + props (then rebuild)"), false,
                    QStringLiteral("{type, props?}"),
                    [this, doc](const QVariantMap &a) {
                        auto *d = doc();
                        if (!d) return fail(QStringLiteral("No document"));
                        const QString type = a.value(QStringLiteral("type")).toString();
                        if (type.isEmpty()) return fail(QStringLiteral("type required"));
                        QVariantMap props = a.value(QStringLiteral("props")).toMap();
                        if (props.isEmpty()) {
                            // Allow flat args except type/props
                            props = a;
                            props.remove(QStringLiteral("type"));
                            props.remove(QStringLiteral("props"));
                        }
                        FissionFeature *f = d->addFeature(type, props);
                        if (!f) return fail(d->lastError().isEmpty() ? QStringLiteral("addFeature failed")
                                                                    : d->lastError());
                        d->rebuild();
                        return ok({{QStringLiteral("feature"), f->id()},
                                   {QStringLiteral("type"), f->type()},
                                   {QStringLiteral("name"), f->name()},
                                   {QStringLiteral("bodies"), d->bodyCount()}});
                    });

    registerCommand(QStringLiteral("body.list"), QStringLiteral("List Bodies"), QStringLiteral("Document"),
                    QStringLiteral("Body names and indices"), true, QString(),
                    [this, doc](const QVariantMap &) {
                        auto *d = doc();
                        if (!d) return fail(QStringLiteral("No document"));
                        QVariantList bodies;
                        for (int i = 0; i < d->bodyCount(); ++i) {
                            auto *b = d->body(i);
                            bodies.append(QVariantMap{{QStringLiteral("index"), i},
                                                      {QStringLiteral("name"), b ? b->name() : QString()},
                                                      {QStringLiteral("visible"), b ? b->isVisible() : false}});
                        }
                        return ok({{QStringLiteral("bodies"), bodies}});
                    });
}
