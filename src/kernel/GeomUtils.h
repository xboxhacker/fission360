// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#pragma once
#include <QVariantMap>
#include <QList>
#include <TopoDS_Shape.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <gp_Pnt.hxx>
#include <gp_Ax3.hxx>
#include <Bnd_Box.hxx>
#include <functional>

namespace GeomUtils {

gp_Pnt edgeMidpoint(const TopoDS_Edge &edge);
gp_Pnt faceCentroid(const TopoDS_Face &face);
// Centroid of all faces of a shape (e.g. a set of sketch profiles)
gp_Pnt shapeCentroid(const TopoDS_Shape &shape);
// Outward bisector of the faces of `shape` meeting at `edge`, at the edge midpoint
gp_Dir edgeOutwardBisector(const TopoDS_Shape &shape, const TopoDS_Edge &edge);

TopTools_IndexedMapOfShape edgeMap(const TopoDS_Shape &shape);
TopTools_IndexedMapOfShape faceMap(const TopoDS_Shape &shape);

// Reference maps understood by RebuildContext::resolveEdge / resolveFace
QVariantMap edgeRef(int body, const TopoDS_Shape &bodyShape, int index);   // index is 0-based
QVariantMap faceRef(int body, const TopoDS_Shape &bodyShape, int index);

// 0-based indices (edgeMap order) of the edges joined to edge `index` with tangent (G1) continuity,
// including it — what Fusion calls a tangent chain
QList<int> tangentChain(const TopoDS_Shape &shape, int index, double angleTolDeg = 1.0);

Bnd_Box boundingBox(const TopoDS_Shape &shape);
double volume(const TopoDS_Shape &shape);
double area(const TopoDS_Shape &shape);
// Volume, its centre and the surface area of a B-rep in one pass; shapes with many faces (converted meshes)
// take their straight-edged planar faces from the triangles those faces carry
struct MassProperties {
    double volume = 0, area = 0;
    gp_Pnt centroid;
};
MassProperties massProperties(const TopoDS_Shape &shape);

// Merges coplanar faces / collinear edges left behind by booleans
TopoDS_Shape unify(const TopoDS_Shape &shape);
QList<TopoDS_Shape> solids(const TopoDS_Shape &shape);
TopoDS_Shape makeCompound(const QList<TopoDS_Shape> &shapes);

gp_Ax3 standardPlane(const QString &name, bool *ok = nullptr);

// A copy of `shape` (geometry too), with `faces` of it found again on the copy as they sit in it, orientation
// included: BRepBuilderAPI_Copy::Modified gives the face without the orientation it has in the solid, so its
// normal can point inwards
TopoDS_Shape copyWithFaces(const TopoDS_Shape &shape, const QList<TopoDS_Face> &faces, QList<TopoDS_Face> &onCopy);

// Runs `work` on a worker thread and waits at most `ms` (pair it with TimeLimit from TimeLimit.h). False on a
// timeout: the worker then finishes on its own in the background, so `work` may only touch what it owns
// (copies of shapes, shared_ptr results).
bool runWithTimeLimit(qint64 ms, std::function<void()> work);

}
