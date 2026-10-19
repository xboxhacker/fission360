// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#include "FissionDocument.h"
#include "Expression.h"
#include "FileExport.h"
#include "FissionProgress.h"
#include "GeomUtils.h"
#include "MeshUtils.h"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSet>
#include <QTextStream>
#include <QHash>
#include <QUuid>
#include <QVector3D>
#include <QtEndian>
#include <array>
#include <cmath>
#include <functional>
#include <new>
#include <stdexcept>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include "FissionConstruction.h"
#include "FissionImport.h"
#include "FissionSketch.h"
#include "DocumentVersionStore.h"
#include "api/FissionApplication.h"
#include "render/PathTracer.h"
#include <QUuid>

#include <BRepAdaptor_Surface.hxx>
#include <BRepGProp.hxx>
#include <BRepMesh_IncrementalMesh.hxx>
#include <BRep_Tool.hxx>
#include <GProp_GProps.hxx>
#include <IGESControl_Controller.hxx>
#include <IGESControl_Writer.hxx>
#include <Poly_Triangulation.hxx>
#include <STEPControl_Writer.hxx>
#include <StlAPI_Writer.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopoDS.hxx>

FissionDocument::FissionDocument(QObject *parent)
    : QObject(parent), m_timeline(new FissionTimeline(this)), m_renderSettings(new RenderSettings(this)) {
    m_documentId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_rebuildTimer.setSingleShot(true);
    // Coalesce property edits / manip typing into one rebuild per frame-ish
    m_rebuildTimer.setInterval(16);
    connect(&m_rebuildTimer, &QTimer::timeout, this, &FissionDocument::rebuild);
    connect(m_timeline, &FissionTimeline::markerChanged, this, [this] {
        if (!m_restoring) scheduleRebuild();
    });
    connect(m_renderSettings, &RenderSettings::qualityChanged, this, &FissionDocument::renderSettingsChanged);
    connect(m_renderSettings, &RenderSettings::sceneChanged, this, &FissionDocument::renderSettingsChanged);
    connect(m_renderSettings, &RenderSettings::environmentChanged, this, &FissionDocument::renderSettingsChanged);
    connect(m_renderSettings, &RenderSettings::lightingChanged, this, &FissionDocument::renderSettingsChanged);
    connect(m_renderSettings, &RenderSettings::outputSizeChanged, this, &FissionDocument::renderSettingsChanged);
    connect(m_renderSettings, &RenderSettings::outputFormatChanged, this, &FissionDocument::renderSettingsChanged);
    connect(this, &FissionDocument::renderSettingsChanged, this, [this] {
        if (!m_restoring) setModified(true);
        emit displayChanged();
    });
}

FissionDocument::~FissionDocument() = default;

void FissionDocument::setName(const QString &name) {
    if (m_name != name) { m_name = name; emit nameChanged(); }
}

void FissionDocument::setDensity(double d) {
    if (d > 0 && m_density != d) { m_density = d; setModified(true); emit densityChanged(); }
}

void FissionDocument::applyAppearance(const QString &appearanceId, const QVariantList &bodyIndices) {
    if (appearanceId.isEmpty()) return;
    QList<int> idxs;
    if (bodyIndices.isEmpty()) {
        for (int i = 0; i < m_bodies.size(); ++i) idxs.append(i);
    } else {
        for (const QVariant &v : bodyIndices) {
            bool ok = false;
            const int i = v.toInt(&ok);
            if (ok && i >= 0 && i < m_bodies.size()) idxs.append(i);
        }
    }
    if (idxs.isEmpty()) return;
    checkpoint();
    for (int i : idxs) m_bodies[i]->applyAppearance(appearanceId);
}

void FissionDocument::applyAppearanceToSelection(const QString &appearanceId, const QVariantList &selectionItems) {
    if (appearanceId.isEmpty()) return;
    QList<QPair<int, int>> faces;   // body, faceIndex
    QSet<int> bodies;
    for (const QVariant &v : selectionItems) {
        const QVariantMap m = v.toMap();
        const QString kind = m.value(QStringLiteral("kind")).toString();
        const int body = m.value(QStringLiteral("body"), -1).toInt();
        if (body < 0 || body >= m_bodies.size()) continue;
        if (kind == QLatin1String("face")) {
            const int idx = m.value(QStringLiteral("index"), -1).toInt();
            if (idx >= 0) faces.append({body, idx});
        } else if (kind == QLatin1String("body")) {
            bodies.insert(body);
        }
    }
    if (faces.isEmpty() && bodies.isEmpty()) return;
    checkpoint();
    if (!faces.isEmpty()) {
        for (const auto &f : faces)
            m_bodies[f.first]->applyAppearanceToFace(f.second, appearanceId);
    } else {
        for (int i : bodies)
            m_bodies[i]->applyAppearance(appearanceId);
    }
}

QString FissionDocument::pathTraceToFile(const QString &path, const QVariantMap &camera) {
    if (path.isEmpty()) {
        m_lastError = QStringLiteral("No output path");
        return {};
    }
    PathTracer::Camera cam;
    auto vec = [](const QVariant &v, const QVector3D &fallback) {
        if (v.canConvert<QVector3D>()) return v.value<QVector3D>();
        const QVariantList L = v.toList();
        if (L.size() >= 3)
            return QVector3D(L[0].toFloat(), L[1].toFloat(), L[2].toFloat());
        return fallback;
    };
    cam.eye = vec(camera.value(QStringLiteral("eye")), QVector3D(200, -200, 150));
    cam.target = vec(camera.value(QStringLiteral("target")), QVector3D(0, 0, 0));
    cam.up = vec(camera.value(QStringLiteral("up")), QVector3D(0, 0, 1));
    cam.fovDeg = camera.value(QStringLiteral("fov"), 30.0).toDouble();
    cam.ortho = camera.value(QStringLiteral("ortho"), false).toBool();

    PathTracer::Settings s;
    if (m_renderSettings) {
        s.width = m_renderSettings->outputWidth();
        s.height = m_renderSettings->outputHeight();
        s.samplesPerPixel = PathTracer::samplesForQuality(m_renderSettings->renderQuality());
        s.environmentPath = m_renderSettings->environmentPath();
        s.environmentIntensity = m_renderSettings->environmentIntensity();
        s.environmentRotation = m_renderSettings->environmentRotation() * M_PI / 180.0;
        s.background = m_renderSettings->backgroundColor();
        s.useSun = m_renderSettings->useSunLight();
        s.sunIntensity = float(m_renderSettings->lightIntensity());
        s.sunColor = QVector3D(float(m_renderSettings->lightColor().redF()),
                               float(m_renderSettings->lightColor().greenF()),
                               float(m_renderSettings->lightColor().blueF()));
        const double yaw = m_renderSettings->lightAngle() * M_PI / 180.0;
        const double alt = m_renderSettings->lightAltitude() * M_PI / 180.0;
        s.sunDir = QVector3D(float(std::cos(alt) * std::cos(yaw)),
                             float(std::cos(alt) * std::sin(yaw)),
                             float(std::sin(alt))).normalized();
    }
    // Allow camera map to override resolution / spp for tests
    if (camera.contains(QStringLiteral("width"))) s.width = camera["width"].toInt();
    if (camera.contains(QStringLiteral("height"))) s.height = camera["height"].toInt();
    if (camera.contains(QStringLiteral("samples"))) s.samplesPerPixel = camera["samples"].toInt();

    if (auto *app = FissionApplication::instance())
        app->beginTask(QStringLiteral("path-trace"), QStringLiteral("Path tracing…"));
    QImage img = PathTracer::render(this, cam, s, [](double p) {
        if (auto *app = FissionApplication::instance())
            app->reportProgress(QStringLiteral("path-trace"), p, QStringLiteral("Path tracing…"));
    });
    if (auto *app = FissionApplication::instance())
        app->endTask(QStringLiteral("path-trace"));
    if (img.isNull() || !img.save(path)) {
        m_lastError = QStringLiteral("Could not write path-traced image");
        return {};
    }
    m_lastError.clear();
    return path;
}

namespace {
const QList<QPair<QString, double>> &lengthUnitTable() {   // mm per unit
    static const QList<QPair<QString, double>> t = {{"mm", 1.0}, {"cm", 10.0}, {"m", 1000.0}, {"in", 25.4}, {"ft", 304.8}};
    return t;
}
const QList<QPair<QString, double>> &massUnitTable() {     // g per unit
    static const QList<QPair<QString, double>> t = {{"g", 1.0}, {"kg", 1000.0}, {"lb", 453.59237}, {"oz", 28.349523125}};
    return t;
}
double unitFactor(const QList<QPair<QString, double>> &table, const QString &unit) {
    for (const auto &u : table)
        if (u.first == unit) return u.second;
    return 1.0;
}
bool hasUnit(const QList<QPair<QString, double>> &table, const QString &unit) {
    for (const auto &u : table)
        if (u.first == unit) return true;
    return false;
}
QString trimmedNumber(double v, int decimals) {
    QString s = QString::number(v, 'f', decimals);
    if (s.contains('.')) {
        while (s.endsWith('0')) s.chop(1);
        if (s.endsWith('.')) s.chop(1);
    }
    return s == "-0" ? QStringLiteral("0") : s;
}
}

void FissionDocument::setLengthUnit(const QString &unit) {
    if (!hasUnit(lengthUnitTable(), unit) || unit == m_lengthUnit) return;
    m_lengthUnit = unit;
    setModified(true);
    emit unitsChanged();
}

void FissionDocument::setMassUnit(const QString &unit) {
    if (!hasUnit(massUnitTable(), unit) || unit == m_massUnit) return;
    m_massUnit = unit;
    setModified(true);
    emit unitsChanged();
}

QStringList FissionDocument::lengthUnits() const {
    QStringList out;
    for (const auto &u : lengthUnitTable()) out << u.first;
    return out;
}

QStringList FissionDocument::massUnits() const {
    QStringList out;
    for (const auto &u : massUnitTable()) out << u.first;
    return out;
}

