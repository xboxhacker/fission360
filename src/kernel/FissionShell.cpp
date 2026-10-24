// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#include "FissionShell.h"
#include "GeomUtils.h"
#include "TimeLimit.h"
#include <BRepAdaptor_Surface.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepAlgoAPI_Fuse.hxx>
#include <BRepBuilderAPI_Copy.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakeSolid.hxx>
#include <BRepBuilderAPI_MakeVertex.hxx>
#include <BRepBuilderAPI_Sewing.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepClass_FaceClassifier.hxx>
#include <BRepExtrema_DistShapeShape.hxx>
#include <BRepGProp.hxx>
#include <BRepLib.hxx>
#include <BRepOffsetAPI_MakeOffset.hxx>
#include <BRepOffsetAPI_MakeOffsetShape.hxx>
#include <BRepOffsetAPI_MakeThickSolid.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <BRepTools.hxx>
#include <GProp_GProps.hxx>
#include <GeomAbs_SurfaceType.hxx>
#include <QElapsedTimer>
#include <ShapeFix_Shape.hxx>
#include <Standard_Failure.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_ListOfShape.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Solid.hxx>
#include <TopoDS_Wire.hxx>
#include <gp_Pnt2d.hxx>
#include <algorithm>
#include <cmath>
#include <iterator>
#include <cstdio>
#include <memory>

