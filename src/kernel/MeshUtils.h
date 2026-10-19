// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#pragma once
#include <QByteArray>
#include <QString>
#include <array>
#include <functional>
#include <vector>
#include <TopoDS_Face.hxx>
#include <TopoDS_Shape.hxx>
#include <gp_Pln.hxx>
#include <gp_Pnt.hxx>

// Mesh bodies: a body whose shape is a TopoDS_Face carrying only a Poly_Triangulation (no surface, no
// edges), or a compound of such faces, one per face group. OCCT modelling algorithms do not accept them;
// mesh tools work on the triangles directly.
namespace MeshUtils {

struct Mesh {
    std::vector<gp_Pnt> nodes;
    std::vector<std::array<int, 3>> tris;   // 0-based node indices, counter-clockwise seen from outside
    std::vector<int> groups;                // face group of each triangle; empty (or stale) = no groups
    bool empty() const { return tris.empty(); }
    bool hasGroups() const { return !groups.empty() && groups.size() == tris.size(); }
};

// Progress of the slow operations below: how far along (0..1) and what is being done. Called from the
// thread running the operation; may be empty.
using Progress = std::function<void(double fraction, const QString &what)>;

bool isMesh(const TopoDS_Shape &shape);
// Number of face groups of a mesh body (1 when it has none)
int groupCount(const TopoDS_Shape &shape);
// Triangles of a mesh body (with its face groups), or of any B-rep shape tessellated with the given deflection
Mesh fromShape(const TopoDS_Shape &shape, double deflection = 0.05);
TopoDS_Face toFace(const Mesh &m);
// The mesh body for m: one face, or a compound with one face per face group
TopoDS_Shape toShape(const Mesh &m);

// STL (ASCII or binary) and OBJ; coordinates are multiplied by `scale`
bool readFile(const QString &path, double scale, Mesh &out, QString &error);
// Welds nodes closer than tol and drops degenerate triangles
void weld(Mesh &m, double tol);

double volume(const Mesh &m);
double area(const Mesh &m);
// Centre of the enclosed volume (closed meshes) or of the area (open ones)
gp_Pnt centroid(const Mesh &m);
bool isClosed(const Mesh &m);

void reverse(Mesh &m);
// Laplacian smoothing (boundary nodes stay put); factor 0..1 per iteration
void smooth(Mesh &m, int iterations, double factor);
// Vertex clustering until about `ratio` (0..1) of the triangles remain
void reduce(Mesh &m, double ratio);
struct RepairStats {
    int welded = 0, duplicates = 0, flipped = 0, holes = 0;
    bool closed = false;
};
// Welds nodes within tol, drops repeated triangles, orients every connected piece consistently (outward
// when closed) and, with fillHoles, closes each boundary loop with a triangle fan
RepairStats repair(Mesh &m, double tol, bool orient, bool fillHoles);
void append(Mesh &m, const Mesh &other);
// Hollows a closed mesh (or thickens an open one, adding side walls) by `thickness` along the vertex
// normals: inward keeps the outer surface, outward keeps the inner one
bool shell(Mesh &m, double thickness, bool outward, QString &error);
// Keeps the part of the mesh behind the plane (opposite its normal); fill closes the cut with caps
bool planeCut(Mesh &m, const gp_Pln &plane, bool fill, QString &error);
// Sews the triangles into a solid (or a shell when the mesh is open)
TopoDS_Shape toBRep(const Mesh &m, QString &error);
// Closed meshes only (MeshFit.cpp). Faceted: coplanar triangles merge into planar faces. Prismatic: smooth
// patches are also fitted with cylinders, cones and spheres bounded by line / circle edges, and smooth
// patches no such surface fits become B-spline faces. Returns a null shape with error set when the result
// would not be a valid solid. summary: "12 faces (6 planes, ...)".
// With face groups, prismatic fitting takes each group as one smooth patch instead of splitting at sharp edges.
TopoDS_Shape toFittedBRep(const Mesh &m, bool prismatic, QString &error, QString *summary = nullptr,
                          const Progress &progress = {});

// Face groups (MeshFit.cpp). "fast": neighbours bending less than angleDeg share a group; groups under
// minAreaFraction of the total area join their longest-bordering neighbour. "accurate": match triangles to
// planes / cylinders / cones / spheres within boundaryTolerance — Fusion Accurate: a fraction of the
// mesh diagonal (0..0.01, default 0.001), not millimetres; values > 0.01 are legacy absolute lengths.
// Fusion Accurate has no angle or minimum size; curved leftovers become thin bands across the bend.
// Sets m.groups and returns the number of groups (0 with error set on failure).
int generateFaceGroups(Mesh &m, bool accurate, double angleDeg, double minAreaFraction, QString &error,
                       double boundaryTolerance = 0, const Progress &progress = {});

// Remeshing (MeshUtils.cpp): splits edges longer than 4/3 target, collapses those shorter than 4/5 target,
// flips edges toward valence 6 and relaxes the nodes tangentially, `iterations` times. Sharp edges (over
// sharpDeg) and open boundaries stay in place. Adaptive scales the target down where the surface bends.
void remesh(Mesh &m, double target, bool adaptive, int iterations, double sharpDeg);

struct Stats {
    int nodes = 0, triangles = 0, groups = 0, openEdges = 0, nonManifoldEdges = 0, pieces = 0;
    bool closed = false, oriented = true;
    double area = 0, volume = 0, minEdge = 0, maxEdge = 0;
};
Stats stats(const Mesh &m);

QByteArray encode(const Mesh &m);
bool decode(const QByteArray &data, Mesh &out);

}