double FissionDocument::lengthScale() const { return unitFactor(lengthUnitTable(), m_lengthUnit); }

QString FissionDocument::formatLength(double mm, int decimals, int power) const {
    const double v = mm / std::pow(lengthScale(), power);
    const QString suffix = power == 2 ? QStringLiteral("\u00b2") : power == 3 ? QStringLiteral("\u00b3") : QString();
    return trimmedNumber(v, decimals) + " " + m_lengthUnit + suffix;
}

QString FissionDocument::formatMass(double grams, int decimals) const {
    return trimmedNumber(grams / unitFactor(massUnitTable(), m_massUnit), decimals) + " " + m_massUnit;
}

QString FissionDocument::withUnit(const QString &text, const QString &kind) const {
    const QString t = text.trimmed();
    if (t.isEmpty() || t.contains(QRegularExpression("[A-Za-z_]"))) return text;
    const QString unit = kind == "length" ? m_lengthUnit : kind == "angle" ? QStringLiteral("deg") : QString();
    if (unit.isEmpty()) return text;
    static const QRegularExpression plain("^[-+]?(\\d+\\.?\\d*|\\.\\d+)$");
    return (plain.match(t).hasMatch() ? t : "(" + t + ")") + " " + unit;
}

void FissionDocument::setInfo(const QString &key, const QString &value) {
    if (m_info.value(key).toString() == value) return;
    if (value.isEmpty()) m_info.remove(key); else m_info.insert(key, value);
    setModified(true);
    emit infoChanged();
}

void FissionDocument::setModified(bool m) {
    // Any transition to dirty (or further edits while dirty) advances the content stamp so
    // linked drawings / renders know they are out of date.
    if (m && !m_restoring) bumpContentRevision();
    if (m_modified == m) return;
    m_modified = m;
    emit modifiedChanged();
}

void FissionDocument::bumpContentRevision() {
    ++m_contentRevision;
    markLinkedAssetsStale();
    emit contentRevisionChanged();
}

QVariantList FissionDocument::linkedAssets() const {
    QVariantList out = m_linkedAssets;
    for (QVariant &v : out) {
        QVariantMap m = v.toMap();
        m["outOfDate"] = m.value(QStringLiteral("sourceRevision")).toLongLong() != m_contentRevision;
        v = m;
    }
    return out;
}

void FissionDocument::linkAsset(const QString &kind, const QString &id, const QString &name, const QString &path) {
    if (kind.isEmpty() || id.isEmpty()) return;
    for (int i = 0; i < m_linkedAssets.size(); ++i) {
        QVariantMap m = m_linkedAssets[i].toMap();
        if (m.value(QStringLiteral("kind")) == kind && m.value(QStringLiteral("id")) == id) {
            m["name"] = name;
            if (!path.isEmpty()) m["path"] = path;
            m["sourceRevision"] = m_contentRevision;
            m["outOfDate"] = false;
            m_linkedAssets[i] = m;
            emit linkedAssetsChanged();
            return;
        }
    }
    m_linkedAssets.append(QVariantMap{
        {"kind", kind},
        {"id", id},
        {"name", name},
        {"path", path},
        {"sourceRevision", m_contentRevision},
        {"outOfDate", false},
    });
    emit linkedAssetsChanged();
}

void FissionDocument::unlinkAsset(const QString &kind, const QString &id) {
    for (int i = 0; i < m_linkedAssets.size(); ++i) {
        const QVariantMap m = m_linkedAssets[i].toMap();
        if (m.value(QStringLiteral("kind")) == kind && m.value(QStringLiteral("id")) == id) {
            m_linkedAssets.removeAt(i);
            emit linkedAssetsChanged();
            return;
        }
    }
}

void FissionDocument::markLinkedAssetsStale() {
    bool changed = false;
    for (int i = 0; i < m_linkedAssets.size(); ++i) {
        QVariantMap m = m_linkedAssets[i].toMap();
        const bool stale = m.value(QStringLiteral("sourceRevision")).toLongLong() != m_contentRevision;
        if (m.value(QStringLiteral("outOfDate")).toBool() != stale) {
            m["outOfDate"] = stale;
            m_linkedAssets[i] = m;
            changed = true;
        }
    }
    if (changed) emit linkedAssetsChanged();
}

bool FissionDocument::anyLinkedAssetOutOfDate() const {
    for (const QVariant &v : linkedAssets())
        if (v.toMap().value(QStringLiteral("outOfDate")).toBool()) return true;
    return false;
}

QByteArray FissionDocument::snapshotJsonForHistory() const {
    // Versions skip the embedded thumbnail to stay light on disk
    return QJsonDocument(toJson()).toJson(QJsonDocument::Compact);
}

QVariantList FissionDocument::versionHistory() const {
    if (m_filePath.isEmpty()) return {};
    return DocumentVersionStore::instance()->listVersions(m_filePath);
}

bool FissionDocument::revertToVersion(int versionNumber) {
    if (m_filePath.isEmpty() || versionNumber < 1) {
        m_lastError = QStringLiteral("No saved version to revert to");
        return false;
    }
    const QByteArray raw = DocumentVersionStore::instance()->loadVersion(m_filePath, versionNumber);
    if (raw.isEmpty()) {
        m_lastError = QStringLiteral("Could not load version %1").arg(versionNumber);
        return false;
    }
    const QJsonObject obj = QJsonDocument::fromJson(raw).object();
    if (obj.isEmpty()) {
        m_lastError = QStringLiteral("Version %1 is not valid JSON").arg(versionNumber);
        return false;
    }
    checkpoint();
    const qint64 prevRev = m_contentRevision;
    m_restoring = true;
    fromJson(obj);
    m_restoring = false;
    // Snapshot may carry an older contentRevision — advance past the pre-revert stamp so links go stale
    m_contentRevision = prevRev;
    bumpContentRevision();
    m_modified = true;
    emit modifiedChanged();
    return true;
}

bool FissionDocument::writeAutosaveNow() {
    return DocumentVersionStore::instance()->writeAutosave(
        m_documentId, snapshotJsonForHistory(), m_name, m_filePath);
}

QVariantList FissionDocument::pendingAutosaves() {
    return DocumentVersionStore::instance()->pendingAutosaves();
}

bool FissionDocument::loadAutosave(const QString &documentId) {
    const QByteArray raw = DocumentVersionStore::instance()->loadAutosave(documentId);
    if (raw.isEmpty()) {
        m_lastError = QStringLiteral("No autosave for that document");
        return false;
    }
    const QJsonObject obj = QJsonDocument::fromJson(raw).object();
    if (obj.isEmpty()) {
        m_lastError = QStringLiteral("Autosave is corrupt");
        return false;
    }
    m_restoring = true;
    fromJson(obj);
    if (!documentId.isEmpty()) m_documentId = documentId;
    m_restoring = false;
    setModified(true);
    return true;
}

// --- Features --------------------------------------------------------------

QString FissionDocument::uniqueFeatureName(const QString &type) const {
    for (int n = 1;; ++n) {
        const QString candidate = type + QString::number(n);
        bool taken = false;
        for (auto *f : m_timeline->features()) taken = taken || f->name() == candidate;
        if (!taken) return candidate;
    }
}

void FissionDocument::attachFeature(FissionFeature *f) {
    f->setParent(this);
    connect(f, &FissionFeature::changed, this, [this] {
        if (m_restoring) return;
        setModified(true);
        scheduleRebuild();
    });
    connect(f, &FissionFeature::statusChanged, this, [this, f] { m_timeline->refresh(m_timeline->indexOf(f)); });
    connect(f, &FissionFeature::visibilityChanged, this, [this] {
        setModified(true);
        emit displayChanged();
    });
}

FissionFeature *FissionDocument::addFeature(const QString &type, const QVariantMap &props, const QString &name) {
    FissionFeature *f = FissionFeature::create(type, this);
    if (!f) {
        m_lastError = QString("Unknown feature type '%1'").arg(type);
        return nullptr;
    }
    if (QStringList{"BaseFlange", "EdgeFlange", "FlatPattern", "Hem", "Tab", "Bend", "ContourFlange", "Unfold"}.contains(type)) ensureSheetMetalRule();
    checkpoint();
    QVariantMap merged = f->props();
    for (auto it = props.begin(); it != props.end(); ++it) merged.insert(it.key(), it.value());
    f->m_props = merged;
    const bool asBuilt = type == "Joint" && merged.value("asBuilt").toBool();
    f->m_name = name.isEmpty() ? uniqueFeatureName(asBuilt ? QStringLiteral("As-built Joint") : type) : name;
    attachFeature(f);
    m_timeline->insert(m_timeline->marker(), f);
    setModified(true);
    rebuild();
    emit featureAdded(f);
    return f;
}

void FissionDocument::removeFeature(FissionFeature *feature) {
    const int idx = m_timeline->indexOf(feature);
    if (idx < 0) return;
    checkpoint();
    m_timeline->take(idx);
    feature->disconnect(this);
    const QString id = feature->id();
    feature->deleteLater();
    setModified(true);
    rebuild();
    emit featureRemoved(id);
}

void FissionDocument::moveFeature(int from, int to) {
    checkpoint();
    m_timeline->move(from, to);
    setModified(true);
    rebuild();
}

FissionFeature *FissionDocument::featureById(const QString &id) const {
    for (auto *f : m_timeline->features())
        if (f->id() == id) return f;
    return nullptr;
}

QVariantList FissionDocument::featureList(const QString &type) const {
    QVariantList out;
    const auto &fs = m_timeline->features();
    for (int i = 0; i < m_timeline->marker() && i < fs.size(); ++i) {
        FissionFeature *f = fs[i];
        if (f->isSuppressed() || (!type.isEmpty() && f->type() != type)) continue;
        out.append(QVariantMap{{"id", f->id()}, {"name", f->name()}, {"type", f->type()}, {"healthy", f->isHealthy()}});
    }
    return out;
}

