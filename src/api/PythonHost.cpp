// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#ifdef FISSION_HAS_PYTHON
// Python.h first, without Qt's `slots` macro and without pulling in the debug runtime import library
#pragma push_macro("slots")
#undef slots
#ifdef _DEBUG
#undef _DEBUG
#include <Python.h>
#define _DEBUG
#else
#include <Python.h>
#endif
#pragma pop_macro("slots")
#endif

#include "PythonHost.h"
#include "FissionApplication.h"

#include <QDir>
#include <QFileInfo>
#include <QJSValue>
#include <QMetaMethod>
#include <QPointer>
#include <QSettings>
#include <QUrl>
#include <QVector3D>
#include <condition_variable>
#include <mutex>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#endif

namespace {
std::function<void(const QString &, const QString &, const QString &)> s_hook;
}

void PythonHost::setCommandHook(std::function<void(const QString &, const QString &, const QString &)> hook) {
    s_hook = std::move(hook);
}

#ifndef FISSION_HAS_PYTHON

QString PythonHost::status() { return "Python support was not built in"; }
bool PythonHost::available(QString *why) {
    if (why) *why = status();
    return false;
}
QVariantMap PythonHost::run(const QString &, QObject *, int) {
    return {{"ok", false}, {"output", QStringList()}, {"error", status()}, {"line", 0}};
}
QVariantMap PythonHost::runCommand(const QString &id, int) { return run(id, nullptr, 0); }
void PythonHost::removeCommand(const QString &) {}

#else

