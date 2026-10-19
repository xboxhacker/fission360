// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#include "FissionMeshFeatures.h"
#include "FissionDocument.h"
#include "FissionProgress.h"
#include "GeomUtils.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QPointer>
#include <QSet>
#include <Standard_Failure.hxx>
#include <algorithm>
#include <cmath>
#include <functional>
#include <thread>

int MeshBackgroundJob::backgroundTriangles = 20000;

namespace {
// Cache key of a mesh and the settings an operation runs it with
QByteArray meshKey(const MeshUtils::Mesh &m, const QString &settings) {
    std::vector<double> coords;
    coords.reserve(m.nodes.size() * 3);
    for (const gp_Pnt &p : m.nodes) { coords.push_back(p.X()); coords.push_back(p.Y()); coords.push_back(p.Z()); }
    return QByteArray::number(qulonglong(qHashBits(coords.data(), coords.size() * sizeof(double))))
        + ':' + QByteArray::number(qulonglong(qHashBits(m.tris.data(), m.tris.size() * sizeof(m.tris[0]))))
        + ':' + QByteArray::number(qulonglong(qHashBits(m.groups.data(), m.groups.size() * sizeof(int))))
        + ':' + QByteArray::number(m.tris.size()) + ':' + settings.toUtf8();
}
}   // namespace

bool MeshBackgroundJob::run(FissionFeature *owner, const QByteArray &key, size_t triangles, const QString &label,
                            const Work &work, Result &result, QString &err) {
    const auto hit = m_cache.constFind(key);
    if (hit != m_cache.constEnd()) {
        if (hit->shape.IsNull() && hit->mesh.empty()) { err = hit->summary; return false; }   // a job that failed
        result = *hit;
        return true;
    }
    const QString busy = label + " in the background...";
    if (m_running.contains(key)) { err = busy; return false; }
    if (int(triangles) > backgroundTriangles && QCoreApplication::instance()) {
        m_running.insert(key);
        QPointer<FissionFeature> self(owner);
        const QString taskId = QString::fromLatin1(key);
        FissionProgress::begin(taskId, label);
        std::thread([this, self, key, taskId, label, work] {
            QString jobErr;
            Result out;
            try {
                out = work(FissionProgress::callback(taskId));
            } catch (const Standard_Failure &e) {
                jobErr = QString("Geometry kernel error: %1").arg(e.GetMessageString());
            } catch (const std::exception &e) {
                jobErr = QString("Error: %1").arg(e.what());
            }
            if (out.shape.IsNull() && out.mesh.empty()) out.summary = jobErr.isEmpty() ? label + " failed" : jobErr;
            // Back on the main thread: only while the feature (and so this job) still exists
            QMetaObject::invokeMethod(QCoreApplication::instance(), [this, self, key, taskId, out] {
                FissionProgress::end(taskId);
                if (!self) return;
                m_running.remove(key);
                if (m_cache.size() > 8) m_cache.clear();
                m_cache.insert(key, out);
                self->touch();
                if (auto *doc = qobject_cast<FissionDocument *>(self->parent())) doc->scheduleRebuild();
            }, Qt::QueuedConnection);
        }).detach();
        err = busy;
        return false;
    }
    result = work({});
    if (result.shape.IsNull() && result.mesh.empty()) { err = result.summary; return false; }
    if (m_cache.size() > 8) m_cache.clear();
    m_cache.insert(key, result);
    return true;
}

bool FissionInsertMesh::compute(RebuildContext &ctx) {
    MeshUtils::Mesh m;
    if (!MeshUtils::decode(str("data").toLatin1(), m) || m.empty()) return fail("The mesh data is missing or damaged");
    QString name = str("bodyName");
    // Fusion may also have written this mesh body into the step STEP as a faceted B-rep: drop that copy
    if (!name.isEmpty() && flag("replaceFusionCopy")) {
        // STEP component names can carry a version ("Part v2") that Fusion's own name lacks
        const QString inComponent = QString("%1 (%2").arg(name, str("fusionComponent"));
        int hit = -1;
        for (int i = 0; i < ctx.bodies.size() && hit < 0; ++i)
            if (ctx.bodies[i].name == name && !MeshUtils::isMesh(ctx.bodies[i].shape)) hit = i;
        for (int i = 0; i < ctx.bodies.size() && hit < 0; ++i)
            if (ctx.bodies[i].name.startsWith(inComponent) && !MeshUtils::isMesh(ctx.bodies[i].shape)) hit = i;
        if (hit >= 0) ctx.bodies.removeAt(hit);
    }
    if (name.isEmpty()) {
        name = ctx.nextBodyName();
    } else {
        QSet<QString> taken;
        for (const BodyState &b : ctx.bodies) taken.insert(b.name);
        const QString wanted = name;
        for (int k = 2; taken.contains(name); ++k) name = QString("%1 %2").arg(wanted).arg(k);
    }
    ctx.bodies.append({name, MeshUtils::toFace(m), id()});
    return true;
}

