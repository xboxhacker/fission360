// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#pragma once
#include <QObject>
#include <QHash>
#include <QVariantList>
#include <QVariantMap>
#include <QColor>
#include "Material.h"

// Built-in body materials (Fusion-style Appearance library). Values are hand-tuned PBR
// presets — no textures required. Catalog entries point at CC0 sources (Poly Haven /
// ambientCG) for optional texture download later.
class AppearanceLibrary : public QObject {
    Q_OBJECT
public:
    explicit AppearanceLibrary(QObject *parent = nullptr);

    // [{id, name, type, color, roughness, metalness, opacity, source}]
    Q_INVOKABLE QVariantList appearances() const { return m_list; }
    Q_INVOKABLE QVariantMap appearance(const QString &id) const;
    Q_INVOKABLE Appearance *makeAppearance(const QString &id, QObject *parent = nullptr) const;
    // Apply a library entry onto an existing Appearance object
    Q_INVOKABLE bool applyTo(Appearance *target, const QString &id) const;

    static AppearanceLibrary *instance();

private:
    void add(const QString &id, const QString &name, Appearance::Type type,
             const QColor &color, double roughness, double metalness, double opacity,
             const QString &source = QString());
    QVariantList m_list;
    QHash<QString, QVariantMap> m_byId;
};
