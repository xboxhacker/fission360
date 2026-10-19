// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#pragma once
#include "FissionFeature.h"
#include "MeshUtils.h"

#include <QHash>
#include <QSet>
#include <functional>

// MESH workspace features. Mesh bodies are faces holding only triangles (see MeshUtils).

// props: data (MeshUtils::encode), fileName, unit, bodyName (optional; Fusion mesh bodies keep their name),
// replaceFusionCopy + fusionComponent (Fusion import: removes the faceted B-rep copy of the same body)
class FissionInsertMesh : public FissionFeature {
    Q_OBJECT
public:
    explicit FissionInsertMesh(QObject *parent = nullptr) : FissionFeature(parent) {}
    QString type() const override { return QStringLiteral("InsertMesh"); }
    QVariantMap defaults() const override { return {{"data", ""}, {"fileName", ""}, {"unit", "mm"}}; }
protected:
    bool compute(RebuildContext &ctx) override;
};

// Slow mesh work (fitting, face groups) for a feature: meshes with more triangles than backgroundTriangles
// are processed on a worker thread while the window stays usable, with progress in the status bar; until
// the result is in, the feature reports that it is still working. Results are cached across rebuilds by a
// key (mesh hash + settings), since fitting a large mesh takes seconds.
class MeshBackgroundJob {
public:
    struct Result {
        TopoDS_Shape shape;
        QString summary;   // or the error when shape is null
        MeshUtils::Mesh mesh;   // for jobs whose result is a mesh (face groups)
    };
    using Work = std::function<Result(const MeshUtils::Progress &)>;
    // Returns true with the result when it is available (computed now, or cached from an earlier rebuild);
    // false with err set when the work was started (or is still running) in the background, or failed
    bool run(FissionFeature *owner, const QByteArray &key, size_t triangles, const QString &label, const Work &work,
             Result &result, QString &err);
    static int backgroundTriangles;

private:
    QHash<QByteArray, Result> m_cache;
    QSet<QByteArray> m_running;
};

// Runs one operation on each selected body (empty = every body the operation applies to)
class FissionMeshOperation : public FissionFeature {
    Q_OBJECT
public:
    using FissionFeature::FissionFeature;
protected:
    bool compute(RebuildContext &ctx) override;
    // true when this operation takes mesh bodies (false: B-rep bodies)
    virtual bool wantsMesh() const { return true; }
    // Replaces the body's shape; returns false with err set on failure
    virtual bool apply(RebuildContext &ctx, TopoDS_Shape &shape, QString &err) = 0;
};

// props: bodies, direction "toMesh" | "toBRep", refinement "coarse" | "medium" | "fine",
// method "prismatic" (fitted planes, cylinders, cones, spheres) | "faceted" (merged flat facets)
class FissionMeshConvert : public FissionMeshOperation {
    Q_OBJECT
    // What the last rebuild made of each body, e.g. "9 faces (7 planes, 2 cylinders)"
    Q_PROPERTY(QString summary READ summary NOTIFY statusChanged)
public:
    using FissionMeshOperation::FissionMeshOperation;
    QString type() const override { return QStringLiteral("MeshConvert"); }
    QVariantMap defaults() const override {
        return {{"direction", "toBRep"}, {"refinement", "medium"}, {"method", "prismatic"}};
    }
    QString summary() const { return m_summary.join("; "); }
    // Meshes with more triangles than this are converted (and grouped) on a worker thread while the window
    // stays usable; until the result is in, the feature reports that it is still working
    static void setBackgroundTriangles(int n) { MeshBackgroundJob::backgroundTriangles = n; }
protected:
    bool wantsMesh() const override { return str("direction", "toBRep") == "toBRep"; }
    bool apply(RebuildContext &ctx, TopoDS_Shape &shape, QString &err) override;
    void resetResult() override { m_summary.clear(); }
private:
    QStringList m_summary;
    MeshBackgroundJob m_job;
};

class FissionMeshReverse : public FissionMeshOperation {
    Q_OBJECT
public:
    using FissionMeshOperation::FissionMeshOperation;
    QString type() const override { return QStringLiteral("MeshReverse"); }
    QVariantMap defaults() const override { return {}; }
protected:
    bool apply(RebuildContext &ctx, TopoDS_Shape &shape, QString &err) override;
};

// props: bodies, iterations, smoothness (0..1)
class FissionMeshSmooth : public FissionMeshOperation {
    Q_OBJECT
public:
    using FissionMeshOperation::FissionMeshOperation;
    QString type() const override { return QStringLiteral("MeshSmooth"); }
    QVariantMap defaults() const override { return {{"iterations", "5"}, {"smoothness", "0.5"}}; }
    QStringList expressionKeys() const override { return {"iterations", "smoothness"}; }
protected:
    bool apply(RebuildContext &ctx, TopoDS_Shape &shape, QString &err) override;
};

