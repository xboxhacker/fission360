// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#pragma once
#include <QString>
#include <QVector>
#include <QVector3D>
#include <QImage>

// Radiance RGBE (.hdr) loader for Poly Haven equirectangular environments.
struct HdrImage {
    int width = 0;
    int height = 0;
    QVector<float> rgb;   // width*height*3 linear floats

    bool isNull() const { return width <= 0 || height <= 0 || rgb.isEmpty(); }
    bool load(const QString &path);
    // Sample the equirectangular map with a direction (Y-up)
    QVector3D sample(const QVector3D &dir) const;
    // Tone-mapped 8-bit preview (Reinhard) for OpenGL RGB8 textures / thumbnails
    QImage toToneMappedImage(float exposure = 1.0f) const;
};
