// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#include "HdrImage.h"
#include <QFile>
#include <QtMath>
#include <algorithm>
#include <cmath>
#include <cstring>

namespace {

float rgbeToFloat(unsigned char mantissa, int exp) {
    if (exp == 0) return 0.f;
    return std::ldexp(mantissa / 256.0f, exp - 128);
}

bool readHeader(QFile &f, int &width, int &height) {
    QByteArray line = f.readLine();
    if (!line.startsWith("#?RADIANCE") && !line.startsWith("#?RGBE")) return false;
    bool gotFormat = false;
    while (!f.atEnd()) {
        line = f.readLine().trimmed();
        if (line.isEmpty()) break;
        if (line.startsWith("FORMAT=")) gotFormat = line.contains("32-bit_rle_rgbe") || line.contains("32-bit_rle_xyze");
    }
    if (!gotFormat) return false;
    line = f.readLine().trimmed();
    // "-Y height +X width"
    const QList<QByteArray> parts = line.split(' ');
    if (parts.size() < 4) return false;
    height = parts[1].toInt();
    width = parts[3].toInt();
    return width > 0 && height > 0;
}

bool readRleScanline(QFile &f, int width, QByteArray &rgbe) {
    rgbe.resize(width * 4);
    unsigned char header[4];
    if (f.read(reinterpret_cast<char *>(header), 4) != 4) return false;
    if (header[0] != 2 || header[1] != 2 || (header[2] << 8 | header[3]) != width) {
        // Uncompressed: first pixel already in header
        std::memcpy(rgbe.data(), header, 4);
        if (f.read(rgbe.data() + 4, (width - 1) * 4) != (width - 1) * 4) return false;
        return true;
    }
    for (int ch = 0; ch < 4; ++ch) {
        int x = 0;
        while (x < width) {
            unsigned char code = 0;
            if (f.read(reinterpret_cast<char *>(&code), 1) != 1) return false;
            if (code > 128) {
                const int count = code - 128;
                unsigned char val = 0;
                if (f.read(reinterpret_cast<char *>(&val), 1) != 1) return false;
                for (int i = 0; i < count && x < width; ++i, ++x)
                    rgbe[x * 4 + ch] = char(val);
            } else {
                const int count = code;
                for (int i = 0; i < count && x < width; ++i, ++x) {
                    unsigned char val = 0;
                    if (f.read(reinterpret_cast<char *>(&val), 1) != 1) return false;
                    rgbe[x * 4 + ch] = char(val);
                }
            }
        }
    }
    return true;
}

} // namespace

bool HdrImage::load(const QString &path) {
    width = height = 0;
    rgb.clear();
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return false;
    int w = 0, h = 0;
    if (!readHeader(f, w, h)) return false;
    rgb.resize(w * h * 3);
    QByteArray scan;
    for (int y = 0; y < h; ++y) {
        if (!readRleScanline(f, w, scan)) { rgb.clear(); return false; }
        float *dst = rgb.data() + y * w * 3;
        for (int x = 0; x < w; ++x) {
            const unsigned char *p = reinterpret_cast<const unsigned char *>(scan.constData()) + x * 4;
            const int e = p[3];
            dst[x * 3 + 0] = rgbeToFloat(p[0], e);
            dst[x * 3 + 1] = rgbeToFloat(p[1], e);
            dst[x * 3 + 2] = rgbeToFloat(p[2], e);
        }
    }
    width = w;
    height = h;
    return true;
}

QVector3D HdrImage::sample(const QVector3D &dir) const {
    if (isNull()) return QVector3D(0.2f, 0.22f, 0.25f);
    QVector3D d = dir.normalized();
    if (d.lengthSquared() < 1e-8f) d = QVector3D(0, 0, 1);
    constexpr float kPi = 3.14159265358979323846f;
    const float u = 0.5f + std::atan2(d.x(), -d.y()) / (2.0f * kPi);
    const float v = std::acos(std::clamp(d.z(), -1.0f, 1.0f)) / kPi;
    const int x = std::clamp(int(std::floor(u * width)) % width, 0, width - 1);
    const int y = std::clamp(int(std::floor(v * height)), 0, height - 1);
    const float *p = rgb.constData() + (y * width + x) * 3;
    return QVector3D(p[0], p[1], p[2]);
}

QImage HdrImage::toToneMappedImage(float exposure) const {
    if (isNull()) return {};
    QImage img(width, height, QImage::Format_RGB888);
    for (int y = 0; y < height; ++y) {
        uchar *line = img.scanLine(y);
        const float *src = rgb.constData() + y * width * 3;
        for (int x = 0; x < width; ++x) {
            auto tonemap = [&](float c) {
                c = std::max(0.f, c * exposure);
                c = c / (1.0f + c);                  // Reinhard
                c = std::pow(c, 1.0f / 2.2f);        // sRGB
                return uchar(std::clamp(int(c * 255.0f + 0.5f), 0, 255));
            };
            line[x * 3 + 0] = tonemap(src[x * 3 + 0]);
            line[x * 3 + 1] = tonemap(src[x * 3 + 1]);
            line[x * 3 + 2] = tonemap(src[x * 3 + 2]);
        }
    }
    return img;
}
