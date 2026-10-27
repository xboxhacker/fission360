// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#include "FissionFeature.h"
#include "FissionDocument.h"
#include "FissionSketch.h"
#include "FissionConstruction.h"
#include "FissionExtrude.h"
#include "FissionRevolve.h"
#include "FissionBox.h"
#include "FissionCylinder.h"
#include "FissionSphere.h"
#include "FissionTorus.h"
#include "FissionCoil.h"
#include "FissionPipe.h"
#include "FissionFillet.h"
#include "FissionChamfer.h"
#include "FissionLoft.h"
#include "FissionSweep.h"
#include "FissionShell.h"
#include "FissionTransform.h"
#include "FissionSolidTools.h"
#include "FissionHole.h"
#include "FissionMeshFeatures.h"
#include "FissionImport.h"
#include "FissionSheetMetal.h"
#include "GeomUtils.h"
#include "MeshUtils.h"

#include <QJsonArray>
#include <QSet>
#include <QUuid>
#include <QVector3D>
#include <algorithm>
#include <array>
#include <cmath>
#include <BRepAdaptor_Curve.hxx>
#include <BRepBndLib.hxx>
#include <Bnd_Box.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepAlgoAPI_Common.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepAlgoAPI_Fuse.hxx>
#include <BRep_Builder.hxx>
#include <BRep_Tool.hxx>
#include <Message_ProgressIndicator.hxx>
#include <Standard_Failure.hxx>
#include <QElapsedTimer>
#include <cstdio>
#include <TopExp_Explorer.hxx>
#include <TopTools_ListOfShape.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Compound.hxx>
#include <limits>

// ---------------------------------------------------------------------------
// RebuildContext
// ---------------------------------------------------------------------------

void RebuildContext::registerFeature(FissionFeature *f) {
    m_features.insert(f->id(), f);
}

double RebuildContext::evaluate(const QString &expr, QString *error) const {
    return m_doc->evaluate(expr, error);
}

static QString featureIdFromRef(const QString &ref) {
    return ref.startsWith("feature:") ? ref.mid(8) : QString();
}

bool RebuildContext::resolvePlane(const QVariant &ref, gp_Ax3 &out, QString &error) const {
    // An explicit frame {frame: true, origin [x,y,z] mm, x [..], y [..]}: a sketch from a Fusion import, where
    // Fusion gives each sketch's exact placement (offset planes, faces, its own X direction)
    if (ref.typeId() == QMetaType::QVariantMap && ref.toMap().value("frame").toBool()) {
        const QVariantMap m = ref.toMap();
        auto vec = [&](const char *k) {
            const QVariantList l = m.value(k).toList();
            return gp_XYZ(l.value(0).toDouble(), l.value(1).toDouble(), l.value(2).toDouble());
        };
        const gp_XYZ x = vec("x"), y = vec("y");
        const gp_XYZ n = x.Crossed(y);
        if (x.Modulus() < 1e-12 || n.Modulus() < 1e-12) { error = "The sketch frame is degenerate"; return false; }
        out = gp_Ax3(gp_Pnt(vec("origin")), gp_Dir(n), gp_Dir(x));
        return true;
    }
    if (ref.typeId() == QMetaType::QVariantMap) {
        int body = -1;
        TopoDS_Face face;
        if (!resolveFace(ref, body, face, error)) return false;
        BRepAdaptor_Surface surf(face);
        if (surf.GetType() != GeomAbs_Plane) { error = "Selected face is not planar"; return false; }
        gp_Ax3 ax = surf.Plane().Position();
        gp_Dir normal = ax.Direction();
        if (face.Orientation() == TopAbs_REVERSED) normal.Reverse();
        // Origin at the face centroid projected on the plane keeps sketch coordinates local to the face
        gp_Pnt c = GeomUtils::faceCentroid(face);
        out = gp_Ax3(c, normal, ax.XDirection());
        return true;
    }
    const QString s = ref.toString();
    bool ok = false;
    out = GeomUtils::standardPlane(s.isEmpty() ? QStringLiteral("XY") : s, &ok);
    if (ok) return true;
    const QString id = featureIdFromRef(s);
    if (auto *f = feature(id)) {
        if (auto *cp = qobject_cast<FissionConstructionPlane *>(f)) { out = cp->plane(); return true; }
        if (auto *sk = qobject_cast<FissionSketch *>(f)) { out = sk->plane(); return true; }
        error = QString("'%1' is not a plane").arg(f->name());
        return false;
    }
    error = QString("Plane reference '%1' not found").arg(s);
    return false;
}

