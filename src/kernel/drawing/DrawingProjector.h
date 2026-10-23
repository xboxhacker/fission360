// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#pragma once
#include <QByteArray>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <TopoDS_Shape.hxx>
#include <gp_Ax2.hxx>

class FissionDocument;

// Orthographic / HLR projection of a design onto a drawing view plane.
namespace DrawingProjector {

struct Result {
    QVariantList visible;   // [{x1,y1,x2,y2}, ...] in mm about the view centre
    QVariantList hidden;
    QVariantList section;   // section cut curves (section views)
    // One entry per projected edge, which segments point at with "e": {type "line" | "circle" | "curve",
    // x1,y1,x2,y2 (end points), cx,cy,r,full for circles, hidden}
    QVariantList edges;
    double width = 0;       // bounding box of the projection (mm)
    double height = 0;
    bool ok = false;
    QString error;
};

// View directions matching DrawingView::orientation.
// Orthographic names plus eight isometric corners (isoFrontTopRight … isoBackBottomLeft).
// "isometric" and "home" share the back-top-right projector.
gp_Ax2 viewAxis(const QString &orientation);

// Project every body in the design (or a single shape) into 2D line segments.
Result project(FissionDocument *design, const QString &orientation, bool showHidden = false);
Result projectShape(const TopoDS_Shape &shape, const QString &orientation, bool showHidden = false);

// Shaded picture of the solid along viewAxis, as a PNG. Uses the shape's triangulation
// (meshed on demand) and a Lambert shade. Empty when the shape has no triangles.
QByteArray renderShadedPng(const TopoDS_Shape &shape, const QString &orientation, int longEdgePx = 420);
QByteArray renderShadedPng(FissionDocument *design, const QString &orientation, int longEdgePx = 420);

// Resolve a projected-view orientation from a parent orientation and a sheet direction
// (up/down/left/right) under first- or third-angle projection.
QString projectedOrientation(const QString &parentOrientation, const QString &direction,
                             const QString &projectionAngle);

// A section view uses the parent orientation with a cutting plane normal in the sheet plane.
// direction: "right" | "left" | "up" | "down" — which way the section looks after the cut.
Result projectSection(FissionDocument *design, const QString &parentOrientation,
                      const QString &direction, double cutOffsetMm = 0);

}   // namespace DrawingProjector
