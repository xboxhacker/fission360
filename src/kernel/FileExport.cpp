// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#include "FileExport.h"

#include <QDateTime>
#include <BRepAdaptor_Curve.hxx>
#include <Standard_Failure.hxx>
#include <array>
#include <cmath>

namespace FileExport {

// --- DXF ---------------------------------------------------------------------------

bool DxfWriter::open(const QString &path, const QList<QPair<QString, int>> &layers, QString *error) {
    m_file.setFileName(path);
    if (!m_file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        if (error) *error = m_file.errorString();
        return false;
    }
    m_out.setDevice(&m_file);
    pair(0, "SECTION"); pair(2, "HEADER");
    pair(9, "$ACADVER"); pair(1, "AC1009");
    pair(9, "$INSUNITS"); pair(70, "4");
    pair(0, "ENDSEC");
    pair(0, "SECTION"); pair(2, "TABLES");
    pair(0, "TABLE"); pair(2, "LAYER"); pair(70, QString::number(layers.size()));
    for (const auto &[name, color] : layers) {
        pair(0, "LAYER"); pair(2, name); pair(70, "0"); pair(62, QString::number(color)); pair(6, "CONTINUOUS");
    }
    pair(0, "ENDTAB");
    pair(0, "ENDSEC");
    pair(0, "SECTION"); pair(2, "ENTITIES");
    return true;
}

void DxfWriter::uv(const gp_Pnt &p, double &u, double &v) const {
    const gp_Vec r(m_frame.Location(), p);
    u = r.Dot(gp_Vec(m_frame.XDirection()));
    v = r.Dot(gp_Vec(m_frame.YDirection()));
}

void DxfWriter::addPoint(double u, double v, const QString &layer) {
    pair(0, "POINT"); pair(8, layer);
    pair(10, u); pair(20, v); pair(30, 0.0);
}

void DxfWriter::addLine2d(double u1, double v1, double u2, double v2, const QString &layer) {
    pair(0, "LINE"); pair(8, layer);
    pair(10, u1); pair(20, v1); pair(30, 0.0);
    pair(11, u2); pair(21, v2); pair(31, 0.0);
}

void DxfWriter::addCircle2d(double cu, double cv, double radius, const QString &layer) {
    pair(0, "CIRCLE"); pair(8, layer);
    pair(10, cu); pair(20, cv); pair(30, 0.0);
    pair(40, radius);
}

void DxfWriter::addText2d(double u, double v, const QString &text, double height, const QString &layer) {
    pair(0, "TEXT"); pair(8, layer);
    pair(10, u); pair(20, v); pair(30, 0.0);
    pair(40, height);
    pair(1, text);
}

void DxfWriter::addEdge(const TopoDS_Edge &edge, const QString &layer) {
    try {
        BRepAdaptor_Curve c(edge);
        const double f = c.FirstParameter(), l = c.LastParameter();
        double au, av, bu, bv;
        uv(c.Value(f), au, av);
        uv(c.Value(l), bu, bv);
        const bool closed = std::hypot(au - bu, av - bv) < 1e-7;
        if (c.GetType() == GeomAbs_Line) {
            pair(0, "LINE"); pair(8, layer);
            pair(10, au); pair(20, av); pair(30, 0.0);
            pair(11, bu); pair(21, bv); pair(31, 0.0);
            return;
        }
        // Circles in the drawing plane: DXF arcs run counter-clockwise from the start to the end angle
        if (c.GetType() == GeomAbs_Circle
            && std::fabs(std::fabs(c.Circle().Axis().Direction().Dot(m_frame.Direction())) - 1) < 1e-9) {
            double cu, cv;
            uv(c.Circle().Location(), cu, cv);
            const double r = c.Circle().Radius();
            if (closed) {
                pair(0, "CIRCLE"); pair(8, layer);
                pair(10, cu); pair(20, cv); pair(30, 0.0);
                pair(40, r);
                return;
            }
            double mu, mv;
            uv(c.Value((f + l) / 2), mu, mv);
            auto angle = [&](double u, double v) {
                double a = std::atan2(v - cv, u - cu) * 180 / M_PI;
                return a < 0 ? a + 360 : a;
            };
            double s = angle(au, av), e = angle(bu, bv);
            const double m = angle(mu, mv);
            auto ccwSpan = [](double from, double to) { double d = to - from; return d < 0 ? d + 360 : d; };
            if (ccwSpan(s, m) > ccwSpan(s, e)) std::swap(s, e);   // the midpoint is on the other way round
            pair(0, "ARC"); pair(8, layer);
            pair(10, cu); pair(20, cv); pair(30, 0.0);
            pair(40, r);
            pair(50, s); pair(51, e);
            return;
        }
        // Ellipses, splines: a polyline through 64 samples
        pair(0, "POLYLINE"); pair(8, layer); pair(66, "1");
        pair(10, 0.0); pair(20, 0.0); pair(30, 0.0);
        pair(70, closed ? "1" : "0");
        const int n = 64;
        for (int k = 0; k <= (closed ? n - 1 : n); ++k) {
            double qu, qv;
            uv(c.Value(f + (l - f) * k / n), qu, qv);
            pair(0, "VERTEX"); pair(8, layer);
            pair(10, qu); pair(20, qv); pair(30, 0.0);
        }
        pair(0, "SEQEND"); pair(8, layer);
    } catch (const Standard_Failure &) {
    }
}

bool DxfWriter::close(QString *error) {
    pair(0, "ENDSEC");
    pair(0, "EOF");
    m_out.flush();
    const bool ok = m_out.status() == QTextStream::Ok;
    m_file.close();
    if (!ok && error) *error = "Write failed";
    return ok;
}

// --- ZIP -----------------------------------------------------------------------------

static quint32 crc32(const QByteArray &data) {
    static const std::array<quint32, 256> table = [] {
        std::array<quint32, 256> t{};
        for (quint32 i = 0; i < 256; ++i) {
            quint32 c = i;
            for (int k = 0; k < 8; ++k) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            t[i] = c;
        }
        return t;
    }();
    quint32 c = 0xFFFFFFFFu;
    for (const char ch : data) c = table[(c ^ quint8(ch)) & 0xFF] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}

bool writeZip(const QString &path, const QList<QPair<QString, QByteArray>> &entries, QString &error) {
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) { error = file.errorString(); return false; }
    auto u16 = [](QByteArray &b, quint16 v) { b.append(char(v & 0xFF)); b.append(char(v >> 8)); };
    auto u32 = [&](QByteArray &b, quint32 v) { u16(b, quint16(v & 0xFFFF)); u16(b, quint16(v >> 16)); };
    const QDateTime now = QDateTime::currentDateTime();
    const quint16 dosTime = quint16((now.time().hour() << 11) | (now.time().minute() << 5) | (now.time().second() / 2));
    const quint16 dosDate = quint16(((now.date().year() - 1980) << 9) | (now.date().month() << 5) | now.date().day());
    QByteArray central;
    quint32 offset = 0;
    for (const auto &[name, data] : entries) {
        // qCompress = 4-byte size + zlib stream; the raw deflate data sits between the 2-byte header and the
        // 4-byte checksum
        const QByteArray z = qCompress(data, 6);
        const QByteArray deflated = z.mid(4 + 2, z.size() - 4 - 2 - 4);
        const QByteArray fname = name.toUtf8();
        const quint32 crc = crc32(data);
        QByteArray local;
        u32(local, 0x04034b50); u16(local, 20); u16(local, 0); u16(local, 8); u16(local, dosTime); u16(local, dosDate);
        u32(local, crc); u32(local, quint32(deflated.size())); u32(local, quint32(data.size()));
        u16(local, quint16(fname.size())); u16(local, 0);
        local += fname;
        if (file.write(local) != local.size() || file.write(deflated) != deflated.size()) {
            error = file.errorString();
            return false;
        }
        u32(central, 0x02014b50); u16(central, 20); u16(central, 20); u16(central, 0); u16(central, 8);
        u16(central, dosTime); u16(central, dosDate);
        u32(central, crc); u32(central, quint32(deflated.size())); u32(central, quint32(data.size()));
        u16(central, quint16(fname.size())); u16(central, 0); u16(central, 0); u16(central, 0); u16(central, 0);
        u32(central, 0); u32(central, offset);
        central += fname;
        offset += quint32(local.size() + deflated.size());
    }
    QByteArray end;
    u32(end, 0x06054b50); u16(end, 0); u16(end, 0);
    u16(end, quint16(entries.size())); u16(end, quint16(entries.size()));
    u32(end, quint32(central.size())); u32(end, offset); u16(end, 0);
    if (file.write(central) != central.size() || file.write(end) != end.size()) { error = file.errorString(); return false; }
    return true;
}