QList<FissionFeature *> FissionDocument::overlayFeatures() const {
    QList<FissionFeature *> out;
    const auto &fs = m_timeline->features();
    for (int i = 0; i < m_timeline->marker() && i < fs.size(); ++i) {
        FissionFeature *f = fs[i];
        if (!f->isSuppressed() && f->isVisible() && f->isHealthy() && !f->overlayShape().IsNull()) out.append(f);
    }
    return out;
}

// --- Parameters ------------------------------------------------------------

void FissionDocument::attachParameter(FissionParameter *p) {
    p->setParent(this);
    connect(p, &FissionParameter::changed, this, [this] {
        if (m_restoring) return;
        setModified(true);
        scheduleRebuild();
    });
}

FissionParameter *FissionDocument::addParameter(const QString &name, const QString &expression,
                                                const QString &unit, const QString &comment) {
    if (!Expression::isValidName(name)) {
        m_lastError = QString("'%1' is not a valid parameter name").arg(name);
        return nullptr;
    }
    if (parameter(name)) {
        m_lastError = QString("Parameter '%1' already exists").arg(name);
        return nullptr;
    }
    checkpoint();
    auto *p = new FissionParameter(name, expression, unit, this);
    p->setComment(comment);
    attachParameter(p);
    m_parameters.append(p);
    setModified(true);
    emit parametersChanged();
    rebuild();
    return p;
}

bool FissionDocument::removeParameter(const QString &name) {
    FissionParameter *p = parameter(name);
    if (!p) return false;
    checkpoint();
    m_parameters.removeOne(p);
    p->disconnect(this);
    p->deleteLater();
    setModified(true);
    emit parametersChanged();
    rebuild();
    return true;
}

FissionParameter *FissionDocument::parameter(const QString &name) const {
    for (auto *p : m_parameters)
        if (p->name() == name) return p;
    return nullptr;
}

void FissionDocument::evaluateParameters() {
    m_paramValues.clear();
    QHash<QString, FissionParameter *> byName;
    for (auto *p : m_parameters) byName.insert(p->name(), p);

    QHash<QString, int> state;   // 1 = evaluating, 2 = done
    QHash<QString, QString> errors;
    std::function<double(const QString &, bool &)> resolve = [&](const QString &name, bool &ok) -> double {
        FissionParameter *p = byName.value(name, nullptr);
        if (!p) { ok = false; return 0.0; }
        ok = true;
        if (state.value(name) == 2) return m_paramValues.value(name);
        if (state.value(name) == 1) {
            errors[name] = "Circular reference";
            return 0.0;
        }
        state[name] = 1;
        QString err;
        const double v = Expression::evaluate(p->expression(), resolve, &err);
        if (!err.isEmpty() && !errors.contains(name)) errors[name] = err;
        state[name] = 2;
        m_paramValues.insert(name, v);
        return v;
    };
    for (auto *p : m_parameters) {
        bool ok = false;
        resolve(p->name(), ok);
    }
    for (auto *p : m_parameters) p->setEvaluated(m_paramValues.value(p->name()), errors.value(p->name()));
}

double FissionDocument::evaluate(const QString &expr, QString *error) const {
    return Expression::evaluate(expr, [this](const QString &name, bool &ok) {
        ok = m_paramValues.contains(name);
        return m_paramValues.value(name);
    }, error);
}

QVariantMap FissionDocument::evaluateExpression(const QString &expr) const {
    QString err;
    const double v = evaluate(expr, &err);
    return {{"value", v}, {"error", err}};
}

// --- Rebuild ---------------------------------------------------------------

void FissionDocument::scheduleRebuild() {
    m_rebuildTimer.start();
}

void FissionDocument::rebuild() {
    m_rebuildTimer.stop();
    evaluateParameters();

    QStringList key{m_lengthUnit};
    for (auto it = m_paramValues.cbegin(); it != m_paramValues.cend(); ++it)
        key << it.key() + "=" + QString::number(it.value(), 'g', 17);
    key.sort();
    const QString stepsKey = key.join(';');
    if (stepsKey != m_stepsKey) { m_steps.clear(); m_stepsKey = stepsKey; }

    const auto &fs = m_timeline->features();
    const int end = std::min<int>(fs.size(), m_timeline->marker());

    auto hasFusionSnap = [](FissionFeature *f) {
        if (!f || !f->flag(QStringLiteral("fusionPassThrough"))) return false;
        if (f->flag(QStringLiteral("snapshotEmpty"))) return true;
        return !f->str(QStringLiteral("snapshot")).isEmpty()
            || !f->str(QStringLiteral("snapshotStepB64")).isEmpty();
    };
    // Fusion imports carry a full BRep after each solid step. Decoding every step OOM-crashes large
    // files; only the last snapshot at/before the marker is required for the visible model.
    int lastFusionSnap = -1;
    for (int i = 0; i < end; ++i)
        if (!fs[i]->isSuppressed() && hasFusionSnap(fs[i])) lastFusionSnap = i;
    const bool fusionHeavy = lastFusionSnap >= 0;

    int reuse = 0;
    if (fusionHeavy) {
        // Intermediate fusion snapshots were skipped — step cache is not trustworthy across marker moves
        m_steps.clear();
        reuse = 0;
    } else {
        while (reuse < m_steps.size() && reuse < fs.size() && m_steps[reuse].feature == fs[reuse]
               && m_steps[reuse].revision == fs[reuse]->revision() && m_steps[reuse].suppressed == fs[reuse]->isSuppressed())
            ++reuse;
        // Steps past the marker stay cached for rolling forward again, unless something before them changed
        if (reuse < end) m_steps.erase(m_steps.begin() + reuse, m_steps.end());
    }
    const int start = std::min(reuse, end);

    QElapsedTimer total, one;
    total.start();
    RebuildContext ctx = start > 0 ? m_steps[start - 1].after : RebuildContext(this);
    for (int i = start; i < end; ++i) {
        FissionFeature *f = fs[i];
        if (f->isSuppressed()) {
            if (!f->m_error.isEmpty()) { f->m_error.clear(); emit f->statusChanged(); }
        } else if (hasFusionSnap(f) && i != lastFusionSnap) {
            // Skip expensive STEP/BRep decode for superseded fusion steps
            if (!f->m_error.isEmpty()) { f->m_error.clear(); emit f->statusChanged(); }
            ctx.registerFeature(f);
        } else {
            ctx.featureIndex = i;
            const QList<BodyState> before = ctx.bodies;
            one.start();
            if (f->run(ctx)) ctx.registerFeature(f);
            else ctx.bodies = before;   // a failed feature leaves the model as it was
            if (one.elapsed() > 50) qInfo("rebuild: %s (%s) took %lld ms", qPrintable(f->name()), qPrintable(f->type()), one.elapsed());
        }
        m_steps.append({f, f->revision(), f->isSuppressed(), ctx});
    }
    if (total.elapsed() > 50) qInfo("rebuild: steps %d-%d of %d took %lld ms", start + 1, end, end, total.elapsed());

    m_hasInput = false;
    m_inputShapes.clear();
    for (int i = 0; i < end && !m_inputFeatureId.isEmpty(); ++i)
        if (fs[i]->id() == m_inputFeatureId && !fs[i]->isSuppressed()) {
            if (i > 0)
                for (const BodyState &b : m_steps[i - 1].after.bodies) m_inputShapes.append(b.shape);
            m_hasInput = true;
            break;
        }

    while (m_bodies.size() > ctx.bodies.size()) m_bodies.takeLast()->deleteLater();
    while (m_bodies.size() < ctx.bodies.size()) {
        auto *b = new FissionBody(this);
        connect(b, &FissionBody::visibilityChanged, this, &FissionDocument::displayChanged);
        connect(b, &FissionBody::appearanceChanged, this, [this] {
            if (!m_restoring) setModified(true);
            emit displayChanged();
        });
        m_bodies.append(b);
    }
    for (int i = 0; i < ctx.bodies.size(); ++i) {
        m_bodies[i]->setSourceFeature(ctx.bodies[i].sourceFeature);
        m_bodies[i]->setShape(ctx.bodies[i].name, ctx.bodies[i].shape);
    }

    ++m_rebuildCount;
    one.start();
    emit bodiesChanged();
    if (one.elapsed() > 50) qInfo("rebuild: bodiesChanged handlers took %lld ms", one.elapsed());
    one.start();
    emit rebuilt();
    if (one.elapsed() > 50) qInfo("rebuild: rebuilt handlers took %lld ms", one.elapsed());
    one.start();
    emit displayChanged();
    if (one.elapsed() > 50) qInfo("rebuild: displayChanged handlers took %lld ms", one.elapsed());
}

void FissionDocument::setInputFeatureId(const QString &id) {
    if (m_inputFeatureId == id) return;
    m_inputFeatureId = id;
    emit inputFeatureChanged();
    rebuild();
}

TopoDS_Shape FissionDocument::displayShape(int i) const {
    if (m_hasInput) return m_inputShapes.value(i);
    FissionBody *b = body(i);
    return b ? b->shape() : TopoDS_Shape();
}

QVariantList FissionDocument::faceEdges(const QVariantMap &faceRef) const {
    const int body = faceRef.value("body", -1).toInt();
    const TopoDS_Shape shape = displayShape(body);
    if (shape.IsNull() || faceRef.value("kind").toString() != "face") return {};
    const auto faces = GeomUtils::faceMap(shape);
    const int fi = faceRef.value("index", -1).toInt() + 1;
    if (fi < 1 || fi > faces.Extent()) return {};
    const auto edges = GeomUtils::edgeMap(shape);
    QVariantList out;
    for (TopExp_Explorer e(faces(fi), TopAbs_EDGE); e.More(); e.Next()) {
        if (BRep_Tool::Degenerated(TopoDS::Edge(e.Current()))) continue;
        const int i = edges.FindIndex(e.Current()) - 1;
        if (i < 0) continue;
        const QVariantMap r = GeomUtils::edgeRef(body, shape, i);
        if (!out.contains(r)) out.append(r);
    }
    return out;
}

