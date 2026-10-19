// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#include "MeshUtils.h"
#include "GeomUtils.h"

#include <QDataStream>
#include <QFile>
#include <QHash>
#include <QFileInfo>
#include <QRegularExpression>
#include <QTextStream>

#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <BRepLib.hxx>
#include <BRepMesh_IncrementalMesh.hxx>
#include <BRep_Builder.hxx>
#include <BRep_Tool.hxx>
#include <Poly_Triangulation.hxx>
#include <ShapeFix_Face.hxx>
#include <Standard_Failure.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Shell.hxx>
#include <TopoDS_Solid.hxx>
#include <TopoDS_Vertex.hxx>
#include <TopoDS_Wire.hxx>
#include <algorithm>
#include <cmath>
#include <functional>
#include <map>
#include <unordered_map>
#include <unordered_set>

namespace MeshUtils {

namespace {

quint64 edgeKey(int a, int b) {
    if (a > b) std::swap(a, b);
    return (quint64(quint32(a)) << 32) | quint32(b);
}

gp_Vec triNormal(const Mesh &m, const std::array<int, 3> &t) {
    const gp_Vec u(m.nodes[t[0]], m.nodes[t[1]]), v(m.nodes[t[0]], m.nodes[t[2]]);
    return u.Crossed(v);
}

double diag(const Mesh &m) {
    if (m.nodes.empty()) return 0;
    double lo[3] = {1e300, 1e300, 1e300}, hi[3] = {-1e300, -1e300, -1e300};
    for (const gp_Pnt &p : m.nodes)
        for (int k = 0; k < 3; ++k) {
            lo[k] = std::min(lo[k], p.Coord(k + 1));
            hi[k] = std::max(hi[k], p.Coord(k + 1));
        }
    return std::sqrt((hi[0] - lo[0]) * (hi[0] - lo[0]) + (hi[1] - lo[1]) * (hi[1] - lo[1]) + (hi[2] - lo[2]) * (hi[2] - lo[2]));
}

// Appends the triangulation of every face of `shape` (already meshed) to m
void appendFaces(const TopoDS_Shape &shape, Mesh &m) {
    for (TopExp_Explorer exp(shape, TopAbs_FACE); exp.More(); exp.Next()) {
        const TopoDS_Face face = TopoDS::Face(exp.Current());
        TopLoc_Location loc;
        Handle(Poly_Triangulation) tri = BRep_Tool::Triangulation(face, loc);
        if (tri.IsNull()) continue;
        const gp_Trsf trsf = loc.Transformation();
        const int base = int(m.nodes.size());
        for (int i = 1; i <= tri->NbNodes(); ++i) m.nodes.push_back(tri->Node(i).Transformed(trsf));
        const bool rev = face.Orientation() == TopAbs_REVERSED;
        for (int t = 1; t <= tri->NbTriangles(); ++t) {
            int a, b, c;
            tri->Triangle(t).Get(a, b, c);
            if (rev) std::swap(b, c);
            m.tris.push_back({base + a - 1, base + b - 1, base + c - 1});
        }
    }
}

bool readStl(const QByteArray &bytes, double scale, Mesh &m, QString &error) {
    auto add = [&](float x, float y, float z) { m.nodes.emplace_back(x * scale, y * scale, z * scale); };
    if (bytes.size() >= 84) {
        quint32 n = 0;
        memcpy(&n, bytes.constData() + 80, 4);
        if (qint64(84) + qint64(n) * 50 == bytes.size()) {
            const char *p = bytes.constData() + 84;
            for (quint32 i = 0; i < n; ++i, p += 50) {
                float v[12];
                memcpy(v, p, 48);
                const int base = int(m.nodes.size());
                add(v[3], v[4], v[5]); add(v[6], v[7], v[8]); add(v[9], v[10], v[11]);
                m.tris.push_back({base, base + 1, base + 2});
            }
            return true;
        }
    }
    if (!bytes.trimmed().startsWith("solid")) { error = "Not an STL file"; return false; }
    QTextStream in(bytes);
    int pending = 0;
    while (!in.atEnd()) {
        const QString line = in.readLine().trimmed();
        if (!line.startsWith("vertex")) continue;
        const QStringList f = line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
        if (f.size() < 4) { error = "Bad STL vertex line"; return false; }
        add(f[1].toFloat(), f[2].toFloat(), f[3].toFloat());
        if (++pending == 3) {
            const int base = int(m.nodes.size()) - 3;
            m.tris.push_back({base, base + 1, base + 2});
            pending = 0;
        }
    }
    return true;
}

bool readObj(const QByteArray &bytes, double scale, Mesh &m, QString &error) {
    QTextStream in(bytes);
    while (!in.atEnd()) {
        const QString line = in.readLine().trimmed();
        if (line.startsWith("v ")) {
            const QStringList f = line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
            if (f.size() < 4) { error = "Bad OBJ vertex line"; return false; }
            m.nodes.emplace_back(f[1].toDouble() * scale, f[2].toDouble() * scale, f[3].toDouble() * scale);
        } else if (line.startsWith("f ")) {
            const QStringList f = line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
            std::vector<int> idx;
            for (int k = 1; k < f.size(); ++k) {
                int i = f[k].section('/', 0, 0).toInt();
                i = i < 0 ? int(m.nodes.size()) + i : i - 1;
                if (i < 0 || i >= int(m.nodes.size())) { error = "OBJ face refers to a missing vertex"; return false; }
                idx.push_back(i);
            }
            for (size_t k = 1; k + 1 < idx.size(); ++k) m.tris.push_back({idx[0], idx[k], idx[k + 1]});
        }
    }
    return true;
}

}   // namespace

bool isMesh(const TopoDS_Shape &shape) {
    if (shape.IsNull()) return false;
    TopExp_Explorer exp(shape, TopAbs_FACE);
    if (!exp.More()) return false;
    const TopoDS_Face f = TopoDS::Face(exp.Current());
    TopLoc_Location loc;
    return BRep_Tool::Surface(f, loc).IsNull() && !BRep_Tool::Triangulation(f, loc).IsNull();
}

Mesh fromShape(const TopoDS_Shape &shape, double deflection) {
    Mesh m;
    if (shape.IsNull()) return m;
    if (!isMesh(shape)) {
        try {
            BRepMesh_IncrementalMesh(shape, deflection, Standard_False, 0.3, Standard_True);
        } catch (const Standard_Failure &) {
            return m;
        }
    }
    appendFaces(shape, m);
    if (isMesh(shape) && groupCount(shape) > 1) {
        // One face per group: the triangles were appended face by face
        int g = 0;
        for (TopExp_Explorer exp(shape, TopAbs_FACE); exp.More(); exp.Next(), ++g) {
            TopLoc_Location loc;
            const Handle(Poly_Triangulation) tri = BRep_Tool::Triangulation(TopoDS::Face(exp.Current()), loc);
            if (!tri.IsNull()) m.groups.insert(m.groups.end(), size_t(tri->NbTriangles()), g);
        }
    }
    weld(m, std::max(1e-9, diag(m) * 1e-7));
    return m;
}

int groupCount(const TopoDS_Shape &shape) {
    if (!isMesh(shape)) return 0;
    int n = 0;
    for (TopExp_Explorer exp(shape, TopAbs_FACE); exp.More(); exp.Next()) ++n;
    return std::max(1, n);
}

TopoDS_Shape toShape(const Mesh &m) {
    if (!m.hasGroups()) return toFace(m);
    int count = 0;
    for (int g : m.groups) count = std::max(count, g + 1);
    if (count <= 1) return toFace(m);
    std::vector<std::vector<int>> byGroup(count);
    for (int t = 0; t < int(m.tris.size()); ++t) byGroup[m.groups[t]].push_back(t);
    BRep_Builder bb;
    TopoDS_Compound comp;
    bb.MakeCompound(comp);
    std::vector<int> local(m.nodes.size(), 0);
    for (const auto &tris : byGroup) {
        if (tris.empty()) continue;
        Mesh part;
        for (int t : tris) {
            std::array<int, 3> tr;
            for (int k = 0; k < 3; ++k) {
                const int v = m.tris[t][k];
                if (!local[v]) { part.nodes.push_back(m.nodes[v]); local[v] = int(part.nodes.size()); }
                tr[k] = local[v] - 1;
            }
            part.tris.push_back(tr);
        }
        for (int t : tris)
            for (int v : m.tris[t]) local[v] = 0;
        bb.Add(comp, toFace(part));
    }
    return comp;
}

TopoDS_Face toFace(const Mesh &m) {
    Handle(Poly_Triangulation) tri = new Poly_Triangulation(int(m.nodes.size()), int(m.tris.size()), Standard_False);
    for (int i = 0; i < int(m.nodes.size()); ++i) tri->SetNode(i + 1, m.nodes[i]);
    for (int t = 0; t < int(m.tris.size()); ++t)
        tri->SetTriangle(t + 1, Poly_Triangle(m.tris[t][0] + 1, m.tris[t][1] + 1, m.tris[t][2] + 1));
    TopoDS_Face f;
    BRep_Builder().MakeFace(f, tri);
    return f;
}

bool readFile(const QString &path, double scale, Mesh &out, QString &error) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) { error = "Cannot open " + path; return false; }
    const QByteArray bytes = file.readAll();
    out = Mesh();
    const QString ext = QFileInfo(path).suffix().toLower();
    const bool ok = ext == "obj" ? readObj(bytes, scale, out, error) : readStl(bytes, scale, out, error);
    if (!ok) return false;
    weld(out, std::max(1e-9, diag(out) * 1e-7));
    if (out.empty()) { error = "The file has no triangles"; return false; }
    return true;
}