bool RebuildContext::resolveAxis(const QVariant &ref, gp_Ax1 &out, QString &error) const {
    if (ref.typeId() == QMetaType::QVariantMap) {
        int body = -1;
        TopoDS_Edge edge;
        if (!resolveEdge(ref, body, edge, error)) return false;
        BRepAdaptor_Curve c(edge);
        if (c.GetType() == GeomAbs_Line) { out = c.Line().Position(); return true; }
        if (c.GetType() == GeomAbs_Circle) { out = c.Circle().Axis(); return true; }
        error = "Selected edge is not linear or circular";
        return false;
    }
    const QString s = ref.toString();
    if (s == "X") { out = gp_Ax1(gp_Pnt(0, 0, 0), gp_Dir(1, 0, 0)); return true; }
    if (s == "Y") { out = gp_Ax1(gp_Pnt(0, 0, 0), gp_Dir(0, 1, 0)); return true; }
    if (s == "Z" || s.isEmpty()) { out = gp_Ax1(gp_Pnt(0, 0, 0), gp_Dir(0, 0, 1)); return true; }
    if (s.startsWith("sketchline:")) {
        const QStringList parts = s.split(':');
        if (parts.size() == 3) {
            if (auto *sk = qobject_cast<FissionSketch *>(feature(parts[1]))) {
                if (sk->lineAxis(parts[2].toInt(), out)) return true;
                error = "Sketch entity is not a line";
                return false;
            }
        }
        error = QString("Axis reference '%1' not found").arg(s);
        return false;
    }
    if (auto *f = feature(featureIdFromRef(s))) {
        if (auto *ca = qobject_cast<FissionConstructionAxis *>(f)) { out = ca->axis(); return true; }
        error = QString("'%1' is not an axis").arg(f->name());
        return false;
    }
    error = QString("Axis reference '%1' not found").arg(s);
    return false;
}

bool RebuildContext::resolvePoint(const QVariant &ref, gp_Pnt &out, QString &error) const {
    if (ref.typeId() == QMetaType::QVariantList || ref.typeId() == QMetaType::QStringList) {
        const QVariantList xyz = ref.toList();
        if (xyz.size() != 3) { error = "Point needs 3 coordinates"; return false; }
        double c[3];
        for (int i = 0; i < 3; ++i) {
            c[i] = evaluate(xyz[i].toString(), &error);
            if (!error.isEmpty()) return false;
        }
        out = gp_Pnt(c[0], c[1], c[2]);
        return true;
    }
    const QString s = ref.toString();
    if (s.isEmpty() || s == "origin") { out = gp_Pnt(0, 0, 0); return true; }
    if (auto *cp = qobject_cast<FissionConstructionPoint *>(feature(featureIdFromRef(s)))) {
        out = cp->point();
        return true;
    }
    error = QString("Point reference '%1' not found").arg(s);
    return false;
}

template <typename Midpoint>
static int matchSubShape(const QVariantMap &ref, const TopTools_IndexedMapOfShape &map, Midpoint midpoint) {
    const int index = ref.value("index", -1).toInt();
    const int count = ref.value("count", -1).toInt();
    // Same topology as when the reference was made: the map order is stable, trust the index
    if (count == map.Extent() && index >= 0 && index < map.Extent()) return index;
    if (!ref.contains("x")) return (index >= 0 && index < map.Extent()) ? index : -1;
    // Topology changed: fall back to the sub-shape nearest the recorded location
    const gp_Pnt hint(ref["x"].toDouble(), ref["y"].toDouble(), ref["z"].toDouble());
    int best = -1;
    double bestDist = std::numeric_limits<double>::max();
    for (int i = 1; i <= map.Extent(); ++i) {
        const double d = midpoint(map(i)).Distance(hint);
        if (d < bestDist) { bestDist = d; best = i - 1; }
    }
    return best;
}

bool RebuildContext::resolveEdge(const QVariant &ref, int &body, TopoDS_Edge &out, QString &error) const {
    const QVariantMap m = ref.toMap();
    body = m.value("body", -1).toInt();
    if (m.value("kind").toString() != "edge" || body < 0 || body >= bodies.size()) {
        error = "Edge reference is invalid (body no longer exists)";
        return false;
    }
    const TopTools_IndexedMapOfShape map = GeomUtils::edgeMap(bodies[body].shape);
    const int idx = matchSubShape(m, map, [](const TopoDS_Shape &s) { return GeomUtils::edgeMidpoint(TopoDS::Edge(s)); });
    if (idx < 0) { error = "Referenced edge no longer exists"; return false; }
    out = TopoDS::Edge(map(idx + 1));
    return true;
}

