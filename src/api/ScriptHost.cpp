// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#include "ScriptHost.h"
#include "FissionApplication.h"
#include "PythonHost.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJSEngine>
#include <QSettings>
#include <QStandardPaths>
#include <QTextStream>
#include <condition_variable>
#include <mutex>
#include <thread>

namespace {

const char *kSampleBox = R"(// Sample: a parametric box with a hole, built through the document API
const size = 40
doc.addParameter("BoxSize", size + " mm", "mm", "Made by a script")
const sk = doc.addFeature("Sketch", { plane: "XY" })
sk.addRectangle(0, 0, size, size)
const ext = doc.addFeature("Extrude", { sketch: sk.id, distance: "BoxSize / 2" })
print("Extrude:", ext.error || "ok", "- bodies:", doc.bodyCount())
app.goHome()
)";

const char *kSampleReport = R"(// Sample: report every body's volume and mass
for (let i = 0; i < doc.bodyCount(); ++i) {
    const p = doc.bodyProperties(i)
    print(p.name + ":", doc.formatLength(p.volume, 2, 3), "-", doc.formatMass(p.mass, 2))
}
print(doc.featureList("").length, "features in the timeline")
)";

const char *kSampleSection = R"(// Sample: save a section along each origin plane (through the origin) in the browser
for (const plane of ["XY", "XZ", "YZ"])
    doc.addAnalysis({ type: "section", plane: plane, offset: 0, flip: false, visible: false })
print(doc.analyses.length, "section analyses in the browser")
)";

const char *kSamplePython = R"(# Sample (Python): a row of cylinders, and the adsk-style entry point
import adsk.core, adsk.fusion

product = adsk.core.Application.get().activeProduct
design = adsk.fusion.Design.cast(product)
for i in range(4):
    sk = design.add_feature("Sketch", {"plane": "XY"})
    sk.add_circle(i * 25, 0, 8)
    design.add_feature("Extrude", {"sketch": sk.id, "distance": f"{10 + i * 5} mm"})
print("Bodies:", design.body_count())
app.goHome()
)";

const char *kAddInJs = R"(// Sample add-in (JavaScript): runs when Fission 360 starts and adds buttons to UTILITIES > ADD-INS
addCommand("sample.bodyCount", "Body Count", "Show how many bodies the design has", () => {
    fission.showMessage(doc.bodyCount() + " bodies in " + doc.name)
})
addCommand("sample.fitAll", "Fit and Home", "Home view, then fit everything", () => {
    app.goHome()
})
)";

const char *kAddInPy = R"(# Sample add-in (Python): adds a button that reports the design's volume
def report():
    total = sum(doc.body_properties(i)["volume"] for i in range(doc.body_count()))
    fission.show_message(f"Total volume: {doc.format_length(total, 2, 3)}")

add_command("sample.volume", "Total Volume", "Sum of every body's volume", report)
)";

QString languageOf(const QString &name) { return name.endsWith(".py", Qt::CaseInsensitive) ? "py" : "js"; }

}   // namespace

ScriptHost::ScriptHost(QJSEngine *engine, QObject *parent) : QObject(parent), m_engine(engine) {
    PythonHost::setCommandHook([this](const QString &id, const QString &label, const QString &tooltip) {
        registerCommand(id, label, tooltip, "py", QJSValue());
    });
}

QString ScriptHost::folder() const {
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/Scripts";
    if (!QDir(dir).exists()) {
        QDir().mkpath(dir);
        const QList<QPair<QString, const char *>> samples = {
            {"Sample - Box.js", kSampleBox}, {"Sample - Body report.js", kSampleReport},
            {"Sample - Sections.js", kSampleSection}, {"Sample - Cylinders.py", kSamplePython}};
        for (const auto &s : samples) {
            QFile f(dir + "/" + s.first);
            if (f.open(QIODevice::WriteOnly | QIODevice::Text)) f.write(s.second);
        }
    } else if (!QFileInfo::exists(dir + "/Sample - Cylinders.py")) {
        QFile f(dir + "/Sample - Cylinders.py");
        if (f.open(QIODevice::WriteOnly | QIODevice::Text)) f.write(kSamplePython);
    }
    return dir;
}

QString ScriptHost::addInFolder() const {
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/AddIns";
    if (!QDir(dir).exists()) {
        QDir().mkpath(dir);
        const QList<QPair<QString, const char *>> samples = {{"Sample - Commands.js", kAddInJs},
                                                              {"Sample - Volume.py", kAddInPy}};
        for (const auto &s : samples) {
            QFile f(dir + "/" + s.first);
            if (f.open(QIODevice::WriteOnly | QIODevice::Text)) f.write(s.second);
        }
    }
    return dir;
}

