// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
// INSPECT queries of FissionDocument: Measure and Interference
#include "FissionDocument.h"
#include "FissionConstruction.h"
#include "GeomUtils.h"
#include "MeshUtils.h"

#include <BRep_Tool.hxx>
#include <BRepAdaptor_Curve.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepAlgoAPI_Common.hxx>
#include <BRepBuilderAPI_MakeVertex.hxx>
#include <BRepExtrema_DistShapeShape.hxx>
#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <Standard_Failure.hxx>
#include <TopoDS.hxx>
#include <cmath>
#include <optional>

namespace {

struct Measured {
    TopoDS_Shape shape;
    QString name;
    std::optional<gp_Dir> direction;   // plane normal or line direction
};

}   // namespace

QVariantMap FissionDocument::measure(const QVariantList &refs) const {
    auto len = [&](double mm) { return formatLength(mm, 3); };
    auto num = [](double v) { return QString::number(v, 'f', 3); };
    QVariantList lines;
    auto line = [&](const QString &label, const QString &text) { lines << QVariant(QVariantList{label, text}); };

    QList<Measured> items;
    for (const QVariant &rv : refs) {
        const QVariantMap r = rv.toMap();
        const QString kind = r.value("kind").toString();
        Measured m;
        if (kind == "face" || kind == "edge" || kind == "body") {
            FissionBody *b = body(r.value("body").toInt());
            if (!b || b->shape().IsNull()) continue;
            const TopoDS_Shape s = b->shape();
            if (kind == "body") {
                m.shape = s;
                m.name = b->name();
            } else {
                const auto map = kind == "face" ? GeomUtils::faceMap(s) : GeomUtils::edgeMap(s);
                const int i = r.value("index").toInt() + 1;
                if (i < 1 || i > map.Extent()) continue;
                m.shape = map(i);
                m.name = QString("%1 %2 (%3)").arg(kind == "face" ? "Face" : "Edge").arg(i).arg(b->name());
            }
        } else if (kind == "point") {
            auto *cp = qobject_cast<FissionConstructionPoint *>(featureById(r.value("id").toString()));
            if (!cp) continue;
            m.shape = BRepBuilderAPI_MakeVertex(cp->point()).Vertex();
            m.name = cp->name();
        } else if (kind == "originpoint") {
            m.shape = BRepBuilderAPI_MakeVertex(gp_Pnt(0, 0, 0)).Vertex();
            m.name = "Origin";
        } else {
            continue;
        }
        items << m;
        if (items.size() == 2) break;
    }
    if (items.isEmpty()) return {{"ok", false}, {"error", "Select a face, edge, body or point"}};

    for (Measured &m : items) {
        line(m.name, "");
        try {
            switch (m.shape.ShapeType()) {
            case TopAbs_FACE: {
                const TopoDS_Face f = TopoDS::Face(m.shape);
                line("Area", formatLength(GeomUtils::area(f), 3, 2));
                if (MeshUtils::isMesh(f)) break;
                BRepAdaptor_Surface s(f);
                if (s.GetType() == GeomAbs_Plane) {
                    gp_Dir n = s.Plane().Axis().Direction();
                    if (f.Orientation() == TopAbs_REVERSED) n.Reverse();
                    m.direction = n;
                    line("Normal", QString("%1, %2, %3").arg(num(n.X()), num(n.Y()), num(n.Z())));
                } else if (s.GetType() == GeomAbs_Cylinder) {
                    line("Radius", len(s.Cylinder().Radius()));
                    line("Diameter", len(2 * s.Cylinder().Radius()));
                    m.direction = s.Cylinder().Axis().Direction();
                } else if (s.GetType() == GeomAbs_Sphere) {
                    line("Radius", len(s.Sphere().Radius()));
                }
                break;
            }
            case TopAbs_EDGE: {
                const TopoDS_Edge e = TopoDS::Edge(m.shape);
                GProp_GProps lp;
                BRepGProp::LinearProperties(e, lp);
                line("Length", len(lp.Mass()));
                BRepAdaptor_Curve c(e);
                if (c.GetType() == GeomAbs_Line) {
                    m.direction = c.Line().Direction();
                } else if (c.GetType() == GeomAbs_Circle) {
                    line("Radius", len(c.Circle().Radius()));
                    line("Diameter", len(2 * c.Circle().Radius()));
                    const gp_Pnt ctr = c.Circle().Location();
                    line("Center", QString("%1, %2, %3").arg(len(ctr.X()), len(ctr.Y()), len(ctr.Z())));
                }
                break;
            }
            case TopAbs_VERTEX: {
                const gp_Pnt p = BRep_Tool::Pnt(TopoDS::Vertex(m.shape));
                line("Position", QString("%1, %2, %3").arg(len(p.X()), len(p.Y()), len(p.Z())));
                break;
            }
            default:
                line("Volume", formatLength(GeomUtils::volume(m.shape), 3, 3));
                line("Area", formatLength(GeomUtils::area(m.shape), 3, 2));
                break;
            }
        } catch (const Standard_Failure &) {
        }
    }

    QVariantMap out{{"ok", true}, {"error", ""}};
    if (items.size() == 2) {
        line("Between", "");
        if (MeshUtils::isMesh(items[0].shape) || MeshUtils::isMesh(items[1].shape)) {
            line("Distance", "not available for mesh bodies");
        } else {
            try {
                BRepExtrema_DistShapeShape dist(items[0].shape, items[1].shape);
                if (dist.IsDone() && dist.NbSolution() > 0) {
                    const gp_Pnt a = dist.PointOnShape1(1), b = dist.PointOnShape2(1);
                    line("Minimum distance", len(dist.Value()));
                    line("\u0394X, \u0394Y, \u0394Z", QString("%1, %2, %3").arg(len(b.X() - a.X()), len(b.Y() - a.Y()), len(b.Z() - a.Z())));
                    out["distance"] = dist.Value();
                    out["from"] = QVariantList{a.X(), a.Y(), a.Z()};
                    out["to"] = QVariantList{b.X(), b.Y(), b.Z()};
                }
            } catch (const Standard_Failure &) {
            }
        }
        if (items[0].direction && items[1].direction) {
            double ang = items[0].direction->Angle(*items[1].direction) * 180.0 / M_PI;
            if (ang > 90) ang = 180 - ang;
            line("Angle", QString::number(ang, 'f', 3) + "\u00b0");
            out["angle"] = ang;
        }
    }
    out["lines"] = lines;
    return out;
}

