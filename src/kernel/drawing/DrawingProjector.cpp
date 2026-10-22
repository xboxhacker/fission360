// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#include "DrawingProjector.h"
#include "kernel/FissionDocument.h"
#include "kernel/FissionBody.h"

#include <BRepMesh_IncrementalMesh.hxx>
#include <BRep_Builder.hxx>
#include <BRep_Tool.hxx>
#include <BRepAdaptor_Curve.hxx>
#include <BRepBndLib.hxx>
#include <Bnd_Box.hxx>
#include <GCPnts_QuasiUniformDeflection.hxx>
#include <Geom_Curve.hxx>
#include <HLRAlgo_Projector.hxx>
#include <HLRBRep_Algo.hxx>
#include <HLRBRep_HLRToShape.hxx>
#include <Poly_Triangulation.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Compound.hxx>
#include <gp_Ax2.hxx>
#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>

#include <QBuffer>
#include <QImage>

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace DrawingProjector {
namespace {

TopoDS_Shape compoundOf(FissionDocument *design) {
    if (!design) return {};
    BRep_Builder bb;
    TopoDS_Compound comp;
    bb.MakeCompound(comp);
    int n = 0;
    for (int i = 0; i < design->bodyCount(); ++i) {
        FissionBody *b = design->body(i);
        if (!b || b->shape().IsNull()) continue;
        bb.Add(comp, b->shape());
        ++n;
    }
    return n ? TopoDS_Shape(comp) : TopoDS_Shape();
}

// HLRBRep_HLRToShape returns 2D edges on the projection plane (BRepLib::Plane, XOY).
// Their 3D points are already (view X right, view Y up, 0). Dotting those points with the
// world Ax2 axes collapses front views onto a horizontal line, because front Y is world Z.
void appendSegment(QVariantList &out, const gp_Pnt &p0, const gp_Pnt &p1) {
    QVariantMap seg;
    seg["x1"] = p0.X();
    seg["y1"] = p0.Y();
    seg["x2"] = p1.X();
    seg["y2"] = p1.Y();
    out.append(seg);
}

void edgesToSegments(const TopoDS_Shape &s, QVariantList &out) {
    if (s.IsNull()) return;
    for (TopExp_Explorer ex(s, TopAbs_EDGE); ex.More(); ex.Next()) {
        const TopoDS_Edge edge = TopoDS::Edge(ex.Current());
        if (BRep_Tool::Degenerated(edge)) continue;
        BRepAdaptor_Curve c(edge);
        const double f = c.FirstParameter(), l = c.LastParameter();
        if (l <= f) continue;
        // Sagitta in millimetres, not a fraction of the end-to-end chord. A semicircle's
        // chord is its diameter, and 2% of that turns a round edge into a dozen sides.
        constexpr double kDrawDeflection = 0.02;
        GCPnts_QuasiUniformDeflection discret(c, kDrawDeflection);
        if (!discret.IsDone() || discret.NbPoints() < 2) {
            Standard_Real a, b;
            Handle(Geom_Curve) gc = BRep_Tool::Curve(edge, a, b);
            if (gc.IsNull()) continue;
            appendSegment(out, gc->Value(a), gc->Value(b));
            continue;
        }
        for (int i = 1; i < discret.NbPoints(); ++i)
            appendSegment(out, discret.Value(i), discret.Value(i + 1));
    }
}

void boundsOf(const QVariantList &segs, double &w, double &h) {
    double loX = 1e300, loY = 1e300, hiX = -1e300, hiY = -1e300;
    for (const QVariant &v : segs) {
        const QVariantMap m = v.toMap();
        for (const char *k : {"x1", "x2"}) {
            const double x = m.value(k).toDouble();
            loX = std::min(loX, x);
            hiX = std::max(hiX, x);
        }
        for (const char *k : {"y1", "y2"}) {
            const double y = m.value(k).toDouble();
            loY = std::min(loY, y);
            hiY = std::max(hiY, y);
        }
    }
    if (loX > hiX) { w = h = 0; return; }
    w = hiX - loX;
    h = hiY - loY;
}

// Shift segments so the bbox centre is at the origin (views place their own frame).
void centreSegments(QVariantList &segs) {
    double loX = 1e300, loY = 1e300, hiX = -1e300, hiY = -1e300;
    for (const QVariant &v : segs) {
        const QVariantMap m = v.toMap();
        loX = std::min({loX, m.value("x1").toDouble(), m.value("x2").toDouble()});
        hiX = std::max({hiX, m.value("x1").toDouble(), m.value("x2").toDouble()});
        loY = std::min({loY, m.value("y1").toDouble(), m.value("y2").toDouble()});
        hiY = std::max({hiY, m.value("y1").toDouble(), m.value("y2").toDouble()});
    }
    if (loX > hiX) return;
    const double cx = 0.5 * (loX + hiX), cy = 0.5 * (loY + hiY);
    for (QVariant &v : segs) {
        QVariantMap m = v.toMap();
        m["x1"] = m.value("x1").toDouble() - cx;
        m["x2"] = m.value("x2").toDouble() - cx;
        m["y1"] = m.value("y1").toDouble() - cy;
        m["y2"] = m.value("y2").toDouble() - cy;
        v = m;
    }
}

}   // namespace

namespace {

// Camera sits in the named corner. Sheet +X is world-up crossed with the view axis so the
// part's top stays toward the top of the sheet. Back-top-right keeps the original isometric X.
gp_Ax2 isoCorner(double x, double y, double z, bool legacySheetX) {
    const gp_Dir zdir(x, y, z);
    if (legacySheetX)
        return gp_Ax2(gp_Pnt(0, 0, 0), zdir, gp_Dir(1, 0, -1));
    gp_Vec right = gp_Vec(0, 0, 1).Crossed(gp_Vec(zdir));
    if (right.SquareMagnitude() < 1e-12)
        right = gp_Vec(1, 0, 0);
    return gp_Ax2(gp_Pnt(0, 0, 0), zdir, gp_Dir(right));
}

}   // namespace

gp_Ax2 viewAxis(const QString &orientation) {
    // Looking toward -Z of the Ax2; X right, Y up on the sheet.
    // Front is the -Y side, right is +X, top is +Z.
    if (orientation == "top") return gp_Ax2(gp_Pnt(0, 0, 0), gp_Dir(0, 0, 1), gp_Dir(1, 0, 0));
    if (orientation == "bottom") return gp_Ax2(gp_Pnt(0, 0, 0), gp_Dir(0, 0, -1), gp_Dir(1, 0, 0));
    if (orientation == "right") return gp_Ax2(gp_Pnt(0, 0, 0), gp_Dir(1, 0, 0), gp_Dir(0, 0, 1));
    if (orientation == "left") return gp_Ax2(gp_Pnt(0, 0, 0), gp_Dir(-1, 0, 0), gp_Dir(0, 0, 1));
    if (orientation == "back") return gp_Ax2(gp_Pnt(0, 0, 0), gp_Dir(0, 1, 0), gp_Dir(-1, 0, 0));
    if (orientation == "isoFrontTopRight") return isoCorner(1, -1, 1, false);
    if (orientation == "isoFrontTopLeft") return isoCorner(-1, -1, 1, false);
    if (orientation == "isoFrontBottomRight") return isoCorner(1, -1, -1, false);
    if (orientation == "isoFrontBottomLeft") return isoCorner(-1, -1, -1, false);
    if (orientation == "isoBackTopLeft") return isoCorner(-1, 1, 1, false);
    if (orientation == "isoBackBottomRight") return isoCorner(1, 1, -1, false);
    if (orientation == "isoBackBottomLeft") return isoCorner(-1, 1, -1, false);
    if (orientation == "home" || orientation == "isometric" || orientation == "isoBackTopRight")
        return isoCorner(1, 1, 1, true);
    // front (default): X right, Z up
    return gp_Ax2(gp_Pnt(0, 0, 0), gp_Dir(0, -1, 0), gp_Dir(1, 0, 0));
}

Result projectShape(const TopoDS_Shape &shape, const QString &orientation, bool showHidden) {
    Result r;
    if (shape.IsNull()) { r.error = "Nothing to project"; return r; }
    const gp_Ax2 ax = viewAxis(orientation);
    try {
        Handle(HLRBRep_Algo) algo = new HLRBRep_Algo();
        algo->Add(shape);
        algo->Projector(HLRAlgo_Projector(ax));
        algo->Update();
        algo->Hide();
        HLRBRep_HLRToShape toShape(algo);
        edgesToSegments(toShape.VCompound(), r.visible);
        edgesToSegments(toShape.Rg1LineVCompound(), r.visible);
        edgesToSegments(toShape.OutLineVCompound(), r.visible);
        if (showHidden) {
            edgesToSegments(toShape.HCompound(), r.hidden);
            edgesToSegments(toShape.Rg1LineHCompound(), r.hidden);
            edgesToSegments(toShape.OutLineHCompound(), r.hidden);
        }
    } catch (const Standard_Failure &e) {
        r.error = QString("Projection failed: %1").arg(e.GetMessageString());
        return r;
    }
    centreSegments(r.visible);
    centreSegments(r.hidden);
    boundsOf(r.visible, r.width, r.height);
    if (r.width <= 0 && !r.hidden.isEmpty()) boundsOf(r.hidden, r.width, r.height);
    r.ok = !r.visible.isEmpty() || !r.hidden.isEmpty();
    if (!r.ok) r.error = "The projection produced no edges";
    return r;
}

Result project(FissionDocument *design, const QString &orientation, bool showHidden) {
    return projectShape(compoundOf(design), orientation, showHidden);
}

QByteArray renderShadedPng(const TopoDS_Shape &shape, const QString &orientation, int longEdgePx) {
    if (shape.IsNull()) return {};
    bool needsMesh = false;
    int faceCount = 0;
    for (TopExp_Explorer ex(shape, TopAbs_FACE); ex.More(); ex.Next(), ++faceCount) {
        TopLoc_Location loc;
        if (BRep_Tool::Triangulation(TopoDS::Face(ex.Current()), loc).IsNull())
            needsMesh = true;
    }
    if (faceCount == 0) return {};
    if (needsMesh) {
        Bnd_Box box;
        BRepBndLib::Add(shape, box);
        const double diag = box.IsVoid() ? 100.0 : std::max(1.0, std::sqrt(box.SquareExtent()));
        const double defl = std::max(0.05, diag * 0.004);
        try {
            BRepMesh_IncrementalMesh(shape, defl, Standard_False, 0.35, Standard_True);
        } catch (const Standard_Failure &) {
            return {};
        }
    }

    const gp_Ax2 ax = viewAxis(orientation);
    const gp_Vec right(ax.XDirection());
    const gp_Vec up(ax.YDirection());
    const gp_Vec toward(ax.Direction());
    auto projectPt = [&](const gp_Pnt &p) {
        const gp_Vec v(p.XYZ());
        return std::array<float, 3>{float(v.Dot(right)), float(v.Dot(up)), float(v.Dot(toward))};
    };

    struct Tri {
        float x[3], y[3], z[3];
        float nx, ny, nz;
    };
    std::vector<Tri> tris;
    for (TopExp_Explorer ex(shape, TopAbs_FACE); ex.More(); ex.Next()) {
        TopLoc_Location loc;
        const Handle(Poly_Triangulation) tri = BRep_Tool::Triangulation(TopoDS::Face(ex.Current()), loc);
        if (tri.IsNull() || tri->NbTriangles() < 1) continue;
        const gp_Trsf tr = loc.Transformation();
        const bool reversed = ex.Current().Orientation() == TopAbs_REVERSED;
        for (int t = 1; t <= tri->NbTriangles(); ++t) {
            int a, b, c;
            tri->Triangle(t).Get(a, b, c);
            if (reversed) std::swap(b, c);
            const auto pa = projectPt(tri->Node(a).Transformed(tr));
            const auto pb = projectPt(tri->Node(b).Transformed(tr));
            const auto pc = projectPt(tri->Node(c).Transformed(tr));
            const float ux = pb[0] - pa[0], uy = pb[1] - pa[1], uz = pb[2] - pa[2];
            const float vx = pc[0] - pa[0], vy = pc[1] - pa[1], vz = pc[2] - pa[2];
            float nx = uy * vz - uz * vy, ny = uz * vx - ux * vz, nz = ux * vy - uy * vx;
            const float len = std::sqrt(nx * nx + ny * ny + nz * nz);
            if (len < 1e-8f) continue;
            nx /= len; ny /= len; nz /= len;
            if (nz < 0) { nx = -nx; ny = -ny; nz = -nz; }
            tris.push_back({{pa[0], pb[0], pc[0]}, {pa[1], pb[1], pc[1]}, {pa[2], pb[2], pc[2]}, nx, ny, nz});
        }
    }
    if (tris.empty()) return {};

    float minX = tris[0].x[0], maxX = minX, minY = tris[0].y[0], maxY = minY;
    for (const Tri &t : tris) {
        for (int k = 0; k < 3; ++k) {
            minX = std::min(minX, t.x[k]); maxX = std::max(maxX, t.x[k]);
            minY = std::min(minY, t.y[k]); maxY = std::max(maxY, t.y[k]);
        }
    }
    const float spanX = std::max(1e-3f, maxX - minX);
    const float spanY = std::max(1e-3f, maxY - minY);
    const int edge = std::clamp(longEdgePx, 32, 512);
    int W, H;
    if (spanX >= spanY) {
        W = edge;
        H = std::max(8, int(std::lround(edge * double(spanY / spanX))));
    } else {
        H = edge;
        W = std::max(8, int(std::lround(edge * double(spanX / spanY))));
    }
    QImage img(W, H, QImage::Format_ARGB32);
    img.fill(Qt::transparent);
    std::vector<float> zbuf(size_t(W * H), -1e30f);
    const float sx = (W - 1) / spanX;
    const float sy = (H - 1) / spanY;
    const float lx = 0.22f, ly = 0.42f, lz = 0.88f;
    const float llen = std::sqrt(lx * lx + ly * ly + lz * lz);
    const float Lx = lx / llen, Ly = ly / llen, Lz = lz / llen;

    auto edgeFn = [](float ax, float ay, float bx, float by, float px, float py) {
        return (px - ax) * (by - ay) - (py - ay) * (bx - ax);
    };
    for (const Tri &t : tris) {
        float px[3], py[3];
        for (int k = 0; k < 3; ++k) {
            px[k] = (t.x[k] - minX) * sx;
            py[k] = (maxY - t.y[k]) * sy;
        }
        const int x0 = std::clamp(int(std::floor(std::min({px[0], px[1], px[2]}))), 0, W - 1);
        const int x1 = std::clamp(int(std::ceil(std::max({px[0], px[1], px[2]}))), 0, W - 1);
        const int y0 = std::clamp(int(std::floor(std::min({py[0], py[1], py[2]}))), 0, H - 1);
        const int y1 = std::clamp(int(std::ceil(std::max({py[0], py[1], py[2]}))), 0, H - 1);
        const float area = edgeFn(px[0], py[0], px[1], py[1], px[2], py[2]);
        if (std::fabs(area) < 1e-4f) continue;
        const float inv = 1.f / area;
        const float ndot = std::fabs(t.nx * Lx + t.ny * Ly + t.nz * Lz);
        const float light = 0.38f + 0.62f * ndot;
        const int r = std::clamp(int(174 * light), 0, 255);
        const int g = std::clamp(int(184 * light), 0, 255);
        const int b = std::clamp(int(196 * light), 0, 255);
        const QRgb pix = qRgba(r, g, b, 255);
        for (int y = y0; y <= y1; ++y) {
            auto *scan = reinterpret_cast<QRgb *>(img.scanLine(y));
            float *zrow = zbuf.data() + size_t(y * W);
            const float pyy = y + 0.5f;
            for (int x = x0; x <= x1; ++x) {
                const float pxx = x + 0.5f;
                const float w0 = edgeFn(px[1], py[1], px[2], py[2], pxx, pyy) * inv;
                const float w1 = edgeFn(px[2], py[2], px[0], py[0], pxx, pyy) * inv;
                const float w2 = edgeFn(px[0], py[0], px[1], py[1], pxx, pyy) * inv;
                if (w0 < -1e-4f || w1 < -1e-4f || w2 < -1e-4f) continue;
                const float z = w0 * t.z[0] + w1 * t.z[1] + w2 * t.z[2];
                if (z <= zrow[x]) continue;
                zrow[x] = z;
                scan[x] = pix;
            }
        }
    }

    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    if (!img.save(&buffer, "PNG")) return {};
    return bytes;
}

QByteArray renderShadedPng(FissionDocument *design, const QString &orientation, int longEdgePx) {
    return renderShadedPng(compoundOf(design), orientation, longEdgePx);
}

QString projectedOrientation(const QString &parentOrientation, const QString &direction,
                             const QString &projectionAngle) {
    // Sheet directions relative to the parent view. Third-angle (default) matches US practice;
    // first-angle flips the left/right and up/down pairs.
    const bool third = projectionAngle != QLatin1String("first");
    const QString dir = direction.toLower();
    auto flip = [&](const QString &a, const QString &b) { return third ? a : b; };

    if (parentOrientation == "front") {
        if (dir == "right") return flip("right", "left");
        if (dir == "left") return flip("left", "right");
        if (dir == "up") return flip("top", "bottom");
        if (dir == "down") return flip("bottom", "top");
    }
    if (parentOrientation == "top") {
        if (dir == "right") return flip("right", "left");
        if (dir == "left") return flip("left", "right");
        if (dir == "up") return flip("back", "front");
        if (dir == "down") return flip("front", "back");
    }
    if (parentOrientation == "right") {
        if (dir == "right") return flip("back", "front");
        if (dir == "left") return flip("front", "back");
        if (dir == "up") return flip("top", "bottom");
        if (dir == "down") return flip("bottom", "top");
    }
    if (parentOrientation == "left") {
        if (dir == "right") return flip("front", "back");
        if (dir == "left") return flip("back", "front");
        if (dir == "up") return flip("top", "bottom");
        if (dir == "down") return flip("bottom", "top");
    }
    if (parentOrientation == "back") {
        if (dir == "right") return flip("left", "right");
        if (dir == "left") return flip("right", "left");
        if (dir == "up") return flip("top", "bottom");
        if (dir == "down") return flip("bottom", "top");
    }
    if (parentOrientation == "bottom") {
        if (dir == "right") return flip("right", "left");
        if (dir == "left") return flip("left", "right");
        if (dir == "up") return flip("front", "back");
        if (dir == "down") return flip("back", "front");
    }
    return parentOrientation;
}

Result projectSection(FissionDocument *design, const QString &parentOrientation,
                      const QString &direction, double cutOffsetMm) {
    // Full HLR of the parent view, plus a cutting-plane line through the middle for the UI.
    Result r = project(design, parentOrientation, false);
    if (!r.ok) return r;
    const QString look = projectedOrientation(parentOrientation, direction, "third");
    Result lookProj = project(design, look, false);
    if (lookProj.ok) {
        r = lookProj;
        // Mark as section by adding a centre cutting line in view space
        QVariantMap cut;
        const double half = std::max(r.width, r.height) * 0.6;
        if (direction == "up" || direction == "down") {
            cut["x1"] = -half; cut["y1"] = cutOffsetMm; cut["x2"] = half; cut["y2"] = cutOffsetMm;
        } else {
            cut["x1"] = cutOffsetMm; cut["y1"] = -half; cut["x2"] = cutOffsetMm; cut["y2"] = half;
        }
        r.section.append(cut);
    }
    return r;
}

}   // namespace DrawingProjector