bool FissionMeshOperation::compute(RebuildContext &ctx) {
    QList<int> idx;
    for (const QVariant &v : list("bodies")) {
        const int i = v.toInt();
        if (i < 0 || i >= ctx.bodies.size()) return fail(QString("Body %1 no longer exists").arg(i + 1));
        if (MeshUtils::isMesh(ctx.bodies[i].shape) != wantsMesh())
            return fail(QString("%1 is %2").arg(ctx.bodies[i].name, wantsMesh() ? "not a mesh body" : "already a mesh body"));
        if (!idx.contains(i)) idx << i;
    }
    // An empty pick list waits for the user; designs and scripts that leave it out mean every matching body
    if (idx.isEmpty() && props().contains("bodies")) return fail(wantsMesh() ? "Select the mesh bodies" : "Select the solid bodies");
    if (idx.isEmpty())
        for (int i = 0; i < ctx.bodies.size(); ++i)
            if (MeshUtils::isMesh(ctx.bodies[i].shape) == wantsMesh()) idx << i;
    if (idx.isEmpty()) return fail(wantsMesh() ? "There are no mesh bodies" : "There are no solid bodies");
    for (int i : idx) {
        QString err;
        TopoDS_Shape s = ctx.bodies[i].shape;
        if (!apply(ctx, s, err)) return fail(ctx.bodies[i].name + ": " + err);
        ctx.bodies[i].shape = s;
    }
    return true;
}

bool FissionMeshConvert::apply(RebuildContext &, TopoDS_Shape &shape, QString &err) {
    if (str("direction", "toBRep") == "toMesh") {
        const double size = std::sqrt(GeomUtils::boundingBox(shape).SquareExtent());
        const QString r = str("refinement", "medium");
        const double defl = size * (r == "coarse" ? 0.004 : r == "fine" ? 0.0002 : 0.001);
        const MeshUtils::Mesh m = MeshUtils::fromShape(shape, std::max(1e-3, defl));
        if (m.empty()) { err = "Could not tessellate the body"; return false; }
        shape = MeshUtils::toShape(m);
        return true;
    }
    const MeshUtils::Mesh m = MeshUtils::fromShape(shape);
    const bool prismatic = str("method", "prismatic") == "prismatic";
    const auto convert = [m, prismatic](const MeshUtils::Progress &progress) -> MeshBackgroundJob::Result {
        QString why, summary, err;
        TopoDS_Shape b = MeshUtils::toFittedBRep(m, prismatic, why, &summary, progress);
        if (b.IsNull() && prismatic) {
            QString again;
            if (progress) progress(-1, "Fitting failed - merging facets");
            b = MeshUtils::toFittedBRep(m, false, again, &summary);
            if (!b.IsNull()) summary += " - faceted, fitting failed: " + why;
        }
        if (b.IsNull()) {
            if (progress) progress(-1, "One face per triangle");
            b = MeshUtils::toBRep(m, err);   // open or non-manifold meshes: one face per triangle
            summary = b.IsNull() ? err : "one face per triangle (" + why + ")";
        }
        return {b, summary, {}};
    };
    MeshBackgroundJob::Result result;
    if (!m_job.run(this, meshKey(m, prismatic ? "p" : "f"), m.tris.size(),
                   QString("Converting %1 triangles").arg(m.tris.size()), convert, result, err))
        return false;
    m_summary << result.summary;
    shape = result.shape;
    return true;
}

bool FissionMeshReverse::apply(RebuildContext &, TopoDS_Shape &shape, QString &) {
    MeshUtils::Mesh m = MeshUtils::fromShape(shape);
    MeshUtils::reverse(m);
    shape = MeshUtils::toShape(m);
    return true;
}

bool FissionMeshSmooth::apply(RebuildContext &ctx, TopoDS_Shape &shape, QString &err) {
    double it = 0, s = 0;
    if (!number(ctx, "iterations", it) || !number(ctx, "smoothness", s)) { err = error(); return false; }
    if (it < 1 || it > 200) { err = "Iterations must be between 1 and 200"; return false; }
    if (s <= 0 || s > 1) { err = "Smoothness must be between 0 and 1"; return false; }
    MeshUtils::Mesh m = MeshUtils::fromShape(shape);
    MeshUtils::smooth(m, int(it), s);
    shape = MeshUtils::toShape(m);
    return true;
}

bool FissionMeshReduce::apply(RebuildContext &ctx, TopoDS_Shape &shape, QString &err) {
    double p = 0;
    if (!number(ctx, "proportion", p)) { err = error(); return false; }
    if (p <= 0 || p > 100) { err = "Proportion must be between 0 and 100 %"; return false; }
    MeshUtils::Mesh m = MeshUtils::fromShape(shape);
    MeshUtils::reduce(m, p / 100.0);
    if (m.empty()) { err = "Nothing is left; use a larger proportion"; return false; }
    shape = MeshUtils::toShape(m);
    return true;
}

