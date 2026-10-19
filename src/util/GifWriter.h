// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#pragma once
#include <QImage>
#include <QString>
#include <QVector>

// Minimal animated GIF89a writer (256-colour frames). Used by the UI walkthrough export.
// Kept small — no third-party deps — so low-end builds stay lean.
class GifWriter {
public:
    // Write an animated GIF. delayCs = centiseconds per frame (e.g. 40 = 0.4s).
    // Frames are scaled to maxWidth if wider (default 800) to keep files light.
    static bool write(const QString &path, const QVector<QImage> &frames, int delayCs = 40, int maxWidth = 800);
};
