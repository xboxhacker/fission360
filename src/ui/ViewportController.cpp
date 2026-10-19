// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#include "ViewportController.h"
#include <QtMath>
#include <cmath>

ViewportController::ViewportController(QObject *parent) : QObject(parent) {
    m_anim.setDuration(350);
    m_anim.setStartValue(0.0);
    m_anim.setEndValue(1.0);
    m_anim.setEasingCurve(QEasingCurve::InOutCubic);
    connect(&m_anim, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) {
        const double t = v.toDouble();
        m_yaw = m_fromYaw + (m_toYaw - m_fromYaw) * t;
        m_pitch = m_fromPitch + (m_toPitch - m_fromPitch) * t;
        m_distance = m_fromDist + (m_toDist - m_fromDist) * t;
        m_target = m_fromTarget + (m_toTarget - m_fromTarget) * float(t);
        emit changed();
    });
}

void ViewportController::setYaw(double deg) { m_yaw = deg; emit changed(); }
void ViewportController::setPitch(double deg) { m_pitch = qBound(-90.0, deg, 90.0); emit changed(); }
void ViewportController::setDistance(double d) { m_distance = qBound(0.01, d, 1e7); emit changed(); }
void ViewportController::setTarget(const QVector3D &t) { m_target = t; emit changed(); }
void ViewportController::setOrthographic(bool o) { if (m_ortho != o) { m_ortho = o; emit changed(); } }

QVector3D ViewportController::eye() const {
    const double y = qDegreesToRadians(m_yaw), p = qDegreesToRadians(m_pitch);
    const QVector3D dir(std::cos(p) * std::cos(y), std::cos(p) * std::sin(y), std::sin(p));
    return m_target + dir * float(m_distance);
}

QVector3D ViewportController::up() const {
    // Exact orthonormal up vector: no singularity when looking straight down
    const double y = qDegreesToRadians(m_yaw), p = qDegreesToRadians(m_pitch);
    return QVector3D(-std::sin(p) * std::cos(y), -std::sin(p) * std::sin(y), std::cos(p));
}

QMatrix4x4 ViewportController::viewMatrix() const {
    QMatrix4x4 m;
    m.lookAt(eye(), m_target, up());
    return m;
}

QMatrix4x4 ViewportController::rotationMatrix() const {
    QMatrix4x4 m;
    m.lookAt(eye() - m_target, QVector3D(), up());
    return m;
}

QMatrix4x4 ViewportController::projectionMatrix(double aspect) const {
    QMatrix4x4 m;
    const float nearP = float(m_distance * 0.005), farP = float(m_distance * 200.0);
    if (m_ortho) {
        const float h = float(m_distance * std::tan(qDegreesToRadians(m_fov / 2)));
        const float w = h * float(aspect);
        m.ortho(-w, w, -h, h, -farP, farP);
    } else {
        m.perspective(float(m_fov), float(aspect), nearP, farP);
    }
    return m;
}

void ViewportController::orbit(double dx, double dy) {
    m_anim.stop();
    m_yaw -= dx * 0.4;
    m_pitch = qBound(-90.0, m_pitch + dy * 0.4, 90.0);
    emit changed();
}

void ViewportController::pan(double dx, double dy, double viewportHeight) {
    m_anim.stop();
    const double visible = 2.0 * m_distance * std::tan(qDegreesToRadians(m_fov / 2));
    const double scale = visible / std::max(1.0, viewportHeight);
    const QVector3D forward = (m_target - eye()).normalized();
    const QVector3D right = QVector3D::crossProduct(forward, up()).normalized();
    m_target += (-right * float(dx) + up() * float(dy)) * float(scale);
    emit changed();
}

void ViewportController::zoom(double factor) {
    m_anim.stop();
    setDistance(m_distance * factor);
}

void ViewportController::zoomAt(double factor, double mouseX, double mouseY,
                                double viewportWidth, double viewportHeight) {
    m_anim.stop();
    if (factor <= 0 || !std::isfinite(factor)) return;
    const double oldDist = m_distance;
    const double newDist = qBound(0.01, oldDist * factor, 1e7);
    const double used = newDist / oldDist;
    if (std::fabs(used - 1.0) < 1e-12) return;

    const double visible = 2.0 * oldDist * std::tan(qDegreesToRadians(m_fov / 2));
    const double scale = visible / std::max(1.0, viewportHeight);
    const QVector3D forward = (m_target - eye()).normalized();
    const QVector3D right = QVector3D::crossProduct(forward, up()).normalized();
    const double cx = viewportWidth * 0.5, cy = viewportHeight * 0.5;
    const QVector3D offset = right * float((mouseX - cx) * scale) + up() * float((cy - mouseY) * scale);
    m_target += offset * float(1.0 - used);
    m_distance = newDist;
    emit changed();
}

void ViewportController::animateTo(double yaw, double pitch, double distance, const QVector3D &target) {
    m_anim.stop();
    // Take the short way around
    while (yaw - m_yaw > 180) yaw -= 360;
    while (yaw - m_yaw < -180) yaw += 360;
    m_fromYaw = m_yaw; m_fromPitch = m_pitch; m_fromDist = m_distance; m_fromTarget = m_target;
    m_toYaw = yaw; m_toPitch = pitch; m_toDist = distance; m_toTarget = target;
    m_anim.start();
}

void ViewportController::setView(const QString &name, bool animate) {
    double yaw = m_yaw, pitch = m_pitch;
    if (name == "front") { yaw = -90; pitch = 0; }
    else if (name == "back") { yaw = 90; pitch = 0; }
    else if (name == "right") { yaw = 0; pitch = 0; }
    else if (name == "left") { yaw = 180; pitch = 0; }
    else if (name == "top") { yaw = -90; pitch = 90; }
    else if (name == "bottom") { yaw = -90; pitch = -90; }
    else { yaw = -60; pitch = 25; }   // home
    if (animate) {
        animateTo(yaw, pitch, m_distance, m_target);
    } else {
        if (m_anim.state() == QAbstractAnimation::Running) {
            m_anim.stop();
            m_distance = m_toDist; m_target = m_toTarget;
        }
        m_yaw = yaw; m_pitch = pitch;
        emit changed();
    }
}

void ViewportController::lookAlong(const QVector3D &normal, const QVector3D &target, bool animate) {
    const QVector3D n = normal.normalized();
    const double pitch = qRadiansToDegrees(std::asin(qBound(-1.0f, n.z(), 1.0f)));
    const double yaw = std::fabs(n.z()) > 0.999f ? -90.0 : qRadiansToDegrees(std::atan2(n.y(), n.x()));
    if (animate) {
        animateTo(yaw, pitch, m_distance, target);
    } else {
        m_yaw = yaw; m_pitch = pitch; m_target = target;
        emit changed();
    }
}

void ViewportController::fitBox(const QVector3D &min, const QVector3D &max, bool animate) {
    const QVector3D center = (min + max) / 2;
    const double radius = std::max(1.0, double((max - min).length()) / 2);
    const double distance = radius / std::sin(qDegreesToRadians(m_fov / 2)) * 1.15;
    const bool turning = m_anim.state() == QAbstractAnimation::Running;
    if (animate) {
        animateTo(turning ? m_toYaw : m_yaw, turning ? m_toPitch : m_pitch, distance, center);
    } else {
        m_anim.stop();
        if (turning) { m_yaw = m_toYaw; m_pitch = m_toPitch; }
        m_distance = distance; m_target = center;
        emit changed();
    }
}
