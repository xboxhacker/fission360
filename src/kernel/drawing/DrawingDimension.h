// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#pragma once
#include <QString>
#include <QVariantList>
#include <QVariantMap>

// Drawing dimensions: built from picks on projected model edges (like Fusion's ViewCurve), and drawn the same
// way on the sheet, in PDF / PNG / TIFF and in DXF. All coordinates are sheet mm from the top left, y down.
//
// Annotation {kind "dimension", dimType, viewScale, decimals, text (override, "" = measured), fontSize}:
//   linear   p1x,p1y, p2x,p2y, axis "h" | "v" | "a" (aligned with p1 -> p2), px,py (where the line sits)
//   radius / diameter   cx,cy, r, px,py (where the text sits)
//   angular  vx,vy (vertex), d1x,d1y, d2x,d2y (unit rays), px,py (arc radius and text side)
// Dimensions saved before 1.13.39 (x,y,x2,y2,text) are drawn as an aligned linear dimension.
namespace DrawingDimension {

// tool: "dimension" (picks decide), "linear", "aligned", "angular", "radius", "diameter".
// a, b: DrawingView::pickEdge results (b may be empty); place: where the user clicked to put it.
// Returns the annotation, or {error} when the picks do not make that dimension.
QVariantMap build(const QString &tool, const QVariantMap &a, const QVariantMap &b, double placeX, double placeY,
                  double viewScale);

// Model value (mm or degrees) and the text shown ("12.5", "R6", "Ø20", "90°")
double value(const QVariantMap &ann);
QString label(const QVariantMap &ann);

// What to draw: {lines [[x1,y1,x2,y2]], arcs [[cx,cy,r,startDeg,sweepDeg]], arrows [[tipX,tipY,dirX,dirY]],
// text {x,y,angle,str,size}} with angles in degrees, clockwise on the sheet (y down)
QVariantMap geometry(const QVariantMap &ann);

// Sheet distance from a point to the dimension's text and lines, for picking it
double distanceTo(const QVariantMap &ann, double x, double y);

// A view's placement on the sheet: sheet = (ox + x * s, oy - y * s) for a view point (x, y), y up
struct ViewFrame {
    double ox = 0, oy = 0, s = 1;
};
// Dimensions attached to a view ({space "view", view <id>}) keep their points, picks and placement in the
// view's model mm, so they move with the view and can be re-attached when it is projected again.
QVariantMap toView(const QVariantMap &sheetAnn, const QVariantList &sheetPicks, const ViewFrame &f);
QVariantMap toSheet(const QVariantMap &viewAnn, const ViewFrame &f);
// After a new projection: find each pick again among the view's edges (same kind, nearest) and rebuild the
// dimension there. False (and the dimension unchanged) when a pick has no match.
bool reattach(QVariantMap &viewAnn, const QVariantList &edges);

}  // namespace DrawingDimension
