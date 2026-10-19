// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#include "MeshUtils.h"

#include <QElapsedTimer>
#include <QStringList>
#include <cstdio>

#include <BRepAdaptor_Curve.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepBuilderAPI_Sewing.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepGProp.hxx>
#include <BRepLib.hxx>
#include <BRep_Builder.hxx>
#include <BRep_Tool.hxx>
#include <ElCLib.hxx>
#include <GProp_GProps.hxx>
#include <Geom_Circle.hxx>
#include <Geom_ConicalSurface.hxx>
#include <Geom_CylindricalSurface.hxx>
#include <Geom_SphericalSurface.hxx>
#include <Geom_SurfaceOfLinearExtrusion.hxx>
#include <GeomAPI_Interpolate.hxx>
#include <GeomAPI_PointsToBSpline.hxx>
#include <GeomAPI_ProjectPointOnSurf.hxx>
#include <Geom_BSplineCurve.hxx>
#include <Geom_Line.hxx>
#include <Geom2d_BSplineCurve.hxx>
#include <TColgp_Array1OfPnt2d.hxx>
#include <Geom_BSplineSurface.hxx>
#include <ShapeFix_Edge.hxx>
#include <TColStd_Array1OfInteger.hxx>
#include <TColStd_Array1OfReal.hxx>
#include <TColgp_Array2OfPnt.hxx>
#include <TColgp_Array1OfPnt.hxx>
#include <gp_Trsf.hxx>
#include <TColgp_HArray1OfPnt.hxx>
#include <Poly_Triangulation.hxx>
#include <Precision.hxx>
#include <ShapeFix_Shape.hxx>
#include <ShapeFix_Solid.hxx>
#include <Standard_Failure.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Shell.hxx>
#include <TopoDS_Solid.hxx>
#include <TopoDS_Vertex.hxx>
#include <TopoDS_Wire.hxx>
#include <TopTools_IndexedDataMapOfShapeListOfShape.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopTools_ListIteratorOfListOfShape.hxx>
#include <GeomAPI_ProjectPointOnCurve.hxx>
#include <gp_Ax2.hxx>
#include <gp_Circ.hxx>
#include <algorithm>
#include <cmath>
#include <map>
#include <unordered_map>
#include <unordered_set>

// Mesh -> B-rep by region fitting: the closed mesh is split into regions bounded by sharp edges, each region
// is fitted with a plane, cylinder, cone or sphere, and the faces are rebuilt on the fitted surfaces with
// line / circle edges. Regions that fit nothing stay as merged flat facets. Everything is linear in the
// number of triangles except the final face building, which only sees the (few) regions.
namespace MeshUtils {

namespace {

constexpr double kPi = 3.14159265358979323846;

constexpr int kLargeFaceCount = 3000;   // above this, no sewing and no full BRepCheck
constexpr int kLargeTriangleCount = 40000;   // likewise for scanned / organic meshes of this size
constexpr int kSewFaceCount = 6000;     // faces that do not meet are still sewn up to this many

QElapsedTimer g_stageTimer;
void stage(const char *name) {
    static const bool on = qEnvironmentVariableIsSet("FIT_TIMING");
    if (!on) return;
    if (!g_stageTimer.isValid()) g_stageTimer.start();
    std::fprintf(stderr, "  [fit] %s: %lld ms\n", name, g_stageTimer.restart());
}

quint64 key2(int a, int b) {
    if (a > b) std::swap(a, b);
    return (quint64(quint32(a)) << 32) | quint32(b);
}

// Symmetric 3x3 eigen decomposition (Jacobi). Columns of V are the eigenvectors of w.
void eigenSym(double A[3][3], double w[3], double V[3][3]) {
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j) V[i][j] = i == j;
    for (int sweep = 0; sweep < 60; ++sweep) {
        const double off = A[0][1] * A[0][1] + A[0][2] * A[0][2] + A[1][2] * A[1][2];
        if (off < 1e-30) break;
        for (int p = 0; p < 2; ++p)
            for (int q = p + 1; q < 3; ++q) {
                if (std::fabs(A[p][q]) < 1e-300) continue;
                const double theta = (A[q][q] - A[p][p]) / (2 * A[p][q]);
                const double t = (theta >= 0 ? 1.0 : -1.0) / (std::fabs(theta) + std::sqrt(theta * theta + 1));
                const double c = 1 / std::sqrt(t * t + 1), s = t * c;
                for (int k = 0; k < 3; ++k) {
                    const double akp = A[k][p], akq = A[k][q];
                    A[k][p] = c * akp - s * akq;
                    A[k][q] = s * akp + c * akq;
                }
                for (int k = 0; k < 3; ++k) {
                    const double apk = A[p][k], aqk = A[q][k];
                    A[p][k] = c * apk - s * aqk;
                    A[q][k] = s * apk + c * aqk;
                }
                for (int k = 0; k < 3; ++k) {
                    const double vkp = V[k][p], vkq = V[k][q];
                    V[k][p] = c * vkp - s * vkq;
                    V[k][q] = s * vkp + c * vkq;
                }
            }
    }
    for (int i = 0; i < 3; ++i) w[i] = A[i][i];
}

// Gaussian elimination with partial pivoting on an n x (n+1) augmented matrix
bool solve(std::vector<std::vector<double>> a, std::vector<double> &x) {
    const int n = int(a.size());
    double scale = 0;
    for (auto &r : a)
        for (int j = 0; j < n; ++j) scale = std::max(scale, std::fabs(r[j]));
    if (scale <= 0) return false;
    for (int c = 0; c < n; ++c) {
        int piv = c;
        for (int r = c + 1; r < n; ++r)
            if (std::fabs(a[r][c]) > std::fabs(a[piv][c])) piv = r;
        if (std::fabs(a[piv][c]) < scale * 1e-13) return false;
        std::swap(a[c], a[piv]);
        for (int r = 0; r < n; ++r) {
            if (r == c) continue;
            const double f = a[r][c] / a[c][c];
            for (int j = c; j <= n; ++j) a[r][j] -= f * a[c][j];
        }
    }
    x.resize(n);
    for (int i = 0; i < n; ++i) x[i] = a[i][n] / a[i][i];
    return true;
}

// Least-squares circle through 2D points (Kasa fit, centred for conditioning)
bool fitCircle2d(const std::vector<std::pair<double, double>> &pts, double &cx, double &cy, double &r) {
    if (pts.size() < 3) return false;
    double mx = 0, my = 0;
    for (auto &p : pts) { mx += p.first; my += p.second; }
    mx /= pts.size(); my /= pts.size();
    double sxx = 0, sxy = 0, syy = 0, sx = 0, sy = 0, sxz = 0, syz = 0, sz = 0;
    for (auto &p : pts) {
        const double x = p.first - mx, y = p.second - my, z = x * x + y * y;
        sxx += x * x; sxy += x * y; syy += y * y; sx += x; sy += y; sxz += x * z; syz += y * z; sz += z;
    }
    std::vector<double> s;
    if (!solve({{sxx, sxy, sx, -sxz}, {sxy, syy, sy, -syz}, {sx, sy, double(pts.size()), -sz}}, s)) return false;
    const double ux = -s[0] / 2, uy = -s[1] / 2, rr = ux * ux + uy * uy - s[2];
    if (rr <= 0) return false;
    cx = ux + mx; cy = uy + my; r = std::sqrt(rr);
    return true;
}

double wrap2pi(double u) {
    u = std::fmod(u, 2 * kPi);
    return u < 0 ? u + 2 * kPi : u;
}
double angDist(double a, double b) {
    const double d = std::fabs(wrap2pi(a) - wrap2pi(b));
    return std::min(d, 2 * kPi - d);
}

struct Region {
    // Extrusion: a free-form profile (a spline through the mesh nodes) swept straight along ax.Direction()
    // Freeform: a B-spline height field over the plane of ax (normal = the outward mean normal), bounded like a
    // planar face by the region's outline
    enum Kind { Plane, Cylinder, Cone, Sphere, Extrusion, Freeform } kind = Plane;
    gp_Ax3 ax;                       // plane / surface position; indirect when the outside faces the axis
    Handle(Geom_Curve) profile;      // extrusion profile, in the plane through the origin normal to the direction
    Handle(Geom_Surface) surface;    // free-form surface
    Handle(Geom_BSplineSurface) height;   // the same in the ax frame: (u, v) -> (u, v, z)
    double dom[4] = {0, 0, 0, 0};    // its u / v range (umin, umax, vmin, vmax)
    double tol = 0;                  // how far a node may lie off it
    double radius = 0, angle = 0;    // cylinder / cone reference radius, sphere radius; cone semi-angle
    double u0 = 0, u1 = 2 * kPi, v0 = 0, v1 = 0;
    double residual = 0;
    bool dead = false;               // demoted to flat facets; owns no triangles any more
};

class Fitter {
public:
    // tolerance: absolute fit tolerance for planes and round surfaces; 0 derives it from the model size
    Fitter(Mesh mesh, bool prismatic, double tolerance = 0, Progress progress = {})
        : m(std::move(mesh)), prismatic(prismatic), fixedTol(tolerance), progress(std::move(progress)) {}

    TopoDS_Shape run(QString &error, QString &summary);
    // Region of every triangle as prismatic fitting splits the mesh, and whether it is a real surface
    // (curved, or a plane of two or more triangles) rather than a leftover facet
    bool segmentOnly(QString &error, std::vector<int> &labels, std::vector<char> &real);

private:
    bool prepare(QString &error);
    void classifyAll();
    Mesh m;
    bool prismatic;
    double diag = 1, tolPlane = 1e-6, fixedTol = 0;
    // Progress for the status bar: the fitting stages span [pFrom, pTo] of the whole operation; reports
    // are throttled to whole percents
    Progress progress;
    double pFrom = 0, pTo = 1, pLast = -1, pBase = 0.66;
    QString pWhat;
    void tick(double f, const QString &what, bool force = false) {
        if (!progress) return;
        const double v = pFrom + (pTo - pFrom) * std::clamp(f, 0.0, 1.0);
        if (!force && std::floor(v * 100) == std::floor(pLast * 100)) return;
        pLast = v;
        progress(v, what);
    }
    std::vector<gp_XYZ> nrm;
    std::vector<double> area;
    std::vector<std::array<int, 3>> nb;
    std::vector<int> label;
    std::vector<Region> regions;
    std::vector<int> stamp;          // per triangle: membership marker of the set being classified
    std::vector<int> nodeStamp;
    int curStamp = 0, curNodeStamp = 0;
    double maxResidual = 0;
    std::vector<int> badRegions;     // curved regions whose faces did not meet their neighbours (last buildShape)
    int ffFold = 0, ffResidual = 0, ffFacetTris = 0, ffFits = 0;   // free-form statistics (FIT_DEBUG)
    std::vector<std::vector<int>> smallPieces;   // free-form fragments too small to fit on their own
    bool grouped = false;                        // the mesh has face groups: each group is one face if it can be
    // Accurate face groups (Fusion): match simple primitives only — no free-form / extrusion mop-up
    bool groupsMode = false;
    double groupsBandTol = 0;   // plane-window while growing bands (mesh-scale); 0 → use tolPlane
    void absorbSmall();
    bool liesOn(const Region &r, const std::vector<int> &piece);
    bool pinched(const std::vector<int> &tris);
    void mopUp();
    int mopMerged = 0, mopRefits = 0, mopOwn = 0;   // mop-up statistics (FIT_DEBUG)

    bool buildTopology(QString &error);
    void demote(const std::vector<int> &regionIds, bool split = false);
    bool joinSlivers(QString &error);
    double tolSurf(double r) const { return std::max(fixedTol > 0 ? fixedTol * 2 : diag * 2e-5, r * 3e-4); }
    std::vector<int> nodesOf(const std::vector<int> &tris);
    std::vector<std::vector<int>> coplanarPieces(const std::vector<int> &tris);
    bool fitPlane(const std::vector<int> &tris, Region &r);
    bool fitCurved(const std::vector<int> &tris, Region &r);
    bool fitSurface(const std::vector<int> &tris, Region &r, const gp_XYZ *axisHint);
    bool crossAxis(const std::vector<int> &tris, gp_XYZ &axis) const;
    bool uvBounds(const std::vector<int> &tris, Region &r);
    struct Rim {
        gp_XYZ d;
        gp_Ax3 frame;
        double hmin = 0, hmax = 0, tolFit = 0, tolV = 0;
        std::vector<int> path, nodes;   // bottom rim in order; every node of the wall
        bool closed = false;
        std::vector<std::pair<int, int>> outline;
    };
    bool extrusionRim(const std::vector<int> &tris, Rim &rim);
    std::vector<int> rimCorners(const Rim &rim) const;
    bool splitAtRimCorners(const std::vector<int> &tris, std::vector<std::vector<int>> &parts);
    bool fitExtrusion(const std::vector<int> &tris, Region &r);
    double surfaceDistance(const Region &r, const gp_XYZ &p) const;
    bool normalAgrees(const Region &r, int t) const;
    void segment(const std::vector<int> &tris);
    void classify(const std::vector<int> &tris, int depth);
    // Split a patch across the bend (or longest axis) into connected halves for primitive matching
    bool splitAcrossBend(const std::vector<int> &tris, std::vector<std::vector<int>> &parts);
    void addFlat(const std::vector<int> &tris);
    void addRegion(const std::vector<int> &tris, const Region &r);
    std::vector<std::vector<int>> components(const std::vector<int> &tris);
    bool fitFreeform(const std::vector<int> &tris, Region &r);
    void addFreeform(const std::vector<int> &tris, int depth);
    TopoDS_Shape buildShape(QString &error);
};

bool Fitter::buildTopology(QString &error) {
    const int nt = int(m.tris.size());
    nrm.resize(nt);
    area.resize(nt);
    for (int t = 0; t < nt; ++t) {
        const auto &tr = m.tris[t];
        const gp_XYZ c = (m.nodes[tr[1]].XYZ() - m.nodes[tr[0]].XYZ()).Crossed(m.nodes[tr[2]].XYZ() - m.nodes[tr[0]].XYZ());
        const double l = c.Modulus();
        area[t] = l / 2;
        nrm[t] = l > 0 ? c / l : gp_XYZ(0, 0, 1);
    }
    nb.assign(nt, {-1, -1, -1});
    std::unordered_map<quint64, int> open;
    open.reserve(size_t(nt) * 2);
    for (int t = 0; t < nt; ++t)
        for (int k = 0; k < 3; ++k) {
            const int a = m.tris[t][k], b = m.tris[t][(k + 1) % 3];
            auto it = open.find(key2(a, b));
            if (it == open.end()) { open.emplace(key2(a, b), t * 3 + k); continue; }
            const int o = it->second;
            if (o < 0) { error = "The mesh has edges shared by more than two triangles"; return false; }
            const int ot = o / 3, ok = o % 3;
            if (m.tris[ot][ok] != b || m.tris[ot][(ok + 1) % 3] != a) {
                error = "The mesh triangles are not consistently oriented";
                return false;
            }
            nb[t][k] = ot;
            nb[ot][ok] = t;
            it->second = -1;
        }
    for (const auto &kv : open)
        if (kv.second >= 0) { error = "The mesh is not closed"; return false; }
    return true;
}

std::vector<int> Fitter::nodesOf(const std::vector<int> &tris) {
    ++curNodeStamp;
    std::vector<int> out;
    for (int t : tris)
        for (int v : m.tris[t])
            if (nodeStamp[v] != curNodeStamp) { nodeStamp[v] = curNodeStamp; out.push_back(v); }
    return out;
}

// Splits a triangle set into connected pieces of coplanar triangles (largest seeds first)
std::vector<std::vector<int>> Fitter::coplanarPieces(const std::vector<int> &tris) {
    const int mark = ++curStamp;
    for (int t : tris) stamp[t] = mark;
    std::vector<int> order = tris;
    std::sort(order.begin(), order.end(), [&](int a, int b) { return area[a] > area[b]; });
    // Convert leftovers stay nearly flat (0.3°). Accurate face groups use a wider normal gate with the
    // dialog boundary tolerance as the plane window (Fusion Accurate) — not a mesh-scale band, which
    // grew "almost flat" patches that fitPlane then shattered into one-triangle groups.
    const double cosTol = std::cos((groupsMode ? 8.0 : 0.3) * kPi / 180);
    const double band = groupsMode && groupsBandTol > 0 ? groupsBandTol : tolPlane;
    std::vector<std::vector<int>> pieces;
    const int taken = ++curStamp;
    for (int seed : order) {
        if (stamp[seed] != mark) continue;
        gp_XYZ n = nrm[seed];
        double d = n.Dot(m.nodes[m.tris[seed][0]].XYZ());
        std::vector<int> piece{seed};
        stamp[seed] = taken;
        size_t nextRefit = groupsMode ? 8 : size_t(-1);
        for (size_t i = 0; i < piece.size(); ++i) {
            for (int u : nb[piece[i]]) {
                if (u < 0 || stamp[u] != mark || (area[u] > 0 && nrm[u].Dot(n) < cosTol)) continue;
                bool flat = true;
                for (int v : m.tris[u]) flat = flat && std::fabs(n.Dot(m.nodes[v].XYZ()) - d) <= band;
                if (!flat) continue;
                stamp[u] = taken;
                piece.push_back(u);
            }
            // Accurate face groups: refit the plane as the band grows so a large CAD flat stays one
            // group under a tight Boundary Tolerance (Fusion Accurate), instead of stalling on the seed.
            if (piece.size() >= nextRefit) {
                gp_XYZ nn(0, 0, 0), cc(0, 0, 0);
                double aa = 0;
                for (int t : piece) {
                    nn += nrm[t] * area[t];
                    const auto &tr = m.tris[t];
                    cc += (m.nodes[tr[0]].XYZ() + m.nodes[tr[1]].XYZ() + m.nodes[tr[2]].XYZ()) * (area[t] / 3);
                    aa += area[t];
                }
                if (aa > 0 && nn.Modulus() > 1e-12) {
                    nn /= nn.Modulus();
                    cc /= aa;
                    bool ok = true;
                    for (int t : piece) {
                        if (area[t] > 0 && nrm[t].Dot(nn) < cosTol) { ok = false; break; }
                        for (int v : m.tris[t])
                            if (std::fabs(nn.Dot(m.nodes[v].XYZ() - cc)) > band) { ok = false; break; }
                        if (!ok) break;
                    }
                    if (ok) { n = nn; d = nn.Dot(cc); }
                }
                nextRefit = piece.size() * 2;
            }
        }
        pieces.push_back(std::move(piece));
    }
    return pieces;
}

bool Fitter::fitPlane(const std::vector<int> &tris, Region &r) {
    gp_XYZ n(0, 0, 0), c(0, 0, 0);
    double a = 0;
    for (int t : tris) {
        n += nrm[t] * area[t];
        const auto &tr = m.tris[t];
        c += (m.nodes[tr[0]].XYZ() + m.nodes[tr[1]].XYZ() + m.nodes[tr[2]].XYZ()) * (area[t] / 3);
        a += area[t];
    }
    if (a <= 0 || n.Modulus() < 1e-12) return false;
    n /= n.Modulus();
    c /= a;
    // Face groups: same normal gate as coplanarPieces (~8°). Convert stays at 0.3° so fitted planes
    // stay truly flat for B-rep edges.
    const double cosTol = std::cos((groupsMode ? 8.0 : 0.3) * kPi / 180);
    const double planeTol = groupsMode && groupsBandTol > 0 ? groupsBandTol : tolPlane;
    for (int t : tris)
        if (area[t] > 0 && nrm[t].Dot(n) < cosTol) return false;
    for (int v : nodesOf(tris))
        if (std::fabs(n.Dot(m.nodes[v].XYZ() - c)) > planeTol) return false;
    r = Region();
    r.kind = Region::Plane;
    r.ax = gp_Ax3(gp_Pnt(c), gp_Dir(n));
    return true;
}

bool Fitter::fitCurved(const std::vector<int> &tris, Region &r) {
    return fitSurface(tris, r, nullptr) && uvBounds(tris, r);
}

// Axis of a cylinder patch from the two most different normals (robust on narrow strips)
bool Fitter::crossAxis(const std::vector<int> &tris, gp_XYZ &axis) const {
    int a = -1;
    for (int t : tris)
        if (area[t] > 0) { a = t; break; }
    if (a < 0) return false;
    int b = -1;
    double best = 1;
    for (int t : tris)
        if (area[t] > 0 && nrm[t].Dot(nrm[a]) < best) { best = nrm[t].Dot(nrm[a]); b = t; }
    if (b < 0 || best > std::cos(2 * kPi / 180)) return false;
    axis = nrm[a].Crossed(nrm[b]);
    if (axis.Modulus() < 1e-9) return false;
    axis /= axis.Modulus();
    return true;
}

bool Fitter::fitSurface(const std::vector<int> &tris, Region &r, const gp_XYZ *axisHint) {
    double a = 0;
    gp_XYZ nbar(0, 0, 0);
    for (int t : tris) { nbar += nrm[t] * area[t]; a += area[t]; }
    if (a <= 0) return false;
    nbar /= a;
    gp_XYZ axis;
    if (axisHint) {
        axis = *axisHint;
    } else {
        double C[3][3] = {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}};
        for (int t : tris) {
            const gp_XYZ d = nrm[t] - nbar;
            for (int i = 0; i < 3; ++i)
                for (int j = 0; j < 3; ++j) C[i][j] += area[t] * d.Coord(i + 1) * d.Coord(j + 1) / a;
        }
        double w[3], V[3][3];
        eigenSym(C, w, V);
        const int mi = int(std::min_element(w, w + 3) - w);
        axis = gp_XYZ(V[0][mi], V[1][mi], V[2][mi]);
    }
    axis /= axis.Modulus();
    double c = 0, spread = 0;
    for (int t : tris) c += nrm[t].Dot(axis) * area[t] / a;
    for (int t : tris)
        if (area[t] > 0) spread = std::max(spread, std::fabs(nrm[t].Dot(axis) - c));
    const std::vector<int> nodes = nodesOf(tris);