QVariantMap FissionDocument::planeFrame(const QVariant &refArg) const {
    gp_Ax3 ax;
    bool ok = false;
    const QVariantMap r = refArg.toMap();
    const QString kind = r.value("kind").toString();
    if (refArg.typeId() == QMetaType::QString) {
        ax = GeomUtils::standardPlane(refArg.toString(), &ok);
    } else if (kind == "originplane") {
        ax = GeomUtils::standardPlane(r.value("id").toString(), &ok);
    } else if (kind == "plane") {
        FissionFeature *f = featureById(r.value("id").toString());
        if (auto *cp = qobject_cast<FissionConstructionPlane *>(f)) { ax = cp->plane(); ok = true; }
        else if (auto *sk = qobject_cast<FissionSketch *>(f)) { ax = sk->plane(); ok = true; }
    } else if (kind == "face") {
        const TopoDS_Shape shape = displayShape(r.value("body", -1).toInt());
        const int fi = r.value("index", -1).toInt() + 1;
        if (!shape.IsNull() && !MeshUtils::isMesh(shape)) {
            const auto faces = GeomUtils::faceMap(shape);
            if (fi >= 1 && fi <= faces.Extent()) {
                const TopoDS_Face face = TopoDS::Face(faces(fi));
                BRepAdaptor_Surface surf(face);
                if (surf.GetType() == GeomAbs_Plane) {
                    gp_Dir n = surf.Plane().Position().Direction();
                    if (face.Orientation() == TopAbs_REVERSED) n.Reverse();
                    ax = gp_Ax3(GeomUtils::faceCentroid(face), n);
                    ok = true;
                }
            }
        }
    }
    if (!ok) return {{"ok", false}};
    const gp_Pnt o = ax.Location();
    const gp_Dir n = ax.Direction(), x = ax.XDirection(), y = ax.YDirection();
    return {{"ok", true}, {"origin", QVariantList{o.X(), o.Y(), o.Z()}}, {"normal", QVariantList{n.X(), n.Y(), n.Z()}},
            {"x", QVariantList{x.X(), x.Y(), x.Z()}}, {"y", QVariantList{y.X(), y.Y(), y.Z()}}};
}

// --- Analyses --------------------------------------------------------------

QString FissionDocument::addAnalysis(const QVariantMap &analysis) {
    checkpoint();
    QVariantMap a = analysis;
    const QString type = a.value("type", "section").toString();
    a["type"] = type;
    int n = 1;
    auto taken = [&](const QString &key, const QString &value) {
        for (const QVariant &v : m_analyses)
            if (v.toMap().value(key).toString() == value) return true;
        return false;
    };
    while (taken("id", QString("analysis%1").arg(n))) ++n;
    a["id"] = QString("analysis%1").arg(n);
    if (a.value("name").toString().isEmpty()) {
        const QString base = type == "section" ? QStringLiteral("Section") : type;
        int k = 1;
        while (taken("name", base + QString::number(k))) ++k;
        a["name"] = base + QString::number(k);
    }
    if (!a.contains("visible")) a["visible"] = true;
    if (a["visible"].toBool())
        for (QVariant &v : m_analyses) {
            QVariantMap o = v.toMap();
            if (o.value("type") == type) { o["visible"] = false; v = o; }
        }
    m_analyses.append(a);
    setModified(true);
    emit analysesChanged();
    return a["id"].toString();
}

void FissionDocument::updateAnalysis(const QString &id, const QVariantMap &changes) {
    int idx = -1;
    for (int i = 0; i < m_analyses.size(); ++i)
        if (m_analyses[i].toMap().value("id").toString() == id) idx = i;
    if (idx < 0) return;
    QVariantMap a = m_analyses[idx].toMap();
    if (!(changes.size() == 1 && changes.contains("visible"))) checkpoint();
    for (auto it = changes.begin(); it != changes.end(); ++it)
        if (it.key() != "id") a.insert(it.key(), it.value());
    if (a.value("visible").toBool())
        for (int i = 0; i < m_analyses.size(); ++i) {
            QVariantMap o = m_analyses[i].toMap();
            if (i != idx && o.value("type") == a.value("type") && o.value("visible").toBool()) {
                o["visible"] = false;
                m_analyses[i] = o;
            }
        }
    m_analyses[idx] = a;
    setModified(true);
    emit analysesChanged();
}

void FissionDocument::removeAnalysis(const QString &id) {
    for (int i = 0; i < m_analyses.size(); ++i)
        if (m_analyses[i].toMap().value("id").toString() == id) {
            checkpoint();
            m_analyses.removeAt(i);
            setModified(true);
            emit analysesChanged();
            return;
        }
}

QVariantMap FissionDocument::analysis(const QString &id) const {
    for (const QVariant &v : m_analyses)
        if (v.toMap().value("id").toString() == id) return v.toMap();
    return {};
}

QVariantList FissionDocument::tangentChain(const QVariantMap &edgeRef) const {
    const int body = edgeRef.value("body", -1).toInt();
    const TopoDS_Shape shape = displayShape(body);
    if (shape.IsNull() || edgeRef.value("kind").toString() != "edge") return {edgeRef};
    QVariantList out;
    for (int i : GeomUtils::tangentChain(shape, edgeRef.value("index").toInt()))
        out.append(GeomUtils::edgeRef(body, shape, i));
    return out.isEmpty() ? QVariantList{edgeRef} : out;
}

QVariantMap FissionDocument::bodyProperties(int index) const {
    FissionBody *b = body(index);
    if (!b || b->shape().IsNull()) return {};
    const bool mesh = b->isMesh();
    const FissionBody::Geometry &g = b->geometry();
    const double volume = g.volume;   // mm^3
    return {{"name", b->name()}, {"volume", volume}, {"area", b->area()},
            {"mass", volume / 1000.0 * m_density}, {"density", m_density},
            {"centroid", QVariantList{g.centroid[0], g.centroid[1], g.centroid[2]}},
            {"size", QVariantList{g.box[3] - g.box[0], g.box[4] - g.box[1], g.box[5] - g.box[2]}},
            {"faces", b->faceCount()}, {"edges", b->edgeCount()}, {"mesh", mesh},
            {"triangles", mesh ? b->meshStats().value("triangles").toInt() : 0},
            {"meshStats", mesh ? b->meshStats() : QVariantMap()}};
}

QVariantList FissionDocument::bodyBox(int index) const {
    FissionBody *b = body(index);
    if (!b || b->shape().IsNull()) return {};
    double bx[6];
    if (!b->box(bx)) return {};
    return {bx[0], bx[1], bx[2], bx[3], bx[4], bx[5]};
}

// --- Undo ------------------------------------------------------------------

void FissionDocument::checkpoint() {
    if (m_restoring) return;
    m_undo.append(toJson());
    if (m_undo.size() > 100) m_undo.removeFirst();
    m_redo.clear();
    emit undoStackChanged();
}

void FissionDocument::undo() {
    if (m_undo.isEmpty()) return;
    m_redo.append(toJson());
    restore(m_undo.takeLast());
    emit undoStackChanged();
}

void FissionDocument::redo() {
    if (m_redo.isEmpty()) return;
    m_undo.append(toJson());
    restore(m_redo.takeLast());
    emit undoStackChanged();
}

void FissionDocument::restore(const QJsonObject &state) {
    m_restoring = true;
    fromJson(state);   // rebuilds
    m_restoring = false;
    setModified(true);
}

// --- Serialization ---------------------------------------------------------

void FissionDocument::clearContent() {
    for (auto *f : m_timeline->features()) {
        f->disconnect(this);
        f->deleteLater();
    }
    m_timeline->clear();
    for (auto *p : m_parameters) {
        p->disconnect(this);
        p->deleteLater();
    }
    m_parameters.clear();
    m_analyses.clear();
}

QJsonObject FissionDocument::toJson() const {
    QJsonObject obj;
    obj["format"] = "Fission360";
    obj["version"] = 1;
    obj["documentId"] = m_documentId;
    obj["contentRevision"] = m_contentRevision;
    obj["versionNumber"] = m_versionNumber;
    obj["name"] = m_name;
    obj["density"] = m_density;
    obj["units"] = QJsonObject{{"length", m_lengthUnit}, {"mass", m_massUnit}};
    obj["info"] = QJsonObject::fromVariantMap(m_info);
    if (!m_linkedAssets.isEmpty())
        obj["linkedAssets"] = QJsonArray::fromVariantList(m_linkedAssets);

    QJsonArray params;
    for (auto *p : m_parameters) params.append(p->toJson());
    obj["parameters"] = params;

    QJsonArray features;
    for (auto *f : m_timeline->features()) features.append(f->toJson());
    QJsonObject timeline;
    timeline["marker"] = m_timeline->marker();
    timeline["features"] = features;
    obj["timeline"] = timeline;

    QJsonArray display;
    for (auto *b : m_bodies) {
        QJsonObject d{{"visible", b->isVisible()},
                      {"color", b->color().name(QColor::HexRgb)},
                      {"opacity", b->opacity()},
                      {"appearanceId", b->appearanceId()},
                      {"roughness", b->roughness()},
                      {"metalness", b->metalness()},
                      {"texturePath", b->texturePath()},
                      {"roughnessPath", b->roughnessPath()},
                      {"metalnessPath", b->metalnessPath()},
                      {"normalPath", b->normalPath()}};
        const QVariantMap faces = b->faceAppearancesMap();
        if (!faces.isEmpty())
            d["faceAppearances"] = QJsonObject::fromVariantMap(faces);
        display.append(d);
    }
    obj["bodyDisplay"] = display;
    obj["analyses"] = QJsonArray::fromVariantList(m_analyses);
    if (m_renderSettings) obj["render"] = QJsonObject::fromVariantMap(m_renderSettings->toJson());
    return obj;
}