// --- 3MF -----------------------------------------------------------------------------

bool write3mf(const QString &path, const QList<MeshPart> &parts, QString &error) {
    if (parts.isEmpty()) { error = "Nothing to export"; return false; }
    auto esc = [](const QString &s) { return s.toHtmlEscaped().toUtf8(); };
    auto num = [](double v) { return QByteArray::number(v, 'g', 9); };
    QByteArray model;
    model += "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
             "<model unit=\"millimeter\" xml:lang=\"en-US\" "
             "xmlns=\"http://schemas.microsoft.com/3dmanufacturing/core/2015/02\">\n"
             " <metadata name=\"Application\">Fission 360</metadata>\n"
             " <resources>\n  <basematerials id=\"1\">\n";
    for (const MeshPart &p : parts) {
        const QColor c = p.color.isValid() ? p.color : QColor(170, 178, 189);
        model += "   <base name=\"" + esc(p.name) + "\" displaycolor=\"" + c.name(QColor::HexRgb).toUpper().toUtf8() + "FF\"/>\n";
    }
    model += "  </basematerials>\n";
    for (int i = 0; i < parts.size(); ++i) {
        const MeshUtils::Mesh &m = parts[i].mesh;
        model += "  <object id=\"" + QByteArray::number(i + 2) + "\" name=\"" + esc(parts[i].name)
                 + "\" type=\"model\" pid=\"1\" pindex=\"" + QByteArray::number(i) + "\">\n   <mesh>\n    <vertices>\n";
        for (const gp_Pnt &p : m.nodes)
            model += "     <vertex x=\"" + num(p.X()) + "\" y=\"" + num(p.Y()) + "\" z=\"" + num(p.Z()) + "\"/>\n";
        model += "    </vertices>\n    <triangles>\n";
        for (const auto &t : m.tris)
            model += "     <triangle v1=\"" + QByteArray::number(t[0]) + "\" v2=\"" + QByteArray::number(t[1]) + "\" v3=\""
                     + QByteArray::number(t[2]) + "\"/>\n";
        model += "    </triangles>\n   </mesh>\n  </object>\n";
    }
    model += " </resources>\n <build>\n";
    for (int i = 0; i < parts.size(); ++i) model += "  <item objectid=\"" + QByteArray::number(i + 2) + "\"/>\n";
    model += " </build>\n</model>\n";

    const QByteArray types =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">\n"
        " <Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>\n"
        " <Default Extension=\"model\" ContentType=\"application/vnd.ms-package.3dmanufacturing-3dmodel+xml\"/>\n"
        "</Types>\n";
    const QByteArray rels =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">\n"
        " <Relationship Target=\"/3D/3dmodel.model\" Id=\"rel0\" "
        "Type=\"http://schemas.microsoft.com/3dmanufacturing/2013/01/3dmodel\"/>\n"
        "</Relationships>\n";
    return writeZip(path, {{"[Content_Types].xml", types}, {"_rels/.rels", rels}, {"3D/3dmodel.model", model}}, error);
}

}  // namespace FileExport
