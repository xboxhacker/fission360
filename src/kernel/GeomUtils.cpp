// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#include "GeomUtils.h"
#include "MeshUtils.h"
#include <BRepAdaptor_Curve.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepBndLib.hxx>
#include <BRepBuilderAPI_Copy.hxx>
#include <BRepLProp_SLProps.hxx>
#include <BRep_Tool.hxx>
#include <Geom2d_Curve.hxx>
#include <TopTools_IndexedDataMapOfShapeListOfShape.hxx>
#include <TopTools_ListIteratorOfListOfShape.hxx>
#include <BRepGProp.hxx>
#include <BRep_Builder.hxx>
#include <GProp_GProps.hxx>
#include <OSD.hxx>
#include <ShapeUpgrade_UnifySameDomain.hxx>
#include <Standard_Failure.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Compound.hxx>
#include <Poly_Triangulation.hxx>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>

namespace GeomUtils {

constexpr int kManyFaces = 500;

gp_Pnt edgeMidpoint(const TopoDS_Edge &edge) {
    try {
        BRepAdaptor_Curve c(edge);
        return c.Value(0.5 * (c.FirstParameter() + c.LastParameter()));
    } catch (const Standard_Failure &) {
        return gp_Pnt();
    }
}

gp_Pnt faceCentroid(const TopoDS_Face &face) {
    if (MeshUtils::isMesh(face)) {
        // Area-weighted centre of the triangles as stored; welding them first costs far more than it changes
        TopLoc_Location loc;
        const Handle(Poly_Triangulation) tri = BRep_Tool::Triangulation(face, loc);
        gp_XYZ sum(0, 0, 0);
        double total = 0;
        if (!tri.IsNull()) {
            const gp_Trsf trsf = loc.Transformation();
            for (int i = 1; i <= tri->NbTriangles(); ++i) {
                int a, b, c;
                tri->Triangle(i).Get(a, b, c);
                const gp_XYZ pa = tri->Node(a).Transformed(trsf).XYZ(), pb = tri->Node(b).Transformed(trsf).XYZ(),
                             pc = tri->Node(c).Transformed(trsf).XYZ();
                const double ar = (pb - pa).Crossed(pc - pa).Modulus() / 2;
                sum += (pa + pb + pc) * (ar / 3);
                total += ar;
            }
        }
        return total > 0 ? gp_Pnt(sum / total) : gp_Pnt();
    }
    GProp_GProps props;
    BRepGProp::SurfaceProperties(face, props);
    return props.CentreOfMass();
}

gp_Pnt shapeCentroid(const TopoDS_Shape &shape) {
    if (MeshUtils::isMesh(shape)) return MeshUtils::centroid(MeshUtils::fromShape(shape));
    GProp_GProps props;
    BRepGProp::SurfaceProperties(shape, props);
    return props.Mass() > 1e-12 ? props.CentreOfMass() : gp_Pnt();
}

gp_Dir edgeOutwardBisector(const TopoDS_Shape &shape, const TopoDS_Edge &edge) {
    TopTools_IndexedDataMapOfShapeListOfShape edgeFaces;
    TopExp::MapShapesAndAncestors(shape, TopAbs_EDGE, TopAbs_FACE, edgeFaces);
    const TopTools_ListOfShape *faces = edgeFaces.Seek(edge);
    gp_Vec sum(0, 0, 0);
    if (faces) {
        for (TopTools_ListIteratorOfListOfShape it(*faces); it.More(); it.Next()) {
            const TopoDS_Face face = TopoDS::Face(it.Value());
            try {
                double f = 0, l = 0;
                Handle(Geom2d_Curve) pc = BRep_Tool::CurveOnSurface(edge, face, f, l);
                if (pc.IsNull()) continue;
                const gp_Pnt2d uv = pc->Value(0.5 * (f + l));
                BRepAdaptor_Surface surf(face);
                BRepLProp_SLProps props(surf, uv.X(), uv.Y(), 1, 1e-7);
                if (!props.IsNormalDefined()) continue;
                gp_Dir n = props.Normal();
                if (face.Orientation() == TopAbs_REVERSED) n.Reverse();
                sum += gp_Vec(n);
            } catch (const Standard_Failure &) {
            }
        }
    }
    return sum.Magnitude() > 1e-9 ? gp_Dir(sum) : gp_Dir(0, 0, 1);
}

TopTools_IndexedMapOfShape edgeMap(const TopoDS_Shape &shape) {
    TopTools_IndexedMapOfShape map;
    if (!shape.IsNull()) TopExp::MapShapes(shape, TopAbs_EDGE, map);
    return map;
}

TopTools_IndexedMapOfShape faceMap(const TopoDS_Shape &shape) {
    TopTools_IndexedMapOfShape map;
    if (!shape.IsNull()) TopExp::MapShapes(shape, TopAbs_FACE, map);
    return map;
}

QVariantMap edgeRef(int body, const TopoDS_Shape &bodyShape, int index) {
    TopTools_IndexedMapOfShape map = edgeMap(bodyShape);
    QVariantMap ref{{"kind", "edge"}, {"body", body}, {"index", index}, {"count", map.Extent()}};
    if (index >= 0 && index < map.Extent()) {
        gp_Pnt p = edgeMidpoint(TopoDS::Edge(map(index + 1)));
        ref["x"] = p.X(); ref["y"] = p.Y(); ref["z"] = p.Z();
    }
    return ref;
}

QList<int> tangentChain(const TopoDS_Shape &shape, int index, double angleTolDeg) {
    const TopTools_IndexedMapOfShape edges = edgeMap(shape);
    if (index < 0 || index >= edges.Extent()) return {};
    TopTools_IndexedDataMapOfShapeListOfShape vertexEdges;
    TopExp::MapShapesAndUniqueAncestors(shape, TopAbs_VERTEX, TopAbs_EDGE, vertexEdges);
    const double cosTol = std::cos(angleTolDeg * M_PI / 180.0);

    auto tangentAt = [](const TopoDS_Edge &e, const TopoDS_Vertex &v, gp_Vec &t) {
        try {
            BRepAdaptor_Curve c(e);
            gp_Pnt p;
            c.D1(BRep_Tool::Parameter(v, e), p, t);
            return t.Magnitude() > 1e-12;
        } catch (const Standard_Failure &) {
            return false;
        }
    };

    QList<int> chain{index};
    QList<int> todo{index};
    while (!todo.isEmpty()) {
        const TopoDS_Edge e = TopoDS::Edge(edges(todo.takeLast() + 1));
        TopoDS_Vertex ends[2];
        TopExp::Vertices(e, ends[0], ends[1]);
        for (const TopoDS_Vertex &v : ends) {
            if (v.IsNull() || !vertexEdges.Contains(v)) continue;
            gp_Vec t0;
            if (!tangentAt(e, v, t0)) continue;
            for (TopTools_ListIteratorOfListOfShape it(vertexEdges.FindFromKey(v)); it.More(); it.Next()) {
                const TopoDS_Edge o = TopoDS::Edge(it.Value());
                const int oi = edges.FindIndex(o) - 1;
                if (oi < 0 || chain.contains(oi) || BRep_Tool::Degenerated(o)) continue;
                gp_Vec t1;
                if (!tangentAt(o, v, t1)) continue;
                if (std::fabs(t0.Normalized().Dot(t1.Normalized())) < cosTol) continue;
                chain.append(oi);
                todo.append(oi);
            }
        }
    }
    return chain;
}

QVariantMap faceRef(int body, const TopoDS_Shape &bodyShape, int index) {
    TopTools_IndexedMapOfShape map = faceMap(bodyShape);
    QVariantMap ref{{"kind", "face"}, {"body", body}, {"index", index}, {"count", map.Extent()}};
    if (index >= 0 && index < map.Extent()) {
        gp_Pnt p = faceCentroid(TopoDS::Face(map(index + 1)));
        ref["x"] = p.X(); ref["y"] = p.Y(); ref["z"] = p.Z();
    }
    return ref;
}

Bnd_Box boundingBox(const TopoDS_Shape &shape) {
    Bnd_Box box;
    if (!shape.IsNull()) BRepBndLib::Add(shape, box);
    return box;
}

double volume(const TopoDS_Shape &shape) {
    if (shape.IsNull()) return 0.0;
    if (MeshUtils::isMesh(shape)) {
        const MeshUtils::Mesh m = MeshUtils::fromShape(shape);
        return MeshUtils::isClosed(m) ? std::fabs(MeshUtils::volume(m)) : 0.0;
    }
    if (faceMap(shape).Extent() > kManyFaces) return massProperties(shape).volume;
    GProp_GProps props;
    BRepGProp::VolumeProperties(shape, props);
    return props.Mass();
}

double area(const TopoDS_Shape &shape) {
    if (shape.IsNull()) return 0.0;
    if (MeshUtils::isMesh(shape)) return MeshUtils::area(MeshUtils::fromShape(shape));
    if (faceMap(shape).Extent() > kManyFaces) return massProperties(shape).area;
    GProp_GProps props;
    BRepGProp::SurfaceProperties(shape, props);
    return props.Mass();
}

// A planar face bounded by straight edges is exactly the triangles it carries
static bool polygonalFace(const TopoDS_Face &face) {
    TopLoc_Location loc;
    if (BRep_Tool::Triangulation(face, loc).IsNull()) return false;
    if (BRepAdaptor_Surface(face, Standard_False).GetType() != GeomAbs_Plane) return false;
    for (TopExp_Explorer e(face, TopAbs_EDGE); e.More(); e.Next())
        if (BRepAdaptor_Curve(TopoDS::Edge(e.Current())).GetType() != GeomAbs_Line) return false;
    return true;
}

MassProperties massProperties(const TopoDS_Shape &shape) {
    MassProperties out;
    if (shape.IsNull()) return out;
    if (MeshUtils::isMesh(shape)) {
        const MeshUtils::Mesh m = MeshUtils::fromShape(shape);
        out.volume = MeshUtils::isClosed(m) ? std::fabs(MeshUtils::volume(m)) : 0.0;
        out.area = MeshUtils::area(m);
        out.centroid = MeshUtils::centroid(m);
        return out;
    }
    BRep_Builder bb;
    TopoDS_Compound rest;
    bb.MakeCompound(rest);
    bool anyRest = false;
    double vol = 0, area = 0;
    gp_XYZ moment(0, 0, 0);
    if (faceMap(shape).Extent() > kManyFaces) {
        for (TopExp_Explorer e(shape, TopAbs_FACE); e.More(); e.Next()) {
            const TopoDS_Face face = TopoDS::Face(e.Current());
            if (!polygonalFace(face)) {
                bb.Add(rest, face);
                anyRest = true;
                continue;
            }
            TopLoc_Location loc;
            const Handle(Poly_Triangulation) tri = BRep_Tool::Triangulation(face, loc);
            const gp_Trsf trsf = loc.Transformation();
            const bool reversed = face.Orientation() == TopAbs_REVERSED;
            for (int i = 1; i <= tri->NbTriangles(); ++i) {
                int a, b, c;
                tri->Triangle(i).Get(a, b, c);
                if (reversed) std::swap(b, c);
                const gp_XYZ pa = tri->Node(a).Transformed(trsf).XYZ(), pb = tri->Node(b).Transformed(trsf).XYZ(),
                             pc = tri->Node(c).Transformed(trsf).XYZ();
                const double v = pa.Dot(pb.Crossed(pc)) / 6.0;
                vol += v;
                moment += (pa + pb + pc) * (v / 4.0);
                area += (pb - pa).Crossed(pc - pa).Modulus() / 2.0;
            }
        }
    } else {
        bb.Add(rest, shape);
        anyRest = true;
    }
    if (anyRest) {
        GProp_GProps vp, sp;
        BRepGProp::VolumeProperties(rest, vp);
        BRepGProp::SurfaceProperties(rest, sp);
        vol += vp.Mass();
        moment += vp.CentreOfMass().XYZ() * vp.Mass();
        area += sp.Mass();
    }
    out.volume = std::fabs(vol);
    out.area = area;
    out.centroid = std::fabs(vol) > 1e-12 ? gp_Pnt(moment / vol) : gp_Pnt();
    return out;
}

TopoDS_Shape unify(const TopoDS_Shape &shape) {
    if (shape.IsNull()) return shape;
    // Merging same-domain faces on a shape with free-form faces (thread helices on imported fasteners) can
    // run for minutes even when those faces are kept; the merge is cosmetic, so such shapes keep their seams
    for (TopExp_Explorer e(shape, TopAbs_FACE); e.More(); e.Next())
        if (BRepAdaptor_Surface(TopoDS::Face(e.Current()), Standard_False).GetType() > GeomAbs_Torus) return shape;
    try {
        ShapeUpgrade_UnifySameDomain unifier(shape, Standard_True, Standard_True, Standard_False);
        unifier.Build();
        return unifier.Shape();
    } catch (const Standard_Failure &) {
        return shape;
    }
}

QList<TopoDS_Shape> solids(const TopoDS_Shape &shape) {
    QList<TopoDS_Shape> out;
    for (TopExp_Explorer exp(shape, TopAbs_SOLID); exp.More(); exp.Next()) out.append(exp.Current());
    return out;
}

TopoDS_Shape makeCompound(const QList<TopoDS_Shape> &shapes) {
    if (shapes.size() == 1) return shapes.first();
    TopoDS_Compound compound;
    BRep_Builder builder;
    builder.MakeCompound(compound);
    for (const auto &s : shapes) builder.Add(compound, s);
    return compound;
}

gp_Ax3 standardPlane(const QString &name, bool *ok) {
    if (ok) *ok = true;
    // Z-up world. Each origin plane keeps sketch X/Y right-handed with its normal.
    if (name == "XY") return gp_Ax3(gp_Pnt(0, 0, 0), gp_Dir(0, 0, 1), gp_Dir(1, 0, 0));
    if (name == "XZ") return gp_Ax3(gp_Pnt(0, 0, 0), gp_Dir(0, -1, 0), gp_Dir(1, 0, 0));
    if (name == "YZ") return gp_Ax3(gp_Pnt(0, 0, 0), gp_Dir(1, 0, 0), gp_Dir(0, 1, 0));
    if (ok) *ok = false;
    return gp_Ax3();
}

TopoDS_Shape copyWithFaces(const TopoDS_Shape &shape, const QList<TopoDS_Face> &faces, QList<TopoDS_Face> &onCopy) {
    BRepBuilderAPI_Copy copier(shape);
    const TopoDS_Shape copy = copier.Shape();
    onCopy.clear();
    for (const TopoDS_Face &f : faces) {
        const TopTools_ListOfShape &m = copier.Modified(f);
        const TopoDS_Shape wanted = m.IsEmpty() ? TopoDS_Shape(f) : m.First();
        TopoDS_Face found = TopoDS::Face(wanted);
        for (TopExp_Explorer e(copy, TopAbs_FACE); e.More(); e.Next())
            if (e.Current().IsSame(wanted)) {
                found = TopoDS::Face(e.Current());
                break;
            }
        onCopy << found;
    }
    return copy;
}

bool runWithTimeLimit(qint64 ms, std::function<void()> work) {
    struct State {
        std::mutex mutex;
        std::condition_variable cv;
        bool done = false;
    };
    auto state = std::make_shared<State>();
    std::thread([state, work = std::move(work)] {
        // OCCT turns access violations into exceptions per thread; without this a crash inside one OCCT try
        // skips the Standard_Failure handlers that move on to the next try
        OSD::SetThreadLocalSignal(OSD_SignalMode_Set, false);
        try {
            work();
        } catch (...) {
        }
        std::lock_guard<std::mutex> lock(state->mutex);
        state->done = true;
        state->cv.notify_all();
    }).detach();
    std::unique_lock<std::mutex> lock(state->mutex);
    return state->cv.wait_for(lock, std::chrono::milliseconds(ms), [&] { return state->done; });
}

}
