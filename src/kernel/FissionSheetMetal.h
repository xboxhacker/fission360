// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#pragma once
#include "FissionFeature.h"
#include <QPair>
#include <TopoDS_Face.hxx>
#include <gp_Ax1.hxx>
#include <gp_Dir.hxx>
#include <gp_Trsf.hxx>

// SHEET METAL (phase 7). The sheet metal rule is three user parameters the document creates on first use:
// SheetThickness, BendRadius (inner) and KFactor; the features default to those names.
//
// Every sheet feature reports what the flat pattern needs: the base sheet it started from, each bend it
// made (the end face the bend grew from, the fold and the material beyond it) and plain additions such as
// tabs. The flat pattern walks them in timeline order, so flanges on flanges unfold too.

// One bend, in the folded coordinates of the moment it was made
struct SheetBend {
    QString body;
    TopoDS_Face side;              // the sheet's end face the bend grew from
    gp_Dir out = gp_Dir(1, 0, 0);  // outward normal of side (in the sheet plane)
    gp_Dir up = gp_Dir(0, 0, 1);   // normal of the sheet face the bend edge lies on
    gp_Ax1 axis;                   // fold axis
    double signedAngle = 0;        // rotation about axis
    gp_Trsf rot;                   // the fold
    double angle = 0, radius = 0, thickness = 0;
    TopoDS_Shape segment;          // folded material beyond the bend (null when there is none)
    bool consumes = false;         // Bend: the material was already part of the sheet, so the flat keeps it
    double allowance = 0;          // consumes: bend allowance the material was cut with
    TopoDS_Shape keep;             // consumes: half-space of the parent that stays where it was
};

// Mixin for the sheet features (dynamic_cast from FissionFeature)
class SheetMetalPart {
public:
    virtual ~SheetMetalPart() = default;
    virtual QString sheetBody() const = 0;
    virtual TopoDS_Shape baseSheet() const { return {}; }
    virtual QList<SheetBend> bends() const { return {}; }
    virtual QList<TopoDS_Shape> additions() const { return {}; }
};

// The sheet unfolded: flat solid, the rigid pieces and bend strips it was made of (for Refold), bend lines
struct SheetFlat {
    struct Region { TopoDS_Shape folded; gp_Trsf toFlat; };
    struct Strip {
        TopoDS_Shape flat;          // the strip in flat coordinates
        gp_Trsf parentToFlat;       // folded coordinates of the bend's parent -> flat
        gp_Pnt origin;              // a point of the side plane (parent folded coordinates)
        gp_Dir out = gp_Dir(1, 0, 0);
        gp_Ax1 axis;
        double signedAngle = 0, allowance = 0;
    };
    TopoDS_Shape flat;
    QList<Region> regions;
    QList<Strip> strips;
    QList<QPair<gp_Pnt, gp_Pnt>> bendLines;
    gp_Dir normal = gp_Dir(0, 0, 1);
    int unfolded = 0, skipped = 0;
};

// Base flange: a sketch profile (or flat faces) thickened into a new sheet body.
// props: sketch, profiles, thickness, flip
class FissionBaseFlange : public FissionFeature, public SheetMetalPart {
    Q_OBJECT
public:
    explicit FissionBaseFlange(QObject *parent = nullptr) : FissionFeature(parent) {}
    QString type() const override { return QStringLiteral("BaseFlange"); }
    QVariantMap defaults() const override {
        return {{"sketch", ""}, {"profiles", QVariantList()}, {"thickness", "SheetThickness"}, {"flip", false}};
    }
    QStringList expressionKeys() const override { return {"thickness"}; }
    TopoDS_Shape sheet() const { return m_sheet; }
    QString bodyName() const { return m_body; }
    QString sheetBody() const override { return m_body; }
    TopoDS_Shape baseSheet() const override { return m_sheet; }
protected:
    bool compute(RebuildContext &ctx) override;
    void resetResult() override { m_sheet.Nullify(); m_body.clear(); }
private:
    TopoDS_Shape m_sheet;
    QString m_body;
};

