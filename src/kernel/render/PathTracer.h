// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#pragma once
#include <QColor>
#include <QImage>
#include <QString>
#include <QVector3D>
#include <functional>

class FissionDocument;

// CPU Monte Carlo path tracer for offline Render (separate from viewport supersample).
class PathTracer {
public:
    struct Camera {
        QVector3D eye;
        QVector3D target;
        QVector3D up{0, 0, 1};
        double fovDeg = 30.0;
        bool ortho = false;
    };
    struct Settings {
        int width = 640;
        int height = 360;
        int samplesPerPixel = 32;
        int maxBounces = 4;
        QString environmentPath;
        double environmentIntensity = 1.0;
        double environmentRotation = 0.0;
        QVector3D sunDir{0.4f, 0.3f, 0.85f};
        QVector3D sunColor{1, 1, 1};
        float sunIntensity = 1.0f;
        QColor background{240, 240, 245};
        bool useSun = true;
    };
    using ProgressFn = std::function<void(double /*0..1*/)>;

    // Quality 0..3 → spp
    static int samplesForQuality(int quality);
    static QImage render(FissionDocument *doc, const Camera &cam, const Settings &settings,
                         ProgressFn progress = {});
};