bool FissionMeshRepair::apply(RebuildContext &ctx, TopoDS_Shape &shape, QString &err) {
    double tol = 0;
    if (!number(ctx, "tolerance", tol)) { err = error(); return false; }
    if (tol < 0) { err = "The weld tolerance cannot be negative"; return false; }
    MeshUtils::Mesh m = MeshUtils::fromShape(shape);
    const MeshUtils::RepairStats st = MeshUtils::repair(m, tol, flag("orient", true), flag("fillHoles", true));
    if (m.empty()) { err = "Nothing is left after welding; use a smaller tolerance"; return false; }
    QStringList done;
    if (st.welded) done << QString("%1 nodes welded").arg(st.welded);
    if (st.duplicates) done << QString("%1 duplicate triangles removed").arg(st.duplicates);
    if (st.flipped) done << QString("%1 triangles flipped").arg(st.flipped);
    if (st.holes) done << QString("%1 holes filled").arg(st.holes);
    if (done.isEmpty()) done << "nothing to fix";
    m_summary << done.join(", ") + (st.closed ? " - closed" : " - still open");
    shape = MeshUtils::toShape(m);
    return true;
}

bool FissionMeshShell::apply(RebuildContext &ctx, TopoDS_Shape &shape, QString &err) {
    double t = 0;
    if (!number(ctx, "thickness", t)) { err = error(); return false; }
    MeshUtils::Mesh m = MeshUtils::fromShape(shape);
    const bool outside = str("direction", "inside") == "outside";
    if (manipulator().isEmpty() && !m.tris.empty()) {
        // Thickness arrow on the top of the first body, pointing the way the wall grows
        size_t top = 0;
        double best = -1e300;
        for (size_t i = 0; i < m.tris.size(); ++i) {
            const auto &tr = m.tris[i];
            const double z = m.nodes[tr[0]].Z() + m.nodes[tr[1]].Z() + m.nodes[tr[2]].Z();
            const gp_XYZ n = (m.nodes[tr[1]].XYZ() - m.nodes[tr[0]].XYZ()).Crossed(m.nodes[tr[2]].XYZ() - m.nodes[tr[0]].XYZ());
            if (z > best && n.Modulus() > 1e-12) { best = z; top = i; }
        }
        const auto &tr = m.tris[top];
        const gp_XYZ c = (m.nodes[tr[0]].XYZ() + m.nodes[tr[1]].XYZ() + m.nodes[tr[2]].XYZ()) / 3;
        const gp_Dir n((m.nodes[tr[1]].XYZ() - m.nodes[tr[0]].XYZ()).Crossed(m.nodes[tr[2]].XYZ() - m.nodes[tr[0]].XYZ()));
        setManipulator(gp_Pnt(c), outside ? n : n.Reversed(), "thickness", t);
    }
    if (!MeshUtils::shell(m, t, outside, err)) return false;
    shape = MeshUtils::toShape(m);
    return true;
}

bool FissionMeshFaceGroups::apply(RebuildContext &ctx, TopoDS_Shape &shape, QString &err) {
    double angle = 0, minSize = 0, tol = 0;
    if (!number(ctx, "angle", angle) || !number(ctx, "minSize", minSize)) { err = error(); return false; }
    // Older designs have no tolerance: the fitter then derives it from the model size
    if (!get("tolerance").toString().isEmpty() && !number(ctx, "tolerance", tol)) { err = error(); return false; }
    if (angle <= 0 || angle >= 90) { err = "The angle must be between 0 and 90 degrees"; return false; }
    if (minSize < 0 || minSize >= 50) { err = "The minimum group size must be between 0 and 50 %"; return false; }
    if (tol < 0) { err = "The boundary tolerance cannot be negative"; return false; }
    // Fusion Accurate allows 0..0.01 (fraction of the body diagonal). Larger values are legacy absolute mm.
    const MeshUtils::Mesh m = MeshUtils::fromShape(shape);
    const bool accurate = str("method", "accurate") == "accurate";
    const auto group = [m, accurate, angle, minSize, tol](const MeshUtils::Progress &progress) -> MeshBackgroundJob::Result {
        MeshUtils::Mesh g = m;
        QString why;
        const int n = MeshUtils::generateFaceGroups(g, accurate, angle, minSize / 100.0, why, tol, progress);
        if (n <= 0) return {{}, why, {}};
        return {{}, QString("%1 face group%2").arg(n).arg(n == 1 ? "" : "s"), g};
    };
    const QString settings = QString("g:%1:%2:%3:%4").arg(accurate ? "a" : "f").arg(angle).arg(minSize).arg(tol);
    MeshBackgroundJob::Result result;
    if (!m_job.run(this, meshKey(m, settings), accurate ? m.tris.size() : 0,
                   QString("Grouping %1 triangles").arg(m.tris.size()), group, result, err))
        return false;
    m_summary << result.summary;
    shape = MeshUtils::toShape(result.mesh);
    return true;
}