namespace {

// One budget for the whole Shell. The work runs on a copy in a worker thread that the feature stops waiting
// for after kShellLimitMs, so the clean-up after each OCCT try (ShapeFix, BRepCheck) counts too. Every OCCT
// offset and boolean step also gets a slice of the budget, so one slow try leaves time for the next one.
using Deadline = Handle(GeomUtils::TimeLimit);
constexpr qint64 kShellLimitMs = 15000;

// FISSION_TRACE_SHELL=1 prints every try with its time and outcome
bool tracing() {
    static const bool on = qEnvironmentVariableIsSet("FISSION_TRACE_SHELL");
    return on;
}
void trace(const char *what, const QElapsedTimer &t, const char *outcome) {
    if (!tracing()) return;
    std::fprintf(stderr, "[shell] %-34s %6lld ms  %s\n", what, t.elapsed(), outcome);
    std::fflush(stderr);
}

// At most `ms` of what is left of the whole budget
Deadline slice(const Deadline &dl, qint64 ms) { return new GeomUtils::TimeLimit(std::min(dl->remaining(), ms)); }

// a minus b within the deadline; null when it fails or runs out of time
TopoDS_Shape cutWithin(const TopoDS_Shape &a, const TopoDS_Shape &b, const Deadline &dl) {
    if (dl->expired()) return {};
    TopTools_ListOfShape args, tools;
    args.Append(a);
    tools.Append(b);
    BRepAlgoAPI_Cut cut;
    cut.SetArguments(args);
    cut.SetTools(tools);
    cut.Build(dl->Start());
    if (!cut.IsDone() || dl->expired()) return {};
    return cut.Shape();
}

gp_Dir outwardNormal(const TopoDS_Face &f) {
    BRepAdaptor_Surface s(f);
    double u0, u1, v0, v1;
    BRepTools::UVBounds(f, u0, u1, v0, v1);
    gp_Pnt p;
    gp_Vec du, dv;
    s.D1((u0 + u1) / 2, (v0 + v1) / 2, p, du, dv);
    const gp_Vec n = du.Crossed(dv);
    if (n.Magnitude() < 1e-12) return gp_Dir(0, 0, 1);
    gp_Dir d(n);
    if (f.Orientation() == TopAbs_REVERSED) d.Reverse();
    return d;
}

bool allPlanar(const TopoDS_Shape &s) {
    for (TopExp_Explorer e(s, TopAbs_FACE); e.More(); e.Next()) {
        if (BRepAdaptor_Surface(TopoDS::Face(e.Current()), Standard_False).GetType() != GeomAbs_Plane)
            return false;
    }
    return true;
}

TopoDS_Shape tryFix(const TopoDS_Shape &s) {
    if (s.IsNull()) return {};
    try {
        ShapeFix_Shape fix(s);
        fix.Perform();
        const TopoDS_Shape out = fix.Shape().IsNull() ? s : fix.Shape();
        return GeomUtils::unify(out);
    } catch (const Standard_Failure &) {
        return GeomUtils::unify(s);
    }
}

// Arc-join shells often leave knife-edge "teeth" at corners — reject those even if volume looks OK.
int tinyFaceCount(const TopoDS_Shape &s, double minArea) {
    int n = 0;
    for (TopExp_Explorer e(s, TopAbs_FACE); e.More(); e.Next()) {
        GProp_GProps props;
        BRepGProp::SurfaceProperties(TopoDS::Face(e.Current()), props);
        if (props.Mass() < minArea) ++n;
    }
    return n;
}

bool usableSolid(const TopoDS_Shape &s, double maxVolume, bool requireCheck, bool rejectTiny) {
    if (s.IsNull() || GeomUtils::solids(s).isEmpty()) return false;
    const double v = GeomUtils::volume(s);
    if (!(v > 1e-9 && v < maxVolume - 1e-9)) return false;
    if (rejectTiny && tinyFaceCount(s, 1e-2) > 0) return false;   // < 0.01 mm² faces = corner spikes
    if (!requireCheck) return true;
    return BRepCheck_Analyzer(s).IsValid();
}

TopoDS_Shape acceptShell(const TopoDS_Shape &candidate, double maxVolume, bool allowLoose) {
    const TopoDS_Shape fixed = tryFix(candidate);
    if (usableSolid(fixed, maxVolume, true, true)) return fixed;
    if (usableSolid(candidate, maxVolume, true, true)) return tryFix(candidate);
    if (!allowLoose) return {};
    // Last resort (punch fallback only): volume OK after fix, still no tiny faces
    if (usableSolid(fixed, maxVolume, false, true)) return fixed;
    return {};
}

struct ThickAttempt {
    const char *name;
    double tol;
    bool complete;           // Intersection flag (complete intersection mode)
    GeomAbs_JoinType join;
    bool removeInt;
};

// Every try gets its own copy: OCCT's offset updates tolerances and curves on the shape it is given, and a try
// stopped at its time limit may still be running on it in the background.
TopoDS_Shape freshCopy(const TopoDS_Shape &s, const TopTools_ListOfShape &faces, TopTools_ListOfShape &facesOnCopy) {
    QList<TopoDS_Face> in, out;
    for (TopTools_ListOfShape::Iterator it(faces); it.More(); it.Next()) in << TopoDS::Face(it.Value());
    const TopoDS_Shape copy = GeomUtils::copyWithFaces(s, in, out);
    facesOnCopy.Clear();
    for (const TopoDS_Face &f : out) facesOnCopy.Append(f);
    return copy;
}

// The volume of a wall `wall` thick over `area`, within a generous band: OCCT can return a closed, valid
// offset that is plainly wrong (on the spool hub the Arc offset without complete intersection covered only the
// flange, 6 of its 32 mm), so every offset and shell is weighed against this.
bool wallVolumeFits(double volume, double area, double wall) {
    const double expected = area * wall;
    return volume > expected * 0.4 && volume < expected * 1.8;
}

// Why a candidate shell was turned down, for the trace
const char *rejection(const TopoDS_Shape &s, double maxVolume) {
    if (s.IsNull() || GeomUtils::solids(s).isEmpty()) return "rejected: no solid";
    const double v = GeomUtils::volume(s);
    if (!(v > 1e-9 && v < maxVolume - 1e-9)) return "rejected: volume";
    if (tinyFaceCount(s, 1e-2) > 0) return "rejected: tiny faces";
    if (!BRepCheck_Analyzer(s).IsValid()) return "rejected: invalid";
    return "rejected";
}

// wallArea: the area of the faces that stay (all but the openings), for wallVolumeFits
// One thick-solid try on its own copy of the body; null unless it gives a clean shell of the right volume
TopoDS_Shape thickTry(const ThickAttempt &a, const TopoDS_Shape &body, const TopTools_ListOfShape &faces, double offset,
                      double maxVolume, double wallArea, const Deadline &step) {
    QElapsedTimer t;
    t.start();
    try {
        BRepOffsetAPI_MakeThickSolid thick;
        thick.MakeThickSolidByJoin(body, faces, offset, a.tol, BRepOffset_Skin, a.complete ? Standard_True : Standard_False,
                                   Standard_False, a.join, a.removeInt ? Standard_True : Standard_False, step->Start());
        if (!thick.IsDone() || step->expired()) {
            trace(a.name, t, step->expired() ? "out of time" : "not done");
            return {};
        }
        // Never loose-accept thick-solid — Arc junk must not ship
        const TopoDS_Shape ok = acceptShell(thick.Shape(), maxVolume, false);
        if (!ok.IsNull() && !wallVolumeFits(GeomUtils::volume(ok), wallArea, std::fabs(offset))) {
            trace(a.name, t, "rejected: wall volume");
            return {};
        }
        trace(a.name, t, ok.IsNull() ? rejection(thick.Shape(), maxVolume) : "ok");
        return ok;
    } catch (const Standard_Failure &) {
        trace(a.name, t, "exception");
        return {};
    }
}

TopoDS_Shape thickSolid(const TopoDS_Shape &solid, const TopTools_ListOfShape &openings, double offset,
                        double maxVolume, double wallArea, const Deadline &dl) {
    // Planar bodies: Intersection-first — Arc joins are what produce the jagged inner-corner teeth.
    // Curved bodies: Arc first (Intersection often fails on cylinders/fillets). There the Intersection join ran
    // past any time slice on the spool hub (OCCT checked the progress only every 10-18 s), so on curved bodies
    // each try runs in a worker of its own for at most 3 s, 6 s for all of them, and the punch still has time.
    const ThickAttempt planarOrder[] = {
        {"thick intersection 1e-7", 1e-7, true, GeomAbs_Intersection, true},
        {"thick intersection 1e-5", 1e-5, true, GeomAbs_Intersection, true},
        {"thick intersection 1e-4", 1e-4, true, GeomAbs_Intersection, true},
        {"thick intersection 1e-3", 1e-3, true, GeomAbs_Intersection, true},
        {"thick intersection partial", 1e-4, false, GeomAbs_Intersection, true},
        {"thick arc complete", 1e-4, true, GeomAbs_Arc, false},
    };
    const ThickAttempt curvedOrder[] = {
        {"thick arc", 1e-4, false, GeomAbs_Arc, false},
        {"thick intersection 1e-7", 1e-7, true, GeomAbs_Intersection, true},
        {"thick arc complete", 1e-4, true, GeomAbs_Arc, true},
        {"thick intersection 1e-4", 1e-4, true, GeomAbs_Intersection, true},
    };
    const bool planar = allPlanar(solid);
    const QList<ThickAttempt> attempts = planar ? QList<ThickAttempt>(std::begin(planarOrder), std::end(planarOrder))
                                                : QList<ThickAttempt>(std::begin(curvedOrder), std::end(curvedOrder));
    QElapsedTimer curvedTime;
    curvedTime.start();
    for (const ThickAttempt &a : attempts) {
        if (dl->expired() || (!planar && curvedTime.elapsed() > 6000 - 200)) break;
        TopTools_ListOfShape faces;
        const TopoDS_Shape body = freshCopy(solid, openings, faces);
        if (planar) {
            // A try that has not finished in a few seconds rarely finishes at all; leave time for the punch
            const TopoDS_Shape ok = thickTry(a, body, faces, offset, maxVolume, wallArea, slice(dl, 4000));
            if (!ok.IsNull()) return ok;
            continue;
        }
        const qint64 ms = std::min({dl->remaining(), qint64(3000), 6000 - curvedTime.elapsed()});
        auto result = std::make_shared<TopoDS_Shape>();
        const bool done = GeomUtils::runWithTimeLimit(ms, [a, body, faces, offset, maxVolume, wallArea, ms, result] {
            *result = thickTry(a, body, faces, offset, maxVolume, wallArea, new GeomUtils::TimeLimit(ms));
        });
        if (!done) {
            QElapsedTimer none;
            none.start();
            trace(a.name, none, "stopped at its time limit");
            continue;
        }
        if (!result->IsNull()) return *result;
    }
    return {};
}

TopoDS_Shape offsetSolid(const TopoDS_Shape &solid, double offset, const Deadline &dl) {
    const bool planar = allPlanar(solid);
    struct Attempt {
        const char *name;
        double tol;
        bool complete;
        GeomAbs_JoinType join;
        int tries = 1;
    };
    // Same priority: Intersection for clean corners on planar offsets. Curved bodies try Arc joins first, since
    // the Intersection join there can run for the whole budget; complete intersection is what makes the Arc
    // offset right on the spool hub. OCCT 7.6 returns that offset only on some runs (an empty result on the
    // others, from the same input, about one run in three succeeds), so it is tried again on fresh copies for up
    // to 8 s; each run takes under a second there.
    const Attempt planarOrder[] = {
        {"offset intersection 1e-7", 1e-7, true, GeomAbs_Intersection},
        {"offset intersection 1e-4", 1e-4, true, GeomAbs_Intersection},
        {"offset arc", 1e-4, false, GeomAbs_Arc},
    };
    const Attempt curvedOrder[] = {
        {"offset arc complete", 1e-4, true, GeomAbs_Arc, 12},
        {"offset arc", 1e-4, false, GeomAbs_Arc},
        {"offset intersection 1e-7", 1e-7, true, GeomAbs_Intersection},   // the tapered, filleted box
    };
    const double v0 = GeomUtils::volume(solid), area = GeomUtils::area(solid);
    const QList<Attempt> attempts = planar ? QList<Attempt>(std::begin(planarOrder), std::end(planarOrder))
                                           : QList<Attempt>(std::begin(curvedOrder), std::end(curvedOrder));
    for (const Attempt &a : attempts) {
        QElapsedTimer total;
        total.start();
        // Retries stop in time for the hollow cut and punch that follow (about 2.5 s on the spool hub)
        for (int k = 0; k < a.tries && (k == 0 || (total.elapsed() < 8000 && dl->remaining() > 3000)); ++k) {
            if (dl->expired()) break;
            QElapsedTimer t;
            t.start();
            try {
                const Deadline step = slice(dl, 4000);
                BRepOffsetAPI_MakeOffsetShape off;
                TopTools_ListOfShape none;
                off.PerformByJoin(freshCopy(solid, {}, none), offset, a.tol, BRepOffset_Skin, a.complete ? Standard_True : Standard_False,
                                  Standard_False, a.join, a.complete ? Standard_True : Standard_False, step->Start());
                if (!off.IsDone() || step->expired()) {
                    trace(a.name, t, step->expired() ? "out of time" : "not done");
                    continue;
                }
                // Every shell: where the body is thinner than two walls the offset comes apart into several
                // pieces, and keeping only the first one left most of the spool hub solid
                QList<TopoDS_Shape> pieces;
                for (TopExp_Explorer exp(off.Shape(), TopAbs_SHELL); exp.More(); exp.Next()) {
                    TopoDS_Solid inner = BRepBuilderAPI_MakeSolid(TopoDS::Shell(exp.Current())).Solid();
                    BRepLib::OrientClosedSolid(inner);
                    if (!inner.IsNull() && GeomUtils::volume(inner) > 1e-9) pieces << GeomUtils::unify(inner);
                }
                if (!pieces.isEmpty()) {
                    const TopoDS_Shape inner = GeomUtils::makeCompound(pieces);
                    if (!wallVolumeFits(std::fabs(GeomUtils::volume(inner) - v0), area, std::fabs(offset))) {
                        trace(a.name, t, "rejected: wall volume");
                        continue;
                    }
                    const QByteArray what = QString("ok, %1 piece(s)").arg(pieces.size()).toUtf8();
                    trace(a.name, t, what.constData());
                    return inner;
                }
                trace(a.name, t, "no solid");
            } catch (const Standard_Failure &) {
                trace(a.name, t, "exception");
            }
        }
    }
    return {};
}

// Points inside a face with the outward normal at each. The centroid of a revolved face (an annulus, a cone)
// lies on the axis, off the face, so this samples the face's UV range: the middle first, then a 5 x 5 grid.
struct FacePoint {
    gp_Pnt p;
    gp_Dir n;
};
QList<FacePoint> pointsOnFace(const TopoDS_Face &f, int most) {
    QList<FacePoint> out;
    BRepAdaptor_Surface s(f);
    double u0, u1, v0, v1;
    BRepTools::UVBounds(f, u0, u1, v0, v1);
    for (int i = 0; i < 26 && out.size() < most; ++i) {
        const double fu = i == 0 ? 0.5 : 0.1 + 0.2 * ((i - 1) % 5), fv = i == 0 ? 0.5 : 0.1 + 0.2 * ((i - 1) / 5);
        const gp_Pnt2d uv(u0 + (u1 - u0) * fu, v0 + (v1 - v0) * fv);
        BRepClass_FaceClassifier inside(f, uv, 1e-7);
        if (inside.State() != TopAbs_IN) continue;
        gp_Pnt p;
        gp_Vec du, dv;
        s.D1(uv.X(), uv.Y(), p, du, dv);
        const gp_Vec cross = du.Crossed(dv);
        if (cross.Magnitude() < 1e-12) continue;
        gp_Dir n(cross);
        if (f.Orientation() == TopAbs_REVERSED) n.Reverse();
        out << FacePoint{p, n};
    }
    return out;
}

// The faces of the inner (offset) solid that lie one wall under an opening face: found from points on the
// opening, stepped one wall inwards. An inner face much larger than the opening reaches past it (into walls
// that must stay), so it is not punched.
QList<TopoDS_Face> facesUnder(const TopoDS_Shape &inner, const TopoDS_Face &opening, double wall) {
    QList<TopoDS_Face> out;
    const double most = GeomUtils::area(opening) * 2 + wall * wall;
    const double closeBy = wall * 0.05 + 1e-3;
    // Exact distances to blends are slow (a filleted box ran past the budget), so a box test comes first
    QList<QPair<TopoDS_Face, Bnd_Box>> candidates;
    for (TopExp_Explorer e(inner, TopAbs_FACE); e.More(); e.Next()) {
        Bnd_Box box = GeomUtils::boundingBox(e.Current());
        box.Enlarge(closeBy);
        candidates << qMakePair(TopoDS::Face(e.Current()), box);
    }
    for (const FacePoint &fp : pointsOnFace(opening, 26)) {
        const gp_Pnt below = fp.p.Translated(gp_Vec(fp.n) * -wall);
        const TopoDS_Vertex under = BRepBuilderAPI_MakeVertex(below).Vertex();
        for (const auto &[f, box] : candidates) {
            if (box.IsOut(below)) continue;
            if (std::any_of(out.begin(), out.end(), [&](const TopoDS_Face &o) { return o.IsSame(f); })) continue;
            BRepExtrema_DistShapeShape dist(under, f);
            if (!dist.IsDone() || dist.NbSolution() < 1 || dist.Value() > closeBy) continue;
            if (GeomUtils::area(f) > most) return {};
            out << f;
        }
    }
    return out;
}

// What a punch removes over one inner face: the wall between it and the opening, plus a little beyond
TopoDS_Shape wallOver(const TopoDS_Face &innerFace, double wall) {
    const double reach = wall * 1.25 + 0.05;
    if (BRepAdaptor_Surface(innerFace, Standard_False).GetType() == GeomAbs_Plane) {
        const QList<FacePoint> at = pointsOnFace(innerFace, 1);
        if (at.isEmpty()) return {};
        BRepPrimAPI_MakePrism prism(innerFace, gp_Vec(at.first().n) * reach);
        return prism.IsDone() ? prism.Shape() : TopoDS_Shape();
    }
    BRepOffsetAPI_MakeThickSolid thick;
    thick.MakeThickSolidBySimple(innerFace, reach);
    return thick.IsDone() ? thick.Shape() : TopoDS_Shape();
}

// Grow a planar opening face in-plane so the punch clears adjacent walls at corners
TopoDS_Shape punchTool(const TopoDS_Face &f, double wall, double margin) {
    const gp_Dir n = outwardNormal(f);
    TopoDS_Shape faceShape = f;
    // 2D offset of the outer wire when the face is planar — grows the cut past sharp corners
    try {
        if (BRepAdaptor_Surface(f, Standard_False).GetType() == GeomAbs_Plane) {
            TopoDS_Wire outer;
            for (TopExp_Explorer w(f, TopAbs_WIRE); w.More(); w.Next()) {
                outer = TopoDS::Wire(w.Current());
                break;
            }
            if (!outer.IsNull()) {
                BRepOffsetAPI_MakeOffset off(outer, GeomAbs_Intersection);
                off.Perform(margin);
                if (off.IsDone() && !off.Shape().IsNull()) {
                    TopoDS_Wire grown;
                    if (off.Shape().ShapeType() == TopAbs_WIRE) grown = TopoDS::Wire(off.Shape());
                    else {
                        for (TopExp_Explorer e(off.Shape(), TopAbs_WIRE); e.More(); e.Next()) {
                            grown = TopoDS::Wire(e.Current());
                            break;
                        }
                    }
                    if (!grown.IsNull()) {
                        BRepBuilderAPI_MakeFace mk(grown);
                        if (mk.IsDone()) faceShape = mk.Face();
                    }
                }
            }
        }
    } catch (const Standard_Failure &) {
        faceShape = f;
    }

    // A flat opening has exactly one wall under it, and cutting deeper shortens the side walls around it. A
    // curved one is pushed along a single normal, so it needs the extra depth to clear the wall everywhere.
    const bool flat = BRepAdaptor_Surface(f, Standard_False).GetType() == GeomAbs_Plane;
    const double depth = flat ? wall * 1.25 + 0.05 : wall * 2.0 + 1.0;
    BRepPrimAPI_MakePrism inPrism(faceShape, gp_Vec(n.Reversed()) * depth);
    BRepPrimAPI_MakePrism outPrism(faceShape, gp_Vec(n) * std::max(0.5, margin));
    TopoDS_Shape tool = inPrism.Shape();
    if (outPrism.IsDone()) {
        BRepAlgoAPI_Fuse fuse(tool, outPrism.Shape());
        if (fuse.IsDone() && !fuse.Shape().IsNull()) tool = fuse.Shape();
    }
    return tool;
}

TopoDS_Shape sewSolid(const TopoDS_Shape &s) {
    try {
        BRepBuilderAPI_Sewing sew(1e-4);
        for (TopExp_Explorer e(s, TopAbs_FACE); e.More(); e.Next()) sew.Add(e.Current());
        sew.Perform();
        TopoDS_Shape sewn = sew.SewedShape();
        if (sewn.IsNull()) return s;
        for (TopExp_Explorer sh(sewn, TopAbs_SHELL); sh.More(); sh.Next()) {
            TopoDS_Solid solid = BRepBuilderAPI_MakeSolid(TopoDS::Shell(sh.Current())).Solid();
            BRepLib::OrientClosedSolid(solid);
            if (!solid.IsNull() && GeomUtils::volume(solid) > 1e-9) return solid;
        }
        return sewn;
    } catch (const Standard_Failure &) {
        return s;
    }
}

TopoDS_Shape checkedPunch(const TopoDS_Shape &s, double maxVolume) {
    TopoDS_Shape ok = acceptShell(s, maxVolume, false);
    if (!ok.IsNull()) return ok;
    ok = acceptShell(s, maxVolume, true);
    if (!ok.IsNull()) return ok;
    // Offset of a blend whose radius equals the wall leaves slivers under 0.01 mm².
    // A solid that still passes BRepCheck is a usable shell; spiked Arc results never get here
    // because they fail the check.
    const TopoDS_Shape fixed = tryFix(s);
    if (usableSolid(fixed, maxVolume, true, false)) return fixed;
    if (usableSolid(s, maxVolume, true, false)) return s;
    return {};
}

// Fallback when MakeThickSolid fails: hollow by solid-offset cut, then punch the openings. The punch first
// removes only the wall over the cavity (the inner faces under each opening, pushed out through the wall),
// which also works on curved openings; the older planar punch (the opening grown and pushed in) is the
// last resort.
TopoDS_Shape punchOpenings(const TopoDS_Shape &solid, const TopTools_ListOfShape &openings, double offset,
                           double maxVolume, double wallArea, const Deadline &dl) {
    if (offset >= 0) return {};
    const TopoDS_Shape inner = offsetSolid(solid, offset, dl);
    if (inner.IsNull()) return {};
    QElapsedTimer t;
    t.start();
    TopoDS_Shape hollow;
    try {
        hollow = cutWithin(solid, inner, slice(dl, 6000));
    } catch (const Standard_Failure &) {
    }
    trace("hollow cut", t, hollow.IsNull() ? "failed" : "ok");
    if (hollow.IsNull()) return {};
    const double wall = std::fabs(offset);

    auto punch = [&](const char *name, auto &&toolFor) -> TopoDS_Shape {
        QElapsedTimer pt;
        pt.start();
        TopoDS_Shape r = hollow;
        for (TopTools_ListOfShape::Iterator it(openings); it.More() && !r.IsNull(); it.Next()) {
            try {
                const TopoDS_Shape tool = toolFor(TopoDS::Face(it.Value()));
                r = tool.IsNull() ? TopoDS_Shape() : cutWithin(r, tool, slice(dl, 4000));
            } catch (const Standard_Failure &) {
                r.Nullify();
            }
        }
        if (r.IsNull()) {
            trace(name, pt, "cut failed");
            return {};
        }
        // Sewing can be what makes a checked solid fail, so try the punched shape first.
        TopoDS_Shape ok = checkedPunch(r, maxVolume);
        if (ok.IsNull() && !dl->expired()) ok = checkedPunch(sewSolid(r), maxVolume);
        if (!ok.IsNull() && !wallVolumeFits(GeomUtils::volume(ok), wallArea, wall)) {
            trace(name, pt, "rejected: wall volume");
            return {};
        }
        trace(name, pt, ok.IsNull() ? "rejected" : "ok");
        return ok;
    };

    TopoDS_Shape ok = punch("punch over cavity", [&](const TopoDS_Face &opening) -> TopoDS_Shape {
        QList<TopoDS_Shape> parts;
        const QList<TopoDS_Face> under = facesUnder(inner, opening, wall);
        if (under.isEmpty()) trace("  no inner face under the opening", t, "");
        for (const TopoDS_Face &f : under) {
            const TopoDS_Shape w = wallOver(f, wall);
            if (w.IsNull()) {
                trace("  inner face could not be thickened", t, "");
                return {};
            }
            parts << w;
        }
        return parts.isEmpty() ? TopoDS_Shape() : GeomUtils::makeCompound(parts);
    });
    if (ok.IsNull() && !dl->expired())
        ok = punch("punch grown opening", [&](const TopoDS_Face &opening) {
            return punchTool(opening, wall, wall * 0.25 + 0.25);
        });
    return ok;
}

struct ShellRun {
    TopoDS_Shape shape;
    QString error;
};

// The whole Shell on its own copy of the body (runs in the worker thread)
void shellBody(ShellRun &run, const TopoDS_Shape &solid, const TopTools_ListOfShape &openings, double offset,
               double maxVolume) {
    const Deadline dl = new GeomUtils::TimeLimit(kShellLimitMs);
    const char *slow = "Shell took longer than 15 s and was stopped (try a thinner wall or other faces)";
    if (!openings.IsEmpty()) {
        double wallArea = GeomUtils::area(solid);
        for (TopTools_ListOfShape::Iterator it(openings); it.More(); it.Next()) wallArea -= GeomUtils::area(it.Value());
        run.shape = thickSolid(solid, openings, offset, maxVolume, wallArea, dl);
        if (run.shape.IsNull()) run.shape = punchOpenings(solid, openings, offset, maxVolume, wallArea, dl);
        if (run.shape.IsNull())
            run.error = dl->expired() ? slow : "Shell failed: the walls could not be offset (try a thinner wall)";
        return;
    }
    const TopoDS_Shape inner = offsetSolid(solid, offset, dl);
    if (inner.IsNull()) {
        run.error = dl->expired() ? "Shell took longer than 15 s and was stopped (try a thinner wall)"
                                  : "Shell failed: the walls could not be offset (try a thinner wall)";
        return;
    }
    const TopoDS_Shape cut = offset < 0 ? cutWithin(solid, inner, dl) : cutWithin(inner, solid, dl);
    if (cut.IsNull()) {
        run.error = dl->expired() ? "Shell took longer than 15 s and was stopped" : "Shell failed";
        return;
    }
    if (offset < 0) {
        const TopoDS_Shape ok = acceptShell(cut, maxVolume, true);
        run.shape = ok.IsNull() ? tryFix(cut) : ok;
    } else {
        run.shape = cut;
    }
}

} // namespace

