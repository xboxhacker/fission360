// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#include "Material.h"
#include <QUuid>

Appearance::Appearance(const QString &id, const QString &name, Type type, QObject *parent)
    : QObject(parent)
    , m_id(id.isEmpty() ? QUuid::createUuid().toString(QUuid::WithoutBraces) : id)
    , m_name(name)
    , m_type(type)
{
}

void Appearance::setName(const QString &n) {
    if (m_name != n) { m_name = n; emit nameChanged(); }
}

QString Appearance::appearanceType() const {
    static const char *names[] = {"Simple","Glass","Metallic","Plastic","Ceramic","Wood","Stone","Fabric","Custom"};
    return (int)m_type < 9 ? names[(int)m_type] : "Custom";
}

void Appearance::setType(Type t) { m_type = t; }

void Appearance::setColor(const QColor &c) {
    if (m_color != c) { m_color = c; emit colorChanged(); }
}

void Appearance::setRoughness(double r) {
    r = qBound(0.0, r, 1.0);
    if (!qFuzzyCompare(m_roughness, r)) { m_roughness = r; emit roughnessChanged(); }
}

void Appearance::setMetalness(double m) {
    m = qBound(0.0, m, 1.0);
    if (!qFuzzyCompare(m_metalness, m)) { m_metalness = m; emit metalnessChanged(); }
}

void Appearance::setOpacity(double o) {
    o = qBound(0.0, o, 1.0);
    if (!qFuzzyCompare(m_opacity, o)) { m_opacity = o; emit opacityChanged(); }
}

void Appearance::setTexturePath(const QString &path) {
    if (m_texturePath != path) { m_texturePath = path; emit textureChanged(); }
}

void Appearance::setRoughnessPath(const QString &path) {
    if (m_roughnessPath != path) { m_roughnessPath = path; emit textureChanged(); }
}

void Appearance::setMetalnessPath(const QString &path) {
    if (m_metalnessPath != path) { m_metalnessPath = path; emit textureChanged(); }
}

void Appearance::setNormalPath(const QString &path) {
    if (m_normalPath != path) { m_normalPath = path; emit textureChanged(); }
}

QVariantMap Appearance::toJson() const {
    QVariantMap j;
    j["id"] = m_id;
    j["name"] = m_name;
    j["type"] = (int)m_type;
    j["color"] = m_color.name(QColor::HexArgb);
    j["roughness"] = m_roughness;
    j["metalness"] = m_metalness;
    j["opacity"] = m_opacity;
    if (!m_texturePath.isEmpty()) j["texturePath"] = m_texturePath;
    if (!m_roughnessPath.isEmpty()) j["roughnessPath"] = m_roughnessPath;
    if (!m_metalnessPath.isEmpty()) j["metalnessPath"] = m_metalnessPath;
    if (!m_normalPath.isEmpty()) j["normalPath"] = m_normalPath;
    return j;
}

Appearance *Appearance::fromJson(const QVariantMap &json, QObject *parent) {
    auto *a = new Appearance(
        json["id"].toString(),
        json["name"].toString(),
        Type(json.contains("type") ? json["type"].toInt() : int(Custom)), parent);
    a->setColor(QColor(json["color"].toString()));
    a->setRoughness(json["roughness"].toDouble());
    a->setMetalness(json["metalness"].toDouble());
    a->setOpacity(json["opacity"].toDouble());
    if (json.contains("texturePath"))
        a->setTexturePath(json["texturePath"].toString());
    if (json.contains("roughnessPath"))
        a->setRoughnessPath(json["roughnessPath"].toString());
    if (json.contains("metalnessPath"))
        a->setMetalnessPath(json["metalnessPath"].toString());
    if (json.contains("normalPath"))
        a->setNormalPath(json["normalPath"].toString());
    return a;
}