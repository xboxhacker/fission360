// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#pragma once
#include "FissionFeature.h"
#include "FissionTransform.h"
#include <QList>
#include <TopoDS_Face.hxx>
#include <gp_Pln.hxx>

// Move `faces` of `solid` by `distance` along their outward normals. A round chamfer is pulled
// (its size changes, or it comes off). Other faces, flat or curved, are offset by OCCT, or else the slab
// between the face and its moved copy is added or cut. Each step has a time limit.
bool offsetFacesOnSolid(const TopoDS_Shape &solid, const QList<TopoDS_Face> &faces, double distance,
                        TopoDS_Shape &out, QString &error);

// Draft of one flat face by turning its plane about the line where it meets `neutral` until it leans `rad`
// towards `pull`, then cutting or filling the body to it over the face's outline. Draft falls back to this
// where OCCT's draft fails. Null when it cannot be done.
TopoDS_Shape draftFlatFace(const TopoDS_Shape &solid, const TopoDS_Face &face, const gp_Dir &pull, const gp_Pln &neutral,
                           double rad);

// MODIFY tools that work on existing bodies. Body inputs are body indices; face inputs are face refs
// (see RebuildContext). Plane inputs take any plane ref accepted by RebuildContext::resolvePlane.

// props: target (body index), tools [body indices], operation "join" | "cut" | "intersect", keepTools
class FissionCombine : public FissionFeature {
    Q_OBJECT
public:
    explicit FissionCombine(QObject *parent = nullptr) : FissionFeature(parent) {}
    QString type() const override { return QStringLiteral("Combine"); }
    QVariantMap defaults() const override {
        return {{"target", -1}, {"tools", QVariantList()}, {"operation", "join"}, {"keepTools", false}};
    }
protected:
    bool compute(RebuildContext &ctx) override;
};

// Deletes bodies (solid or mesh) from the design from this point of the timeline on. props: bodies
class FissionRemove : public FissionFeature {
    Q_OBJECT
public:
    explicit FissionRemove(QObject *parent = nullptr) : FissionFeature(parent) {}
    QString type() const override { return QStringLiteral("Remove"); }
    QVariantMap defaults() const override { return {{"bodies", QVariantList()}}; }
protected:
    bool compute(RebuildContext &ctx) override;
};

// props: bodies, moveType, translate [x, y, z], rotation [x, y, z] (deg), pivot [x, y, z], axis (axis ref), angle,
//        fromPoint / toPoint [x, y, z], copy
// moveType:
//   free            rotate about the pivot (X, then Y, then Z, world axes), then translate
//   translate       translate only
//   rotate          rotate by angle about axis
//   pointToPoint    translate by toPoint - fromPoint
//   pointToPosition translate so fromPoint lands on the position given by translate
//   "" (older documents): rotate about axis, then translate
// Mesh bodies move too.
class FissionMove : public FissionFeature {
    Q_OBJECT
public:
    explicit FissionMove(QObject *parent = nullptr) : FissionFeature(parent) {}
    QString type() const override { return QStringLiteral("Move"); }
    QVariantMap defaults() const override {
        const QVariantList zero{"0 mm", "0 mm", "0 mm"};
        return {{"bodies", QVariantList()}, {"moveType", "free"}, {"translate", zero},
                {"rotation", QVariantList{"0 deg", "0 deg", "0 deg"}}, {"pivot", zero},
                {"axis", "Z"}, {"angle", "0 deg"}, {"fromPoint", zero}, {"toPoint", zero}, {"copy", false}};
    }
    QStringList expressionKeys() const override { return {"angle"}; }
protected:
    bool compute(RebuildContext &ctx) override;
};

