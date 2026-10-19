// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#include "PathTracer.h"
#include "HdrImage.h"
#include "kernel/FissionBody.h"
#include "kernel/FissionDocument.h"
#include "kernel/GeomUtils.h"

#include <QFileInfo>
#include <QImage>
#include <QtMath>

#include <BRepMesh_IncrementalMesh.hxx>
#include <BRep_Tool.hxx>
#include <Poly_Triangulation.hxx>
#include <TopExp_Explorer.hxx>
#include <TopLoc_Location.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Face.hxx>

#include <algorithm>
#include <cmath>
#include <random>
#include <vector>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace {

struct Tri {
    QVector3D v0, v1, v2, n;
    QVector3D albedo{0.7f, 0.7f, 0.72f};
    float rough = 0.55f;
    float metal = 0.0f;
    float opacity = 1.0f;
};

struct BvhNode {
    QVector3D bmin{1e9f, 1e9f, 1e9f}, bmax{-1e9f, -1e9f, -1e9f};
    int left = -1, right = -1;
    int first = 0, count = 0;
};

QVector3D vmin(const QVector3D &a, const QVector3D &b) {
    return {std::min(a.x(), b.x()), std::min(a.y(), b.y()), std::min(a.z(), b.z())};
}
QVector3D vmax(const QVector3D &a, const QVector3D &b) {
    return {std::max(a.x(), b.x()), std::max(a.y(), b.y()), std::max(a.z(), b.z())};
}

void expand(BvhNode &n, const QVector3D &p) {
    n.bmin = vmin(n.bmin, p);
    n.bmax = vmax(n.bmax, p);
}

float comp(const QVector3D &v, int a) {
    return a == 0 ? v.x() : a == 1 ? v.y() : v.z();
}

bool hitAabb(const QVector3D &ro, const QVector3D &rd, const QVector3D &bmin, const QVector3D &bmax, float tMax) {
    float t0 = 0.f, t1 = tMax;
    for (int a = 0; a < 3; ++a) {
        const float o = comp(ro, a), d = comp(rd, a);
        const float mn = comp(bmin, a), mx = comp(bmax, a);
        if (std::fabs(d) < 1e-8f) {
            if (o < mn || o > mx) return false;
            continue;
        }
        float inv = 1.f / d;
        float ta = (mn - o) * inv, tb = (mx - o) * inv;
        if (ta > tb) std::swap(ta, tb);
        t0 = std::max(t0, ta);
        t1 = std::min(t1, tb);
        if (t0 > t1) return false;
    }
    return true;
}

bool hitTri(const Tri &t, const QVector3D &ro, const QVector3D &rd, float &tHit, QVector3D &nOut) {
    const QVector3D e1 = t.v1 - t.v0, e2 = t.v2 - t.v0;
    const QVector3D p = QVector3D::crossProduct(rd, e2);
    const float det = QVector3D::dotProduct(e1, p);
    if (std::fabs(det) < 1e-8f) return false;
    const float inv = 1.f / det;
    const QVector3D s = ro - t.v0;
    const float u = QVector3D::dotProduct(s, p) * inv;
    if (u < 0.f || u > 1.f) return false;
    const QVector3D q = QVector3D::crossProduct(s, e1);
    const float v = QVector3D::dotProduct(rd, q) * inv;
    if (v < 0.f || u + v > 1.f) return false;
    const float th = QVector3D::dotProduct(e2, q) * inv;
    if (th <= 1e-4f || th >= tHit) return false;
    tHit = th;
    nOut = t.n;
    if (QVector3D::dotProduct(nOut, rd) > 0.f) nOut = -nOut;
    return true;
}

