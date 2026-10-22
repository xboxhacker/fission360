// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#pragma once
#include <QObject>
#include <QString>
#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QHash>
#include <QTimer>
#include <QVariantMap>
#include "FissionTimeline.h"
#include "FissionParameter.h"
#include "FissionBody.h"
#include "FissionFeature.h"
#include "render/RenderSettings.h"

class FissionDocument : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)
    Q_PROPERTY(QString filePath READ filePath NOTIFY nameChanged)
    Q_PROPERTY(bool modified READ isModified NOTIFY modifiedChanged)
    // Stable id for linking drawings / renders across renames; assigned once and kept in the file
    Q_PROPERTY(QString documentId READ documentId CONSTANT)
    // Monotonic content stamp — drawings & renders compare against this to know if they are out of date
    Q_PROPERTY(qint64 contentRevision READ contentRevision NOTIFY contentRevisionChanged)
    // Named save version (1, 2, …) — bumped on each explicit Save; snapshots live in the .f360 "history" array
    Q_PROPERTY(int versionNumber READ versionNumber NOTIFY versionNumberChanged)
    // Linked outputs: [{kind:"drawing"|"render", id, name, path, sourceRevision, outOfDate}]
    Q_PROPERTY(QVariantList linkedAssets READ linkedAssets NOTIFY linkedAssetsChanged)
    Q_PROPERTY(FissionTimeline *timeline READ timeline CONSTANT)
    Q_PROPERTY(QList<QObject *> bodies READ bodyObjects NOTIFY bodiesChanged)
    Q_PROPERTY(QList<QObject *> parameters READ parameterObjects NOTIFY parametersChanged)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY undoStackChanged)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY undoStackChanged)
    Q_PROPERTY(double density READ density WRITE setDensity NOTIFY densityChanged)
    // Document units. Geometry and evaluated expressions stay in mm / degrees; the units decide how
    // lengths are shown and what a bare number typed into a length input means ("10" -> "10 in").
    Q_PROPERTY(QString lengthUnit READ lengthUnit WRITE setLengthUnit NOTIFY unitsChanged)
    Q_PROPERTY(QString massUnit READ massUnit WRITE setMassUnit NOTIFY unitsChanged)
    // Free-form document properties: description, partNumber, designer
    Q_PROPERTY(QVariantMap info READ info NOTIFY infoChanged)
    // While set, the viewport shows and picks the bodies as they were just before this feature
    // (so edges/faces it consumes can still be clicked); its result is drawn as a preview outline.
    Q_PROPERTY(QString inputFeatureId READ inputFeatureId WRITE setInputFeatureId NOTIFY inputFeatureChanged)
    // Saved analyses shown in the browser's Analysis folder:
    // [{id, name, type: "section", plane: ref, offset (mm), flip, visible}]
    Q_PROPERTY(QVariantList analyses READ analyses NOTIFY analysesChanged)
    Q_PROPERTY(RenderSettings *renderSettings READ renderSettings CONSTANT)
    // Phase 12 manufacture data (setups / tools / operations) — light CAM scaffolding, not a full CAM kernel
    Q_PROPERTY(QVariantMap manufacture READ manufacture NOTIFY manufactureChanged)

