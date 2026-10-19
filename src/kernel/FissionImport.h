// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#pragma once
#include "FissionFeature.h"

// STEP / IGES import. The B-rep is stored in the design (compressed BRepTools text), so the file is no
// longer needed after the import. Every solid becomes a body named after its part (with the assembly path,
// "Assembly/Part"); loose shells / faces are kept as one body.
// props: data, fileName, format "STEP" | "IGES", parts [{name, solidName, component, color "#rrggbb" or ""}]
// per solid in order
class FissionImportCAD : public FissionFeature {
    Q_OBJECT
public:
    explicit FissionImportCAD(QObject *parent = nullptr) : FissionFeature(parent) {}
    QString type() const override { return QStringLiteral("ImportCAD"); }
    QVariantMap defaults() const override { return {{"data", ""}, {"fileName", ""}, {"format", "STEP"}}; }

    // Reads a .step / .stp / .iges / .igs file (in mm) with its product structure; returns a null shape with
    // error set on failure. parts receives a name and colour for every solid of the result, in order.
    static TopoDS_Shape readFile(const QString &path, QString &format, QString &error, QVariantList *parts = nullptr);
    // Fusion per-step STEP (base64) → shape, without leaving a permanent temp file
    static TopoDS_Shape readStepBase64(const QString &b64, QString &error, QVariantList *parts = nullptr);
    // Fusion body names from readFile parts (the solids' own names), made unique; "" where a solid has none
    static QStringList fusionBodyNames(const QVariantList &parts);
    static QString encode(const TopoDS_Shape &shape);
    static TopoDS_Shape decode(const QString &data);

protected:
    bool compute(RebuildContext &ctx) override;
};
