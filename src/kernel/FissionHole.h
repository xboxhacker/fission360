// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#pragma once
#include "FissionFeature.h"
#include <gp_Ax3.hxx>

namespace ThreadUtils {
// ISO metric coarse pitch for a nominal diameter (nearest table entry)
double coarsePitch(double diameter);

// Standard thread sizes: ISO metric coarse / fine (ISO 261) and Unified UNC / UNF (ASME B1.1), with clearance
// hole diameters (ISO 273 fine / medium / coarse; ASME B18.2.8 close / normal / loose). All values in mm.
struct ThreadSize {
    QString standard;                  // "isoCoarse" | "isoFine" | "unc" | "unf"
    QString name;                      // "M8", "M8x1", "1/4-20", "#10-32"
    double major = 0;
    double pitch = 0;
    double clearance[3] = {0, 0, 0};   // close, normal, loose
    double tapDrill() const { return major - 1.0825 * pitch; }   // basic minor diameter
    QString designation() const;       // "M8x1.25", "1/4-20 UNC"
};
QList<ThreadSize> threadSizes(const QString &standard);
bool findThread(const QString &standard, const QString &name, ThreadSize &out);
bool nearestThread(const QString &standard, double major, ThreadSize &out);
// fit "close" | "normal" | "loose" → index into ThreadSize::clearance
int fitIndex(const QString &fit);
// Solid of a 60° thread groove swept along a helix. axis: origin at the thread start, Z along the thread.
// internal = the material is outside the radius (tapped hole), else inside (bolt).
TopoDS_Shape threadTool(const gp_Ax3 &axis, double radius, double length, double pitch, bool internal);
}

// Holes drilled into the body under a face or at sketch points.
// props:
//   face       : face ref (flat) — with center [x, y, z] on it (empty = face centroid)
//   sketch     : sketch id — a hole at every sketch point instead (overrides face)
//   holeType   : "simple" | "counterbore" | "countersink"
//   diameter, extent ("distance" | "all"), depth, tipAngle (0 = flat bottom), flip
//   cboreDiameter, cboreDepth, csinkDiameter, csinkAngle
//   tapped (bool), pitch ("" = metric coarse), modeled (bool: cut the thread, else cosmetic)
//   standard   : "custom" (diameter / pitch as typed) | "isoCoarse" | "isoFine" | "unc" | "unf"
//   size       : size name in the standard ("M8", "M8x1", "1/4-20"); sets the diameter (tap drill when tapped)
//                and the pitch
//   fit        : clearance hole fit for an untapped standard size, "close" | "normal" | "loose"
class FissionHole : public FissionFeature {
    Q_OBJECT
    Q_PROPERTY(QString designation READ designation NOTIFY statusChanged)
public:
    explicit FissionHole(QObject *parent = nullptr) : FissionFeature(parent) {}
    QString type() const override { return QStringLiteral("Hole"); }
    QVariantMap defaults() const override {
        return {{"face", QVariant()}, {"center", QVariantList()}, {"sketch", ""}, {"holeType", "simple"},
                {"diameter", "6 mm"}, {"extent", "distance"}, {"depth", "10 mm"}, {"tipAngle", "118 deg"},
                {"flip", false}, {"cboreDiameter", "10 mm"}, {"cboreDepth", "3 mm"}, {"csinkDiameter", "10 mm"},
                {"csinkAngle", "90 deg"}, {"tapped", false}, {"pitch", ""}, {"modeled", true},
                {"standard", "custom"}, {"size", ""}, {"fit", "normal"}};
    }
    QStringList expressionKeys() const override {
        return {"diameter", "depth", "tipAngle", "cboreDiameter", "cboreDepth", "csinkDiameter", "csinkAngle", "pitch"};
    }
    QString designation() const { return m_designation; }
protected:
    bool compute(RebuildContext &ctx) override;
    void resetResult() override { m_designation.clear(); }
private:
    QString m_designation;   // standard sizes: "M8x1.25 tapped", "M8 clearance, normal Ø9.00 mm"
    // [[x, y, z, label], ...]: face center, circle centers and vertices of the face, points of shown sketches
    // in its plane
    QVariantList faceSnaps(const RebuildContext &ctx, const gp_Ax3 &plane) const;
};

// Thread on a cylindrical face (outside of a shaft or inside of a hole).
// props: face (cylindrical face ref), pitch ("" = metric coarse), length ("" = whole face), offset,
//        modeled (bool: cut the groove; false = cosmetic, geometry unchanged),
//        standard ("custom" | "isoCoarse" | "isoFine" | "unc" | "unf"), size ("" = the size that fits the face)
class FissionThread : public FissionFeature {
    Q_OBJECT
    Q_PROPERTY(QString designation READ designation NOTIFY statusChanged)
public:
    explicit FissionThread(QObject *parent = nullptr) : FissionFeature(parent) {}
    QString type() const override { return QStringLiteral("Thread"); }
    QVariantMap defaults() const override {
        return {{"face", QVariant()}, {"pitch", ""}, {"length", ""}, {"offset", "0 mm"}, {"modeled", true},
                {"standard", "custom"}, {"size", ""}};
    }
    QStringList expressionKeys() const override { return {"pitch", "length", "offset"}; }
    QString designation() const { return m_designation; }
protected:
    bool compute(RebuildContext &ctx) override;
    void resetResult() override { m_designation.clear(); }
private:
    QString m_designation;   // e.g. "M10x1.5 external"
};
