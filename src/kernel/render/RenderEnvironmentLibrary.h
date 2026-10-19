// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#pragma once
#include <QObject>
#include <QHash>
#include <QVariantList>
#include <QVariantMap>

// Built-in lighting presets plus a curated Poly Haven (CC0) HDRI catalog.
// HDR files are not shipped in the repo — downloadUrl points at Poly Haven's
// public CDN (1k .hdr). Terms: assets are CC0; credit "Powered by Poly Haven"
// when using their live API / catalog in the UI.
class RenderEnvironmentLibrary : public QObject {
    Q_OBJECT
public:
    explicit RenderEnvironmentLibrary(QObject *parent = nullptr);

    // Built-in light + background presets (no external files)
    Q_INVOKABLE QVariantList presets() const { return m_presets; }
    // Curated Poly Haven HDRIs [{id, name, category, thumbnail, downloadUrl, credit}]
    Q_INVOKABLE QVariantList polyHavenHdris() const { return m_hdris; }
    Q_INVOKABLE QVariantMap environment(const QString &id) const;
    Q_INVOKABLE QString polyHavenCredit() const {
        return QStringLiteral("HDRI environments: Poly Haven (CC0) — https://polyhaven.com");
    }

    static RenderEnvironmentLibrary *instance();

private:
    void addPreset(const QString &id, const QString &name, const QString &bgType,
                   const QColor &bg, double lightIntensity, double lightAngle, double lightAltitude,
                   const QColor &lightColor);
    void addHdri(const QString &id, const QString &name, const QString &category);
    QVariantList m_presets, m_hdris;
    QHash<QString, QVariantMap> m_byId;
};