bool RebuildContext::resolveFace(const QVariant &ref, int &body, TopoDS_Face &out, QString &error) const {
    const QVariantMap m = ref.toMap();
    body = m.value("body", -1).toInt();
    if (m.value("kind").toString() != "face" || body < 0 || body >= bodies.size()) {
        error = "Face reference is invalid (body no longer exists)";
        return false;
    }
    const TopTools_IndexedMapOfShape map = GeomUtils::faceMap(bodies[body].shape);
    const int idx = matchSubShape(m, map, [](const TopoDS_Shape &s) { return GeomUtils::faceCentroid(TopoDS::Face(s)); });
    if (idx < 0) { error = "Referenced face no longer exists"; return false; }
    out = TopoDS::Face(map(idx + 1));
    return true;
}

bool RebuildContext::resolveProfiles(const QString &sketchId, const QVariantList &profiles,
                                     TopoDS_Shape &out, gp_Ax3 &plane, QString &error) const {
    auto *sketch = qobject_cast<FissionSketch *>(feature(sketchId));
    if (!sketch) { error = "Select a sketch profile"; return false; }
    if (!sketch->isHealthy()) { error = QString("Sketch '%1' has errors").arg(sketch->name()); return false; }
    plane = sketch->plane();
    QList<int> indices;
    if (profiles.isEmpty()) {
        indices = sketch->defaultProfiles();
    } else {
        for (const QVariant &v : profiles) indices.append(v.toInt());
        // After a sketch edit, profile indices often shift. Fall back to the sketch's default
        // closed regions so Extrude / Revolve / Sweep do not vanish with "Profile N no longer exists".
        bool stale = false;
        for (int i : indices)
            if (i < 0 || i >= sketch->profileCount()) { stale = true; break; }
        if (stale) {
            indices = sketch->defaultProfiles();
            if (indices.isEmpty()) {
                error = QString("Sketch '%1' no longer has the selected profiles (and no closed profiles remain)")
                            .arg(sketch->name());
                return false;
            }
        }
    }
    QList<TopoDS_Shape> faces;
    for (int i : indices) {
        if (i < 0 || i >= sketch->profileCount()) {
            error = QString("Profile %1 no longer exists in '%2'").arg(i).arg(sketch->name());
            return false;
        }
        faces.append(sketch->profiles()[i]);
    }
    if (faces.isEmpty()) { error = QString("Sketch '%1' has no closed profiles").arg(sketch->name()); return false; }
    out = GeomUtils::makeCompound(faces);
    return true;
}

bool RebuildContext::resolveFaceProfiles(const QVariantList &faceRefs, TopoDS_Shape &out, gp_Ax3 &plane,
                                         QString &error) const {
    QList<TopoDS_Shape> faces;
    for (int i = 0; i < faceRefs.size(); ++i) {
        int body = -1;
        TopoDS_Face face;
        if (!resolveFace(faceRefs[i], body, face, error)) return false;
        BRepAdaptor_Surface surf(face);
        if (surf.GetType() != GeomAbs_Plane) { error = "Only flat faces can be extruded"; return false; }
        if (i == 0) {
            if (!resolvePlane(faceRefs[i], plane, error)) return false;
        } else {
            const gp_Pln pl = surf.Plane();
            if (!pl.Axis().Direction().IsParallel(plane.Direction(), 1e-6)
                || pl.Distance(plane.Location()) > 1e-6) {
                error = "Faces extruded together must lie in one plane";
                return false;
            }
        }
        faces.append(face);
    }
    if (faces.isEmpty()) { error = "Select a profile or a flat face"; return false; }
    out = GeomUtils::makeCompound(faces);
    return true;
}

double RebuildContext::modelSize() const {
    Bnd_Box box;
    for (const auto &b : bodies) box.Add(GeomUtils::boundingBox(b.shape));
    if (box.IsVoid()) return 100.0;
    return std::max(100.0, std::sqrt(box.SquareExtent()));
}

void RebuildContext::addBodies(const TopoDS_Shape &shape, const QString &featureId) {
    QList<TopoDS_Shape> parts = GeomUtils::solids(shape);
    if (parts.isEmpty() && !shape.IsNull()) parts.append(shape);   // surface bodies
    for (const auto &p : parts) bodies.append({nextBodyName(), p, featureId});
}

