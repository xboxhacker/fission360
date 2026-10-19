// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#pragma once
#include <QObject>
#include <QString>
#include <QColor>
#include <QVector3D>
#include <QVariantMap>
#include <QQmlEngine>

// Fusion API equivalents: adsk::fusion::RenderManager, Rendering, InCanvasRendering,
//   RenderSceneSettings, RenderEnvironment, RenderEnvironments
class RenderSettings : public QObject {
    Q_OBJECT
    QML_ELEMENT
    // Render quality
    Q_PROPERTY(int renderQuality READ renderQuality WRITE setRenderQuality NOTIFY qualityChanged)
    // Scene
    Q_PROPERTY(QString sceneBackgroundType READ sceneBackgroundType WRITE setSceneBackgroundType NOTIFY sceneChanged)
    Q_PROPERTY(QColor backgroundColor READ backgroundColor WRITE setBackgroundColor NOTIFY sceneChanged)
    Q_PROPERTY(QColor groundColor READ groundColor WRITE setGroundColor NOTIFY sceneChanged)
    Q_PROPERTY(bool showGround READ showGround WRITE setShowGround NOTIFY sceneChanged)
    Q_PROPERTY(bool showGroundShadow READ showGroundShadow WRITE setShowGroundShadow NOTIFY sceneChanged)
    Q_PROPERTY(bool showReflections READ showReflections WRITE setShowReflections NOTIFY sceneChanged)
    // Environment (HDRI)
    Q_PROPERTY(QString environmentPath READ environmentPath WRITE setEnvironmentPath NOTIFY environmentChanged)
    Q_PROPERTY(double environmentIntensity READ environmentIntensity WRITE setEnvironmentIntensity NOTIFY environmentChanged)
    Q_PROPERTY(double environmentRotation READ environmentRotation WRITE setEnvironmentRotation NOTIFY environmentChanged)
    // Lighting
    Q_PROPERTY(double lightIntensity READ lightIntensity WRITE setLightIntensity NOTIFY lightingChanged)
    Q_PROPERTY(double lightAngle READ lightAngle WRITE setLightAngle NOTIFY lightingChanged)
    Q_PROPERTY(double lightAltitude READ lightAltitude WRITE setLightAltitude NOTIFY lightingChanged)
    Q_PROPERTY(QColor lightColor READ lightColor WRITE setLightColor NOTIFY lightingChanged)
    Q_PROPERTY(bool useSunLight READ useSunLight WRITE setUseSunLight NOTIFY lightingChanged)
    // Output
    Q_PROPERTY(int outputWidth READ outputWidth WRITE setOutputWidth NOTIFY outputSizeChanged)
    Q_PROPERTY(int outputHeight READ outputHeight WRITE setOutputHeight NOTIFY outputSizeChanged)
    Q_PROPERTY(QString outputFormat READ outputFormat WRITE setOutputFormat NOTIFY outputFormatChanged)
public:
    explicit RenderSettings(QObject *parent = nullptr);

    // Quality: 0=Low, 1=Medium, 2=High, 3=Ultra
    int renderQuality() const { return m_quality; }
    void setRenderQuality(int q);

    QString sceneBackgroundType() const { return m_bgType; }
    void setSceneBackgroundType(const QString &t); // "solid" | "gradient" | "environment" | "image"
    QColor backgroundColor() const { return m_bgColor; }
    void setBackgroundColor(const QColor &c);
    QColor groundColor() const { return m_groundColor; }
    void setGroundColor(const QColor &c);
    bool showGround() const { return m_showGround; }
    void setShowGround(bool v);
    bool showGroundShadow() const { return m_showShadow; }
    void setShowGroundShadow(bool v);
    bool showReflections() const { return m_showReflections; }
    void setShowReflections(bool v);

    QString environmentPath() const { return m_envPath; }
    void setEnvironmentPath(const QString &p);
    double environmentIntensity() const { return m_envIntensity; }
    void setEnvironmentIntensity(double v);
    double environmentRotation() const { return m_envRotation; }
    void setEnvironmentRotation(double v);

    double lightIntensity() const { return m_lightIntensity; }
    void setLightIntensity(double v);
    double lightAngle() const { return m_lightAngle; }
    void setLightAngle(double v);
    double lightAltitude() const { return m_lightAltitude; }
    void setLightAltitude(double v);
    QColor lightColor() const { return m_lightColor; }
    void setLightColor(const QColor &c);
    bool useSunLight() const { return m_useSun; }
    void setUseSunLight(bool v);

    int outputWidth() const { return m_outW; }
    void setOutputWidth(int w);
    int outputHeight() const { return m_outH; }
    void setOutputHeight(int h);
    QString outputFormat() const { return m_outFmt; }
    void setOutputFormat(const QString &f); // "png" | "tiff" | "bmp" | "jpg"

    Q_INVOKABLE QVariantMap toJson() const;
    Q_INVOKABLE void fromJson(const QVariantMap &json);

signals:
    void qualityChanged();
    void sceneChanged();
    void environmentChanged();
    void lightingChanged();
    void outputSizeChanged();
    void outputFormatChanged();

private:
    int m_quality = 1;
    QString m_bgType = QStringLiteral("solid");
    QColor m_bgColor = Qt::white;
    QColor m_groundColor = QColor(200, 200, 200);
    bool m_showGround = false;
    bool m_showShadow = true;
    bool m_showReflections = false;
    QString m_envPath;
    double m_envIntensity = 1.0;
    double m_envRotation = 0.0;
    double m_lightIntensity = 1.0;
    double m_lightAngle = 45.0;
    double m_lightAltitude = 30.0;
    QColor m_lightColor = Qt::white;
    bool m_useSun = true;
    int m_outW = 1920;
    int m_outH = 1080;
    QString m_outFmt = QStringLiteral("png");
};