void FissionDocument::fromJson(const QJsonObject &obj) {
    const bool wasRestoring = m_restoring;
    m_restoring = true;
    clearContent();
    setName(obj["name"].toString("Untitled"));
    if (obj.contains(QStringLiteral("documentId")) && !obj["documentId"].toString().isEmpty())
        m_documentId = obj["documentId"].toString();
    m_contentRevision = obj["contentRevision"].toVariant().toLongLong();
    if (m_contentRevision < 1) m_contentRevision = 1;
    m_versionNumber = obj["versionNumber"].toInt(0);
    m_linkedAssets = obj["linkedAssets"].toArray().toVariantList();
    m_density = obj["density"].toDouble(7.85);
    emit densityChanged();
    emit contentRevisionChanged();
    emit versionNumberChanged();
    emit linkedAssetsChanged();
    const QJsonObject units = obj["units"].toObject();
    const QString len = units["length"].toString("mm"), mass = units["mass"].toString("g");
    m_lengthUnit = hasUnit(lengthUnitTable(), len) ? len : QStringLiteral("mm");
    m_massUnit = hasUnit(massUnitTable(), mass) ? mass : QStringLiteral("g");
    emit unitsChanged();
    m_info = obj["info"].toObject().toVariantMap();
    emit infoChanged();

    for (const QJsonValue &v : obj["parameters"].toArray()) {
        auto *p = new FissionParameter(QString(), QString(), QString(), this);
        p->fromJson(v.toObject());
        attachParameter(p);
        m_parameters.append(p);
    }

    const QJsonObject timeline = obj["timeline"].toObject();
    const QJsonArray features = timeline["features"].toArray();
    for (const QJsonValue &v : features) {
        FissionFeature *f = FissionFeature::fromJson(v.toObject(), this);
        if (!f) continue;
        attachFeature(f);
        m_timeline->insert(m_timeline->count(), f);
    }
    m_timeline->setMarker(timeline["marker"].toInt(m_timeline->count()));
    m_restoring = wasRestoring;

    rebuild();
    const QJsonArray display = obj["bodyDisplay"].toArray();
    for (int i = 0; i < display.size() && i < m_bodies.size(); ++i) {
        const QJsonObject d = display[i].toObject();
        m_bodies[i]->setVisible(d["visible"].toBool(true));
        if (d.contains("appearanceId") && !d["appearanceId"].toString().isEmpty())
            m_bodies[i]->applyAppearance(d["appearanceId"].toString());
        else {
            if (d.contains("color")) m_bodies[i]->setColor(QColor(d["color"].toString()));
            m_bodies[i]->setOpacity(d.contains("opacity") ? d["opacity"].toDouble() : 1.0);
            if (d.contains("roughness")) m_bodies[i]->setRoughness(d["roughness"].toDouble());
            if (d.contains("metalness")) m_bodies[i]->setMetalness(d["metalness"].toDouble());
        }
        if (d.contains("texturePath")) m_bodies[i]->setTexturePath(d["texturePath"].toString());
        if (d.contains("roughnessPath")) m_bodies[i]->setRoughnessPath(d["roughnessPath"].toString());
        if (d.contains("metalnessPath")) m_bodies[i]->setMetalnessPath(d["metalnessPath"].toString());
        if (d.contains("normalPath")) m_bodies[i]->setNormalPath(d["normalPath"].toString());
        if (d.contains("faceAppearances"))
            m_bodies[i]->setFaceAppearancesFromJson(d["faceAppearances"].toObject().toVariantMap());
    }
    m_analyses = obj["analyses"].toArray().toVariantList();
    emit analysesChanged();
    if (m_renderSettings && obj.contains("render"))
        m_renderSettings->fromJson(obj["render"].toObject().toVariantMap());
    emit parametersChanged();
}

bool FissionDocument::save(const QString &path) {
    ++m_versionNumber;
    emit versionNumberChanged();

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        m_lastError = file.errorString();
        --m_versionNumber;
        emit versionNumberChanged();
        return false;
    }

    // Large Fusion imports carry tens of MB of snapshotStepB64. Serializing twice (file + version
    // history) with Indented JSON froze the UI ("Not Responding"). One Compact encode, chunked write,
    // and progress ticks keep the event loop alive.
    FissionProgress::begin(QStringLiteral("save"), QStringLiteral("Saving…"));
    auto pump = [] {
        if (QCoreApplication::instance())
            QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents, 50);
    };
    FissionProgress::report(QStringLiteral("save"), 0.05, QStringLiteral("Serializing…"));
    pump();

    const QJsonObject root = toJson();
    FissionProgress::report(QStringLiteral("save"), 0.35, QStringLiteral("Encoding…"));
    pump();

    const QByteArray compact = QJsonDocument(root).toJson(QJsonDocument::Compact);
    // Thumbnail first so the Projects panel / Explorer handler can read only the head of the file
    QByteArray json = compact;
    if (!m_thumbnail.isEmpty()) {
        const qsizetype brace = json.indexOf('{');
        if (brace >= 0) {
            const bool empty = json.indexOf('"', brace) < 0;
            json.insert(brace + 1,
                        "\"thumbnail\": \"" + m_thumbnail.toBase64() + (empty ? "\"" : "\","));
        }
    }

    FissionProgress::report(QStringLiteral("save"), 0.45, QStringLiteral("Writing…"));
    pump();
    constexpr qsizetype kChunk = 1 << 20; // 1 MiB
    for (qsizetype off = 0; off < json.size(); off += kChunk) {
        const qsizetype n = qMin(kChunk, json.size() - off);
        if (file.write(json.constData() + off, n) != n) {
            m_lastError = file.errorString();
            FissionProgress::end(QStringLiteral("save"));
            --m_versionNumber;
            emit versionNumberChanged();
            return false;
        }
        if (json.size() > 0) {
            FissionProgress::report(QStringLiteral("save"),
                                    0.45 + 0.40 * double(off + n) / double(json.size()),
                                    QStringLiteral("Writing…"));
            pump();
        }
    }
    file.close();

    m_filePath = path;
    setName(QFileInfo(path).completeBaseName());
    emit nameChanged();
    // Saving does not bump contentRevision — linked drawings stay current if design didn't change since link
    m_modified = false;
    emit modifiedChanged();

    FissionProgress::report(QStringLiteral("save"), 0.92, QStringLiteral("Version history…"));
    pump();
    // Reuse the Compact payload (no thumbnail) — do not call toJson() again
    DocumentVersionStore::instance()->pushVersion(
        m_filePath, m_versionNumber, compact,
        QStringLiteral("v%1").arg(m_versionNumber));
    DocumentVersionStore::instance()->clearAutosave(m_documentId);
    FissionProgress::end(QStringLiteral("save"));
    return true;
}

void FissionDocument::setFilePath(const QString &path) {
    if (path == m_filePath) return;
    m_filePath = path;
    setName(QFileInfo(path).completeBaseName());
    emit nameChanged();
}

static double fusionPt(const QJsonArray &a, int i) {
    return i < a.size() ? a.at(i).toDouble() : 0.0;
}

// Map a Fusion sketch (curves in sketch space) to a healthy Fission Sketch feature.
static QJsonObject fusionSketchToFeature(const QJsonObject &fs) {
    QJsonObject feat;
    feat["type"] = QStringLiteral("Sketch");
    feat["name"] = fs["name"].toString(QStringLiteral("Sketch"));
    feat["suppressed"] = false;
    feat["visible"] = fs["isVisible"].toBool(true);

    QString plane = QStringLiteral("XY");
    double offsetMm = 0;
    const QJsonObject pl = fs["plane"].toObject();
    if (!pl.isEmpty()) {
        const QJsonArray n = pl["normal"].toArray(), o = pl["origin"].toArray();
        const double nx = std::fabs(fusionPt(n, 0)), ny = std::fabs(fusionPt(n, 1)), nz = std::fabs(fusionPt(n, 2));
        if (nz >= nx && nz >= ny) {
            plane = QStringLiteral("XY");
            offsetMm = fusionPt(o, 2);
        } else if (ny >= nx) {
            plane = QStringLiteral("XZ");
            offsetMm = fusionPt(o, 1);
        } else {
            plane = QStringLiteral("YZ");
            offsetMm = fusionPt(o, 0);
        }
    }

    QJsonObject props;
    // Offset sketches: put geometry on the matching origin plane; large offsets stay on XY
    // (construction-plane chaining is a later upgrade). Curves are still useful for editing.
    props["plane"] = plane;
    Q_UNUSED(offsetMm);

    QJsonArray ents;
    for (const QJsonValue &cv : fs["curves"].toArray()) {
        const QJsonObject c = cv.toObject();
        const QString t = c["type"].toString();
        QJsonObject e;
        e["construction"] = c["construction"].toBool(false);
        if (t == "line") {
            const QJsonArray s = c["start"].toArray(), en = c["end"].toArray();
            e["type"] = QStringLiteral("line");
            e["x1"] = fusionPt(s, 0); e["y1"] = fusionPt(s, 1);
            e["x2"] = fusionPt(en, 0); e["y2"] = fusionPt(en, 1);
            ents.append(e);
        } else if (t == "circle") {
            const QJsonArray ctr = c["center"].toArray();
            e["type"] = QStringLiteral("circle");
            e["cx"] = fusionPt(ctr, 0); e["cy"] = fusionPt(ctr, 1);
            e["r"] = c["radius"].toDouble();
            if (e["r"].toDouble() > 1e-9) ents.append(e);
        } else if (t == "arc") {
            const QJsonArray ctr = c["center"].toArray();
            const QJsonArray s = c["start"].toArray(), en = c["end"].toArray();
            const double cx = fusionPt(ctr, 0), cy = fusionPt(ctr, 1);
            const double r = c["radius"].toDouble();
            if (r <= 1e-9) continue;
            constexpr double rad2deg = 180.0 / 3.14159265358979323846;
            const double a1 = std::atan2(fusionPt(s, 1) - cy, fusionPt(s, 0) - cx) * rad2deg;
            const double a2 = std::atan2(fusionPt(en, 1) - cy, fusionPt(en, 0) - cx) * rad2deg;
            e["type"] = QStringLiteral("arc");
            e["cx"] = cx; e["cy"] = cy; e["r"] = r;
            e["a1"] = a1; e["a2"] = a2;
            ents.append(e);
        }
    }
    for (const QJsonValue &pv : fs["points"].toArray()) {
        const QJsonArray p = pv.toArray();
        if (p.size() < 2) continue;
        QJsonObject e;
        e["type"] = QStringLiteral("point");
        e["x"] = fusionPt(p, 0); e["y"] = fusionPt(p, 1);
        ents.append(e);
    }
    props["entities"] = ents;
    props["constraints"] = QJsonArray();
    feat["props"] = props;
    return feat;
}