void weld(Mesh &m, double tol) {
    struct Key { qint64 x, y, z; bool operator==(const Key &o) const { return x == o.x && y == o.y && z == o.z; } };
    struct Hash { size_t operator()(const Key &k) const { return size_t(k.x * 73856093 ^ k.y * 19349663 ^ k.z * 83492791); } };
    std::unordered_map<Key, int, Hash> index;
    std::vector<int> remap(m.nodes.size());
    std::vector<gp_Pnt> nodes;
    for (size_t i = 0; i < m.nodes.size(); ++i) {
        const gp_Pnt &p = m.nodes[i];
        const Key k{qint64(std::llround(p.X() / tol)), qint64(std::llround(p.Y() / tol)), qint64(std::llround(p.Z() / tol))};
        auto it = index.find(k);
        if (it == index.end()) {
            it = index.emplace(k, int(nodes.size())).first;
            nodes.push_back(p);
        }
        remap[i] = it->second;
    }
    std::vector<std::array<int, 3>> tris;
    std::vector<int> groups;
    const bool grouped = m.hasGroups();
    tris.reserve(m.tris.size());
    for (size_t i = 0; i < m.tris.size(); ++i) {
        auto t = m.tris[i];
        for (int &v : t) v = remap[v];
        if (t[0] == t[1] || t[1] == t[2] || t[0] == t[2]) continue;
        tris.push_back(t);
        if (grouped) groups.push_back(m.groups[i]);
    }
    m.nodes = std::move(nodes);
    m.tris = std::move(tris);
    m.groups = std::move(groups);
}

double volume(const Mesh &m) {
    double v = 0;
    for (const auto &t : m.tris) {
        const gp_XYZ a = m.nodes[t[0]].XYZ(), b = m.nodes[t[1]].XYZ(), c = m.nodes[t[2]].XYZ();
        v += a.Dot(b.Crossed(c));
    }
    return v / 6.0;
}

double area(const Mesh &m) {
    double a = 0;
    for (const auto &t : m.tris) a += 0.5 * triNormal(m, t).Magnitude();
    return a;
}

gp_Pnt centroid(const Mesh &m) {
    gp_XYZ sv(0, 0, 0), sa(0, 0, 0);
    double v = 0, a = 0;
    for (const auto &t : m.tris) {
        const gp_XYZ p = m.nodes[t[0]].XYZ(), q = m.nodes[t[1]].XYZ(), r = m.nodes[t[2]].XYZ();
        const double dv = p.Dot(q.Crossed(r)) / 6.0;
        sv += (p + q + r) * (dv / 4.0);
        v += dv;
        const double da = 0.5 * (q - p).Crossed(r - p).Modulus();
        sa += (p + q + r) * (da / 3.0);
        a += da;
    }
    if (std::fabs(v) > 1e-12 && isClosed(m)) return gp_Pnt(sv / v);
    return a > 1e-12 ? gp_Pnt(sa / a) : gp_Pnt();
}

bool isClosed(const Mesh &m) {
    std::unordered_map<quint64, int> count;
    for (const auto &t : m.tris)
        for (int k = 0; k < 3; ++k) ++count[edgeKey(t[k], t[(k + 1) % 3])];
    for (const auto &e : count)
        if (e.second != 2) return false;
    return !m.tris.empty();
}

void reverse(Mesh &m) {
    for (auto &t : m.tris) std::swap(t[1], t[2]);
}