// props: bodies, proportion (percent of triangles to keep)
class FissionMeshReduce : public FissionMeshOperation {
    Q_OBJECT
public:
    using FissionMeshOperation::FissionMeshOperation;
    QString type() const override { return QStringLiteral("MeshReduce"); }
    QVariantMap defaults() const override { return {{"proportion", "50"}}; }
    QStringList expressionKeys() const override { return {"proportion"}; }
protected:
    bool apply(RebuildContext &ctx, TopoDS_Shape &shape, QString &err) override;
};

// props: bodies, tolerance (weld distance), orient, fillHoles
class FissionMeshRepair : public FissionMeshOperation {
    Q_OBJECT
    // What the last rebuild fixed, e.g. "Body1: 12 nodes welded, 2 holes filled - closed"
    Q_PROPERTY(QString summary READ summary NOTIFY statusChanged)
public:
    using FissionMeshOperation::FissionMeshOperation;
    QString type() const override { return QStringLiteral("MeshRepair"); }
    QVariantMap defaults() const override {
        return {{"tolerance", "0.01 mm"}, {"orient", true}, {"fillHoles", true}};
    }
    QStringList expressionKeys() const override { return {"tolerance"}; }
    QString summary() const { return m_summary.join("; "); }
protected:
    bool apply(RebuildContext &ctx, TopoDS_Shape &shape, QString &err) override;
    void resetResult() override { m_summary.clear(); }
private:
    QStringList m_summary;
};

// props: bodies, thickness, direction "inside" | "outside"
class FissionMeshShell : public FissionMeshOperation {
    Q_OBJECT
public:
    using FissionMeshOperation::FissionMeshOperation;
    QString type() const override { return QStringLiteral("MeshShell"); }
    QVariantMap defaults() const override {
        return {{"thickness", "2 mm"}, {"direction", "inside"}};
    }
    QStringList expressionKeys() const override { return {"thickness"}; }
protected:
    bool apply(RebuildContext &ctx, TopoDS_Shape &shape, QString &err) override;
};

// props: bodies, method "accurate" (fitted surfaces) | "fast" (angle only), angle (deg), minSize (% of area)
class FissionMeshFaceGroups : public FissionMeshOperation {
    Q_OBJECT
    // "Body1: 14 face groups"
    Q_PROPERTY(QString summary READ summary NOTIFY statusChanged)
public:
    using FissionMeshOperation::FissionMeshOperation;
    QString type() const override { return QStringLiteral("MeshFaceGroups"); }
    QVariantMap defaults() const override {
        // Boundary tolerance matches Fusion Accurate: a fraction of the mesh diagonal (0..0.01), not mm.
        return {{"method", "accurate"}, {"tolerance", "0.001"}, {"angle", "20 deg"}, {"minSize", "0.1"}};
    }
    QStringList expressionKeys() const override { return {"tolerance", "angle", "minSize"}; }
    QString summary() const { return m_summary.join("; "); }
protected:
    bool apply(RebuildContext &ctx, TopoDS_Shape &shape, QString &err) override;
    void resetResult() override { m_summary.clear(); }
private:
    QStringList m_summary;
    MeshBackgroundJob m_job;
};

// props: bodies, edgeLength (0 = mean edge length), adaptive, iterations, sharpAngle (deg; sharper edges kept)
class FissionMeshRemesh : public FissionMeshOperation {
    Q_OBJECT
    Q_PROPERTY(QString summary READ summary NOTIFY statusChanged)
public:
    using FissionMeshOperation::FissionMeshOperation;
    QString type() const override { return QStringLiteral("MeshRemesh"); }
    QVariantMap defaults() const override {
        return {{"edgeLength", "1 mm"}, {"adaptive", false}, {"iterations", "3"}, {"sharpAngle", "30 deg"}};
    }
    QStringList expressionKeys() const override { return {"edgeLength", "iterations", "sharpAngle"}; }
    QString summary() const { return m_summary.join("; "); }
protected:
    bool apply(RebuildContext &ctx, TopoDS_Shape &shape, QString &err) override;
    void resetResult() override { m_summary.clear(); }
private:
    QStringList m_summary;
};

// props: bodies (two or more mesh bodies). The first keeps the triangles of all; the others are removed.
class FissionMeshCombine : public FissionFeature {
    Q_OBJECT
public:
    explicit FissionMeshCombine(QObject *parent = nullptr) : FissionFeature(parent) {}
    QString type() const override { return QStringLiteral("MeshCombine"); }
    QVariantMap defaults() const override { return {{"bodies", QVariantList()}}; }
protected:
    bool compute(RebuildContext &ctx) override;
};

// props: bodies, plane (plane ref), flip, fill. Removes what lies in front of the plane.
class FissionMeshPlaneCut : public FissionMeshOperation {
    Q_OBJECT
public:
    using FissionMeshOperation::FissionMeshOperation;
    QString type() const override { return QStringLiteral("MeshPlaneCut"); }
    // offset: the cut plane moved along its normal (dragged with the arrow in the view)
    QVariantMap defaults() const override {
        return {{"plane", "XY"}, {"offset", "0 mm"}, {"flip", false}, {"fill", true}};
    }
    QStringList expressionKeys() const override { return {"offset"}; }
protected:
    bool apply(RebuildContext &ctx, TopoDS_Shape &shape, QString &err) override;
};