namespace {

struct Install {
    QString home, version;
    int minor = 0;
};

// FISSION_PYTHON, else the newest 64-bit Python 3.11+ registered for the user or the machine
Install findPython() {
    Install best;
    const QString env = qEnvironmentVariable("FISSION_PYTHON");
    if (!env.isEmpty() && QFileInfo::exists(env + "/python3.dll")) {
        best.home = QDir::cleanPath(env);
        for (int m = 30; m >= 11; --m)
            if (QFileInfo::exists(QString("%1/python3%2.dll").arg(best.home).arg(m))) { best.minor = m; break; }
        best.version = QString("3.%1").arg(best.minor);
        return best;
    }
    for (const char *root : {"HKEY_CURRENT_USER\\Software\\Python\\PythonCore", "HKEY_LOCAL_MACHINE\\Software\\Python\\PythonCore"}) {
        QSettings reg(root, QSettings::NativeFormat);
        for (const QString &tag : reg.childGroups()) {
            if (!tag.startsWith("3.") || tag.contains('-')) continue;   // 32-bit and ARM builds have suffixes
            const int minor = tag.mid(2).toInt();
            if (minor < 11 || minor <= best.minor) continue;
            const QString home = QDir::cleanPath(reg.value(tag + "/InstallPath/.").toString());
            if (home.isEmpty() || !QFileInfo::exists(home + "/python3.dll")) continue;
            best = {home, tag, minor};
        }
    }
    return best;
}

const Install &install() {
    static const Install i = findPython();
    return i;
}

bool s_ready = false, s_failed = false;
QString s_error;
PyObject *s_module = nullptr;       // fission360
// Data exports of python3.dll (a delay-loaded DLL can only be called, so these are looked up)
PyObject *s_none = nullptr, *s_true = nullptr, *s_false = nullptr, *s_floatType = nullptr;
PyObject *s_excRuntimeError = nullptr, *s_excAttributeError = nullptr, *s_excTypeError = nullptr;
PyObject *newNone() {
    Py_INCREF(s_none);
    return s_none;
}
PyObject *s_objType = nullptr, *s_methodType = nullptr;

// --- Qt objects in Python -----------------------------------------------------

struct QtObject {
    PyObject_HEAD
    QPointer<QObject> *obj;
};

struct QtMethod {
    PyObject_HEAD
    QPointer<QObject> *obj;
    QByteArray *name;
};

PyObject *toPython(const QVariant &v);
QVariant fromPython(PyObject *o);

PyObject *wrap(QObject *o) {
    if (!o) return newNone();
    auto alloc = reinterpret_cast<allocfunc>(PyType_GetSlot(reinterpret_cast<PyTypeObject *>(s_objType), Py_tp_alloc));
    auto *w = reinterpret_cast<QtObject *>(alloc(reinterpret_cast<PyTypeObject *>(s_objType), 0));
    if (!w) return nullptr;
    w->obj = new QPointer<QObject>(o);
    return reinterpret_cast<PyObject *>(w);
}

QObject *unwrap(PyObject *o) {
    if (Py_TYPE(o) != reinterpret_cast<PyTypeObject *>(s_objType)) return nullptr;
    return reinterpret_cast<QtObject *>(o)->obj->data();
}

QString pyString(PyObject *o) {
    Py_ssize_t n = 0;
    const char *s = PyUnicode_AsUTF8AndSize(o, &n);
    return s ? QString::fromUtf8(s, n) : QString();
}

PyObject *toPython(const QVariant &in) {
    QVariant v = in;
    if (v.metaType() == QMetaType::fromType<QJSValue>()) v = v.value<QJSValue>().toVariant();
    if (!v.isValid() || v.isNull() && v.metaType().id() != QMetaType::QString) return newNone();
    const int t = v.metaType().id();
    switch (t) {
    case QMetaType::Bool: return PyBool_FromLong(v.toBool());
    case QMetaType::Int: case QMetaType::UInt: case QMetaType::LongLong: case QMetaType::ULongLong:
    case QMetaType::Long: case QMetaType::Short: case QMetaType::UShort:
        return PyLong_FromLongLong(v.toLongLong());
    case QMetaType::Double: case QMetaType::Float: return PyFloat_FromDouble(v.toDouble());
    case QMetaType::QString: return PyUnicode_FromString(v.toString().toUtf8().constData());
    case QMetaType::QUrl: return PyUnicode_FromString(v.toUrl().toString().toUtf8().constData());
    case QMetaType::QVector3D: {
        const QVector3D p = v.value<QVector3D>();
        return Py_BuildValue("[ddd]", double(p.x()), double(p.y()), double(p.z()));
    }
    case QMetaType::QVariantMap: {
        PyObject *d = PyDict_New();
        const QVariantMap m = v.toMap();
        for (auto it = m.begin(); it != m.end(); ++it) {
            PyObject *val = toPython(it.value());
            if (!val) { Py_DECREF(d); return nullptr; }
            PyDict_SetItemString(d, it.key().toUtf8().constData(), val);
            Py_DECREF(val);
        }
        return d;
    }
    default: break;
    }
    if (v.metaType().flags() & QMetaType::PointerToQObject) return wrap(v.value<QObject *>());
    if (t != QMetaType::QString && t != QMetaType::QByteArray && v.canConvert<QVariantList>()) {
        const QVariantList l = v.value<QVariantList>();
        PyObject *list = PyList_New(l.size());
        for (int i = 0; i < l.size(); ++i) {
            PyObject *item = toPython(l[i]);
            if (!item) { Py_DECREF(list); return nullptr; }
            PyList_SetItem(list, i, item);   // steals
        }
        return list;
    }
    if (v.canConvert<QVariantMap>()) return toPython(QVariant(v.value<QVariantMap>()));
    return PyUnicode_FromString(v.toString().toUtf8().constData());
}

QVariant fromPython(PyObject *o) {
    if (!o || o == s_none) return {};
    if ((o == s_true || o == s_false)) return QVariant(o == s_true);
    if (PyLong_Check(o)) {
        const long long n = PyLong_AsLongLong(o);
        if (PyErr_Occurred()) { PyErr_Clear(); return PyFloat_AsDouble(o); }
        if (n >= INT_MIN && n <= INT_MAX) return QVariant(int(n));
        return QVariant(n);
    }
    if (PyObject_IsInstance(o, s_floatType) == 1) return PyFloat_AsDouble(o);
    if (PyUnicode_Check(o)) return pyString(o);
    if (QObject *q = unwrap(o)) return QVariant::fromValue(q);
    if (PyList_Check(o) || PyTuple_Check(o)) {
        QVariantList l;
        const bool isList = PyList_Check(o);
        const Py_ssize_t n = isList ? PyList_Size(o) : PyTuple_Size(o);
        for (Py_ssize_t i = 0; i < n; ++i) l << fromPython(isList ? PyList_GetItem(o, i) : PyTuple_GetItem(o, i));
        return l;
    }
    if (PyDict_Check(o)) {
        QVariantMap m;
        PyObject *k, *val;
        Py_ssize_t pos = 0;
        while (PyDict_Next(o, &pos, &k, &val)) {
            PyObject *ks = PyObject_Str(k);
            m.insert(pyString(ks), fromPython(val));
            Py_XDECREF(ks);
        }
        return m;
    }
    PyObject *s = PyObject_Str(o);
    const QString str = s ? pyString(s) : QString();
    Py_XDECREF(s);
    return str;
}

QByteArray camel(const QByteArray &name) {
    QByteArray out;
    bool up = false;
    for (char c : name) {
        if (c == '_' && !out.isEmpty()) { up = true; continue; }
        out += up ? char(toupper(c)) : c;
        up = false;
    }
    return out;
}

QList<QMetaMethod> methodsNamed(const QObject *o, const QByteArray &name) {
    QList<QMetaMethod> out;
    const QMetaObject *mo = o->metaObject();
    for (int i = 0; i < mo->methodCount(); ++i) {
        const QMetaMethod m = mo->method(i);
        if (m.name() == name && m.access() == QMetaMethod::Public && m.methodType() != QMetaMethod::Signal) out << m;
    }
    return out;
}

void objDealloc(PyObject *self) {
    delete reinterpret_cast<QtObject *>(self)->obj;
    PyTypeObject *tp = Py_TYPE(self);
    reinterpret_cast<freefunc>(PyType_GetSlot(tp, Py_tp_free))(self);
    Py_DECREF(tp);
}

PyObject *objRepr(PyObject *self) {
    QObject *o = reinterpret_cast<QtObject *>(self)->obj->data();
    if (!o) return PyUnicode_FromString("<deleted Fission object>");
    const QString n = o->property("name").toString();
    return PyUnicode_FromString(QString("<%1%2>").arg(o->metaObject()->className(), n.isEmpty() ? "" : " '" + n + "'")
                                    .toUtf8().constData());
}

PyObject *makeMethod(QObject *o, const QByteArray &name) {
    auto alloc = reinterpret_cast<allocfunc>(PyType_GetSlot(reinterpret_cast<PyTypeObject *>(s_methodType), Py_tp_alloc));
    auto *m = reinterpret_cast<QtMethod *>(alloc(reinterpret_cast<PyTypeObject *>(s_methodType), 0));
    if (!m) return nullptr;
    m->obj = new QPointer<QObject>(o);
    m->name = new QByteArray(name);
    return reinterpret_cast<PyObject *>(m);
}

PyObject *objGetAttr(PyObject *self, PyObject *nameObj) {
    QObject *o = reinterpret_cast<QtObject *>(self)->obj->data();
    const QByteArray raw = pyString(nameObj).toUtf8();
    if (raw.startsWith("__")) return PyObject_GenericGetAttr(self, nameObj);
    if (!o) { PyErr_SetString(s_excRuntimeError, "The Fission object has been deleted"); return nullptr; }
    for (const QByteArray &name : {raw, camel(raw)}) {
        if (o->metaObject()->indexOfProperty(name) >= 0 || o->dynamicPropertyNames().contains(name))
            return toPython(o->property(name));
        if (!methodsNamed(o, name).isEmpty()) return makeMethod(o, name);
    }
    PyErr_Format(s_excAttributeError, "'%s' has no attribute '%s'", o->metaObject()->className(), raw.constData());
    return nullptr;
}

int objSetAttr(PyObject *self, PyObject *nameObj, PyObject *value) {
    QObject *o = reinterpret_cast<QtObject *>(self)->obj->data();
    if (!o) { PyErr_SetString(s_excRuntimeError, "The Fission object has been deleted"); return -1; }
    if (!value) { PyErr_SetString(s_excAttributeError, "Fission properties cannot be deleted"); return -1; }
    const QByteArray raw = pyString(nameObj).toUtf8();
    for (const QByteArray &name : {raw, camel(raw)}) {
        const int pi = o->metaObject()->indexOfProperty(name);
        if (pi < 0) continue;
        const QMetaProperty p = o->metaObject()->property(pi);
        QVariant v = fromPython(value);
        if (p.metaType().id() != QMetaType::QVariant && !v.convert(p.metaType())) {
            PyErr_Format(s_excTypeError, "Cannot set '%s' from that value", name.constData());
            return -1;
        }
        if (!p.write(o, v)) { PyErr_Format(s_excAttributeError, "'%s' is read-only", name.constData()); return -1; }
        return 0;
    }
    PyErr_Format(s_excAttributeError, "'%s' has no property '%s'", o->metaObject()->className(), raw.constData());
    return -1;
}

void methodDealloc(PyObject *self) {
    auto *m = reinterpret_cast<QtMethod *>(self);
    delete m->obj;
    delete m->name;
    PyTypeObject *tp = Py_TYPE(self);
    reinterpret_cast<freefunc>(PyType_GetSlot(tp, Py_tp_free))(self);
    Py_DECREF(tp);
}

PyObject *methodCall(PyObject *self, PyObject *args, PyObject *kwargs) {
    auto *mo = reinterpret_cast<QtMethod *>(self);
    QObject *o = mo->obj->data();
    if (!o) { PyErr_SetString(s_excRuntimeError, "The Fission object has been deleted"); return nullptr; }
    if (kwargs && PyDict_Size(kwargs) > 0) { PyErr_SetString(s_excTypeError, "Fission methods take positional arguments only"); return nullptr; }
    QVariantList in;
    for (Py_ssize_t i = 0; i < PyTuple_Size(args); ++i) in << fromPython(PyTuple_GetItem(args, i));
    if (PyErr_Occurred()) return nullptr;
    // The first overload with that many arguments that the values convert to
    for (const QMetaMethod &m : methodsNamed(o, *mo->name)) {
        if (m.parameterCount() != in.size()) continue;
        QVariantList conv = in;
        bool ok = true;
        for (int i = 0; i < conv.size() && ok; ++i) {
            const QMetaType pt = m.parameterMetaType(i);
            if (pt.id() == QMetaType::QVariant) continue;
            if (pt == QMetaType::fromType<QJSValue>()) { ok = false; break; }
            if (conv[i].metaType() != pt) ok = conv[i].convert(pt);
        }
        if (!ok) continue;
        void *argv[11] = {};
        if (conv.size() > 10) break;
        const QMetaType rt = m.returnMetaType();
        void *ret = rt.isValid() && rt.id() != QMetaType::Void ? rt.create() : nullptr;
        argv[0] = ret;
        for (int i = 0; i < conv.size(); ++i)
            argv[i + 1] = m.parameterMetaType(i).id() == QMetaType::QVariant ? static_cast<void *>(&conv[i]) : conv[i].data();
        QMetaObject::metacall(o, QMetaObject::InvokeMetaMethod, m.methodIndex(), argv);
        if (PyErr_Occurred()) { if (ret) rt.destroy(ret); return nullptr; }
        if (!ret) return newNone();
        const QVariant r = rt.id() == QMetaType::QVariant ? *static_cast<QVariant *>(ret) : QVariant(rt, ret);
        rt.destroy(ret);
        return toPython(r);
    }
    PyErr_Format(s_excTypeError, "No form of %s() takes those %d argument(s)", mo->name->constData(), int(in.size()));
    return nullptr;
}

PyObject *registerCommand(PyObject *, PyObject *args) {
    const char *id = nullptr, *label = nullptr, *tip = nullptr;
    if (!PyArg_ParseTuple(args, "sss", &id, &label, &tip)) return nullptr;
    if (s_hook) s_hook(QString::fromUtf8(id), QString::fromUtf8(label), QString::fromUtf8(tip));
    return newNone();
}

PyMethodDef s_methods[] = {
    {"_register", registerCommand, METH_VARARGS, nullptr},
    {nullptr, nullptr, 0, nullptr}};

PyModuleDef s_moduleDef = {PyModuleDef_HEAD_INIT, "fission360", "Fission 360 scripting", -1, s_methods,
                           nullptr, nullptr, nullptr, nullptr};

const char *kBootstrap = R"PY(
import sys, io, signal, traceback, types
try:
    signal.signal(signal.SIGINT, signal.default_int_handler)
except Exception:
    pass
if not getattr(sys, "argv", None):
    sys.argv = [""]

def _capture(fn):
    out = io.StringIO()
    old = sys.stdout, sys.stderr
    sys.stdout = sys.stderr = out
    try:
        r = fn()
        return (True, out.getvalue(), "", 0, None if r is None else str(r))
    except KeyboardInterrupt:
        return (False, out.getvalue(), "KeyboardInterrupt", 0, None)
    except BaseException as e:
        line = 0
        if isinstance(e, SyntaxError) and e.filename == "<script>":
            line = e.lineno or 0
        for fr in traceback.extract_tb(e.__traceback__):
            if fr.filename == "<script>":
                line = fr.lineno
        msg = traceback.format_exception_only(type(e), e)[-1].strip()
        return (False, out.getvalue(), msg, line, None)
    finally:
        sys.stdout, sys.stderr = old

def _run(code, g):
    def go():
        exec(compile(code, "<script>", "exec"), g)
    return _capture(go)

_commands = {}
def add_command(id, label, tooltip=None, fn=None):
    if fn is None and callable(tooltip):
        fn, tooltip = tooltip, None
    if not callable(fn):
        raise TypeError("add_command(id, label, tooltip, fn) needs a function to call")
    _commands[str(id)] = fn
    _register(str(id), str(label), str(tooltip or label))
addCommand = add_command

def _run_command(id):
    fn = _commands.get(id)
    if fn is None:
        return (False, "", "Unknown command " + id, 0, None)
    # The command sees the design and window of now, not of when the add-in loaded
    g = getattr(fn, "__globals__", None)
    if isinstance(g, dict) and g is not globals():
        g["doc"] = _mod.doc
        if _mod.app is not None:
            g["app"] = _mod.app
    return _capture(fn)

def _remove_command(id):
    _commands.pop(id, None)

# adsk-style entry points over the same objects
_mod = sys.modules[__name__]
class _UserInterface:
    def messageBox(self, text, title=None, *rest):
        _mod.application.showMessage(str(text))
class Application:
    _ui = _UserInterface()
    @staticmethod
    def get():
        return Application()
    @property
    def activeProduct(self):
        return _mod.doc
    activeDocument = activeProduct
    @property
    def userInterface(self):
        return self._ui
    @property
    def version(self):
        return _mod.application.version
class Design:
    @staticmethod
    def cast(o):
        return o
adsk = types.ModuleType("adsk")
adsk.core = types.ModuleType("adsk.core")
adsk.fusion = types.ModuleType("adsk.fusion")
adsk.core.Application = Application
adsk.fusion.Design = Design
sys.modules.update({"adsk": adsk, "adsk.core": adsk.core, "adsk.fusion": adsk.fusion})
)PY";