void smooth(Mesh &m, int iterations, double factor) {
    const int n = int(m.nodes.size());
    std::vector<std::vector<int>> nbr(n);
    std::unordered_map<quint64, int> count;
    for (const auto &t : m.tris)
        for (int k = 0; k < 3; ++k) {
            const int a = t[k], b = t[(k + 1) % 3];
            if (++count[edgeKey(a, b)] == 1) { nbr[a].push_back(b); nbr[b].push_back(a); }
        }
    std::vector<bool> fixed(n, false);
    for (const auto &e : count)
        if (e.second == 1) { fixed[int(e.first >> 32)] = true; fixed[int(e.first & 0xffffffffu)] = true; }
    auto step = [&](double w) {
        std::vector<gp_Pnt> next = m.nodes;
        for (int i = 0; i < n; ++i) {
            if (fixed[i] || nbr[i].empty()) continue;
            gp_XYZ avg(0, 0, 0);
            for (int j : nbr[i]) avg += m.nodes[j].XYZ();
            avg /= double(nbr[i].size());
            next[i] = gp_Pnt(m.nodes[i].XYZ() + (avg - m.nodes[i].XYZ()) * w);
        }
        m.nodes = std::move(next);
    };
    // Taubin lambda|mu: the negative second pass undoes the shrinking of plain Laplacian smoothing
    const double lambda = std::clamp(factor, 0.01, 1.0);
    const double mu = 1.0 / (0.1 - 1.0 / lambda);
    for (int it = 0; it < iterations; ++it) {
        step(lambda);
        step(mu);
    }
}

void reduce(Mesh &m, double ratio) {
    m.groups.clear();
    if (m.tris.empty() || ratio >= 1.0) return;
    const size_t target = std::max<size_t>(4, size_t(m.tris.size() * std::max(0.01, ratio)));
    double lo[3] = {1e300, 1e300, 1e300};
    for (const gp_Pnt &p : m.nodes)
        for (int k = 0; k < 3; ++k) lo[k] = std::min(lo[k], p.Coord(k + 1));

    auto cluster = [&](double h, Mesh &out) {
        struct Key { qint64 x, y, z; bool operator==(const Key &o) const { return x == o.x && y == o.y && z == o.z; } };
        struct Hash { size_t operator()(const Key &k) const { return size_t(k.x * 73856093 ^ k.y * 19349663 ^ k.z * 83492791); } };
        std::unordered_map<Key, int, Hash> cell;
        std::vector<gp_XYZ> sum;
        std::vector<int> cnt, remap(m.nodes.size());
        for (size_t i = 0; i < m.nodes.size(); ++i) {
            const gp_Pnt &p = m.nodes[i];
            const Key k{qint64((p.X() - lo[0]) / h), qint64((p.Y() - lo[1]) / h), qint64((p.Z() - lo[2]) / h)};
            auto it = cell.find(k);
            if (it == cell.end()) { it = cell.emplace(k, int(sum.size())).first; sum.emplace_back(0, 0, 0); cnt.push_back(0); }
            sum[it->second] += p.XYZ();
            ++cnt[it->second];
            remap[i] = it->second;
        }
        out = Mesh();
        for (size_t c = 0; c < sum.size(); ++c) out.nodes.emplace_back(sum[c] / double(cnt[c]));
        std::unordered_set<quint64> seen;
        for (const auto &t : m.tris) {
            std::array<int, 3> r{remap[t[0]], remap[t[1]], remap[t[2]]};
            if (r[0] == r[1] || r[1] == r[2] || r[0] == r[2]) continue;
            std::array<int, 3> s = r;
            std::sort(s.begin(), s.end());
            const quint64 key = (quint64(s[0]) * 2654435761u) ^ (quint64(s[1]) << 21) ^ (quint64(s[2]) << 42);
            if (!seen.insert(key).second) continue;
            out.tris.push_back(r);
        }
    };
    double a = 1e-9, b = diag(m);
    Mesh best = m, trial;
    for (int it = 0; it < 18; ++it) {
        const double h = std::sqrt(a * b);
        cluster(h, trial);
        if (trial.tris.size() > target) a = h;
        else { b = h; best = trial; }
    }
    m = best;
}

RepairStats repair(Mesh &m, double tol, bool orient, bool fillHoles) {
    m.groups.clear();
    RepairStats st;
    const size_t nodes0 = m.nodes.size();
    weld(m, std::max(1e-12, tol));
    st.welded = int(nodes0 - m.nodes.size());

    std::unordered_set<quint64> seen;
    std::vector<std::array<int, 3>> unique;
    unique.reserve(m.tris.size());
    for (const auto &t : m.tris) {
        std::array<int, 3> s = t;
        std::sort(s.begin(), s.end());
        const quint64 key = (quint64(s[0]) * 2654435761u) ^ (quint64(s[1]) << 21) ^ (quint64(s[2]) << 42);
        if (seen.insert(key).second) unique.push_back(t);
        else ++st.duplicates;
    }
    m.tris = std::move(unique);

    if (orient && !m.tris.empty()) {
        std::unordered_map<quint64, std::vector<int>> byEdge;
        for (int i = 0; i < int(m.tris.size()); ++i)
            for (int k = 0; k < 3; ++k) byEdge[edgeKey(m.tris[i][k], m.tris[i][(k + 1) % 3])].push_back(i);
        auto hasDirected = [&](int tri, int a, int b) {
            const auto &t = m.tris[tri];
            for (int k = 0; k < 3; ++k)
                if (t[k] == a && t[(k + 1) % 3] == b) return true;
            return false;
        };
        std::vector<char> done(m.tris.size(), 0), flip(m.tris.size(), 0);
        for (int seed = 0; seed < int(m.tris.size()); ++seed) {
            if (done[seed]) continue;
            std::vector<int> piece{seed}, queue{seed};
            done[seed] = 1;
            bool closed = true;
            while (!queue.empty()) {
                const int cur = queue.back();
                queue.pop_back();
                for (int k = 0; k < 3; ++k) {
                    const int a = m.tris[cur][k], b = m.tris[cur][(k + 1) % 3];
                    const auto &users = byEdge[edgeKey(a, b)];
                    if (users.size() != 2) closed = false;
                    if (users.size() > 2) continue;   // non-manifold: no reliable neighbour to copy the winding from
                    for (int nb : users) {
                        if (nb == cur || done[nb]) continue;
                        // Consistent neighbours run the shared edge the other way
                        if (hasDirected(nb, a, b)) { std::swap(m.tris[nb][1], m.tris[nb][2]); flip[nb] ^= 1; }
                        done[nb] = 1;
                        piece.push_back(nb);
                        queue.push_back(nb);
                    }
                }
            }
            if (!closed) continue;
            double v = 0;
            for (int i : piece) {
                const auto &t = m.tris[i];
                v += m.nodes[t[0]].XYZ().Dot(m.nodes[t[1]].XYZ().Crossed(m.nodes[t[2]].XYZ()));
            }
            if (v < 0)
                for (int i : piece) { std::swap(m.tris[i][1], m.tris[i][2]); flip[i] ^= 1; }
        }
        for (char f : flip) st.flipped += f;
    }

    if (fillHoles) {
        // A boundary edge a->b is used once; its fill triangle runs b->a, so the loops chain the reversed edges
        std::unordered_map<quint64, int> count;
        for (const auto &t : m.tris)
            for (int k = 0; k < 3; ++k) ++count[edgeKey(t[k], t[(k + 1) % 3])];
        std::multimap<int, int> next;
        for (const auto &t : m.tris)
            for (int k = 0; k < 3; ++k)
                if (count[edgeKey(t[k], t[(k + 1) % 3])] == 1) next.emplace(t[(k + 1) % 3], t[k]);
        while (!next.empty()) {
            std::vector<int> loop{next.begin()->first};
            int cur = next.begin()->second;
            next.erase(next.begin());
            while (cur != loop.front()) {
                loop.push_back(cur);
                auto it = next.find(cur);
                if (it == next.end()) break;
                cur = it->second;
                next.erase(it);
            }
            if (cur != loop.front() || loop.size() < 3) continue;
            ++st.holes;
            if (loop.size() == 3) {
                m.tris.push_back({loop[0], loop[1], loop[2]});
                continue;
            }
            gp_XYZ c(0, 0, 0);
            for (int i : loop) c += m.nodes[i].XYZ();
            m.nodes.emplace_back(c / double(loop.size()));
            const int ci = int(m.nodes.size()) - 1;
            for (size_t k = 0; k < loop.size(); ++k) m.tris.push_back({loop[k], loop[(k + 1) % loop.size()], ci});
        }
    }
    st.closed = isClosed(m);
    return st;
}