int buildBvh(std::vector<Tri> &tris, std::vector<int> &order, std::vector<BvhNode> &nodes, int start, int end) {
    BvhNode node;
    for (int i = start; i < end; ++i) {
        const Tri &t = tris[order[i]];
        expand(node, t.v0); expand(node, t.v1); expand(node, t.v2);
    }
    const int count = end - start;
    if (count <= 4) {
        node.first = start;
        node.count = count;
        nodes.push_back(node);
        return int(nodes.size()) - 1;
    }
    const QVector3D ext = node.bmax - node.bmin;
    int axis = 0;
    if (ext.y() > ext.x()) axis = 1;
    if (ext.z() > comp(ext, axis)) axis = 2;
    const int mid = start + count / 2;
    std::nth_element(order.begin() + start, order.begin() + mid, order.begin() + end, [&](int a, int b) {
        const auto c = [&](int i) {
            const Tri &t = tris[i];
            return (comp(t.v0, axis) + comp(t.v1, axis) + comp(t.v2, axis)) * (1.f / 3.f);
        };
        return c(a) < c(b);
    });
    const int self = int(nodes.size());
    nodes.push_back(node);
    const int L = buildBvh(tris, order, nodes, start, mid);
    const int R = buildBvh(tris, order, nodes, mid, end);
    nodes[self].left = L;
    nodes[self].right = R;
    return self;
}

bool closestHit(const std::vector<Tri> &tris, const std::vector<int> &order, const std::vector<BvhNode> &nodes,
                int root, const QVector3D &ro, const QVector3D &rd, float &tHit, int &triOut, QVector3D &nOut) {
    if (root < 0 || nodes.empty()) return false;
    int stack[64];
    int sp = 0;
    stack[sp++] = root;
    bool hit = false;
    while (sp) {
        const BvhNode &n = nodes[stack[--sp]];
        if (!hitAabb(ro, rd, n.bmin, n.bmax, tHit)) continue;
        if (n.count > 0) {
            for (int i = 0; i < n.count; ++i) {
                const int ti = order[n.first + i];
                if (hitTri(tris[ti], ro, rd, tHit, nOut)) {
                    hit = true;
                    triOut = ti;
                }
            }
        } else {
            if (n.left >= 0 && sp < 64) stack[sp++] = n.left;
            if (n.right >= 0 && sp < 64) stack[sp++] = n.right;
        }
    }
    return hit;
}

QVector3D sampleEnv(const HdrImage &hdr, const QVector3D &dir, float intensity, float rot) {
    if (hdr.isNull()) return QVector3D(0.55f, 0.58f, 0.62f) * intensity;
    QVector3D d = dir.normalized();
    const float c = std::cos(rot), s = std::sin(rot);
    d = QVector3D(c * d.x() - s * d.y(), s * d.x() + c * d.y(), d.z());
    return hdr.sample(d) * intensity;
}

QVector3D orthonormal(const QVector3D &n) {
    const QVector3D a = std::fabs(n.z()) < 0.9f ? QVector3D(0, 0, 1) : QVector3D(1, 0, 0);
    return QVector3D::crossProduct(n, a).normalized();
}

QVector3D sampleCosine(const QVector3D &n, float u1, float u2) {
    const float r = std::sqrt(u1), phi = 2.f * float(M_PI) * u2;
    const QVector3D t = orthonormal(n);
    const QVector3D b = QVector3D::crossProduct(n, t);
    return (t * (r * std::cos(phi)) + b * (r * std::sin(phi)) + n * std::sqrt(std::max(0.f, 1.f - u1))).normalized();
}

float luminance(const QVector3D &c) { return 0.2126f * c.x() + 0.7152f * c.y() + 0.0722f * c.z(); }