// Edge flange: bends a flange up (or down, flip) from a straight edge of a sheet face.
// props: edge (edge ref), height (flat length after the bend), angle (deg), bendRadius, flip
class FissionEdgeFlange : public FissionFeature, public SheetMetalPart {
    Q_OBJECT
public:
    explicit FissionEdgeFlange(QObject *parent = nullptr) : FissionFeature(parent) {}
    QString type() const override { return QStringLiteral("EdgeFlange"); }
    QVariantMap defaults() const override {
        return {{"edge", QVariant()}, {"height", "10 mm"}, {"angle", "90 deg"}, {"bendRadius", "BendRadius"}, {"flip", false}};
    }
    QStringList expressionKeys() const override { return {"height", "angle", "bendRadius"}; }
    QString sheetBody() const override { return m_bend.body; }
    QList<SheetBend> bends() const override { return m_valid ? QList<SheetBend>{m_bend} : QList<SheetBend>{}; }
protected:
    bool compute(RebuildContext &ctx) override;
    void resetResult() override { m_bend = SheetBend(); m_valid = false; }
    // The flange from the picked edge: shared by Edge Flange and Hem
    bool flangeFromEdge(RebuildContext &ctx, double height, double angleRad, double radius, bool down,
                        const QString &manipulatorKey);
private:
    SheetBend m_bend;
    bool m_valid = false;
};

// Hem: the sheet edge folded back on itself. props: edge, hemType (flat | open | rolled), length, gap,
// radius, angle (rolled), flip
class FissionHem : public FissionEdgeFlange {
    Q_OBJECT
public:
    explicit FissionHem(QObject *parent = nullptr) : FissionEdgeFlange(parent) {}
    QString type() const override { return QStringLiteral("Hem"); }
    QVariantMap defaults() const override {
        return {{"edge", QVariant()}, {"hemType", "flat"}, {"length", "6 mm"}, {"gap", "0.2 mm"},
                {"radius", "SheetThickness"}, {"angle", "270 deg"}, {"flip", false}};
    }
    QStringList expressionKeys() const override { return {"length", "gap", "radius", "angle"}; }
protected:
    bool compute(RebuildContext &ctx) override;
};

// Tab: a sketch profile added to a sheet in its own plane, the sheet's thickness deep. props: sketch,
// profiles, thickness, flip (other side of the sketch plane)
class FissionTab : public FissionFeature, public SheetMetalPart {
    Q_OBJECT
public:
    explicit FissionTab(QObject *parent = nullptr) : FissionFeature(parent) {}
    QString type() const override { return QStringLiteral("Tab"); }
    QVariantMap defaults() const override {
        return {{"sketch", ""}, {"profiles", QVariantList()}, {"thickness", "SheetThickness"}, {"flip", false}};
    }
    QStringList expressionKeys() const override { return {"thickness"}; }
    QString sheetBody() const override { return m_body; }
    QList<TopoDS_Shape> additions() const override { return m_tab.IsNull() ? QList<TopoDS_Shape>{} : QList<TopoDS_Shape>{m_tab}; }
protected:
    bool compute(RebuildContext &ctx) override;
    void resetResult() override { m_tab.Nullify(); m_body.clear(); }
private:
    TopoDS_Shape m_tab;
    QString m_body;
};

// Bend: folds a sheet along a sketch line on one of its faces; the flat length is kept (the bend uses up
// sheet beyond the line). props: line ("sketchline:<sketch>:<entity>"), angle, bendRadius, kFactor,
// flip (the other side moves), down (bend away from the face the line is on)
class FissionBend : public FissionFeature, public SheetMetalPart {
    Q_OBJECT
public:
    explicit FissionBend(QObject *parent = nullptr) : FissionFeature(parent) {}
    QString type() const override { return QStringLiteral("Bend"); }
    QVariantMap defaults() const override {
        return {{"line", ""}, {"angle", "90 deg"}, {"bendRadius", "BendRadius"}, {"kFactor", "KFactor"},
                {"flip", false}, {"down", false}};
    }
    QStringList expressionKeys() const override { return {"angle", "bendRadius", "kFactor"}; }
    QString sheetBody() const override { return m_bend.body; }
    QList<SheetBend> bends() const override { return m_valid ? QList<SheetBend>{m_bend} : QList<SheetBend>{}; }
protected:
    bool compute(RebuildContext &ctx) override;
    void resetResult() override { m_bend = SheetBend(); m_valid = false; }
private:
    SheetBend m_bend;
    bool m_valid = false;
};

