// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#pragma once
#include "MeshUtils.h"

#include <QColor>
#include <QFile>
#include <QList>
#include <QString>
#include <QTextStream>
#include <gp_Ax3.hxx>
#include <TopoDS_Edge.hxx>

namespace FileExport {

// DXF R12 drawing in the XY plane of a frame: lines, circles, arcs and polylines (for anything else)
class DxfWriter {
public:
    explicit DxfWriter(const gp_Ax3 &frame) : m_frame(frame) {}
    bool open(const QString &path, const QList<QPair<QString, int>> &layers, QString *error);
    void addEdge(const TopoDS_Edge &edge, const QString &layer);
    void addPoint(double u, double v, const QString &layer);
    void addLine2d(double u1, double v1, double u2, double v2, const QString &layer);
    void addCircle2d(double cu, double cv, double radius, const QString &layer);
    void addText2d(double u, double v, const QString &text, double height, const QString &layer);
    bool close(QString *error);

private:
    void pair(int code, const QString &value) { m_out << code << "\n" << value << "\n"; }
    void pair(int code, double value) { pair(code, QString::number(value, 'f', 6)); }
    void uv(const gp_Pnt &p, double &u, double &v) const;
    gp_Ax3 m_frame;
    QFile m_file;
    QTextStream m_out;
};

// 3MF (3D Manufacturing Format): one mesh object per part, in millimetres, with its display colour
struct MeshPart {
    QString name;
    QColor color;
    MeshUtils::Mesh mesh;
};
bool write3mf(const QString &path, const QList<MeshPart> &parts, QString &error);

// Minimal ZIP archive (deflated entries), as used by 3MF
bool writeZip(const QString &path, const QList<QPair<QString, QByteArray>> &entries, QString &error);

}  // namespace FileExport