bool FissionMeshRemesh::apply(RebuildContext &ctx, TopoDS_Shape &shape, QString &err) {
    double len = 0, it = 0, sharp = 0;
    if (!number(ctx, "edgeLength", len) || !number(ctx, "iterations", it) || !number(ctx, "sharpAngle", sharp)) {
        err = error();
        return false;
    }
    MeshUtils::Mesh m = MeshUtils::fromShape(shape);
    if (len <= 0) {
        // Default: the mean edge length
        double sum = 0;
        for (const auto &t : m.tris)
            for (int k = 0; k < 3; ++k) sum += m.nodes[t[k]].Distance(m.nodes[t[(k + 1) % 3]]);
        len = m.tris.empty() ? 1 : sum / (3.0 * m.tris.size());
    }
    if (it < 1 || it > 20) { err = "Iterations must be between 1 and 20"; return false; }
    const double area = MeshUtils::area(m);
    if (area / (len * len * 0.433) > 3e6) { err = "The edge length is too small: over 3 million triangles"; return false; }
    const size_t before = m.tris.size();
    MeshUtils::remesh(m, len, flag("adaptive", false), int(it), sharp);
    if (m.empty()) { err = "Remeshing left no triangles"; return false; }
    m_summary << QString("%1 -> %2 triangles").arg(before).arg(m.tris.size());
    shape = MeshUtils::toShape(m);
    return true;
}

bool FissionMeshCombine::compute(RebuildContext &ctx) {
    QList<int> idx;
    for (const QVariant &v : list("bodies")) {
        const int i = v.toInt();
        if (i < 0 || i >= ctx.bodies.size()) return fail(QString("Body %1 no longer exists").arg(i + 1));
        if (!MeshUtils::isMesh(ctx.bodies[i].shape)) return fail(ctx.bodies[i].name + " is not a mesh body");
        if (!idx.contains(i)) idx << i;
    }
    if (idx.size() < 2) return fail("Select two or more mesh bodies");
    MeshUtils::Mesh m = MeshUtils::fromShape(ctx.bodies[idx[0]].shape);
    for (int k = 1; k < idx.size(); ++k) MeshUtils::append(m, MeshUtils::fromShape(ctx.bodies[idx[k]].shape));
    ctx.bodies[idx[0]].shape = MeshUtils::toShape(m);
    QList<int> drop = idx.mid(1);
    std::sort(drop.begin(), drop.end(), std::greater<int>());
    for (int i : drop) ctx.bodies.removeAt(i);
    return true;
}

bool FissionMeshPlaneCut::apply(RebuildContext &ctx, TopoDS_Shape &shape, QString &err) {
    gp_Ax3 ax;
    if (!ctx.resolvePlane(get("plane"), ax, err)) return false;
    double offset = 0;
    if (props().contains("offset") && !number(ctx, "offset", offset)) { err = error(); return false; }
    MeshUtils::Mesh m = MeshUtils::fromShape(shape);
    if (manipulator().isEmpty() && !m.nodes.empty()) {
        // Offset arrow where the plane passes the middle of the first body
        const gp_Pnt mid = MeshUtils::centroid(m);
        const gp_XYZ n = ax.Direction().XYZ(), o = ax.Location().XYZ();
        setManipulator(gp_Pnt(mid.XYZ() - n * (mid.XYZ() - o).Dot(n)), ax.Direction(), "offset", offset);
    }
    ax.Translate(gp_Vec(ax.Direction()) * offset);
    if (flag("flip")) ax.ZReverse();
    if (!MeshUtils::planeCut(m, gp_Pln(ax), flag("fill", true), err)) return false;
    shape = MeshUtils::toShape(m);
    return true;
}

QString FissionDocument::insertMesh(const QString &path, const QString &unit) {
    const double scale = unit == "cm" ? 10.0 : unit == "m" ? 1000.0 : unit == "in" ? 25.4 : 1.0;
    MeshUtils::Mesh m;
    QString err;
    if (!MeshUtils::readFile(path, scale, m, err)) { m_lastError = err; return QString(); }
    FissionFeature *f = addFeature("InsertMesh", {{"data", QString::fromLatin1(MeshUtils::encode(m))},
                                                  {"fileName", QFileInfo(path).fileName()}, {"unit", unit}},
                                   QFileInfo(path).completeBaseName());
    if (!f) { m_lastError = "Could not add the mesh"; return QString(); }
    return f->id();
}