    bool ok = false;
    if (spread < 0.05) {
        const gp_Dir zd(axis);
        const gp_Ax3 frame(gp_Pnt(0, 0, 0), zd);
        const gp_XYZ X = frame.XDirection().XYZ(), Y = frame.YDirection().XYZ();
        if (std::fabs(c) < 0.03) {
            std::vector<std::pair<double, double>> pts;
            for (int v : nodes) pts.emplace_back(m.nodes[v].XYZ().Dot(X), m.nodes[v].XYZ().Dot(Y));
            double cx, cy, rad;
            if (fitCircle2d(pts, cx, cy, rad)) {
                double res = 0;
                for (auto &p : pts) res = std::max(res, std::fabs(std::hypot(p.first - cx, p.second - cy) - rad));
                if (res < tolSurf(rad) && rad < 50 * diag) {
                    r = Region();
                    r.kind = Region::Cylinder;
                    r.ax = gp_Ax3(gp_Pnt(X * cx + Y * cy), zd, gp_Dir(X));
                    r.radius = rad;
                    r.residual = res;
                    ok = true;
                }
            }
        } else {
            // Cone about the known axis direction, fitted to the nodes (exact for nodes on the surface):
            // x^2 + y^2 = A x + B y + K z^2 + L z + M, with the axis at (A/2, B/2), K = tan^2, apex at z = -L / 2K
            gp_XYZ mean(0, 0, 0);
            for (int v : nodes) mean += m.nodes[v].XYZ();
            mean /= double(nodes.size());
            std::vector<std::vector<double>> N(5, std::vector<double>(6, 0.0));
            for (int v : nodes) {
                const gp_XYZ p = m.nodes[v].XYZ() - mean;
                const double x = p.Dot(X), y = p.Dot(Y), z = p.Dot(axis);
                const double row[5] = {x, y, z * z, z, 1.0}, rhs = x * x + y * y;
                for (int i = 0; i < 5; ++i) {
                    for (int j = 0; j < 5; ++j) N[i][j] += row[i] * row[j];
                    N[i][5] += row[i] * rhs;
                }
            }
            std::vector<double> x;
            if (nodes.size() >= 6 && solve(N, x) && x[2] > 1e-8) {
                const gp_XYZ apex = mean + X * (x[0] / 2) + Y * (x[1] / 2) + axis * (-x[3] / (2 * x[2]));
                double hs = 0;
                for (int v : nodes) hs += (m.nodes[v].XYZ() - apex).Dot(axis);
                const gp_XYZ ax = hs < 0 ? axis.Reversed() : axis;
                double sumAng = 0, hMax = 0;
                int cnt = 0;
                for (int v : nodes) {
                    const gp_XYZ d = m.nodes[v].XYZ() - apex;
                    const double h = d.Dot(ax), rho = (d - ax * h).Modulus();
                    hMax = std::max(hMax, h);
                    if (h > diag * 1e-6) { sumAng += std::atan2(rho, h); ++cnt; }
                }
                if (cnt > 2 && hMax > 0) {
                    const double alpha = sumAng / cnt;
                    double res = 0;
                    for (int v : nodes) {
                        const gp_XYZ d = m.nodes[v].XYZ() - apex;
                        const double h = d.Dot(ax), rho = (d - ax * h).Modulus();
                        res = std::max(res, std::fabs(rho * std::cos(alpha) - h * std::sin(alpha)));
                    }
                    const double R = hMax * std::tan(alpha);
                    if (alpha > 0.5 * kPi / 180 && alpha < 89.5 * kPi / 180 && res < tolSurf(R) && hMax < 50 * diag) {
                        r = Region();
                        r.kind = Region::Cone;
                        const gp_Dir zc(ax);
                        r.ax = gp_Ax3(gp_Pnt(apex + ax * hMax), zc, gp_Ax3(gp_Pnt(0, 0, 0), zc).XDirection());
                        r.radius = R;
                        r.angle = alpha;
                        r.residual = res;
                        ok = true;
                    }
                }
            }
        }
    }
    if (!ok) {
        // Sphere: x^2 + y^2 + z^2 + Dx + Ey + Fz + G = 0
        gp_XYZ mean(0, 0, 0);
        for (int v : nodes) mean += m.nodes[v].XYZ();
        mean /= double(nodes.size());
        std::vector<std::vector<double>> A(4, std::vector<double>(5, 0.0));
        for (int v : nodes) {
            const gp_XYZ p = m.nodes[v].XYZ() - mean;
            const double row[4] = {p.X(), p.Y(), p.Z(), 1.0}, rhs = -p.SquareModulus();
            for (int i = 0; i < 4; ++i) {
                for (int j = 0; j < 4; ++j) A[i][j] += row[i] * row[j];
                A[i][4] += row[i] * rhs;
            }
        }
        std::vector<double> s;
        if (solve(A, s)) {
            const gp_XYZ cen(-s[0] / 2, -s[1] / 2, -s[2] / 2);
            const double rr = cen.SquareModulus() - s[3];
            if (rr > 0) {
                const double R = std::sqrt(rr);
                double res = 0;
                for (int v : nodes) res = std::max(res, std::fabs((m.nodes[v].XYZ() - mean - cen).Modulus() - R));
                if (res < tolSurf(R) && R < 50 * diag) {
                    r = Region();
                    r.kind = Region::Sphere;
                    const gp_Dir z = nbar.Modulus() > 0.1 ? gp_Dir(nbar) : gp_Dir(0, 0, 1);
                    r.ax = gp_Ax3(gp_Pnt(cen + mean), z, gp_Ax3(gp_Pnt(0, 0, 0), z).XDirection());
                    r.radius = R;
                    r.residual = res;
                    ok = true;
                }
            }
        }
    }
    if (!ok) return false;

    // Outward side: make the surface normal agree with the triangles
    const gp_XYZ O = r.ax.Location().XYZ(), Z = r.ax.Direction().XYZ();
    double agree = 0;
    for (int t : tris) {
        const auto &tr = m.tris[t];
        const gp_XYZ p = (m.nodes[tr[0]].XYZ() + m.nodes[tr[1]].XYZ() + m.nodes[tr[2]].XYZ()) / 3;
        gp_XYZ d = p - O, out;
        if (r.kind == Region::Sphere) out = d;
        else {
            gp_XYZ radial = d - Z * d.Dot(Z);
            if (radial.Modulus() < 1e-12) continue;
            radial /= radial.Modulus();
            out = r.kind == Region::Cone ? radial * std::cos(r.angle) - Z * std::sin(r.angle) : radial;
        }
        agree += out.Dot(nrm[t]) * area[t];
    }
    if (agree < 0) r.ax.YReverse();
    return true;
}

// Parameter box of a curved region; false when its outline is not made of iso-lines (then the face cannot
// be a plain u/v rectangle on the surface and the region is kept faceted)
bool Fitter::uvBounds(const std::vector<int> &tris, Region &r) {
    const gp_XYZ O = r.ax.Location().XYZ(), X = r.ax.XDirection().XYZ(), Y = r.ax.YDirection().XYZ(),
                 Z = r.ax.Direction().XYZ();
    auto uv = [&](int node, double &u, double &v, double &rho) {
        const gp_XYZ d = m.nodes[node].XYZ() - O;
        u = wrap2pi(std::atan2(d.Dot(Y), d.Dot(X)));
        const double h = d.Dot(Z);
        rho = (d - Z * h).Modulus();
        if (r.kind == Region::Cylinder) v = h;
        else if (r.kind == Region::Cone) v = h / std::cos(r.angle);
        else v = std::asin(std::clamp(h / r.radius, -1.0, 1.0));
    };
    const int mark = ++curStamp;
    for (int t : tris) stamp[t] = mark;
    ++curNodeStamp;
    std::vector<int> boundary;
    std::vector<std::pair<int, int>> boundaryEdges;
    for (int t : tris)
        for (int k = 0; k < 3; ++k)
            if (stamp[nb[t][k]] != mark) {
                boundaryEdges.push_back({m.tris[t][k], m.tris[t][(k + 1) % 3]});
                for (int v : {m.tris[t][k], m.tris[t][(k + 1) % 3]})
                    if (nodeStamp[v] != curNodeStamp) { nodeStamp[v] = curNodeStamp; boundary.push_back(v); }
            }

    std::vector<double> us;
    double vmin = 1e300, vmax = -1e300;
    for (int n : nodesOf(tris)) {
        double u, v, rho;
        uv(n, u, v, rho);
        if (rho > tolSurf(r.radius)) us.push_back(u);   // apex / pole nodes have no angle
        vmin = std::min(vmin, v);
        vmax = std::max(vmax, v);
    }
    if (us.size() < 3) return false;
    const double vScale = r.kind == Region::Sphere ? r.radius : 1.0;
    const double tolV = tolSurf(r.radius) * 4 / vScale;
    auto onV = [&](double v) { return std::fabs(v - vmin) <= tolV || std::fabs(v - vmax) <= tolV; };
    auto limit = [&](double v) { return std::fabs(v - vmin) <= tolV ? 0 : std::fabs(v - vmax) <= tolV ? 1 : -1; };

    // A full band only has outline edges running along its two end circles (a quarter-round fillet
    // can have every outline node on an end circle, but its straight sides join the two)
    bool full = true;
    for (const auto &e : boundaryEdges) {
        double ua, va, ra, ub, vb, rb;
        uv(e.first, ua, va, ra);
        uv(e.second, ub, vb, rb);
        const int la = limit(va), lb = limit(vb);
        if (la < 0 || lb < 0 || la != lb) { full = false; break; }
    }
    double u0 = 0, u1 = 2 * kPi;
    if (!full) {
        std::sort(us.begin(), us.end());
        double gap = us.front() + 2 * kPi - us.back();
        u0 = us.front();
        for (size_t i = 1; i < us.size(); ++i)
            if (us[i] - us[i - 1] > gap) { gap = us[i] - us[i - 1]; u0 = us[i]; }
        u1 = u0 + 2 * kPi - gap;
        for (int n : boundary) {
            double u, v, rho;
            uv(n, u, v, rho);
            if (onV(v) || rho <= tolSurf(r.radius)) continue;
            const double tolU = tolSurf(r.radius) * 4 / rho;
            if (angDist(u, u0) > tolU && angDist(u, u1) > tolU) return false;
        }
    }
    if (r.kind == Region::Sphere) {
        if (vmax > kPi / 2 - tolV) vmax = kPi / 2;
        if (vmin < -kPi / 2 + tolV) vmin = -kPi / 2;
    } else if (r.kind == Region::Cone) {
        const double vApex = -r.radius / std::sin(r.angle);
        if (vmin < vApex + tolV) vmin = vApex;
    }
    if (vmax - vmin <= tolV) return false;
    r.u0 = u0; r.u1 = u1; r.v0 = vmin; r.v1 = vmax;
    return true;
}

// A free-form wall swept straight along one direction (an extruded spline): every normal is square to the
// direction, the bottom outline is the profile, and the outline runs only along the bottom and top rims
// (plus, on an open wall, straight sides along the direction). Built as a u/v rectangle like the cylinders.
bool Fitter::extrusionRim(const std::vector<int> &tris, Rim &rim) {
    double C[3][3] = {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}}, a = 0;
    for (int t : tris) {
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j) C[i][j] += area[t] * nrm[t].Coord(i + 1) * nrm[t].Coord(j + 1);
        a += area[t];
    }
    if (a <= 0) return false;
    double w[3], V[3][3];
    eigenSym(C, w, V);
    const int mi = int(std::min_element(w, w + 3) - w);
    gp_XYZ d(V[0][mi], V[1][mi], V[2][mi]);
    if (d.Modulus() < 1e-9) return false;
    d /= d.Modulus();
    for (int t : tris)
        if (area[t] > 0 && std::fabs(nrm[t].Dot(d)) > 0.035) return false;
    const gp_Ax3 frame(gp_Pnt(0, 0, 0), gp_Dir(d));
    const gp_XYZ X = frame.XDirection().XYZ(), Y = frame.YDirection().XYZ();

    const std::vector<int> nodes = nodesOf(tris);
    double hmin = 1e300, hmax = -1e300;
    for (int v : nodes) {
        const double h = m.nodes[v].XYZ().Dot(d);
        hmin = std::min(hmin, h);
        hmax = std::max(hmax, h);
    }
    const double tolFit = std::max(diag * 1e-4, 1e-6), tolV = tolFit * 4;
    if (hmax - hmin <= tolV * 4) return false;
    auto height = [&](int v) { return m.nodes[v].XYZ().Dot(d); };

    // Outline half-edges (in the triangles' sense); the bottom rim's run gives the profile with the
    // outside on the right side for C'(u) x d
    const int mark = ++curStamp;
    for (int t : tris) stamp[t] = mark;
    std::unordered_map<int, int> nextOnRim;
    std::unordered_set<int> hasPrev;
    std::vector<std::pair<int, int>> outline;
    for (int t : tris)
        for (int k = 0; k < 3; ++k)
            if (stamp[nb[t][k]] != mark) {
                const int p = m.tris[t][k], q = m.tris[t][(k + 1) % 3];
                outline.push_back({p, q});
                if (std::fabs(height(p) - hmin) <= tolV && std::fabs(height(q) - hmin) <= tolV) {
                    if (nextOnRim.count(p)) return false;   // branching rim
                    nextOnRim[p] = q;
                    hasPrev.insert(q);
                }
            }
    if (nextOnRim.size() < 3) return false;
    int start = nextOnRim.begin()->first;
    for (const auto &kv : nextOnRim)
        if (!hasPrev.count(kv.first)) { start = kv.first; break; }
    std::vector<int> path{start};
    bool closed = false;
    for (auto it = nextOnRim.find(start); it != nextOnRim.end(); it = nextOnRim.find(path.back())) {
        if (it->second == start) { closed = true; break; }
        path.push_back(it->second);
        if (path.size() > nextOnRim.size() + 1) return false;
    }
    if (path.size() != nextOnRim.size() + (closed ? 0 : 1) || path.size() < 4) return false;
    rim.d = d;
    rim.frame = frame;
    rim.hmin = hmin;
    rim.hmax = hmax;
    rim.tolFit = tolFit;
    rim.tolV = tolV;
    rim.path = std::move(path);
    rim.closed = closed;
    rim.outline = std::move(outline);
    rim.nodes = nodes;
    return true;
}

// Rim nodes where the profile turns sharply compared with its neighbours (index into rim.path)
std::vector<int> Fitter::rimCorners(const Rim &rim) const {
    const auto &path = rim.path;
    const int n = int(path.size());
    const gp_XYZ X = rim.frame.XDirection().XYZ(), Y = rim.frame.YDirection().XYZ();
    auto flat = [&](int i) {
        const gp_XYZ p = m.nodes[path[((i % n) + n) % n]].XYZ();
        return std::make_pair(p.Dot(X), p.Dot(Y));
    };
    std::vector<double> turn(n, 0.0);
    for (int i = 0; i < n; ++i) {
        if (!rim.closed && (i == 0 || i == n - 1)) continue;
        const auto a = flat(i - 1), b = flat(i), c = flat(i + 1);
        const double a1 = std::atan2(b.second - a.second, b.first - a.first), a2 = std::atan2(c.second - b.second, c.first - b.first);
        double t = std::fabs(a2 - a1);
        if (t > kPi) t = 2 * kPi - t;
        turn[i] = t;
    }
    std::vector<int> corners;
    for (int i = 0; i < n; ++i) {
        const double prev = (rim.closed || i > 0) ? turn[(i + n - 1) % n] : 0, next = (rim.closed || i < n - 1) ? turn[(i + 1) % n] : 0;
        if (turn[i] > 10 * kPi / 180 && turn[i] > 2.5 * std::max(prev, next)) corners.push_back(i);
    }
    return corners;
}