static QString newFeatId() { return QUuid::createUuid().toString(QUuid::WithoutBraces); }

static QString mmExpr(double v) { return QString::number(v, 'g', 12) + QStringLiteral(" mm"); }
static QString degExpr(double v) { return QString::number(v, 'g', 12) + QStringLiteral(" deg"); }

static QString mapFusionType(const QString &raw) {
    const QString t = raw.toLower();
    if (t.contains("extrude")) return QStringLiteral("Extrude");
    if (t.contains("revolve")) return QStringLiteral("Revolve");
    if (t.contains("fillet")) return QStringLiteral("Fillet");
    if (t.contains("chamfer")) return QStringLiteral("Chamfer");
    if (t.contains("shell")) return QStringLiteral("Shell");
    if (t.contains("hole")) return QStringLiteral("Hole");
    if (t.contains("sweep")) return QStringLiteral("Sweep");
    if (t.contains("loft")) return QStringLiteral("Loft");
    if (t.contains("mirror")) return QStringLiteral("Mirror");
    if (t.contains("circularpattern")) return QStringLiteral("CircularPattern");
    if (t.contains("rectangularpattern")) return QStringLiteral("RectangularPattern");
    if (t.contains("thread")) return QStringLiteral("Thread");
    if (t.contains("combine")) return QStringLiteral("Combine");
    if (t.contains("draft")) return QStringLiteral("Draft");
    if (t.contains("rib")) return QStringLiteral("Rib");
    if (t.contains("sketch")) return QStringLiteral("Sketch");
    if (t.contains("constructionplane") || t.contains("workplane")) return QStringLiteral("ConstructionPlane");
    return QString();
}

static QString mapFusionOperation(int op) {
    // Fusion FeatureOperations: Join=0, Cut=1, Intersect=2, NewBody=3
    switch (op) {
    case 1: return QStringLiteral("cut");
    case 2: return QStringLiteral("intersect");
    case 3: return QStringLiteral("newBody");
    default: return QStringLiteral("join");
    }
}

// Prefer keeping Fusion STEP base64 on the feature (snapshotStepB64) and converting lazily on rebuild.
// Eager OCCT conversion of every timeline step OOM-crashes large exports (100+ MB of stepData).

static QJsonObject fusionSolidFeature(const QString &fType, const QJsonObject &src,
                                      const QString &sketchId, bool passThrough) {
    QJsonObject feat;
    feat["id"] = newFeatId();
    feat["type"] = fType;
    feat["name"] = src["name"].toString(fType);
    feat["suppressed"] = src["isSuppressed"].toBool(false);
    feat["visible"] = true;
    QJsonObject props;
    props["fusionPassThrough"] = passThrough;
    props["fusionType"] = src["type"].toString();

    auto takeMm = [&](const char *key, const char *fallbackKey = nullptr) {
        if (src.contains(QLatin1String(key) + QLatin1String("Expr")))
            return src[QLatin1String(key) + QLatin1String("Expr")].toString();
        if (src.contains(key)) return mmExpr(src[key].toDouble());
        if (fallbackKey && src.contains(fallbackKey)) return mmExpr(src[fallbackKey].toDouble());
        return QString();
    };
    auto takeDeg = [&](const char *key) {
        if (src.contains(QLatin1String(key) + QLatin1String("Expr")))
            return src[QLatin1String(key) + QLatin1String("Expr")].toString();
        if (src.contains(key)) return degExpr(src[key].toDouble());
        return QString();
    };

    if (fType == "Extrude") {
        props["sketch"] = sketchId;
        props["profiles"] = QJsonArray();
        props["extent"] = QStringLiteral("distance");
        const QString d = takeMm("distance");
        props["distance"] = d.isEmpty() ? QStringLiteral("10 mm") : d;
        const QString tap = takeDeg("taperAngle");
        if (tap.isEmpty()) {
            const QString t2 = takeDeg("taper");
            props["taper"] = t2.isEmpty() ? QStringLiteral("0 deg") : t2;
        } else props["taper"] = tap;
        props["operation"] = mapFusionOperation(src["operation"].toInt(3));
        props["flip"] = src["isOpposite"].toBool(false);
    } else if (fType == "Revolve") {
        props["sketch"] = sketchId;
        props["profiles"] = QJsonArray();
        props["axis"] = QStringLiteral("Y");
        props["extent"] = QStringLiteral("angle");
        const QString a = takeDeg("angle");
        props["angle"] = a.isEmpty() ? QStringLiteral("360 deg") : a;
        props["operation"] = mapFusionOperation(src["operation"].toInt(3));
    } else if (fType == "Fillet") {
        const QString r = takeMm("radius");
        props["radius"] = r.isEmpty() ? QStringLiteral("2 mm") : r;
        props["edges"] = QJsonArray();
        props["body"] = 0;
        props["allEdges"] = true;
        props["tangentChain"] = true;
    } else if (fType == "Chamfer") {
        const QString d = takeMm("distance");
        props["distance"] = d.isEmpty() ? QStringLiteral("1 mm") : d;
        props["edges"] = QJsonArray();
        props["body"] = 0;
        props["allEdges"] = true;
        props["chamferType"] = QStringLiteral("equal");
    } else if (fType == "Shell") {
        const QString th = takeMm("thickness");
        props["thickness"] = th.isEmpty() ? QStringLiteral("2 mm") : th;
        props["body"] = 0;
        props["faces"] = QJsonArray();
        props["direction"] = QStringLiteral("inside");
    } else if (fType == "Hole") {
        const QString dia = takeMm("diameter");
        props["diameter"] = dia.isEmpty() ? QStringLiteral("6 mm") : dia;
        const QString depth = takeMm("depth", "distance");
        props["depth"] = depth.isEmpty() ? QStringLiteral("10 mm") : depth;
        props["extent"] = QStringLiteral("distance");
    } else {
        // Keep any numeric props we exported for display / later mapping
        for (auto it = src.begin(); it != src.end(); ++it) {
            if (it.key() == "name" || it.key() == "type" || it.key() == "timelineIndex") continue;
            props[it.key()] = it.value();
        }
    }
    feat["props"] = props;
    return feat;
}