void append(Mesh &m, const Mesh &other) {
    if (m.hasGroups() && other.hasGroups()) {
        const int offset = *std::max_element(m.groups.begin(), m.groups.end()) + 1;
        for (int g : other.groups) m.groups.push_back(g + offset);
    } else {
        m.groups.clear();
    }
    const int base = int(m.nodes.size());
    m.nodes.insert(m.nodes.end(), other.nodes.begin(), other.nodes.end());
    for (const auto &t : other.tris) m.tris.push_back({t[0] + base, t[1] + base, t[2] + base});
}

bool shell(Mesh &m, double thickness, bool outward, QString &error) {
    m.groups.clear();
    if (m.tris.empty()) { error = "The mesh is empty"; return false; }
    if (thickness <= 0) { error = "The thickness must be positive"; return false; }
    const bool closed = isClosed(m);
    if (closed && volume(m) < 0) reverse(m);
    const int n = int(m.nodes.size());
    std::vector<gp_XYZ> normal(n, gp_XYZ(0, 0, 0));
    for (const auto &t : m.tris) {
        const gp_XYZ w = triNormal(m, t).XYZ();   // area weighted
        for (int k = 0; k < 3; ++k) normal[t[k]] += w;
    }
    const double s = outward ? thickness : -thickness;
    Mesh out;
    out.nodes = m.nodes;
    for (int i = 0; i < n; ++i) {
        const double len = normal[i].Modulus();
        out.nodes.emplace_back(m.nodes[i].XYZ() + (len > 1e-300 ? normal[i] / len : gp_XYZ(0, 0, 0)) * s);
    }
    // Forward copy A faces out of the wall, the reversed copy B faces into the cavity
    const int a = outward ? n : 0, b = outward ? 0 : n;
    for (const auto &t : m.tris) {
        out.tris.push_back({t[0] + a, t[1] + a, t[2] + a});
        out.tris.push_back({t[0] + b, t[2] + b, t[1] + b});
    }
    if (!closed) {
        std::unordered_map<quint64, int> count;
        for (const auto &t : m.tris)
            for (int k = 0; k < 3; ++k) ++count[edgeKey(t[k], t[(k + 1) % 3])];
        for (const auto &t : m.tris)
            for (int k = 0; k < 3; ++k) {
                const int p = t[k], q = t[(k + 1) % 3];
                if (count[edgeKey(p, q)] != 1) continue;
                out.tris.push_back({q + a, p + a, p + b});
                out.tris.push_back({q + a, p + b, q + b});
            }
    }
    m = std::move(out);
    return true;
}