// A swept wall whose profile has corners: its triangles split at the corners into smooth runs
bool Fitter::splitAtRimCorners(const std::vector<int> &tris, std::vector<std::vector<int>> &parts) {
    Rim rim;
    if (!extrusionRim(tris, rim)) return false;
    const std::vector<int> corners = rimCorners(rim);
    if (corners.empty() || rim.path.size() * rim.nodes.size() > 30000000) return false;
    const int n = int(rim.path.size());
    const gp_XYZ X = rim.frame.XDirection().XYZ(), Y = rim.frame.YDirection().XYZ();
    // Column of each node: the rim node right below it
    std::unordered_map<int, int> column;
    for (int v : rim.nodes) {
        const gp_XYZ p = m.nodes[v].XYZ();
        int best = -1;
        double bestD = 1e300;
        for (int i = 0; i < n; ++i) {
            const gp_XYZ q = m.nodes[rim.path[i]].XYZ();
            const double dx = (p - q).Dot(X), dy = (p - q).Dot(Y), dd = dx * dx + dy * dy;
            if (dd < bestD) { bestD = dd; best = i; }
        }
        if (bestD > rim.tolV * rim.tolV) return false;   // not a straight sweep
        column[v] = best;
    }
    // Runs between corners; a triangle spans two neighbouring columns, so it belongs to the run holding both
    std::vector<int> runOf(n, 0);
    int run = 0;
    const int first = rim.closed ? corners.front() : 0;
    for (int k = 0; k < n; ++k) {
        const int i = (first + k) % n;
        if (k > 0 && std::find(corners.begin(), corners.end(), i) != corners.end()) ++run;
        runOf[i] = run;
    }
    const int runs = run + 1;
    if (runs < 2) return false;
    parts.assign(runs, {});
    for (int t : tris) {
        int lo = n, hi = -1;
        for (int v : m.tris[t]) {
            const int c = (column[v] - first + n) % n;
            lo = std::min(lo, c);
            hi = std::max(hi, c);
        }
        // The segment starts at its lower column (on a closed rim the one before the wrap)
        const int startCol = (rim.closed && hi - lo > n / 2) ? hi : lo;
        parts[runOf[(first + startCol) % n]].push_back(t);
    }
    parts.erase(std::remove_if(parts.begin(), parts.end(), [](const std::vector<int> &p) { return p.empty(); }), parts.end());
    return parts.size() >= 2;
}

bool Fitter::fitExtrusion(const std::vector<int> &tris, Region &r) {
    Rim rim;
    if (!extrusionRim(tris, rim) || !rimCorners(rim).empty()) return false;
    const gp_XYZ d = rim.d, X = rim.frame.XDirection().XYZ(), Y = rim.frame.YDirection().XYZ();
    const gp_Ax3 frame = rim.frame;
    const double hmin = rim.hmin, hmax = rim.hmax, tolFit = rim.tolFit, tolV = rim.tolV;
    const std::vector<int> &path = rim.path, &nodes = rim.nodes;
    const auto &outline = rim.outline;
    const bool closed = rim.closed;
    auto height = [&](int v) { return m.nodes[v].XYZ().Dot(d); };

    Handle(TColgp_HArray1OfPnt) pts = new TColgp_HArray1OfPnt(1, int(path.size()));
    for (int i = 0; i < int(path.size()); ++i) {
        const gp_XYZ p = m.nodes[path[i]].XYZ();
        pts->SetValue(i + 1, gp_Pnt(X * p.Dot(X) + Y * p.Dot(Y)));
    }
    Handle(Geom_BSplineCurve) curve;
    try {
        GeomAPI_Interpolate interp(pts, closed, tolFit * 1e-3);
        interp.Perform();
        if (!interp.IsDone()) return false;
        curve = interp.Curve();
    } catch (const Standard_Failure &) {
        return false;
    }

    // Every node on the swept profile; outline nodes off the rims lie on the open wall's straight ends
    const gp_Pnt e0 = curve->Value(curve->FirstParameter()), e1 = curve->Value(curve->LastParameter());
    double res = 0;
    std::unordered_set<int> boundary;
    for (const auto &e : outline) { boundary.insert(e.first); boundary.insert(e.second); }
    GeomAPI_ProjectPointOnCurve proj;
    for (int v : nodes) {
        const gp_XYZ p = m.nodes[v].XYZ();
        const gp_Pnt flat(X * p.Dot(X) + Y * p.Dot(Y));
        proj.Init(flat, curve);
        if (proj.NbPoints() == 0) return false;
        res = std::max(res, proj.LowerDistance());
        if (res > tolFit) return false;
        const double h = height(v);
        if (boundary.count(v) && std::fabs(h - hmin) > tolV && std::fabs(h - hmax) > tolV) {
            if (closed || std::min(flat.Distance(e0), flat.Distance(e1)) > tolV) return false;
        }
    }
    // The swept surface's normal C'(u) x d must face the way the triangles do
    double agree = 0;
    for (int t : tris) {
        const auto &tr = m.tris[t];
        const gp_XYZ c = (m.nodes[tr[0]].XYZ() + m.nodes[tr[1]].XYZ() + m.nodes[tr[2]].XYZ()) / 3;
        proj.Init(gp_Pnt(X * c.Dot(X) + Y * c.Dot(Y)), curve);
        if (proj.NbPoints() == 0) continue;
        gp_Pnt p;
        gp_Vec tan;
        curve->D1(proj.LowerDistanceParameter(), p, tan);
        agree += tan.XYZ().Crossed(d).Dot(nrm[t]) * area[t];
    }
    if (agree == 0) return false;
    if (agree < 0) curve->Reverse();
    if (qEnvironmentVariableIsSet("FIT_DEBUG")) {
        const gp_Pnt c0 = curve->Value(curve->FirstParameter()), c1 = curve->Value(curve->LastParameter());
        std::fprintf(stderr, "  [fit] extrusion: %zu tris, rim %zu nodes, closed %d, dir (%.2f %.2f %.2f), h %.3f..%.3f, "
                             "ends (%.3f %.3f %.3f)-(%.3f %.3f %.3f), agree %.3f\n",
                     tris.size(), path.size(), int(closed), d.X(), d.Y(), d.Z(), hmin, hmax, c0.X(), c0.Y(), c0.Z(), c1.X(),
                     c1.Y(), c1.Z(), agree);
    }

    r = Region();
    r.kind = Region::Extrusion;
    r.ax = frame;
    r.profile = curve;
    r.u0 = curve->FirstParameter();
    r.u1 = curve->LastParameter();
    r.v0 = hmin;
    r.v1 = hmax;
    r.residual = res;
    return true;
}

void Fitter::addRegion(const std::vector<int> &tris, const Region &r) {
    const int id = int(regions.size());
    regions.push_back(r);
    maxResidual = std::max(maxResidual, r.residual);
    for (int t : tris) label[t] = id;
}

// A flat piece; if it fails the plane check as a whole, one face per triangle — except face groups /
// grouped Convert, where coplanarPieces (or the whole group) must stay one face like Fusion.
void Fitter::addFlat(const std::vector<int> &tris) {
    Region r;
    if (fitPlane(tris, r)) { addRegion(tris, r); return; }
    if ((groupsMode || grouped) && tris.size() >= 2) {
        gp_XYZ n(0, 0, 0), c(0, 0, 0);
        double a = 0;
        for (int t : tris) {
            n += nrm[t] * area[t];
            const auto &tr = m.tris[t];
            c += (m.nodes[tr[0]].XYZ() + m.nodes[tr[1]].XYZ() + m.nodes[tr[2]].XYZ()) * (area[t] / 3);
            a += area[t];
        }
        if (a > 0 && n.Modulus() > 1e-12) {
            n /= n.Modulus();
            c /= a;
            r = Region();
            r.kind = Region::Plane;
            r.ax = gp_Ax3(gp_Pnt(c), gp_Dir(n));
            addRegion(tris, r);
            return;
        }
    }
    for (int t : tris)
        if (fitPlane({t}, r)) addRegion({t}, r);
}

// Connected pieces of a triangle set
std::vector<std::vector<int>> Fitter::components(const std::vector<int> &tris) {
    const int mark = ++curStamp;
    for (int t : tris) stamp[t] = mark;
    const int done = ++curStamp;
    std::vector<std::vector<int>> out;
    for (int s : tris) {
        if (stamp[s] != mark) continue;
        std::vector<int> comp{s};
        stamp[s] = done;
        for (size_t i = 0; i < comp.size(); ++i)
            for (int u : nb[comp[i]])
                if (u >= 0 && stamp[u] == mark) { stamp[u] = done; comp.push_back(u); }
        out.push_back(std::move(comp));
    }
    return out;
}

// A smooth patch as one B-spline face. Heights over the patch's mean plane are sampled on a grid: inside the
// triangles by interpolation, outside them by spreading the nearest heights and smoothing, so the surface runs
// on a little past the outline. Fails when the patch folds over its plane or the surface strays from the nodes.
bool Fitter::fitFreeform(const std::vector<int> &tris, Region &r) {
    gp_XYZ n(0, 0, 0), c(0, 0, 0);
    double a = 0;
    for (int t : tris) {
        n += nrm[t] * area[t];
        const auto &tr = m.tris[t];
        c += (m.nodes[tr[0]].XYZ() + m.nodes[tr[1]].XYZ() + m.nodes[tr[2]].XYZ()) * (area[t] / 3);
        a += area[t];
    }
    if (a <= 0 || n.Modulus() < 1e-12) return false;
    n /= n.Modulus();
    c /= a;
    for (int t : tris)
        if (area[t] > 0 && nrm[t].Dot(n) < 0.2) { ++ffFold; return false; }
    const gp_Ax3 ax{gp_Pnt(c), gp_Dir(n)};
    const gp_XYZ X = ax.XDirection().XYZ(), Y = ax.YDirection().XYZ();
    const std::vector<int> nodes = nodesOf(tris);
    std::unordered_map<int, gp_XYZ> loc;
    loc.reserve(nodes.size());
    double lo[2] = {1e300, 1e300}, hi[2] = {-1e300, -1e300};
    for (int v : nodes) {
        const gp_XYZ d = m.nodes[v].XYZ() - c;
        const gp_XYZ p(d.Dot(X), d.Dot(Y), d.Dot(n));
        loc.emplace(v, p);
        lo[0] = std::min(lo[0], p.X()); hi[0] = std::max(hi[0], p.X());
        lo[1] = std::min(lo[1], p.Y()); hi[1] = std::max(hi[1], p.Y());
    }
    const double w = hi[0] - lo[0], h = hi[1] - lo[1], size = std::max(w, h);
    if (size <= 0 || std::min(w, h) <= size * 1e-6) return false;
    const double margin = size * 0.05;
    for (int k = 0; k < 2; ++k) { lo[k] -= margin; hi[k] += margin; }
    // Bicubic height field z(x, y) on clamped uniform knots, least squares through the nodes plus a small
    // bending term on the control net (which also carries the surface on past the outline). The poles sit at
    // the Greville abscissae, so S(u, v) = (u, v, z).
    // A smooth surface cannot follow the flat facets exactly: allow about the mesh's own chord error
    double edgeSum = 0;
    for (int t : tris)
        for (int k = 0; k < 3; ++k) edgeSum += m.nodes[m.tris[t][k]].Distance(m.nodes[m.tris[t][(k + 1) % 3]]);
    const double tolFree = std::max(diag * 3e-4, edgeSum / (3.0 * tris.size()) * 0.05);
    Handle(Geom_BSplineSurface) s;
    const double cells = std::clamp(std::sqrt(double(nodes.size())) * 0.6, 4.0, 40.0);
    const double spacing = size / cells;
    const int nu = std::max(4, int(std::ceil((hi[0] - lo[0]) / spacing)) + 3);
    const int nv = std::max(4, int(std::ceil((hi[1] - lo[1]) / spacing)) + 3);
    struct Basis { int span; double b[4]; };
    auto basis = [](double t, double a, double b, int n) {
        // Cubic B-spline basis on n poles, knots a(x4), interior uniform, b(x4)
        const int spans = n - 3;
        const double h = (b - a) / spans;
        const double s = std::clamp((t - a) / h, 0.0, double(spans) * (1 - 1e-12));
        const int k = int(s);
        auto knot = [&](int i) { return a + std::clamp(i - 3, 0, spans) * h; };   // knot index i of n + 4
        Basis out;
        out.span = k;   // poles k .. k + 3
        const int sp = k + 3;
        double left[4], right[4], N[4] = {1, 0, 0, 0};
        for (int j = 1; j <= 3; ++j) {
            left[j] = t - knot(sp + 1 - j);
            right[j] = knot(sp + j) - t;
            double saved = 0;
            for (int r = 0; r < j; ++r) {
                const double den = right[r + 1] + left[j - r];
                const double tmp = den != 0 ? N[r] / den : 0;
                N[r] = saved + right[r + 1] * tmp;
                saved = left[j - r] * tmp;
            }
            N[j] = saved;
        }
        for (int i = 0; i < 4; ++i) out.b[i] = N[i];
        return out;
    };
    // The nodes, plus points along long edges and inside big triangles: between sparse nodes the surface
    // would otherwise be free to bulge away from the flat triangles
    // The rim is what the face's edges follow, so it is sampled densely and weighted heavily: the surface
    // may smooth over the chords inside, but must run along its outline
    struct Pt { gp_XYZ p; double w; bool rim; };
    std::vector<Pt> pts;
    pts.reserve(loc.size() * 2);
    {
        const int mark = ++curStamp;
        for (int t : tris) stamp[t] = mark;
        const double step = spacing, rimStep = spacing * 0.5, rimWeight = 8;
        ++curNodeStamp;
        for (int t : tris) {
            const auto &tr = m.tris[t];
            const gp_XYZ p[3] = {loc[tr[0]], loc[tr[1]], loc[tr[2]]};
            for (int k = 0; k < 3; ++k) {
                const int u = nb[t][k];
                const bool rim = u < 0 || stamp[u] != mark;
                if (!rim && u < t) continue;   // each inner edge once
                const gp_XYZ a = p[k], b = p[(k + 1) % 3];
                const int segs = std::max(1, int(std::floor((b - a).Modulus() / (rim ? rimStep : step))));
                for (int i = 1; i < segs; ++i) pts.push_back({a + (b - a) * (double(i) / segs), rim ? rimWeight : 1.0, rim});
                if (rim)
                    for (int v : {tr[k], tr[(k + 1) % 3]})
                        if (nodeStamp[v] != curNodeStamp) { nodeStamp[v] = curNodeStamp; pts.push_back({loc[v], rimWeight, true}); }
            }
            if (area[t] > step * step) pts.push_back({(p[0] + p[1] + p[2]) / 3, 1.0, false});
        }
        for (const auto &kv : loc)
            if (nodeStamp[kv.first] != curNodeStamp) pts.push_back({kv.second, 1.0, false});
    }
    struct Sample { Basis bu, bv; double z, w; };
    std::vector<Sample> data;
    data.reserve(pts.size());
    for (const Pt &q : pts)
        data.push_back({basis(q.p.X(), lo[0], hi[0], nu), basis(q.p.Y(), lo[1], hi[1], nv), q.p.Z(), q.w});
    const int np = nu * nv;
    const double lambda = 1e-3 * double(data.size()) / np + 1e-9;
    // y = (A^T W A + lambda D^T D) x
    auto apply = [&](const std::vector<double> &x, std::vector<double> &y) {
        std::fill(y.begin(), y.end(), 0.0);
        for (const Sample &d : data) {
            double v = 0;
            for (int i = 0; i < 4; ++i)
                for (int j = 0; j < 4; ++j) v += d.bu.b[i] * d.bv.b[j] * x[(d.bu.span + i) * nv + d.bv.span + j];
            v *= d.w;
            for (int i = 0; i < 4; ++i)
                for (int j = 0; j < 4; ++j) y[(d.bu.span + i) * nv + d.bv.span + j] += d.bu.b[i] * d.bv.b[j] * v;
        }
        auto bend = [&](int i0, int j0, int i1, int j1, int i2, int j2) {
            const int a = i0 * nv + j0, b = i1 * nv + j1, c2 = i2 * nv + j2;
            const double s = x[a] - 2 * x[b] + x[c2];
            y[a] += lambda * s; y[b] -= 2 * lambda * s; y[c2] += lambda * s;
        };
        for (int i = 0; i < nu; ++i)
            for (int j = 0; j < nv; ++j) {
                if (i + 2 < nu) bend(i, j, i + 1, j, i + 2, j);
                if (j + 2 < nv) bend(i, j, i, j + 1, i, j + 2);
                if (i + 1 < nu && j + 1 < nv) {
                    const int p00 = i * nv + j, p10 = (i + 1) * nv + j, p01 = i * nv + j + 1, p11 = (i + 1) * nv + j + 1;
                    const double s = x[p11] - x[p10] - x[p01] + x[p00];
                    const double l2 = 2 * lambda;
                    y[p11] += l2 * s; y[p10] -= l2 * s; y[p01] -= l2 * s; y[p00] += l2 * s;
                }
            }
    };
    std::vector<double> rhs(np, 0.0), x(np, 0.0), rr(np), pp(np), ap(np);
    double meanZ = 0, sumW = 0;
    for (const Sample &d : data) {
        meanZ += d.z * d.w;
        sumW += d.w;
        for (int i = 0; i < 4; ++i)
            for (int j = 0; j < 4; ++j) rhs[(d.bu.span + i) * nv + d.bv.span + j] += d.bu.b[i] * d.bv.b[j] * d.z * d.w;
    }
    std::fill(x.begin(), x.end(), meanZ / sumW);
    apply(x, ap);
    double rs = 0, rs0 = 0;
    for (int i = 0; i < np; ++i) { rr[i] = rhs[i] - ap[i]; pp[i] = rr[i]; rs += rr[i] * rr[i]; }
    for (double v : rhs) rs0 += v * v;
    for (int it = 0; it < 4 * np && rs > rs0 * 1e-24; ++it) {
        apply(pp, ap);
        double pap = 0;
        for (int i = 0; i < np; ++i) pap += pp[i] * ap[i];
        if (pap <= 0) break;
        const double alpha = rs / pap;
        double rsNew = 0;
        for (int i = 0; i < np; ++i) { x[i] += alpha * pp[i]; rr[i] -= alpha * ap[i]; rsNew += rr[i] * rr[i]; }
        for (int i = 0; i < np; ++i) pp[i] = rr[i] + rsNew / rs * pp[i];
        rs = rsNew;
    }
    auto greville = [](int i, double a, double b, int n) {
        const int spans = n - 3;
        const double h = (b - a) / spans;
        double s = 0;
        for (int k = i + 1; k <= i + 3; ++k) s += a + std::clamp(k - 3, 0, spans) * h;
        return s / 3;
    };
    TColgp_Array2OfPnt poles(1, nu, 1, nv);
    for (int i = 0; i < nu; ++i)
        for (int j = 0; j < nv; ++j)
            poles(i + 1, j + 1) = gp_Pnt(greville(i, lo[0], hi[0], nu), greville(j, lo[1], hi[1], nv), x[i * nv + j]);
    auto knots = [](double a, double b, int n, TColStd_Array1OfReal &k, TColStd_Array1OfInteger &mult) {
        const int spans = n - 3;
        k.Resize(1, spans + 1, Standard_False);
        mult.Resize(1, spans + 1, Standard_False);
        for (int i = 0; i <= spans; ++i) { k(i + 1) = a + (b - a) * i / spans; mult(i + 1) = (i == 0 || i == spans) ? 4 : 1; }
    };
    TColStd_Array1OfReal uk(1, 2), vk(1, 2);
    TColStd_Array1OfInteger um(1, 2), vm(1, 2);
    knots(lo[0], hi[0], nu, uk, um);
    knots(lo[1], hi[1], nv, vk, vm);
    try {
        s = new Geom_BSplineSurface(poles, uk, vk, um, vm, 3, 3);
    } catch (const Standard_Failure &) {
        return false;
    }
    // S(u, v) = (u, v, height): the residual is the height difference at every node
    double res = 0, rimRes = 0;
    for (const Pt &q : pts) {
        const double d = std::fabs(s->Value(q.p.X(), q.p.Y()).Z() - q.p.Z());
        res = std::max(res, d);
        if (q.rim) rimRes = std::max(rimRes, d);
    }
    if (qEnvironmentVariableIsSet("FIT_DEBUG") && tris.size() > 200)
        std::fprintf(stderr, "  [fit] free-form %zu tris poles %dx%d residual %.4g rim %.4g (tol %.4g)\n", tris.size(), nu, nv, res, rimRes, tolFree);
    if (res > tolFree) { ++ffResidual; return false; }
    ++ffFits;
    gp_Trsf toWorld;
    toWorld.SetTransformation(ax, gp_Ax3());
    r = Region();
    r.kind = Region::Freeform;
    r.ax = ax;
    r.surface = Handle(Geom_Surface)::DownCast(s->Transformed(toWorld));
    r.height = s;
    r.dom[0] = lo[0]; r.dom[1] = hi[0]; r.dom[2] = lo[1]; r.dom[3] = hi[1];
    r.tol = tolFree;
    r.residual = rimRes;   // what the face's edges are off by; inside, the chords may be smoothed over
    return !r.surface.IsNull();
}