QColor sampleTriplanarGray(const QImage &img, const QVector3D &p, const QVector3D &n, float scaleMm = 50.f) {
    if (img.isNull()) return Qt::white;
    auto sample = [&](float u, float v) {
        const int x = (int(std::floor(u * img.width())) % img.width() + img.width()) % img.width();
        const int y = (int(std::floor(v * img.height())) % img.height() + img.height()) % img.height();
        return img.pixelColor(x, y);
    };
    const QVector3D an(std::fabs(n.x()), std::fabs(n.y()), std::fabs(n.z()));
    const float sum = an.x() + an.y() + an.z() + 1e-5f;
    const QColor cx = sample(p.y() / scaleMm, p.z() / scaleMm);
    const QColor cy = sample(p.x() / scaleMm, p.z() / scaleMm);
    const QColor cz = sample(p.x() / scaleMm, p.y() / scaleMm);
    const float wx = an.x() / sum, wy = an.y() / sum, wz = an.z() / sum;
    return QColor::fromRgbF(cx.redF() * wx + cy.redF() * wy + cz.redF() * wz,
                            cx.greenF() * wx + cy.greenF() * wy + cz.greenF() * wz,
                            cx.blueF() * wx + cy.blueF() * wy + cz.blueF() * wz);
}

void appendFaceTris(std::vector<Tri> &out, const TopoDS_Face &face, const QColor &col, float rough, float metal,
                    float opacity, const QImage &albedo, const QImage &roughMap, const QImage &metalMap) {
    TopLoc_Location loc;
    Handle(Poly_Triangulation) tri = BRep_Tool::Triangulation(face, loc);
    if (tri.IsNull()) {
        BRepMesh_IncrementalMesh(face, 0.5, Standard_False, 0.5);
        tri = BRep_Tool::Triangulation(face, loc);
    }
    if (tri.IsNull() || tri->NbTriangles() < 1) return;
    const gp_Trsf tr = loc.Transformation();
    const bool rev = face.Orientation() == TopAbs_REVERSED;
    auto gp = [&](int i) {
        const gp_Pnt p = tri->Node(i).Transformed(tr);
        return QVector3D(float(p.X()), float(p.Y()), float(p.Z()));
    };
    for (int i = 1; i <= tri->NbTriangles(); ++i) {
        int i1, i2, i3;
        tri->Triangle(i).Get(i1, i2, i3);
        if (rev) std::swap(i2, i3);
        Tri t;
        t.v0 = gp(i1); t.v1 = gp(i2); t.v2 = gp(i3);
        t.n = QVector3D::crossProduct(t.v1 - t.v0, t.v2 - t.v0).normalized();
        if (t.n.lengthSquared() < 1e-10f) continue;
        const QVector3D c = (t.v0 + t.v1 + t.v2) * (1.f / 3.f);
        QColor base = col;
        if (!albedo.isNull()) {
            const QColor te = sampleTriplanarGray(albedo, c, t.n);
            base = QColor::fromRgbF(base.redF() * te.redF(), base.greenF() * te.greenF(), base.blueF() * te.blueF());
        }
        t.albedo = QVector3D(float(base.redF()), float(base.greenF()), float(base.blueF()));
        t.rough = rough;
        t.metal = metal;
        t.opacity = opacity;
        if (!roughMap.isNull())
            t.rough = std::clamp(float(sampleTriplanarGray(roughMap, c, t.n).redF()), 0.04f, 1.f);
        if (!metalMap.isNull())
            t.metal = std::clamp(float(sampleTriplanarGray(metalMap, c, t.n).redF()), 0.f, 1.f);
        out.push_back(t);
    }
}