bool planeCut(Mesh &m, const gp_Pln &plane, bool fill, QString &error) {
    m.groups.clear();
    const gp_Dir n = plane.Axis().Direction();
    const gp_Pnt o = plane.Location();
    auto dist = [&](const gp_Pnt &p) { return gp_Vec(o, p).Dot(gp_Vec(n)); };
    const double eps = std::max(1e-9, diag(m) * 1e-9);
    std::vector<double> d(m.nodes.size());
    for (size_t i = 0; i < m.nodes.size(); ++i) d[i] = dist(m.nodes[i]);

    std::unordered_map<quint64, int> cutNode;
    auto crossing = [&](int a, int b) {
        const quint64 k = edgeKey(a, b);
        auto it = cutNode.find(k);
        if (it != cutNode.end()) return it->second;
        const double t = d[a] / (d[a] - d[b]);
        const gp_Pnt p(m.nodes[a].XYZ() + (m.nodes[b].XYZ() - m.nodes[a].XYZ()) * t);
        m.nodes.push_back(p);
        d.push_back(0);
        const int idx = int(m.nodes.size()) - 1;
        cutNode.emplace(k, idx);
        return idx;
    };
    std::vector<std::array<int, 3>> kept;
    std::vector<std::pair<int, int>> segments;   // directed cut segments (for the caps)
    bool touched = false;
    for (const auto &t : m.tris) {
        const bool in[3] = {d[t[0]] <= eps, d[t[1]] <= eps, d[t[2]] <= eps};
        const int nin = in[0] + in[1] + in[2];
        if (nin == 3) { kept.push_back(t); continue; }
        touched = true;
        if (nin == 0) continue;
        // Clip the triangle polygon against the plane, keeping its winding
        std::vector<int> poly, cuts;
        for (int k = 0; k < 3; ++k) {
            const int a = t[k], b = t[(k + 1) % 3];
            const bool ia = in[k], ib = in[(k + 1) % 3];
            if (ia) poly.push_back(a);
            if (ia != ib && std::fabs(d[a]) > eps && std::fabs(d[b]) > eps) {
                const int c = crossing(a, b);
                poly.push_back(c);
                cuts.push_back(c);
            } else if (ia != ib) {
                cuts.push_back(ia ? a : b);
            }
        }
        for (size_t k = 1; k + 1 < poly.size(); ++k) kept.push_back({poly[0], poly[k], poly[k + 1]});
        if (cuts.size() == 2 && cuts[0] != cuts[1]) segments.emplace_back(cuts[0], cuts[1]);
    }
    if (!touched) { error = "The plane does not cut the mesh"; return false; }
    m.tris = std::move(kept);
    if (m.tris.empty()) { error = "Nothing is left behind the plane"; return false; }

    if (fill && !segments.empty()) {
        // Chain the segments into loops and cap each loop with a meshed planar face
        std::multimap<int, int> next;
        for (const auto &s : segments) next.emplace(s.first, s.second);
        std::vector<std::vector<int>> loops;
        while (!next.empty()) {
            std::vector<int> loop{next.begin()->first};
            int cur = next.begin()->second;
            next.erase(next.begin());
            while (cur != loop.front()) {
                loop.push_back(cur);
                auto it = next.find(cur);
                if (it == next.end()) break;
                cur = it->second;
                next.erase(it);
            }
            if (cur == loop.front() && loop.size() >= 3) loops.push_back(loop);
        }
        // Outer loops and the holes inside them (2D in plane coordinates)
        const gp_Ax3 ax = plane.Position();
        auto uv = [&](int i) {
            const gp_Vec v(o, m.nodes[i]);
            return std::pair<double, double>(v.Dot(gp_Vec(ax.XDirection())), v.Dot(gp_Vec(ax.YDirection())));
        };
        auto loopArea = [&](const std::vector<int> &l) {
            double a = 0;
            for (size_t k = 0; k < l.size(); ++k) {
                const auto p = uv(l[k]), q = uv(l[(k + 1) % l.size()]);
                a += p.first * q.second - q.first * p.second;
            }
            return a / 2;
        };
        auto inside = [&](const std::vector<int> &l, std::pair<double, double> p) {
            bool c = false;
            for (size_t k = 0, j = l.size() - 1; k < l.size(); j = k++) {
                const auto a = uv(l[k]), b = uv(l[j]);
                if (((a.second > p.second) != (b.second > p.second))
                    && (p.first < (b.first - a.first) * (p.second - a.second) / (b.second - a.second) + a.first))
                    c = !c;
            }
            return c;
        };
        std::vector<int> order(loops.size());
        for (size_t k = 0; k < loops.size(); ++k) order[k] = int(k);
        std::sort(order.begin(), order.end(), [&](int a, int b) { return std::fabs(loopArea(loops[a])) > std::fabs(loopArea(loops[b])); });
        std::vector<int> parent(loops.size(), -1), depth(loops.size(), 0);
        for (size_t i = 0; i < order.size(); ++i) {
            const int li = order[i];
            for (int j = int(i) - 1; j >= 0; --j) {
                const int lj = order[j];
                if (inside(loops[lj], uv(loops[li][0]))) { parent[li] = lj; depth[li] = depth[lj] + 1; break; }
            }
        }
        auto wireOf = [&](const std::vector<int> &l) {
            BRepBuilderAPI_MakePolygon poly;
            for (int i : l) poly.Add(m.nodes[i]);
            poly.Close();
            return poly.Wire();
        };
        Mesh caps;
        for (size_t li = 0; li < loops.size(); ++li) {
            if (depth[li] % 2) continue;
            try {
                BRepBuilderAPI_MakeFace mk(plane, wireOf(loops[li]));
                for (size_t h = 0; h < loops.size(); ++h)
                    if (parent[h] == int(li) && depth[h] % 2) mk.Add(wireOf(loops[h]));
                if (!mk.IsDone()) continue;
                ShapeFix_Face fix(mk.Face());
                fix.FixOrientation();
                fix.Perform();
                const TopoDS_Shape face = fix.Face();
                BRepMesh_IncrementalMesh(face, std::max(1e-3, diag(m) * 1e-3), Standard_False, 0.3);
                appendFaces(face, caps);
            } catch (const Standard_Failure &) {
            }
        }
        // Caps face away from the kept side, i.e. along the plane normal
        for (auto &t : caps.tris) {
            if (triNormal(caps, t).Dot(gp_Vec(n)) < 0) std::swap(t[1], t[2]);
            const int base = int(m.nodes.size());
            for (int k = 0; k < 3; ++k) m.nodes.push_back(caps.nodes[t[k]]);
            m.tris.push_back({base, base + 1, base + 2});
        }
    }
    weld(m, std::max(1e-9, diag(m) * 1e-7));
    return true;
}

TopoDS_Shape toBRep(const Mesh &m, QString &error) {
    if (m.tris.empty()) { error = "The mesh is empty"; return {}; }
    if (m.tris.size() > 300000) {
        error = QString("The mesh has %1 triangles; reduce it below 300000 first").arg(m.tris.size());
        return {};
    }
    // Merging coplanar triangles afterwards takes minutes on big meshes
    const bool merge = m.tris.size() <= 5000;
    BRep_Builder bb;
    std::vector<TopoDS_Vertex> verts(m.nodes.size());
    for (size_t i = 0; i < m.nodes.size(); ++i) bb.MakeVertex(verts[i], m.nodes[i], 1e-7);
    std::unordered_map<quint64, TopoDS_Edge> edges;
    auto edge = [&](int a, int b) {
        const quint64 k = edgeKey(a, b);
        auto it = edges.find(k);
        if (it == edges.end())
            it = edges.emplace(k, BRepBuilderAPI_MakeEdge(verts[std::min(a, b)], verts[std::max(a, b)]).Edge()).first;
        return a < b ? it->second : TopoDS::Edge(it->second.Reversed());
    };
    TopoDS_Shell shell;
    bb.MakeShell(shell);
    int faces = 0;
    for (const auto &t : m.tris) {
        const gp_Vec nv = triNormal(m, t);
        if (nv.Magnitude() < 1e-14) continue;
        TopoDS_Wire w;
        bb.MakeWire(w);
        bb.Add(w, edge(t[0], t[1]));
        bb.Add(w, edge(t[1], t[2]));
        bb.Add(w, edge(t[2], t[0]));
        BRepBuilderAPI_MakeFace mk(gp_Pln(m.nodes[t[0]], gp_Dir(nv)), w, Standard_True);
        if (!mk.IsDone()) continue;
        Handle(Poly_Triangulation) pt = new Poly_Triangulation(3, 1, Standard_False);
        for (int k = 0; k < 3; ++k) pt->SetNode(k + 1, m.nodes[t[k]]);
        pt->SetTriangle(1, Poly_Triangle(1, 2, 3));
        pt->Deflection(0.0);
        const TopoDS_Face face = mk.Face();
        bb.UpdateFace(face, pt);
        bb.Add(shell, face);
        ++faces;
    }
    if (!faces) { error = "No valid triangles"; return {}; }
    if (!isClosed(m)) {
        shell.Closed(Standard_False);
        return merge ? GeomUtils::unify(shell) : TopoDS_Shape(shell);
    }
    shell.Closed(Standard_True);
    TopoDS_Solid solid;
    bb.MakeSolid(solid);
    bb.Add(solid, shell);
    if (volume(m) < 0) solid.Reverse();
    return merge ? GeomUtils::unify(solid) : TopoDS_Shape(solid);
}

QByteArray encode(const Mesh &m) {
    QByteArray raw;
    QDataStream s(&raw, QIODevice::WriteOnly);
    s.setFloatingPointPrecision(QDataStream::SinglePrecision);
    s << quint32(0x464D5348) << quint32(m.nodes.size()) << quint32(m.tris.size());
    for (const gp_Pnt &p : m.nodes) s << float(p.X()) << float(p.Y()) << float(p.Z());
    for (const auto &t : m.tris) s << qint32(t[0]) << qint32(t[1]) << qint32(t[2]);
    return qCompress(raw, 6).toBase64();
}