// Free-form patches that fold over their mean plane (or fit badly) are halved across their longest
// direction until each half fits; tiny or hopeless pieces stay as flat facets
void Fitter::addFreeform(const std::vector<int> &tris, int depth) {
    Region r;
    const int foldsBefore = ffFold;
    if (tris.size() >= 6 && fitFreeform(tris, r)) { addRegion(tris, r); return; }
    bool folded = ffFold > foldsBefore;
    // Face groups must stay one face each (Fusion Convert). Do not subdivide a group.
    if (grouped) {
        addFlat(tris);
        return;
    }
    if (tris.size() < (folded ? 6 : 12) || depth >= 12) {
        smallPieces.push_back(tris);   // absorbSmall: onto a neighbouring free-form face, else facets
        return;
    }
    gp_XYZ c(0, 0, 0), n(0, 0, 0);
    std::vector<gp_XYZ> centre(tris.size());
    for (size_t i = 0; i < tris.size(); ++i) {
        const auto &tr = m.tris[tris[i]];
        centre[i] = (m.nodes[tr[0]].XYZ() + m.nodes[tr[1]].XYZ() + m.nodes[tr[2]].XYZ()) / 3;
        c += centre[i];
        n += nrm[tris[i]] * area[tris[i]];
    }
    c /= double(tris.size());
    std::vector<char> side(tris.size());
    double A[3][3] = {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}}, w[3], V[3][3];
    for (const gp_XYZ &p : centre) {
        const gp_XYZ d = p - c;
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j) A[i][j] += d.Coord(i + 1) * d.Coord(j + 1);
    }
    eigenSym(A, w, V);
    int pick = int(std::max_element(w, w + 3) - w);
    if (folded && n.Modulus() > 1e-12) {
        // The patch bends too far for one height field: cut it across the bend - along the principal axis the
        // normals turn most along - so each half spans half the turn, instead of lengthwise down a round-over
        n /= n.Modulus();
        double N[3][3] = {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}};
        for (int t : tris) {
            const gp_XYZ d = nrm[t] - n;
            for (int i = 0; i < 3; ++i)
                for (int j = 0; j < 3; ++j) N[i][j] += area[t] * d.Coord(i + 1) * d.Coord(j + 1);
        }
        double wn[3], Vn[3][3];
        eigenSym(N, wn, Vn);
        const int bn = int(std::max_element(wn, wn + 3) - wn);
        const gp_XYZ turn(Vn[0][bn], Vn[1][bn], Vn[2][bn]);
        double best = -1;
        for (int k = 0; k < 3; ++k) {
            if (w[k] <= w[pick] * 1e-4) continue;   // not along a flat direction of the patch
            const gp_XYZ e(V[0][k], V[1][k], V[2][k]);
            double sxy = 0, sxx = 0, syy = 0;
            for (size_t i = 0; i < tris.size(); ++i) {
                const double x = (centre[i] - c).Dot(e), y = (nrm[tris[i]] - n).Dot(turn);
                sxy += x * y; sxx += x * x; syy += y * y;
            }
            const double corr = sxx > 0 && syy > 0 ? std::fabs(sxy) / std::sqrt(sxx * syy) : 0;
            if (corr > best) { best = corr; pick = k; }
        }
    }
    const gp_XYZ axis(V[0][pick], V[1][pick], V[2][pick]);
    for (size_t i = 0; i < tris.size(); ++i) side[i] = (centre[i] - c).Dot(axis) > 0;
    // Slivers the cut leaves on one side go to the other, so the halves stay whole
    std::unordered_map<int, size_t> at;
    for (size_t i = 0; i < tris.size(); ++i) at.emplace(tris[i], i);
    for (int s = 0; s < 2; ++s) {
        std::vector<int> hf;
        for (size_t i = 0; i < tris.size(); ++i)
            if (side[i] == s) hf.push_back(tris[i]);
        const auto comps = components(hf);
        size_t biggest = 0;
        for (const auto &comp : comps) biggest = std::max(biggest, comp.size());
        for (const auto &comp : comps)
            if (comp.size() < biggest && comp.size() < std::max<size_t>(12, tris.size() / 20))
                for (int t : comp) side[at[t]] = char(1 - s);
    }
    std::vector<int> half[2];
    for (size_t i = 0; i < tris.size(); ++i) half[int(side[i])].push_back(tris[i]);
    for (const auto &hf : half)
        for (const auto &comp : components(hf)) addFreeform(comp, depth + 1);
}

// Fragments left from splitting free-form patches join a free-form neighbour whose surface they lie on
void Fitter::absorbSmall() {
    for (const auto &piece : smallPieces) {
        std::unordered_set<int> around;
        for (int t : piece)
            for (int u : nb[t])
                if (u >= 0 && label[u] >= 0 && regions[label[u]].kind == Region::Freeform && !regions[label[u]].dead)
                    around.insert(label[u]);
        int into = -1;
        const std::vector<int> nodes = nodesOf(piece);
        for (int id : around) {
            const Region &r = regions[id];
            const gp_XYZ c = r.ax.Location().XYZ(), X = r.ax.XDirection().XYZ(), Y = r.ax.YDirection().XYZ(),
                         Z = r.ax.Direction().XYZ();
            bool on = true;
            for (int t : piece) on = on && nrm[t].Dot(Z) >= 0.2;
            for (size_t i = 0; on && i < nodes.size(); ++i) {
                const gp_XYZ d = m.nodes[nodes[i]].XYZ() - c;
                const double u = d.Dot(X), v = d.Dot(Y);
                on = u >= r.dom[0] && u <= r.dom[1] && v >= r.dom[2] && v <= r.dom[3]
                     && std::fabs(r.height->Value(u, v).Z() - d.Dot(Z)) <= r.tol * 2;
            }
            if (on) { into = id; break; }
        }
        if (into >= 0) {
            for (int t : piece) label[t] = into;
            continue;
        }
        ffFacetTris += int(piece.size());
        for (const auto &p : coplanarPieces(piece)) addFlat(p);
    }
    smallPieces.clear();
}

// The outline of the triangle set passes through some node twice (the set touches itself at a corner or
// encloses a hole at one), which a face cannot have
bool Fitter::pinched(const std::vector<int> &tris) {
    const int mark = ++curStamp;
    for (int t : tris) stamp[t] = mark;
    std::unordered_map<int, int> out;
    for (int t : tris)
        for (int k = 0; k < 3; ++k) {
            const int u = nb[t][k];
            if (u >= 0 && stamp[u] == mark) continue;
            if (++out[m.tris[t][k]] > 1) return true;
        }
    return false;
}

// The piece lies on the region's surface (within twice its fit tolerance) and faces the same way
bool Fitter::liesOn(const Region &r, const std::vector<int> &piece) {
    if (r.dead || r.kind == Region::Plane) return false;
    const std::vector<int> nodes = nodesOf(piece);
    if (r.kind == Region::Freeform) {
        const gp_XYZ c = r.ax.Location().XYZ(), X = r.ax.XDirection().XYZ(), Y = r.ax.YDirection().XYZ(),
                     Z = r.ax.Direction().XYZ();
        for (int t : piece)
            if (area[t] > 0 && nrm[t].Dot(Z) < 0.2) return false;
        auto on = [&](const gp_XYZ &p) {
            const gp_XYZ d = p - c;
            const double u = d.Dot(X), w = d.Dot(Y);
            if (u < r.dom[0] || u > r.dom[1] || w < r.dom[2] || w > r.dom[3]) return false;
            return std::fabs(r.height->Value(u, w).Z() - d.Dot(Z)) <= r.tol * 2;
        };
        for (int v : nodes)
            if (!on(m.nodes[v].XYZ())) return false;
        // Long thin triangles: the surface must follow their edges, not just their corners
        for (int t : piece) {
            const auto &tr = m.tris[t];
            const gp_XYZ p[3] = {m.nodes[tr[0]].XYZ(), m.nodes[tr[1]].XYZ(), m.nodes[tr[2]].XYZ()};
            if (!on((p[0] + p[1] + p[2]) / 3)) return false;
            for (int k = 0; k < 3; ++k)
                for (double s : {0.25, 0.5, 0.75})
                    if (!on(p[k] + (p[(k + 1) % 3] - p[k]) * s)) return false;
        }
        return true;
    }
    const double tol = std::max(r.tol, tolSurf(r.radius)) * 2;
    for (int t : piece)
        if (!normalAgrees(r, t)) return false;
    for (int v : nodes)
        if (surfaceDistance(r, m.nodes[v].XYZ()) > tol) return false;
    return true;
}

// Leftover facets: one- and two-triangle planes, and small planes with a smooth rim, are what free-form areas
// break into where no fit succeeded. They join the fitted surfaces around them - a free-form neighbour is
// refitted to run over them - and groups with no such neighbour get a free-form face of their own.
void Fitter::mopUp() {
    const int nt = int(label.size());
    const double cosSmoothRim = std::cos(10 * kPi / 180);
    for (int round = 0; round < 6; ++round) {
        std::vector<int> count(regions.size(), 0);
        std::vector<double> rimAll(regions.size(), 0), rimSmooth(regions.size(), 0);
        for (int t = 0; t < nt; ++t) {
            const int a = label[t];
            if (a < 0) continue;
            ++count[a];
            for (int k = 0; k < 3; ++k) {
                const int u = nb[t][k];
                if (u < 0 || label[u] == a) continue;
                const double len = m.nodes[m.tris[t][k]].Distance(m.nodes[m.tris[t][(k + 1) % 3]]);
                rimAll[a] += len;
                if (nrm[t].Dot(nrm[u]) > cosSmoothRim) rimSmooth[a] += len;
            }
        }
        std::vector<char> tiny(regions.size(), 0);
        for (size_t i = 0; i < regions.size(); ++i) {
            if (count[i] == 0) regions[i].dead = true;
            const Region &r = regions[i];
            if (r.dead || r.kind != Region::Plane || count[i] > 20) continue;
            tiny[i] = rimSmooth[i] > (count[i] <= 2 ? 0.3 : 0.7) * rimAll[i];
        }
        std::vector<int> loose;
        for (int t = 0; t < nt; ++t)
            if (label[t] >= 0 && tiny[label[t]]) loose.push_back(t);
        if (loose.empty()) break;

        std::vector<std::vector<int>> regionTris(regions.size());
        for (int t = 0; t < nt; ++t)
            if (label[t] >= 0) regionTris[label[t]].push_back(t);
        std::unordered_map<int, std::vector<int>> pending;   // free-form region -> triangles that want to join it
        bool changed = false;
        for (const auto &comp : components(loose)) {
            std::unordered_map<int, double> border;   // fitted neighbours by shared outline length
            for (int t : comp)
                for (int k = 0; k < 3; ++k) {
                    const int u = nb[t][k];
                    if (u < 0 || label[u] < 0 || tiny[label[u]]) continue;
                    const Region &r = regions[label[u]];
                    if (r.kind == Region::Plane || r.dead) continue;
                    border[label[u]] += m.nodes[m.tris[t][k]].Distance(m.nodes[m.tris[t][(k + 1) % 3]]);
                }
            if (border.empty()) continue;   // own face below
            std::vector<std::pair<double, int>> order;
            for (const auto &kv : border) order.push_back({-kv.second, kv.first});
            std::sort(order.begin(), order.end());
            int into = -1;
            for (const auto &o : order) {
                Region r = regions[o.second];
                if (!liesOn(r, comp)) continue;
                std::vector<int> all = regionTris[o.second];
                all.insert(all.end(), comp.begin(), comp.end());
                if (pinched(all)) continue;
                // Round faces are bounded at fit time: their band must still be a clean one
                if (r.kind != Region::Freeform && r.kind != Region::Extrusion && !uvBounds(all, r)) continue;
                regions[o.second] = r;
                regionTris[o.second] = std::move(all);
                into = o.second;
                break;
            }
            if (into >= 0) {
                for (int t : comp) label[t] = into;
                mopMerged += int(comp.size());
                changed = true;
                continue;
            }
            // Each triangle goes to the free-form neighbour it faces along with, which is refitted over it;
            // only connected pieces touching that neighbour, so no region grows an island
            std::unordered_map<int, std::vector<int>> wants;
            auto faces = [&](int t, const Region &r) { return r.kind == Region::Freeform && nrm[t].Dot(r.ax.Direction().XYZ()) >= 0.3; };
            int whole = -1;   // the whole group to one neighbour when it can, so strips are not combed apart
            for (const auto &o : order) {
                bool all = true;
                for (int t : comp) all = all && faces(t, regions[o.second]);
                if (all) { whole = o.second; break; }
            }
            if (whole >= 0) wants[whole] = comp;
            else
                for (int t : comp)
                    for (const auto &o : order)
                        if (faces(t, regions[o.second])) { wants[o.second].push_back(t); break; }
            for (auto &kv : wants)
                for (const auto &piece : components(kv.second)) {
                    bool touches = false;
                    for (int t : piece)
                        for (int u : nb[t]) touches = touches || (u >= 0 && label[u] == kv.first);
                    if (touches) pending[kv.first].insert(pending[kv.first].end(), piece.begin(), piece.end());
                }
        }
        std::unordered_set<int> touched;   // regions that lost triangles to a refit
        for (auto &kv : pending) {
            const int id = kv.first;
            if (regionTris[id].size() > 40000) continue;
            auto join = [&](const std::vector<int> &extra) {
                std::vector<int> all = regionTris[id];
                all.insert(all.end(), extra.begin(), extra.end());
                if (pinched(all)) return false;
                Region r;
                ++mopRefits;
                if (!fitFreeform(all, r)) return false;
                regions[id] = r;
                maxResidual = std::max(maxResidual, r.residual);
                for (int t : extra) { touched.insert(label[t]); label[t] = id; }
                regionTris[id] = std::move(all);
                mopMerged += int(extra.size());
                changed = true;
                return true;
            };
            if (join(kv.second)) continue;
            const auto comps = components(kv.second);
            if (comps.size() > 1 && comps.size() <= 12)
                for (const auto &c : comps) join(c);
        }
        // A region must stay one piece: what a partial merge cut off from a small plane becomes facets again
        for (int id : touched) {
            std::vector<int> rest;
            for (int t = 0; t < nt; ++t)
                if (label[t] == id) rest.push_back(t);
            if (rest.size() < 2) continue;
            auto comps = components(rest);
            if (comps.size() < 2) continue;
            std::sort(comps.begin(), comps.end(), [](const auto &a, const auto &b) { return a.size() > b.size(); });
            for (size_t k = 1; k < comps.size(); ++k) {
                for (int t : comps[k]) label[t] = -1;
                for (const auto &p : coplanarPieces(comps[k])) addFlat(p);
            }
        }
        // What no neighbour would take gets free-form faces of its own (split like any free-form patch)
        std::vector<int> still;
        for (int t : loose)
            if (label[t] >= 0 && label[t] < int(tiny.size()) && tiny[label[t]]) still.push_back(t);
        for (const auto &comp : components(still)) {
            if (comp.size() < 3) continue;
            Region r;
            if (!pinched(comp) && fitFreeform(comp, r)) { addRegion(comp, r); ++mopOwn; changed = true; }
            else if (comp.size() >= 12) { addFreeform(comp, 6); ++mopOwn; changed = true; }
        }
        absorbSmall();
        if (!changed) break;
    }
    std::vector<int> count(regions.size(), 0);
    for (int t = 0; t < nt; ++t)
        if (label[t] >= 0) ++count[label[t]];
    for (size_t i = 0; i < regions.size(); ++i)
        if (count[i] == 0) regions[i].dead = true;
    if (qEnvironmentVariableIsSet("FIT_DEBUG")) {
        // What is left: tiny planes by what surrounds them
        std::vector<char> tiny(regions.size(), 0);
        for (size_t i = 0; i < regions.size(); ++i)
            tiny[i] = !regions[i].dead && regions[i].kind == Region::Plane && count[i] <= 2;
        std::vector<int> loose;
        for (int t = 0; t < nt; ++t)
            if (label[t] >= 0 && tiny[label[t]]) loose.push_back(t);
        int onlyPlanes = 0, nearFree = 0, nearRound = 0, comps = 0, big = 0;
        for (const auto &comp : components(loose)) {
            ++comps;
            if (comp.size() >= 12) ++big;
            bool fr = false, rd = false;
            for (int t : comp)
                for (int u : nb[t]) {
                    if (u < 0 || label[u] < 0 || tiny[label[u]]) continue;
                    const Region &r = regions[label[u]];
                    if (r.kind == Region::Freeform) fr = true;
                    else if (r.kind != Region::Plane) rd = true;
                }
            if (fr) nearFree += int(comp.size());
            else if (rd) nearRound += int(comp.size());
            else onlyPlanes += int(comp.size());
        }
        std::fprintf(stderr, "  [fit] mop-up left %zu facet triangles in %d groups (%d of 12+): %d beside free-form, %d beside round, %d among planes\n",
                     loose.size(), comps, big, nearFree, nearRound, onlyPlanes);
    }
}