bool start(QString &err) {
    if (s_ready) return true;
    if (s_failed) { err = s_error; return false; }
    s_failed = true;
    const Install &py = install();
    if (py.home.isEmpty()) {
        s_error = "Python 3.11 or newer (64-bit) was not found. Install it from python.org or set FISSION_PYTHON to its folder.";
        err = s_error;
        return false;
    }
    if (!QFileInfo::exists(py.home + "/Lib/encodings")) {
        s_error = "The Python standard library is missing in " + py.home;
        err = s_error;
        return false;
    }
    const std::wstring home = QDir::toNativeSeparators(py.home).toStdWString();
    SetDllDirectoryW(home.c_str());
    if (py.minor) LoadLibraryExW((home + L"\\python3" + std::to_wstring(py.minor) + L".dll").c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (!LoadLibraryExW((home + L"\\python3.dll").c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH)) {
        s_error = "Could not load python3.dll from " + py.home;
        err = s_error;
        return false;
    }
    qputenv("PYTHONHOME", py.home.toLocal8Bit());
    qputenv("PYTHONIOENCODING", "utf-8");
    Py_InitializeEx(0);
    if (!Py_IsInitialized()) {
        s_error = "Python did not start";
        err = s_error;
        return false;
    }
    {
        HMODULE dll = GetModuleHandleW(L"python3.dll");
        auto data = [&](const char *name) { return dll ? reinterpret_cast<PyObject *>(GetProcAddress(dll, name)) : nullptr; };
        auto exc = [&](const char *name) {
            auto **p = dll ? reinterpret_cast<PyObject **>(GetProcAddress(dll, name)) : nullptr;
            return p ? *p : nullptr;
        };
        s_none = data("_Py_NoneStruct");
        s_true = data("_Py_TrueStruct");
        s_false = data("_Py_FalseStruct");
        s_floatType = data("PyFloat_Type");
        s_excRuntimeError = exc("PyExc_RuntimeError");
        s_excAttributeError = exc("PyExc_AttributeError");
        s_excTypeError = exc("PyExc_TypeError");
        if (!s_none || !s_true || !s_false || !s_floatType || !s_excRuntimeError || !s_excAttributeError || !s_excTypeError) {
            s_error = "python3.dll is missing expected exports";
            err = s_error;
            return false;
        }
    }
    static PyType_Slot objSlots[] = {
        {Py_tp_dealloc, reinterpret_cast<void *>(objDealloc)},
        {Py_tp_getattro, reinterpret_cast<void *>(objGetAttr)},
        {Py_tp_setattro, reinterpret_cast<void *>(objSetAttr)},
        {Py_tp_repr, reinterpret_cast<void *>(objRepr)},
        {0, nullptr}};
    static PyType_Spec objSpec = {"fission360.Object", int(sizeof(QtObject)), 0, Py_TPFLAGS_DEFAULT, objSlots};
    static PyType_Slot methodSlots[] = {
        {Py_tp_dealloc, reinterpret_cast<void *>(methodDealloc)},
        {Py_tp_call, reinterpret_cast<void *>(methodCall)},
        {0, nullptr}};
    static PyType_Spec methodSpec = {"fission360.Method", int(sizeof(QtMethod)), 0, Py_TPFLAGS_DEFAULT, methodSlots};
    s_objType = PyType_FromSpec(&objSpec);
    s_methodType = PyType_FromSpec(&methodSpec);
    s_module = PyModule_Create(&s_moduleDef);
    if (!s_objType || !s_methodType || !s_module) {
        PyErr_Clear();
        s_error = "Could not set up the fission360 module";
        err = s_error;
        return false;
    }
    PyDict_SetItemString(PyImport_GetModuleDict(), "fission360", s_module);
    PyObject *dict = PyModule_GetDict(s_module);   // borrowed
    PyDict_SetItemString(dict, "__builtins__", PyEval_GetBuiltins());
    PyObject *builtins = PyImport_ImportModule("builtins");
    PyObject *exec = builtins ? PyObject_GetAttrString(builtins, "exec") : nullptr;
    PyObject *r = exec ? PyObject_CallFunction(exec, "sO", kBootstrap, dict) : nullptr;
    Py_XDECREF(builtins);
    Py_XDECREF(exec);
    if (!r) {
        PyErr_Clear();
        s_error = "The Python bootstrap failed";
        err = s_error;
        return false;
    }
    Py_DECREF(r);
    s_failed = false;
    s_ready = true;
    return true;
}

// Runs fn under a watchdog; fn returns the (ok, output, error, line, result) tuple
QVariantMap guarded(const std::function<PyObject *()> &fn, int timeoutMs) {
    QVariantMap out{{"ok", false}, {"output", QStringList()}, {"error", QString()}, {"line", 0}};
    std::mutex mx;
    std::condition_variable cv;
    bool done = false, timedOut = false;
    std::thread watchdog([&] {
        std::unique_lock<std::mutex> lock(mx);
        if (timeoutMs > 0 && !cv.wait_for(lock, std::chrono::milliseconds(timeoutMs), [&] { return done; })) {
            timedOut = true;
            PyErr_SetInterrupt();
        } else if (timeoutMs <= 0) {
            cv.wait(lock, [&] { return done; });
        }
    });
    PyObject *t = fn();
    {
        std::lock_guard<std::mutex> lock(mx);
        done = true;
    }
    cv.notify_all();
    watchdog.join();
    if (timedOut) {
        // A late interrupt must not hit the next script
        if (PyErr_CheckSignals() < 0) PyErr_Clear();
    }
    if (!t || !PyTuple_Check(t) || PyTuple_Size(t) != 5) {
        PyErr_Clear();
        Py_XDECREF(t);
        out["error"] = "The Python script could not be run";
        return out;
    }
    QStringList lines = pyString(PyTuple_GetItem(t, 1)).split('\n');
    while (!lines.isEmpty() && lines.last().isEmpty()) lines.removeLast();
    out["output"] = lines;
    out["ok"] = PyTuple_GetItem(t, 0) == s_true;
    out["error"] = timedOut ? QString("Stopped after %1 s").arg(timeoutMs / 1000.0) : pyString(PyTuple_GetItem(t, 2));
    out["line"] = int(PyLong_AsLong(PyTuple_GetItem(t, 3)));
    PyObject *res = PyTuple_GetItem(t, 4);
    if (res != s_none) out["result"] = pyString(res);
    if (timedOut) out["ok"] = false;
    Py_DECREF(t);
    return out;
}

QPointer<QObject> s_app;   // the main window of the last run (add-in commands run against it too)

void setModuleObjects(QObject *app) {
    if (app) s_app = app;
    FissionApplication *fa = FissionApplication::instance();
    PyObject *dict = PyModule_GetDict(s_module);
    for (auto [name, obj] : {std::pair<const char *, QObject *>{"app", s_app.data()}, {"doc", fa->activeDocument()}, {"application", fa}}) {
        PyObject *w = wrap(obj);
        PyDict_SetItemString(dict, name, w);
        Py_XDECREF(w);
    }
}

}   // namespace