namespace {
// Cancels an OCCT boolean that runs longer than the limit, so a pathological imported shape fails the
// feature instead of freezing the app
class BooleanTimeLimit : public Message_ProgressIndicator {
public:
    explicit BooleanTimeLimit(qint64 ms) : m_ms(ms) { m_timer.start(); }
    Standard_Boolean UserBreak() override { return m_timer.elapsed() > m_ms; }
    void Show(const Message_ProgressScope &, const Standard_Boolean) override {}
    bool expired() const { return m_timer.elapsed() > m_ms; }
    qint64 elapsed() const { return m_timer.elapsed(); }

private:
    QElapsedTimer m_timer;
    qint64 m_ms;
};

constexpr qint64 kBooleanLimitMs = 20000;

void traceBoolean(const char *stage, qint64 ms) {
    static const bool on = qEnvironmentVariableIsSet("FISSION_TRACE_BOOLEANS");
    if (!on) return;
    std::printf("    [%s %lld ms]\n", stage, ms);
    std::fflush(stdout);
}

// Builds a boolean with the time limit; false (with error) when it failed or was cancelled
bool buildBoolean(BRepAlgoAPI_BooleanOperation &op, const char *what, QString &error) {
    Handle(BooleanTimeLimit) limit = new BooleanTimeLimit(kBooleanLimitMs);
    op.SetRunParallel(Standard_True);
    op.Build(limit->Start());
    traceBoolean(what, limit->elapsed());
    if (limit->expired()) {
        error = QString("%1 took longer than %2 s and was stopped (very complex geometry such as threads). "
                        "Try the New Body operation instead.")
                    .arg(what).arg(kBooleanLimitMs / 1000);
        return false;
    }
    if (!op.IsDone() || op.HasErrors()) { error = QString("%1 failed").arg(what); return false; }
    return true;
}

TopoDS_Shape unifyTraced(const TopoDS_Shape &shape) {
    QElapsedTimer t;
    t.start();
    TopoDS_Shape out = GeomUtils::unify(shape);
    traceBoolean("unify", t.elapsed());
    return out;
}

// Join of several profiles (one Extrude/Revolve/...) must produce as few solids as possible. Without this,
// empty-document Join falls through to addBodies() and each prism becomes its own BodyN.
TopoDS_Shape fuseToolLumps(const TopoDS_Shape &tool, QString &error) {
    QList<TopoDS_Shape> lumps = GeomUtils::solids(tool);
    if (lumps.size() <= 1) return tool;
    TopoDS_Shape acc = lumps.first();
    for (int i = 1; i < lumps.size(); ++i) {
        BRepAlgoAPI_Fuse fuse;
        TopTools_ListOfShape args, tools;
        args.Append(acc);
        tools.Append(lumps[i]);
        fuse.SetArguments(args);
        fuse.SetTools(tools);
        if (!buildBoolean(fuse, "Join", error)) return TopoDS_Shape();
        acc = unifyTraced(fuse.Shape());
    }
    return acc;
}
}  // namespace

bool RebuildContext::applyOperation(const TopoDS_Shape &tool, FeatureOperation op, const QVariantList &targets,
                                    const QString &featureId, QString &error) {
    if (tool.IsNull()) { error = "Feature produced no geometry"; return false; }

    TopoDS_Shape work = tool;
    if (op == FeatureOperation::Join) {
        work = fuseToolLumps(tool, error);
        if (work.IsNull()) return false;
    }

    if (op == FeatureOperation::NewBody || (op == FeatureOperation::Join && bodies.isEmpty())) {
        addBodies(work, featureId);
        return true;
    }

    // Target bodies: explicit list, otherwise every body whose bounds touch the tool
    QList<int> idx;
    if (!targets.isEmpty()) {
        for (const QVariant &v : targets) {
            int i = v.toInt();
            if (i < 0 || i >= bodies.size()) { error = QString("Target body %1 no longer exists").arg(i); return false; }
            if (MeshUtils::isMesh(bodies[i].shape)) {
                error = QString("%1 is a mesh body; convert it to a solid first").arg(bodies[i].name);
                return false;
            }
            if (!idx.contains(i)) idx.append(i);
        }
        std::sort(idx.begin(), idx.end());
    } else {
        Bnd_Box toolBox = GeomUtils::boundingBox(tool);
        toolBox.Enlarge(1e-4);
        // Match by name against the live document bodies so a hidden body is not silently joined / cut
        auto bodyHidden = [&](const QString &name) {
            if (!m_doc || name.isEmpty()) return false;
            for (FissionBody *b : m_doc->bodies())
                if (b && b->name() == name) return !b->isVisible();
            return false;
        };
        for (int i = 0; i < bodies.size(); ++i) {
            if (MeshUtils::isMesh(bodies[i].shape)) continue;
            if (bodyHidden(bodies[i].name)) continue;
            if (!toolBox.IsOut(GeomUtils::boundingBox(bodies[i].shape)))
                idx.append(i);
        }
    }

    if (idx.isEmpty()) {
        if (op == FeatureOperation::Join) { addBodies(work, featureId); return true; }
        error = op == FeatureOperation::Cut ? "Cut does not intersect any body" : "Intersect does not overlap any body";
        return false;
    }

    try {
        if (op == FeatureOperation::Join) {
            TopTools_ListOfShape args, tools;
            for (int i : idx) args.Append(bodies[i].shape);
            tools.Append(work);
            BRepAlgoAPI_Fuse fuse;
            fuse.SetArguments(args);
            fuse.SetTools(tools);
            if (!buildBoolean(fuse, "Join", error)) return false;
            const QList<TopoDS_Shape> parts = GeomUtils::solids(unifyTraced(fuse.Shape()));
            if (parts.isEmpty()) { error = "Join produced an empty result"; return false; }
            // First target keeps its name; merged targets disappear; disjoint lumps become new bodies
            for (int k = idx.size() - 1; k >= 1; --k) bodies.removeAt(idx[k]);
            const int at = idx.first();
            bodies[at].shape = parts.first();
            for (int k = 1; k < parts.size(); ++k) bodies.insert(at + k, {nextBodyName(), parts[k], featureId});
            return true;
        }

        // Cut / Intersect each target independently
        for (int k = idx.size() - 1; k >= 0; --k) {
            const int i = idx[k];
            TopoDS_Shape result;
            TopTools_ListOfShape args, tools;
            args.Append(bodies[i].shape);
            tools.Append(work);
            if (op == FeatureOperation::Cut) {
                BRepAlgoAPI_Cut cut;
                cut.SetArguments(args);
                cut.SetTools(tools);
                if (!buildBoolean(cut, "Cut", error)) return false;
                result = cut.Shape();
            } else {
                BRepAlgoAPI_Common common;
                common.SetArguments(args);
                common.SetTools(tools);
                if (!buildBoolean(common, "Intersect", error)) return false;
                result = common.Shape();
            }
            const QList<TopoDS_Shape> parts = GeomUtils::solids(unifyTraced(result));
            if (parts.isEmpty()) {
                bodies.removeAt(i);
                continue;
            }
            bodies[i].shape = parts.first();
            for (int p = 1; p < parts.size(); ++p) bodies.insert(i + p, {nextBodyName(), parts[p], featureId});
        }
        return true;
    } catch (const Standard_Failure &e) {
        error = QString("Boolean operation failed: %1").arg(e.GetMessageString());
        return false;
    }
}