double Fitter::surfaceDistance(const Region &r, const gp_XYZ &p) const {
    const gp_XYZ d = p - r.ax.Location().XYZ(), Z = r.ax.Direction().XYZ();
    switch (r.kind) {
        case Region::Plane: return std::fabs(d.Dot(Z));
        case Region::Sphere: return std::fabs(d.Modulus() - r.radius);
        case Region::Cylinder: return std::fabs((d - Z * d.Dot(Z)).Modulus() - r.radius);
        case Region::Cone: {
            const double h = d.Dot(Z), rho = (d - Z * h).Modulus();
            return std::fabs(rho - (r.radius + h * std::tan(r.angle))) * std::cos(r.angle);
        }
        case Region::Extrusion: {
            GeomAPI_ProjectPointOnCurve proj(gp_Pnt(p - Z * p.Dot(Z)), r.profile);
            return proj.NbPoints() ? proj.LowerDistance() : 1e300;
        }
        case Region::Freeform: {
            GeomAPI_ProjectPointOnSurf proj(gp_Pnt(p), r.surface);
            return proj.NbPoints() ? proj.LowerDistance() : 1e300;
        }
    }
    return 1e300;
}

// The triangle faces the way the surface does at its centre (within 25 degrees)
bool Fitter::normalAgrees(const Region &r, int t) const {
    if (area[t] <= 0 || r.kind == Region::Extrusion || r.kind == Region::Freeform) return true;
    const auto &tr = m.tris[t];
    const gp_XYZ p = (m.nodes[tr[0]].XYZ() + m.nodes[tr[1]].XYZ() + m.nodes[tr[2]].XYZ()) / 3;
    const gp_XYZ d = p - r.ax.Location().XYZ(), Z = r.ax.Direction().XYZ();
    gp_XYZ n;
    if (r.kind == Region::Plane) n = Z;
    else if (r.kind == Region::Sphere) n = d;
    else {
        gp_XYZ radial = d - Z * d.Dot(Z);
        if (radial.Modulus() < 1e-12) return true;
        radial /= radial.Modulus();
        n = r.kind == Region::Cone ? radial * std::cos(r.angle) - Z * std::sin(r.angle) : radial;
    }
    if (n.Modulus() < 1e-12) return true;
    return std::fabs(n.Dot(nrm[t])) / n.Modulus() > std::cos(25 * kPi / 180);
}

// Region growing for a smooth patch that no single surface fits (fillets running into faces, blends between
// holes): small seed neighbourhoods are fitted with a plane or a curved surface, which is then grown over
// the neighbours lying on it. What never fits stays as merged flat facets.
void Fitter::segment(const std::vector<int> &tris) {
    const int avail = ++curStamp;
    for (int t : tris) stamp[t] = avail;
    std::vector<int> order = tris;
    std::sort(order.begin(), order.end(), [&](int a, int b) { return area[a] < area[b]; });

    auto seedSet = [&](int seed, size_t want) {
        std::vector<int> s{seed};
        const int seen = ++curStamp;
        std::vector<int> restore{seed};
        stamp[seed] = seen;
        for (size_t i = 0; i < s.size() && s.size() < want; ++i)
            for (int u : nb[s[i]])
                if (u >= 0 && stamp[u] == avail && s.size() < want) { stamp[u] = seen; s.push_back(u); restore.push_back(u); }
        for (int t : restore) stamp[t] = avail;
        return s;
    };
    auto onSurface = [&](const Region &r, int t) {
        const double tol = r.kind == Region::Plane ? tolPlane : tolSurf(r.radius) * 2;
        for (int v : m.tris[t])
            if (surfaceDistance(r, m.nodes[v].XYZ()) > tol) return false;
        return normalAgrees(r, t);
    };

    std::unordered_set<int> tried;   // triangles of a grown region that failed: not seeds again
    for (int seed : order) {
        if (stamp[seed] != avail || tried.count(seed)) continue;
        Region r;
        bool found = false;
        // Accurate face groups: seed a single triangle as a plane, then grow with Boundary Tolerance.
        // Large adjacency seeds mixed neighbouring faces into the fit and under-merged CAD flats.
        // Fillet chords become thin plane bands (Fusion Accurate look); round leftovers still try cylinders.
        if (groupsMode && fitPlane({seed}, r)) {
            found = true;
        } else {
            for (size_t want : {24u, 12u, 6u}) {
                const std::vector<int> s = seedSet(seed, want);
                if (s.size() < 2) break;
                if (fitPlane(s, r)) { found = true; break; }
                if (s.size() < 4) continue;
                gp_XYZ ax;
                if (fitSurface(s, r, nullptr) || (crossAxis(s, ax) && fitSurface(s, r, &ax))) {
                    found = true;
                    break;
                }
            }
        }
        if (!found) continue;
        // Grow, refitting each time the region doubles
        const int mine = ++curStamp;
        std::vector<int> region{seed};
        stamp[seed] = mine;
        size_t nextFit = 16;
        for (size_t i = 0; i < region.size(); ++i) {
            for (int u : nb[region[i]])
                if (u >= 0 && stamp[u] == avail && onSurface(r, u)) { stamp[u] = mine; region.push_back(u); }
            if (region.size() >= nextFit && r.kind != Region::Plane) {
                Region refit;
                const gp_XYZ ax = r.ax.Direction().XYZ();
                if (fitSurface(region, refit, r.kind == Region::Sphere ? nullptr : &ax) && refit.kind == r.kind) r = refit;
                nextFit = region.size() * 2;
            }
        }
        Region final = r;
        bool ok;
        if (r.kind == Region::Plane) {
            ok = region.size() >= 2 && fitPlane(region, final);
        } else {
            const gp_XYZ ax = r.ax.Direction().XYZ();
            ok = region.size() >= 4 && fitSurface(region, final, r.kind == Region::Sphere ? nullptr : &ax)
                 && final.kind == r.kind && uvBounds(region, final);
        }
        if (ok) {
            addRegion(region, final);
        } else {
            for (int t : region) { stamp[t] = avail; tried.insert(t); }   // still free for other regions
        }
    }
    // Leftovers: merged coplanar facets
    std::vector<int> rest;
    for (int t : tris)
        if (label[t] < 0) rest.push_back(t);
    for (const auto &p : coplanarPieces(rest)) addFlat(p);
}

// Cut a patch in half across the direction the normals turn most (Fusion-like bands on fillets), falling
// back to the longest geometric axis when the patch is flat.
bool Fitter::splitAcrossBend(const std::vector<int> &tris, std::vector<std::vector<int>> &parts) {
    if (tris.size() < 4) return false;
    gp_XYZ c(0, 0, 0), n(0, 0, 0);
    std::vector<gp_XYZ> centre(tris.size());
    for (size_t i = 0; i < tris.size(); ++i) {
        const auto &tr = m.tris[tris[i]];
        centre[i] = (m.nodes[tr[0]].XYZ() + m.nodes[tr[1]].XYZ() + m.nodes[tr[2]].XYZ()) / 3;
        c += centre[i];
        n += nrm[tris[i]] * area[tris[i]];
    }
    c /= double(tris.size());
    double A[3][3] = {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}}, w[3], V[3][3];
    for (const gp_XYZ &p : centre) {
        const gp_XYZ d = p - c;
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j) A[i][j] += d.Coord(i + 1) * d.Coord(j + 1);
    }
    eigenSym(A, w, V);
    int pick = int(std::max_element(w, w + 3) - w);
    if (n.Modulus() > 1e-12) {
        n /= n.Modulus();
        double N[3][3] = {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}};
        for (int t : tris) {
            const gp_XYZ d = nrm[t] - n;
            for (int i = 0; i < 3; ++i)
                for (int j = 0; j < 3; ++j) N[i][j] += area[t] * d.Coord(i + 1) * d.Coord(j + 1);
        }
        double wn[3], Vn[3][3];
        eigenSym(N, wn, Vn);
        const int bn = int(std::max_element(wn, wn + 3) - wn);
        const gp_XYZ turn(Vn[0][bn], Vn[1][bn], Vn[2][bn]);
        double best = -1;
        for (int k = 0; k < 3; ++k) {
            if (w[k] <= w[pick] * 1e-4) continue;
            const gp_XYZ e(V[0][k], V[1][k], V[2][k]);
            double sxy = 0, sxx = 0, syy = 0;
            for (size_t i = 0; i < tris.size(); ++i) {
                const double x = (centre[i] - c).Dot(e), y = (nrm[tris[i]] - n).Dot(turn);
                sxy += x * y; sxx += x * x; syy += y * y;
            }
            const double corr = sxx > 0 && syy > 0 ? std::fabs(sxy) / std::sqrt(sxx * syy) : 0;
            if (corr > best) { best = corr; pick = k; }
        }
    }
    const gp_XYZ axis(V[0][pick], V[1][pick], V[2][pick]);
    std::vector<char> side(tris.size());
    for (size_t i = 0; i < tris.size(); ++i) side[i] = (centre[i] - c).Dot(axis) > 0;
    std::unordered_map<int, size_t> at;
    for (size_t i = 0; i < tris.size(); ++i) at.emplace(tris[i], i);
    for (int s = 0; s < 2; ++s) {
        std::vector<int> hf;
        for (size_t i = 0; i < tris.size(); ++i)
            if (side[i] == s) hf.push_back(tris[i]);
        const auto comps = components(hf);
        size_t biggest = 0;
        for (const auto &comp : comps) biggest = std::max(biggest, comp.size());
        for (const auto &comp : comps)
            if (comp.size() < biggest && comp.size() < std::max<size_t>(8, tris.size() / 20))
                for (int t : comp) side[at[t]] = char(1 - s);
    }
    parts.clear();
    for (int s = 0; s < 2; ++s) {
        std::vector<int> hf;
        for (size_t i = 0; i < tris.size(); ++i)
            if (side[i] == s) hf.push_back(tris[i]);
        for (auto &comp : components(hf))
            if (!comp.empty()) parts.push_back(std::move(comp));
    }
    return parts.size() >= 2;
}

void Fitter::classify(const std::vector<int> &tris, int depth) {
    Region r;
    if (fitPlane(tris, r)) { addRegion(tris, r); return; }
    if (tris.size() >= 4 && fitCurved(tris, r)) { addRegion(tris, r); return; }
    // Accurate face groups: Fusion grows planes / cylinders / cones / spheres greedily (no free-form).
    // A smooth patch is segmented into primitives; leftovers become coplanar strips (fillet bands).
    if (groupsMode) {
        if (tris.size() >= 4) {
            segment(tris);
            return;
        }
        for (const auto &p : coplanarPieces(tris)) addFlat(p);
        return;
    }
    // With face groups (Fusion Convert): one group → one face. Do not segment / split the group.
    if (grouped && depth == 0) {
        if (tris.size() >= 6 && (fitExtrusion(tris, r) || fitFreeform(tris, r))) {
            addRegion(tris, r);
            return;
        }
        if (tris.size() >= 6) {
            addFreeform(tris, 0);   // grouped path keeps the whole group as one plane if free-form fails
            return;
        }
        addFlat(tris);
        return;
    }
    if (tris.size() >= 4) {
        // Planes and round surfaces first; a free-form extrusion replaces them only when they leave facets
        const size_t before = regions.size();
        segment(tris);
        std::unordered_map<int, int> count;
        std::unordered_map<int, double> regionArea;
        double patchArea = 0;
        for (int t : tris) { ++count[label[t]]; regionArea[label[t]] += area[t]; patchArea += area[t]; }
        // Single facets, tiny planes whose border is mostly smooth (two triangles of a quad on a curved
        // surface), and round strips side by side (a spline approximated by arcs), which cannot share their
        // corners either
        std::unordered_map<int, double> rimAll, rimSmooth;
        const double cosSmoothRim = std::cos(10 * kPi / 180);
        for (int t : tris)
            for (int k = 0; k < 3; ++k) {
                const int u = nb[t][k], a = label[t];
                if (u < 0 || a == label[u] || a < int(before)) continue;
                const double len = m.nodes[m.tris[t][k]].Distance(m.nodes[m.tris[t][(k + 1) % 3]]);
                rimAll[a] += len;
                if (nrm[t].Dot(nrm[u]) > cosSmoothRim) rimSmooth[a] += len;
            }
        std::unordered_set<int> bad;
        for (const auto &kv : count) {
            if (kv.first < int(before) || regions[kv.first].kind != Region::Plane) continue;
            const bool smoothRim = rimSmooth[kv.first] > 0.5 * rimAll[kv.first];
            if (kv.second == 1 || (smoothRim && (kv.second <= 6 || regionArea[kv.first] < patchArea * 0.02)))
                bad.insert(kv.first);
        }
        for (int t : tris)
            for (int u : nb[t]) {
                const int a = label[t], b = label[u];
                if (a != b && a >= int(before) && b >= int(before) && regions[a].kind != Region::Plane
                    && regions[b].kind != Region::Plane) { bad.insert(a); bad.insert(b); }
            }
        if (bad.empty() || tris.size() < 6) return;
        double badArea = 0;
        for (int id : bad) badArea += regionArea[id];
        std::vector<std::vector<int>> parts;
        if (fitExtrusion(tris, r)) {
            for (size_t k = before; k < regions.size(); ++k) regions[k].dead = true;
            addRegion(tris, r);
        } else if (depth < 4 && splitAtRimCorners(tris, parts)) {
            for (size_t k = before; k < regions.size(); ++k) regions[k].dead = true;
            for (int t : tris) label[t] = -1;
            for (const auto &p : parts) classify(p, depth + 1);
        } else if (prismatic && badArea > patchArea * 0.15) {
            // Mostly free-form: the bad pieces and the small fits among them become B-spline faces; the
            // large planes and round faces stay
            for (const auto &kv : regionArea)
                if (kv.first >= int(before) && kv.second < patchArea * 0.02) bad.insert(kv.first);
            std::vector<int> rest;
            for (int t : tris)
                if (bad.count(label[t])) { rest.push_back(t); label[t] = -1; }
            for (int id : bad) regions[id].dead = true;
            for (const auto &comp : components(rest)) addFreeform(comp, 0);
        }
        return;
    }
    // Pull out the large flat pieces (faces a fillet runs into), then fit what is left in between
    double total = 0;
    for (int t : tris) total += area[t];
    const auto pieces = coplanarPieces(tris);
    std::vector<const std::vector<int> *> big;
    if (depth < 3)
        for (const auto &p : pieces) {
            double a = 0;
            for (int t : p) a += area[t];
            if (a >= total / 8 && p.size() >= 2) big.push_back(&p);
        }
    if (big.empty() || big.size() == pieces.size()) {
        for (const auto &p : pieces) addFlat(p);
        return;
    }
    for (const auto *p : big) addFlat(*p);
    // Connected leftovers
    const int mark = ++curStamp;
    for (int t : tris)
        if (label[t] < 0) stamp[t] = mark;
    for (int t : tris) {
        if (stamp[t] != mark) continue;
        std::vector<int> comp{t};
        stamp[t] = 0;
        for (size_t i = 0; i < comp.size(); ++i)
            for (int u : nb[comp[i]])
                if (u >= 0 && stamp[u] == mark) { stamp[u] = 0; comp.push_back(u); }
        classify(comp, depth + 1);
    }
}

bool Fitter::prepare(QString &error) {
    if (m.tris.empty()) { error = "The mesh is empty"; return false; }
    double lo[3] = {1e300, 1e300, 1e300}, hi[3] = {-1e300, -1e300, -1e300};
    for (const gp_Pnt &p : m.nodes)
        for (int k = 0; k < 3; ++k) { lo[k] = std::min(lo[k], p.Coord(k + 1)); hi[k] = std::max(hi[k], p.Coord(k + 1)); }
    diag = std::max(1e-9, std::sqrt((hi[0] - lo[0]) * (hi[0] - lo[0]) + (hi[1] - lo[1]) * (hi[1] - lo[1]) +
                                    (hi[2] - lo[2]) * (hi[2] - lo[2])));
    stage("start");
    tick(0, "Welding the mesh", true);
    weld(m, diag * 1e-7);
    tolPlane = fixedTol > 0 ? fixedTol : diag * 1e-5;
    // Accurate face groups: Fusion's Boundary Tolerance is a proportion of the mesh diagonal (API 0..0.01,
    // default 0.001) — not millimetres. Values above 0.01 are treated as legacy absolute lengths from
    // earlier Fission builds that stored "0.001 mm".
    if (groupsMode) {
        if (fixedTol <= 0)
            tolPlane = 0.001 * diag;
        else if (fixedTol <= 0.01)
            tolPlane = fixedTol * diag;
        else
            tolPlane = fixedTol;
        groupsBandTol = tolPlane;
        if (qEnvironmentVariableIsSet("FIT_DEBUG"))
            std::fprintf(stderr, "  [fit] face-group proportion -> band tol %.4g mm (diag %.4g)\n", groupsBandTol, diag);
    }
    if (!buildTopology(error)) return false;
    stage("weld + topology");
    const int nt = int(m.tris.size());
    label.assign(nt, -1);
    stamp.assign(nt, 0);
    nodeStamp.assign(m.nodes.size(), 0);
    return true;
}