std::vector<Tri> buildScene(FissionDocument *doc) {
    std::vector<Tri> tris;
    if (!doc) return tris;
    for (int bi = 0; bi < doc->bodyCount(); ++bi) {
        FissionBody *b = doc->body(bi);
        if (!b || !b->isVisible() || b->shape().IsNull()) continue;
        QImage albedo, roughMap, metalMap;
        if (!b->texturePath().isEmpty() && QFileInfo::exists(b->texturePath()))
            albedo = QImage(b->texturePath()).convertToFormat(QImage::Format_RGB888);
        if (!b->roughnessPath().isEmpty() && QFileInfo::exists(b->roughnessPath()))
            roughMap = QImage(b->roughnessPath()).convertToFormat(QImage::Format_RGB888);
        if (!b->metalnessPath().isEmpty() && QFileInfo::exists(b->metalnessPath()))
            metalMap = QImage(b->metalnessPath()).convertToFormat(QImage::Format_RGB888);

        const TopTools_IndexedMapOfShape faces = GeomUtils::faceMap(b->shape());
        for (int fi = 1; fi <= faces.Extent(); ++fi) {
            const int faceIdx = fi - 1;
            const QVariantMap ov = b->faceAppearance(faceIdx);
            QColor col = b->color();
            float rough = float(b->roughness());
            float metal = float(b->metalness());
            float opacity = float(b->opacity());
            QImage a = albedo, r = roughMap, m = metalMap;
            if (!ov.isEmpty()) {
                if (ov.contains("color")) col = QColor(ov["color"].toString());
                if (ov.contains("roughness")) rough = float(ov["roughness"].toDouble());
                if (ov.contains("metalness")) metal = float(ov["metalness"].toDouble());
                if (ov.contains("opacity")) opacity = float(ov["opacity"].toDouble());
                if (ov.contains("texturePath") && QFileInfo::exists(ov["texturePath"].toString()))
                    a = QImage(ov["texturePath"].toString()).convertToFormat(QImage::Format_RGB888);
                if (ov.contains("roughnessPath") && QFileInfo::exists(ov["roughnessPath"].toString()))
                    r = QImage(ov["roughnessPath"].toString()).convertToFormat(QImage::Format_RGB888);
                if (ov.contains("metalnessPath") && QFileInfo::exists(ov["metalnessPath"].toString()))
                    m = QImage(ov["metalnessPath"].toString()).convertToFormat(QImage::Format_RGB888);
            }
            appendFaceTris(tris, TopoDS::Face(faces(fi)), col, rough, metal, opacity, a, r, m);
        }
    }
    return tris;
}

QVector3D trace(const std::vector<Tri> &tris, const std::vector<int> &order, const std::vector<BvhNode> &nodes,
                int root, QVector3D ro, QVector3D rd, const HdrImage &hdr, const PathTracer::Settings &s,
                std::mt19937 &rng) {
    std::uniform_real_distribution<float> U(0.f, 1.f);
    QVector3D throughput(1, 1, 1), color(0, 0, 0);
    for (int bounce = 0; bounce < s.maxBounces; ++bounce) {
        float tHit = 1e30f;
        int ti = -1;
        QVector3D n;
        if (!closestHit(tris, order, nodes, root, ro, rd, tHit, ti, n)) {
            color += throughput * sampleEnv(hdr, rd, float(s.environmentIntensity), float(s.environmentRotation));
            if (s.useSun && bounce == 0) {
                // soft miss still gets env; sun is handled on hits
            }
            break;
        }
        const Tri &hit = tris[ti];
        const QVector3D p = ro + rd * tHit;
        // Direct sun (shadow ray)
        if (s.useSun && s.sunIntensity > 0.01f) {
            const QVector3D L = s.sunDir.normalized();
            float tShadow = 1e30f;
            int ignore = -1;
            QVector3D nIgnore;
            if (!closestHit(tris, order, nodes, root, p + n * 1e-3f, L, tShadow, ignore, nIgnore)) {
                const float ndotl = std::max(0.f, QVector3D::dotProduct(n, L));
                const QVector3D diffuse = hit.albedo * (1.f - hit.metal);
                const QVector3D spec = hit.albedo * hit.metal
                                      + QVector3D(0.04f, 0.04f, 0.04f) * (1.f - hit.metal);
                const QVector3D H = (L - rd).normalized();
                const float nh = std::max(0.f, QVector3D::dotProduct(n, H));
                const float shin = 8.f + (180.f - 8.f) * std::pow(1.f - hit.rough, 1.5f);
                const float specAmt = std::pow(nh, shin) * (1.f - 0.6f * hit.rough);
                color += throughput * (diffuse * ndotl + spec * specAmt) * s.sunColor * s.sunIntensity;
            }
        }
        // Next bounce: mix diffuse / specular by metalness and roughness
        const float u1 = U(rng), u2 = U(rng);
        QVector3D nextDir;
        QVector3D brdf;
        if (U(rng) < hit.metal * 0.85f + (1.f - hit.rough) * 0.15f) {
            const QVector3D R = (rd - 2.f * QVector3D::dotProduct(rd, n) * n).normalized();
            // Cone around mirror for roughness
            nextDir = sampleCosine(R, u1 * hit.rough * hit.rough + 1e-4f, u2);
            if (QVector3D::dotProduct(nextDir, n) <= 0.f) nextDir = sampleCosine(n, u1, u2);
            brdf = QVector3D(hit.albedo.x(), hit.albedo.y(), hit.albedo.z());
        } else {
            nextDir = sampleCosine(n, u1, u2);
            brdf = hit.albedo * (1.f - hit.metal);
        }
        throughput *= brdf;
        if (luminance(throughput) < 0.01f) break;
        if (bounce > 1) {
            const float q = std::clamp(luminance(throughput), 0.05f, 0.95f);
            if (U(rng) > q) break;
            throughput /= q;
        }
        ro = p + n * 1e-3f;
        rd = nextDir;
    }
    return color;
}

} // namespace

