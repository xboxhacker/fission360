// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#include "FissionBody.h"
#include "GeomUtils.h"
#include "MeshUtils.h"
#include "render/AppearanceLibrary.h"

#include <QElapsedTimer>
#include <BRepGProp.hxx>
#include <Bnd_Box.hxx>
#include <GProp_GProps.hxx>
#include <algorithm>

void FissionBody::setShape(const QString &name, const TopoDS_Shape &shape) {
    // Rebuilds hand back the same shape for bodies they did not touch; keep what was computed for it
    if (name == m_name && shape.IsEqual(m_shape)) return;
    m_name = name;
    m_shape = shape;
    clearCaches();
    emit shapeChanged();
}

void FissionBody::clearCaches() {
    m_volume = m_area = -1;
    m_faceCount = m_edgeCount = m_isMesh = m_boxState = -1;
    m_meshStats.clear();
    m_hasGeometry = false;
}

void FissionBody::setVisible(bool v) {
    if (m_visible != v) { m_visible = v; emit visibilityChanged(); }
}

void FissionBody::setColor(const QColor &c) {
    if (m_color != c) { m_color = c; emit visibilityChanged(); }
}

void FissionBody::setOpacity(double o) {
    o = std::clamp(o, 0.05, 1.0);
    if (!qFuzzyCompare(m_opacity, o)) { m_opacity = o; emit visibilityChanged(); }
}

void FissionBody::setAppearanceId(const QString &id) {
    if (m_appearanceId == id) return;
    m_appearanceId = id;
    emit appearanceChanged();
    emit visibilityChanged();
}

void FissionBody::applyAppearance(const QString &id) {
    const QVariantMap m = AppearanceLibrary::instance()->appearance(id);
    if (m.isEmpty()) return;
    m_appearanceId = m["id"].toString();
    m_color = QColor(m["color"].toString());
    m_roughness = m.contains("roughness") ? m["roughness"].toDouble() : 0.55;
    m_metalness = m.contains("metalness") ? m["metalness"].toDouble() : 0.0;
    m_opacity = std::clamp(m.contains("opacity") ? m["opacity"].toDouble() : 1.0, 0.05, 1.0);
    emit appearanceChanged();
    emit visibilityChanged();
}

void FissionBody::setRoughness(double r) {
    r = std::clamp(r, 0.0, 1.0);
    if (qFuzzyCompare(m_roughness, r)) return;
    m_roughness = r;
    m_appearanceId.clear();   // custom values leave the library id
    emit appearanceChanged();
    emit visibilityChanged();
}

void FissionBody::setMetalness(double m) {
    m = std::clamp(m, 0.0, 1.0);
    if (qFuzzyCompare(m_metalness, m)) return;
    m_metalness = m;
    m_appearanceId.clear();
    emit appearanceChanged();
    emit visibilityChanged();
}

void FissionBody::setTexturePath(const QString &path) {
    if (m_texturePath == path) return;
    m_texturePath = path;
    emit appearanceChanged();
    emit visibilityChanged();
}

void FissionBody::setRoughnessPath(const QString &path) {
    if (m_roughnessPath == path) return;
    m_roughnessPath = path;
    emit appearanceChanged();
    emit visibilityChanged();
}

void FissionBody::setMetalnessPath(const QString &path) {
    if (m_metalnessPath == path) return;
    m_metalnessPath = path;
    emit appearanceChanged();
    emit visibilityChanged();
}

void FissionBody::setNormalPath(const QString &path) {
    if (m_normalPath == path) return;
    m_normalPath = path;
    emit appearanceChanged();
    emit visibilityChanged();
}

void FissionBody::applyAppearanceToFace(int faceIndex, const QString &id) {
    if (faceIndex < 0) return;
    const QVariantMap m = AppearanceLibrary::instance()->appearance(id);
    if (m.isEmpty()) return;
    QVariantMap ov;
    ov["appearanceId"] = m["id"];
    ov["color"] = m["color"];
    ov["roughness"] = m.contains("roughness") ? m["roughness"] : 0.55;
    ov["metalness"] = m.contains("metalness") ? m["metalness"] : 0.0;
    ov["opacity"] = m.contains("opacity") ? m["opacity"] : 1.0;
    m_faceAppearances.insert(faceIndex, ov);
    emit appearanceChanged();
    emit visibilityChanged();
}

void FissionBody::setFaceMaps(int faceIndex, const QString &albedo, const QString &roughness,
                              const QString &metalness, const QString &normal) {
    if (faceIndex < 0) return;
    QVariantMap ov = m_faceAppearances.value(faceIndex);
    if (!albedo.isEmpty()) ov["texturePath"] = albedo;
    if (!roughness.isEmpty()) ov["roughnessPath"] = roughness;
    else if (ov.contains("roughnessPath") && roughness.isNull()) { /* keep */ }
    if (!metalness.isEmpty()) ov["metalnessPath"] = metalness;
    if (!normal.isEmpty()) ov["normalPath"] = normal;
    // Always write paths (including clears when empty string passed for body-level sync)
    if (albedo.isEmpty() && ov.contains("texturePath") == false) {
        // no-op if nothing to store and no existing override
        if (ov.isEmpty()) return;
    }
    m_faceAppearances.insert(faceIndex, ov);
    emit appearanceChanged();
    emit visibilityChanged();
}

void FissionBody::clearFaceAppearance(int faceIndex) {
    if (!m_faceAppearances.remove(faceIndex)) return;
    emit appearanceChanged();
    emit visibilityChanged();
}