void Fitter::classifyAll() {
    const int nt = int(m.tris.size());
    if (prismatic) {
        grouped = m.hasGroups();
        if (qEnvironmentVariableIsSet("FIT_DEBUG"))
            std::fprintf(stderr, "  [fit] classify: %d tris, grouped=%d groupsMode=%d groupCount=%d\n", nt,
                         int(grouped), int(groupsMode),
                         grouped ? int(*std::max_element(m.groups.begin(), m.groups.end()) + 1) : 0);
        // Accurate face groups (Fusion): grow planes / cylinders / cones / spheres greedily with the
        // Boundary Tolerance (fraction of the diagonal). Plane-first flooding ate fillet strips; segment
        // tries a plane seed then a round seed so cylinders win on bends.
        if (groupsMode && !grouped) {
            tick(0.05, "Fitting surfaces", true);
            std::vector<int> all(nt);
            for (int t = 0; t < nt; ++t) all[t] = t;
            segment(all);
            absorbSmall();
            stage("classify");
            return;
        }
        // Smooth patches: the face groups when there are any, else neighbours bending less than the
        // sharp-edge angle
        const double cosSmooth = std::cos(35.0 * kPi / 180);
        std::vector<int> group(nt, -1);
        int done = 0;
        for (int s = 0; s < nt; ++s) {
            if (group[s] >= 0) continue;
            tick(0.03 + 0.55 * done / nt, "Fitting surfaces");
            std::vector<int> patch{s};
            group[s] = s;
            for (size_t i = 0; i < patch.size(); ++i)
                for (int u : nb[patch[i]]) {
                    if (group[u] >= 0) continue;
                    const bool same = grouped ? m.groups[u] == m.groups[patch[i]]
                                              : (area[u] <= 0 || area[patch[i]] <= 0 || nrm[u].Dot(nrm[patch[i]]) > cosSmooth);
                    if (!same) continue;
                    group[u] = s;
                    patch.push_back(u);
                }
            done += int(patch.size());
            classify(patch, 0);
        }
        absorbSmall();
        stage("classify");
        // When the mesh already has face groups, Convert keeps one face per group — mop-up would merge them.
        if (!grouped) {
            tick(0.6, "Joining leftover facets", true);
            if (!qEnvironmentVariableIsSet("FIT_NOMOP")) mopUp();
            stage("mop up");
        }
        return;
    } else {
        std::vector<int> all(nt);
        for (int t = 0; t < nt; ++t) all[t] = t;
        for (const auto &p : coplanarPieces(all)) addFlat(p);
    }
    stage("classify");
}

bool Fitter::segmentOnly(QString &error, std::vector<int> &labels, std::vector<char> &real) {
    // Accurate face groups: same primitive matching Fusion documents (planes / cylinders / cones /
    // spheres). Free-form and mop-up stay for Convert only.
    groupsMode = true;
    if (!prepare(error)) return false;
    classifyAll();
    if (!joinSlivers(error)) return false;
    real.assign(label.size(), 0);
    for (size_t t = 0; t < label.size(); ++t) {
        if (label[t] < 0 || regions[label[t]].dead) continue;
        // Keep every fitted primitive as its own group — including single-triangle planes on a fillet
        // strip. Flood-merging them by angle is what made our groups look blotchy next to Fusion's.
        real[t] = 1;
    }
    labels = label;
    return true;
}

TopoDS_Shape Fitter::run(QString &error, QString &summary) {
    if (!prepare(error)) return {};
    classifyAll();
    if (!joinSlivers(error)) return {};

    // Curved faces that do not meet their neighbours (two curved faces touching, a missed corner) cannot be
    // sewn on big models; they fall back to flat facets one round at a time, keeping every other fitted face
    TopoDS_Shape shape;
    int demoted = 0;
    for (int pass = 0; pass < 10; ++pass) {
        pBase = 0.66 + 0.03 * pass;
        pWhat = pass ? QString("Building faces (round %1)").arg(pass + 1) : QString("Building faces");
        tick(pBase, pWhat, true);
        shape = buildShape(error);
        if (!shape.IsNull() || !prismatic || badRegions.empty()) break;
        // Free-form patches are halved first (the rim that fails is usually local to one half); what still
        // fails in the later rounds becomes facets. With face groups, never split — one group stays one face.
        const bool split = !grouped && pass < 5;
        if (!split) demoted += int(badRegions.size());
        else
            for (int ri : badRegions)
                if (ri >= 0 && ri < int(regions.size()) && regions[ri].kind != Region::Freeform) ++demoted;
        demote(badRegions, split);
        if (!joinSlivers(error)) return {};
        stage("demote");
    }
    if (shape.IsNull()) return {};
    tick(1, "Done", true);
    if (qEnvironmentVariableIsSet("FIT_DEBUG"))
        std::fprintf(stderr, "  [fit] free-form: %d fitted, %d folded, %d off the nodes, %d triangles left as facets\n",
                     ffFits, ffFold, ffResidual, ffFacetTris);
    if (qEnvironmentVariableIsSet("FIT_DEBUG"))
        std::fprintf(stderr, "  [fit] mop-up: %d facet triangles joined fitted faces (%d refits), %d own free-form faces\n",
                     mopMerged, mopRefits, mopOwn);
    if (qEnvironmentVariableIsSet("FIT_DEBUG")) {
        std::unordered_map<int, int> sz;
        for (size_t t = 0; t < label.size(); ++t)
            if (label[t] >= 0 && !regions[label[t]].dead && regions[label[t]].kind == Region::Plane) ++sz[label[t]];
        int h[6] = {0, 0, 0, 0, 0, 0};
        for (const auto &kv : sz) ++h[kv.second == 1 ? 0 : kv.second == 2 ? 1 : kv.second <= 4 ? 2 : kv.second <= 10 ? 3 : kv.second <= 50 ? 4 : 5];
        std::fprintf(stderr, "  [fit] plane sizes: 1:%d 2:%d 3-4:%d 5-10:%d 11-50:%d >50:%d\n", h[0], h[1], h[2], h[3], h[4], h[5]);
    }
    int counts[6] = {0, 0, 0, 0, 0, 0}, live = 0;
    for (const Region &r : regions)
        if (!r.dead) { ++counts[r.kind]; ++live; }
    QStringList parts;
    const char *names[6] = {"plane", "cylinder", "cone", "sphere", "extruded free-form face", "free-form face"};
    for (int k = 0; k < 6; ++k)
        if (counts[k]) parts << QString("%1 %2%3").arg(counts[k]).arg(names[k]).arg(counts[k] > 1 ? "s" : "");
    summary = QString("%1 faces (%2)").arg(live).arg(parts.join(", "));
    if (demoted) summary += QString(", %1 curved patches left faceted").arg(demoted);
    return shape;
}

void Fitter::demote(const std::vector<int> &regionIds, bool split) {
    for (int ri : regionIds) {
        if (ri < 0 || ri >= int(regions.size()) || regions[ri].dead) continue;
        regions[ri].dead = true;
        std::vector<int> tris;
        for (int t = 0; t < int(label.size()); ++t)
            if (label[t] == ri) { tris.push_back(t); label[t] = -1; }
        if (split && regions[ri].kind == Region::Freeform && tris.size() >= 24) {
            // Two halves across the longest direction, each fitted again
            gp_XYZ c(0, 0, 0);
            std::vector<gp_XYZ> centre(tris.size());
            for (size_t i = 0; i < tris.size(); ++i) {
                const auto &tr = m.tris[tris[i]];
                centre[i] = (m.nodes[tr[0]].XYZ() + m.nodes[tr[1]].XYZ() + m.nodes[tr[2]].XYZ()) / 3;
                c += centre[i];
            }
            c /= double(tris.size());
            double A[3][3] = {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}}, w[3], V[3][3];
            for (const gp_XYZ &p : centre) {
                const gp_XYZ d = p - c;
                for (int i = 0; i < 3; ++i)
                    for (int j = 0; j < 3; ++j) A[i][j] += d.Coord(i + 1) * d.Coord(j + 1);
            }
            eigenSym(A, w, V);
            const int big = int(std::max_element(w, w + 3) - w);
            const gp_XYZ axis(V[0][big], V[1][big], V[2][big]);
            std::vector<int> half[2];
            for (size_t i = 0; i < tris.size(); ++i) half[(centre[i] - c).Dot(axis) > 0].push_back(tris[i]);
            if (!half[0].empty() && !half[1].empty()) {
                for (const auto &hf : half)
                    for (const auto &comp : components(hf)) addFreeform(comp, 6);
                absorbSmall();
                continue;
            }
        }
        // Face groups stay one face (Fusion Convert). Forced plane beats faceting a group into many faces.
        if (grouped) addFlat(tris);
        else
            for (const auto &p : coplanarPieces(tris)) addFlat(p);
    }
}

// Zero-area slivers join a neighbouring region
bool Fitter::joinSlivers(QString &error) {
    const int nt = int(label.size());
    for (bool changed = true; changed;) {
        changed = false;
        for (int t = 0; t < nt; ++t)
            if (label[t] < 0)
                for (int u : nb[t])
                    if (label[u] >= 0) { label[t] = label[u]; changed = true; break; }
    }
    for (int t = 0; t < nt; ++t)
        if (label[t] < 0) { error = "Some triangles could not be classified"; return false; }
    return true;
}