// ---------------------------------------------------------------------------
// FissionFeature
// ---------------------------------------------------------------------------

static quint64 g_revisions = 0;

FissionFeature::FissionFeature(QObject *parent)
    : QObject(parent), m_id(QUuid::createUuid().toString(QUuid::WithoutBraces).left(8)), m_revision(++g_revisions) {
    connect(this, &FissionFeature::changed, this, [this] { m_revision = ++g_revisions; });
}

void FissionFeature::touch() { m_revision = ++g_revisions; }

void FissionFeature::setName(const QString &name) {
    if (m_name != name) { m_name = name; emit changed(); }
}

void FissionFeature::setSuppressed(bool s) {
    if (m_suppressed != s) { m_suppressed = s; emit changed(); }
}

void FissionFeature::setVisible(bool v) {
    if (m_visible != v) { m_visible = v; emit visibilityChanged(); }
}

void FissionFeature::setProps(const QVariantMap &props) {
    if (m_props != props) { m_props = props; emit changed(); }
}

void FissionFeature::set(const QString &key, const QVariant &value) {
    if (m_props.value(key) != value) { m_props.insert(key, value); emit changed(); }
}

bool FissionFeature::run(RebuildContext &ctx) {
    const QString before = m_error;
    m_error.clear();
    m_manipulator.clear();
    resetResult();
    bool ok = false;
    try {
        if (flag("fusionPassThrough"))
            ok = applyFusionSnapshot(ctx);
        else {
            ok = compute(ctx);
            if (!ok && m_error.isEmpty()) m_error = "Feature failed";
        }
    } catch (const Standard_Failure &e) {
        m_error = QString("Geometry kernel error: %1").arg(e.GetMessageString());
    } catch (const std::exception &e) {
        m_error = QString::fromUtf8(e.what());
    } catch (...) {
        m_error = QStringLiteral("Geometry kernel error");
    }
    if (before != m_error) emit statusChanged();
    return m_error.isEmpty();
}