// ASSEMBLE > Joint (J) / As-built Joint (Shift+J). Bodies stand in for components.
// props: bodies (the moving part), from / to (joint origins: a face ref, clicked point included, or an edge ref),
//        asBuilt, motion "rigid" | "revolute" | "slider" | "cylindrical", angle, offset, flip
// A joint origin is a point with a Z direction: a flat face at the clicked point (or its centre) along its normal,
// a round face on its axis, a circular edge at its centre along its axis, a straight edge at its midpoint.
// Joint: the bodies move so `from` lands on `to` with the Z axes facing each other (flip: same way).
// As-built: nothing moves into place; `to` only sets the joint axis.
// Then the bodies turn by angle about the joint Z and slide by offset along it (the motion's drive values:
// revolute = angle, slider = offset, cylindrical = both, rigid = fixed placement offsets).
class FissionJoint : public FissionFeature {
    Q_OBJECT
public:
    explicit FissionJoint(QObject *parent = nullptr) : FissionFeature(parent) {}
    QString type() const override { return QStringLiteral("Joint"); }
    QVariantMap defaults() const override {
        return {{"bodies", QVariantList()}, {"from", QVariant()}, {"to", QVariant()}, {"asBuilt", false},
                {"motion", "rigid"}, {"angle", "0 deg"}, {"offset", "0 mm"}, {"flip", false}};
    }
    QStringList expressionKeys() const override { return {"angle", "offset"}; }
protected:
    bool compute(RebuildContext &ctx) override;
};

// props: bodies, point (point ref, "" = origin), scaleType "uniform" | "nonUniform", scale, scaleX/Y/Z
class FissionScale : public FissionFeature {
    Q_OBJECT
public:
    explicit FissionScale(QObject *parent = nullptr) : FissionFeature(parent) {}
    QString type() const override { return QStringLiteral("Scale"); }
    QVariantMap defaults() const override {
        return {{"bodies", QVariantList()}, {"point", ""}, {"scaleType", "uniform"}, {"scale", "2"},
                {"scaleX", "1"}, {"scaleY", "1"}, {"scaleZ", "1"}};
    }
    QStringList expressionKeys() const override { return {"scale", "scaleX", "scaleY", "scaleZ"}; }
protected:
    bool compute(RebuildContext &ctx) override;
};

// props: bodies, tool (plane ref) or toolBody (body index, -1 = use the plane)
// Every resulting lump becomes its own body.
class FissionSplitBody : public FissionFeature {
    Q_OBJECT
public:
    explicit FissionSplitBody(QObject *parent = nullptr) : FissionFeature(parent) {}
    QString type() const override { return QStringLiteral("SplitBody"); }
    QVariantMap defaults() const override {
        return {{"bodies", QVariantList()}, {"tool", "XY"}, {"toolBody", -1}};
    }
protected:
    bool compute(RebuildContext &ctx) override;
};

// props: faces (face refs on one body), tool (plane ref). Splits only the chosen faces along the plane.
class FissionSplitFace : public FissionFeature {
    Q_OBJECT
public:
    explicit FissionSplitFace(QObject *parent = nullptr) : FissionFeature(parent) {}
    QString type() const override { return QStringLiteral("SplitFace"); }
    QVariantMap defaults() const override { return {{"faces", QVariantList()}, {"tool", "XY"}}; }
protected:
    bool compute(RebuildContext &ctx) override;
};

// props: faces (face refs on one body), distance. Moves the faces along their normals; neighbours follow.
class FissionOffsetFace : public FissionFeature {
    Q_OBJECT
public:
    explicit FissionOffsetFace(QObject *parent = nullptr) : FissionFeature(parent) {}
    QString type() const override { return QStringLiteral("OffsetFace"); }
    QVariantMap defaults() const override { return {{"faces", QVariantList()}, {"distance", "2 mm"}}; }
    QStringList expressionKeys() const override { return {"distance"}; }
protected:
    bool compute(RebuildContext &ctx) override;
};

// props: faces (face refs) or bodies, distance. Adds offset copies as new bodies (surfaces for faces,
// a grown/shrunk solid for whole bodies).
class FissionOffset : public FissionFeature {
    Q_OBJECT
public:
    explicit FissionOffset(QObject *parent = nullptr) : FissionFeature(parent) {}
    QString type() const override { return QStringLiteral("Offset"); }
    QVariantMap defaults() const override {
        return {{"faces", QVariantList()}, {"bodies", QVariantList()}, {"distance", "2 mm"}};
    }
    QStringList expressionKeys() const override { return {"distance"}; }
protected:
    bool compute(RebuildContext &ctx) override;
};