bool FissionShell::compute(RebuildContext &ctx) {
    double t = 0;
    if (!number(ctx, "thickness", t)) return false;
    if (t <= 0) return fail("Thickness must be positive");
    const double offset = str("direction", "inside") == "outside" ? t : -t;

    QString err;
    const QVariantList refs = list("faces");
    int body = integer("body", -1);
    // Nothing picked yet: wait for faces (or a body to hollow) instead of hollowing a body straight away
    if (refs.isEmpty() && body < 0) return fail("Select the faces to remove, or a body to hollow");
    TopTools_ListOfShape openings;
    for (const QVariant &ref : refs) {
        int b = -1;
        TopoDS_Face f;
        if (!ctx.resolveFace(ref, b, f, err)) return fail(err);
        if (!openings.IsEmpty() && b != body) return fail("All shell faces must belong to the same body");
        body = b;
        openings.Append(f);
    }
    if (body < 0 || body >= ctx.bodies.size()) return fail("Body no longer exists");
    const TopoDS_Shape solid = GeomUtils::unify(ctx.bodies[body].shape);
    const double v0 = GeomUtils::volume(solid);
    const double maxVolume = offset < 0 ? v0 : 1e300;

    TopTools_ListOfShape unifiedOpenings;
    for (const QVariant &ref : refs) {
        int b = -1;
        TopoDS_Face f;
        if (!ctx.resolveFace(ref, b, f, err)) return fail(err);
        const gp_Pnt want = GeomUtils::faceCentroid(f);
        TopoDS_Face matched;
        double best = 1e300;
        for (TopExp_Explorer e(solid, TopAbs_FACE); e.More(); e.Next()) {
            const TopoDS_Face cand = TopoDS::Face(e.Current());
            const double d = GeomUtils::faceCentroid(cand).Distance(want);
            if (d < best) {
                best = d;
                matched = cand;
            }
        }
        if (matched.IsNull() || best > 1.0) return fail("Shell face no longer exists on the body");
        unifiedOpenings.Append(matched);
    }

    // The worker gets its own copy, so it can keep running on it after the feature has given up
    TopTools_ListOfShape copiedOpenings;
    const TopoDS_Shape copy = freshCopy(solid, unifiedOpenings, copiedOpenings);
    auto run = std::make_shared<ShellRun>();
    const bool finished = GeomUtils::runWithTimeLimit(kShellLimitMs + 500, [run, copy, copiedOpenings, offset, maxVolume] {
        try {
            shellBody(*run, copy, copiedOpenings, offset, maxVolume);
        } catch (...) {
            run->shape.Nullify();
            run->error = QStringLiteral("Shell failed in the geometry kernel");
        }
    });
    if (!finished)
        return fail(openings.IsEmpty() ? "Shell took longer than 15 s and was stopped (try a thinner wall)"
                                       : "Shell took longer than 15 s and was stopped (try a thinner wall or other faces)");
    if (run->shape.IsNull()) return fail(run->error.isEmpty() ? QStringLiteral("Shell failed") : run->error);
    ctx.bodies[body].shape = run->shape;
    return true;
}
