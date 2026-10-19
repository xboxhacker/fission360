// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#include "GifWriter.h"

#include <QFile>
#include <algorithm>
#include <cstring>
#include <vector>

namespace {

// Tiny GIF LZW encoder for indexed 8-bit frames (code size 8).
void lzwEncode(const quint8 *pixels, int count, QByteArray &out) {
    const int minCodeSize = 8;
    out.append(char(minCodeSize));
    const int clear = 1 << minCodeSize;
    const int end = clear + 1;
    int codeSize = minCodeSize + 1;
    int nextCode = end + 1;

    struct Entry { int prefix; int suffix; };
    std::vector<Entry> table;
    table.reserve(4096);

    auto reset = [&] {
        table.clear();
        codeSize = minCodeSize + 1;
        nextCode = end + 1;
    };

    quint32 buf = 0;
    int bits = 0;
    QByteArray block;
    auto flushBlock = [&] {
        if (block.isEmpty()) return;
        out.append(char(block.size()));
        out.append(block);
        block.clear();
    };
    auto writeCode = [&](int code) {
        buf |= quint32(code) << bits;
        bits += codeSize;
        while (bits >= 8) {
            block.append(char(buf & 0xFF));
            buf >>= 8;
            bits -= 8;
            if (block.size() >= 255) flushBlock();
        }
    };

    reset();
    writeCode(clear);
    if (count <= 0) {
        writeCode(end);
        if (bits) block.append(char(buf & 0xFF));
        flushBlock();
        out.append(char(0));
        return;
    }

    int w = pixels[0];
    for (int i = 1; i < count; ++i) {
        const int k = pixels[i];
        int found = -1;
        for (int t = 0; t < int(table.size()); ++t) {
            if (table[t].prefix == w && table[t].suffix == k) { found = clear + 2 + t; break; }
        }
        if (found >= 0) {
            w = found;
            continue;
        }
        writeCode(w);
        if (nextCode < 4096) {
            table.push_back({w, k});
            if (nextCode == (1 << codeSize) && codeSize < 12) ++codeSize;
            ++nextCode;
        } else {
            writeCode(clear);
            reset();
        }
        w = k;
    }
    writeCode(w);
    writeCode(end);
    if (bits) block.append(char(buf & 0xFF));
    flushBlock();
    out.append(char(0));
}

QImage toIndexed(const QImage &src) {
    QImage img = src.convertToFormat(QImage::Format_RGB888);
    // Build a simple 6x6x6 colour cube + greys (216+ greys fits in 256)
    QVector<QRgb> palette;
    palette.reserve(256);
    for (int r = 0; r < 6; ++r)
        for (int g = 0; g < 6; ++g)
            for (int b = 0; b < 6; ++b)
                palette.append(qRgb(r * 51, g * 51, b * 51));
    while (palette.size() < 256)
        palette.append(qRgb(0, 0, 0));

    QImage indexed(img.size(), QImage::Format_Indexed8);
    indexed.setColorTable(palette);
    for (int y = 0; y < img.height(); ++y) {
        const uchar *in = img.constScanLine(y);
        uchar *out = indexed.scanLine(y);
        for (int x = 0; x < img.width(); ++x) {
            const int r = in[x * 3 + 0], g = in[x * 3 + 1], b = in[x * 3 + 2];
            const int ri = std::min(5, (r * 5 + 127) / 255);
            const int gi = std::min(5, (g * 5 + 127) / 255);
            const int bi = std::min(5, (b * 5 + 127) / 255);
            out[x] = uchar(ri * 36 + gi * 6 + bi);
        }
    }
    return indexed;
}

} // namespace

bool GifWriter::write(const QString &path, const QVector<QImage> &frames, int delayCs, int maxWidth) {
    if (path.isEmpty() || frames.isEmpty()) return false;
    delayCs = std::clamp(delayCs, 2, 500);
    maxWidth = std::max(64, maxWidth);

    QVector<QImage> prepared;
    prepared.reserve(frames.size());
    int W = 0, H = 0;
    for (const QImage &f : frames) {
        if (f.isNull()) continue;
        QImage s = f;
        if (s.width() > maxWidth)
            s = s.scaledToWidth(maxWidth, Qt::SmoothTransformation);
        s = toIndexed(s);
        W = std::max(W, s.width());
        H = std::max(H, s.height());
        prepared.append(s);
    }
    if (prepared.isEmpty() || W < 1 || H < 1) return false;

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;

    // Header
    file.write("GIF89a", 6);
    auto u16 = [&](quint16 v) {
        char b[2] = {char(v & 0xFF), char((v >> 8) & 0xFF)};
        file.write(b, 2);
    };
    u16(quint16(W));
    u16(quint16(H));
    file.write("\xF7\x00\x00", 3); // GCT 256 colours, bg 0

    // Global colour table from first frame
    {
        const QVector<QRgb> pal = prepared.first().colorTable();
        for (int i = 0; i < 256; ++i) {
            const QRgb c = i < pal.size() ? pal[i] : qRgb(0, 0, 0);
            char rgb[3] = {char(qRed(c)), char(qGreen(c)), char(qBlue(c))};
            file.write(rgb, 3);
        }
    }

    // Netscape loop
    file.write("\x21\xFF\x0BNETSCAPE2.0\x03\x01", 16);
    u16(0); // loop forever
    file.write("\x00", 1);

    for (const QImage &frame : prepared) {
        // Graphic control
        file.write("\x21\xF9\x04\x00", 4);
        u16(quint16(delayCs));
        file.write("\x00\x00", 2);
        // Image descriptor
        file.write("\x2C", 1);
        u16(0); u16(0);
        u16(quint16(frame.width()));
        u16(quint16(frame.height()));
        file.write("\x00", 1); // no local CT
        QByteArray lzw;
        std::vector<quint8> pix(size_t(frame.width() * frame.height()));
        for (int y = 0; y < frame.height(); ++y)
            std::memcpy(pix.data() + y * frame.width(), frame.constScanLine(y), size_t(frame.width()));
        lzwEncode(pix.data(), int(pix.size()), lzw);
        file.write(lzw);
    }
    file.write("\x3B", 1); // trailer
    return true;
}