public:
    explicit FissionDocument(QObject *parent = nullptr);
    ~FissionDocument() override;

    QString name() const { return m_name; }
    void setName(const QString &name);
    QString filePath() const { return m_filePath; }
    bool isModified() const { return m_modified; }
    QString documentId() const { return m_documentId; }
    qint64 contentRevision() const { return m_contentRevision; }
    int versionNumber() const { return m_versionNumber; }
    QVariantList linkedAssets() const;
    // Register / refresh a linked drawing or render capture against the current contentRevision
    Q_INVOKABLE void linkAsset(const QString &kind, const QString &id, const QString &name,
                               const QString &path = QString());
    Q_INVOKABLE void unlinkAsset(const QString &kind, const QString &id);
    Q_INVOKABLE void markLinkedAssetsStale();
    Q_INVOKABLE bool anyLinkedAssetOutOfDate() const;
    // Explicit save already bumps version; these expose history / revert / crash recovery
    Q_INVOKABLE QVariantList versionHistory() const;
    Q_INVOKABLE bool revertToVersion(int versionNumber);
    Q_INVOKABLE bool writeAutosaveNow();
    Q_INVOKABLE static QVariantList pendingAutosaves();
    Q_INVOKABLE bool loadAutosave(const QString &documentId);
    FissionTimeline *timeline() const { return m_timeline; }
    double density() const { return m_density; }   // g/cm^3
    void setDensity(double d);

    // --- Document settings ---
    QString lengthUnit() const { return m_lengthUnit; }
    void setLengthUnit(const QString &unit);   // mm, cm, m, in, ft
    QString massUnit() const { return m_massUnit; }
    void setMassUnit(const QString &unit);     // g, kg, lb, oz
    Q_INVOKABLE QStringList lengthUnits() const;
    Q_INVOKABLE QStringList massUnits() const;
    Q_INVOKABLE double lengthScale() const;    // mm per document length unit
    // mm (power 1), mm^2 (2) or mm^3 (3) in document units with the unit suffix, e.g. "0.394 in"
    Q_INVOKABLE QString formatLength(double mm, int decimals = 3, int power = 1) const;
    Q_INVOKABLE QString formatMass(double grams, int decimals = 3) const;
    // Appends the default unit to an input without units or names: kind "length" -> document unit,
    // "angle" -> deg, anything else unchanged. "10" -> "10 in", "10 / 2" -> "(10 / 2) in".
    Q_INVOKABLE QString withUnit(const QString &text, const QString &kind) const;
    QVariantMap info() const { return m_info; }
    Q_INVOKABLE void setInfo(const QString &key, const QString &value);

    // --- Features ---
    // Inserted at the timeline marker. props override the feature's defaults.
    Q_INVOKABLE FissionFeature *addFeature(const QString &type, const QVariantMap &props = {},
                                           const QString &name = QString());
    Q_INVOKABLE void removeFeature(FissionFeature *feature);
    Q_INVOKABLE FissionFeature *featureById(const QString &id) const;
    // [{id, name, type}] of active features, optionally filtered by type ("Sketch", "ConstructionPlane", ...)
    Q_INVOKABLE QVariantList featureList(const QString &type = QString()) const;
    Q_INVOKABLE void moveFeature(int from, int to);

    // --- Parameters ---
    Q_INVOKABLE FissionParameter *addParameter(const QString &name, const QString &expression,
                                               const QString &unit = QStringLiteral("mm"),
                                               const QString &comment = QString());
    Q_INVOKABLE bool removeParameter(const QString &name);
    Q_INVOKABLE FissionParameter *parameter(const QString &name) const;
    // Creates the sheet metal rule parameters (SheetThickness, BendRadius, KFactor) that are missing
    Q_INVOKABLE void ensureSheetMetalRule();
    QList<FissionParameter *> parameters() const { return m_parameters; }
    // {value, error} for an expression using current parameter values
    Q_INVOKABLE QVariantMap evaluateExpression(const QString &expr) const;
    double evaluate(const QString &expr, QString *error) const;

    // --- Rebuild ---
    Q_INVOKABLE void rebuild();
    Q_INVOKABLE void scheduleRebuild();
    int rebuildCount() const { return m_rebuildCount; }

    // --- Results ---
    QList<FissionBody *> bodies() const { return m_bodies; }
    Q_INVOKABLE FissionBody *body(int index) const { return m_bodies.value(index, nullptr); }
    Q_INVOKABLE int bodyCount() const { return m_bodies.size(); }
    Q_INVOKABLE QVariantMap bodyProperties(int index) const;
    // [xmin, ymin, zmin, xmax, ymax, zmax] of a body (empty when there is none)
    Q_INVOKABLE QVariantList bodyBox(int index) const;
    // Active, visible sketches and construction geometry
    QList<FissionFeature *> overlayFeatures() const;

    QString inputFeatureId() const { return m_inputFeatureId; }
    void setInputFeatureId(const QString &id);
    // True when the last rebuild reached inputFeatureId; inputShapes() are the bodies before it
    bool showingInput() const { return m_hasInput; }
    const QList<TopoDS_Shape> &inputShapes() const { return m_inputShapes; }
    // Shape drawn / picked for body index i: the input snapshot while showingInput(), else the result
    TopoDS_Shape displayShape(int i) const;
    int displayBodyCount() const { return m_hasInput ? m_inputShapes.size() : m_bodies.size(); }
    // Edge refs of the tangent chain through an edge ref of a displayed body (the ref itself first)
    Q_INVOKABLE QVariantList tangentChain(const QVariantMap &edgeRef) const;
    // Edge refs of every edge bounding a face (Fillet / Chamfer on a picked face)
    Q_INVOKABLE QVariantList faceEdges(const QVariantMap &faceRef) const;
    Q_INVOKABLE bool isPlanarFace(const QVariantMap &faceRef) const;
    // {ok, origin: [x, y, z], normal: [x, y, z]} of "XY" / "XZ" / "YZ", an originplane, plane or planar face ref
    Q_INVOKABLE QVariantMap planeFrame(const QVariant &ref) const;

    // --- Analyses ---
    QVariantList analyses() const { return m_analyses; }
    // Adds {type, plane, offset, flip, ...}; returns the new id. A visible section hides the other sections.
    Q_INVOKABLE QString addAnalysis(const QVariantMap &analysis);
    Q_INVOKABLE void updateAnalysis(const QString &id, const QVariantMap &changes);
    Q_INVOKABLE void removeAnalysis(const QString &id);
    Q_INVOKABLE QVariantMap analysis(const QString &id) const;

    // --- Inspect (FissionInspect.cpp) ---
    // Measures one or two selection refs (face, edge, body, point, originpoint).
    // {ok, error, lines: [[label, text]..], distance (mm), angle (deg, when both have a direction)}
    Q_INVOKABLE QVariantMap measure(const QVariantList &refs) const;
    // Overlapping pairs among the given body indices (empty = all): [{a, b, volume}]
    Q_INVOKABLE QVariantList interference(const QVariantList &bodies) const;
    // Reads an STL / OBJ file and adds an Insert Mesh feature holding its triangles; "" or the feature id
    Q_INVOKABLE QString insertMesh(const QString &path, const QString &unit = QStringLiteral("mm"));
    // Reads a STEP / IGES file and adds an Import feature holding its B-rep (FissionImport.cpp); "" or the id
    Q_INVOKABLE QString importCAD(const QString &path);
    // Same work off the UI thread; emits importCADFinished(featureId, error, firstBody, bodyCount)
    Q_INVOKABLE void importCADAsync(const QString &path);
    Q_INVOKABLE bool importCADBusy() const { return m_importCADBusy; }
    // --- Linked designs (FissionLinkedDesign.cpp) ---
    // Inserts the bodies of another saved .f360 design as an Import feature that remembers the source
    // (path, documentId, contentRevision). "" on failure (lastError), otherwise the feature id.
    Q_INVOKABLE QString insertLinkedDesign(const QString &path);
    // [{featureId, name, path, revision, sourceRevision, outOfDate, missing}] for every still-linked insert
    Q_INVOKABLE QVariantList linkedDesigns() const;
    // Re-reads the source design into the insert; false on failure (lastError)
    Q_INVOKABLE bool updateLinkedDesign(const QString &featureId);
    // Keeps the geometry but forgets the source: the insert is never checked for updates again
    Q_INVOKABLE void unlinkDesign(const QString &featureId);
    // {documentId, contentRevision, name} read from the head of a saved design (cached by file time)
    Q_INVOKABLE static QVariantMap designFileInfo(const QString &path);
    // Standard thread sizes for the Hole / Thread size lists (FissionHole.cpp):
    // [{name, major, pitch, tapDrill, close, normal, loose}] in mm; standard "isoCoarse" | "isoFine" | "unc" | "unf"
    Q_INVOKABLE QVariantList threadSizes(const QString &standard) const;

    // --- Undo ---
    Q_INVOKABLE void checkpoint();
    Q_INVOKABLE void undo();
    Q_INVOKABLE void redo();
    bool canUndo() const { return !m_undo.isEmpty(); }
    bool canRedo() const { return !m_redo.isEmpty(); }

    // --- Files ---
    QJsonObject toJson() const;
    void fromJson(const QJsonObject &obj);
    Q_INVOKABLE bool save(const QString &path);
    Q_INVOKABLE bool load(const QString &path);
    // Drawing bodies for the live .f360. Version snapshots and autosave omit this array.
    QJsonArray drawingPayload() const { return m_drawings; }
    void setDrawingPayload(const QJsonArray &drawings);
    bool drawingsLive() const { return m_drawingsLive; }
    void setDrawingsLive(bool live) { m_drawingsLive = live; }
    // Copy the drawings array out of an existing .f360 (autosave recovery). Missing key leaves the payload unchanged.
    void copyDrawingPayloadFromFile(const QString &path);
    // Preview stored in the file on the next save (PNG); a loaded design keeps the one it was saved with
    Q_PROPERTY(bool hasThumbnail READ hasThumbnail NOTIFY thumbnailChanged)
    void setThumbnail(const QByteArray &png);
    QByteArray thumbnail() const { return m_thumbnail; }
    bool hasThumbnail() const { return !m_thumbnail.isEmpty(); }
    // The file was moved or renamed outside the document (project explorer)
    void setFilePath(const QString &path);
    Q_INVOKABLE QString exportFile(const QString &path, const QString &format = QString());
    // Exports the given body indices (any visibility); returns the path or "" (see lastError)
    Q_INVOKABLE QString exportBodies(const QString &path, const QVariantList &bodies, const QString &format = QString());
    // The edges of a planar face as a DXF drawing, seen from outside the body; empty on failure (lastError)
    Q_INVOKABLE QString exportFaceDxf(const QVariantMap &faceRef, const QString &path);
    // A flat pattern (or unfolded sheet) as a DXF outline with its bend lines on layer BEND; featureId ""
    // takes the last one in the timeline. Empty on failure (lastError)
    Q_INVOKABLE QString exportFlatPatternDxf(const QString &featureId, const QString &path);
    // Writes the bodies in every format to folder: one file per body (named after it) or one file for all
    // (baseName). Returns the files written; lastError lists the ones that failed.
    Q_INVOKABLE QVariantList exportBatch(const QVariantList &bodies, const QString &folder, const QStringList &formats,
                                         bool separate, const QString &baseName = QString());
    Q_INVOKABLE QString lastError() const { return m_lastError; }

    RenderSettings *renderSettings() const { return m_renderSettings; }
    // Apply a library appearance id to the given body indices (empty list = all bodies)
    Q_INVOKABLE void applyAppearance(const QString &appearanceId, const QVariantList &bodyIndices);
    // Apply to faces: each item is {body, index} (SelectionManager face refs). Falls back to bodies if empty faces.
    Q_INVOKABLE void applyAppearanceToSelection(const QString &appearanceId, const QVariantList &selectionItems);
    // Offline Monte Carlo path trace. camera: {eye, target, up, fov, ortho}. Returns local path or "" on failure.
    Q_INVOKABLE QString pathTraceToFile(const QString &path, const QVariantMap &camera);

    QVariantMap manufacture() const { return m_manufacture; }
    Q_INVOKABLE void setManufacture(const QVariantMap &m);
    // kind: "setups" | "tools" | "operations". Returns new id or "".
    Q_INVOKABLE QString addMfgItem(const QString &kind, const QVariantMap &props = {});
    Q_INVOKABLE bool updateMfgItem(const QString &kind, const QString &id, const QVariantMap &patch);
    Q_INVOKABLE bool removeMfgItem(const QString &kind, const QString &id);
    Q_INVOKABLE void setActiveMfgSetup(const QString &id);
    // Recompute stock AABB from visible bodies + setup stockOffset.
    Q_INVOKABLE bool refreshMfgStock(const QString &setupId = QString());
    // Writes an NC stub for the active setup (op-type aware stock paths — not a full CAM kernel).
    Q_INVOKABLE QString exportMfgGCode(const QString &path);
    // Human-readable simulate report for the active setup.
    Q_INVOKABLE QString simulateMfgReport() const;

    QList<QObject *> bodyObjects() const;
    QList<QObject *> parameterObjects() const;