QStringList ScriptHost::scripts() const {
    return QDir(folder()).entryList({"*.js", "*.py"}, QDir::Files, QDir::Name);
}

QStringList ScriptHost::disabledAddIns() const {
    return QSettings().value("addIns/disabled").toStringList();
}

QVariantList ScriptHost::addIns() const {
    QVariantList out;
    const QStringList off = disabledAddIns();
    for (const QString &n : QDir(addInFolder()).entryList({"*.js", "*.py"}, QDir::Files, QDir::Name)) {
        const AddInState st = m_addIns.value(n);
        out << QVariantMap{{"name", n}, {"language", languageOf(n)}, {"enabled", !off.contains(n)},
                           {"loaded", st.loaded}, {"error", st.error}, {"output", st.output}};
    }
    return out;
}

QVariantList ScriptHost::commands() const {
    QVariantList out;
    for (const Command &c : m_commands)
        out << QVariantMap{{"id", c.id}, {"label", c.label}, {"tooltip", c.tooltip}, {"addIn", c.addIn}};
    return out;
}

QString ScriptHost::pythonStatus() const { return PythonHost::status(); }

QString ScriptHost::path(const QString &name, bool addIn) const {
    QString n = QFileInfo(name).fileName();   // no directories
    if (!n.endsWith(".js", Qt::CaseInsensitive) && !n.endsWith(".py", Qt::CaseInsensitive)) n += ".js";
    return (addIn ? addInFolder() : folder()) + "/" + n;
}

QString ScriptHost::load(const QString &name, bool addIn) const {
    QFile f(path(name, addIn));
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return {};
    return QString::fromUtf8(f.readAll());
}

bool ScriptHost::save(const QString &name, const QString &code, bool addIn) {
    if (QFileInfo(name).fileName().trimmed().isEmpty()) return false;
    QFile f(path(name, addIn));
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) return false;
    f.write(code.toUtf8());
    f.close();
    emit addIn ? addInsChanged() : scriptsChanged();
    return true;
}

bool ScriptHost::remove(const QString &name, bool addIn) {
    const QString p = path(name, addIn);
    const bool ok = QFile::remove(p);
    if (ok && addIn) {
        dropCommands(QFileInfo(p).fileName());
        m_addIns.remove(QFileInfo(p).fileName());
    }
    if (ok) emit addIn ? addInsChanged() : scriptsChanged();
    return ok;
}

QVariantMap ScriptHost::runFile(const QString &name, QObject *app) {
    return run(load(name), app, 30000, languageOf(name));
}

QVariantMap ScriptHost::run(const QString &code, QObject *app, int timeoutMs, const QString &language) {
    if (app) m_app = app;
    if (language == "py") return PythonHost::run(code, app, timeoutMs);
    return runJs(code, app, timeoutMs);
}

QVariantMap ScriptHost::guardedCall(const std::function<QJSValue()> &call, int timeoutMs) {
    QVariantMap out{{"ok", false}, {"output", QStringList()}, {"error", QString()}, {"line", 0}};
    m_log.clear();
    // Watchdog: interrupts runaway scripts (setInterrupted is thread safe)
    std::mutex mx;
    std::condition_variable cv;
    bool done = false, timedOut = false;
    QJSEngine *engine = m_engine;
    std::thread watchdog([&] {
        std::unique_lock<std::mutex> lock(mx);
        if (!cv.wait_for(lock, std::chrono::milliseconds(timeoutMs), [&] { return done; })) {
            timedOut = true;
            engine->setInterrupted(true);
        }
    });
    const QJSValue result = call();
    {
        std::lock_guard<std::mutex> lock(mx);
        done = true;
    }
    cv.notify_all();
    watchdog.join();
    m_engine->setInterrupted(false);

    out["output"] = m_log;
    m_log.clear();
    if (timedOut) {
        out["error"] = QString("Stopped after %1 s").arg(timeoutMs / 1000.0);
        return out;
    }
    if (result.isError()) {
        out["error"] = result.toString();
        const int l = result.property("lineNumber").toInt();
        out["line"] = l > 2 ? l - 2 : 0;
        return out;
    }
    out["ok"] = true;
    if (!result.isUndefined()) out["result"] = result.toString();
    return out;
}

