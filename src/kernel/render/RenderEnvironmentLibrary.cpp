// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#include "RenderEnvironmentLibrary.h"
#include <QColor>

RenderEnvironmentLibrary *RenderEnvironmentLibrary::instance() {
    static RenderEnvironmentLibrary lib;
    return &lib;
}

RenderEnvironmentLibrary::RenderEnvironmentLibrary(QObject *parent) : QObject(parent) {
    // Built-in (no download) — drive sun / background until an HDRI is loaded
    addPreset("studio_soft", "Studio Soft", "gradient", QColor(210, 215, 222), 1.1, 35, 50, Qt::white);
    addPreset("studio_bright", "Studio Bright", "solid", QColor(240, 242, 245), 1.6, 45, 55, Qt::white);
    addPreset("warm_key", "Warm Key Light", "gradient", QColor(40, 42, 48), 1.3, 25, 40, QColor(255, 230, 200));
    addPreset("cool_overcast", "Cool Overcast", "solid", QColor(180, 190, 200), 0.85, 0, 75, QColor(210, 220, 235));
    addPreset("product_dark", "Product Dark", "solid", QColor(28, 30, 34), 1.4, 50, 35, Qt::white);

    // Curated Poly Haven CC0 HDRIs (popular, small 1k downloads)
    addHdri("studio_small_09", "Studio Small 09", "studio");
    addHdri("photo_studio_01", "Photo Studio 01", "studio");
    addHdri("photo_studio_loft_hall", "Photo Studio Loft Hall", "studio");
    addHdri("empty_warehouse_01", "Empty Warehouse 01", "indoor");
    addHdri("modern_buildings_2", "Modern Buildings 2", "outdoor");
    addHdri("kloppenheim_06_puresky", "Kloppenheim 06 (Pure Sky)", "outdoor");
    addHdri("rooitou_park", "Rooitou Park", "outdoor");
    addHdri("venice_sunset", "Venice Sunset", "outdoor");
    addHdri("spruit_sunrise", "Spruit Sunrise", "outdoor");
    addHdri("forest_slope", "Forest Slope", "outdoor");
}

void RenderEnvironmentLibrary::addPreset(const QString &id, const QString &name, const QString &bgType,
                                         const QColor &bg, double lightIntensity, double lightAngle,
                                         double lightAltitude, const QColor &lightColor) {
    QVariantMap m{{"id", id},
                  {"name", name},
                  {"kind", "preset"},
                  {"sceneBackgroundType", bgType},
                  {"backgroundColor", bg.name(QColor::HexRgb)},
                  {"lightIntensity", lightIntensity},
                  {"lightAngle", lightAngle},
                  {"lightAltitude", lightAltitude},
                  {"lightColor", lightColor.name(QColor::HexRgb)},
                  {"useSunLight", true}};
    m_presets.append(m);
    m_byId.insert(id, m);
}

void RenderEnvironmentLibrary::addHdri(const QString &id, const QString &name, const QString &category) {
    // 1k HDR is typically a few MB — suitable for a first download. Thumbnails are WebP/PNG from the CDN.
    const QString dl = QStringLiteral("https://dl.polyhaven.org/file/ph-assets/HDRIs/hdr/1k/%1_1k.hdr").arg(id);
    const QString thumb =
        QStringLiteral("https://cdn.polyhaven.com/asset_img/primary/%1.png?width=256&height=144").arg(id);
    QVariantMap m{{"id", id},
                  {"name", name},
                  {"kind", "hdri"},
                  {"category", category},
                  {"thumbnail", thumb},
                  {"downloadUrl", dl},
                  {"pageUrl", QStringLiteral("https://polyhaven.com/a/%1").arg(id)},
                  {"license", "CC0"},
                  {"credit", polyHavenCredit()}};
    m_hdris.append(m);
    m_byId.insert(id, m);
}

QVariantMap RenderEnvironmentLibrary::environment(const QString &id) const {
    return m_byId.value(id);
}