int PathTracer::samplesForQuality(int quality) {
    static const int spp[] = {8, 32, 96, 256};
    return spp[std::clamp(quality, 0, 3)];
}

QImage PathTracer::render(FissionDocument *doc, const Camera &cam, const Settings &settings, ProgressFn progress) {
    const int W = std::max(8, settings.width);
    const int H = std::max(8, settings.height);
    const int spp = std::max(1, settings.samplesPerPixel);

    auto tris = buildScene(doc);
    std::vector<int> order(tris.size());
    for (int i = 0; i < int(tris.size()); ++i) order[i] = i;
    std::vector<BvhNode> nodes;
    int root = -1;
    if (!tris.empty())
        root = buildBvh(tris, order, nodes, 0, int(tris.size()));

    HdrImage hdr;
    if (!settings.environmentPath.isEmpty() && QFileInfo::exists(settings.environmentPath))
        hdr.load(settings.environmentPath);

    const QVector3D forward = (cam.target - cam.eye).normalized();
    const QVector3D right = QVector3D::crossProduct(forward, cam.up).normalized();
    const QVector3D up = QVector3D::crossProduct(right, forward);
    const float aspect = float(W) / float(H);
    const float tanHalf = std::tan(float(cam.fovDeg) * float(M_PI) / 360.f);

    QImage img(W, H, QImage::Format_RGB888);
    img.fill(settings.background);
    std::mt19937 rng(0xF15510);
    std::uniform_real_distribution<float> U(0.f, 1.f);

    for (int y = 0; y < H; ++y) {
        for (int x = 0; x < W; ++x) {
            QVector3D sum(0, 0, 0);
            for (int s = 0; s < spp; ++s) {
                const float px = (float(x) + U(rng)) / float(W) * 2.f - 1.f;
                const float py = 1.f - (float(y) + U(rng)) / float(H) * 2.f;
                QVector3D rd;
                if (cam.ortho) {
                    rd = forward;
                } else {
                    rd = (forward + right * (px * aspect * tanHalf) + up * (py * tanHalf)).normalized();
                }
                QVector3D ro = cam.eye;
                if (cam.ortho)
                    ro = cam.eye + right * (px * aspect * cam.eye.distanceToPoint(cam.target) * 0.5f)
                         + up * (py * cam.eye.distanceToPoint(cam.target) * 0.5f);
                sum += trace(tris, order, nodes, root, ro, rd, hdr, settings, rng);
            }
            sum /= float(spp);
            // Reinhard + gamma
            auto tonemap = [](float v) {
                v = v / (1.f + v);
                return int(std::clamp(std::pow(v, 1.f / 2.2f) * 255.f, 0.f, 255.f));
            };
            img.setPixel(x, y, qRgb(tonemap(sum.x()), tonemap(sum.y()), tonemap(sum.z())));
        }
        if (progress && ((y & 7) == 0 || y == H - 1))
            progress(double(y + 1) / double(H));
    }
    return img;
}