bool decode(const QByteArray &data, Mesh &out) {
    const QByteArray raw = qUncompress(QByteArray::fromBase64(data));
    if (raw.isEmpty()) return false;
    QDataStream s(raw);
    s.setFloatingPointPrecision(QDataStream::SinglePrecision);
    quint32 magic = 0, nn = 0, nt = 0;
    s >> magic >> nn >> nt;
    if (magic != 0x464D5348) return false;
    out = Mesh();
    out.nodes.reserve(nn);
    out.tris.reserve(nt);
    for (quint32 i = 0; i < nn; ++i) {
        float x, y, z;
        s >> x >> y >> z;
        out.nodes.emplace_back(x, y, z);
    }
    for (quint32 i = 0; i < nt; ++i) {
        qint32 a, b, c;
        s >> a >> b >> c;
        if (a < 0 || b < 0 || c < 0 || quint32(a) >= nn || quint32(b) >= nn || quint32(c) >= nn) return false;
        out.tris.push_back({a, b, c});
    }
    return s.status() == QDataStream::Ok;
}

// --- statistics ---------------------------------------------------------------------------------------

Stats stats(const Mesh &m) {
    Stats st;
    st.nodes = int(m.nodes.size());
    st.triangles = int(m.tris.size());
    if (m.hasGroups()) {
        std::unordered_set<int> g(m.groups.begin(), m.groups.end());
        st.groups = int(g.size());
    } else {
        st.groups = m.tris.empty() ? 0 : 1;
    }
    st.area = area(m);
    st.volume = volume(m);
    std::unordered_map<quint64, int> undirected, directed;
    st.minEdge = 1e300;
    for (const auto &t : m.tris)
        for (int k = 0; k < 3; ++k) {
            const int a = t[k], b = t[(k + 1) % 3];
            ++undirected[edgeKey(a, b)];
            ++directed[(quint64(quint32(a)) << 32) | quint32(b)];
            const double len = m.nodes[a].Distance(m.nodes[b]);
            st.minEdge = std::min(st.minEdge, len);
            st.maxEdge = std::max(st.maxEdge, len);
        }
    if (m.tris.empty()) st.minEdge = 0;
    for (const auto &kv : undirected) {
        if (kv.second == 1) ++st.openEdges;
        else if (kv.second > 2) ++st.nonManifoldEdges;
    }
    for (const auto &kv : directed)
        if (kv.second > 1) { st.oriented = false; break; }
    st.closed = !m.tris.empty() && st.openEdges == 0 && st.nonManifoldEdges == 0;
    // Connected pieces (union-find over nodes)
    std::vector<int> parent(m.nodes.size());
    for (size_t i = 0; i < parent.size(); ++i) parent[i] = int(i);
    std::function<int(int)> find = [&](int x) {
        while (parent[x] != x) { parent[x] = parent[parent[x]]; x = parent[x]; }
        return x;
    };
    for (const auto &t : m.tris) {
        const int r = find(t[0]);
        parent[find(t[1])] = r;
        parent[find(t[2])] = r;
    }
    std::unordered_set<int> roots;
    for (const auto &t : m.tris) roots.insert(find(t[0]));
    st.pieces = int(roots.size());
    return st;
}

// --- remeshing ----------------------------------------------------------------------------------------

namespace {

// Closest point to p on triangle abc (Ericson, Real-Time Collision Detection 5.1.5)
gp_XYZ closestOnTriangle(const gp_XYZ &p, const gp_XYZ &a, const gp_XYZ &b, const gp_XYZ &c) {
    const gp_XYZ ab = b - a, ac = c - a, ap = p - a;
    const double d1 = ab.Dot(ap), d2 = ac.Dot(ap);
    if (d1 <= 0 && d2 <= 0) return a;
    const gp_XYZ bp = p - b;
    const double d3 = ab.Dot(bp), d4 = ac.Dot(bp);
    if (d3 >= 0 && d4 <= d3) return b;
    const double vc = d1 * d4 - d3 * d2;
    if (vc <= 0 && d1 >= 0 && d3 <= 0) return a + ab * (d1 / (d1 - d3));
    const gp_XYZ cp = p - c;
    const double d5 = ab.Dot(cp), d6 = ac.Dot(cp);
    if (d6 >= 0 && d5 <= d6) return c;
    const double vb = d5 * d2 - d1 * d6;
    if (vb <= 0 && d2 >= 0 && d6 <= 0) return a + ac * (d2 / (d2 - d6));
    const double va = d3 * d6 - d5 * d4;
    if (va <= 0 && (d4 - d3) >= 0 && (d5 - d6) >= 0) return b + (c - b) * ((d4 - d3) / ((d4 - d3) + (d5 - d6)));
    const double denom = 1.0 / (va + vb + vc);
    return a + ab * (vb * denom) + ac * (vc * denom);
}

// The original surface, for pulling relaxed nodes back onto it
class SurfaceGrid {
public:
    SurfaceGrid(const Mesh &m, double cell) : m(m), cell(cell) {
        for (int t = 0; t < int(m.tris.size()); ++t) {
            gp_XYZ lo = m.nodes[m.tris[t][0]].XYZ(), hi = lo;
            for (int v : m.tris[t]) {
                const gp_XYZ p = m.nodes[v].XYZ();
                lo.SetCoord(std::min(lo.X(), p.X()), std::min(lo.Y(), p.Y()), std::min(lo.Z(), p.Z()));
                hi.SetCoord(std::max(hi.X(), p.X()), std::max(hi.Y(), p.Y()), std::max(hi.Z(), p.Z()));
            }
            const auto a = index(lo), b = index(hi);
            for (qint64 x = a[0]; x <= b[0]; ++x)
                for (qint64 y = a[1]; y <= b[1]; ++y)
                    for (qint64 z = a[2]; z <= b[2]; ++z) cells[key(x, y, z)].push_back(t);
        }
    }
    bool project(gp_XYZ &p) const {
        const auto c = index(p);
        double best = 1e300;
        gp_XYZ out;
        for (qint64 x = c[0] - 1; x <= c[0] + 1; ++x)
            for (qint64 y = c[1] - 1; y <= c[1] + 1; ++y)
                for (qint64 z = c[2] - 1; z <= c[2] + 1; ++z) {
                    auto it = cells.find(key(x, y, z));
                    if (it == cells.end()) continue;
                    for (int t : it->second) {
                        const auto &tr = m.tris[t];
                        const gp_XYZ q = closestOnTriangle(p, m.nodes[tr[0]].XYZ(), m.nodes[tr[1]].XYZ(), m.nodes[tr[2]].XYZ());
                        const double d = (q - p).SquareModulus();
                        if (d < best) { best = d; out = q; }
                    }
                }
        if (best == 1e300) return false;
        p = out;
        return true;
    }

private:
    const Mesh &m;
    double cell;
    std::unordered_map<quint64, std::vector<int>> cells;
    std::array<qint64, 3> index(const gp_XYZ &p) const {
        return {qint64(std::floor(p.X() / cell)), qint64(std::floor(p.Y() / cell)), qint64(std::floor(p.Z() / cell))};
    }
    static quint64 key(qint64 x, qint64 y, qint64 z) {
        return (quint64(x & 0x1FFFFF) << 42) | (quint64(y & 0x1FFFFF) << 21) | quint64(z & 0x1FFFFF);
    }
};

class Remesher {
public:
    Remesher(const Mesh &orig, double target, bool adaptive, double sharpDeg)
        : grid(orig, std::max(target * 2, diag(orig) * 1e-3)), target(target), adaptive(adaptive),
          cosSharp(std::cos(sharpDeg * 3.14159265358979323846 / 180)) {
        for (const gp_Pnt &p : orig.nodes) P.push_back(p.XYZ());
        T = orig.tris;
        alive.assign(T.size(), 1);
        fixed.assign(P.size(), 0);
        markFeatures();
    }
    void run(int iterations) {
        for (int it = 0; it < iterations; ++it) {
            updateTargets();
            for (int k = 0; k < 6 && split(); ++k) {}
            collapse();
            for (int k = 0; k < 3 && flip(); ++k) {}
            relax();
        }
    }
    Mesh result() const {
        Mesh out;
        std::vector<int> map(P.size(), -1);
        for (size_t t = 0; t < T.size(); ++t) {
            if (!alive[t]) continue;
            std::array<int, 3> tr;
            for (int k = 0; k < 3; ++k) {
                int &v = map[T[t][k]];
                if (v < 0) { v = int(out.nodes.size()); out.nodes.emplace_back(P[T[t][k]]); }
                tr[k] = v;
            }
            out.tris.push_back(tr);
        }
        return out;
    }

private:
    SurfaceGrid grid;
    double target;
    bool adaptive;
    double cosSharp;
    std::vector<gp_XYZ> P;
    std::vector<std::array<int, 3>> T;
    std::vector<char> alive, fixed;
    std::vector<double> L;                  // target edge length per node
    std::unordered_set<quint64> feature;    // sharp / open / non-manifold edges

