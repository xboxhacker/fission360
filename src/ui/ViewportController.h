// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#pragma once
#include <QObject>
#include <QMatrix4x4>
#include <QVector3D>
#include <QVariantAnimation>

// Z-up orbit camera shared by the 3D viewport, ViewCube and picking.
class ViewportController : public QObject {
    Q_OBJECT
    Q_PROPERTY(double yaw READ yaw WRITE setYaw NOTIFY changed)
    Q_PROPERTY(double pitch READ pitch WRITE setPitch NOTIFY changed)
    Q_PROPERTY(double distance READ distance WRITE setDistance NOTIFY changed)
    Q_PROPERTY(QVector3D target READ target WRITE setTarget NOTIFY changed)
    Q_PROPERTY(QVector3D eye READ eye NOTIFY changed)
    Q_PROPERTY(bool orthographic READ orthographic WRITE setOrthographic NOTIFY changed)
    Q_PROPERTY(QMatrix4x4 rotationMatrix READ rotationMatrix NOTIFY changed)

public:
    explicit ViewportController(QObject *parent = nullptr);

    double yaw() const { return m_yaw; }
    void setYaw(double deg);
    double pitch() const { return m_pitch; }
    void setPitch(double deg);
    double distance() const { return m_distance; }
    void setDistance(double d);
    QVector3D target() const { return m_target; }
    void setTarget(const QVector3D &t);
    bool orthographic() const { return m_ortho; }
    void setOrthographic(bool o);

    // Mouse-driven navigation; deltas are in pixels, viewportHeight in pixels
    Q_INVOKABLE void orbit(double dx, double dy);
    Q_INVOKABLE void pan(double dx, double dy, double viewportHeight);
    Q_INVOKABLE void zoom(double factor);
    // Zoom keeping the world point under (mouseX, mouseY) fixed on screen
    Q_INVOKABLE void zoomAt(double factor, double mouseX, double mouseY,
                            double viewportWidth, double viewportHeight);
    Q_INVOKABLE void setView(const QString &name, bool animate = true);
    // Looks against `normal` (camera placed on the +normal side) centred on target
    Q_INVOKABLE void lookAlong(const QVector3D &normal, const QVector3D &target, bool animate = true);
    Q_INVOKABLE void fitBox(const QVector3D &min, const QVector3D &max, bool animate = true);

    QVector3D eye() const;
    QVector3D up() const;
    QMatrix4x4 viewMatrix() const;
    QMatrix4x4 projectionMatrix(double aspect) const;
    QMatrix4x4 rotationMatrix() const;   // view rotation only (for the ViewCube)
    double fieldOfView() const { return m_fov; }

signals:
    void changed();

private:
    void animateTo(double yaw, double pitch, double distance, const QVector3D &target);

    double m_yaw = -60.0;     // degrees around +Z, 0 = looking from +X
    double m_pitch = 25.0;    // degrees above the XY plane
    double m_distance = 250.0;
    QVector3D m_target;
    bool m_ortho = false;
    double m_fov = 30.0;
    QVariantAnimation m_anim;
    double m_fromYaw = 0, m_fromPitch = 0, m_fromDist = 0, m_toYaw = 0, m_toPitch = 0, m_toDist = 0;
    QVector3D m_fromTarget, m_toTarget;
};