QString PythonHost::status() {
    const Install &py = install();
    if (s_failed && !s_error.isEmpty()) return s_error;
    if (py.home.isEmpty()) return "Python 3.11 or newer (64-bit) was not found";
    return QString("Python %1 (%2)").arg(py.version, QDir::toNativeSeparators(py.home));
}

bool PythonHost::available(QString *why) {
    QString err;
    const bool ok = start(err);
    if (why) *why = err;
    return ok;
}

QVariantMap PythonHost::run(const QString &code, QObject *app, int timeoutMs) {
    QString err;
    if (!start(err)) return {{"ok", false}, {"output", QStringList()}, {"error", err}, {"line", 0}};
    setModuleObjects(app);
    return guarded([&]() -> PyObject * {
        PyObject *mdict = PyModule_GetDict(s_module);
        PyObject *g = PyDict_New();
        PyDict_SetItemString(g, "__builtins__", PyEval_GetBuiltins());
        PyObject *main = PyUnicode_FromString("__main__");
        PyDict_SetItemString(g, "__name__", main);
        Py_XDECREF(main);
        for (const char *k : {"app", "doc", "add_command", "addCommand"})
            if (PyObject *v = PyDict_GetItemString(mdict, k)) PyDict_SetItemString(g, k, v);
        if (PyObject *v = PyDict_GetItemString(mdict, "application")) PyDict_SetItemString(g, "fission", v);
        PyObject *runFn = PyDict_GetItemString(mdict, "_run");
        PyObject *src = PyUnicode_FromString(code.toUtf8().constData());
        PyObject *t = runFn ? PyObject_CallFunctionObjArgs(runFn, src, g, nullptr) : nullptr;
        Py_XDECREF(src);
        Py_DECREF(g);
        return t;
    }, timeoutMs);
}

QVariantMap PythonHost::runCommand(const QString &id, int timeoutMs) {
    QString err;
    if (!start(err)) return {{"ok", false}, {"output", QStringList()}, {"error", err}, {"line", 0}};
    setModuleObjects(nullptr);
    return guarded([&]() -> PyObject * {
        PyObject *fn = PyDict_GetItemString(PyModule_GetDict(s_module), "_run_command");
        return fn ? PyObject_CallFunction(fn, "s", id.toUtf8().constData()) : nullptr;
    }, timeoutMs);
}

void PythonHost::removeCommand(const QString &id) {
    if (!s_ready) return;
    PyObject *fn = PyDict_GetItemString(PyModule_GetDict(s_module), "_remove_command");
    PyObject *r = fn ? PyObject_CallFunction(fn, "s", id.toUtf8().constData()) : nullptr;
    if (!r) PyErr_Clear();
    Py_XDECREF(r);
}

#endif