bool FissionFeature::applyFusionSnapshot(RebuildContext &ctx) {
    const QString data = str("snapshot");
    const QString stepB64 = str("snapshotStepB64");
    const bool empty = flag("snapshotEmpty");
    if (data.isEmpty() && stepB64.isEmpty() && !empty) return true;   // no snapshot: bodies stay as they are
    const QList<BodyState> before = ctx.bodies;
    ctx.bodies.clear();
    if (empty) return true;
    if (m_snapshotRevision != revision()) {
        m_snapshotShape.Nullify();
        m_snapshotNames.clear();
        if (!data.isEmpty()) {
            m_snapshotShape = FissionImportCAD::decode(data);
        } else if (!stepB64.isEmpty()) {
            // Lazy: Fusion exporter embeds base64 STEP per step — convert only when this step is needed
            QString err;
            QVariantList parts;
            m_snapshotShape = FissionImportCAD::readStepBase64(stepB64, err, &parts);
            m_snapshotNames = FissionImportCAD::fusionBodyNames(parts);
            if (m_snapshotShape.IsNull() && !err.isEmpty()) {
                ctx.bodies = before;
                return fail(err);
            }
        }
        m_snapshotRevision = revision();
    }
    if (m_snapshotShape.IsNull()) {
        ctx.bodies = before;
        return fail("The Fusion BRep snapshot of this step is damaged");
    }
    QList<TopoDS_Shape> solids;
    for (TopExp_Explorer e(m_snapshotShape, TopAbs_SOLID); e.More(); e.Next()) solids << e.Current();
    // Fusion's body names: the solid's own name in the STEP, else the exporter's list matched by bounding box
    QStringList names = m_snapshotNames.size() == solids.size() ? m_snapshotNames : QStringList();
    while (names.size() < solids.size()) names << QString();
    const QVariantList listed = list("snapshotBodies");
    if (!listed.isEmpty() && names.contains(QString())) {
        struct Pair { double d; int s, l; };
        QList<std::array<double, 6>> boxes;
        for (const QVariant &v : listed) {
            const QVariantList b = v.toMap().value("box").toList();
            std::array<double, 6> a{};
            for (int k = 0; k < 6; ++k) a[k] = b.value(k).toDouble();
            boxes << a;
        }
        QList<Pair> pairs;
        QList<double> tol;
        for (int s = 0; s < solids.size(); ++s) {
            Bnd_Box bb;
            BRepBndLib::Add(solids[s], bb, false);
            if (bb.IsVoid()) { tol << 0.0; continue; }
            double x0, y0, z0, x1, y1, z1;
            bb.Get(x0, y0, z0, x1, y1, z1);
            const double mine[6] = {x0, y0, z0, x1, y1, z1};
            tol << 0.25 * std::sqrt((x1 - x0) * (x1 - x0) + (y1 - y0) * (y1 - y0) + (z1 - z0) * (z1 - z0)) + 1.0;
            if (!names[s].isEmpty()) continue;
            for (int l = 0; l < boxes.size(); ++l) {
                double d = 0;
                for (int k = 0; k < 6; ++k) d += std::abs(mine[k] - boxes[l][k]);
                pairs.append({d, s, l});
            }
        }
        std::sort(pairs.begin(), pairs.end(), [](const Pair &a, const Pair &b) { return a.d < b.d; });
        QSet<int> usedL;
        for (const QString &n : names)
            for (int l = 0; l < listed.size(); ++l)
                if (!n.isEmpty() && listed[l].toMap().value("name").toString() == n) usedL.insert(l);
        for (const Pair &p : pairs) {
            if (!names[p.s].isEmpty() || usedL.contains(p.l) || p.d > tol[p.s] * 6) continue;
            names[p.s] = listed[p.l].toMap().value("name").toString();
            usedL.insert(p.l);
        }
    }
    QSet<QString> taken;
    // Otherwise keep body names and origins by position so later steps and the browser stay stable
    auto add = [&](const TopoDS_Shape &s, QString name) {
        const int i = ctx.bodies.size();
        if (name.isEmpty()) name = i < before.size() ? before[i].name : ctx.nextBodyName();
        const QString wanted = name;
        for (int k = 2; taken.contains(name); ++k) name = QString("%1 %2").arg(wanted).arg(k);
        taken.insert(name);
        ctx.bodies.append({name, s, id()});
    };
    for (int s = 0; s < solids.size(); ++s) add(solids[s], names[s]);
    BRep_Builder bb;
    TopoDS_Compound loose;
    bb.MakeCompound(loose);
    bool hasLoose = false;
    for (TopExp_Explorer e(m_snapshotShape, TopAbs_SHELL, TopAbs_SOLID); e.More(); e.Next()) { bb.Add(loose, e.Current()); hasLoose = true; }
    for (TopExp_Explorer e(m_snapshotShape, TopAbs_FACE, TopAbs_SHELL); e.More(); e.Next()) { bb.Add(loose, e.Current()); hasLoose = true; }
    if (hasLoose) add(loose, QString());
    return true;
}

static QVector3D vec3(const gp_XYZ &p) { return QVector3D(p.X(), p.Y(), p.Z()); }

void FissionFeature::setManipulator(const gp_Pnt &origin, const gp_Dir &direction, const QString &key, double value,
                                    double scale) {
    m_manipulator = {{"origin", vec3(origin.XYZ())},
                     {"direction", vec3(direction.XYZ())},
                     {"key", key},
                     {"value", value},
                     {"scale", scale}};
}

void FissionFeature::addArrow(const gp_Pnt &origin, const gp_Dir &direction, const QString &key, double value,
                              double scale) {
    QVariantList arrows = m_manipulator.value("arrows").toList();
    arrows.append(QVariantMap{{"origin", vec3(origin.XYZ())},
                              {"direction", vec3(direction.XYZ())},
                              {"key", key},
                              {"value", value},
                              {"scale", scale}});
    m_manipulator["arrows"] = arrows;
}