TopoDS_Shape Fitter::buildShape(QString &error) {
    const int nt = int(m.tris.size());
    BRep_Builder bb;
    const double tolVert = std::max(1e-7, diag * 1e-7);

    // Junction nodes: where the outline between two regions meets another region (or branches)
    std::vector<int> degree(m.nodes.size(), 0);
    std::vector<qint64> pairOf(m.nodes.size(), -1);
    std::vector<char> junction(m.nodes.size(), 0);
    const qint64 R = qint64(regions.size()) + 1;
    std::vector<std::vector<std::pair<int, int>>> halfEdges(regions.size());
    for (int t = 0; t < nt; ++t)
        for (int k = 0; k < 3; ++k) {
            const int o = nb[t][k];
            if (label[o] == label[t]) continue;
            const int a = m.tris[t][k], b = m.tris[t][(k + 1) % 3];
            halfEdges[label[t]].push_back({a, b});
            const qint64 pk = qint64(std::min(label[t], label[o])) * R + std::max(label[t], label[o]);
            for (int v : {a, b}) {
                ++degree[v];
                if (pairOf[v] < 0) pairOf[v] = pk;
                else if (pairOf[v] != pk) junction[v] = 1;
            }
        }
    for (size_t v = 0; v < degree.size(); ++v)
        if (degree[v] && degree[v] != 4) junction[v] = 1;   // each undirected edge counted from both sides

    std::unordered_map<int, TopoDS_Vertex> verts;
    auto vertex = [&](int n) {
        auto it = verts.find(n);
        if (it != verts.end()) return it->second;
        TopoDS_Vertex v;
        bb.MakeVertex(v, m.nodes[n], tolVert);
        verts.emplace(n, v);
        return v;
    };
    auto widen = [&](const TopoDS_Vertex &v, const gp_Pnt &onCurve) {
        const double d = BRep_Tool::Pnt(v).Distance(onCurve);
        if (d > BRep_Tool::Tolerance(v)) bb.UpdateVertex(v, d * 1.05);
    };

    // Curved faces first: they are plain u/v rectangles on their surfaces
    // Their outline edges are reused by the planar neighbours, and their corners become the vertices of the
    // matching mesh junction nodes, so the faces share topology without sewing.
    badRegions.clear();
    std::vector<std::vector<int>> regionTris(regions.size());
    for (int t = 0; t < nt; ++t) regionTris[label[t]].push_back(t);
    // The region's own triangles are the face's display mesh, so it never has to be meshed again
    auto attachMesh = [&](const TopoDS_Face &face, size_t ri) {
        const std::vector<int> &rt = regionTris[ri];
        std::unordered_map<int, int> local;
        for (int t : rt)
            for (int v : m.tris[t]) local.emplace(v, int(local.size()) + 1);
        Handle(Poly_Triangulation) tri = new Poly_Triangulation(int(local.size()), int(rt.size()), Standard_False);
        for (const auto &kv : local) tri->SetNode(kv.second, m.nodes[kv.first]);
        for (size_t i = 0; i < rt.size(); ++i) {
            const auto &tr = m.tris[rt[i]];
            // Faces whose surface runs against the triangles (reversed) list them the other way round
            if (face.Orientation() == TopAbs_REVERSED)
                tri->SetTriangle(int(i) + 1, Poly_Triangle(local[tr[0]], local[tr[2]], local[tr[1]]));
            else
                tri->SetTriangle(int(i) + 1, Poly_Triangle(local[tr[0]], local[tr[1]], local[tr[2]]));
        }
        tri->Deflection(0.0);
        bb.UpdateFace(face, tri);
    };

    std::vector<TopoDS_Face> faces;
    std::vector<int> faceRegion;
    // Vertex tolerances grown for free-form rims, applied once every edge exists (a grown vertex would
    // swallow a short edge built after it)
    std::vector<std::pair<TopoDS_Vertex, double>> vertexTol;
    std::vector<TopoDS_Edge> curvedCircles, curvedLoops, curvedEdges;
    bool anyCurved = false, cornerConflict = false;
    for (size_t ri = 0; ri < regions.size(); ++ri) {
        const Region &r = regions[ri];
        if (r.kind == Region::Plane || r.dead) continue;
        anyCurved = true;
        if (r.kind == Region::Freeform) continue;   // bounded by its outline, like a plane (below)
        Handle(Geom_Surface) s;
        if (r.kind == Region::Cylinder) s = new Geom_CylindricalSurface(r.ax, r.radius);
        else if (r.kind == Region::Cone) s = new Geom_ConicalSurface(r.ax, r.angle, r.radius);
        else if (r.kind == Region::Extrusion) s = new Geom_SurfaceOfLinearExtrusion(r.profile, r.ax.Direction());
        else s = new Geom_SphericalSurface(r.ax, r.radius);
        BRepBuilderAPI_MakeFace mk(s, r.u0, r.u1, r.v0, r.v1, Precision::Confusion());
        if (!mk.IsDone()) { error = "Could not build a fitted face"; badRegions.push_back(int(ri)); return {}; }
        attachMesh(mk.Face(), ri);
        faces.push_back(mk.Face());
        faceRegion.push_back(int(ri));
        for (TopExp_Explorer e(mk.Face(), TopAbs_EDGE); e.More(); e.Next()) {
            const TopoDS_Edge ed = TopoDS::Edge(e.Current());
            if (BRep_Tool::Degenerated(ed) || BRep_Tool::IsClosed(ed, mk.Face())) continue;
            if (TopExp::FirstVertex(ed).IsSame(TopExp::LastVertex(ed))) {
                if (BRepAdaptor_Curve(ed).GetType() == GeomAbs_Circle) curvedCircles.push_back(ed);
                else curvedLoops.push_back(ed);
            } else {
                curvedEdges.push_back(ed);
            }
        }
        const double tolMatch = std::max(tolSurf(r.radius) * 8, diag * 1e-5);
        TopTools_IndexedMapOfShape corners;
        TopExp::MapShapes(mk.Face(), TopAbs_VERTEX, corners);
        for (int i = 1; i <= corners.Extent(); ++i) {
            const TopoDS_Vertex cv = TopoDS::Vertex(corners(i));
            const gp_Pnt p = BRep_Tool::Pnt(cv);
            int best = -1;
            double bestD = tolMatch;
            for (const auto &he : halfEdges[ri])
                for (int n : {he.first, he.second})
                    if (junction[n] && m.nodes[n].Distance(p) < bestD) { bestD = m.nodes[n].Distance(p); best = n; }
            if (best < 0) continue;
            auto it = verts.find(best);
            if (it == verts.end()) verts.emplace(best, cv);
            else if (!it->second.IsSame(cv)) {
                cornerConflict = true;
                badRegions.push_back(int(ri));
                if (qEnvironmentVariableIsSet("FIT_DEBUG"))
                    std::fprintf(stderr, "  [fit] corner conflict region %d at (%.3f %.3f %.3f)\n", int(ri), p.X(), p.Y(), p.Z());
            }
        }
    }
    // An outline chain running along one edge of a curved face: that edge, from the chain's first node to its last
    auto curvedEdgeFor = [&](const std::vector<int> &seq) -> TopoDS_Edge {
        auto a = verts.find(seq.front()), b = verts.find(seq.back());
        if (a == verts.end() || b == verts.end() || a->second.IsSame(b->second)) return {};
        const gp_Pnt mid = m.nodes[seq[seq.size() / 2]];
        for (const TopoDS_Edge &e : curvedEdges) {
            const TopoDS_Vertex f = TopExp::FirstVertex(e, Standard_True), l = TopExp::LastVertex(e, Standard_True);
            const bool fwd = f.IsSame(a->second) && l.IsSame(b->second), rev = f.IsSame(b->second) && l.IsSame(a->second);
            if (!fwd && !rev) continue;
            if (seq.size() == 2 && BRepAdaptor_Curve(e).GetType() != GeomAbs_Line) {
                // A single straight segment only stands for a curve that stays near it
                double f0, f1;
                Handle(Geom_Curve) c = BRep_Tool::Curve(e, f0, f1);
                if (c.IsNull()) continue;
                const gp_Pnt chordMid((m.nodes[seq[0]].XYZ() + m.nodes[seq[1]].XYZ()) / 2);
                GeomAPI_ProjectPointOnCurve proj(chordMid, c, f0, f1);
                if (proj.NbPoints() == 0 || proj.LowerDistance() > 0.25 * m.nodes[seq[0]].Distance(m.nodes[seq[1]])) continue;
            }
            if (seq.size() > 2) {
                double f0, f1;
                Handle(Geom_Curve) c = BRep_Tool::Curve(e, f0, f1);
                if (c.IsNull()) continue;
                GeomAPI_ProjectPointOnCurve proj(mid, c, f0, f1);
                if (proj.NbPoints() == 0 || proj.LowerDistance() > diag * 1e-3) continue;
            }
            return fwd ? e : TopoDS::Edge(e.Reversed());
        }
        return {};
    };

    // Chains of outline nodes between junctions, fitted once and shared by both planar neighbours
    struct ChainEdges { std::vector<TopoDS_Edge> edges; };
    std::map<std::vector<int>, ChainEdges> chains;
    const double tolLine = diag * 1e-5;
    auto makeChain = [&](const std::vector<int> &seq, bool closed) -> ChainEdges {
        ChainEdges ce;
        const int n = int(seq.size()) - 1;   // segments
        if (!closed) {
            const TopoDS_Edge shared = curvedEdgeFor(seq);
            if (!shared.IsNull()) { ce.edges.push_back(shared); return ce; }
        } else {
            // A closed free-form rim of a curved face: every chain node lies on it
            const double tolOn = diag * 4e-4;
            for (const TopoDS_Edge &e : curvedLoops) {
                double f0, f1;
                Handle(Geom_Curve) c = BRep_Tool::Curve(e, f0, f1);
                if (c.IsNull()) continue;
                GeomAPI_ProjectPointOnCurve proj;
                bool on = true;
                const int step = std::max(1, n / 16);
                for (int i = 0; on && i < n; i += step) {
                    proj.Init(m.nodes[seq[i]], c, f0, f1);
                    on = proj.NbPoints() > 0 && proj.LowerDistance() <= tolOn;
                }
                if (!on) continue;
                // Same sense as the chain?
                proj.Init(m.nodes[seq[0]], c, f0, f1);
                gp_Pnt p;
                gp_Vec tan;
                c->D1(proj.LowerDistanceParameter(), p, tan);
                if (e.Orientation() == TopAbs_REVERSED) tan.Reverse();
                const bool same = tan.Dot(gp_Vec(m.nodes[seq[0]], m.nodes[seq[1]])) > 0;
                ce.edges.push_back(same ? e : TopoDS::Edge(e.Reversed()));
                return ce;
            }
        }
        // Straight?
        if (!closed) {
            const gp_XYZ a = m.nodes[seq.front()].XYZ(), b = m.nodes[seq.back()].XYZ();
            const gp_XYZ ab = b - a;
            const double len = ab.Modulus();
            bool straight = len > 0;
            for (int i = 1; straight && i < n; ++i)
                straight = (m.nodes[seq[i]].XYZ() - a).Crossed(ab).Modulus() / len <= tolLine;
            if (straight) {
                ce.edges.push_back(BRepBuilderAPI_MakeEdge(vertex(seq.front()), vertex(seq.back())).Edge());
                return ce;
            }
        }
        // Circle or arc?
        if (n >= 3) {
            gp_XYZ c0(0, 0, 0), nn(0, 0, 0);
            for (int i = 0; i < n; ++i) c0 += m.nodes[seq[i]].XYZ();
            c0 /= n;
            for (int i = 0; i + 1 <= n; ++i)
                nn += (m.nodes[seq[i]].XYZ() - c0).Crossed(m.nodes[seq[i + 1]].XYZ() - c0);
            if (nn.Modulus() > 1e-14) {
                const gp_Dir pd(nn);
                const gp_Ax3 pl(gp_Pnt(c0), pd);
                const gp_XYZ X = pl.XDirection().XYZ(), Y = pl.YDirection().XYZ();
                std::vector<std::pair<double, double>> pts;
                double offPlane = 0;
                for (int v : seq) {
                    const gp_XYZ d = m.nodes[v].XYZ() - c0;
                    pts.emplace_back(d.Dot(X), d.Dot(Y));
                    offPlane = std::max(offPlane, std::fabs(d.Dot(pd.XYZ())));
                }
                double cx, cy, rad;
                if (offPlane <= tolSurf(0) * 4 && fitCircle2d(pts, cx, cy, rad)) {
                    double res = 0;
                    for (auto &p : pts) res = std::max(res, std::fabs(std::hypot(p.first - cx, p.second - cy) - rad));
                    if (res <= tolSurf(rad) * 4 && rad < 50 * diag) {
                        const gp_Pnt center(c0 + X * cx + Y * cy);
                        // The node order runs counter-clockwise about pd (nn is the polygon's area normal)
                        if (closed) {
                            for (const TopoDS_Edge &e : curvedCircles) {
                                BRepAdaptor_Curve cv(e);
                                const gp_Circ ci = cv.Circle();
                                if (ci.Location().Distance(center) > tolSurf(rad) * 8 || std::fabs(ci.Radius() - rad) > tolSurf(rad) * 8
                                    || std::fabs(ci.Axis().Direction().Dot(pd)) < 0.999)
                                    continue;
                                // Same sense as the chain?
                                gp_Pnt p; gp_Vec tan;
                                cv.D1(cv.FirstParameter(), p, tan);
                                if (e.Orientation() == TopAbs_REVERSED) tan.Reverse();
                                const gp_Vec radial(center, p);
                                const bool ccw = radial.Crossed(tan).Dot(gp_Vec(pd)) > 0;
                                ce.edges.push_back(ccw ? e : TopoDS::Edge(e.Reversed()));
                                return ce;
                            }
                        }
                        const gp_XYZ sx = m.nodes[seq.front()].XYZ() - center.XYZ();
                        gp_Ax2 cax(center, pd, gp_Dir(sx - pd.XYZ() * sx.Dot(pd.XYZ())));
                        if (!closed) {
                            // A shallow, noisy arc can give the wrong sense; then the edge would take the long
                            // way round. The middle node must lie inside the arc, and the arc as long as the chain.
                            gp_Circ probe(cax, rad);
                            const double pEnd = ElCLib::Parameter(probe, m.nodes[seq.back()]);
                            const double pMid = ElCLib::Parameter(probe, m.nodes[seq[n / 2]]);
                            if (pMid > pEnd) {
                                cax = gp_Ax2(center, pd.Reversed(), gp_Dir(sx - pd.XYZ() * sx.Dot(pd.XYZ())));
                                probe = gp_Circ(cax, rad);
                            }
                            double chain = 0;
                            for (int i = 0; i < n; ++i) chain += m.nodes[seq[i]].Distance(m.nodes[seq[i + 1]]);
                            const double arc = ElCLib::Parameter(probe, m.nodes[seq.back()]) * rad;
                            if (arc > chain * 1.5 + tolSurf(rad) * 4) {
                                for (int i = 0; i < n; ++i)
                                    ce.edges.push_back(BRepBuilderAPI_MakeEdge(vertex(seq[i]), vertex(seq[i + 1])).Edge());
                                return ce;
                            }
                        }
                        const gp_Circ circ(cax, rad);
                        const TopoDS_Vertex v0 = vertex(seq.front());
                        widen(v0, ElCLib::Value(0.0, circ));
                        if (closed) {
                            BRepBuilderAPI_MakeEdge mk(Handle(Geom_Curve)(new Geom_Circle(circ)), v0, v0, 0.0, 2 * kPi);
                            if (mk.IsDone()) ce.edges.push_back(mk.Edge());
                        } else {
                            const TopoDS_Vertex v1 = vertex(seq.back());
                            const double p1 = ElCLib::Parameter(circ, m.nodes[seq.back()]);
                            widen(v1, ElCLib::Value(p1, circ));
                            BRepBuilderAPI_MakeEdge mk(circ, v0, v1);
                            if (mk.IsDone()) ce.edges.push_back(mk.Edge());
                        }
                        if (!ce.edges.empty()) return ce;
                    }
                }
            }
        }
        // The rim of a free-form face: one spline per run (a closed rim in two halves)
        if (n >= 3 && pairOf[seq[1]] >= 0) {
            const qint64 pk = pairOf[seq[1]];
            const int ra = int(pk / R), rb = int(pk % R);
            const bool freeform = (ra < int(regions.size()) && regions[ra].kind == Region::Freeform)
                                  || (rb < int(regions.size()) && regions[rb].kind == Region::Freeform);
            std::vector<std::pair<int, int>> runs;
            if (closed) runs = {{0, n / 2}, {n / 2, n}};
            else runs = {{0, n}};
            for (const auto &run : runs) {
                if (!freeform || run.second - run.first < 3) break;
                TColgp_Array1OfPnt pts(1, run.second - run.first + 1);
                for (int i = run.first; i <= run.second; ++i) pts(i - run.first + 1) = m.nodes[seq[i]];
                Handle(Geom_BSplineCurve) curve;
                try {
                    GeomAPI_PointsToBSpline fit(pts, Approx_ChordLength, 2, 3, GeomAbs_C2, diag * 1e-4);
                    if (fit.IsDone()) curve = Handle(Geom_BSplineCurve)::DownCast(fit.Curve()->Copy());
                } catch (const Standard_Failure &) {
                }
                if (curve.IsNull()) break;
                // Clamped: the end poles are the ends, so the rim meets the junction nodes exactly
                curve->SetPole(1, pts(pts.Lower()));
                curve->SetPole(curve->NbPoles(), pts(pts.Upper()));
                // A fit that swings away from the rim (sparse or zig-zag nodes) would show as a stray loop
                double segMax = 0;
                for (int i = pts.Lower(); i < pts.Upper(); ++i) segMax = std::max(segMax, pts(i).Distance(pts(i + 1)));
                const double allowed = std::max(diag * 1e-3, std::min(segMax * 0.25, diag * 0.01));
                bool wild = false;
                const int samples = 4 * pts.Length();
                for (int k = 0; k <= samples && !wild; ++k) {
                    const gp_Pnt q = curve->Value(curve->FirstParameter()
                                                  + (curve->LastParameter() - curve->FirstParameter()) * k / samples);
                    double best = 1e300;
                    for (int i = pts.Lower(); i < pts.Upper(); ++i) {
                        const gp_XYZ a = pts(i).XYZ(), d = pts(i + 1).XYZ() - a;
                        const double len2 = d.SquareModulus();
                        const double s = len2 > 0 ? std::clamp((q.XYZ() - a).Dot(d) / len2, 0.0, 1.0) : 0.0;
                        best = std::min(best, (a + d * s - q.XYZ()).Modulus());
                    }
                    wild = best > allowed;
                }
                // ... and must stay on both faces it bounds: a swing along a steep height field would otherwise
                // be paid for with a huge edge tolerance
                for (int k = 0; k <= samples && !wild; ++k) {
                    const gp_Pnt q = curve->Value(curve->FirstParameter()
                                                  + (curve->LastParameter() - curve->FirstParameter()) * k / samples);
                    for (int rid : {ra, rb}) {
                        if (rid >= int(regions.size()) || regions[rid].dead) continue;
                        const Region &rr = regions[rid];
                        const double allow = std::max(rr.tol * 2, diag * 5e-4);
                        if (rr.kind == Region::Plane) {
                            wild = wild || surfaceDistance(rr, q.XYZ()) > allow;
                        } else if (rr.kind == Region::Freeform) {
                            const gp_XYZ d = q.XYZ() - rr.ax.Location().XYZ();
                            const double u = d.Dot(rr.ax.XDirection().XYZ()), v = d.Dot(rr.ax.YDirection().XYZ());
                            if (u < rr.dom[0] || u > rr.dom[1] || v < rr.dom[2] || v > rr.dom[3]) { wild = true; continue; }
                            wild = wild || std::fabs(rr.height->Value(u, v).Z() - d.Dot(rr.ax.Direction().XYZ())) > allow;
                        }
                    }
                }
                if (wild) break;
                const TopoDS_Vertex v0 = vertex(seq[run.first]), v1 = vertex(seq[run.second]);
                const double f0 = curve->FirstParameter(), f1 = curve->LastParameter();
                BRepBuilderAPI_MakeEdge mk(Handle(Geom_Curve)(curve), v0, v1, f0, f1);
                if (!mk.IsDone()) break;
                ce.edges.push_back(mk.Edge());
            }
            if (ce.edges.size() == runs.size() && freeform) return ce;
            ce.edges.clear();
        }
        for (int i = 0; i < n; ++i)
            ce.edges.push_back(BRepBuilderAPI_MakeEdge(vertex(seq[i]), vertex(seq[i + 1])).Edge());
        return ce;
    };
    // Edges of a chain in the given node order
    auto chainEdges = [&](std::vector<int> seq, bool closed) {
        std::vector<int> canon = seq;
        if (closed) {
            canon.pop_back();
            const auto mn = std::min_element(canon.begin(), canon.end());
            std::rotate(canon.begin(), mn, canon.end());
            std::vector<int> rev(canon.rbegin(), canon.rend());
            std::rotate(rev.begin(), std::min_element(rev.begin(), rev.end()), rev.end());
            if (rev.size() > 1 && rev[1] < canon[1]) canon = rev;
            canon.push_back(canon.front());
        } else if (canon.front() > canon.back()) {
            std::reverse(canon.begin(), canon.end());
        }
        auto it = chains.find(canon);
        if (it == chains.end()) it = chains.emplace(canon, makeChain(canon, closed)).first;
        // Same direction as the canonical chain?
        bool forward;
        if (closed) {
            const auto pos = std::find(seq.begin(), seq.end() - 1, canon[0]) - seq.begin();
            forward = seq[(pos + 1) % (seq.size() - 1)] == canon[1];
        } else {
            forward = seq.front() == canon.front() && seq[1] == canon[1];
        }
        std::vector<TopoDS_Edge> out = it->second.edges;
        if (!forward) {
            std::reverse(out.begin(), out.end());
            for (auto &e : out) e = TopoDS::Edge(e.Reversed());
        }
        return out;
    };

    for (size_t ri = 0; ri < regions.size(); ++ri) {
        const Region &r = regions[ri];
        if ((r.kind != Region::Plane && r.kind != Region::Freeform) || r.dead) continue;
        if ((ri & 127) == 0) tick(pBase + 0.03 * double(ri) / double(regions.size()), pWhat);
        try {
            // Outline loops of the region, in its own orientation (counter-clockwise about the outward normal)
            std::unordered_multimap<int, int> from;
            const auto &he = halfEdges[ri];
            for (int i = 0; i < int(he.size()); ++i) from.emplace(he[i].first, i);
            std::vector<char> used(he.size(), 0);
            std::vector<TopoDS_Wire> wires;
            std::vector<double> loopArea;
            const gp_XYZ N = r.ax.Direction().XYZ();
            for (int s = 0; s < int(he.size()); ++s) {
                if (used[s]) continue;
                std::vector<int> loop;
                int cur = s;
                while (cur >= 0 && !used[cur]) {
                    used[cur] = 1;
                    loop.push_back(he[cur].first);
                    int next = -1;
                    auto range = from.equal_range(he[cur].second);
                    for (auto it = range.first; it != range.second; ++it)
                        if (!used[it->second]) { next = it->second; break; }
                    cur = next;
                }
                if (loop.size() < 3) continue;
                gp_XYZ an(0, 0, 0);
                for (size_t i = 0; i < loop.size(); ++i)
                    an += m.nodes[loop[i]].XYZ().Crossed(m.nodes[loop[(i + 1) % loop.size()]].XYZ());
                // Split at junctions
                const auto j0 = std::find_if(loop.begin(), loop.end(), [&](int n) { return junction[n] != 0; });
                BRepBuilderAPI_MakeWire mw;
                if (j0 == loop.end()) {
                    std::vector<int> seq = loop;
                    seq.push_back(loop.front());
                    for (const auto &e : chainEdges(seq, true)) mw.Add(e);
                } else {
                    std::rotate(loop.begin(), j0, loop.end());
                    loop.push_back(loop.front());
                    size_t start = 0;
                    for (size_t i = 1; i < loop.size(); ++i) {
                        if (!junction[loop[i]] && i + 1 < loop.size()) continue;
                        const std::vector<int> seq(loop.begin() + start, loop.begin() + i + 1);
                        for (const auto &e : chainEdges(seq, false)) mw.Add(e);
                        start = i;
                    }
                }
                if (!mw.IsDone()) { error = "Could not close a face outline"; return {}; }
                wires.push_back(mw.Wire());
                loopArea.push_back(an.Dot(N));
            }
            if (wires.empty()) { error = "A planar region has no outline"; return {}; }
            const size_t outer = std::max_element(loopArea.begin(), loopArea.end()) - loopArea.begin();
            const bool freeform = r.kind == Region::Freeform;
            BRepBuilderAPI_MakeFace mk = freeform ? BRepBuilderAPI_MakeFace(r.surface, wires[outer], Standard_False)
                                                  : BRepBuilderAPI_MakeFace(gp_Pln(r.ax), wires[outer], Standard_False);
            for (size_t i = 0; i < wires.size(); ++i)
                if (i != outer) mk.Add(wires[i]);
            if (!mk.IsDone()) {
                error = "Could not build a planar face";
                if (freeform) badRegions.push_back(int(ri));
                return {};
            }
            const TopoDS_Face face = mk.Face();
            if (freeform) {
                // The outline edges are 3D only. The surface is a height field over the ax plane, so a line's or
                // spline's curve on it is the same curve with its poles dropped onto that plane (same
                // parameters); other curves are projected by ShapeFix.
                ShapeFix_Edge fixEdge;
                const double prec = std::max(r.residual * 2, diag * 1e-6);
                const gp_XYZ O = r.ax.Location().XYZ(), X = r.ax.XDirection().XYZ(), Y = r.ax.YDirection().XYZ();
                auto flat = [&](const gp_Pnt &p) { const gp_XYZ d = p.XYZ() - O; return gp_Pnt2d(d.Dot(X), d.Dot(Y)); };
                bool ok = true;
                for (TopExp_Explorer e(face, TopAbs_EDGE); e.More() && ok; e.Next()) {
                    const TopoDS_Edge ed = TopoDS::Edge(e.Current());
                    double f0, f1;
                    if (!BRep_Tool::CurveOnSurface(ed, face, f0, f1).IsNull()) continue;
                    const Handle(Geom_Curve) c3 = BRep_Tool::Curve(ed, f0, f1);
                    Handle(Geom2d_Curve) c2;
                    if (!c3.IsNull() && c3->IsKind(STANDARD_TYPE(Geom_Line))) {
                        TColgp_Array1OfPnt2d poles(1, 2);
                        poles(1) = flat(c3->Value(f0));
                        poles(2) = flat(c3->Value(f1));
                        TColStd_Array1OfReal kn(1, 2);
                        TColStd_Array1OfInteger mu(1, 2);
                        kn(1) = f0; kn(2) = f1; mu(1) = mu(2) = 2;
                        c2 = new Geom2d_BSplineCurve(poles, kn, mu, 1);
                    } else if (!c3.IsNull() && c3->IsKind(STANDARD_TYPE(Geom_BSplineCurve))) {
                        const Handle(Geom_BSplineCurve) b = Handle(Geom_BSplineCurve)::DownCast(c3);
                        TColgp_Array1OfPnt2d poles(1, b->NbPoles());
                        for (int i = 1; i <= b->NbPoles(); ++i) poles(i) = flat(b->Pole(i));
                        TColStd_Array1OfReal kn(1, b->NbKnots());
                        TColStd_Array1OfInteger mu(1, b->NbKnots());
                        b->Knots(kn);
                        b->Multiplicities(mu);
                        if (b->IsRational()) {
                            TColStd_Array1OfReal wt(1, b->NbPoles());
                            b->Weights(wt);
                            c2 = new Geom2d_BSplineCurve(poles, wt, kn, mu, b->Degree(), b->IsPeriodic());
                        } else {
                            c2 = new Geom2d_BSplineCurve(poles, kn, mu, b->Degree(), b->IsPeriodic());
                        }
                    }
                    if (!c2.IsNull()) {
                        // Off by the surface's height error only
                        double dev = 0;
                        for (int i = 0; i <= 8; ++i) {
                            const double t = f0 + (f1 - f0) * i / 8;
                            const gp_Pnt2d uv = c2->Value(t);
                            dev = std::max(dev, r.surface->Value(uv.X(), uv.Y()).Distance(c3->Value(t)));
                        }
                        const double tolE = std::max(BRep_Tool::Tolerance(ed), dev * 1.5 + prec);
                        if (qEnvironmentVariableIsSet("FIT_DEBUG") && tolE > diag * 2e-3) {
                            const gp_Pnt2d a = c2->Value(f0), b = c2->Value(f1);
                            const bool inside = a.X() >= r.dom[0] && a.X() <= r.dom[1] && a.Y() >= r.dom[2] && a.Y() <= r.dom[3]
                                                && b.X() >= r.dom[0] && b.X() <= r.dom[1] && b.Y() >= r.dom[2] && b.Y() <= r.dom[3];
                            std::fprintf(stderr, "  [fit] region %zu: edge %s dev %.3g (edge tol %.3g, prec %.3g, length %.3g, in domain %d)\n", ri,
                                         c3->IsKind(STANDARD_TYPE(Geom_Line)) ? "line" : "spline", dev, BRep_Tool::Tolerance(ed), prec,
                                         c3->Value(f0).Distance(c3->Value(f1)), int(inside));
                        }
                        bb.UpdateEdge(ed, c2, face, tolE);
                        bb.Range(ed, face, f0, f1);
                        for (TopExp_Explorer v(ed, TopAbs_VERTEX); v.More(); v.Next())
                            vertexTol.push_back({TopoDS::Vertex(v.Current()), tolE});
                        continue;
                    }
                    fixEdge.FixAddPCurve(ed, face, Standard_False, prec);
                    ok = !BRep_Tool::CurveOnSurface(ed, face, f0, f1).IsNull();
                    if (ok) BRepLib::SameParameter(ed, prec);
                }
                if (!ok) {
                    error = "Could not bound a free-form face";
                    badRegions.push_back(int(ri));
                    return {};
                }
            }
            attachMesh(face, ri);
            faces.push_back(face);
            faceRegion.push_back(int(ri));
        } catch (const Standard_Failure &) {
            // A rim or face OCCT could not build: the free-form faces at it fall back to facets
            std::unordered_set<int> ff;
            if (r.kind == Region::Freeform) ff.insert(int(ri));
            for (int t : regionTris[ri])
                for (int u : nb[t])
                    if (regions[label[u]].kind == Region::Freeform) ff.insert(label[u]);
            if (qEnvironmentVariableIsSet("FIT_DEBUG"))
                std::fprintf(stderr, "  [fit] face of region %d (kind %d) failed, %zu free-form faces around\n", int(ri),
                             int(r.kind), ff.size());
            if (ff.empty()) throw;
            badRegions.assign(ff.begin(), ff.end());
            error = "Could not build a face next to a free-form face";
            return {};
        }
    }
    for (const auto &vt : vertexTol)
        if (BRep_Tool::Tolerance(vt.first) < vt.second) bb.UpdateVertex(vt.first, vt.second);
    stage("faces");

    // Faces built from shared edges close up directly; otherwise (two curved faces meeting, a curved edge
    // split by a junction) small models are sewn, which is too slow for big ones
    const int nFaces = int(faces.size());
    const bool large = (nFaces > kLargeFaceCount || nt > kLargeTriangleCount) && !qEnvironmentVariableIsSet("FIT_CHECK");
    TopoDS_Solid result;
    TopoDS_Shell shell;
    bb.MakeShell(shell);
    for (const auto &f : faces) bb.Add(shell, f);
    bool closedUp = !cornerConflict;
    {
        // Every open edge marks the curved faces along it as not meeting their neighbours
        TopTools_IndexedMapOfShape faceIndex;
        for (const auto &f : faces) faceIndex.Add(f);
        TopTools_IndexedDataMapOfShapeListOfShape edgeFaces;
        TopExp::MapShapesAndAncestors(shell, TopAbs_EDGE, TopAbs_FACE, edgeFaces);
        std::unordered_set<int> bad(badRegions.begin(), badRegions.end());
        for (int i = 1; i <= edgeFaces.Extent(); ++i) {
            const TopoDS_Edge &e = TopoDS::Edge(edgeFaces.FindKey(i));
            if (BRep_Tool::Degenerated(e) || edgeFaces(i).Extent() == 2) continue;
            closedUp = false;
            if (qEnvironmentVariableIsSet("FIT_DEBUG")) {
                const BRepAdaptor_Curve ac(e);
                const gp_Pnt a = ac.Value(ac.FirstParameter()), b = ac.Value(ac.LastParameter());
                std::fprintf(stderr, "  [fit] open edge (%d faces) type %d (%.3f %.3f %.3f)-(%.3f %.3f %.3f)\n",
                             edgeFaces(i).Extent(), int(ac.GetType()), a.X(), a.Y(), a.Z(), b.X(), b.Y(), b.Z());
            }
            for (TopTools_ListIteratorOfListOfShape it(edgeFaces(i)); it.More(); it.Next()) {
                const int fi = faceIndex.FindIndex(it.Value()) - 1;
                if (fi >= 0 && regions[faceRegion[fi]].kind != Region::Plane) bad.insert(faceRegion[fi]);
            }
        }
        badRegions.assign(bad.begin(), bad.end());
    }
    stage("shell");
    if (closedUp) {
        shell.Closed(Standard_True);
        bb.MakeSolid(result);
        bb.Add(result, shell);
    } else if (!anyCurved || nFaces > kSewFaceCount) {
        error = anyCurved ? "The fitted faces do not meet (too many faces to sew)" : "The faces do not close up";
        return {};
    } else {
        const double tolSew = std::clamp(maxResidual * 8, diag * 1e-6, diag * 2e-3);
        BRepBuilderAPI_Sewing sew(tolSew);
        for (const auto &f : faces) sew.Add(f);
        sew.Perform();
        TopoDS_Shell sewn;
        int shells = 0;
        for (TopExp_Explorer e(sew.SewedShape(), TopAbs_SHELL); e.More(); e.Next()) { sewn = TopoDS::Shell(e.Current()); ++shells; }
        if (shells != 1 || sew.NbFreeEdges() > 0) { error = "The fitted faces do not close up"; return {}; }
        ShapeFix_Solid fix;
        result = fix.SolidFromShell(sewn);
        stage("sew");
    }
    if (result.IsNull()) { error = "The fitted faces do not form a solid"; return {}; }
    // Big shells are checked by their closed edge graph and volume only; the full check takes seconds
    if (!large) BRepLib::OrientClosedSolid(result);
    const bool valid = large || BRepCheck_Analyzer(result).IsValid();
    stage("check");
    if (!valid) {
        // Sewn faces with a pole / apex sometimes need their curves on surfaces and orientation repaired
        ShapeFix_Shape fix(result);
        fix.SetPrecision(std::max(maxResidual, diag * 1e-6));
        fix.SetMaxTolerance(diag * 2e-3);
        fix.Perform();
        TopExp_Explorer e(fix.Shape(), TopAbs_SOLID);
        if (e.More()) {
            result = TopoDS::Solid(e.Current());
            BRepLib::OrientClosedSolid(result);
        }
        if (!e.More() || !BRepCheck_Analyzer(result).IsValid()) {
            if (qEnvironmentVariableIsSet("FIT_DEBUG")) {
                int badByKind[6] = {0, 0, 0, 0, 0, 0}, badEdges = 0, badShell = 0;
                for (int i = 0; i < int(faces.size()); ++i)
                    if (!BRepCheck_Analyzer(faces[i]).IsValid()) {
                        ++badByKind[regions[faceRegion[i]].kind];
                        const Region &r = regions[faceRegion[i]];
                        int tris = 0;
                        for (int l : label) tris += l == faceRegion[i];
                        QString st;
                        BRepCheck_Analyzer an(faces[i]);
                        for (TopExp_Explorer x(faces[i], TopAbs_FACE); x.More(); x.Next()) {
                            const Handle(BRepCheck_Result) &res = an.Result(x.Current());
                            if (res.IsNull()) continue;
                            for (BRepCheck_ListIteratorOfListOfStatus it(res->Status()); it.More(); it.Next())
                                st += QString(" f%1").arg(int(it.Value()));
                        }
                        int wires = 0;
                        for (TopExp_Explorer x(faces[i], TopAbs_WIRE); x.More(); x.Next(), ++wires) {
                            const Handle(BRepCheck_Result) &res = an.Result(x.Current());
                            if (res.IsNull()) continue;
                            res->InitContextIterator();
                            for (; res->MoreShapeInContext(); res->NextShapeInContext())
                                for (BRepCheck_ListIteratorOfListOfStatus it(res->StatusOnShape()); it.More(); it.Next())
                                    if (it.Value() != BRepCheck_NoError) st += QString(" w%1").arg(int(it.Value()));
                        }
                        std::vector<int> rt;
                        for (int t = 0; t < int(label.size()); ++t)
                            if (label[t] == faceRegion[i]) rt.push_back(t);
                        double maxTol = 0, minEdge = 1e300;
                        int edges = 0;
                        for (TopExp_Explorer x(faces[i], TopAbs_VERTEX); x.More(); x.Next())
                            maxTol = std::max(maxTol, BRep_Tool::Tolerance(TopoDS::Vertex(x.Current())));
                        for (TopExp_Explorer x(faces[i], TopAbs_EDGE); x.More(); x.Next(), ++edges) {
                            GProp_GProps lp;
                            BRepGProp::LinearProperties(x.Current(), lp);
                            minEdge = std::min(minEdge, lp.Mass());
                        }
                        std::fprintf(stderr, "  [fit] invalid face: region %d kind %d, %d triangles, %d wires %d edges, pinched %d, max vertex tol %.4g, shortest edge %.4g, status%s\n",
                                     faceRegion[i], int(r.kind), tris, wires, edges, int(pinched(rt)), maxTol, minEdge, st.toUtf8().constData());
                    }
                if (e.More())
                    for (TopExp_Explorer x(result, TopAbs_EDGE); x.More(); x.Next())
                        if (!BRepCheck_Analyzer(x.Current()).IsValid()) ++badEdges;
                if (e.More())
                    for (TopExp_Explorer x(result, TopAbs_SHELL); x.More(); x.Next())
                        if (!BRepCheck_Analyzer(x.Current()).IsValid()) ++badShell;
                std::fprintf(stderr, "  [fit] invalid solid: faces by kind %d %d %d %d %d %d, %d bad edges, %d bad shells\n",
                             badByKind[0], badByKind[1], badByKind[2], badByKind[3], badByKind[4], badByKind[5], badEdges, badShell);
            }
            error = "The fitted solid is not valid";
            return {};
        }
    }
    if (large) {
        // The faces follow the triangles' normals, so an inside-out mesh gives an inside-out shell. The closed
        // edge graph stands in for the volume check, which takes seconds here.
        if (volume(m) < 0) result.Reverse();
        return result;
    }
    GProp_GProps props;
    BRepGProp::VolumeProperties(result, props);
    stage("volume");
    // Fitted round faces hold more volume than the chords of a coarse mesh, hence the slack
    const double vm = std::fabs(volume(m)), vb = std::fabs(props.Mass());
    if (vm <= 0 || std::fabs(vb - vm) > vm * (anyCurved ? 0.06 : 0.001) + diag * diag * diag * 1e-9) {
        error = "The fitted solid does not match the mesh volume";
        return {};
    }
    return result;
}

}   // namespace