void FissionBody::clearAllFaceAppearances() {
    if (m_faceAppearances.isEmpty()) return;
    m_faceAppearances.clear();
    emit appearanceChanged();
    emit visibilityChanged();
}

QVariantMap FissionBody::faceAppearance(int faceIndex) const {
    return m_faceAppearances.value(faceIndex);
}

QVariantMap FissionBody::faceAppearancesMap() const {
    QVariantMap out;
    for (auto it = m_faceAppearances.cbegin(); it != m_faceAppearances.cend(); ++it)
        out.insert(QString::number(it.key()), it.value());
    return out;
}

void FissionBody::setFaceAppearancesFromJson(const QVariantMap &map) {
    m_faceAppearances.clear();
    for (auto it = map.cbegin(); it != map.cend(); ++it) {
        bool ok = false;
        const int idx = it.key().toInt(&ok);
        if (ok && idx >= 0) m_faceAppearances.insert(idx, it.value().toMap());
    }
}

FissionBody::EffectiveLook FissionBody::effectiveLook(int faceIndex) const {
    EffectiveLook look;
    look.color = m_color;
    look.roughness = m_roughness;
    look.metalness = m_metalness;
    look.opacity = m_opacity;
    look.texturePath = m_texturePath;
    look.roughnessPath = m_roughnessPath;
    look.metalnessPath = m_metalnessPath;
    look.normalPath = m_normalPath;
    const QVariantMap ov = m_faceAppearances.value(faceIndex);
    if (ov.isEmpty()) return look;
    if (ov.contains("color")) look.color = QColor(ov["color"].toString());
    if (ov.contains("roughness")) look.roughness = ov["roughness"].toDouble();
    if (ov.contains("metalness")) look.metalness = ov["metalness"].toDouble();
    if (ov.contains("opacity")) look.opacity = ov["opacity"].toDouble();
    if (ov.contains("texturePath")) look.texturePath = ov["texturePath"].toString();
    if (ov.contains("roughnessPath")) look.roughnessPath = ov["roughnessPath"].toString();
    if (ov.contains("metalnessPath")) look.metalnessPath = ov["metalnessPath"].toString();
    if (ov.contains("normalPath")) look.normalPath = ov["normalPath"].toString();
    return look;
}

bool FissionBody::isMesh() const {
    if (m_isMesh < 0) m_isMesh = MeshUtils::isMesh(m_shape) ? 1 : 0;
    return m_isMesh == 1;
}

QVariantMap FissionBody::meshStats() const {
    if (!m_meshStats.isEmpty() || !isMesh()) return m_meshStats;
    const MeshUtils::Stats s = MeshUtils::stats(MeshUtils::fromShape(m_shape));
    m_meshStats = {{"nodes", s.nodes},         {"triangles", s.triangles}, {"groups", s.groups},
                   {"openEdges", s.openEdges}, {"nonManifoldEdges", s.nonManifoldEdges},
                   {"pieces", s.pieces},       {"closed", s.closed},       {"oriented", s.oriented},
                   {"area", s.area},           {"volume", s.volume},       {"minEdge", s.minEdge},
                   {"maxEdge", s.maxEdge}};
    return m_meshStats;
}

double FissionBody::volume() const {
    if (m_volume < 0) m_volume = GeomUtils::volume(m_shape);
    return m_volume;
}

double FissionBody::area() const {
    if (m_area < 0) {
        QElapsedTimer timer;
        timer.start();
        m_area = GeomUtils::area(m_shape);
        if (timer.elapsed() > 50) qInfo("body %s: area took %lld ms", qPrintable(m_name), timer.elapsed());
    }
    return m_area;
}

int FissionBody::faceCount() const {
    if (m_faceCount < 0) m_faceCount = GeomUtils::faceMap(m_shape).Extent();
    return m_faceCount;
}

int FissionBody::edgeCount() const {
    if (m_edgeCount < 0) m_edgeCount = GeomUtils::edgeMap(m_shape).Extent();
    return m_edgeCount;
}

bool FissionBody::box(double out[6]) const {
    if (m_boxState < 0) {
        const Bnd_Box box = GeomUtils::boundingBox(m_shape);
        m_boxState = box.IsVoid() ? 0 : 1;
        if (m_boxState) box.Get(m_box[0], m_box[1], m_box[2], m_box[3], m_box[4], m_box[5]);
    }
    std::copy(m_box, m_box + 6, out);
    return m_boxState == 1;
}

const FissionBody::Geometry &FissionBody::geometry() const {
    if (m_hasGeometry || m_shape.IsNull()) return m_geometry;
    QElapsedTimer timer;
    timer.start();
    Geometry g;
    gp_Pnt c;
    if (isMesh()) {
        c = GeomUtils::shapeCentroid(m_shape);
        g.volume = volume();
    } else {
        const GeomUtils::MassProperties mp = GeomUtils::massProperties(m_shape);
        c = mp.centroid;
        g.volume = mp.volume;
        if (m_area < 0) m_area = mp.area;
    }
    g.centroid[0] = c.X();
    g.centroid[1] = c.Y();
    g.centroid[2] = c.Z();
    g.hasBox = box(g.box);
    m_geometry = g;
    m_hasGeometry = true;
    if (timer.elapsed() > 50) qInfo("body %s: mass properties took %lld ms", qPrintable(m_name), timer.elapsed());
    return m_geometry;
}