void FissionFeature::setHandles(const gp_Ax3 &base, std::initializer_list<Arrow> arrows) {
    if (arrows.size() == 0) return;
    auto it = arrows.begin();
    setManipulator(it->origin, it->direction, QString::fromLatin1(it->key), it->value, it->scale);
    for (++it; it != arrows.end(); ++it)
        addArrow(it->origin, it->direction, QString::fromLatin1(it->key), it->value, it->scale);
    setMoveManipulator(arrows.begin()->origin, base.Direction(), QStringLiteral("origin"), {});
    QVariantMap move = m_manipulator.value("move").toMap();
    move["x"] = vec3(base.XDirection().XYZ());
    move["y"] = vec3(base.YDirection().XYZ());
    move["planeOrigin"] = vec3(base.Location().XYZ());
    m_manipulator["move"] = move;
}

void FissionFeature::setAngleManipulator(const gp_Pnt &pivot, const gp_Dir &up, const gp_Dir &out, double height,
                                         const QString &key, double degrees) {
    m_manipulator["angle"] = QVariantMap{{"pivot", QVector3D(pivot.X(), pivot.Y(), pivot.Z())},
                                         {"up", QVector3D(up.X(), up.Y(), up.Z())},
                                         {"out", QVector3D(out.X(), out.Y(), out.Z())},
                                         {"height", height},
                                         {"key", key},
                                         {"value", degrees}};
}

void FissionFeature::setMoveManipulator(const gp_Pnt &origin, const gp_Dir &normal, const QString &key,
                                        const QVariantList &snaps) {
    m_manipulator["move"] = QVariantMap{{"origin", QVector3D(origin.X(), origin.Y(), origin.Z())},
                                        {"normal", QVector3D(normal.X(), normal.Y(), normal.Z())},
                                        {"key", key},
                                        {"snaps", snaps}};
}

QString FissionFeature::str(const QString &key, const QString &def) const {
    return m_props.contains(key) ? m_props.value(key).toString() : def;
}

bool FissionFeature::flag(const QString &key, bool def) const {
    return m_props.contains(key) ? m_props.value(key).toBool() : def;
}

int FissionFeature::integer(const QString &key, int def) const {
    return m_props.contains(key) ? m_props.value(key).toInt() : def;
}

bool FissionFeature::number(RebuildContext &ctx, const QString &key, double &out) {
    const QVariant v = m_props.value(key);
    if (!v.isValid()) return fail(QString("Missing value for '%1'").arg(key));
    QString err;
    out = ctx.evaluate(v.toString(), &err);
    if (!err.isEmpty()) return fail(QString("%1: %2").arg(key, err));
    return true;
}

FeatureOperation FissionFeature::operation() const {
    return operationFromString(str("operation", "join"));
}

QJsonObject FissionFeature::toJson() const {
    QJsonObject obj;
    obj["id"] = m_id;
    obj["type"] = type();
    obj["name"] = m_name;
    obj["suppressed"] = m_suppressed;
    obj["visible"] = m_visible;
    obj["props"] = QJsonObject::fromVariantMap(m_props);
    return obj;
}