    gp_XYZ triNormal(int t) const {
        const gp_XYZ n = (P[T[t][1]] - P[T[t][0]]).Crossed(P[T[t][2]] - P[T[t][0]]);
        const double l = n.Modulus();
        return l > 0 ? n / l : gp_XYZ(0, 0, 0);
    }
    // Undirected edge -> adjacent live triangles (more than two: non-manifold)
    std::unordered_map<quint64, std::vector<int>> edgeTris() const {
        std::unordered_map<quint64, std::vector<int>> e;
        e.reserve(T.size() * 2);
        for (int t = 0; t < int(T.size()); ++t)
            if (alive[t])
                for (int k = 0; k < 3; ++k) e[edgeKey(T[t][k], T[t][(k + 1) % 3])].push_back(t);
        return e;
    }
    std::vector<std::vector<int>> nodeTris() const {
        std::vector<std::vector<int>> v(P.size());
        for (int t = 0; t < int(T.size()); ++t)
            if (alive[t])
                for (int k = 0; k < 3; ++k) v[T[t][k]].push_back(t);
        return v;
    }
    void markFeatures() {
        for (const auto &kv : edgeTris()) {
            bool sharp = kv.second.size() != 2;
            if (!sharp) sharp = triNormal(kv.second[0]).Dot(triNormal(kv.second[1])) < cosSharp;
            if (!sharp) continue;
            feature.insert(kv.first);
            fixed[int(kv.first >> 32)] = 1;
            fixed[int(kv.first & 0xFFFFFFFF)] = 1;
        }
    }
    void updateTargets() {
        L.assign(P.size(), target);
        if (!adaptive) return;
        // Curvature from the turn of the normals across each edge: about 20 degrees per edge where it bends
        for (const auto &kv : edgeTris()) {
            if (kv.second.size() != 2) continue;
            const int a = int(kv.first >> 32), b = int(kv.first & 0xFFFFFFFF);
            const double turn = std::acos(std::clamp(triNormal(kv.second[0]).Dot(triNormal(kv.second[1])), -1.0, 1.0));
            const double len = (P[a] - P[b]).Modulus();
            if (turn < 1e-6 || len <= 0) continue;
            const double want = std::clamp(0.35 * len / turn, target * 0.25, target);
            L[a] = std::min(L[a], want);
            L[b] = std::min(L[b], want);
        }
    }
    double edgeTarget(int a, int b) const { return std::min(L[a], L[b]); }

    bool split() {
        const auto edges = edgeTris();
        std::vector<std::pair<double, quint64>> longOnes;
        for (const auto &kv : edges) {
            const int a = int(kv.first >> 32), b = int(kv.first & 0xFFFFFFFF);
            const double len = (P[a] - P[b]).Modulus();
            if (len > edgeTarget(a, b) * 4 / 3 && kv.second.size() <= 2) longOnes.push_back({len, kv.first});
        }
        if (longOnes.empty()) return false;
        std::sort(longOnes.begin(), longOnes.end(), [](auto &x, auto &y) { return x.first > y.first; });
        std::vector<char> touched(T.size(), 0);
        bool any = false;
        for (const auto &le : longOnes) {
            const auto &tris = edges.at(le.second);
            if (std::any_of(tris.begin(), tris.end(), [&](int t) { return t >= int(touched.size()) || touched[t]; })) continue;
            const int a = int(le.second >> 32), b = int(le.second & 0xFFFFFFFF);
            const int mid = int(P.size());
            P.push_back((P[a] + P[b]) / 2);
            const bool feat = feature.count(le.second) > 0;
            fixed.push_back(feat);
            L.push_back((L[a] + L[b]) / 2);
            if (feat) {
                feature.erase(le.second);
                feature.insert(edgeKey(a, mid));
                feature.insert(edgeKey(mid, b));
            }
            for (int t : tris) {
                // Rotate so the split edge runs x -> y, opposite node z
                auto tr = T[t];
                while (!((tr[0] == a && tr[1] == b) || (tr[0] == b && tr[1] == a))) std::rotate(tr.begin(), tr.begin() + 1, tr.end());
                const int x = tr[0], y = tr[1], z = tr[2];
                T[t] = {x, mid, z};
                T.push_back({mid, y, z});
                alive.push_back(1);
                touched[t] = 1;
            }
            any = true;
        }
        return any;
    }

