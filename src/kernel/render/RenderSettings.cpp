// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#include "RenderSettings.h"
#include <algorithm>

RenderSettings::RenderSettings(QObject *parent) : QObject(parent) {}

void RenderSettings::setRenderQuality(int q) {
    q = std::clamp(q, 0, 3);
    if (m_quality == q) return;
    m_quality = q;
    emit qualityChanged();
}

void RenderSettings::setSceneBackgroundType(const QString &t) {
    if (m_bgType == t) return;
    m_bgType = t;
    emit sceneChanged();
}

void RenderSettings::setBackgroundColor(const QColor &c) {
    if (m_bgColor == c) return;
    m_bgColor = c;
    emit sceneChanged();
}

void RenderSettings::setGroundColor(const QColor &c) {
    if (m_groundColor == c) return;
    m_groundColor = c;
    emit sceneChanged();
}

void RenderSettings::setShowGround(bool v) {
    if (m_showGround == v) return;
    m_showGround = v;
    emit sceneChanged();
}

void RenderSettings::setShowGroundShadow(bool v) {
    if (m_showShadow == v) return;
    m_showShadow = v;
    emit sceneChanged();
}

void RenderSettings::setShowReflections(bool v) {
    if (m_showReflections == v) return;
    m_showReflections = v;
    emit sceneChanged();
}

void RenderSettings::setEnvironmentPath(const QString &p) {
    if (m_envPath == p) return;
    m_envPath = p;
    emit environmentChanged();
}

void RenderSettings::setEnvironmentIntensity(double v) {
    if (qFuzzyCompare(m_envIntensity, v)) return;
    m_envIntensity = v;
    emit environmentChanged();
}

void RenderSettings::setEnvironmentRotation(double v) {
    if (qFuzzyCompare(m_envRotation, v)) return;
    m_envRotation = v;
    emit environmentChanged();
}

void RenderSettings::setLightIntensity(double v) {
    if (qFuzzyCompare(m_lightIntensity, v)) return;
    m_lightIntensity = v;
    emit lightingChanged();
}

void RenderSettings::setLightAngle(double v) {
    if (qFuzzyCompare(m_lightAngle, v)) return;
    m_lightAngle = v;
    emit lightingChanged();
}

void RenderSettings::setLightAltitude(double v) {
    if (qFuzzyCompare(m_lightAltitude, v)) return;
    m_lightAltitude = v;
    emit lightingChanged();
}

void RenderSettings::setLightColor(const QColor &c) {
    if (m_lightColor == c) return;
    m_lightColor = c;
    emit lightingChanged();
}

void RenderSettings::setUseSunLight(bool v) {
    if (m_useSun == v) return;
    m_useSun = v;
    emit lightingChanged();
}

void RenderSettings::setOutputWidth(int w) {
    if (m_outW == w) return;
    m_outW = w;
    emit outputSizeChanged();
}

void RenderSettings::setOutputHeight(int h) {
    if (m_outH == h) return;
    m_outH = h;
    emit outputSizeChanged();
}

void RenderSettings::setOutputFormat(const QString &f) {
    if (m_outFmt == f) return;
    m_outFmt = f;
    emit outputFormatChanged();
}

QVariantMap RenderSettings::toJson() const {
    return QVariantMap{
        {"quality", m_quality},
        {"bgType", m_bgType},
        {"bgColor", m_bgColor.name(QColor::HexRgb)},
        {"groundColor", m_groundColor.name(QColor::HexRgb)},
        {"showGround", m_showGround},
        {"showShadow", m_showShadow},
        {"showReflections", m_showReflections},
        {"envPath", m_envPath},
        {"envIntensity", m_envIntensity},
        {"envRotation", m_envRotation},
        {"lightIntensity", m_lightIntensity},
        {"lightAngle", m_lightAngle},
        {"lightAltitude", m_lightAltitude},
        {"lightColor", m_lightColor.name(QColor::HexRgb)},
        {"useSun", m_useSun},
        {"outputWidth", m_outW},
        {"outputHeight", m_outH},
        {"outputFormat", m_outFmt},
    };
}

void RenderSettings::fromJson(const QVariantMap &j) {
    auto d = [&](const char *k, double fallback) {
        return j.contains(QLatin1String(k)) ? j[QLatin1String(k)].toDouble() : fallback;
    };
    auto i = [&](const char *k, int fallback) {
        return j.contains(QLatin1String(k)) ? j[QLatin1String(k)].toInt() : fallback;
    };
    auto s = [&](const char *k, const QString &fallback) {
        return j.contains(QLatin1String(k)) ? j[QLatin1String(k)].toString() : fallback;
    };
    auto b = [&](const char *k, bool fallback) {
        return j.contains(QLatin1String(k)) ? j[QLatin1String(k)].toBool() : fallback;
    };
    setRenderQuality(i("quality", 1));
    setSceneBackgroundType(s("bgType", QStringLiteral("solid")));
    setBackgroundColor(QColor(s("bgColor", QStringLiteral("#ffffff"))));
    setGroundColor(QColor(s("groundColor", QStringLiteral("#c8c8c8"))));
    setShowGround(b("showGround", false));
    setShowGroundShadow(b("showShadow", true));
    setShowReflections(b("showReflections", false));
    setEnvironmentPath(s("envPath", QString()));
    setEnvironmentIntensity(d("envIntensity", 1.0));
    setEnvironmentRotation(d("envRotation", 0.0));
    setLightIntensity(d("lightIntensity", 1.0));
    setLightAngle(d("lightAngle", 45.0));
    setLightAltitude(d("lightAltitude", 30.0));
    setLightColor(QColor(s("lightColor", QStringLiteral("#ffffff"))));
    setUseSunLight(b("useSun", true));
    setOutputWidth(i("outputWidth", 1920));
    setOutputHeight(i("outputHeight", 1080));
    setOutputFormat(s("outputFormat", QStringLiteral("png")));
}
