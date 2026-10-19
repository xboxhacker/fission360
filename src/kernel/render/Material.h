// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#pragma once
#include <QObject>
#include <QString>
#include <QColor>
#include <QVector3D>
#include <QVariantMap>
#include <QQmlEngine>

// Fusion API equivalent: adsk::core::Appearance
class Appearance : public QObject {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QString id READ id CONSTANT)
    Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)
    Q_PROPERTY(QString appearanceType READ appearanceType CONSTANT)
    Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY colorChanged)
    Q_PROPERTY(double roughness READ roughness WRITE setRoughness NOTIFY roughnessChanged)
    Q_PROPERTY(double metalness READ metalness WRITE setMetalness NOTIFY metalnessChanged)
    Q_PROPERTY(double opacity READ opacity WRITE setOpacity NOTIFY opacityChanged)
    Q_PROPERTY(bool hasTexture READ hasTexture NOTIFY textureChanged)
    Q_PROPERTY(QString texturePath READ texturePath WRITE setTexturePath NOTIFY textureChanged)
    Q_PROPERTY(QString roughnessPath READ roughnessPath WRITE setRoughnessPath NOTIFY textureChanged)
    Q_PROPERTY(QString metalnessPath READ metalnessPath WRITE setMetalnessPath NOTIFY textureChanged)
    Q_PROPERTY(QString normalPath READ normalPath WRITE setNormalPath NOTIFY textureChanged)
public:
    enum Type { Simple, Glass, Metallic, Plastic, Ceramic, Wood, Stone, Fabric, Custom };
    Q_ENUM(Type)

    explicit Appearance(const QString &id, const QString &name, Type type = Custom, QObject *parent = nullptr);
    ~Appearance() = default;

    QString id() const { return m_id; }
    QString name() const { return m_name; }
    void setName(const QString &n);
    QString appearanceType() const;
    Type type() const { return m_type; }
    void setType(Type t);

    QColor color() const { return m_color; }
    void setColor(const QColor &c);
    double roughness() const { return m_roughness; }
    void setRoughness(double r);
    double metalness() const { return m_metalness; }
    void setMetalness(double m);
    double opacity() const { return m_opacity; }
    void setOpacity(double o);

    bool hasTexture() const { return !m_texturePath.isEmpty(); }
    QString texturePath() const { return m_texturePath; }
    void setTexturePath(const QString &path);
    QString roughnessPath() const { return m_roughnessPath; }
    void setRoughnessPath(const QString &path);
    QString metalnessPath() const { return m_metalnessPath; }
    void setMetalnessPath(const QString &path);
    QString normalPath() const { return m_normalPath; }
    void setNormalPath(const QString &path);

    Q_INVOKABLE QVariantMap toJson() const;
    Q_INVOKABLE static Appearance *fromJson(const QVariantMap &json, QObject *parent = nullptr);

signals:
    void nameChanged();
    void colorChanged();
    void roughnessChanged();
    void metalnessChanged();
    void opacityChanged();
    void textureChanged();

private:
    QString m_id;
    QString m_name;
    Type m_type = Custom;
    QColor m_color = Qt::lightGray;
    double m_roughness = 0.5;
    double m_metalness = 0.0;
    double m_opacity = 1.0;
    QString m_texturePath;
    QString m_roughnessPath;
    QString m_metalnessPath;
    QString m_normalPath;
};