signals:
    void nameChanged();
    void modifiedChanged();
    void bodiesChanged();
    void parametersChanged();
    void rebuilt();
    void displayChanged();   // geometry or visibility of anything drawn changed
    void undoStackChanged();
    void densityChanged();
    void unitsChanged();
    void infoChanged();
    void featureAdded(FissionFeature *feature);
    void featureRemoved(QString id);
    void inputFeatureChanged();
    void analysesChanged();
    void thumbnailChanged();
    void renderSettingsChanged();
    void contentRevisionChanged();
    void versionNumberChanged();
    void linkedAssetsChanged();
    void linkedDesignsChanged();
    void manufactureChanged();
    // featureId empty on failure; firstBody/bodyCount are new body indices after a successful import
    void importCADFinished(const QString &featureId, const QString &error, int firstBody, int bodyCount);

private:
    static QVariantMap defaultManufacture();
    void bumpContentRevision();
    void touchDrawingLink();
    QByteArray snapshotJsonForHistory() const;
    void attachFeature(FissionFeature *f);
    void attachParameter(FissionParameter *p);
    void evaluateParameters();
    void setModified(bool m);
    void restore(const QJsonObject &state);
    void clearContent();
    // Hash of the timeline, parameters, and units. The file stores the rebuilt solids under this key.
    QByteArray geometryKey() const;
    QJsonObject resultCacheJson() const;
    bool restoreResultCache(const QJsonObject &cache);
    void adoptBodies(const QList<BodyState> &bodies);
    void realizeLightweightFeatures();
    // Replay only when the solids on screen are not the ones at the current marker.
    void syncModelToMarker();
    QString uniqueFeatureName(const QString &type) const;
    // Fusion bulk-export STEP blob → temp file → importCAD (bodies with stepData)
    void importStepFallback(const QByteArray &stepData, const QString &name);

    QString m_name = QStringLiteral("Untitled");
    QString m_filePath;
    QString m_documentId;
    qint64 m_contentRevision = 1;
    int m_versionNumber = 0;
    QVariantList m_linkedAssets;   // see linkAsset()
    QJsonArray m_drawings;         // live .f360 drawing bodies; omitted from version snapshots
    bool m_drawingsLive = false;   // manager has the drawings for this design, so Save may replace m_drawings
    QByteArray m_thumbnail;
    bool m_modified = false;
    double m_density = 7.85;
    QString m_lengthUnit = QStringLiteral("mm");
    QString m_massUnit = QStringLiteral("g");
    QVariantMap m_info;
    FissionTimeline *m_timeline;
    QList<FissionParameter *> m_parameters;
    QHash<QString, double> m_paramValues;
    QList<FissionBody *> m_bodies;
    RenderSettings *m_renderSettings = nullptr;
    QString m_inputFeatureId;
    QList<TopoDS_Shape> m_inputShapes;
    bool m_hasInput = false;
    QTimer m_rebuildTimer;
    // The model after each timeline step of the last rebuild; steps before the first changed feature are reused
    struct RebuildStep { FissionFeature *feature; quint64 revision; bool suppressed; RebuildContext after; };
    QList<RebuildStep> m_steps;
    QString m_stepsKey;   // parameter values and units the steps were computed with
    // Decoded model at a marker, so scrubbing back to a visited step does not replay it.
    struct MarkerSolids { QString sig; QList<BodyState> bodies; };
    QHash<int, MarkerSolids> m_atMarker;
    QList<int> m_atMarkerOrder;
    bool m_restoring = false;
    bool m_loading = false;
    bool m_resultFromCache = false;   // solids are the saved marker state, so Save must not replay them
    int m_rebuildCount = 0;
    QVariantList m_analyses;
    QVariantMap m_manufacture;
    QJsonArray m_history;   // embedded version snapshots inside the .f360 file
    QList<QJsonObject> m_undo, m_redo;
    QString m_lastError;
    bool m_importCADBusy = false;
};
