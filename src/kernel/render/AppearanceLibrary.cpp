// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#include "AppearanceLibrary.h"

AppearanceLibrary *AppearanceLibrary::instance() {
    static AppearanceLibrary lib;
    return &lib;
}

AppearanceLibrary::AppearanceLibrary(QObject *parent) : QObject(parent) {
    // CAD default
    add("default", "CAD Grey", Appearance::Simple, QColor(170, 178, 189), 0.55, 0.0, 1.0);

    // Plastics
    add("plastic_white", "Plastic White", Appearance::Plastic, QColor(245, 245, 245), 0.45, 0.0, 1.0);
    add("plastic_black", "Plastic Black", Appearance::Plastic, QColor(32, 32, 34), 0.40, 0.0, 1.0);
    add("plastic_red", "Plastic Red", Appearance::Plastic, QColor(196, 48, 48), 0.42, 0.0, 1.0);
    add("plastic_blue", "Plastic Blue", Appearance::Plastic, QColor(48, 98, 186), 0.42, 0.0, 1.0);
    add("plastic_green", "Plastic Green", Appearance::Plastic, QColor(52, 148, 72), 0.42, 0.0, 1.0);
    add("abs_orange", "ABS Orange", Appearance::Plastic, QColor(230, 120, 40), 0.50, 0.0, 1.0);

    // Metals
    add("aluminum", "Aluminum", Appearance::Metallic, QColor(198, 202, 206), 0.28, 0.95, 1.0);
    add("steel", "Steel", Appearance::Metallic, QColor(150, 154, 160), 0.35, 0.90, 1.0);
    add("stainless", "Stainless Steel", Appearance::Metallic, QColor(190, 194, 200), 0.22, 0.95, 1.0);
    add("brass", "Brass", Appearance::Metallic, QColor(196, 160, 64), 0.30, 0.92, 1.0);
    add("copper", "Copper", Appearance::Metallic, QColor(184, 98, 58), 0.32, 0.95, 1.0);
    add("chrome", "Chrome", Appearance::Metallic, QColor(230, 232, 236), 0.08, 1.0, 1.0);
    add("gold", "Gold", Appearance::Metallic, QColor(212, 175, 55), 0.20, 1.0, 1.0);

    // Glass / translucent
    add("glass_clear", "Glass Clear", Appearance::Glass, QColor(220, 235, 245), 0.05, 0.0, 0.22);
    add("glass_tinted", "Glass Tinted", Appearance::Glass, QColor(80, 120, 140), 0.08, 0.0, 0.35);
    add("acrylic", "Acrylic", Appearance::Glass, QColor(230, 240, 250), 0.12, 0.0, 0.40);

    // Ceramic / rubber / fabric / wood / stone (solid-colour approximations of CC0 PBR looks)
    add("ceramic_white", "Ceramic White", Appearance::Ceramic, QColor(248, 248, 250), 0.35, 0.0, 1.0);
    add("rubber_black", "Rubber Black", Appearance::Custom, QColor(28, 28, 30), 0.85, 0.0, 1.0);
    add("fabric_grey", "Fabric Grey", Appearance::Fabric, QColor(120, 122, 128), 0.90, 0.0, 1.0);
    add("wood_oak", "Wood Oak", Appearance::Wood, QColor(164, 116, 60), 0.65, 0.0, 1.0,
        QStringLiteral("Inspired by ambientCG WoodFloor / Poly Haven wood (CC0)"));
    add("concrete", "Concrete", Appearance::Stone, QColor(150, 148, 142), 0.80, 0.0, 1.0,
        QStringLiteral("Inspired by ambientCG Concrete / Poly Haven (CC0)"));
    add("marble", "Marble", Appearance::Stone, QColor(232, 228, 222), 0.25, 0.0, 1.0,
        QStringLiteral("Inspired by ambientCG Marble / Poly Haven (CC0)"));
}

void AppearanceLibrary::add(const QString &id, const QString &name, Appearance::Type type,
                            const QColor &color, double roughness, double metalness, double opacity,
                            const QString &source) {
    static const char *typeNames[] = {"Simple", "Glass", "Metallic", "Plastic", "Ceramic",
                                      "Wood", "Stone", "Fabric", "Custom"};
    const int ti = int(type);
    QVariantMap m{{"id", id},
                  {"name", name},
                  {"type", typeNames[ti >= 0 && ti < 9 ? ti : 8]},
                  {"typeId", ti},
                  {"color", color.name(QColor::HexRgb)},
                  {"roughness", roughness},
                  {"metalness", metalness},
                  {"opacity", opacity}};
    if (!source.isEmpty()) m["source"] = source;
    m_list.append(m);
    m_byId.insert(id, m);
}

QVariantMap AppearanceLibrary::appearance(const QString &id) const {
    return m_byId.value(id.isEmpty() ? QStringLiteral("default") : id);
}

Appearance *AppearanceLibrary::makeAppearance(const QString &id, QObject *parent) const {
    const QVariantMap m = appearance(id);
    if (m.isEmpty()) return nullptr;
    auto *a = new Appearance(m["id"].toString(), m["name"].toString(),
                             Appearance::Type(m["typeId"].toInt()), parent);
    a->setColor(QColor(m["color"].toString()));
    a->setRoughness(m["roughness"].toDouble());
    a->setMetalness(m["metalness"].toDouble());
    a->setOpacity(m["opacity"].toDouble());
    return a;
}

bool AppearanceLibrary::applyTo(Appearance *target, const QString &id) const {
    if (!target) return false;
    const QVariantMap m = appearance(id);
    if (m.isEmpty()) return false;
    target->setName(m["name"].toString());
    target->setType(Appearance::Type(m["typeId"].toInt()));
    target->setColor(QColor(m["color"].toString()));
    target->setRoughness(m["roughness"].toDouble());
    target->setMetalness(m["metalness"].toDouble());
    target->setOpacity(m["opacity"].toDouble());
    return true;
}