QVariantMap ScriptHost::runJs(const QString &code, QObject *app, int timeoutMs) {
    QVariantMap out{{"ok", false}, {"output", QStringList()}, {"error", QString()}, {"line", 0}};
    if (!m_engine) { out["error"] = "No script engine"; return out; }
    FissionApplication *fa = FissionApplication::instance();
    for (QObject *o : {static_cast<QObject *>(fa), static_cast<QObject *>(fa->activeDocument()), app,
                       static_cast<QObject *>(this)})
        if (o) QJSEngine::setObjectOwnership(o, QJSEngine::CppOwnership);

    // The code starts on line 3 of the wrapper (two lines of prelude)
    const QString wrapped = "(function (app, doc, fission, __host) {\n"
                            "const print = function () { __host.log(Array.prototype.map.call(arguments, String).join(' ')) }; "
                            "const __refresh = function (d, a) { doc = d; if (a) app = a }; "
                            "const addCommand = function (id, label, tooltip, fn) { if (typeof tooltip === 'function') { fn = tooltip; tooltip = label } "
                            "__host.addCommand(String(id), String(label), String(tooltip || label), fn, __refresh) };\n"
                            + code + "\n})";
    QJSValue fn = m_engine->evaluate(wrapped, QStringLiteral("script.js"), 1);
    if (fn.isError()) {
        out["error"] = fn.toString();
        const int l = fn.property("lineNumber").toInt();
        out["line"] = l > 2 ? l - 2 : 0;
        return out;
    }
    return guardedCall([&] {
        return fn.call({m_engine->newQObject(app), m_engine->newQObject(fa->activeDocument()), m_engine->newQObject(fa),
                        m_engine->newQObject(this)});
    }, timeoutMs);
}

void ScriptHost::registerCommand(const QString &id, const QString &label, const QString &tooltip, const QString &language,
                                 const QJSValue &fn) {
    if (id.isEmpty()) return;
    for (int i = m_commands.size() - 1; i >= 0; --i)
        if (m_commands[i].id == id) m_commands.removeAt(i);
    m_commands << Command{id, label.isEmpty() ? id : label, tooltip, m_currentAddIn, language, fn};
    emit commandsChanged();
}

void ScriptHost::addCommand(const QString &id, const QString &label, const QString &tooltip, const QJSValue &fn,
                            const QJSValue &refresh) {
    if (!fn.isCallable()) {
        if (m_engine) m_engine->throwError(QStringLiteral("addCommand(id, label, tooltip, fn) needs a function to call"));
        return;
    }
    registerCommand(id, label, tooltip, "js", fn);
    m_commands.last().refresh = refresh;
}

void ScriptHost::dropCommands(const QString &addIn) {
    bool changed = false;
    for (int i = m_commands.size() - 1; i >= 0; --i)
        if (m_commands[i].addIn == addIn) {
            if (m_commands[i].language == "py") PythonHost::removeCommand(m_commands[i].id);
            m_commands.removeAt(i);
            changed = true;
        }
    if (changed) emit commandsChanged();
}

QVariantMap ScriptHost::runCommand(const QString &id) {
    for (const Command &c : m_commands) {
        if (c.id != id) continue;
        if (c.language == "py") return PythonHost::runCommand(id, 30000);
        QJSValue fn = c.fn, refresh = c.refresh;
        // The command sees the design and window of now, not of when the add-in loaded
        FissionApplication *fa = FissionApplication::instance();
        if (refresh.isCallable() && m_engine)
            refresh.call({m_engine->newQObject(fa->activeDocument()), m_app ? m_engine->newQObject(m_app) : QJSValue()});
        return guardedCall([&] { return fn.call(); }, 30000);
    }
    return {{"ok", false}, {"output", QStringList()}, {"error", "Unknown command " + id}, {"line", 0}};
}

QVariantMap ScriptHost::runAddIn(const QString &name, QObject *app) {
    const QString file = QFileInfo(path(name, true)).fileName();
    dropCommands(file);
    m_currentAddIn = file;
    const QVariantMap r = run(load(file, true), app, 30000, languageOf(file));
    m_currentAddIn.clear();
    AddInState &st = m_addIns[file];
    st.loaded = r.value("ok").toBool();
    st.error = st.loaded ? QString() : (r.value("line").toInt() ? QString("Line %1: ").arg(r.value("line").toInt()) : QString())
                                             + r.value("error").toString();
    st.output = r.value("output").toStringList();
    emit addInsChanged();
    return r;
}

void ScriptHost::startAddIns(QObject *app) {
    if (m_started) return;
    m_started = true;
    const QStringList off = disabledAddIns();
    for (const QString &n : QDir(addInFolder()).entryList({"*.js", "*.py"}, QDir::Files, QDir::Name))
        if (!off.contains(n)) runAddIn(n, app);
}

void ScriptHost::setAddInEnabled(const QString &name, bool enabled, QObject *app) {
    QStringList off = disabledAddIns();
    off.removeAll(name);
    if (!enabled) off << name;
    QSettings().setValue("addIns/disabled", off);
    if (enabled) {
        runAddIn(name, app);
    } else {
        dropCommands(name);
        m_addIns.remove(name);
        emit addInsChanged();
    }
}