FissionFeature *FissionFeature::create(const QString &type, QObject *parent) {
    FissionFeature *f = nullptr;
    if (type == "Sketch") f = new FissionSketch(parent);
    else if (type == "Extrude") f = new FissionExtrude(parent);
    else if (type == "Revolve") f = new FissionRevolve(parent);
    else if (type == "Box") f = new FissionBox(parent);
    else if (type == "Cylinder") f = new FissionCylinder(parent);
    else if (type == "Sphere") f = new FissionSphere(parent);
    else if (type == "Torus") f = new FissionTorus(parent);
    else if (type == "Coil") f = new FissionCoil(parent);
    else if (type == "Pipe") f = new FissionPipe(parent);
    else if (type == "Fillet") f = new FissionFillet(parent);
    else if (type == "Chamfer") f = new FissionChamfer(parent);
    else if (type == "Loft") f = new FissionLoft(parent);
    else if (type == "Sweep") f = new FissionSweep(parent);
    else if (type == "Shell") f = new FissionShell(parent);
    else if (type == "Mirror") f = new FissionMirror(parent);
    else if (type == "RectangularPattern") f = new FissionRectangularPattern(parent);
    else if (type == "CircularPattern") f = new FissionCircularPattern(parent);
    else if (type == "ConstructionPlane") f = new FissionConstructionPlane(parent);
    else if (type == "ConstructionAxis") f = new FissionConstructionAxis(parent);
    else if (type == "ConstructionPoint") f = new FissionConstructionPoint(parent);
    else if (type == "Hole") f = new FissionHole(parent);
    else if (type == "Thread") f = new FissionThread(parent);
    else if (type == "Combine") f = new FissionCombine(parent);
    else if (type == "Move") f = new FissionMove(parent);
    else if (type == "Scale") f = new FissionScale(parent);
    else if (type == "SplitBody") f = new FissionSplitBody(parent);
    else if (type == "SplitFace") f = new FissionSplitFace(parent);
    else if (type == "OffsetFace") f = new FissionOffsetFace(parent);
    else if (type == "Offset") f = new FissionOffset(parent);
    else if (type == "ReplaceFace") f = new FissionReplaceFace(parent);
    else if (type == "Draft") f = new FissionDraft(parent);
    else if (type == "Joint") f = new FissionJoint(parent);
    else if (type == "PathPattern") f = new FissionPathPattern(parent);
    else if (type == "Rib") f = new FissionRib(parent);
    else if (type == "Web") f = new FissionWeb(parent);
    else if (type == "Remove") f = new FissionRemove(parent);
    else if (type == "DeleteFace") f = new FissionDeleteFace(parent);
    else if (type == "InsertMesh") f = new FissionInsertMesh(parent);
    else if (type == "MeshConvert") f = new FissionMeshConvert(parent);
    else if (type == "MeshReverse") f = new FissionMeshReverse(parent);
    else if (type == "MeshSmooth") f = new FissionMeshSmooth(parent);
    else if (type == "MeshReduce") f = new FissionMeshReduce(parent);
    else if (type == "MeshPlaneCut") f = new FissionMeshPlaneCut(parent);
    else if (type == "MeshRepair") f = new FissionMeshRepair(parent);
    else if (type == "MeshShell") f = new FissionMeshShell(parent);
    else if (type == "MeshFaceGroups") f = new FissionMeshFaceGroups(parent);
    else if (type == "MeshRemesh") f = new FissionMeshRemesh(parent);
    else if (type == "MeshCombine") f = new FissionMeshCombine(parent);
    else if (type == "ImportCAD") f = new FissionImportCAD(parent);
    else if (type == "BaseFlange") f = new FissionBaseFlange(parent);
    else if (type == "EdgeFlange") f = new FissionEdgeFlange(parent);
    else if (type == "FlatPattern") f = new FissionFlatPattern(parent);
    else if (type == "Hem") f = new FissionHem(parent);
    else if (type == "Tab") f = new FissionTab(parent);
    else if (type == "Bend") f = new FissionBend(parent);
    else if (type == "ContourFlange") f = new FissionContourFlange(parent);
    else if (type == "Unfold") f = new FissionUnfold(parent);
    else if (type == "Refold") f = new FissionRefold(parent);
    if (f) f->m_props = f->defaults();
    return f;
}

FissionFeature *FissionFeature::fromJson(const QJsonObject &obj, QObject *parent) {
    FissionFeature *f = create(obj["type"].toString(), parent);
    if (!f) return nullptr;
    if (obj.contains("id")) f->m_id = obj["id"].toString();
    f->m_name = obj["name"].toString(f->type());
    f->m_suppressed = obj["suppressed"].toBool(false);
    f->m_visible = obj["visible"].toBool(true);
    QVariantMap props = f->defaults();
    const QVariantMap stored = obj["props"].toObject().toVariantMap();
    for (auto it = stored.begin(); it != stored.end(); ++it) props.insert(it.key(), it.value());
    // Moves saved before the move types existed rotate about their axis, then translate
    if (f->type() == "Move" && !stored.contains("moveType")) props["moveType"] = "";
    f->m_props = props;
    return f;
}

QStringList FissionFeature::types() {
    return {"Sketch", "Extrude", "Revolve", "Box", "Cylinder", "Sphere", "Torus", "Coil", "Pipe",
            "Fillet", "Chamfer", "Loft", "Sweep", "Shell", "Mirror",
            "RectangularPattern", "CircularPattern", "ConstructionPlane", "ConstructionAxis", "ConstructionPoint",
            "Hole", "Thread", "Combine", "Move", "Scale", "SplitBody", "SplitFace", "OffsetFace", "Offset",
            "ReplaceFace", "Draft", "Joint", "PathPattern", "Rib", "Web", "Remove", "DeleteFace", "InsertMesh", "MeshConvert", "MeshReverse",
            "MeshSmooth", "MeshReduce", "MeshPlaneCut", "MeshRepair", "MeshShell", "MeshCombine", "MeshFaceGroups", "MeshRemesh", "ImportCAD", "BaseFlange", "EdgeFlange", "FlatPattern", "Hem", "Tab", "Bend", "ContourFlange", "Unfold", "Refold"};
}