    void collapse() {
        const auto edges = edgeTris();
        auto around = nodeTris();
        std::vector<std::pair<double, quint64>> shortOnes;
        for (const auto &kv : edges) {
            const int a = int(kv.first >> 32), b = int(kv.first & 0xFFFFFFFF);
            if (kv.second.size() != 2 || fixed[a] || fixed[b]) continue;
            const double len = (P[a] - P[b]).Modulus();
            if (len < edgeTarget(a, b) * 4 / 5) shortOnes.push_back({len, kv.first});
        }
        std::sort(shortOnes.begin(), shortOnes.end());
        std::vector<char> touched(T.size(), 0);
        for (const auto &se : shortOnes) {
            const int a = int(se.second >> 32), b = int(se.second & 0xFFFFFFFF);
            bool busy = false;
            for (int v : {a, b})
                for (int t : around[v]) busy = busy || touched[t] || !alive[t];
            if (busy) continue;
            // Link condition: a and b share exactly the two opposite nodes
            std::unordered_set<int> na, common;
            for (int t : around[a])
                for (int v : T[t]) na.insert(v);
            for (int t : around[b])
                for (int v : T[t])
                    if (v != a && v != b && na.count(v)) common.insert(v);
            if (common.size() != 2) continue;
            const gp_XYZ p = (P[a] + P[b]) / 2;
            const double lim = edgeTarget(a, b) * 4 / 3;
            bool ok = true;
            for (int v : {a, b})
                for (int t : around[v]) {
                    const auto &tr = T[t];
                    const bool hasA = tr[0] == a || tr[1] == a || tr[2] == a, hasB = tr[0] == b || tr[1] == b || tr[2] == b;
                    if (hasA && hasB) continue;
                    std::array<gp_XYZ, 3> q;
                    for (int k = 0; k < 3; ++k) {
                        q[k] = (tr[k] == a || tr[k] == b) ? p : P[tr[k]];
                        if (tr[k] != a && tr[k] != b && (P[tr[k]] - p).Modulus() > lim) ok = false;
                    }
                    const gp_XYZ n = (q[1] - q[0]).Crossed(q[2] - q[0]);
                    if (n.Modulus() <= 0 || n.Dot(triNormal(t)) / n.Modulus() < 0.5) ok = false;
                }
            if (!ok) continue;
            P[a] = p;
            for (int t : around[b]) {
                touched[t] = 1;
                auto &tr = T[t];
                const bool hasA = tr[0] == a || tr[1] == a || tr[2] == a;
                if (hasA) { alive[t] = 0; continue; }
                for (int &v : tr)
                    if (v == b) v = a;
                around[a].push_back(t);
            }
            for (int t : around[a]) touched[t] = 1;
            around[b].clear();
        }
    }

    bool flip() {
        const auto edges = edgeTris();
        auto around = nodeTris();
        std::vector<int> valence(P.size(), 0);
        std::vector<char> boundary(P.size(), 0);
        for (const auto &kv : edges) {
            const int a = int(kv.first >> 32), b = int(kv.first & 0xFFFFFFFF);
            ++valence[a];
            ++valence[b];
            if (kv.second.size() != 2) boundary[a] = boundary[b] = 1;
        }
        auto want = [&](int v) { return boundary[v] ? 4 : 6; };
        std::vector<char> touched(T.size(), 0);
        bool any = false;
        for (const auto &kv : edges) {
            if (kv.second.size() != 2 || feature.count(kv.first)) continue;
            const int t1 = kv.second[0], t2 = kv.second[1];
            if (touched[t1] || touched[t2]) continue;
            const int a = int(kv.first >> 32), b = int(kv.first & 0xFFFFFFFF);
            // t1 runs a -> b (else swap), t2 runs b -> a
            auto rot = [&](std::array<int, 3> tr, int x, int y) {
                for (int k = 0; k < 3; ++k)
                    if (tr[0] == x && tr[1] == y) return tr;
                    else std::rotate(tr.begin(), tr.begin() + 1, tr.end());
                return std::array<int, 3>{-1, -1, -1};
            };
            std::array<int, 3> r1 = rot(T[t1], a, b), r2 = rot(T[t2], b, a);
            int u1 = t1, u2 = t2;
            if (r1[0] < 0) { r1 = rot(T[t2], a, b); r2 = rot(T[t1], b, a); u1 = t2; u2 = t1; }
            if (r1[0] < 0 || r2[0] < 0) continue;
            const int c = r1[2], d = r2[2];
            if (c == d) continue;
            const int before = std::abs(valence[a] - want(a)) + std::abs(valence[b] - want(b)) + std::abs(valence[c] - want(c)) +
                               std::abs(valence[d] - want(d));
            const int after = std::abs(valence[a] - 1 - want(a)) + std::abs(valence[b] - 1 - want(b)) +
                              std::abs(valence[c] + 1 - want(c)) + std::abs(valence[d] + 1 - want(d));
            if (after >= before) continue;
            if (edges.count(edgeKey(c, d))) continue;
            const gp_XYZ n1 = triNormal(u1), n2 = triNormal(u2);
            if (n1.Dot(n2) < 0.94) continue;   // only nearly flat quads
            const std::array<int, 3> f1{a, d, c}, f2{d, b, c};
            auto nrmOf = [&](const std::array<int, 3> &tr) {
                const gp_XYZ n = (P[tr[1]] - P[tr[0]]).Crossed(P[tr[2]] - P[tr[0]]);
                const double l = n.Modulus();
                return l > 0 ? n / l : gp_XYZ(0, 0, 0);
            };
            const gp_XYZ avg = n1 + n2;
            if (nrmOf(f1).Dot(avg) <= 0.5 || nrmOf(f2).Dot(avg) <= 0.5) continue;
            T[u1] = f1;
            T[u2] = f2;
            touched[u1] = touched[u2] = 1;
            --valence[a]; --valence[b]; ++valence[c]; ++valence[d];
            any = true;
        }
        return any;
    }

    void relax() {
        const auto around = nodeTris();
        std::vector<gp_XYZ> next = P;
        for (size_t v = 0; v < P.size(); ++v) {
            if (fixed[v] || around[v].empty()) continue;
            gp_XYZ n(0, 0, 0), c(0, 0, 0);
            double w = 0;
            for (int t : around[v]) {
                const gp_XYZ fn = (P[T[t][1]] - P[T[t][0]]).Crossed(P[T[t][2]] - P[T[t][0]]);
                n += fn;
                const double a = fn.Modulus();
                c += (P[T[t][0]] + P[T[t][1]] + P[T[t][2]]) * (a / 3);
                w += a;
            }
            if (w <= 0 || n.Modulus() <= 0) continue;
            c /= w;
            n /= n.Modulus();
            const gp_XYZ d = c - P[v];
            gp_XYZ p = P[v] + d - n * d.Dot(n);
            grid.project(p);
            next[v] = p;
        }
        P = std::move(next);
    }
};

}   // namespace

void remesh(Mesh &m, double target, bool adaptive, int iterations, double sharpDeg) {
    if (m.tris.empty() || target <= 0) return;
    Remesher r(m, target, adaptive, sharpDeg);
    r.run(std::max(1, iterations));
    m = r.result();
}

}   // namespace MeshUtils