// Convert a Fusion-exported .f360 into native Fission360 JSON with a full timeline.
static QJsonObject convertFromFusionExport(const QJsonObject &src) {
    QJsonObject dst;
    dst["format"] = QStringLiteral("Fission360");
    dst["version"] = 1;

    const QJsonObject docInfo = src["document"].toObject();
    dst["name"] = docInfo["name"].toString(QStringLiteral("Imported"));
    dst["density"] = 7.85;

    QJsonObject units;
    units["length"] = docInfo["units"].toString(QStringLiteral("mm"));
    units["mass"] = QStringLiteral("g");
    dst["units"] = units;

    const QJsonObject paramsObj = src["parameters"].toObject();
    QJsonArray paramsArr;
    for (auto it = paramsObj.begin(); it != paramsObj.end(); ++it) {
        QJsonObject p = it.value().toObject();
        p["name"] = it.key();
        p["expression"] = p["expression"].toString(QString::number(p["value"].toDouble()));
        if (!p.contains("unit")) p["unit"] = QStringLiteral("mm");
        paramsArr.append(p);
    }
    dst["parameters"] = paramsArr;

    // Index Fusion sketches / features for timeline reconstruction
    QHash<QString, QJsonObject> sketchByName;
    QHash<int, QJsonObject> sketchByIndex;
    for (const QJsonValue &sv : src["sketches"].toArray()) {
        const QJsonObject sk = sv.toObject();
        sketchByName.insert(sk["name"].toString(), sk);
        const int ti = sk["timelineIndex"].toInt(-1);
        if (ti >= 0) sketchByIndex.insert(ti, sk);
    }
    QHash<int, QJsonObject> featByIndex;
    QHash<QString, QJsonObject> featByName;
    for (const QJsonValue &fv : src["features"].toArray()) {
        const QJsonObject f = fv.toObject();
        featByName.insert(f["name"].toString(), f);
        const int ti = f["timelineIndex"].toInt(-1);
        if (ti >= 0) featByIndex.insert(ti, f);
    }

    QJsonArray tlFeatures;
    QHash<QString, QString> sketchIdByName;
    QString lastSketchId;
    QSet<int> usedSketchIdx, usedFeatIdx;
    int snapshotCount = 0;

    auto appendSketch = [&](const QJsonObject &skSrc) {
        QJsonObject sk = fusionSketchToFeature(skSrc);
        const QString id = newFeatId();
        sk["id"] = id;
        sketchIdByName.insert(skSrc["name"].toString(), id);
        lastSketchId = id;
        tlFeatures.append(sk);
        const int ti = skSrc["timelineIndex"].toInt(-1);
        if (ti >= 0) usedSketchIdx.insert(ti);
    };

    const QJsonArray tlArr = src["timeline"].toArray();
    if (!tlArr.isEmpty()) {
        for (const QJsonValue &tv : tlArr) {
            const QJsonObject entry = tv.toObject();
            const int idx = entry["index"].toInt(-1);
            const QString rawType = entry["type"].toString();
            const QString name = entry["name"].toString();
            const QString mapped = mapFusionType(rawType);

            if (mapped == "Sketch" || rawType.contains("Sketch", Qt::CaseInsensitive)) {
                QJsonObject sk = sketchByIndex.value(idx);
                if (sk.isEmpty()) sk = sketchByName.value(name);
                if (!sk.isEmpty()) appendSketch(sk);
                continue;
            }

            QJsonObject srcFeat = featByIndex.value(idx);
            if (srcFeat.isEmpty()) srcFeat = featByName.value(name);
            if (srcFeat.isEmpty()) {
                srcFeat = QJsonObject{{"name", name.isEmpty() ? mapped : name},
                                      {"type", rawType},
                                      {"timelineIndex", idx},
                                      {"isSuppressed", entry["isSuppressed"].toBool(false)}};
            }
            const bool hasSnap = entry.contains("stepData") || entry["stepEmpty"].toBool(false);
            if (mapped.isEmpty() && !hasSnap) continue;
            QString sketchId = lastSketchId;
            const QString prof = srcFeat["profileSketch"].toString();
            if (!prof.isEmpty() && sketchIdByName.contains(prof))
                sketchId = sketchIdByName.value(prof);
            // Solid ops: editable timeline props; BRep from the per-step snapshot (avoids red errors from unmapped face/edge refs).
            QJsonObject feat = fusionSolidFeature(mapped.isEmpty() ? QStringLiteral("ImportCAD") : mapped,
                                                  srcFeat, sketchId, true);
            if (hasSnap) {
                QJsonObject props = feat["props"].toObject();
                if (entry["stepEmpty"].toBool(false)) {
                    props["snapshotEmpty"] = true;
                } else {
                    // Keep exporter STEP as-is — decode only the step needed for the current marker
                    const QString b64 = entry["stepData"].toString();
                    if (!b64.isEmpty()) {
                        props["snapshotStepB64"] = b64;
                        ++snapshotCount;
                    }
                    if (entry.contains("stepBodies")) props["snapshotBodies"] = entry["stepBodies"];
                }
                feat["props"] = props;
            }
            tlFeatures.append(feat);
            if (idx >= 0) usedFeatIdx.insert(idx);
        }
    }

    // Any sketches / features not referenced by the timeline list
    for (auto it = sketchByName.begin(); it != sketchByName.end(); ++it) {
        const int ti = it.value()["timelineIndex"].toInt(-1);
        if (ti >= 0 && usedSketchIdx.contains(ti)) continue;
        if (sketchIdByName.contains(it.key())) continue;
        appendSketch(it.value());
    }
    for (auto it = featByName.begin(); it != featByName.end(); ++it) {
        const int ti = it.value()["timelineIndex"].toInt(-1);
        if (ti >= 0 && usedFeatIdx.contains(ti)) continue;
        const QString mapped = mapFusionType(it.value()["type"].toString());
        if (mapped.isEmpty() || mapped == "Sketch") continue;
        QString sketchId = lastSketchId;
        const QString prof = it.value()["profileSketch"].toString();
        if (!prof.isEmpty() && sketchIdByName.contains(prof))
            sketchId = sketchIdByName.value(prof);
        tlFeatures.append(fusionSolidFeature(mapped, it.value(), sketchId, true));
    }

    // Fusion mesh bodies (not in STEP): one Insert Mesh each, after the B-rep snapshots so those don't
    // replace them. nodes = float32 x y z (mm), triangles = int32 node indices, both little-endian base64.
    int meshCount = 0;
    for (const QJsonValue &mv : src["meshBodies"].toArray()) {
        const QJsonObject mb = mv.toObject();
        const QByteArray nodes = QByteArray::fromBase64(mb["nodes"].toString().toLatin1());
        const QByteArray tris = QByteArray::fromBase64(mb["triangles"].toString().toLatin1());
        MeshUtils::Mesh m;
        const qsizetype nodeCount = nodes.size() / 12;
        m.nodes.reserve(size_t(nodeCount));
        for (qsizetype i = 0; i < nodeCount; ++i) {
            const char *p = nodes.constData() + i * 12;
            m.nodes.emplace_back(qFromLittleEndian<float>(p), qFromLittleEndian<float>(p + 4),
                                 qFromLittleEndian<float>(p + 8));
        }
        const qsizetype triCount = tris.size() / 12;
        m.tris.reserve(size_t(triCount));
        for (qsizetype i = 0; i < triCount; ++i) {
            const char *p = tris.constData() + i * 12;
            const std::array<int, 3> t{qFromLittleEndian<qint32>(p), qFromLittleEndian<qint32>(p + 4),
                                       qFromLittleEndian<qint32>(p + 8)};
            if (t[0] < 0 || t[1] < 0 || t[2] < 0 || t[0] >= nodeCount || t[1] >= nodeCount || t[2] >= nodeCount)
                continue;
            m.tris.push_back(t);
        }
        if (m.empty()) continue;
        const QString meshName = mb["name"].toString().isEmpty() ? QStringLiteral("Mesh body") : mb["name"].toString();
        QJsonObject meshFeat;
        meshFeat["id"] = newFeatId();
        meshFeat["type"] = QStringLiteral("InsertMesh");
        meshFeat["name"] = meshName;
        meshFeat["suppressed"] = false;
        meshFeat["props"] = QJsonObject{{"data", QString::fromLatin1(MeshUtils::encode(m))},
                                        {"fileName", QStringLiteral("Fusion mesh body")},
                                        {"unit", QStringLiteral("mm")},
                                        {"bodyName", meshName},
                                        {"replaceFusionCopy", true},
                                        {"fusionComponent", mb["component"].toString()}};
        tlFeatures.append(meshFeat);
        ++meshCount;
    }

    QJsonObject tlObj;
    tlObj["features"] = tlFeatures;
    tlObj["marker"] = tlFeatures.size();
    dst["timeline"] = tlObj;
    // Do NOT copy fusionTimeline/fusionFeatures — they duplicate ~100MB+ of stepData and OOM on open

    QJsonArray nativeBodies;
    for (const QJsonValue &bv : src["bodies"].toArray()) {
        const QJsonObject b = bv.toObject();
        QJsonObject nativeBody;
        nativeBody["name"] = b["name"].toString();
        nativeBody["isSolid"] = b["isSolid"].toBool(true);
        if (b.contains("stepData"))
            nativeBody["stepData"] = b["stepData"];
        nativeBodies.append(nativeBody);
    }
    dst["bodies"] = nativeBodies;

    if (src.contains("thumbnail"))
        dst["thumbnail"] = src["thumbnail"];

    QJsonObject info;
    const QJsonObject meta = src["metadata"].toObject();
    info["exportedFrom"] = meta["exportedFrom"].toString(QStringLiteral("Fusion 360"));
    info["exportedAt"] = meta["exportedAt"].toString();
    info["exporterVersion"] = meta["exporterVersion"].toString();
    info["fusionTimelineCount"] = tlArr.size();
    info["importedFeatureCount"] = tlFeatures.size();
    info["snapshotCount"] = snapshotCount;
    info["hasSnapshots"] = snapshotCount > 0;
    info["meshBodyCount"] = meshCount;
    info["note"] = snapshotCount > 0
        ? QStringLiteral("Full Fusion timeline imported with editable feature props. "
                         "Each solid step carries the Fusion BRep after that step, so scrubbing shows the model history.")
        : QStringLiteral("Full Fusion timeline imported with editable feature props. "
                         "Sketches rebuild for display; solid BRep comes from the embedded STEP.");
    dst["info"] = info;
    return dst;
}

void FissionDocument::setThumbnail(const QByteArray &png) {
    if (m_thumbnail == png) return;
    m_thumbnail = png;
    emit thumbnailChanged();
}

bool FissionDocument::load(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        m_lastError = file.errorString();
        return false;
    }
    QJsonParseError perr;
    QJsonDocument doc;
    QJsonObject root;
    QByteArray thumbB64;
    try {
        const QByteArray bytes = file.readAll();
        file.close();
        doc = QJsonDocument::fromJson(bytes, &perr);
        if (doc.isNull()) {
            m_lastError = perr.errorString();
            return false;
        }
        root = doc.object();
        thumbB64 = root.value(QStringLiteral("thumbnail")).toString().toLatin1();
        // Fusion bulk exporter writes document/metadata (often also format:Fission360)
        if (root.contains(QStringLiteral("document")) || root.contains(QStringLiteral("metadata"))) {
            root = convertFromFusionExport(root);
            doc = QJsonDocument();   // drop the raw export tree before rebuild (frees duplicated stepData)
        } else if (root.value(QStringLiteral("format")).toString() != QStringLiteral("Fission360")) {
            m_lastError = QStringLiteral("Not a Fission 360 document");
            return false;
        }
        fromJson(root);
    } catch (const std::bad_alloc &) {
        m_lastError = QStringLiteral("Out of memory opening this design — try a machine with more RAM, or re-export without per-step BRep snapshots");
        clearContent();
        return false;
    } catch (const std::exception &e) {
        m_lastError = QString::fromUtf8(e.what());
        clearContent();
        return false;
    }
    // One STEP blob → one healthy ImportCAD (avoid importing the same STEP on every body)
    bool importedStep = root["info"].toObject()["hasSnapshots"].toBool(false);
    for (const QJsonValue &bv : root["bodies"].toArray()) {
        if (importedStep) break;
        const QJsonObject b = bv.toObject();
        if (!b.contains("stepData")) continue;
        const QByteArray stepRaw = QByteArray::fromBase64(b["stepData"].toString().toLatin1());
        if (stepRaw.isEmpty()) continue;
        importStepFallback(stepRaw, b["name"].toString().isEmpty() ? name() : b["name"].toString());
        importedStep = true;
        break;
    }
    Q_UNUSED(importedStep);
    QByteArray thumb = QByteArray::fromBase64(root["thumbnail"].toString().toLatin1());
    if (thumb.isEmpty())
        thumb = QByteArray::fromBase64(thumbB64);
    setThumbnail(thumb);
    m_filePath = path;
    setName(QFileInfo(path).completeBaseName());
    emit nameChanged();
    m_undo.clear();
    m_redo.clear();
    emit undoStackChanged();
    setModified(false);
    return true;
}