// props: faces (face refs on one body), plane (plane ref). Extends or trims the body under the faces
// so the faces end on the plane.
class FissionReplaceFace : public FissionFeature {
    Q_OBJECT
public:
    explicit FissionReplaceFace(QObject *parent = nullptr) : FissionFeature(parent) {}
    QString type() const override { return QStringLiteral("ReplaceFace"); }
    QVariantMap defaults() const override { return {{"faces", QVariantList()}, {"plane", ""}}; }
protected:
    bool compute(RebuildContext &ctx) override;
};

// props: faces (face refs, on one or more bodies). Removes the faces and grows their neighbours over the gap
// (OCCT defeaturing), like Fusion's Delete Face. Fails when the neighbours cannot close the solid.
class FissionDeleteFace : public FissionFeature {
    Q_OBJECT
public:
    explicit FissionDeleteFace(QObject *parent = nullptr) : FissionFeature(parent) {}
    QString type() const override { return QStringLiteral("DeleteFace"); }
    QVariantMap defaults() const override { return {{"faces", QVariantList()}}; }
protected:
    bool compute(RebuildContext &ctx) override;
};

// props: faces (face refs on one body), plane (pull direction / parting plane; "" = XY), angle, flip,
// draftType "face" | "parting" (parting: both sides of the plane taper away from it).
// plane may also be non-planar: a cylindrical / conical / revolved face (its axis), a straight edge, a
// construction axis or a sketch line; the neutral plane then goes through it, square to the pull.
class FissionDraft : public FissionFeature {
    Q_OBJECT
public:
    explicit FissionDraft(QObject *parent = nullptr) : FissionFeature(parent) {}
    QString type() const override { return QStringLiteral("Draft"); }
    QVariantMap defaults() const override {
        return {{"faces", QVariantList()}, {"plane", ""}, {"angle", "3 deg"}, {"flip", false},
                {"draftType", "face"}};
    }
    QStringList expressionKeys() const override { return {"angle"}; }
protected:
    bool compute(RebuildContext &ctx) override;
};

// props: bodies, path (sketch id), count, distance ("" = whole path), orientation "identical" | "path",
// operation "join" | "newBody"
class FissionPathPattern : public FissionBodyCopyFeature {
    Q_OBJECT
public:
    explicit FissionPathPattern(QObject *parent = nullptr) : FissionBodyCopyFeature(parent) {}
    QString type() const override { return QStringLiteral("PathPattern"); }
    QVariantMap defaults() const override {
        return {{"bodies", QVariantList()}, {"path", ""}, {"count", "4"}, {"distance", ""},
                {"orientation", "identical"}, {"operation", "newBody"}};
    }
    QStringList expressionKeys() const override { return {"count", "distance"}; }
protected:
    bool transforms(RebuildContext &ctx, QList<gp_Trsf> &out) override;
};

// Thin walls from the open curves of a sketch, grown until they meet the body (or a set depth).
// props: sketch, thickness, extent "toNext" | "distance", depth, flip
// Rib grows to one side of the sketch plane; Web grows to both sides.
class FissionRib : public FissionFeature {
    Q_OBJECT
public:
    explicit FissionRib(QObject *parent = nullptr) : FissionFeature(parent) {}
    QString type() const override { return QStringLiteral("Rib"); }
    QVariantMap defaults() const override {
        return {{"sketch", ""}, {"thickness", "2 mm"}, {"extent", "toNext"}, {"depth", "10 mm"}, {"flip", false}};
    }
    QStringList expressionKeys() const override { return {"thickness", "depth"}; }
protected:
    bool compute(RebuildContext &ctx) override;
    virtual bool bothSides() const { return false; }
};

class FissionWeb : public FissionRib {
    Q_OBJECT
public:
    explicit FissionWeb(QObject *parent = nullptr) : FissionRib(parent) {}
    QString type() const override { return QStringLiteral("Web"); }
    QVariantMap defaults() const override {
        QVariantMap d = FissionRib::defaults();
        d["extent"] = "distance";
        d["depth"] = "5 mm";
        return d;
    }
protected:
    bool bothSides() const override { return true; }
};