QVariantList FissionDocument::interference(const QVariantList &bodiesArg) const {
    QList<int> idx;
    for (const QVariant &v : bodiesArg)
        if (v.toInt() >= 0 && v.toInt() < m_bodies.size() && !idx.contains(v.toInt())) idx << v.toInt();
    if (idx.isEmpty())
        for (int i = 0; i < m_bodies.size(); ++i) idx << i;
    QVariantList out;
    for (int a = 0; a < idx.size(); ++a) {
        for (int b = a + 1; b < idx.size(); ++b) {
            const TopoDS_Shape sa = m_bodies[idx[a]]->shape(), sb = m_bodies[idx[b]]->shape();
            if (sa.IsNull() || sb.IsNull() || MeshUtils::isMesh(sa) || MeshUtils::isMesh(sb)) continue;
            if (GeomUtils::boundingBox(sa).IsOut(GeomUtils::boundingBox(sb))) continue;
            try {
                BRepAlgoAPI_Common common(sa, sb);
                if (!common.IsDone() || common.HasErrors()) continue;
                const double v = GeomUtils::volume(common.Shape());
                if (v > 1e-6)
                    out << QVariantMap{{"a", idx[a]}, {"b", idx[b]}, {"volume", v},
                                       {"names", m_bodies[idx[a]]->name() + " \u2229 " + m_bodies[idx[b]]->name()}};
            } catch (const Standard_Failure &) {
            }
        }
    }
    return out;
}