TopoDS_Shape toFittedBRep(const Mesh &m, bool prismatic, QString &error, QString *summary, const Progress &progress) {
    QString s;
    TopoDS_Shape out;
    try {
        out = Fitter(m, prismatic, 0, progress).run(error, s);
    } catch (const Standard_Failure &e) {
        error = QString("Fitting failed: %1").arg(e.GetMessageString());
        out.Nullify();
    }
    if (summary) *summary = s;
    return out;
}

int generateFaceGroups(Mesh &m, bool accurate, double angleDeg, double minAreaFraction, QString &error,
                       double boundaryTolerance, const Progress &progress) {
    if (m.tris.empty()) { error = "The mesh is empty"; return 0; }
    if (progress) progress(0, "Welding the mesh");
    m.groups.clear();
    double d = 0;
    {
        double lo[3] = {1e300, 1e300, 1e300}, hi[3] = {-1e300, -1e300, -1e300};
        for (const gp_Pnt &p : m.nodes)
            for (int k = 0; k < 3; ++k) { lo[k] = std::min(lo[k], p.Coord(k + 1)); hi[k] = std::max(hi[k], p.Coord(k + 1)); }
        d = std::max(1e-9, std::sqrt((hi[0] - lo[0]) * (hi[0] - lo[0]) + (hi[1] - lo[1]) * (hi[1] - lo[1]) +
                                     (hi[2] - lo[2]) * (hi[2] - lo[2])));
    }
    weld(m, d * 1e-7);   // the same weld the fitter does, so its triangle numbers match
    const int nt = int(m.tris.size());
    if (!nt) { error = "The mesh has no usable triangles"; return 0; }

    // Neighbours across manifold edges (open and non-manifold edges separate groups)
    std::vector<gp_XYZ> nrm(nt);
    std::vector<double> area(nt);
    for (int t = 0; t < nt; ++t) {
        const auto &tr = m.tris[t];
        const gp_XYZ c = (m.nodes[tr[1]].XYZ() - m.nodes[tr[0]].XYZ()).Crossed(m.nodes[tr[2]].XYZ() - m.nodes[tr[0]].XYZ());
        area[t] = c.Modulus() / 2;
        nrm[t] = c.Modulus() > 0 ? c / c.Modulus() : gp_XYZ(0, 0, 0);
    }
    std::unordered_map<quint64, std::vector<int>> edges;
    edges.reserve(size_t(nt) * 2);
    for (int t = 0; t < nt; ++t)
        for (int k = 0; k < 3; ++k) edges[key2(m.tris[t][k], m.tris[t][(k + 1) % 3])].push_back(t);
    std::vector<std::vector<std::pair<int, double>>> nb(nt);   // neighbour, shared edge length
    for (const auto &kv : edges) {
        if (kv.second.size() != 2) continue;
        const int a = int(kv.first >> 32), b = int(kv.first & 0xFFFFFFFF);
        const double len = m.nodes[a].Distance(m.nodes[b]);
        nb[kv.second[0]].push_back({kv.second[1], len});
        nb[kv.second[1]].push_back({kv.second[0], len});
    }

    std::vector<int> label(nt, -1);
    int next = 0;
    // Fast uses the dialog angle. Accurate leftovers (if any) use a tight angle so fillets stay as
    // thin bands instead of blotching into neighbouring flats — Fusion Accurate has no angle control.
    const double cosAngle = std::cos(std::clamp(accurate ? 8.0 : angleDeg, 0.1, 89.0) * kPi / 180);
    // Flood fill over the triangles allowed by `free`, crossing edges that bend less than the angle
    auto flood = [&](const std::vector<char> *free) {
        for (int s = 0; s < nt; ++s) {
            if (label[s] >= 0 || (free && !(*free)[s])) continue;
            const int id = next++;
            std::vector<int> stack{s};
            label[s] = id;
            while (!stack.empty()) {
                const int t = stack.back();
                stack.pop_back();
                for (const auto &e : nb[t]) {
                    const int u = e.first;
                    if (label[u] >= 0 || (free && !(*free)[u])) continue;
                    if (area[u] > 0 && area[t] > 0 && nrm[u].Dot(nrm[t]) < cosAngle) continue;
                    label[u] = id;
                    stack.push_back(u);
                }
            }
        }
    };

    bool fitted = false;
    if (accurate) {
        std::vector<int> regions;
        std::vector<char> real;
        QString why;
        Progress sub;
        if (progress) sub = [&progress](double v, const QString &what) { progress(v * 0.9, what); };
        Fitter f(m, true, boundaryTolerance, sub);
        if (f.segmentOnly(why, regions, real) && int(regions.size()) == nt) {
            // Fitted surfaces keep their region; leftover facets are joined by a tight angle
            std::unordered_map<int, int> ids;
            for (int t = 0; t < nt; ++t)
                if (real[t]) {
                    auto it = ids.find(regions[t]);
                    if (it == ids.end()) it = ids.emplace(regions[t], next++).first;
                    label[t] = it->second;
                }
            std::vector<char> loose(nt);
            for (int t = 0; t < nt; ++t) loose[t] = !real[t];
            flood(&loose);
            fitted = true;
        }
    }
    if (!fitted) flood(nullptr);
    if (progress) progress(0.92, "Joining small groups");

    // Small-group merging is Fast-only (Fusion Accurate has no minimum face group size)
    double total = 0;
    for (double a : area) total += a;
    const double minArea = accurate ? 0.0 : total * std::max(0.0, minAreaFraction);
    for (int round = 0; round < 50 && minArea > 0; ++round) {
        std::vector<double> ga(next, 0.0);
        for (int t = 0; t < nt; ++t) ga[label[t]] += area[t];
        std::vector<std::unordered_map<int, double>> border(next);
        for (int t = 0; t < nt; ++t)
            for (const auto &e : nb[t])
                if (label[e.first] != label[t]) border[label[t]][label[e.first]] += e.second;
        std::vector<int> into(next);
        for (int g = 0; g < next; ++g) into[g] = g;
        bool changed = false;
        // Smallest first; a group that already received another this round stays put
        std::vector<int> order(next);
        for (int g = 0; g < next; ++g) order[g] = g;
        std::sort(order.begin(), order.end(), [&](int a, int b) { return ga[a] < ga[b]; });
        std::vector<char> receiver(next, 0);
        for (int g : order) {
            if (ga[g] <= 0 || ga[g] >= minArea || receiver[g] || border[g].empty()) continue;
            int best = -1;
            double bestLen = -1;
            for (const auto &kv : border[g])
                if (kv.second > bestLen) { bestLen = kv.second; best = kv.first; }
            while (into[best] != best) best = into[best];
            if (best == g) continue;
            into[g] = best;
            receiver[best] = 1;
            changed = true;
        }
        if (!changed) break;
        for (int t = 0; t < nt; ++t) {
            int g = label[t];
            while (into[g] != g) g = into[g];
            label[t] = g;
        }
    }
    // Number the groups 0.. in triangle order
    std::unordered_map<int, int> renumber;
    m.groups.resize(nt);
    for (int t = 0; t < nt; ++t) {
        auto it = renumber.find(label[t]);
        if (it == renumber.end()) it = renumber.emplace(label[t], int(renumber.size())).first;
        m.groups[t] = it->second;
    }
    return int(renumber.size());
}

}   // namespace MeshUtils