void FissionDocument::importStepFallback(const QByteArray &stepData, const QString &name) {
    // Write STEP data to temp file and import via the existing CAD import pipeline
    const QString tmpPath = QDir::tempPath() + QStringLiteral("/_f360_step_") + name + QStringLiteral(".stp");
    QFile tmpFile(tmpPath);
    if (!tmpFile.open(QIODevice::WriteOnly)) return;
    tmpFile.write(stepData);
    tmpFile.close();
    QString format, err;
    QVariantList parts;
    const TopoDS_Shape shape = FissionImportCAD::readFile(tmpPath, format, err, &parts);
    QFile::remove(tmpPath);
    if (shape.IsNull()) { m_lastError = err; return; }
    // Bodies keep their Fusion names (the solids' own names) rather than the "Design/Component" path
    const QStringList fusionNames = FissionImportCAD::fusionBodyNames(parts);
    for (int i = 0; i < parts.size(); ++i) {
        if (fusionNames.value(i).isEmpty()) continue;
        QVariantMap p = parts[i].toMap();
        p["name"] = fusionNames[i];
        parts[i] = p;
    }
    const int before = bodyCount();
    FissionFeature *f = addFeature("ImportCAD", {{"data", FissionImportCAD::encode(shape)},
                                                 {"fileName", name + ".stp"}, {"format", format}, {"parts", parts}},
                                   name);
    if (!f) return;
    for (int i = 0; i < parts.size() && before + i < bodyCount(); ++i) {
        const QColor c(parts[i].toMap().value("color").toString());
        if (c.isValid()) body(before + i)->setColor(c);
    }
}

// --- Export ----------------------------------------------------------------

static bool writeObj(const TopoDS_Shape &shape, const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) return false;
    QTextStream out(&file);
    out << "# Fission 360 OBJ export\n";
    int base = 1;
    for (TopExp_Explorer exp(shape, TopAbs_FACE); exp.More(); exp.Next()) {
        const TopoDS_Face face = TopoDS::Face(exp.Current());
        TopLoc_Location loc;
        Handle(Poly_Triangulation) tri = BRep_Tool::Triangulation(face, loc);
        if (tri.IsNull()) continue;
        const gp_Trsf t = loc.Transformation();
        for (int i = 1; i <= tri->NbNodes(); ++i) {
            const gp_Pnt p = tri->Node(i).Transformed(t);
            out << "v " << p.X() << ' ' << p.Y() << ' ' << p.Z() << '\n';
        }
        const bool reversed = face.Orientation() == TopAbs_REVERSED;
        for (int i = 1; i <= tri->NbTriangles(); ++i) {
            int a, b, c;
            tri->Triangle(i).Get(a, b, c);
            if (reversed) std::swap(b, c);
            out << "f " << base + a - 1 << ' ' << base + b - 1 << ' ' << base + c - 1 << '\n';
        }
        base += tri->NbNodes();
    }
    return true;
}

QString FissionDocument::exportFile(const QString &path, const QString &format) {
    QVariantList visible;
    for (int i = 0; i < m_bodies.size(); ++i)
        if (m_bodies[i]->isVisible()) visible.append(i);
    if (visible.isEmpty()) { m_lastError = "There are no visible bodies to export"; return QString(); }
    return exportBodies(path, visible, format);
}

QString FissionDocument::exportBodies(const QString &path, const QVariantList &bodies, const QString &format) {
    QString fmt = format.isEmpty() ? QFileInfo(path).suffix().toLower() : format.toLower();
    if (fmt == "stp") fmt = "step";
    if (fmt == "igs") fmt = "iges";

    QList<TopoDS_Shape> shapes;
    QList<FissionBody *> picked;
    for (const QVariant &v : bodies) {
        const int i = v.toInt();
        if (i >= 0 && i < m_bodies.size() && !m_bodies[i]->shape().IsNull()) {
            shapes.append(m_bodies[i]->shape());
            picked.append(m_bodies[i]);
        }
    }
    if (shapes.isEmpty()) { m_lastError = "There are no bodies to export"; return QString(); }
    const TopoDS_Shape all = GeomUtils::makeCompound(shapes);
    const std::string file = QFile::encodeName(path).toStdString();

    if (fmt == "3mf") {
        const double size = std::sqrt(GeomUtils::boundingBox(all).SquareExtent());
        QList<FileExport::MeshPart> parts;
        for (FissionBody *b : picked) {
            FileExport::MeshPart p{b->name(), b->color(), MeshUtils::fromShape(b->shape(), std::max(0.005, size * 0.0005))};
            if (!p.mesh.empty()) parts.append(p);
        }
        QString err;
        if (!FileExport::write3mf(path, parts, err)) { m_lastError = err; return QString(); }
        return path;
    }

    bool ok = false;
    if ((fmt == "step" || fmt == "iges") && std::any_of(shapes.begin(), shapes.end(), MeshUtils::isMesh)) {
        shapes.erase(std::remove_if(shapes.begin(), shapes.end(), MeshUtils::isMesh), shapes.end());
        if (shapes.isEmpty()) { m_lastError = "Mesh bodies export as STL or OBJ only"; return QString(); }
    }
    if (fmt == "stl" || fmt == "obj") {
        const double size = std::sqrt(GeomUtils::boundingBox(all).SquareExtent());
        for (const auto &s : shapes)
            if (!MeshUtils::isMesh(s))
                BRepMesh_IncrementalMesh(s, std::max(0.005, size * 0.0005), Standard_False, 0.2, Standard_True);
        if (fmt == "stl") {
            StlAPI_Writer writer;
            writer.ASCIIMode() = Standard_False;
            ok = writer.Write(all, file.c_str());
        } else {
            ok = writeObj(all, path);
        }
    } else if (fmt == "step") {
        STEPControl_Writer writer;
        for (const auto &s : shapes) writer.Transfer(s, STEPControl_AsIs);
        ok = writer.Write(file.c_str()) == IFSelect_RetDone;
    } else if (fmt == "iges") {
        IGESControl_Controller::Init();
        IGESControl_Writer writer("MM", 0);
        for (const auto &s : shapes) writer.AddShape(s);
        writer.ComputeModel();
        ok = writer.Write(file.c_str());
    } else {
        m_lastError = QString("Unsupported export format '%1'").arg(fmt);
        return QString();
    }
    if (!ok) { m_lastError = QString("Failed to write %1").arg(path); return QString(); }
    return path;
}

QString FissionDocument::exportFaceDxf(const QVariantMap &faceRef, const QString &path) {
    const int bi = faceRef.value("body", -1).toInt();
    FissionBody *b = body(bi);
    if (!b || b->shape().IsNull() || faceRef.value("kind").toString() != "face") { m_lastError = "Pick a planar face"; return QString(); }
    const TopTools_IndexedMapOfShape faces = GeomUtils::faceMap(b->shape());
    const int fi = faceRef.value("index", -1).toInt() + 1;
    if (fi < 1 || fi > faces.Extent()) { m_lastError = "The face no longer exists"; return QString(); }
    const TopoDS_Face face = TopoDS::Face(faces(fi));
    BRepAdaptor_Surface surf(face, Standard_False);
    if (surf.GetType() != GeomAbs_Plane || MeshUtils::isMesh(face)) { m_lastError = "Only planar faces export as DXF"; return QString(); }
    // Drawn as seen from outside the body: the frame's Z is the face's outward normal
    const gp_Ax3 pl = surf.Plane().Position();
    gp_Dir n = pl.Direction();
    if (face.Orientation() == TopAbs_REVERSED) n.Reverse();
    FileExport::DxfWriter dxf(gp_Ax3(pl.Location(), n, pl.XDirection()));
    QString err;
    if (!dxf.open(path, {{"0", 7}}, &err)) { m_lastError = err; return QString(); }
    for (TopExp_Explorer e(face, TopAbs_EDGE); e.More(); e.Next())
        if (!BRep_Tool::Degenerated(TopoDS::Edge(e.Current()))) dxf.addEdge(TopoDS::Edge(e.Current()), "0");
    if (!dxf.close(&err)) { m_lastError = err; return QString(); }
    return path;
}

QVariantList FissionDocument::exportBatch(const QVariantList &bodies, const QString &folder, const QStringList &formats,
                                          bool separate, const QString &baseName) {
    QVariantList written;
    QStringList errors;
    QDir dir(folder);
    if (!dir.exists() && !dir.mkpath(".")) { m_lastError = "Cannot create the folder " + folder; return written; }
    // Body names can hold assembly paths ("Asm/Part"); keep file names to safe characters
    auto safe = [](QString s) {
        s.replace(QRegularExpression(R"([\\/:*?"<>|]+)"), "_");
        s = s.trimmed();
        return s.isEmpty() ? QStringLiteral("body") : s;
    };
    QList<QPair<QString, QVariantList>> jobs;
    if (separate) {
        QSet<QString> used;
        for (const QVariant &v : bodies) {
            FissionBody *b = body(v.toInt());
            if (!b) continue;
            QString name = safe(b->name());
            for (int k = 2; used.contains(name.toLower()); ++k) name = safe(b->name()) + QString(" %1").arg(k);
            used.insert(name.toLower());
            jobs.append({name, {v}});
        }
    } else {
        jobs.append({safe(baseName.isEmpty() ? QStringLiteral("design") : baseName), bodies});
    }
    for (const QString &f : formats) {
        const QString fmt = f.toLower();
        for (const auto &[name, list] : jobs) {
            const QString path = dir.filePath(name + "." + fmt);
            if (exportBodies(path, list, fmt).isEmpty()) errors << name + "." + fmt + ": " + m_lastError;
            else written << path;
        }
    }
    m_lastError = errors.join("\n");
    return written;
}

QList<QObject *> FissionDocument::bodyObjects() const {
    QList<QObject *> out;
    for (auto *b : m_bodies) out.append(b);
    return out;
}

QList<QObject *> FissionDocument::parameterObjects() const {
    QList<QObject *> out;
    for (auto *p : m_parameters) out.append(p);
    return out;
}