// Contour flange: an open chain of sketch lines thickened into a sheet with a bend at every corner, run
// out along the sketch normal. props: sketch, distance, thickness, bendRadius, flip (thickness on the
// other side of the lines), reverse (run out the other way)
class FissionContourFlange : public FissionFeature, public SheetMetalPart {
    Q_OBJECT
public:
    explicit FissionContourFlange(QObject *parent = nullptr) : FissionFeature(parent) {}
    QString type() const override { return QStringLiteral("ContourFlange"); }
    QVariantMap defaults() const override {
        return {{"sketch", ""}, {"distance", "20 mm"}, {"thickness", "SheetThickness"}, {"bendRadius", "BendRadius"},
                {"flip", false}, {"reverse", false}};
    }
    QStringList expressionKeys() const override { return {"distance", "thickness", "bendRadius"}; }
    QString sheetBody() const override { return m_body; }
    TopoDS_Shape baseSheet() const override { return m_first; }
    QList<SheetBend> bends() const override { return m_bends; }
protected:
    bool compute(RebuildContext &ctx) override;
    void resetResult() override { m_first.Nullify(); m_bends.clear(); m_body.clear(); }
private:
    TopoDS_Shape m_first;
    QList<SheetBend> m_bends;
    QString m_body;
};

// Flat pattern: the sheet body unfolded (bends become flat strips of length angle * (R + K * T)), added as a
// new body next to the folded one. props: body (index), kFactor
class FissionFlatPattern : public FissionFeature {
    Q_OBJECT
    Q_PROPERTY(QString summary READ summary NOTIFY statusChanged)
public:
    explicit FissionFlatPattern(QObject *parent = nullptr) : FissionFeature(parent) {}
    QString type() const override { return QStringLiteral("FlatPattern"); }
    QVariantMap defaults() const override { return {{"body", -1}, {"kFactor", "KFactor"}}; }
    QStringList expressionKeys() const override { return {"kFactor"}; }
    QString summary() const { return m_summary; }
    // For DXF export: the laid-out flat body and its bend lines
    const SheetFlat &flat() const { return m_flat; }
protected:
    bool compute(RebuildContext &ctx) override;
    void resetResult() override { m_summary.clear(); m_flat = SheetFlat(); }
private:
    QString m_summary;
    SheetFlat m_flat;
};

// Unfold: the sheet body is laid flat in place, so cuts can be made across bends; Refold folds it back.
// props: body (index), kFactor
class FissionUnfold : public FissionFeature {
    Q_OBJECT
    Q_PROPERTY(QString summary READ summary NOTIFY statusChanged)
public:
    explicit FissionUnfold(QObject *parent = nullptr) : FissionFeature(parent) {}
    QString type() const override { return QStringLiteral("Unfold"); }
    QVariantMap defaults() const override { return {{"body", -1}, {"kFactor", "KFactor"}}; }
    QStringList expressionKeys() const override { return {"kFactor"}; }
    QString summary() const { return m_summary; }
    QString bodyName() const { return m_body; }
    TopoDS_Shape folded() const { return m_folded; }
    const SheetFlat &flat() const { return m_flat; }
protected:
    bool compute(RebuildContext &ctx) override;
    void resetResult() override { m_summary.clear(); m_body.clear(); m_folded.Nullify(); m_flat = SheetFlat(); }
private:
    QString m_summary, m_body;
    TopoDS_Shape m_folded;
    SheetFlat m_flat;
};

// Refold: folds an unfolded sheet back; cuts and additions made while it was flat follow the bends.
// props: unfold (feature id; "" = the latest Unfold before this)
class FissionRefold : public FissionFeature {
    Q_OBJECT
    Q_PROPERTY(QString summary READ summary NOTIFY statusChanged)
public:
    explicit FissionRefold(QObject *parent = nullptr) : FissionFeature(parent) {}
    QString type() const override { return QStringLiteral("Refold"); }
    QVariantMap defaults() const override { return {{"unfold", ""}}; }
    QString summary() const { return m_summary; }
protected:
    bool compute(RebuildContext &ctx) override;
    void resetResult() override { m_summary.clear(); }
private:
    QString m_summary;
};
