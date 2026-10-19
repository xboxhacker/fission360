// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#include "UiTestDriver.h"
#include "util/GifWriter.h"

#include <QFileInfo>
#include <QImage>
#include <QVector>

bool UiTestDriver::exportGif(const QString &fileName, const QVariantList &names, int delayCs, int maxWidth) {
    if (m_dir.isEmpty()) return false;
    QStringList baseNames;
    if (names.isEmpty()) {
        const QDir dir(m_dir);
        for (const QFileInfo &fi : dir.entryInfoList({QStringLiteral("*.png")}, QDir::Files, QDir::Name))
            baseNames.append(fi.completeBaseName());
    } else {
        for (const QVariant &v : names)
            baseNames.append(v.toString());
    }
    QVector<QImage> frames;
    frames.reserve(baseNames.size());
    for (const QString &base : baseNames) {
        const QString path = QDir(m_dir).filePath(base.endsWith(QLatin1String(".png")) ? base : base + QStringLiteral(".png"));
        QImage img(path);
        if (!img.isNull()) frames.append(img);
    }
    if (frames.isEmpty()) {
        std::printf("[GIF] no frames in %s\n", qPrintable(m_dir));
        std::fflush(stdout);
        return false;
    }
    const QString out = QDir(m_dir).filePath(fileName.isEmpty() ? QStringLiteral("walkthrough.gif") : fileName);
    const bool ok = GifWriter::write(out, frames, delayCs, maxWidth);
    std::printf("[GIF] %s%s (%d frames)\n", qPrintable(out), ok ? "" : " (FAILED)", frames.size());
    std::fflush(stdout);
    return ok;
}
