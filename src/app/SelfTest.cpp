// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#include "SelfTest.h"
#include "api/PythonHost.h"
#include "api/ScriptHost.h"
#include <QRegularExpression>
#include "kernel/Expression.h"
#include "kernel/FissionDocument.h"
#include "kernel/render/AppearanceLibrary.h"
#include "kernel/render/HdrImage.h"
#include "kernel/render/PathTracer.h"
#include "kernel/render/RenderAssetStore.h"
#include "kernel/render/RenderEnvironmentLibrary.h"
#include "kernel/mcmaster/McMasterCarrClient.h"
#include "kernel/FissionMeshFeatures.h"
#include "kernel/FissionProgress.h"
#include "kernel/FissionSketch.h"
#include "kernel/GeomUtils.h"
#include "kernel/MeshUtils.h"
#include "kernel/DocumentVersionStore.h"
#include "kernel/drawing/DrawingManager.h"
#include "kernel/drawing/DrawingDocument.h"
#include "kernel/drawing/DrawingProjector.h"
#include "kernel/drawing/DrawingSheet.h"
#include "kernel/drawing/DrawingSketch.h"
#include "kernel/drawing/DrawingView.h"
#include "ui/BRepRenderer.h"
#include "ui/ViewportController.h"
#include "util/GifWriter.h"

#include <QColor>
#include <QDir>
#include <QImage>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <limits>
#include <QFile>
#include <QFileInfo>
#include <QJSEngine>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointF>
#include <QTemporaryDir>
#include <QVector3D>
#include <BRepAdaptor_Surface.hxx>
#include <BRepClass3d_SolidClassifier.hxx>
#include <BRepMesh_IncrementalMesh.hxx>
#undef near   // windows.h, via OCCT
#include <BRepGProp.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <TopExp_Explorer.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeSphere.hxx>
#include <IFSelect_ReturnStatus.hxx>
#include <Quantity_Color.hxx>
#include <BRep_Builder.hxx>
#include <STEPCAFControl_Writer.hxx>
#include <STEPControl_Writer.hxx>
#include <TopoDS_Compound.hxx>
#include <TDataStd_Name.hxx>
#include <TDocStd_Document.hxx>
#include <XCAFApp_Application.hxx>
#include <XCAFDoc_ColorTool.hxx>
#include <XCAFDoc_DocumentTool.hxx>
#include <XCAFDoc_ShapeTool.hxx>
#include <BRepAdaptor_Curve.hxx>
#include <BndLib_Add3dCurve.hxx>
#include <Bnd_Box.hxx>
#include <GProp_GProps.hxx>
#include <TopoDS.hxx>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>

namespace {

int g_failures = 0;
int g_passes = 0;

void check(bool ok, const QString &name, const QString &detail = QString()) {
    if (ok) ++g_passes; else ++g_failures;
    std::printf("[%s] %s%s\n", ok ? "PASS" : "FAIL", qPrintable(name),
                detail.isEmpty() ? "" : qPrintable("  (" + detail + ")"));
    std::fflush(stdout);
}

bool near(double a, double b, double relTol = 1e-3) {
    return std::fabs(a - b) <= relTol * std::max(1.0, std::fabs(b));
}

double totalVolume(FissionDocument *doc) {
    double v = 0;
    for (auto *b : doc->bodies()) v += b->volume();
    return v;
}

QString status(FissionDocument *doc) {
    QStringList errs;
    for (auto *f : doc->timeline()->features())
        if (!f->isHealthy()) errs << QString("%1: %2").arg(f->name(), f->error());
    return QString("bodies=%1 volume=%2 %3").arg(doc->bodyCount()).arg(totalVolume(doc), 0, 'f', 3).arg(errs.join("; "));
}

void checkVolume(FissionDocument *doc, double expected, const QString &name, double tol = 1e-3) {
    const double v = totalVolume(doc);
    check(near(v, expected, tol), name, QString("expected %1, %2").arg(expected, 0, 'f', 3).arg(status(doc)));
}

FissionSketch *rectSketch(FissionDocument *doc, const QVariant &plane, const QVariant &x, const QVariant &y,
                          const QVariant &w, const QVariant &h) {
    auto *s = qobject_cast<FissionSketch *>(doc->addFeature("Sketch", {{"plane", plane}}));
    s->addRectangle(x, y, w, h);
    doc->rebuild();
    return s;
}

FissionFeature *extrude(FissionDocument *doc, FissionSketch *s, const QVariant &dist, const QString &op = "join",
                        const QVariantMap &extra = {}) {
    QVariantMap props{{"sketch", s->id()}, {"distance", dist}, {"operation", op}};
    for (auto it = extra.begin(); it != extra.end(); ++it) props.insert(it.key(), it.value());
    return doc->addFeature("Extrude", props);
}

void testExpressions() {
    QString err;
    check(near(Expression::evaluate("2 * (3 + 4)", &err), 14), "expr: arithmetic");
    check(near(Expression::evaluate("1 in", &err), 25.4), "expr: inch unit");
    check(near(Expression::evaluate("2cm + 5 mm", &err), 25), "expr: mixed units");
    check(near(Expression::evaluate("sqrt(3^2 + 4^2)", &err), 5), "expr: functions and power");
    check(near(Expression::evaluate("cos(60 deg)", &err), 0.5), "expr: trig in degrees");
    Expression::evaluate("2 +", &err);
    check(!err.isEmpty(), "expr: syntax error reported", err);
    check(Expression::isValidName("width_1") && !Expression::isValidName("mm") && !Expression::isValidName("1a"),
          "expr: parameter name validation");
}

void testExtrudeAndParameters() {
    FissionDocument doc;
    doc.addParameter("width", "40 mm");
    auto *s = rectSketch(&doc, "XY", 0, 0, "width", 20);
    check(s->isHealthy() && s->profileCount() == 1, "sketch: rectangle gives one profile", status(&doc));
    auto *ex = extrude(&doc, s, "10 mm");
    check(ex && ex->isHealthy(), "extrude: healthy", status(&doc));
    checkVolume(&doc, 40 * 20 * 10, "extrude: volume 40x20x10");
    {
        const QVariantMap m = ex->manipulator();
        const QVector3D o = m.value("origin").value<QVector3D>(), d = m.value("direction").value<QVector3D>();
        check(m.value("key") == "distance" && near(m.value("value").toDouble(), 10) && near(d.z(), 1)
                  && near(o.x(), 20) && near(o.y(), 10) && near(o.z(), 0),
              "manipulator: extrude arrow at the profile centroid along +Z",
              QString("origin (%1, %2, %3) dir z %4").arg(o.x()).arg(o.y()).arg(o.z()).arg(d.z()));
    }

    doc.parameter("width")->setExpression("60 mm");
    doc.rebuild();
    checkVolume(&doc, 60 * 20 * 10, "parameters: editing width rebuilds the timeline");

    ex->set("extent", "symmetric");
    doc.rebuild();
    checkVolume(&doc, 60 * 20 * 20, "extrude: symmetric");
    ex->set("extent", "twoSides");
    ex->set("distance2", "5 mm");
    doc.rebuild();
    checkVolume(&doc, 60 * 20 * 15, "extrude: two sides");
    ex->set("extent", "distance");
    ex->set("taper", "5 deg");
    doc.rebuild();
    const double tapered = totalVolume(&doc);
    check(ex->isHealthy() && tapered < 60 * 20 * 10 && tapered > 0.7 * 60 * 20 * 10, "extrude: taper angle", status(&doc));
    ex->set("taper", "0 deg");
    doc.rebuild();

    auto *hole = qobject_cast<FissionSketch *>(doc.addFeature("Sketch", {{"plane", "XY"}}));
    hole->addCircle(30, 10, 4);
    doc.rebuild();
    extrude(&doc, hole, 10, "cut", {{"extent", "throughAll"}});
    checkVolume(&doc, 60 * 20 * 10 - M_PI * 16 * 10, "extrude: through-all cut");

    ex->setSuppressed(true);
    doc.rebuild();
    check(doc.bodyCount() == 0, "timeline: suppressing the base extrude removes the body", status(&doc));
    ex->setSuppressed(false);
    doc.timeline()->setMarker(3);
    doc.rebuild();
    checkVolume(&doc, 60 * 20 * 10, "timeline: rolling the marker back skips later features");
    doc.timeline()->setMarker(doc.timeline()->count());
    doc.rebuild();

    doc.undo();
    check(doc.timeline()->count() == 3, "undo: removes the last added feature", status(&doc));
    doc.redo();
    check(doc.timeline()->count() == 4, "redo: restores it", status(&doc));
    checkVolume(&doc, 60 * 20 * 10 - M_PI * 16 * 10, "redo: model matches");

    doc.parameter("width")->setExpression("height * 2");
    doc.rebuild();
    check(!doc.parameter("width")->error().isEmpty(), "parameters: unknown name reported",
          doc.parameter("width")->error());
}

void testJoinMultiProfileExtrude() {
    // Two abutting rectangles extruded with Join must be one body (not Body1+Body2)
    FissionDocument doc;
    auto *s = qobject_cast<FissionSketch *>(doc.addFeature("Sketch", {{"plane", "XY"}}));
    s->addRectangle(0, 0, 20, 20);
    s->addRectangle(20, 0, 20, 20);
    doc.rebuild();
    check(s->profileCount() >= 2, "join multi-profile: two sketch regions",
          QString("profiles=%1").arg(s->profileCount()));
    auto *ex = doc.addFeature("Extrude",
                              {{"sketch", s->id()},
                               {"profiles", QVariantList{0, 1}},
                               {"distance", "10 mm"},
                               {"operation", "join"}});
    check(ex && ex->isHealthy() && doc.bodyCount() == 1, "join multi-profile: Join makes one body",
          QString("bodies=%1 %2").arg(doc.bodyCount()).arg(ex ? ex->error() : "no feature"));
    checkVolume(&doc, 40 * 20 * 10, "join multi-profile: volume of the fused solid");

    FissionDocument docNew;
    auto *s2 = qobject_cast<FissionSketch *>(docNew.addFeature("Sketch", {{"plane", "XY"}}));
    s2->addRectangle(0, 0, 20, 20);
    s2->addRectangle(20, 0, 20, 20);
    docNew.rebuild();
    auto *nb = docNew.addFeature("Extrude",
                                 {{"sketch", s2->id()},
                                  {"profiles", QVariantList{0, 1}},
                                  {"distance", "10 mm"},
                                  {"operation", "newBody"}});
    check(nb && nb->isHealthy() && docNew.bodyCount() == 2, "join multi-profile: New Body keeps two bodies",
          QString("bodies=%1 %2").arg(docNew.bodyCount()).arg(nb ? nb->error() : "no feature"));
}

void testProfilesWithHoles() {
    FissionDocument doc;
    auto *s = qobject_cast<FissionSketch *>(doc.addFeature("Sketch", {{"plane", "XY"}}));
    s->addRectangle(-20, -20, 40, 40);
    s->addCircle(0, 0, 10);
    doc.rebuild();
    check(s->profileCount() == 2 && s->defaultProfiles().size() == 1, "sketch: ring and disk regions found",
          QString("profiles=%1 default=%2").arg(s->profileCount()).arg(s->defaultProfiles().size()));
    extrude(&doc, s, 5);
    checkVolume(&doc, (1600 - M_PI * 100) * 5, "extrude: default profile keeps the hole");

    auto *s2 = qobject_cast<FissionSketch *>(doc.addFeature("Sketch", {{"plane", "XY"}}));
    s2->addRectangle(100, 0, 20, 20);
    s2->addCircle(120, 20, 10);
    doc.rebuild();
    check(s2->profileCount() == 3, "sketch: overlapping curves split into regions",
          QString("profiles=%1").arg(s2->profileCount()));
    s2->addSlot(200, 0, 240, 0, 10);
    s2->addPolygon(300, 0, 10, 6);
    s2->addEllipse(400, 0, 20, 10);
    doc.rebuild();
    check(s2->isHealthy() && s2->profileCount() == 6, "sketch: slot, polygon and ellipse profiles",
          QString("profiles=%1 %2").arg(s2->profileCount()).arg(s2->error()));
}

void testFilletChamferShell() {
    FissionDocument doc;
    auto *s = rectSketch(&doc, "XY", 0, 0, 40, 20);
    extrude(&doc, s, 10);
    const TopoDS_Shape box = doc.body(0)->shape();

    auto *fillet = doc.addFeature("Fillet", {{"allEdges", true}, {"body", 0}, {"radius", "2 mm"}});
    check(fillet->isHealthy() && totalVolume(&doc) < 8000 && totalVolume(&doc) > 7500, "fillet: all edges",
          status(&doc));
    doc.setInputFeatureId(fillet->id());
    check(doc.showingInput() && doc.displayBodyCount() == 1 && GeomUtils::faceMap(doc.displayShape(0)).Extent() == 6
              && GeomUtils::faceMap(doc.body(0)->shape()).Extent() > 6,
          "input view: shows the pre-fillet box while the fillet result stays", status(&doc));
    doc.setInputFeatureId(QString());
    check(!doc.showingInput() && GeomUtils::faceMap(doc.displayShape(0)).Extent() > 6, "input view: cleared",
          status(&doc));
    doc.removeFeature(fillet);

    // Pick the edge along X at y=0, z=10 by its midpoint
    const auto edges = GeomUtils::edgeMap(box);
    int edgeIndex = -1;
    for (int i = 1; i <= edges.Extent(); ++i) {
        const gp_Pnt m = GeomUtils::edgeMidpoint(TopoDS::Edge(edges(i)));
        if (near(m.X(), 20) && near(m.Y(), 0) && near(m.Z(), 10)) edgeIndex = i - 1;
    }
    const QVariantMap ref = GeomUtils::edgeRef(0, box, edgeIndex);
    auto *chamfer = doc.addFeature("Chamfer", {{"edges", QVariantList{ref}}, {"distance", "2 mm"}});
    checkVolume(&doc, 8000 - 2.0 * 2.0 / 2.0 * 40, "chamfer: equal distance on one edge");
    {
        const QVariantMap m = chamfer->manipulator();
        const QVector3D o = m.value("origin").value<QVector3D>(), d = m.value("direction").value<QVector3D>();
        check(m.value("key") == "distance" && near(o.x(), 20) && near(o.y(), 0) && near(o.z(), 10)
                  && near(d.y(), -M_SQRT1_2) && near(d.z(), M_SQRT1_2),
              "manipulator: chamfer handle on the edge, pointing out of the corner",
              QString("origin (%1, %2, %3) dir (%4, %5, %6)").arg(o.x()).arg(o.y()).arg(o.z())
                  .arg(d.x()).arg(d.y()).arg(d.z()));
    }
    chamfer->set("chamferType", "twoDistances");
    chamfer->set("distance2", "4 mm");
    doc.rebuild();
    checkVolume(&doc, 8000 - 2.0 * 4.0 / 2.0 * 40, "chamfer: two distances");
    doc.removeFeature(chamfer);

    auto *f1 = doc.addFeature("Fillet", {{"edges", QVariantList{ref}}, {"radius", "3 mm"}});
    checkVolume(&doc, 8000 - (9 - M_PI * 9 / 4) * 40, "fillet: single edge radius 3");
    doc.removeFeature(f1);

    // Shell with the top face (z = 10) open
    const auto faces = GeomUtils::faceMap(box);
    int top = -1;
    for (int i = 1; i <= faces.Extent(); ++i)
        if (near(GeomUtils::faceCentroid(TopoDS::Face(faces(i))).Z(), 10)) top = i - 1;
    doc.addFeature("Shell", {{"faces", QVariantList{GeomUtils::faceRef(0, box, top)}}, {"thickness", "1 mm"}});
    checkVolume(&doc, 8000 - 38 * 18 * 9, "shell: open top, 1 mm walls");
}

void testRevolveLoftSweep() {
    {
        FissionDocument doc;
        auto *s = rectSketch(&doc, "XZ", 10, 0, 10, 10);
        doc.addFeature("Revolve", {{"sketch", s->id()}, {"axis", "Z"}});
        checkVolume(&doc, M_PI * (400 - 100) * 10, "revolve: full turn about Z", 2e-3);
        doc.timeline()->at(1)->set("extent", "angle");
        doc.timeline()->at(1)->set("angle", "90 deg");
        doc.rebuild();
        checkVolume(&doc, M_PI * (400 - 100) * 10 / 4, "revolve: 90 degrees", 2e-3);
    }
    {
        FissionDocument doc;
        auto *a = rectSketch(&doc, "XY", -10, -10, 20, 20);
        auto *plane = doc.addFeature("ConstructionPlane", {{"mode", "offset"}, {"base", "XY"}, {"offset", "30 mm"}});
        auto *b = qobject_cast<FissionSketch *>(doc.addFeature("Sketch", {{"plane", "feature:" + plane->id()}}));
        b->addRectangle(-5, -5, 10, 10);
        doc.rebuild();
        doc.addFeature("Loft", {{"sections", QVariantList{QVariantMap{{"sketch", a->id()}}, QVariantMap{{"sketch", b->id()}}}},
                                {"ruled", true}});
        // Frustum of a square pyramid: h/3 (A1 + A2 + sqrt(A1 A2))
        checkVolume(&doc, 30.0 / 3.0 * (400 + 100 + 200), "loft: square frustum through an offset plane", 2e-3);
        const Bnd_Box box = GeomUtils::boundingBox(doc.body(0)->shape());
        double x0, y0, z0, x1, y1, z1;
        box.Get(x0, y0, z0, x1, y1, z1);
        check(near(z1, 30, 1e-3), "construction: offset plane at 30 mm", QString("zmax=%1").arg(z1));
    }
    {
        FissionDocument doc;
        auto *profile = qobject_cast<FissionSketch *>(doc.addFeature("Sketch", {{"plane", "XZ"}}));
        profile->addCircle(0, 0, 3);
        auto *path = qobject_cast<FissionSketch *>(doc.addFeature("Sketch", {{"plane", "XY"}}));
        path->addLine(0, 0, 0, 50);
        doc.rebuild();
        doc.addFeature("Sweep", {{"sketch", profile->id()}, {"path", path->id()}});
        checkVolume(&doc, M_PI * 9 * 50, "sweep: circle along a line", 2e-3);
        path->addArc(20, 50, 20, 90, 180);
        doc.rebuild();
        checkVolume(&doc, M_PI * 9 * 50 + M_PI * 9 * (M_PI * 20 / 2), "sweep: line + tangent arc path", 5e-3);
    }
}

void testFacePatternCut() {
    FissionDocument doc;
    auto *s = qobject_cast<FissionSketch *>(doc.addFeature("Sketch", {{"plane", "XY"}}));
    s->addRectangle(0, 0, 40, 20);
    s->addCircle(10, 10, 3);
    doc.rebuild();
    extrude(&doc, s, 10);
    check(doc.bodyCount() == 1 && near(totalVolume(&doc), 40 * 20 * 10 - M_PI * 9 * 10, 2e-3),
          "face pattern: plate with one hole", status(&doc));
    int cyl = -1;
    const auto faces = GeomUtils::faceMap(doc.body(0)->shape());
    for (int i = 1; i <= faces.Extent(); ++i) {
        BRepAdaptor_Surface surf(TopoDS::Face(faces(i)));
        if (surf.GetType() == GeomAbs_Cylinder) { cyl = i - 1; break; }
    }
    check(cyl >= 0, "face pattern: found cylindrical hole face");
    if (cyl < 0) return;
    auto *pat = doc.addFeature("RectangularPattern", {
        {"faces", QVariantList{GeomUtils::faceRef(0, doc.body(0)->shape(), cyl)}},
        {"operation", "cut"},
        {"direction1", "X"}, {"count1", "3"}, {"spacing1", "10 mm"},
        {"direction2", ""}, {"count2", "1"}
    });
    check(pat && pat->isHealthy() && doc.bodyCount() == 1
              && near(totalVolume(&doc), 40 * 20 * 10 - 3 * M_PI * 9 * 10, 5e-3),
          "face pattern: three holes cut into the plate", status(&doc));
}

void testPatternsAndMirror() {
    FissionDocument doc;
    auto *s = rectSketch(&doc, "XY", 10, 0, 10, 10);
    extrude(&doc, s, 5);
    auto *mirror = doc.addFeature("Mirror", {{"plane", "YZ"}, {"operation", "join"}});
    check(doc.bodyCount() == 2, "mirror: disjoint copy becomes a second body", status(&doc));
    checkVolume(&doc, 1000, "mirror: volume doubled");
    doc.removeFeature(mirror);

    doc.addFeature("CircularPattern", {{"axis", "Z"}, {"count", "6"}});
    check(doc.bodyCount() == 6, "circular pattern: 6 instances", status(&doc));
    checkVolume(&doc, 3000, "circular pattern: volume");
    doc.removeFeature(doc.timeline()->at(2));

    doc.addFeature("RectangularPattern", {{"direction1", "X"}, {"count1", "3"}, {"spacing1", "20 mm"},
                                          {"direction2", "Y"}, {"count2", "2"}, {"spacing2", "15 mm"}});
    check(doc.bodyCount() == 6, "rectangular pattern: 3 x 2 instances", status(&doc));
}

void testSketchOnFaceAndFiles() {
    FissionDocument doc;
    doc.addParameter("thick", "8 mm", "mm", "plate thickness");
    auto *s = rectSketch(&doc, "XY", 0, 0, 50, 30);
    extrude(&doc, s, "thick");
    const TopoDS_Shape plate = doc.body(0)->shape();
    const auto faces = GeomUtils::faceMap(plate);
    int top = -1;
    for (int i = 1; i <= faces.Extent(); ++i)
        if (near(GeomUtils::faceCentroid(TopoDS::Face(faces(i))).Z(), 8)) top = i - 1;
    auto *onFace = qobject_cast<FissionSketch *>(doc.addFeature("Sketch", {{"plane", GeomUtils::faceRef(0, plate, top)}}));
    onFace->addCircle(0, 0, 5);   // centred on the face centroid
    doc.rebuild();
    extrude(&doc, onFace, 6);
    checkVolume(&doc, 50 * 30 * 8 + M_PI * 25 * 6, "sketch on face: boss extruded from the top face", 2e-3);

    doc.parameter("thick")->setExpression("12 mm");
    doc.rebuild();
    checkVolume(&doc, 50 * 30 * 12 + M_PI * 25 * 6, "sketch on face: follows the face after a parameter edit", 2e-3);

    QTemporaryDir dir;
    const QString path = dir.filePath("roundtrip.f360");
    check(doc.save(path), "file: save .f360", doc.lastError());
    FissionDocument loaded;
    check(loaded.load(path), "file: load .f360", loaded.lastError());
    check(near(totalVolume(&loaded), totalVolume(&doc)) && loaded.timeline()->count() == doc.timeline()->count()
              && loaded.parameter("thick"),
          "file: reloaded model matches", status(&loaded));

    const QByteArray png = QByteArray::fromBase64(
        "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mP8z8BQDwAEhQGAhKmMIQAAAABJRU5ErkJggg==");
    doc.setThumbnail(png);
    const QString thumbPath = dir.filePath("preview.f360");
    doc.save(thumbPath);
    {
        QFile f(thumbPath);
        f.open(QIODevice::ReadOnly);
        const QByteArray head = f.read(64);
        check(head.indexOf("\"thumbnail\": \"") >= 0 && head.indexOf("\"thumbnail\"") < 12,
              "file: preview PNG is the first key in the .f360", QString::fromLatin1(head.left(40)));
    }
    FissionDocument withThumb;
    check(withThumb.load(thumbPath) && withThumb.thumbnail() == png && near(totalVolume(&withThumb), totalVolume(&doc)),
          "file: embedded preview reloads with the model", withThumb.lastError());
    doc.setThumbnail({});

    // Appearance library + render settings round-trip with the model
    check(AppearanceLibrary::instance()->appearances().size() >= 10, "appearance: built-in library has materials");
    check(RenderEnvironmentLibrary::instance()->polyHavenHdris().size() >= 5
              && RenderEnvironmentLibrary::instance()->presets().size() >= 3,
          "render: lighting presets and Poly Haven HDRI catalog");
    check(!RenderAssetStore::instance()->cacheRoot().isEmpty()
              && RenderAssetStore::instance()->hdriDownloadUrl("studio_small_09", "2k").contains("studio_small_09_2k.hdr")
              && RenderAssetStore::instance()->materialCatalog().size() >= 5,
          "render: asset store cache + HDRI/material download URLs");
    {
        // Minimal 2×2 uncompressed RGBE so the HDR loader is exercised without a network download
        const QString hdrPath = dir.filePath("tiny.hdr");
        QFile hf(hdrPath);
        hf.open(QIODevice::WriteOnly);
        hf.write("#?RADIANCE\nFORMAT=32-bit_rle_rgbe\n\n-Y 2 +X 2\n");
        const unsigned char px[] = {128, 128, 128, 128, 200, 180, 160, 130, 100, 120, 140, 125, 255, 200, 100, 140};
        hf.write(reinterpret_cast<const char *>(px), sizeof px);
        hf.close();
        HdrImage img;
        check(img.load(hdrPath) && img.width == 2 && img.height == 2 && !img.toToneMappedImage().isNull(),
              "render: Radiance HDR loader reads a tiny .hdr");
    }
    check(doc.bodyCount() >= 1, "appearance: document has a body to paint");
    doc.applyAppearance(QStringLiteral("aluminum"), QVariantList{0});
    check(doc.body(0)->appearanceId() == "aluminum" && near(doc.body(0)->metalness(), 0.95, 0.01),
          "appearance: aluminum applied to body 0");
    doc.renderSettings()->setLightIntensity(1.75);
    doc.renderSettings()->setLightAltitude(42);
    doc.renderSettings()->setEnvironmentPath(
        RenderEnvironmentLibrary::instance()->environment("studio_small_09").value("downloadUrl").toString());
    const QString matPath = dir.filePath("materials.f360");
    check(doc.save(matPath), "appearance: save with materials", doc.lastError());
    FissionDocument painted;
    check(painted.load(matPath), "appearance: reload painted file", painted.lastError());
    check(painted.body(0)->appearanceId() == "aluminum" && near(painted.body(0)->metalness(), 0.95, 0.01)
              && near(painted.renderSettings()->lightIntensity(), 1.75, 0.01)
              && near(painted.renderSettings()->lightAltitude(), 42, 0.01)
              && painted.renderSettings()->environmentPath().contains("studio_small_09"),
          "appearance: materials and render settings survive save/load");

    // Per-face appearance + PBR map path round-trip
    check(doc.body(0)->faceCount() >= 1, "appearance: body has faces for per-face paint");
    doc.body(0)->applyAppearanceToFace(0, QStringLiteral("plastic_red"));
    doc.body(0)->setFaceMaps(0, dir.filePath("fake_diff.jpg"), dir.filePath("fake_rough.jpg"),
                             QString(), QString());
    // Write tiny JPGs so paths exist for the path tracer / save
    {
        QImage tiny(4, 4, QImage::Format_RGB888);
        tiny.fill(QColor(200, 40, 40));
        tiny.save(dir.filePath("fake_diff.jpg"), "JPG");
        tiny.fill(QColor(180, 180, 180));
        tiny.save(dir.filePath("fake_rough.jpg"), "JPG");
    }
    doc.body(0)->setRoughnessPath(dir.filePath("fake_rough.jpg"));
    doc.body(0)->setNormalPath(dir.filePath("fake_diff.jpg")); // any RGB image exercises the normal path field
    const QString facePath = dir.filePath("face_appear.f360");
    check(doc.save(facePath), "appearance: save with face overrides", doc.lastError());
    FissionDocument faced;
    check(faced.load(facePath), "appearance: reload face overrides", faced.lastError());
    check(!faced.body(0)->faceAppearance(0).isEmpty()
              && faced.body(0)->faceAppearance(0).value("appearanceId").toString() == "plastic_red"
              && !faced.body(0)->roughnessPath().isEmpty(),
          "appearance: per-face override and PBR map paths survive save/load");

    // Monte Carlo path tracer (tiny image, few samples)
    {
        const QString png = dir.filePath("pathtrace.png");
        QVariantMap cam;
        cam["eye"] = QVariantList{80.0, -80.0, 60.0};
        cam["target"] = QVariantList{5.0, 5.0, 5.0};
        cam["up"] = QVariantList{0.0, 0.0, 1.0};
        cam["fov"] = 35.0;
        cam["width"] = 48;
        cam["height"] = 36;
        cam["samples"] = 4;
        const QString out = doc.pathTraceToFile(png, cam);
        QImage img(out);
        check(!out.isEmpty() && !img.isNull() && img.width() == 48 && img.height() == 36,
              "render: path tracer writes a small PNG", doc.lastError());
        // At least some non-background pixels (scene has geometry)
        int lit = 0;
        for (int y = 0; y < img.height(); ++y)
            for (int x = 0; x < img.width(); ++x) {
                const QColor c = img.pixelColor(x, y);
                if (c.red() + c.green() + c.blue() > 30) ++lit;
            }
        check(lit > 10, "render: path tracer produces lit pixels");
    }
    check(RenderAssetStore::instance()->materialRoughnessPath("metal_plate").contains("metal_plate_rough_1k"),
          "render: material roughness path naming");
    check(PathTracer::samplesForQuality(0) == 8 && PathTracer::samplesForQuality(3) == 256,
          "render: path tracer quality → spp mapping");
    {
        BRepRenderer snapR;
        check(snapR.snapToGrid(), "sketch: snap to grid defaults on");
        snapR.setSnapToGrid(false);
        check(!snapR.snapToGrid(), "sketch: snap to grid can be turned off");
    }

    for (const QString ext : {"stl", "step", "iges", "obj"}) {
        const QString out = dir.filePath("export." + ext);
        const bool ok = !doc.exportFile(out).isEmpty() && QFileInfo(out).size() > 100;
        check(ok, "export: " + ext, doc.lastError());
    }

    // Fusion export with a BRep snapshot per timeline step: scrubbing must change the bodies
    auto stepB64 = [&](const TopoDS_Shape &shape, const QString &name) {
        const QString p = dir.filePath(name + ".stp");
        STEPControl_Writer w;
        w.Transfer(shape, STEPControl_AsIs);
        w.Write(p.toLocal8Bit().constData());
        QFile f(p);
        f.open(QIODevice::ReadOnly);
        return QString::fromLatin1(f.readAll().toBase64());
    };
    BRep_Builder bb;
    TopoDS_Compound two;
    bb.MakeCompound(two);
    bb.Add(two, BRepPrimAPI_MakeBox(10, 10, 10).Shape());
    bb.Add(two, BRepPrimAPI_MakeBox(gp_Pnt(20, 0, 0), 10, 10, 10).Shape());
    // Fusion can write a mesh body into the STEP as B-rep too; the imported mesh must replace that copy
    TopoDS_Compound withFacetedCopy;
    bb.MakeCompound(withFacetedCopy);
    bb.Add(withFacetedCopy, BRepPrimAPI_MakeBox(10, 10, 5).Shape());
    bb.Add(withFacetedCopy, BRepPrimAPI_MakeBox(gp_Pnt(50, 0, 0), 2, 2, 2).Shape());
    const QJsonObject fusion{
        {"metadata", QJsonObject{{"exportedFrom", "Fusion 360"}, {"exporterVersion", "1.4"}}},
        {"document", QJsonObject{{"name", "Steps"}}},
        {"timeline", QJsonArray{
            QJsonObject{{"index", 0}, {"type", "adsk::fusion::ExtrudeFeature"}, {"name", "Extrude1"},
                        {"stepData", stepB64(BRepPrimAPI_MakeBox(10, 10, 10).Shape(), "s1")}},
            QJsonObject{{"index", 1}, {"type", "adsk::fusion::ExtrudeFeature"}, {"name", "Extrude2"},
                        {"stepData", stepB64(two, "s2")},
                        {"stepBodies", QJsonArray{
                            QJsonObject{{"name", "Right"}, {"box", QJsonArray{20, 0, 0, 30, 10, 10}}},
                            QJsonObject{{"name", "Left"}, {"box", QJsonArray{0, 0, 0, 10, 10, 10}}}}}},
            QJsonObject{{"index", 2}, {"type", "adsk::fusion::SomeFutureFeature"}, {"name", "Odd1"},
                        {"stepData", stepB64(withFacetedCopy, "s3")},
                        {"stepBodies", QJsonArray{
                            QJsonObject{{"name", "Plate"}, {"box", QJsonArray{0, 0, 0, 10, 10, 5}}},
                            QJsonObject{{"name", "Scan"}, {"box", QJsonArray{50, 0, 0, 52, 2, 2}}}}}}}},
        {"meshBodies", QJsonArray{QJsonObject{
            {"name", "Scan"},
            {"nodes", QString::fromLatin1([] {
                 const float n[] = {50, 0, 0, 51, 0, 0, 50, 1, 0, 50, 0, 1};
                 return QByteArray(reinterpret_cast<const char *>(n), sizeof n).toBase64();
             }())},
            {"triangles", QString::fromLatin1([] {
                 const qint32 t[] = {0, 2, 1, 0, 1, 3, 1, 2, 3, 0, 3, 2};
                 return QByteArray(reinterpret_cast<const char *>(t), sizeof t).toBase64();
             }())}}}},
        {"features", QJsonArray{}}, {"bodies", QJsonArray{}}, {"sketches", QJsonArray{}}};
    const QString fusionPath = dir.filePath("fusion_steps.f360");
    {
        QFile f(fusionPath);
        f.open(QIODevice::WriteOnly);
        f.write(QJsonDocument(fusion).toJson());
    }
    FissionDocument steps;
    check(steps.load(fusionPath), "fusion import: per-step snapshot file loads", steps.lastError());
    check(steps.timeline()->count() == 4 && steps.bodyCount() == 2 && near(totalVolume(&steps), 500, 1),
          "fusion import: last snapshot is the model, unknown steps kept", status(&steps));
    check(steps.bodyCount() == 2 && steps.body(0)->name() == "Plate" && steps.body(1)->name() == "Scan"
              && MeshUtils::isMesh(steps.body(1)->shape()),
          "fusion import: mesh body comes in as a named mesh body, replacing its STEP copy", status(&steps));
    steps.timeline()->setMarker(1);
    steps.rebuild();
    check(steps.bodyCount() == 1 && near(totalVolume(&steps), 1000, 1), "fusion import: marker at step 1 shows one box",
          status(&steps));
    steps.timeline()->setMarker(2);
    steps.rebuild();
    check(steps.bodyCount() == 2 && near(totalVolume(&steps), 2000, 1), "fusion import: marker at step 2 shows two boxes",
          status(&steps));
    check(steps.bodyCount() == 2 && steps.body(0)->name() == "Left" && steps.body(1)->name() == "Right",
          "fusion import: step bodies keep their Fusion names",
          steps.bodyCount() == 2 ? steps.body(0)->name() + ", " + steps.body(1)->name() : status(&steps));
}

void testConstraints() {
    FissionDocument doc;
    doc.addParameter("h", "20 mm");
    auto *s = qobject_cast<FissionSketch *>(doc.addFeature("Sketch", {{"plane", "XY"}}));
    const int l0 = s->addRectangleLines(3, 2, 30, 15);
    doc.rebuild();
    check(s->isHealthy() && s->profileCount() == 1 && s->degreesOfFreedom() == 4,
          "constraints: constrained rectangle has 4 degrees of freedom",
          QString("dof %1, profiles %2 %3").arg(s->degreesOfFreedom()).arg(s->profileCount()).arg(s->error()));

    auto pt = [](int e, const char *role) { return QVariantMap{{"e", e}, {"p", role}}; };
    QVariantMap r = s->addConstraint({{"type", "coincident"}, {"a", pt(l0, "start")}, {"b", pt(-1, "origin")}});
    const QVariantMap e0 = s->entity(l0);
    check(r.value("ok").toBool() && near(e0.value("x1").toDouble(), 0) && near(e0.value("y1").toDouble(), 0),
          "constraints: corner snaps to the origin", r.value("error").toString());
    s->addConstraint({{"type", "distance"}, {"orient", "horizontal"}, {"a", pt(l0, "start")}, {"b", pt(l0, "end")},
                      {"value", "50"}});
    r = s->addConstraint({{"type", "distance"}, {"orient", "vertical"}, {"a", pt(l0 + 1, "start")},
                          {"b", pt(l0 + 1, "end")}, {"value", "h"}});
    doc.rebuild();
    check(r.value("ok").toBool() && s->degreesOfFreedom() == 0 && s->entityFullyConstrained(l0 + 2),
          "constraints: two driving dimensions fully constrain it", QString("dof %1").arg(s->degreesOfFreedom()));
    {
        // Geometric constraints report one [u, v] anchor per referenced item
        const QVariantList annos = s->annotations();
        const QVariantMap h = annos.value(4).toMap();
        const QVariantList anchor = h.value("anchors").toList().value(0).toList();
        check(annos.size() == s->constraintCount() && h.value("type") == "horizontal" && anchor.size() == 2
                  && near(anchor[0].toDouble(), 25) && near(anchor[1].toDouble(), 0),
              "constraints: annotation anchors for the viewport badges",
              QJsonDocument::fromVariant(h).toJson(QJsonDocument::Compact));
    }
    auto *ex = extrude(&doc, s, "10 mm");
    doc.rebuild();
    checkVolume(&doc, 50 * 20 * 10, "constraints: dimensions drive the extruded size");
    doc.parameter("h")->setExpression("35 mm");
    doc.rebuild();
    checkVolume(&doc, 50 * 35 * 10, "constraints: parameter in a dimension drives the sketch");

    const int before = s->constraintCount();
    r = s->addConstraint({{"type", "distance"}, {"orient", "horizontal"}, {"a", pt(l0 + 2, "start")},
                          {"b", pt(l0 + 2, "end")}, {"value", "70"}});
    check(!r.value("ok").toBool() && s->constraintCount() == before, "constraints: conflicting dimension is rejected",
          r.value("error").toString());

    // Circle above the top edge made tangent, then sized
    const int c = s->addCircle(25, 45, 4);
    s->addConstraint({{"type", "tangent"}, {"a", QVariantMap{{"e", l0 + 2}}}, {"b", QVariantMap{{"e", c}}}});
    s->addConstraint({{"type", "radius"}, {"a", QVariantMap{{"e", c}}}, {"value", "6"}});
    doc.rebuild();
    const QVariantMap ce = s->entity(c);
    check(near(ce.value("r").toDouble(), 6) && near(ce.value("cy").toDouble(), 35 + 6),
          "constraints: tangent circle keeps its side of the line",
          QString("r %1 cy %2").arg(ce.value("r").toDouble()).arg(ce.value("cy").toDouble()));
    s->removeEntity(c);
    doc.rebuild();
    check(s->constraintCount() == before && s->isHealthy(), "constraints: deleting geometry removes its constraints",
          QString("%1 constraints %2").arg(s->constraintCount()).arg(s->error()));
    checkVolume(&doc, 50 * 35 * 10, "constraints: model unchanged after the delete");
    Q_UNUSED(ex);

    auto *s2 = qobject_cast<FissionSketch *>(doc.addFeature("Sketch", {{"plane", "XY"}}));
    const int a = s2->addArcThroughPoints(-10, 0, 0, 10, 10, 0);
    const QVariantMap ae = s2->entity(a);
    check(near(ae.value("r").toDouble(), 10) && near(ae.value("a1").toDouble(), 0) && near(ae.value("a2").toDouble(), 180)
              && s2->roleAt(a, -10, 0) == "end",
          "constraints: three-point arc becomes a CCW center arc",
          QString("a1 %1 a2 %2").arg(ae.value("a1").toDouble()).arg(ae.value("a2").toDouble()));
    const int m1 = s2->addLine(0, 0, 20, 0), m2 = s2->addLine(0, 0, 20, 5);
    s2->addConstraint({{"type", "fix"}, {"a", QVariantMap{{"e", m1}}}});
    s2->addConstraint({{"type", "coincident"}, {"a", pt(m2, "start")}, {"b", pt(m1, "start")}});
    r = s2->addConstraint({{"type", "angle"}, {"a", QVariantMap{{"e", m1}}}, {"b", QVariantMap{{"e", m2}}}, {"value", "30"}});
    const QVariantMap me = s2->entity(m2);
    const double ang = std::atan2(me.value("y2").toDouble(), me.value("x2").toDouble()) * 180 / M_PI;
    check(r.value("ok").toBool() && near(ang, 30), "constraints: angle dimension", QString("%1 deg").arg(ang));
}

double num(const QVariantMap &m, const char *key) { return m.value(key).toDouble(); }

void testLoftFromFace() {
    FissionDocument doc;
    auto *base = rectSketch(&doc, "XY", -20, -20, 40, 40);
    extrude(&doc, base, 10);
    const TopoDS_Shape box = doc.body(0)->shape();
    const auto faces = GeomUtils::faceMap(box);
    int top = -1;
    for (int i = 1; i <= faces.Extent(); ++i)
        if (near(GeomUtils::faceCentroid(TopoDS::Face(faces(i))).Z(), 10)) top = i - 1;
    auto *plane = doc.addFeature("ConstructionPlane", {{"mode", "offset"}, {"base", "XY"}, {"offset", "30 mm"}});
    auto *cap = qobject_cast<FissionSketch *>(doc.addFeature("Sketch", {{"plane", "feature:" + plane->id()}}));
    cap->addCircle(0, 0, 8);
    doc.rebuild();
    auto *loft = doc.addFeature("Loft", {{"sections", QVariantList{GeomUtils::faceRef(0, box, top),
                                                                   QVariantMap{{"kind", "profile"}, {"sketch", cap->id()}, {"profile", 0}}}},
                                         {"ruled", true}, {"operation", "join"}});
    const Bnd_Box bounds = GeomUtils::boundingBox(doc.body(0)->shape());
    double x0, y0, z0, x1, y1, z1;
    bounds.Get(x0, y0, z0, x1, y1, z1);
    check(loft->isHealthy() && doc.bodyCount() == 1 && near(z1, 30) && totalVolume(&doc) > 16000,
          "loft: a face section joins the whole face to a sketch profile", status(&doc));
    // A point just above a corner of the top face is inside only when the loft starts from that whole face.
    // A loft from the base sketch would already have narrowed past this point.
    BRepClass3d_SolidClassifier cls(doc.body(0)->shape(), gp_Pnt(17, 17, 12), 1e-6);
    check(cls.State() == TopAbs_IN, "loft: the face's corners are part of the section",
          QString("state %1, %2").arg(int(cls.State())).arg(status(&doc)));
}

void testExtrudeFace() {
    FissionDocument doc;
    auto *s = rectSketch(&doc, "XY", 0, 0, 40, 20);
    auto *base = extrude(&doc, s, "10 mm", "newBody");
    base->set("taper", "40 deg");
    doc.rebuild();
    const double tapered = totalVolume(&doc);
    const TopoDS_Shape body = doc.body(0)->shape();
    const auto faces = GeomUtils::faceMap(body);
    int top = -1;
    for (int i = 1; i <= faces.Extent(); ++i)
        if (near(GeomUtils::faceCentroid(TopoDS::Face(faces(i))).Z(), 10)) top = i - 1;
    GProp_GProps props;
    BRepGProp::SurfaceProperties(faces(top + 1), props);
    const double topArea = props.Mass();

    auto *up = doc.addFeature("Extrude", {{"profiles", QVariantList{GeomUtils::faceRef(0, body, top)}},
                                          {"distance", "5 mm"}, {"operation", "join"}});
    const Bnd_Box box = GeomUtils::boundingBox(doc.body(0)->shape());
    double x0, y0, z0, x1, y1, z1;
    box.Get(x0, y0, z0, x1, y1, z1);
    check(up->isHealthy() && doc.bodyCount() == 1 && near(totalVolume(&doc), tapered + topArea * 5, 1e-3)
              && near(z1, 15, 1e-3) && near(x1 - x0, 40, 1e-3),
          "extrude face: the top face of a tapered body extrudes by itself and joins, keeping the taper",
          QString("top area %1, z max %2, width %3, %4").arg(topArea).arg(z1).arg(x1 - x0).arg(status(&doc)));

    up->set("distance", "-3 mm");
    up->set("operation", "cut");
    doc.rebuild();
    check(up->isHealthy() && near(totalVolume(&doc), tapered - topArea * 3, 2e-3),
          "extrude face: a negative distance with Cut cuts into the body under the face", status(&doc));

    const QVariantMap m = up->manipulator();
    check(m.value("key") == "distance" && near(m.value("origin").value<QVector3D>().z(), 10, 1e-3),
          "extrude face: the distance arrow starts on the face", status(&doc));
}

void testHandlesAndMeshExport() {
    FissionDocument doc;
    auto *s = rectSketch(&doc, "XY", 0, 0, 40, 20);
    auto *ex = extrude(&doc, s, "10 mm", "newBody");
    {
        const QVariantMap a = ex->manipulator().value("angle").toMap();
        const QVector3D pivot = a.value("pivot").value<QVector3D>(), out = a.value("out").value<QVector3D>();
        check(a.value("key") == "taper" && near(a.value("height").toDouble(), 10) && near(pivot.x(), 40)
                  && near(out.x(), 1),
              "manipulator: extrude taper knob pivots on the profile's +X side",
              QString("key %1 pivot x %2 height %3").arg(a.value("key").toString()).arg(pivot.x())
                  .arg(a.value("height").toDouble()));
    }
    ex->set("taper", "5 deg");
    doc.rebuild();
    check(near(ex->manipulator().value("angle").toMap().value("value").toDouble(), 5) && totalVolume(&doc) < 8000,
          "manipulator: taper value follows the feature and narrows the body", status(&doc));

    auto *s2 = rectSketch(&doc, "XY", 100, 0, 10, 10);
    extrude(&doc, s2, "5 mm", "newBody");
    QTemporaryDir dir;
    const QString one = dir.filePath("one.stl"), both = dir.filePath("both.stl");
    const bool okOne = !doc.exportBodies(one, {1}).isEmpty();
    const bool okBoth = !doc.exportBodies(both, {0, 1}).isEmpty();
    check(okOne && okBoth && QFileInfo(one).size() > 100 && QFileInfo(one).size() < QFileInfo(both).size(),
          "export: Save As Mesh writes only the chosen body",
          QString("%1 vs %2 bytes %3").arg(QFileInfo(one).size()).arg(QFileInfo(both).size()).arg(doc.lastError()));
    check(doc.exportBodies(dir.filePath("none.stl"), {7}).isEmpty(), "export: a missing body is an error");
    {
        // 3MF: a ZIP package with the content types, the relationship and the model, in that order
        doc.body(1)->setColor(QColor("#ff0000"));
        const QString m3 = dir.filePath("parts.3mf");
        const bool ok = !doc.exportBodies(m3, {0, 1}).isEmpty();
        QFile f(m3);
        QStringList names;
        qint64 modelSize = 0;
        if (ok && f.open(QIODevice::ReadOnly)) {
            const QByteArray z = f.readAll();
            auto u16 = [&](qsizetype o) { return int(quint8(z[o])) | int(quint8(z[o + 1])) << 8; };
            auto u32 = [&](qsizetype o) { return qint64(u16(o)) | qint64(u16(o + 2)) << 16; };
            for (qsizetype o = 0; o + 30 <= z.size() && u32(o) == 0x04034b50;) {
                const qint64 packed = u32(o + 18), size = u32(o + 22);
                const int nameLen = u16(o + 26), extra = u16(o + 28);
                names << QString::fromUtf8(z.mid(o + 30, nameLen));
                if (names.last() == "3D/3dmodel.model") modelSize = size;
                o += 30 + nameLen + extra + packed;
            }
        }
        check(ok && names == QStringList{"[Content_Types].xml", "_rels/.rels", "3D/3dmodel.model"} && modelSize > 1000,
              "export: 3MF package with the model", names.join(", ") + " " + doc.lastError());
        if (qEnvironmentVariableIsSet("FISSION_KEEP_EXPORT"))   // a copy to open in other tools
            QFile::copy(m3, QDir(qEnvironmentVariable("FISSION_KEEP_EXPORT")).filePath("selftest.3mf"));

        // DXF of a planar face: the top of the 10 x 10 box is four lines
        const auto faces = GeomUtils::faceMap(doc.body(1)->shape());
        int top = -1;
        for (int i = 1; i <= faces.Extent(); ++i)
            if (near(GeomUtils::faceCentroid(TopoDS::Face(faces(i))).Z(), 5, 1e-6)) top = i - 1;
        const QString fd = dir.filePath("face.dxf");
        const bool dxfOk = !doc.exportFaceDxf({{"kind", "face"}, {"body", 1}, {"index", top}}, fd).isEmpty();
        QFile df(fd);
        const QString dxf = dxfOk && df.open(QIODevice::ReadOnly | QIODevice::Text) ? QString::fromLatin1(df.readAll()) : QString();
        check(dxfOk && dxf.count("\nLINE\n") == 4 && dxf.trimmed().endsWith("EOF"), "export: DXF of a planar face",
              doc.lastError());
        const auto side = [&] {
            for (int i = 1; i <= faces.Extent(); ++i)
                if (BRepAdaptor_Surface(TopoDS::Face(faces(i))).GetType() != GeomAbs_Plane) return i - 1;
            return -1;
        }();
        check(side < 0 || doc.exportFaceDxf({{"kind", "face"}, {"body", 1}, {"index", side}}, fd).isEmpty(),
              "export: DXF refuses curved faces");

        // Batch: one file per body and format, named after the bodies
        const QString out = dir.filePath("batch");
        const QVariantList files = doc.exportBatch({0, 1}, out, {"stl", "3mf", "step"}, true);
        bool allThere = files.size() == 6;
        for (const QVariant &v : files) allThere = allThere && QFileInfo(v.toString()).size() > 100;
        const QVariantList one = doc.exportBatch({0, 1}, out, {"obj"}, false, "Design A/B");
        check(allThere && one.size() == 1 && QFileInfo(one[0].toString()).fileName() == "Design A_B.obj",
              "export: batch writes each body in each format", QString("%1 files %2").arg(files.size()).arg(doc.lastError()));
    }

    auto *plane = doc.addFeature("ConstructionPlane", {{"mode", "offset"}, {"base", "XY"}, {"offset", "30 mm"}});
    plane->set("display", QVariantList{-5, 60, -10, 25});
    doc.rebuild();
    QVariantMap info;
    QMetaObject::invokeMethod(plane, "displayInfo", Q_RETURN_ARG(QVariantMap, info));
    const QVariantList b = info.value("bounds").toList();
    const Bnd_Box box = GeomUtils::boundingBox(plane->overlayShape());
    double x0, y0, z0, x1, y1, z1;
    box.Get(x0, y0, z0, x1, y1, z1);
    check(b.size() == 4 && near(b[1].toDouble(), 60) && near(x0, -5, 1e-2) && near(x1, 60, 1e-2) && near(y1, 25, 1e-2)
              && near(z0, 30, 1e-2),
          "construction plane: a resized edge moves the drawn rectangle",
          QString("x %1..%2 y %3..%4 z %5").arg(x0).arg(x1).arg(y0).arg(y1).arg(z0));
}

void testDocumentSettings() {
    FissionDocument doc;
    check(doc.lengthUnit() == "mm" && doc.massUnit() == "g" && doc.withUnit("10", "length") == "10 mm",
          "units: a new design is mm and g");
    doc.checkpoint();
    doc.setLengthUnit("in");
    doc.setMassUnit("lb");
    doc.setLengthUnit("furlong");
    check(doc.lengthUnit() == "in" && doc.massUnit() == "lb" && near(doc.lengthScale(), 25.4),
          "units: switch to in / lb; an unknown unit is ignored", doc.lengthUnit() + " " + doc.massUnit());
    const QString a = doc.withUnit("10", "length"), b = doc.withUnit("10 / 2", "length"),
                  c = doc.withUnit("w * 2", "length"), d = doc.withUnit("30", "angle"), e = doc.withUnit("4", ""),
                  f = doc.withUnit("5 mm", "length");
    check(a == "10 in" && b == "(10 / 2) in" && c == "w * 2" && d == "30 deg" && e == "4" && f == "5 mm",
          "units: bare numbers take the document unit, names and explicit units are kept",
          QStringList{a, b, c, d, e, f}.join(" | "));
    check(near(doc.evaluateExpression(b).value("value").toDouble(), 127),
          "units: \"(10 / 2) in\" evaluates to 127 mm");
    const QString l = doc.formatLength(25.4), ar = doc.formatLength(645.16, 3, 2), m = doc.formatMass(453.59237);
    check(l == "1 in" && ar == QString("1 in\u00b2") && m == "1 lb", "units: lengths, areas and masses shown in document units",
          QStringList{l, ar, m}.join(" | "));

    doc.setInfo("partNumber", "FX-100");
    doc.setInfo("description", "Bracket");
    QTemporaryDir dir;
    const QString path = dir.filePath("settings.f360");
    doc.save(path);
    FissionDocument loaded;
    loaded.load(path);
    check(loaded.lengthUnit() == "in" && loaded.massUnit() == "lb" && loaded.info().value("partNumber") == "FX-100"
              && loaded.info().value("description") == "Bracket",
          "document settings: units and properties survive save and load");
    doc.undo();
    check(doc.lengthUnit() == "mm" && doc.massUnit() == "g", "document settings: undo restores the units",
          doc.lengthUnit() + " " + doc.massUnit());
}

void testTangentChain() {
    FissionDocument doc;
    auto *s = qobject_cast<FissionSketch *>(doc.addFeature("Sketch", {{"plane", "XY"}}));
    s->addSlot(0, 0, 40, 0, 10);
    doc.rebuild();
    extrude(&doc, s, 5);
    const TopoDS_Shape slot = doc.body(0)->shape();
    const auto edges = GeomUtils::edgeMap(slot);
    int topStraight = -1, vertical = -1;
    for (int i = 1; i <= edges.Extent(); ++i) {
        const gp_Pnt m = GeomUtils::edgeMidpoint(TopoDS::Edge(edges(i)));
        if (near(m.X(), 20) && near(m.Y(), 5) && near(m.Z(), 5)) topStraight = i - 1;
        if (near(m.Z(), 2.5)) vertical = i - 1;
    }
    const QVariantList chain = doc.tangentChain(GeomUtils::edgeRef(0, slot, topStraight));
    check(chain.size() == 4, "tangent chain: slot top loop (2 lines + 2 arcs)", QString("%1 edges").arg(chain.size()));
    const QVariantList single = doc.tangentChain(GeomUtils::edgeRef(0, slot, vertical));
    check(single.size() == 1, "tangent chain: sharp edge stays alone", QString("%1 edges").arg(single.size()));
}

void testSketchEditing() {
    FissionDocument doc;
    auto *s = qobject_cast<FissionSketch *>(doc.addFeature("Sketch", {{"plane", "XY"}}));
    const int h = s->addLine(0, 0, 20, 0);
    s->addLine(10, -5, 10, 5);
    doc.rebuild();
    bool ok = s->trimAt(h, 15, 0);
    QVariantMap e = s->entity(h);
    check(ok && near(num(e, "x1"), 0) && near(num(e, "x2"), 10), "sketch trim: line cut back to the crossing",
          QJsonDocument::fromVariant(e).toJson(QJsonDocument::Compact));

    const int ext = s->addLine(0, 10, 5, 10);
    s->addLine(25, 0, 25, 20);
    doc.rebuild();
    ok = s->extendAt(ext, 5, 10);
    e = s->entity(ext);
    check(ok && near(num(e, "x2"), 25) && near(num(e, "x1"), 0), "sketch extend: line runs to the next curve",
          QJsonDocument::fromVariant(e).toJson(QJsonDocument::Compact));

    const int c = s->addCircle(100, 0, 10);
    s->addLine(95, -20, 95, 20);
    doc.rebuild();
    ok = s->trimAt(c, 110, 0);
    e = s->entity(c);
    check(ok && e.value("type") == "arc" && near(num(e, "r"), 10), "sketch trim: circle becomes an arc",
          QJsonDocument::fromVariant(e).toJson(QJsonDocument::Compact));

    auto *s2 = qobject_cast<FissionSketch *>(doc.addFeature("Sketch", {{"plane", "XY"}}));
    const int r0 = s2->addRectangleLines(0, 0, 20, 10);
    doc.rebuild();
    const QVariantList chain = s2->chainOf(r0);
    const QVariantList added = s2->offsetEntities(chain, 2, 10, 5);
    double x0 = 1e9, x1 = -1e9, y0 = 1e9, y1 = -1e9;
    for (const QVariant &iv : added) {
        const QVariantMap m = s2->entity(iv.toInt());
        for (const char *k : {"x1", "x2"}) { x0 = std::min(x0, num(m, k)); x1 = std::max(x1, num(m, k)); }
        for (const char *k : {"y1", "y2"}) { y0 = std::min(y0, num(m, k)); y1 = std::max(y1, num(m, k)); }
    }
    doc.rebuild();
    check(chain.size() == 4 && added.size() == 4 && near(x0, 2) && near(x1, 18) && near(y0, 2) && near(y1, 8)
              && s2->profileCount() == 2,
          "sketch offset: rectangle loop offset inward by 2",
          QString("chain %1 added %2 x %3..%4 y %5..%6 profiles %7").arg(chain.size()).arg(added.size())
              .arg(x0).arg(x1).arg(y0).arg(y1).arg(s2->profileCount()));
    const int circ = s2->addCircle(50, 50, 5);
    const QVariantList grown = s2->offsetEntities({circ}, 1, 60, 50);
    check(grown.size() == 1 && near(num(s2->entity(grown[0].toInt()), "r"), 6), "sketch offset: circle grows outward");

    auto *s3 = qobject_cast<FissionSketch *>(doc.addFeature("Sketch", {{"plane", "XY"}}));
    const int a = s3->addLine(0, 0, 20, 0), b = s3->addLine(0, 0, 0, 20);
    s3->addConstraint({{"type", "coincident"}, {"a", QVariantMap{{"e", a}, {"p", "start"}}},
                       {"b", QVariantMap{{"e", b}, {"p", "start"}}}});
    const QVariantMap fr = s3->filletCorner(a, b, "3");
    doc.rebuild();
    const QVariantMap arc = s3->entity(fr.value("index").toInt());
    check(fr.value("ok").toBool() && near(num(s3->entity(a), "x1"), 3) && near(num(s3->entity(b), "y1"), 3)
              && arc.value("type") == "arc" && near(num(arc, "r"), 3) && near(num(arc, "cx"), 3) && s3->isHealthy(),
          "sketch fillet: corner rounded with a tangent R3 arc",
          QJsonDocument::fromVariant(fr).toJson(QJsonDocument::Compact) + " " + s3->error());

    // Drag: a free line end follows the cursor; a fixed (projected) entity stays put
    QVariantMap target{{"e", a}, {"p", "end"}, {"u", 30}, {"v", 0}};
    ok = s3->dragTo({target});
    check(ok && near(num(s3->entity(a), "x2"), 30) && near(num(s3->entity(fr.value("index").toInt()), "r"), 3),
          "sketch drag: line end follows the cursor",
          QJsonDocument::fromVariant(s3->entity(a)).toJson(QJsonDocument::Compact));

    auto *ps = rectSketch(&doc, "XY", 0, 0, 40, 20);
    extrude(&doc, ps, 10);
    const TopoDS_Shape box = doc.body(0)->shape();
    const auto faces = GeomUtils::faceMap(box);
    int top = -1;
    for (int i = 1; i <= faces.Extent(); ++i)
        if (near(GeomUtils::faceCentroid(TopoDS::Face(faces(i))).Z(), 10)) top = i - 1;
    auto *proj = qobject_cast<FissionSketch *>(doc.addFeature("Sketch", {{"plane", "XY"}}));
    const int n = proj->projectRef(GeomUtils::faceRef(0, box, top));
    doc.rebuild();
    const QVariantMap p0 = proj->entity(0);
    check(n == 4 && p0.value("fixed").toBool() && p0.value("projected").toBool() && proj->profileCount() == 1,
          "sketch project: top face outline becomes 4 fixed lines",
          QString("%1 entities, %2 profiles").arg(n).arg(proj->profileCount()));
    proj->dragTo({QVariantMap{{"e", 0}, {"p", "start"}, {"u", 100}, {"v", 100}}});
    const QVariantMap p0b = proj->entity(0);
    check(near(num(p0b, "x1"), num(p0, "x1")) && near(num(p0b, "y1"), num(p0, "y1")),
          "sketch drag: projected geometry is fixed");

    auto *s4 = qobject_cast<FissionSketch *>(doc.addFeature("Sketch", {{"plane", "XY"}}));
    const int dl = s4->addLine(0, 0, 10, 0);
    const QVariantMap dr = s4->addConstraint({{"type", "distance"}, {"a", QVariantMap{{"e", dl}, {"p", "start"}}},
                                              {"b", QVariantMap{{"e", dl}, {"p", "end"}}}, {"value", "10"}});
    s4->setDimensionOffset(dr.value("index").toInt(), 7.5);
    doc.rebuild();
    check(near(s4->annotations().value(dr.value("index").toInt()).toMap().value("offset").toDouble(), 7.5),
          "sketch dimension: dragged label offset is kept");

    QTemporaryDir dir;
    const QString path = dir.filePath("sketch.dxf");
    s3->addCircle(40, 40, 4);
    doc.rebuild();
    QString err;
    QFile f(path);
    const bool wrote = s3->exportDxf(path, &err) && f.open(QIODevice::ReadOnly | QIODevice::Text);
    const QString dxf = wrote ? QString::fromLatin1(f.readAll()) : QString();
    check(wrote && dxf.contains("AC1009") && dxf.count("\nLINE\n") == 2 && dxf.contains("\nARC\n")
              && dxf.contains("\nCIRCLE\n") && dxf.trimmed().endsWith("EOF"),
          "sketch DXF: lines, arc and circle exported", err);
}

// Index of the first face of body `b` whose centroid passes `pred`; -1 if none
int findFace(FissionDocument *doc, int b, const std::function<bool(const gp_Pnt &)> &pred) {
    const auto faces = GeomUtils::faceMap(doc->body(b)->shape());
    for (int i = 1; i <= faces.Extent(); ++i)
        if (pred(GeomUtils::faceCentroid(TopoDS::Face(faces(i))))) return i - 1;
    return -1;
}

QVariantMap faceAt(FissionDocument *doc, int b, const std::function<bool(const gp_Pnt &)> &pred) {
    return GeomUtils::faceRef(b, doc->body(b)->shape(), findFace(doc, b, pred));
}

FissionFeature *box(FissionDocument *doc, double x, double y, double w, double h, double d, const QString &op = "newBody",
                    const QVariant &plane = "XY") {
    return extrude(doc, rectSketch(doc, plane, x, y, w, h), QString("%1 mm").arg(d), op);
}

void testSolidTools() {
    const auto top10 = [](const gp_Pnt &p) { return near(p.Z(), 10, 1e-6) && near(p.X(), 5, 1e-6); };
    {
        FissionDocument doc;
        box(&doc, 0, 0, 10, 10, 10);
        box(&doc, 5, 0, 10, 10, 10);
        auto *c = doc.addFeature("Combine", {{"target", 0}, {"tools", QVariantList{1}}, {"operation", "join"}});
        check(c->isHealthy() && doc.bodyCount() == 1 && near(totalVolume(&doc), 1500), "combine: join", status(&doc));
        c->set("operation", "cut");
        doc.rebuild();
        check(doc.bodyCount() == 1 && near(totalVolume(&doc), 500), "combine: cut removes the tool body", status(&doc));
        c->set("operation", "intersect");
        c->set("keepTools", true);
        doc.rebuild();
        check(doc.bodyCount() == 2 && near(totalVolume(&doc), 1500), "combine: intersect keeping the tool", status(&doc));
    }
    {
        FissionDocument doc;
        box(&doc, 0, 0, 10, 10, 10);
        auto *mv = doc.addFeature("Move", {{"bodies", QVariantList{0}}, {"translate", QVariantList{"20 mm", "0 mm", "5 mm"}}});
        double x0, y0, z0, x1, y1, z1;
        GeomUtils::boundingBox(doc.body(0)->shape()).Get(x0, y0, z0, x1, y1, z1);
        check(mv->isHealthy() && near(x0, 20, 1e-4) && near(z0, 5, 1e-4), "move: translate", status(&doc));
        mv->set("translate", QVariantList{"0 mm", "0 mm", "0 mm"});
        mv->set("moveType", "rotate");
        mv->set("angle", "90 deg");
        mv->set("copy", true);
        doc.rebuild();
        GeomUtils::boundingBox(doc.body(1)->shape()).Get(x0, y0, z0, x1, y1, z1);
        check(doc.bodyCount() == 2 && near(x0, -10, 1e-4) && near(x1, 0, 1e-4), "move: rotated copy about Z", status(&doc));

        auto *sc = doc.addFeature("Scale", {{"bodies", QVariantList{0}}, {"scale", "2"}});
        check(sc->isHealthy() && near(totalVolume(&doc), 9000), "scale: uniform x2 from the origin", status(&doc));
        sc->set("scaleType", "nonUniform");
        sc->set("scaleX", "3");
        doc.rebuild();
        check(near(totalVolume(&doc), 4000), "scale: non-uniform", status(&doc));
    }
    {
        FissionDocument doc;
        box(&doc, 0, 0, 10, 10, 10);
        auto *pl = doc.addFeature("ConstructionPlane", {{"mode", "offset"}, {"base", "XY"}, {"offset", "4 mm"}});
        auto *sb = doc.addFeature("SplitBody", {{"bodies", QVariantList{0}}, {"tool", "feature:" + pl->id()}});
        QList<double> vols;
        for (auto *b : doc.bodies()) vols << b->volume();
        std::sort(vols.begin(), vols.end());
        check(sb->isHealthy() && vols.size() == 2 && near(vols[0], 400) && near(vols[1], 600), "split body: by a plane",
              status(&doc));
        doc.removeFeature(sb);

        auto *rf = doc.addFeature("ReplaceFace", {{"faces", QVariantList{faceAt(&doc, 0, top10)}},
                                                  {"plane", "feature:" + pl->id()}});
        checkVolume(&doc, 400, "replace face: top face moved down to a plane");
        doc.removeFeature(rf);

        auto *of = doc.addFeature("OffsetFace", {{"faces", QVariantList{faceAt(&doc, 0, top10)}}, {"distance", "2 mm"}});
        check(of->isHealthy() && near(totalVolume(&doc), 1200), "offset face: top face pushed out 2 mm", status(&doc));
        doc.removeFeature(of);

        auto *xp = doc.addFeature("ConstructionPlane", {{"mode", "offset"}, {"base", "YZ"}, {"offset", "5 mm"}});
        auto *sf = doc.addFeature("SplitFace", {{"faces", QVariantList{faceAt(&doc, 0, top10)}}, {"tool", "feature:" + xp->id()}});
        check(sf->isHealthy() && GeomUtils::faceMap(doc.body(0)->shape()).Extent() == 7 && near(totalVolume(&doc), 1000),
              "split face: top face cut in two", status(&doc));
        doc.removeFeature(sf);

        const auto side = [](const gp_Pnt &p) { return near(p.X(), 10, 1e-6); };
        auto *dr = doc.addFeature("Draft", {{"faces", QVariantList{faceAt(&doc, 0, side)}}, {"plane", "XY"}, {"angle", "10 deg"}});
        const double wedge = 0.5 * 10 * 10 * std::tan(10 * M_PI / 180) * 10;
        check(dr->isHealthy() && near(std::fabs(totalVolume(&doc) - 1000), wedge, 1e-2), "draft: side face tilted 10 deg",
              status(&doc));
        doc.removeFeature(dr);

        auto *off = doc.addFeature("Offset", {{"bodies", QVariantList{0}}, {"distance", "1 mm"}});
        check(off->isHealthy() && doc.bodyCount() == 2 && totalVolume(&doc) > 2650, "offset: grown copy of a body",
              status(&doc));
    }
    {
        FissionDocument doc;
        auto *low = doc.addFeature("ConstructionPlane", {{"mode", "offset"}, {"base", "XY"}, {"offset", "-5 mm"}});
        box(&doc, 0, 0, 10, 10, 10, "newBody", "feature:" + low->id());
        const auto side = [](const gp_Pnt &p) { return near(p.X(), 10, 1e-6); };
        auto *dr = doc.addFeature("Draft", {{"faces", QVariantList{faceAt(&doc, 0, side)}}, {"plane", "XY"},
                                            {"angle", "10 deg"}, {"draftType", "parting"}});
        const double wedges = 2 * 0.5 * 5 * 5 * std::tan(10 * M_PI / 180) * 10;
        check(dr->isHealthy() && doc.bodyCount() == 1 && near(std::fabs(totalVolume(&doc) - 1000), wedges, 2e-2),
              "draft: parting line on both sides of a plane", status(&doc));
    }
    {
        FissionDocument doc;
        box(&doc, 0, 0, 2, 2, 2);
        auto *path = qobject_cast<FissionSketch *>(doc.addFeature("Sketch", {{"plane", "XY"}}));
        path->addLine(0, 0, 30, 0);
        doc.rebuild();
        auto *pp = doc.addFeature("PathPattern", {{"bodies", QVariantList{0}}, {"path", path->id()}, {"count", "4"}});
        check(pp->isHealthy() && doc.bodyCount() == 4 && near(totalVolume(&doc), 32), "path pattern: 4 along a line",
              status(&doc));
    }
    {
        FissionDocument doc;
        box(&doc, 0, 0, 40, 20, 5);
        auto *pl = doc.addFeature("ConstructionPlane", {{"mode", "offset"}, {"base", "XY"}, {"offset", "20 mm"}});
        auto *s = qobject_cast<FissionSketch *>(doc.addFeature("Sketch", {{"plane", "feature:" + pl->id()}}));
        s->addLine(5, 10, 35, 10);
        doc.rebuild();
        auto *rib = doc.addFeature("Rib", {{"sketch", s->id()}, {"thickness", "2 mm"}});
        double x0, y0, z0, x1, y1, z1;
        GeomUtils::boundingBox(doc.body(0)->shape()).Get(x0, y0, z0, x1, y1, z1);
        check(rib->isHealthy() && doc.bodyCount() == 1 && near(totalVolume(&doc), 4000 + 40 * 2 * 15, 2e-3),
              "rib: wall grows down to the plate and runs to its ends",
              QString("box %1..%2 %3..%4 %5..%6 %7").arg(x0).arg(x1).arg(y0).arg(y1).arg(z0).arg(z1).arg(status(&doc)));
        doc.removeFeature(rib);
        doc.removeFeature(s);
        doc.removeFeature(pl);

        auto *s2 = qobject_cast<FissionSketch *>(doc.addFeature("Sketch", {{"plane", "XY"}}));
        s2->addLine(5, 10, 35, 10);
        doc.rebuild();
        auto *web = doc.addFeature("Web", {{"sketch", s2->id()}, {"thickness", "2 mm"}, {"depth", "8 mm"}});
        check(web->isHealthy() && doc.bodyCount() == 1 && near(totalVolume(&doc), 4000 + 30 * 2 * 11, 2e-3),
              "web: symmetric wall of set depth joins the plate", status(&doc));
    }
}

void testThread() {
    FissionDocument rod;
    auto *c = qobject_cast<FissionSketch *>(rod.addFeature("Sketch", {{"plane", "XY"}}));
    c->addCircle(0, 0, 5);
    rod.rebuild();
    extrude(&rod, c, "20 mm");
    const double before = totalVolume(&rod);
    const auto cyl = [](const gp_Pnt &p) { return near(p.Z(), 10, 1e-6); };
    auto *th = rod.addFeature("Thread", {{"face", faceAt(&rod, 0, cyl)}});
    const double groove = before - totalVolume(&rod);
    check(th->isHealthy() && th->property("designation").toString() == "M10x1.5 external" && groove > 100 && groove < 300,
          "thread: modeled M10 on a shaft", QString("%1, groove %2, %3").arg(th->property("designation").toString())
                                                .arg(groove).arg(status(&rod)));
}

void testHoleAndThread() {
    testThread();
    {
        FissionDocument doc;
        box(&doc, 0, 0, 40, 20, 10, "join");
        auto *bs = qobject_cast<FissionSketch *>(doc.addFeature("Sketch", {{"plane", "XY"}}));
        bs->addCircle(20, 10, 3);
        doc.rebuild();
        extrude(&doc, bs, "10 mm", "cut");
        const double before = totalVolume(&doc);
        const auto bore = [](const gp_Pnt &p) { return near(p.Z(), 5, 1e-6) && std::hypot(p.X() - 20, p.Y() - 10) < 3; };
        std::printf("internal thread...\n");
        std::fflush(stdout);
        auto *th = doc.addFeature("Thread", {{"face", faceAt(&doc, 0, bore)}});
        const double groove = before - totalVolume(&doc);
        check(th->isHealthy() && th->property("designation").toString() == "M6x1 internal" && groove > 20 && groove < 120,
              "thread: modeled M6 in a bore", QString("%1, groove %2, %3").arg(th->property("designation").toString())
                                                   .arg(groove).arg(status(&doc)));
    }
    FissionDocument doc;
    box(&doc, 0, 0, 40, 20, 10, "join");
    const auto top = [](const gp_Pnt &p) { return near(p.Z(), 10, 1e-6); };
    const QVariantMap face = faceAt(&doc, 0, top);
    auto *h = doc.addFeature("Hole", {{"face", face}, {"center", QVariantList{"20", "10", "10"}}, {"diameter", "6 mm"},
                                      {"depth", "5 mm"}, {"tipAngle", "0 deg"}});
    checkVolume(&doc, 8000 - M_PI * 9 * 5, "hole: simple flat-bottom");
    const QVariantMap m = h->manipulator();
    check(m.value("key") == "depth" && near(m.value("origin").value<QVector3D>().z(), 10), "hole: depth handle on the face");
    h->set("tipAngle", "118 deg");
    doc.rebuild();
    checkVolume(&doc, 8000 - M_PI * 9 * 5 - M_PI * 9 * (3 / std::tan(59 * M_PI / 180)) / 3, "hole: drill-point tip", 2e-4);
    h->set("extent", "all");
    doc.rebuild();
    checkVolume(&doc, 8000 - M_PI * 9 * 10, "hole: through all");
    h->set("extent", "distance");
    h->set("depth", "8 mm");
    h->set("tipAngle", "0 deg");
    h->set("holeType", "counterbore");
    doc.rebuild();
    checkVolume(&doc, 8000 - M_PI * 25 * 3 - M_PI * 9 * 5, "hole: counterbore");
    h->set("holeType", "countersink");
    doc.rebuild();
    checkVolume(&doc, 8000 - M_PI * 2 / 3 * (25 + 15 + 9) - M_PI * 9 * 6, "hole: countersink 90 deg");
    h->set("holeType", "simple");
    h->set("depth", "5 mm");
    h->set("tapped", true);
    doc.rebuild();
    const double removed = 8000 - totalVolume(&doc);
    check(h->isHealthy() && removed > M_PI * 9 * 5 * 1.05 && removed < M_PI * 9 * 5 * 1.6,
          "hole: tapped M6 cuts a modeled thread", QString("removed %1, %2").arg(removed).arg(status(&doc)));
    h->set("modeled", false);
    doc.rebuild();
    checkVolume(&doc, 8000 - M_PI * 9 * 5, "hole: cosmetic thread leaves the bore plain");
    doc.removeFeature(h);

    auto *pts = qobject_cast<FissionSketch *>(doc.addFeature("Sketch", {{"plane", "XY"}}));
    pts->addPoint(10, 10);
    pts->addPoint(30, 10);
    doc.rebuild();
    doc.addFeature("Hole", {{"sketch", pts->id()}, {"diameter", "4 mm"}, {"depth", "6 mm"}, {"tipAngle", "0 deg"}});
    checkVolume(&doc, 8000 - 2 * M_PI * 4 * 6, "hole: one per sketch point, drilled into the body");
}

void testMeshBodies() {
    MeshUtils::Mesh m;
    QString err;
    {
        FissionDocument src;
        box(&src, 0, 0, 10, 10, 10);
        m = MeshUtils::fromShape(src.body(0)->shape());
    }
    check(m.tris.size() == 12 && MeshUtils::isClosed(m) && near(MeshUtils::volume(m), 1000, 1e-6),
          "mesh: tessellated box is closed with the right volume", QString("%1 tris").arg(m.tris.size()));
    MeshUtils::Mesh round;
    check(MeshUtils::decode(MeshUtils::encode(m), round) && round.tris.size() == 12 && near(MeshUtils::volume(round), 1000),
          "mesh: encode / decode round trip");

    FissionDocument doc;
    box(&doc, 0, 0, 10, 10, 10);
    QTemporaryDir dir;
    const QString stl = dir.filePath("box.stl");
    doc.exportBodies(stl, {0});
    const QString id = doc.insertMesh(stl, "mm");
    check(!id.isEmpty() && doc.bodyCount() == 2 && MeshUtils::isMesh(doc.body(1)->shape()) && near(totalVolume(&doc), 2000, 1e-4),
          "mesh: STL export then Insert Mesh gives a mesh body", doc.lastError() + " " + status(&doc));
    check(near(doc.bodyProperties(1).value("volume").toDouble(), 1000, 1e-4) && doc.bodyProperties(1).value("mesh").toBool(),
          "mesh: physical properties of a mesh body");
    check(doc.insertMesh(dir.filePath("missing.stl"), "mm").isEmpty(), "mesh: a missing file is an error");
    doc.removeFeature(doc.featureById(id));
    const QString cm = doc.insertMesh(stl, "cm");
    check(near(totalVolume(&doc), 1000 + 1000000, 1e-4), "mesh: insert in cm scales x10", status(&doc));
    doc.removeFeature(doc.featureById(cm));
    doc.insertMesh(stl, "mm");

    auto *sc = doc.addFeature("Scale", {{"bodies", QVariantList{1}}, {"scale", "2"}});
    check(!sc->isHealthy(), "mesh: solid tools refuse mesh bodies", sc->error());
    doc.removeFeature(sc);
    auto *mv = doc.addFeature("Move", {{"bodies", QVariantList{1}}, {"translate", QVariantList{"5 mm", "0 mm", "0 mm"}}});
    double mx0, my0, mz0, mx1, my1, mz1;
    GeomUtils::boundingBox(doc.body(1)->shape()).Get(mx0, my0, mz0, mx1, my1, mz1);
    check(mv->isHealthy() && MeshUtils::isMesh(doc.body(1)->shape()) && near(mx0, 5, 1e-4)
              && near(MeshUtils::volume(MeshUtils::fromShape(doc.body(1)->shape())), 1000, 1e-4),
          "mesh: Move works on mesh bodies", status(&doc));
    doc.removeFeature(mv);

    auto *cut = doc.addFeature("MeshPlaneCut", {{"bodies", QVariantList{1}}, {"plane", "feature:" +
        doc.addFeature("ConstructionPlane", {{"mode", "offset"}, {"base", "XY"}, {"offset", "4 mm"}})->id()}});
    MeshUtils::Mesh after = MeshUtils::fromShape(doc.body(1)->shape());
    check(cut->isHealthy() && near(MeshUtils::volume(after), 400, 1e-4) && MeshUtils::isClosed(after),
          "mesh: plane cut keeps the part below and fills the cut", status(&doc));
    cut->set("fill", false);
    doc.rebuild();
    after = MeshUtils::fromShape(doc.body(1)->shape());
    check(cut->isHealthy() && !MeshUtils::isClosed(after), "mesh: plane cut without fill leaves it open");
    cut->set("fill", true);
    cut->set("offset", "2 mm");
    doc.rebuild();
    {
        const QVariantMap mp = cut->manipulator();
        const QVector3D o = mp.value("origin").value<QVector3D>(), d = mp.value("direction").value<QVector3D>();
        check(cut->isHealthy() && near(MeshUtils::volume(MeshUtils::fromShape(doc.body(1)->shape())), 600, 1e-4)
                  && mp.value("key") == "offset" && near(mp.value("value").toDouble(), 2) && near(o.z(), 4) && d.z() > 0.99,
              "mesh plane cut: the offset moves the plane, with an arrow on the plane to drag it",
              QString("%1, origin z %2").arg(MeshUtils::volume(MeshUtils::fromShape(doc.body(1)->shape()))).arg(o.z()));
    }
    cut->set("offset", "0 mm");
    doc.rebuild();

    auto *rev = doc.addFeature("MeshReverse", {{"bodies", QVariantList{1}}});
    after = MeshUtils::fromShape(doc.body(1)->shape());
    check(rev->isHealthy() && near(MeshUtils::volume(after), -400, 1e-4), "mesh: reverse normals flips the volume sign",
          QString::number(MeshUtils::volume(after)));
    doc.removeFeature(rev);

    auto *conv = doc.addFeature("MeshConvert", {{"bodies", QVariantList{1}}, {"direction", "toBRep"}});
    check(conv->isHealthy() && !MeshUtils::isMesh(doc.body(1)->shape()) && near(doc.body(1)->volume(), 400, 1e-4),
          "mesh: convert to a solid body", status(&doc));
    auto *comb = doc.addFeature("Combine", {{"target", 0}, {"tools", QVariantList{1}}, {"operation", "cut"}});
    checkVolume(&doc, 600, "mesh: converted solid works with Combine");
    doc.removeFeature(comb);
    doc.removeFeature(conv);

    auto *toMesh = doc.addFeature("MeshConvert", {{"bodies", QVariantList{0}}, {"direction", "toMesh"}});
    check(toMesh->isHealthy() && MeshUtils::isMesh(doc.body(0)->shape()) && near(totalVolume(&doc), 1400, 1e-4),
          "mesh: convert a solid to a mesh", status(&doc));
    doc.removeFeature(toMesh);

    // A finely tessellated cylinder for smoothing and reduction
    FissionDocument ball;
    auto *c = qobject_cast<FissionSketch *>(ball.addFeature("Sketch", {{"plane", "XY"}}));
    c->addCircle(0, 0, 10);
    ball.rebuild();
    extrude(&ball, c, "20 mm");
    ball.addFeature("MeshConvert", {{"direction", "toMesh"}, {"refinement", "fine"}});
    const MeshUtils::Mesh sphere = MeshUtils::fromShape(ball.body(0)->shape());
    auto *red = ball.addFeature("MeshReduce", {{"proportion", "25"}});
    const MeshUtils::Mesh reduced = MeshUtils::fromShape(ball.body(0)->shape());
    check(red->isHealthy() && reduced.tris.size() < sphere.tris.size() * 0.4 && reduced.tris.size() > sphere.tris.size() * 0.1 &&
              near(MeshUtils::volume(reduced), MeshUtils::volume(sphere), 0.05),
          "mesh: reduce to about a quarter keeps the shape",
          QString("%1 -> %2 tris, %3 %4").arg(sphere.tris.size()).arg(reduced.tris.size()).arg(status(&ball)).arg(red->error()));
    ball.removeFeature(red);
    auto *sm = ball.addFeature("MeshSmooth", {{"iterations", "3"}, {"smoothness", "0.5"}});
    const MeshUtils::Mesh smoothed = MeshUtils::fromShape(ball.body(0)->shape());
    const double vs = MeshUtils::volume(smoothed);
    double moved = 0;
    for (size_t i = 0; i < smoothed.nodes.size() && i < sphere.nodes.size(); ++i)
        moved = std::max(moved, smoothed.nodes[i].Distance(sphere.nodes[i]));
    check(sm->isHealthy() && moved > 0.1 && vs > 0.85 * MeshUtils::volume(sphere) && vs < 1.05 * MeshUtils::volume(sphere),
          "mesh: smoothing rounds a cylinder without shrinking it much",
          QString("%1 vs %2, moved %3").arg(vs).arg(MeshUtils::volume(sphere)).arg(moved));
}

void testMeshRepairShellCombine() {
    FissionDocument doc;
    box(&doc, 0, 0, 10, 10, 10);
    doc.addFeature("MeshConvert", {{"bodies", QVariantList{0}}, {"direction", "toMesh"}});
    const MeshUtils::Mesh cube = MeshUtils::fromShape(doc.body(0)->shape());

    // Damage a copy: unwelded duplicate of one triangle, two flipped triangles, one missing
    MeshUtils::Mesh bad = cube;
    bad.nodes.push_back(bad.nodes[bad.tris[0][0]]);
    bad.tris.push_back({int(bad.nodes.size()) - 1, bad.tris[0][1], bad.tris[0][2]});
    std::swap(bad.tris[1][1], bad.tris[1][2]);
    std::swap(bad.tris[2][1], bad.tris[2][2]);
    bad.tris.erase(bad.tris.begin() + 5);
    check(!MeshUtils::isClosed(bad), "mesh repair: the damaged cube is open");
    const MeshUtils::RepairStats st = MeshUtils::repair(bad, 1e-6, true, true);
    check(st.closed && st.welded == 1 && st.duplicates == 1 && st.flipped == 2 && st.holes == 1
              && near(MeshUtils::volume(bad), 1000, 1e-6),
          "mesh repair: welds, drops the duplicate, re-orients and fills the hole",
          QString("welded %1 dup %2 flipped %3 holes %4 vol %5").arg(st.welded).arg(st.duplicates).arg(st.flipped)
              .arg(st.holes).arg(MeshUtils::volume(bad)));
    MeshUtils::Mesh inside = cube;
    MeshUtils::reverse(inside);
    MeshUtils::repair(inside, 1e-6, true, false);
    check(near(MeshUtils::volume(inside), 1000, 1e-6), "mesh repair: an inside-out closed mesh is turned outward");

    auto *rep = doc.addFeature("MeshRepair", {});
    check(rep->isHealthy() && rep->property("summary").toString().contains("closed"), "mesh repair: feature on a clean mesh",
          rep->property("summary").toString() + " " + rep->error());
    doc.removeFeature(rep);

    auto *sh = doc.addFeature("MeshShell", {{"thickness", "1 mm"}});
    const MeshUtils::Mesh hollow = MeshUtils::fromShape(doc.body(0)->shape());
    // Corner normals move the offset by at most the thickness, so the wall holds between 0 and a 1 mm box wall
    const double vIn = MeshUtils::volume(hollow);
    check(sh->isHealthy() && MeshUtils::isClosed(hollow) && vIn > 100 && vIn < 1000 - 512 + 1e-6,
          "mesh shell: hollows a closed mesh inward", QString::number(vIn) + " " + sh->error());
    {
        const QVariantMap mp = sh->manipulator();
        const QVector3D d = mp.value("direction").value<QVector3D>();
        check(mp.value("key") == "thickness" && near(mp.value("value").toDouble(), 1) && d.z() < -0.99,
              "mesh shell: a thickness arrow on top of the body points into it", QString("dir z %1").arg(d.z()));
    }
    sh->set("direction", "outside");
    doc.rebuild();
    const double vOut = MeshUtils::volume(MeshUtils::fromShape(doc.body(0)->shape()));
    check(sh->isHealthy() && vOut > 100 && vOut < 1728 - 1000 + 1e-6, "mesh shell: outward keeps the original as the inside",
          QString::number(vOut));
    doc.removeFeature(sh);

    MeshUtils::Mesh open = cube;
    open.tris.erase(open.tris.begin());
    QString err;
    const bool thick = MeshUtils::shell(open, 0.5, false, err);
    check(thick && MeshUtils::isClosed(open) && MeshUtils::volume(open) > 0,
          "mesh shell: an open mesh is thickened into a closed wall", err);

    box(&doc, 20, 0, 10, 10, 10);
    doc.addFeature("MeshConvert", {{"bodies", QVariantList{1}}, {"direction", "toMesh"}});
    auto *comb = doc.addFeature("MeshCombine", {{"bodies", QVariantList{0, 1}}});
    check(comb->isHealthy() && doc.bodyCount() == 1 && MeshUtils::isMesh(doc.body(0)->shape())
              && near(MeshUtils::volume(MeshUtils::fromShape(doc.body(0)->shape())), 2000, 1e-6),
          "mesh combine: two mesh bodies become one", status(&doc));
    comb->set("bodies", QVariantList{0});
    doc.rebuild();
    check(!comb->isHealthy(), "mesh combine: needs two bodies");
}

QVariantMap edgeAt(FissionDocument *doc, int b, const std::function<bool(const gp_Pnt &)> &pred);

void testInspect() {
    FissionDocument doc;
    box(&doc, 0, 0, 10, 10, 10);
    box(&doc, 15, 0, 10, 10, 10);
    const auto right0 = [](const gp_Pnt &p) { return near(p.X(), 10, 1e-6); };
    const auto left1 = [](const gp_Pnt &p) { return near(p.X(), 15, 1e-6); };
    const QVariantMap a = faceAt(&doc, 0, right0), b = faceAt(&doc, 1, left1);
    QVariantMap r = doc.measure({a, b});
    check(r.value("ok").toBool() && near(r.value("distance").toDouble(), 5) && near(r.value("angle").toDouble(), 0, 1e-6),
          "measure: distance and angle between parallel faces",
          QJsonDocument::fromVariant(r).toJson(QJsonDocument::Compact));
    const auto top0 = [](const gp_Pnt &p) { return near(p.Z(), 10, 1e-6) && p.X() < 10; };
    r = doc.measure({a, faceAt(&doc, 0, top0)});
    check(near(r.value("angle").toDouble(), 90, 1e-6) && near(r.value("distance").toDouble(), 0, 1e-6),
          "measure: perpendicular faces touch at 90 deg");
    r = doc.measure({QVariantMap{{"kind", "body"}, {"body", 0}}});
    bool hasVolume = false;
    for (const QVariant &l : r.value("lines").toList())
        if (l.toList().value(0) == "Volume" && l.toList().value(1).toString().startsWith("1000")) hasVolume = true;
    check(hasVolume, "measure: body volume", QJsonDocument::fromVariant(r).toJson(QJsonDocument::Compact));
    check(!doc.measure({}).value("ok").toBool(), "measure: nothing selected");

    // Two parallel vertical edges of the first box: distance = box width (10), angle = 0
    const auto eLeft = [](const gp_Pnt &p) { return near(p.X(), 0, 1e-6) && near(p.Y(), 0, 1e-6); };
    const auto eRight = [](const gp_Pnt &p) { return near(p.X(), 10, 1e-6) && near(p.Y(), 0, 1e-6); };
    r = doc.measure({edgeAt(&doc, 0, eLeft), edgeAt(&doc, 0, eRight)});
    check(r.value("ok").toBool() && near(r.value("distance").toDouble(), 10) && near(r.value("angle").toDouble(), 0, 1e-6),
          "measure: distance and angle between parallel edges",
          QJsonDocument::fromVariant(r).toJson(QJsonDocument::Compact));
    // Front bottom edge (along X) and left vertical edge (along Z): 90°
    const auto eFront = [](const gp_Pnt &p) { return near(p.Z(), 0, 1e-6) && near(p.Y(), 0, 1e-6) && p.X() > 2 && p.X() < 8; };
    r = doc.measure({edgeAt(&doc, 0, eFront), edgeAt(&doc, 0, eLeft)});
    check(near(r.value("angle").toDouble(), 90, 1e-6),
          "measure: angle between perpendicular edges",
          QJsonDocument::fromVariant(r).toJson(QJsonDocument::Compact));

    check(doc.interference({}).isEmpty(), "interference: separate bodies do not clash");
    doc.addFeature("Move", {{"bodies", QVariantList{1}}, {"translate", QVariantList{"-8 mm", "0 mm", "0 mm"}}});
    const QVariantList clashes = doc.interference({});
    check(clashes.size() == 1 && near(clashes[0].toMap().value("volume").toDouble(), 300),
          "interference: overlapping bodies report the shared volume", QJsonDocument::fromVariant(clashes).toJson(QJsonDocument::Compact));
}

QVariantMap edgeAt(FissionDocument *doc, int b, const std::function<bool(const gp_Pnt &)> &pred) {
    const auto edges = GeomUtils::edgeMap(doc->body(b)->shape());
    for (int i = 1; i <= edges.Extent(); ++i)
        if (pred(GeomUtils::edgeMidpoint(TopoDS::Edge(edges(i))))) return GeomUtils::edgeRef(b, doc->body(b)->shape(), i - 1);
    return {};
}

int faceCount(FissionDocument *doc, int b) { return GeomUtils::faceMap(doc->body(b)->shape()).Extent(); }

void testRemoveDraftHoleEdges() {
    FissionDocument doc;
    box(&doc, 0, 0, 10, 10, 10);
    box(&doc, 20, 0, 10, 10, 5);
    auto *rm = doc.addFeature("Remove", {{"bodies", QVariantList{0}}});
    check(rm->isHealthy() && doc.bodyCount() == 1 && near(totalVolume(&doc), 500), "remove: deletes the picked body",
          status(&doc));
    rm->set("bodies", QVariantList{0, 1});
    doc.rebuild();
    check(rm->isHealthy() && doc.bodyCount() == 0, "remove: several bodies at once", status(&doc));
    rm->set("bodies", QVariantList{7});
    doc.rebuild();
    check(!rm->isHealthy() && doc.bodyCount() == 2, "remove: a missing body is an error", rm->error());
    doc.removeFeature(rm);

    const auto top = [](const gp_Pnt &p) { return near(p.Z(), 10, 1e-6) && p.X() < 10; };
    check(doc.faceEdges(faceAt(&doc, 0, top)).size() == 4, "edges of a face: a box top has 4");

    // Draft: a face as the pull direction
    const double v0 = totalVolume(&doc);
    const auto side = [](const gp_Pnt &p) { return near(p.X(), 0, 1e-6); };
    auto *dr = doc.addFeature("Draft", {{"plane", faceAt(&doc, 0, top)}, {"faces", QVariantList{faceAt(&doc, 0, side)}},
                                        {"angle", "5 deg"}});
    check(dr->isHealthy() && !near(totalVolume(&doc), v0, 1e-4), "draft: a picked face sets the pull direction", status(&doc));
    doc.removeFeature(dr);

    // Hole: move handle with snap points on the face
    FissionDocument hd;
    box(&hd, 0, 0, 40, 20, 10, "join");
    auto *ring = qobject_cast<FissionSketch *>(hd.addFeature("Sketch", {{"plane", "XY"}}));
    ring->addCircle(8, 10, 2);
    hd.rebuild();
    extrude(&hd, ring, "10 mm", "cut");
    const auto htop = [](const gp_Pnt &p) { return near(p.Z(), 10, 1e-6); };
    auto *h = hd.addFeature("Hole", {{"face", faceAt(&hd, 0, htop)}, {"center", QVariantList{"30", "10", "10"}},
                                     {"diameter", "4 mm"}, {"depth", "5 mm"}, {"tipAngle", "0 deg"}});
    const QVariantMap mv = h->manipulator().value("move").toMap();
    QStringList labels;
    bool circle = false;
    for (const QVariant &s : mv.value("snaps").toList()) {
        const QVariantList l = s.toList();
        labels << l.value(3).toString();
        if (l.value(3) == "Circle center" && near(l.value(0).toDouble(), 8) && near(l.value(1).toDouble(), 10)) circle = true;
    }
    check(h->isHealthy() && mv.value("key") == "center" && labels.contains("Face center") && circle,
          "hole: move handle snaps to the face center and other holes", labels.join(", "));
}

void testSectionAnalyses() {
    FissionDocument doc;
    box(&doc, 0, 0, 20, 10, 10);
    QVariantMap f = doc.planeFrame("XZ");
    check(f.value("ok").toBool() && near(f.value("normal").toList().value(1).toDouble(), -1), "section: XZ plane frame");
    const auto top = [](const gp_Pnt &p) { return near(p.Z(), 10, 1e-6); };
    f = doc.planeFrame(faceAt(&doc, 0, top));
    check(f.value("ok").toBool() && near(f.value("origin").toList().value(2).toDouble(), 10)
              && near(f.value("normal").toList().value(2).toDouble(), 1),
          "section: a planar face gives a cut plane");
    auto *cp = doc.addFeature("ConstructionPlane", {{"mode", "offset"}, {"base", "XY"}, {"offset", "4 mm"}});
    f = doc.planeFrame(QVariantMap{{"kind", "plane"}, {"id", cp->id()}});
    check(f.value("ok").toBool() && near(f.value("origin").toList().value(2).toDouble(), 4), "section: a construction plane");

    const QString a = doc.addAnalysis({{"type", "section"}, {"plane", "XY"}, {"offset", 5.0}});
    const QString b = doc.addAnalysis({{"type", "section"}, {"plane", "YZ"}, {"offset", 10.0}});
    QVariantList list = doc.analyses();
    check(list.size() == 2 && doc.analysis(a).value("name") == "Section1" && doc.analysis(b).value("name") == "Section2"
              && !doc.analysis(a).value("visible").toBool() && doc.analysis(b).value("visible").toBool(),
          "section: saved analyses, only the newest shown");
    doc.updateAnalysis(a, {{"visible", true}});
    check(doc.analysis(a).value("visible").toBool() && !doc.analysis(b).value("visible").toBool(),
          "section: showing one hides the other");
    doc.updateAnalysis(a, {{"offset", 7.5}});
    QTemporaryDir dir;
    const QString path = dir.filePath("sections.f360");
    doc.save(path);
    FissionDocument loaded;
    loaded.load(path);
    check(loaded.analyses().size() == 2 && near(loaded.analysis(a).value("offset").toDouble(), 7.5),
          "section: analyses are saved with the design");
    doc.removeAnalysis(b);
    check(doc.analyses().size() == 1, "section: delete an analysis");
    doc.undo();
    check(doc.analyses().size() == 2, "section: undo brings it back");
}

// The progress hub long operations report through (the status bar's progress bar listens)
void testProgress() {
    struct Event { QString id, label; double fraction; bool done; };
    std::vector<Event> events;
    FissionProgress::setSink([&](const QString &id, const QString &label, double fraction, bool done) {
        events.push_back({id, label, fraction, done});
    });
    FissionProgress::begin("job", "Converting");
    const auto cb = FissionProgress::callback("job", 0.2, 0.7);
    cb(0.5, "Fitting surfaces");
    cb(-1, "Unknown");
    FissionProgress::end("job");
    FissionProgress::setSink({});
    FissionProgress::report("job", 0.9);   // no sink: dropped
    check(events.size() == 4 && events[0].fraction < 0 && !events[0].done && events[0].label == "Converting"
              && near(events[1].fraction, 0.45, 1e-9) && events[1].label == "Fitting surfaces" && events[2].fraction < 0
              && events[3].done && events[3].id == "job",
          "progress: begin / report (mapped onto the task's range) / end reach the sink", QString::number(events.size()));
}

// Conversion and accurate face groups of big meshes run on a worker thread, reporting progress, and the
// feature picks the result up on the next rebuild
void testBackgroundJobs() {
    if (!QCoreApplication::instance()) return;
    struct Event { QString id, label; double fraction; bool done; };
    std::vector<Event> events;
    FissionProgress::setSink([&](const QString &id, const QString &label, double fraction, bool done) {
        events.push_back({id, label, fraction, done});
    });
    FissionMeshConvert::setBackgroundTriangles(0);   // everything in the background
    auto wait = [](FissionFeature *f) {
        QElapsedTimer t;
        t.start();
        while (!f->isHealthy() && t.elapsed() < 30000) QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
        // The queued progress events arrive on this thread too
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    };

    FissionDocument doc;
    box(&doc, 0, 0, 20, 20, 10);
    doc.addFeature("MeshConvert", {{"direction", "toMesh"}, {"refinement", "medium"}});
    auto *c = doc.addFeature("MeshConvert", {{"direction", "toBRep"}});
    const QString first = c->error();
    wait(c);
    check(first.contains("in the background") && c->isHealthy() && faceCount(&doc, 0) == 6,
          "background: a conversion reports it is running, then the result arrives on its own",
          QString("%1 -> %2 / %3 faces").arg(first, status(&doc)).arg(faceCount(&doc, 0)));
    const bool began = !events.empty() && !events.front().done && events.front().label.startsWith("Converting");
    int reports = 0;
    for (const Event &e : events) reports += !e.done && e.fraction >= 0;
    check(began && reports >= 2 && events.back().done, "background: the conversion reports its progress to the status bar",
          QString("%1 events, %2 with a fraction").arg(events.size()).arg(reports));

    events.clear();
    FissionDocument grouped;
    box(&grouped, 0, 0, 20, 20, 10);
    grouped.addFeature("MeshConvert", {{"direction", "toMesh"}, {"refinement", "medium"}});
    auto *g = grouped.addFeature("MeshFaceGroups", {{"method", "accurate"}});
    const QString gFirst = g->error();
    wait(g);
    check(gFirst.contains("in the background") && g->isHealthy() && g->property("summary").toString().contains("6 face groups"),
          "background: accurate face groups run in the background too",
          QString("%1 -> %2 / %3").arg(gFirst, status(&grouped), g->property("summary").toString()));
    check(!events.empty() && events.front().label.startsWith("Grouping") && events.back().done,
          "background: face groups report their progress", QString::number(events.size()));

    FissionMeshConvert::setBackgroundTriangles(std::numeric_limits<int>::max());
    FissionProgress::setSink({});
}

void testDrawing() {
    DrawingManager mgr;
    check(mgr.drawingCount() == 0, "drawing: starts with no drawings");
    const QVariantMap input = mgr.createDrawingInput("design-1");
    check(input.value("standard").toString() == "ISO" && input.value("sheetSize").toString() == "A3"
              && input.value("projectionAngle").toString() == "third",
          "drawing: createDrawingInput defaults (ISO, A3, third angle)");
    const QString id = mgr.createDrawingDocument(input);
    check(!id.isEmpty() && mgr.drawingCount() == 1, "drawing: createDrawingDocument returns an id");
    DrawingDocument *doc = mgr.drawingById(id);
    check(doc && doc->sheetCount() == 1 && doc->standard() == "ISO", "drawing: new drawing has one sheet");
    DrawingSheet *sheet = doc->sheet(0);
    check(sheet && sheet->size() == "A3" && sheet->orientation() == "landscape" && sheet->width() > sheet->height(),
          "drawing: default sheet is A3 landscape",
          QString("%1 x %2").arg(sheet ? sheet->width() : 0).arg(sheet ? sheet->height() : 0));
    DrawingView *view = sheet->addView("base", {{"position", QPointF(40, 50)}, {"orientation", "front"}, {"scale", 2.0}});
    check(view && sheet->viewCount() == 1 && view->type() == "base" && view->orientation() == "front"
              && near(view->scale(), 2.0) && near(view->position().x(), 40),
          "drawing: base view on the sheet");
    check(doc->addSheet("Sheet2", "A4", "portrait") && doc->sheetCount() == 2
              && doc->sheet(1)->orientation() == "portrait" && doc->sheet(1)->height() > doc->sheet(1)->width(),
          "drawing: add a portrait A4 sheet");
    check(doc->addCustomSheet("Wide", 500, 300, "landscape") && doc->sheetCount() == 3
              && doc->sheet(2)->size() == "custom" && doc->sheet(2)->width() >= doc->sheet(2)->height(),
          "drawing: custom sheet size");

    // Project a box design onto the base view
    FissionDocument part;
    box(&part, 0, 0, 40, 20, 10);
    const bool baseOk = mgr.projectView(view, &part, doc);
    check(baseOk && view->projected() && view->visibleCurves().size() >= 4,
          "drawing: HLR projects the design onto a base view",
          QString("%1 visible segs, %2x%3 mm")
              .arg(view->visibleCurves().size()).arg(view->width(), 0, 'f', 1).arg(view->height(), 0, 'f', 1));
    DrawingView *proj = sheet->addView("projected",
        {{"position", QPointF(200, 50)}, {"parentViewId", view->id()}, {"projectDirection", "right"}});
    const bool projOk = mgr.projectView(proj, &part, doc);
    check(projOk && proj->projected() && proj->orientation() == "right",
          "drawing: projected view resolves third-angle orientation", proj->orientation());
    DrawingView *sec = sheet->addView("section",
        {{"position", QPointF(40, 160)}, {"orientation", "front"}, {"projectDirection", "right"},
         {"parentViewId", view->id()}});
    check(mgr.projectView(sec, &part, doc) && sec->projected(), "drawing: section view projects");
    DrawingView *det = sheet->addView("detail",
        {{"position", QPointF(200, 160)}, {"orientation", "front"}, {"detailRadius", 25.0},
         {"detailCenter", QPointF(0, 0)}, {"scale", 2.0}, {"parentViewId", view->id()}});
    check(mgr.projectView(det, &part, doc) && det->projected(), "drawing: detail view projects",
          mgr.lastError());

    const QString dimId = sheet->addAnnotation(
        {{"kind", "dimension"}, {"x", 10}, {"y", 10}, {"x2", 50}, {"y2", 10}, {"text", "40 mm"}});
    sheet->addAnnotation({{"kind", "text"}, {"x", 12}, {"y", 30}, {"text", "Note"}, {"fontSize", 3.5}});
    sheet->addAnnotation({{"kind", "centerMark"}, {"x", 60}, {"y", 40}, {"radius", 3}});
    check(!dimId.isEmpty() && sheet->annotationCount() == 3, "drawing: dimensions and notes on the sheet");
    DrawingSketch *sk = sheet->addSketch("Title");
    check(sk && !sk->addLine(0, 0, 10, 0).isEmpty() && sk->curveCount() == 1, "drawing: sheet sketch curves");

    sheet->setPartName("Stock");
    sheet->setDrawnBy("Test");
    sheet->setRevision("A");
    const QString pdf = QDir::temp().filePath("f360_drawing_test.pdf");
    const QString dxf = QDir::temp().filePath("f360_drawing_test.dxf");
    const QString png = QDir::temp().filePath("f360_drawing_test.png");
    QFile::remove(pdf); QFile::remove(dxf); QFile::remove(png);
    check(mgr.exportPdf(sheet, pdf) && QFileInfo::exists(pdf), "drawing: export PDF", mgr.lastError());
    check(mgr.exportDxf(sheet, dxf) && QFileInfo::exists(dxf), "drawing: export DXF", mgr.lastError());
    check(mgr.exportPng(sheet, png) && QFileInfo::exists(png), "drawing: export PNG", mgr.lastError());
    check(DrawingProjector::projectedOrientation("front", "up", "third") == "top"
              && DrawingProjector::projectedOrientation("front", "up", "first") == "bottom",
          "drawing: first- vs third-angle projection");

    const QString bom = mgr.addPartsList(sheet, &part, QPointF(10, 80));
    const QString rev = mgr.addRevisionTable(sheet, QPointF(10, 120));
    check(!bom.isEmpty() && !rev.isEmpty() && sheet->tableCount() >= 2, "drawing: parts list and revision table");
    doc->pushHistory("Added parts list");
    check(doc->history().size() >= 1, "drawing: history records sheet actions");
    const QString tiff = QDir::temp().filePath("f360_drawing_test.tif");
    QFile::remove(tiff);
    check(mgr.exportTiff(sheet, tiff) && QFileInfo::exists(tiff), "drawing: export TIFF", mgr.lastError());

    // Design linkage / out-of-date tracking (drawing stamped to this design's documentId)
    DrawingManager syncMgr;
    QVariantMap syncIn = syncMgr.createDrawingInput(part.documentId());
    syncIn[QStringLiteral("contentRevision")] = part.contentRevision();
    const QString syncId = syncMgr.createDrawingDocument(syncIn);
    DrawingDocument *syncDoc = syncMgr.drawingById(syncId);
    part.linkAsset(QStringLiteral("drawing"), syncId, syncDoc ? syncDoc->name() : QStringLiteral("Drawing"));
    check(syncDoc && !syncDoc->outOfDate() && !part.anyLinkedAssetOutOfDate(),
          "drawing: linked drawing is current at stamp");
    box(&part, 50, 0, 10, 10, 10);   // another edit → contentRevision bumps
    syncMgr.refreshDrawingSync(&part);
    check(syncDoc->outOfDate() && part.anyLinkedAssetOutOfDate(),
          "drawing: design edit marks drawing out of date",
          QString("src=%1 design=%2").arg(syncDoc->sourceDesignRevision()).arg(part.contentRevision()));
    const int updated = syncMgr.updateDrawingFromDesign(syncDoc, &part);
    check(updated >= 0 && !syncDoc->outOfDate()
              && syncDoc->sourceDesignRevision() == part.contentRevision(),
          "drawing: updateDrawingFromDesign clears out-of-date",
          QString("n=%1 src=%2").arg(updated).arg(syncDoc->sourceDesignRevision()));
}

// Fusion bulk exports embed large per-step STEP blobs (snapshotStepB64). Saving used to pretty-print
// the whole document twice on the UI thread and freeze Windows ("Not Responding"). This builds a
// synthetic large timeline, times save, and checks progress is reported so the event loop can pump.
void testLargeFileSave() {
    if (!QCoreApplication::instance()) return;
    QTemporaryDir dir;
    check(dir.isValid(), "large-save: temp dir");

    constexpr int kFeatures = 32;
    constexpr int kBlobBytes = 256 * 1024; // 32 × 256 KiB ≈ 8 MiB raw → ~11 MiB base64 in the file
    const QString blob = QString::fromLatin1(QByteArray(kBlobBytes, char('Q')).toBase64());

    QJsonArray features;
    for (int i = 0; i < kFeatures; ++i) {
        features.append(QJsonObject{
            {QStringLiteral("id"), QStringLiteral("pt%1").arg(i)},
            {QStringLiteral("type"), QStringLiteral("ConstructionPoint")},
            {QStringLiteral("name"), QStringLiteral("Point%1").arg(i)},
            {QStringLiteral("suppressed"), false},
            {QStringLiteral("visible"), true},
            {QStringLiteral("props"),
             QJsonObject{{QStringLiteral("x"), QString::number(i)},
                         {QStringLiteral("y"), QStringLiteral("0")},
                         {QStringLiteral("z"), QStringLiteral("0")},
                         {QStringLiteral("snapshotStepB64"), blob}}},
        });
    }
    QJsonObject root{
        {QStringLiteral("format"), QStringLiteral("Fission360")},
        {QStringLiteral("version"), 1},
        {QStringLiteral("name"), QStringLiteral("LargeSave")},
        {QStringLiteral("documentId"), QStringLiteral("large-save-test")},
        {QStringLiteral("contentRevision"), 1},
        {QStringLiteral("timeline"),
         QJsonObject{{QStringLiteral("marker"), kFeatures}, {QStringLiteral("features"), features}}},
        {QStringLiteral("parameters"), QJsonArray{}},
        {QStringLiteral("bodyDisplay"), QJsonArray{}},
    };

    FissionDocument doc;
    doc.fromJson(root);
    check(doc.timeline()->count() == kFeatures, "large-save: timeline loaded",
          QString::number(doc.timeline()->count()));

    int progressReports = 0;
    bool saveBegan = false, saveEnded = false;
    FissionProgress::setSink([&](const QString &id, const QString &, double, bool done) {
        if (id != QLatin1String("save")) return;
        if (done) saveEnded = true;
        else {
            saveBegan = true;
            ++progressReports;
        }
    });

    const QString path = dir.filePath(QStringLiteral("large.f360"));
    QElapsedTimer t;
    t.start();
    const bool ok = doc.save(path);
    const qint64 ms = t.elapsed();
    FissionProgress::setSink({});

    check(ok, "large-save: save succeeds", doc.lastError());
    const qint64 bytes = QFileInfo(path).size();
    check(bytes > qint64(kFeatures) * qint64(kBlobBytes),
          "large-save: file holds the embedded STEP blobs",
          QStringLiteral("%1 bytes").arg(bytes));
    // Compact serialize + single pass should finish a ~10 MiB design well under this; the old
    // Indented + double toJson path routinely hung for tens of seconds on similar sizes.
    check(ms < 8000, "large-save: finishes within 8 s (UI must not freeze)",
          QStringLiteral("%1 ms, %2 MB").arg(ms).arg(bytes / (1024.0 * 1024.0), 0, 'f', 1));
    check(saveBegan && saveEnded && progressReports >= 2,
          "large-save: reports progress so the UI can pump events",
          QStringLiteral("began=%1 ended=%2 reports=%3").arg(saveBegan).arg(saveEnded).arg(progressReports));

    FissionDocument reloaded;
    check(reloaded.load(path) && reloaded.timeline()->count() == kFeatures,
          "large-save: reload keeps the timeline",
          QStringLiteral("%1 / %2").arg(reloaded.timeline()->count()).arg(reloaded.lastError()));
    const QString roundTrip =
        reloaded.timeline()->at(0)->get(QStringLiteral("snapshotStepB64")).toString();
    check(roundTrip.size() == blob.size(), "large-save: STEP blob survives round-trip",
          QStringLiteral("%1 vs %2").arg(roundTrip.size()).arg(blob.size()));
}

void testDocumentVersionsAndAutosave() {
    QTemporaryDir tmp;
    check(tmp.isValid(), "versions: temp dir");
    DocumentVersionStore::instance()->setMaxVersions(4);

    FissionDocument part;
    box(&part, 0, 0, 20, 10, 5);
    const QString path = QDir(tmp.path()).filePath(QStringLiteral("bracket.f360"));
    check(part.save(path) && part.versionNumber() == 1, "versions: first save is v1");
    const QVariantList hist1 = part.versionHistory();
    check(hist1.size() == 1 && hist1[0].toMap().value(QStringLiteral("version")).toInt() == 1,
          "versions: history after first save");

    box(&part, 30, 0, 8, 8, 8);
    check(part.isModified(), "versions: edit dirties the design");
    check(part.writeAutosaveNow(), "versions: writeAutosaveNow");
    const QVariantList pending = FissionDocument::pendingAutosaves();
    bool found = false;
    for (const QVariant &v : pending)
        if (v.toMap().value(QStringLiteral("documentId")).toString() == part.documentId())
            found = true;
    check(found, "versions: pendingAutosaves lists this document");

    check(part.save(path) && part.versionNumber() == 2, "versions: second save is v2");
    check(part.versionHistory().size() == 2, "versions: two snapshots on disk");

    // Revert to v1 — contentRevision advances so linked assets go stale
    const qint64 beforeRevert = part.contentRevision();
    check(part.revertToVersion(1) && part.contentRevision() > beforeRevert,
          "versions: revertToVersion(1)", part.lastError());
    check(part.bodies().size() >= 1, "versions: reverted design still has bodies");

    DocumentVersionStore::instance()->clearAutosave(part.documentId());
}

void testGifWriter() {
    QTemporaryDir tmp;
    check(tmp.isValid(), "gif: temp dir");
    QImage a(64, 48, QImage::Format_RGB32);
    a.fill(QColor(40, 80, 160));
    QImage b(64, 48, QImage::Format_RGB32);
    b.fill(QColor(200, 60, 40));
    const QString out = QDir(tmp.path()).filePath(QStringLiteral("walkthrough.gif"));
    check(GifWriter::write(out, {a, b}, 20, 64) && QFileInfo::exists(out) && QFileInfo(out).size() > 32,
          "gif: writes animated GIF", out);
}

void testViewportBackground() {
    BRepRenderer r;
    const QColor top(80, 90, 100), bottom(20, 25, 30);
    r.setBackgroundTop(top);
    r.setBackgroundBottom(bottom);
    check(r.backgroundTop() == top && r.backgroundBottom() == bottom,
          "viewport: background colours can be set on the fly");

    ViewportController cam;
    cam.setYaw(0);
    cam.setPitch(0);
    cam.setDistance(100);
    cam.setTarget(QVector3D(0, 0, 0));
    cam.zoomAt(0.5, 200, 150, 400, 300);   // centre of view
    check(near(cam.target().x(), 0) && near(cam.target().y(), 0) && near(cam.target().z(), 0)
              && near(cam.distance(), 50),
          "zoomAt: centre keeps the target",
          QString("target=(%1,%2,%3) d=%4").arg(cam.target().x()).arg(cam.target().y()).arg(cam.target().z()).arg(cam.distance()));
    cam.setDistance(100);
    cam.setTarget(QVector3D(0, 0, 0));
    cam.zoomAt(0.5, 350, 150, 400, 300);   // right of centre → target moves along +Y (right)
    check(near(cam.distance(), 50) && cam.target().y() > 1.0 && near(cam.target().x(), 0, 1e-3)
              && near(cam.target().z(), 0, 1e-3),
          "zoomAt: zooms toward the cursor",
          QString("target=(%1,%2,%3)").arg(cam.target().x()).arg(cam.target().y()).arg(cam.target().z()));
}

void testMeshFaceGroupsRemesh() {
    // Box with a hole and a filleted edge: 8 faces
    FissionDocument part;
    box(&part, 0, 0, 40, 30, 10, "join");
    auto *hs = qobject_cast<FissionSketch *>(part.addFeature("Sketch", {{"plane", "XY"}}));
    hs->addCircle(12, 15, 5);
    part.rebuild();
    extrude(&part, hs, "10 mm", "cut");
    const auto vert = [](const gp_Pnt &p) { return near(p.X(), 40, 1e-6) && near(p.Y(), 30, 1e-6); };
    part.addFeature("Fillet", {{"edges", QVariantList{edgeAt(&part, 0, vert)}}, {"radius", "4 mm"}});
    const int pf = faceCount(&part, 0);
    const double pv = totalVolume(&part);
    part.addFeature("MeshConvert", {{"bodies", QVariantList{0}}, {"direction", "toMesh"}, {"refinement", "medium"}});
    const int tris = MeshUtils::fromShape(part.body(0)->shape()).tris.size();

    auto *fg = part.addFeature("MeshFaceGroups", {{"bodies", QVariantList{0}}, {"method", "accurate"}});
    const int groups = MeshUtils::groupCount(part.body(0)->shape());
    // Accurate matches Fusion: primitives only. Flats stay one group; a fillet may split into several
    // bands when the mesh chord is coarser than the boundary tolerance.
    check(fg->isHealthy() && groups >= pf && fg->property("summary").toString().contains("face group"),
          "face groups: accurate finds at least one group per original face",
          QString("%1 groups (want >= %2), %3").arg(groups).arg(pf).arg(status(&part)));
    const MeshUtils::Mesh grouped = MeshUtils::fromShape(part.body(0)->shape());
    check(grouped.hasGroups() && int(grouped.tris.size()) == tris && part.body(0)->isMesh()
              && part.body(0)->meshStats().value("groups").toInt() == groups && part.body(0)->meshStats().value("closed").toBool(),
          "face groups: the body keeps every triangle, one face per group",
          QString("%1 / %2 triangles, %3 groups").arg(grouped.tris.size()).arg(tris).arg(groups));

    fg->set("method", "fast");
    fg->set("angle", "20 deg");
    part.rebuild();
    const int fast = MeshUtils::groupCount(part.body(0)->shape());
    check(fg->isHealthy() && fast >= 4 && fast < pf, "face groups: fast joins faces that meet smoothly (the fillet)",
          QString("%1 groups").arg(fast));
    fg->set("method", "accurate");
    part.rebuild();
    const int groups2 = MeshUtils::groupCount(part.body(0)->shape());

    // Groups survive operations that keep the triangles, and steer the conversion
    auto *rev = part.addFeature("MeshReverse", {{"bodies", QVariantList{0}}});
    check(rev->isHealthy() && MeshUtils::groupCount(part.body(0)->shape()) == groups2, "face groups: kept by Reverse Normal");
    rev->setSuppressed(true);
    part.rebuild();
    auto *conv = part.addFeature("MeshConvert", {{"bodies", QVariantList{0}}, {"direction", "toBRep"}});
    // Groups steer the convert: at least the original B-rep faces, volume preserved. A curved group that
    // cannot stay one free-form face may become a few planes (Fusion does the same when a group is awkward).
    check(conv->isHealthy() && faceCount(&part, 0) >= pf && near(totalVolume(&part), pv, 0.05),
          "face groups: prismatic conversion keeps the solid from the groups",
          QString("%1 faces (groups were %2), vol %3 (want %4), %5")
              .arg(faceCount(&part, 0)).arg(groups2).arg(totalVolume(&part), 0, 'f', 1).arg(pv, 0, 'f', 1)
              .arg(conv->isHealthy() ? conv->property("summary").toString() : conv->error()));

    // Mesh commands wait for a pick instead of taking every body
    auto *none = part.addFeature("MeshSmooth", {{"bodies", QVariantList()}});
    check(!none->isHealthy() && none->error().contains("Select"), "mesh commands: nothing picked = nothing done", none->error());
    part.removeFeature(none);

    // Remesh: even triangles near the target length, closed and with the same volume
    FissionDocument rm;
    box(&rm, 0, 0, 20, 20, 20, "join");
    rm.addFeature("MeshConvert", {{"bodies", QVariantList{0}}, {"direction", "toMesh"}, {"refinement", "medium"}});
    auto *re = rm.addFeature("MeshRemesh", {{"bodies", QVariantList{0}}, {"edgeLength", "2 mm"}, {"iterations", "4"}});
    const MeshUtils::Mesh rmesh = MeshUtils::fromShape(rm.body(0)->shape());
    const MeshUtils::Stats rs = MeshUtils::stats(rmesh);
    double mean = 0;
    for (const auto &t : rmesh.tris)
        for (int k = 0; k < 3; ++k) mean += rmesh.nodes[t[k]].Distance(rmesh.nodes[t[(k + 1) % 3]]);
    mean /= std::max<size_t>(1, rmesh.tris.size() * 3);
    check(re->isHealthy() && rs.closed && rs.oriented && rs.triangles > 500 && mean > 1.4 && mean < 2.6
              && near(std::fabs(rs.volume), 8000, 1e-3),
          "remesh: even triangles near the target, closed, same volume",
          QString("%1 tris, mean edge %2, volume %3, %4").arg(rs.triangles).arg(mean).arg(rs.volume).arg(re->property("summary").toString()));
}

void testMeshFitting() {
    // Cylinder: tessellated, then converted back with fitted surfaces
    FissionDocument doc;
    auto *c = qobject_cast<FissionSketch *>(doc.addFeature("Sketch", {{"plane", "XY"}}));
    c->addCircle(0, 0, 10);
    doc.rebuild();
    extrude(&doc, c, "20 mm");
    auto *tess = doc.addFeature("MeshConvert", {{"direction", "toMesh"}, {"refinement", "fine"}});
    const MeshUtils::Mesh mesh = MeshUtils::fromShape(doc.body(0)->shape());
    QElapsedTimer timer;
    timer.start();
    auto *conv = doc.addFeature("MeshConvert", {{"direction", "toBRep"}, {"method", "prismatic"}});
    const qint64 ms = timer.elapsed();
    const QString summary = conv->property("summary").toString();
    check(tess->isHealthy() && conv->isHealthy() && faceCount(&doc, 0) == 3 && summary.contains("1 cylinder")
              && near(totalVolume(&doc), M_PI * 100 * 20, 2e-3),
          "mesh to solid: prismatic fits a true cylinder",
          QString("%1, %2 tris in %3 ms, %4").arg(summary).arg(mesh.tris.size()).arg(ms).arg(status(&doc)));
    conv->set("method", "faceted");
    doc.rebuild();
    check(conv->isHealthy() && faceCount(&doc, 0) > 20 && near(totalVolume(&doc), MeshUtils::volume(mesh), 1e-4),
          "mesh to solid: faceted keeps the flat triangles", QString("%1 faces").arg(faceCount(&doc, 0)));
    {
        // Big faceted solids take their mass properties from the face triangles; same answer as the exact integration
        QString err;
        const TopoDS_Shape ball = MeshUtils::toBRep(MeshUtils::fromShape(BRepPrimAPI_MakeSphere(gp_Pnt(3, -2, 5), 10).Shape(), 0.05), err);
        BRepMesh_IncrementalMesh(ball, 0.1, Standard_False, 0.5);
        GProp_GProps vp, sp;
        BRepGProp::VolumeProperties(ball, vp);
        BRepGProp::SurfaceProperties(ball, sp);
        const GeomUtils::MassProperties mp = GeomUtils::massProperties(ball);
        check(GeomUtils::faceMap(ball).Extent() > 500 && near(mp.volume, vp.Mass(), 1e-6) && near(mp.area, sp.Mass(), 1e-6)
                  && mp.centroid.Distance(vp.CentreOfMass()) < 1e-6,
              "mass properties: many-faced solids summed from their triangles match the exact values",
              QString("%1 faces, volume %2 / %3, area %4 / %5").arg(GeomUtils::faceMap(ball).Extent()).arg(mp.volume)
                  .arg(vp.Mass()).arg(mp.area).arg(sp.Mass()));
    }

    // A cone standing on a plate (the converted-mesh case from the bug report)
    FissionDocument cone;
    box(&cone, -20, -20, 40, 40, 5, "join");
    auto *prof = qobject_cast<FissionSketch *>(cone.addFeature("Sketch", {{"plane", "XZ"}}));
    prof->addPolyline({QVariantList{0, 5}, QVariantList{10, 5}, QVariantList{0, 25}}, true);
    cone.rebuild();
    cone.addFeature("Revolve", {{"sketch", prof->id()}, {"axis", "Z"}, {"angle", "360 deg"}, {"operation", "join"}});
    const double exact = totalVolume(&cone);
    cone.addFeature("MeshConvert", {{"direction", "toMesh"}, {"refinement", "medium"}});
    timer.restart();
    auto *cc = cone.addFeature("MeshConvert", {{"direction", "toBRep"}});
    const QString cs = cc->property("summary").toString();
    check(cc->isHealthy() && cs.contains("1 cone") && faceCount(&cone, 0) == 7 && near(totalVolume(&cone), exact, 5e-3),
          "mesh to solid: cone on a plate becomes 6 planes and a cone",
          QString("%1 in %2 ms, %3").arg(cs).arg(timer.elapsed()).arg(status(&cone)));

    // A dome on a plate
    FissionDocument dome;
    box(&dome, -20, -20, 40, 40, 5, "join");
    auto *arc = qobject_cast<FissionSketch *>(dome.addFeature("Sketch", {{"plane", "XZ"}}));
    arc->addArc(0, 5, 8, 0, 90);
    arc->addLine(0, 5, 8, 5);
    arc->addLine(0, 5, 0, 13);
    dome.rebuild();
    dome.addFeature("Revolve", {{"sketch", arc->id()}, {"axis", "Z"}, {"angle", "360 deg"}, {"operation", "join"}});
    const double dv = totalVolume(&dome);
    dome.addFeature("MeshConvert", {{"direction", "toMesh"}, {"refinement", "medium"}});
    auto *dc = dome.addFeature("MeshConvert", {{"direction", "toBRep"}});
    const QString ds = dc->property("summary").toString();
    check(dc->isHealthy() && ds.contains("1 sphere") && faceCount(&dome, 0) == 7 && near(totalVolume(&dome), dv, 5e-3),
          "mesh to solid: a dome becomes a sphere face", QString("%1, %2").arg(ds).arg(status(&dome)));

    // Box with a round hole and a filleted edge
    FissionDocument part;
    box(&part, 0, 0, 40, 30, 10, "join");
    auto *hs = qobject_cast<FissionSketch *>(part.addFeature("Sketch", {{"plane", "XY"}}));
    hs->addCircle(12, 15, 5);
    part.rebuild();
    extrude(&part, hs, "10 mm", "cut");
    const auto vert = [](const gp_Pnt &p) { return near(p.X(), 40, 1e-6) && near(p.Y(), 30, 1e-6); };
    part.addFeature("Fillet", {{"edges", QVariantList{edgeAt(&part, 0, vert)}}, {"radius", "4 mm"}});
    const double pv = totalVolume(&part);
    const int pf = faceCount(&part, 0);
    part.addFeature("MeshConvert", {{"direction", "toMesh"}, {"refinement", "medium"}});
    auto *pc = part.addFeature("MeshConvert", {{"direction", "toBRep"}});
    const QString ps = pc->property("summary").toString();
    check(pc->isHealthy() && faceCount(&part, 0) == pf && ps.contains("2 cylinders") && near(totalVolume(&part), pv, 5e-3),
          "mesh to solid: hole and fillet come back as cylinders",
          QString("%1 (was %2 faces), %3").arg(ps).arg(pf).arg(status(&part)));

    // A free-form wall: a spline profile extruded straight up (the case from the user's picture)
    FissionDocument wave;
    auto *ws = qobject_cast<FissionSketch *>(wave.addFeature("Sketch", {{"plane", "XY"}}));
    ws->addSpline({QVariantList{0, 0}, QVariantList{10, 6}, QVariantList{20, -2}, QVariantList{30, 5}, QVariantList{40, 0}});
    ws->addLine(40, 0, 40, -20);
    ws->addLine(40, -20, 0, -20);
    ws->addLine(0, -20, 0, 0);
    wave.rebuild();
    extrude(&wave, ws, "15 mm");
    const int wf = faceCount(&wave, 0);
    const double wv = totalVolume(&wave);
    wave.addFeature("MeshConvert", {{"bodies", QVariantList{0}}, {"direction", "toMesh"}, {"refinement", "fine"}});
    timer.restart();
    auto *wc = wave.addFeature("MeshConvert", {{"bodies", QVariantList{0}}, {"direction", "toBRep"}});
    const QString wsum = wc->property("summary").toString();
    check(wc->isHealthy() && wsum.contains("1 extruded free-form face") && faceCount(&wave, 0) == wf
              && near(totalVolume(&wave), wv, 2e-3),
          "mesh to solid: an extruded spline wall comes back as one smooth face",
          QString("%1 (was %2 faces) in %3 ms, %4").arg(wsum).arg(wf).arg(timer.elapsed()).arg(status(&wave)));

    // A block with a wavy top (an organic surface no plane / round surface fits): the top is one B-spline face
    {
        MeshUtils::Mesh wavy;
        const int N = 25;
        const double L = 40;
        auto top = [](double x, double y) { return 10 + 3 * std::sin(x / 8) * std::cos(y / 10); };
        for (int j = 0; j < N; ++j)
            for (int i = 0; i < N; ++i) {
                const double x = L * i / (N - 1), y = L * j / (N - 1);
                wavy.nodes.emplace_back(x, y, top(x, y));
            }
        for (int j = 0; j < N; ++j)
            for (int i = 0; i < N; ++i) wavy.nodes.emplace_back(L * i / (N - 1), L * j / (N - 1), 0);
        // Each triangle is turned to face `out`
        auto tri = [&](int a, int b, int c, const gp_XYZ &out) {
            const gp_XYZ pa = wavy.nodes[a].XYZ(), pb = wavy.nodes[b].XYZ(), pc = wavy.nodes[c].XYZ();
            if ((pb - pa).Crossed(pc - pa).Dot(out) < 0) std::swap(b, c);
            wavy.tris.push_back({a, b, c});
        };
        auto quad = [&](int a, int b, int c, int d, const gp_XYZ &out) { tri(a, b, c, out); tri(a, c, d, out); };
        const int B = N * N;
        for (int j = 0; j + 1 < N; ++j)
            for (int i = 0; i + 1 < N; ++i) {
                const int k = j * N + i;
                quad(k, k + 1, k + N + 1, k + N, gp_XYZ(0, 0, 1));
                quad(B + k, B + k + 1, B + k + N + 1, B + k + N, gp_XYZ(0, 0, -1));
            }
        for (int i = 0; i + 1 < N; ++i) {
            quad(i, i + 1, B + i + 1, B + i, gp_XYZ(0, -1, 0));
            quad((N - 1) * N + i, (N - 1) * N + i + 1, B + (N - 1) * N + i + 1, B + (N - 1) * N + i, gp_XYZ(0, 1, 0));
            quad(i * N, (i + 1) * N, B + (i + 1) * N, B + i * N, gp_XYZ(-1, 0, 0));
            quad(i * N + N - 1, (i + 1) * N + N - 1, B + (i + 1) * N + N - 1, B + i * N + N - 1, gp_XYZ(1, 0, 0));
        }
        QString why, sum;
        timer.restart();
        // Progress for the status bar: climbs from 0 to 1 with a name for every stage
        std::vector<double> fractions;
        QStringList stages;
        const TopoDS_Shape s = MeshUtils::toFittedBRep(wavy, true, why, &sum, [&](double f, const QString &what) {
            fractions.push_back(f);
            if (stages.isEmpty() || stages.last() != what) stages << what;
        });
        bool climbs = fractions.size() >= 3;
        for (size_t i = 1; i < fractions.size(); ++i) climbs = climbs && fractions[i] >= fractions[i - 1] - 1e-9;
        check(climbs && near(fractions.empty() ? 0 : fractions.back(), 1, 1e-9) && stages.contains("Fitting surfaces")
                  && stages.contains("Building faces") && stages.last() == "Done",
              "mesh to solid: progress climbs 0..1 through named stages",
              QString("%1 reports, stages %2").arg(fractions.size()).arg(stages.join(" > ")));
        GProp_GProps gp;
        if (!s.IsNull()) BRepGProp::VolumeProperties(s, gp);
        int nf = 0;
        for (TopExp_Explorer e(s, TopAbs_FACE); e.More(); e.Next()) ++nf;
        check(!s.IsNull() && sum.contains("free-form face") && nf <= 8 && BRepCheck_Analyzer(s).IsValid()
                  && near(gp.Mass(), MeshUtils::volume(wavy), 0.01),
              "mesh to solid: a wavy organic top becomes a B-spline face, not facets",
              QString("%1, %2 faces in %3 ms %4").arg(sum).arg(nf).arg(timer.elapsed()).arg(why));
        Bnd_Box meshBox;
        for (const gp_Pnt &p : wavy.nodes) meshBox.Add(p);
        meshBox.Enlarge(1.0);
        int stray = 0;
        for (TopExp_Explorer e(s, TopAbs_EDGE); e.More(); e.Next()) {
            Bnd_Box eb;
            BndLib_Add3dCurve::Add(BRepAdaptor_Curve(TopoDS::Edge(e.Current())), 0.0, eb);
            double x0, y0, z0, x1, y1, z1, a0, b0, c0, a1, b1, c1;
            eb.Get(x0, y0, z0, x1, y1, z1);
            meshBox.Get(a0, b0, c0, a1, b1, c1);
            if (x0 < a0 || y0 < b0 || z0 < c0 || x1 > a1 || y1 > b1 || z1 > c1) ++stray;
        }
        check(!s.IsNull() && stray == 0, "mesh to solid: every free-form rim curve stays on the part (no stray loops)",
              QString("%1 edges outside").arg(stray));
    }

    // An open mesh still converts (one face per triangle)
    MeshUtils::Mesh open = mesh;
    open.tris.pop_back();
    QString err;
    check(MeshUtils::toFittedBRep(open, true, err).IsNull() && !err.isEmpty(), "mesh to solid: fitting needs a closed mesh", err);
}

void testImportCAD() {
    FissionDocument src;
    box(&src, 0, 0, 30, 20, 10);
    auto *c = qobject_cast<FissionSketch *>(src.addFeature("Sketch", {{"plane", "XY"}}));
    c->addCircle(50, 10, 5);
    src.rebuild();
    extrude(&src, c, "10 mm", "newBody");
    QTemporaryDir dir;
    const QString step = dir.filePath("parts.step"), iges = dir.filePath("parts.iges");
    src.exportBodies(step, {0, 1});
    src.exportBodies(iges, {0, 1});
    FissionDocument doc;
    const QString id = doc.importCAD(step);
    check(!id.isEmpty() && doc.bodyCount() == 2 && near(totalVolume(&doc), totalVolume(&src), 1e-4),
          "import: STEP solids become bodies", doc.lastError() + " " + status(&doc));
    const QString path = dir.filePath("imported.f360");
    doc.save(path);
    FissionDocument loaded;
    loaded.load(path);
    check(loaded.bodyCount() == 2 && near(totalVolume(&loaded), totalVolume(&src), 1e-4),
          "import: the geometry is stored in the design", status(&loaded));
    FissionDocument ig;
    const bool igesOk = !ig.importCAD(iges).isEmpty();
    check(igesOk && ig.bodyCount() >= 1 && GeomUtils::faceMap(ig.body(0)->shape()).Extent() >= 6,
          "import: IGES", ig.lastError() + " " + status(&ig));
    check(doc.importCAD(dir.filePath("missing.step")).isEmpty() && !doc.lastError().isEmpty(), "import: a missing file is an error");
    auto *ex = doc.addFeature("Fillet", {{"edges", QVariantList{edgeAt(&doc, 0, [](const gp_Pnt &p) {
                                             return near(p.X(), 30, 1e-6) && near(p.Y(), 20, 1e-6); })}}, {"radius", "2 mm"}});
    check(ex->isHealthy(), "import: imported bodies work with modeling tools", status(&doc));

    // An assembly with named, coloured parts: bodies take the part names (with the assembly path) and colours
    {
        Handle(TDocStd_Document) xdoc;
        XCAFApp_Application::GetApplication()->NewDocument("MDTV-XCAF", xdoc);
        const Handle(XCAFDoc_ShapeTool) st = XCAFDoc_DocumentTool::ShapeTool(xdoc->Main());
        const Handle(XCAFDoc_ColorTool) ct = XCAFDoc_DocumentTool::ColorTool(xdoc->Main());
        const TDF_Label asmLabel = st->NewShape();
        TDataStd_Name::Set(asmLabel, "Fixture");
        const TDF_Label base = st->AddShape(BRepPrimAPI_MakeBox(40, 20, 5).Shape(), Standard_False);
        TDataStd_Name::Set(base, "Base");
        ct->SetColor(base, Quantity_Color(1, 0, 0, Quantity_TOC_sRGB), XCAFDoc_ColorSurf);
        const TDF_Label pin = st->AddShape(BRepPrimAPI_MakeBox(4, 4, 10).Shape(), Standard_False);
        TDataStd_Name::Set(pin, "Pin");
        ct->SetColor(pin, Quantity_Color(0, 0, 1, Quantity_TOC_sRGB), XCAFDoc_ColorSurf);
        gp_Trsf up;
        up.SetTranslation(gp_Vec(10, 8, 5));
        st->AddComponent(asmLabel, base, TopLoc_Location());
        st->AddComponent(asmLabel, pin, TopLoc_Location(up));
        st->UpdateAssemblies();
        const QString asmFile = dir.filePath("fixture.step");
        STEPCAFControl_Writer writer;
        writer.SetColorMode(true);
        writer.SetNameMode(true);
        const bool written = writer.Transfer(xdoc, STEPControl_AsIs) && writer.Write(asmFile.toLocal8Bit().constData()) == IFSelect_RetDone;
        XCAFApp_Application::GetApplication()->Close(xdoc);
        FissionDocument as;
        const bool imported = written && !as.importCAD(asmFile).isEmpty();
        QStringList names, colors;
        for (int i = 0; i < as.bodyCount(); ++i) {
            names << as.body(i)->name();
            colors << as.body(i)->color().name();
        }
        double pz0 = 0, pz1 = 0;
        if (as.bodyCount() == 2) {
            double a, b;
            GeomUtils::boundingBox(as.body(1)->shape()).Get(a, b, pz0, a, b, pz1);
        }
        check(imported && names == QStringList{"Fixture/Base", "Fixture/Pin"} && colors == QStringList{"#ff0000", "#0000ff"}
                  && near(pz0, 5, 1e-4) && near(pz1, 15, 1e-4),
              "import: STEP assembly parts keep their names, colours and placement",
              names.join(", ") + " | " + colors.join(", ") + " | " + as.lastError());
    }
}

void testLinkedDesign() {
    QTemporaryDir dir;
    const QString srcPath = dir.filePath("nut.f360"), dstPath = dir.filePath("assembly.f360");
    FissionDocument src;
    box(&src, 0, 0, 10, 10, 5);
    src.save(srcPath);

    FissionDocument doc;
    box(&doc, 50, 0, 20, 20, 20);
    doc.save(dstPath);
    const QString id = doc.insertLinkedDesign(srcPath);
    QVariantList links = doc.linkedDesigns();
    check(!id.isEmpty() && doc.bodyCount() == 2 && links.size() == 1 && !links[0].toMap().value("outOfDate").toBool()
              && doc.body(1)->sourceFeature() == id,
          "linked design: a saved design inserts as linked bodies", doc.lastError() + " " + status(&doc));
    check(doc.insertLinkedDesign(dstPath).isEmpty(), "linked design: a design cannot be inserted into itself");

    auto *c = qobject_cast<FissionSketch *>(src.addFeature("Sketch", {{"plane", "XY"}}));
    c->addCircle(30, 30, 4);
    src.rebuild();
    extrude(&src, c, "6 mm", "newBody");
    src.save(srcPath);
    links = doc.linkedDesigns();
    check(links.size() == 1 && links[0].toMap().value("outOfDate").toBool(),
          "linked design: saving the original with changes marks the insert out of date");

    const bool updated = doc.updateLinkedDesign(id);
    links = doc.linkedDesigns();
    check(updated && doc.bodyCount() == 3 && near(totalVolume(&doc), 8000 + totalVolume(&src), 1e-3)
              && links.size() == 1 && !links[0].toMap().value("outOfDate").toBool(),
          "linked design: update brings in the new version", doc.lastError() + " " + status(&doc));

    doc.unlinkDesign(id);
    check(doc.linkedDesigns().isEmpty() && doc.bodyCount() == 3,
          "linked design: unlink keeps the bodies and stops tracking the original");
    doc.save(dstPath);
    FissionDocument loaded;
    loaded.load(dstPath);
    check(loaded.bodyCount() == 3 && loaded.linkedDesigns().isEmpty() && near(totalVolume(&loaded), totalVolume(&doc), 1e-3),
          "linked design: an unlinked insert reloads as plain geometry", status(&loaded));
}

void testSheetMetal() {
    FissionDocument doc;
    auto *s = rectSketch(&doc, "XY", 0, 0, 50, 30);
    auto *base = doc.addFeature("BaseFlange", {{"sketch", s->id()}});
    check(base->isHealthy() && doc.parameter("SheetThickness") && doc.parameter("BendRadius") && doc.parameter("KFactor")
              && near(totalVolume(&doc), 1500),
          "sheet metal: flange from a profile with the default rule (1 mm)", status(&doc));
    const auto backTop = [](const gp_Pnt &p) { return near(p.Y(), 30, 1e-6) && near(p.Z(), 1, 1e-6); };
    auto *ef = doc.addFeature("EdgeFlange", {{"edge", edgeAt(&doc, 0, backTop)}, {"height", "10 mm"}});
    const double bend = M_PI / 4 * (4 - 1) * 50;
    checkVolume(&doc, 1500 + bend + 500, "sheet metal: edge flange adds a 90 deg bend and a flange");
    double x0, y0, z0, x1, y1, z1;
    GeomUtils::boundingBox(doc.body(0)->shape()).Get(x0, y0, z0, x1, y1, z1);
    check(ef->isHealthy() && near(z1, 1 + 1 + 10, 1e-3) && near(y1, 32, 1e-3), "sheet metal: the flange stands up",
          QString("ymax %1 zmax %2").arg(y1).arg(z1));
    auto *flat = doc.addFeature("FlatPattern", {});
    const double allowance = M_PI / 2 * (1 + 0.44 * 1);
    check(flat->isHealthy() && doc.bodyCount() == 2 && near(doc.body(1)->volume(), 1500 + (allowance + 10) * 50, 1e-3),
          "sheet metal: flat pattern unrolls the bend with the K-factor",
          flat->property("summary").toString() + " " + status(&doc));
    GeomUtils::boundingBox(doc.body(1)->shape()).Get(x0, y0, z0, x1, y1, z1);
    check(near(z1 - z0, 1, 1e-3) && near(y1 - y0, 30 + allowance + 10, 1e-3), "sheet metal: the flat pattern is flat",
          QString("%1 x %2 x %3").arg(x1 - x0).arg(y1 - y0).arg(z1 - z0));
    doc.parameter("SheetThickness")->setExpression("2 mm");
    doc.rebuild();
    check(base->isHealthy() && ef->isHealthy() && near(doc.body(0)->volume(), 3000 + M_PI / 4 * (9 - 1) * 50 + 1000, 1e-3),
          "sheet metal: changing the rule thickness updates the part", status(&doc));
    ef->set("flip", true);
    doc.rebuild();
    GeomUtils::boundingBox(doc.body(0)->shape()).Get(x0, y0, z0, x1, y1, z1);
    check(ef->isHealthy() && z0 < -10, "sheet metal: flipped flange bends down", status(&doc));
}

void testSheetMetalPhase7() {
    const double k = 0.44, allow90 = M_PI / 2 * (1 + k);
    auto flatExtent = [](const TopoDS_Shape &s) {
        double x0, y0, z0, x1, y1, z1;
        GeomUtils::boundingBox(s).Get(x0, y0, z0, x1, y1, z1);
        return std::min({x1 - x0, y1 - y0, z1 - z0});
    };
    {
        // A flange on a flange unfolds through both bends
        FissionDocument doc;
        auto *s = rectSketch(&doc, "XY", 0, 0, 50, 30);
        doc.addFeature("BaseFlange", {{"sketch", s->id()}});
        doc.addFeature("EdgeFlange", {{"edge", edgeAt(&doc, 0, [](const gp_Pnt &p) { return near(p.Y(), 30, 1e-6) && near(p.Z(), 1, 1e-6); })},
                                      {"height", "10 mm"}});
        auto *ef2 = doc.addFeature("EdgeFlange", {{"edge", edgeAt(&doc, 0, [](const gp_Pnt &p) { return near(p.Y(), 32, 1e-6) && near(p.Z(), 12, 1e-6); })},
                                                  {"height", "8 mm"}});
        auto *flat = doc.addFeature("FlatPattern", {});
        check(ef2->isHealthy() && flat->isHealthy() && doc.bodyCount() == 2
                  && near(doc.body(1)->volume(), 1500 + (2 * allow90 + 18) * 50, 1e-3) && near(flatExtent(doc.body(1)->shape()), 1, 1e-3),
              "sheet metal: a flange on a flange unfolds flat", flat->property("summary").toString() + " " + status(&doc));
        const QString dxf = QDir::temp().filePath("f360_flat_test.dxf");
        const QString written = doc.exportFlatPatternDxf("", dxf);
        QFile f(dxf);
        const QString text = f.open(QIODevice::ReadOnly) ? QString::fromUtf8(f.readAll()) : QString();
        check(!written.isEmpty() && text.contains("BEND") && text.count(QRegularExpression("^LINE\\r?$", QRegularExpression::MultilineOption)) >= 6,
              "sheet metal: flat pattern exports as DXF with bend lines", doc.lastError());
    }
    {
        // Hem (flat, 0.2 mm gap) and a tab
        FissionDocument doc;
        auto *s = rectSketch(&doc, "XY", 0, 0, 50, 30);
        doc.addFeature("BaseFlange", {{"sketch", s->id()}});
        auto *hem = doc.addFeature("Hem", {{"edge", edgeAt(&doc, 0, [](const gp_Pnt &p) { return near(p.Y(), 0, 1e-6) && near(p.Z(), 1, 1e-6); })},
                                           {"hemType", "flat"}, {"gap", "0.2 mm"}, {"length", "6 mm"}});
        checkVolume(&doc, 1500 + M_PI / 2 * (1.1 * 1.1 - 0.1 * 0.1) * 50 + 300, "sheet metal: flat hem folds the edge back");
        double x0, y0, z0, x1, y1, z1;
        GeomUtils::boundingBox(doc.body(0)->shape()).Get(x0, y0, z0, x1, y1, z1);
        check(hem->isHealthy() && near(z1, 2.2, 1e-3) && near(y0, -1.1, 1e-3), "sheet metal: the hem lies on the sheet with the gap",
              QString("ymin %1 zmax %2 %3").arg(y0).arg(z1).arg(status(&doc)));
        auto *ts = rectSketch(&doc, "XY", 10, 25, 10, 15);
        auto *tab = doc.addFeature("Tab", {{"sketch", ts->id()}});
        const double folded = totalVolume(&doc);
        check(tab->isHealthy() && near(folded, 1500 + M_PI / 2 * (1.1 * 1.1 - 0.1 * 0.1) * 50 + 300 + 100),
              "sheet metal: a tab adds to the sheet in its plane", status(&doc));
        auto *flat = doc.addFeature("FlatPattern", {});
        check(flat->isHealthy() && near(doc.body(1)->volume(), 1500 + (M_PI * (0.1 + k) + 6) * 50 + 100, 1e-3),
              "sheet metal: hem and tab in the flat pattern", flat->property("summary").toString() + " " + status(&doc));
    }
    {
        // Bend along a sketch line keeps the flat length
        FissionDocument doc;
        auto *s = rectSketch(&doc, "XY", 0, 0, 60, 30);
        doc.addFeature("BaseFlange", {{"sketch", s->id()}});
        auto *ls = qobject_cast<FissionSketch *>(doc.addFeature("Sketch", {{"plane", "XY"}}));
        const int line = ls->addLine(30, -5, 30, 35);
        doc.rebuild();
        auto *bend = doc.addFeature("Bend", {{"line", QString("sketchline:%1:%2").arg(ls->id()).arg(line)}});
        checkVolume(&doc, 30 * 30 + M_PI / 4 * 3 * 30 + (30 - allow90) * 30, "sheet metal: bend along a sketch line");
        double x0, y0, z0, x1, y1, z1;
        GeomUtils::boundingBox(doc.body(0)->shape()).Get(x0, y0, z0, x1, y1, z1);
        check(bend->isHealthy() && std::max(std::fabs(z0), std::fabs(z1)) > 20, "sheet metal: the bent side turns out of the plane",
              QString("z %1..%2 %3").arg(z0).arg(z1).arg(status(&doc)));
        auto *flat = doc.addFeature("FlatPattern", {});
        check(flat->isHealthy() && near(doc.body(1)->volume(), 1800, 1e-3) && near(flatExtent(doc.body(1)->shape()), 1, 1e-3),
              "sheet metal: a bent sheet unfolds to its original size", status(&doc));
    }
    {
        // Contour flange: an L of lines, one corner bend
        FissionDocument doc;
        doc.ensureSheetMetalRule();
        auto *cs = qobject_cast<FissionSketch *>(doc.addFeature("Sketch", {{"plane", "XZ"}}));
        cs->addLine(0, 0, 30, 0);
        cs->addLine(30, 0, 30, 20);
        doc.rebuild();
        auto *cf = doc.addFeature("ContourFlange", {{"sketch", cs->id()}, {"distance", "20 mm"}});
        const double folded = totalVolume(&doc);
        auto *flat = doc.addFeature("FlatPattern", {});
        const double flatVol = doc.bodyCount() == 2 ? doc.body(1)->volume() : 0;
        check(cf->isHealthy() && flat->isHealthy() && near(flatVol - folded, M_PI / 2 * 20 * (k - 0.5), 1e-3)
                  && near(flatExtent(doc.body(1)->shape()), 1, 1e-3),
              "sheet metal: contour flange bends at the corner and unfolds",
              QString("flat-folded %1 %2").arg(flatVol - folded).arg(status(&doc)));
    }
    {
        // Unfold, cut a hole across the flat, refold: the hole follows the flange
        FissionDocument doc;
        auto *s = rectSketch(&doc, "XY", 0, 0, 50, 30);
        doc.addFeature("BaseFlange", {{"sketch", s->id()}});
        doc.addFeature("EdgeFlange", {{"edge", edgeAt(&doc, 0, [](const gp_Pnt &p) { return near(p.Y(), 30, 1e-6) && near(p.Z(), 1, 1e-6); })},
                                      {"height", "10 mm"}});
        const double folded = totalVolume(&doc);
        auto *unfold = doc.addFeature("Unfold", {});
        check(unfold->isHealthy() && doc.bodyCount() == 1 && near(totalVolume(&doc), 1500 + (allow90 + 10) * 50, 1e-3)
                  && near(flatExtent(doc.body(0)->shape()), 1, 1e-3),
              "sheet metal: unfold lays the sheet flat in place", status(&doc));
        auto *hole = qobject_cast<FissionSketch *>(doc.addFeature("Sketch", {{"plane", "XY"}}));
        hole->addCircle(25, 30 + allow90 + 5, 2);
        doc.rebuild();
        extrude(&doc, hole, 10, "cut", {{"extent", "throughAll"}});
        auto *refold = doc.addFeature("Refold", {});
        double x0, y0, z0, x1, y1, z1;
        GeomUtils::boundingBox(doc.body(0)->shape()).Get(x0, y0, z0, x1, y1, z1);
        check(refold->isHealthy() && near(totalVolume(&doc), folded - M_PI * 4, 1e-3) && near(z1, 12, 1e-3),
              "sheet metal: refold carries a cut made flat onto the flange",
              refold->property("summary").toString() + " " + status(&doc));
    }
}

void testScripts() {
    QJSEngine engine;
    ScriptHost host(&engine);
    QVariantMap r = host.run("print('a', 1 + 1)\nconst x = 3\nx * 2", nullptr);
    check(r.value("ok").toBool() && r.value("output").toStringList() == QStringList{"a 2"},
          "scripts: run and print", QJsonDocument::fromVariant(r).toJson(QJsonDocument::Compact));
    r = host.run("const a = 1\nundefinedFunction()", nullptr);
    check(!r.value("ok").toBool() && r.value("line").toInt() == 2, "scripts: errors report the line",
          QJsonDocument::fromVariant(r).toJson(QJsonDocument::Compact));
    r = host.run("while (true) {}", nullptr, 300);
    check(!r.value("ok").toBool() && r.value("error").toString().startsWith("Stopped"), "scripts: runaway scripts are stopped",
          r.value("error").toString());
    r = host.run("return fission.version", nullptr);
    check(r.value("ok").toBool() && r.value("result").toString() == FISSION_VERSION, "scripts: access to the application",
          QJsonDocument::fromVariant(r).toJson(QJsonDocument::Compact));
    const QString name = "selftest-temp.js";
    check(host.save(name, "print('saved')") && host.scripts().contains(name) && host.load(name) == "print('saved')"
              && host.remove(name) && !host.scripts().contains(name),
          "scripts: save, list, load and delete in the scripts folder", host.folder());

    r = host.run("addCommand('selftest.js', 'JS Command', 'tip', () => print('js command ran'))", nullptr);
    const QVariantMap rc = host.runCommand("selftest.js");
    check(r.value("ok").toBool() && host.commands().size() == 1 && rc.value("ok").toBool()
              && rc.value("output").toStringList() == QStringList{"js command ran"},
          "add-ins: a script adds a command and the command runs", QJsonDocument::fromVariant(rc).toJson(QJsonDocument::Compact));

    QString why;
    if (!PythonHost::available(&why)) {
        check(true, "python: not installed here, Python checks skipped", why);
        return;
    }
    auto json = [](const QVariantMap &m) { return QString::fromUtf8(QJsonDocument::fromVariant(m).toJson(QJsonDocument::Compact)); };
    r = host.run("x = 3\nprint('a', 1 + 1)\nprint(x * 2)", nullptr, 30000, "py");
    check(r.value("ok").toBool() && r.value("output").toStringList() == QStringList{"a 2", "6"}, "python: run and print", json(r));
    r = host.run("a = 1\nundefined_function()", nullptr, 30000, "py");
    check(!r.value("ok").toBool() && r.value("line").toInt() == 2 && r.value("error").toString().contains("NameError"),
          "python: errors report the line", json(r));
    r = host.run("while True:\n    pass", nullptr, 300, "py");
    check(!r.value("ok").toBool() && r.value("error").toString().startsWith("Stopped"), "python: runaway scripts are stopped", json(r));
    r = host.run("print(fission.version)\nimport adsk.core\nprint(adsk.core.Application.get().version)", nullptr, 30000, "py");
    check(r.value("ok").toBool() && r.value("output").toStringList() == QStringList{FISSION_VERSION, FISSION_VERSION},
          "python: the application and the adsk-style entry point", json(r));
    r = host.run("d = fission.new_document()\ns = d.add_feature('Sketch', {'plane': 'XY'})\ns.add_rectangle(0, 0, 10, 20)\n"
                 "e = d.add_feature('Extrude', {'sketch': s.id, 'distance': '5 mm'})\n"
                 "print(d.body_count(), round(d.body_properties(0)['volume'], 3), e.healthy)",
                 nullptr, 30000, "py");
    check(r.value("ok").toBool() && r.value("output").toStringList() == QStringList{"1 1000.0 True"},
          "python: build a part through the document API", json(r));
    r = host.run("add_command('selftest.py', 'Py Command', 'tip', lambda: print('py command ran'))", nullptr, 30000, "py");
    const QVariantMap pc = host.runCommand("selftest.py");
    check(r.value("ok").toBool() && host.commands().size() == 2 && pc.value("ok").toBool()
              && pc.value("output").toStringList() == QStringList{"py command ran"},
          "add-ins: Python add-ins add commands too", json(pc));
}

QJsonValue withoutKey(const QJsonValue &v, const QString &key) {
    if (v.isObject()) {
        QJsonObject o = v.toObject();
        o.remove(key);
        for (auto it = o.begin(); it != o.end(); ++it) it.value() = withoutKey(it.value(), key);
        return o;
    }
    if (v.isArray()) {
        QJsonArray a = v.toArray();
        for (int i = 0; i < a.size(); ++i) a[i] = withoutKey(a[i], key);
        return a;
    }
    return v;
}

void testMoveTypes() {
    auto bounds = [](FissionDocument &doc, int i) {
        double b[6];
        GeomUtils::boundingBox(doc.body(i)->shape()).Get(b[0], b[1], b[2], b[3], b[4], b[5]);
        return std::vector<double>(b, b + 6);
    };
    FissionDocument doc;
    box(&doc, 0, 0, 20, 10, 10);
    auto *mv = doc.addFeature("Move", {{"bodies", QVariantList{0}}, {"moveType", "free"},
                                       {"pivot", QVariantList{"10 mm", "5 mm", "5 mm"}},
                                       {"rotation", QVariantList{"0 deg", "0 deg", "90 deg"}},
                                       {"translate", QVariantList{"0 mm", "0 mm", "100 mm"}}});
    auto b = bounds(doc, 0);
    check(mv->isHealthy() && near(b[0], 5, 1e-4) && near(b[3], 15, 1e-4) && near(b[1], -5, 1e-4) && near(b[4], 15, 1e-4)
              && near(b[2], 100, 1e-4),
          "move: free move turns about the pivot, then translates", status(&doc));
    mv->set("rotation", QVariantList{"90 deg", "0 deg", "90 deg"});
    doc.rebuild();
    b = bounds(doc, 0);
    check(near(b[3] - b[0], 10, 1e-4) && near(b[4] - b[1], 20, 1e-4) && near(b[5] - b[2], 10, 1e-4),
          "move: rotations about X then Z", status(&doc));

    mv->set("moveType", "pointToPoint");
    mv->set("fromPoint", QVariantList{"20 mm", "10 mm", "10 mm"});
    mv->set("toPoint", QVariantList{"0 mm", "0 mm", "0 mm"});
    doc.rebuild();
    b = bounds(doc, 0);
    check(near(b[0], -20, 1e-4) && near(b[4], 0, 1e-4) && near(b[5], 0, 1e-4), "move: point to point", status(&doc));

    mv->set("moveType", "pointToPosition");
    mv->set("fromPoint", QVariantList{"0 mm", "0 mm", "0 mm"});
    mv->set("translate", QVariantList{"5 mm", "6 mm", "7 mm"});
    doc.rebuild();
    b = bounds(doc, 0);
    check(near(b[0], 5, 1e-4) && near(b[1], 6, 1e-4) && near(b[2], 7, 1e-4), "move: point to position", status(&doc));

    mv->set("moveType", "translate");
    mv->set("copy", true);
    doc.rebuild();
    b = bounds(doc, 1);
    check(doc.bodyCount() == 2 && near(b[0], 5, 1e-4) && near(bounds(doc, 0)[0], 0, 1e-4),
          "move: translate with Create Copy keeps the original", status(&doc));

    // Moves saved before the move types existed: rotate about the axis, then translate
    mv->set("copy", false);
    mv->set("moveType", "rotate");
    mv->set("axis", "Z");
    mv->set("angle", "90 deg");
    mv->set("translate", QVariantList{"100 mm", "0 mm", "0 mm"});
    QTemporaryDir dir;
    const QString path = dir.filePath("legacy.f360");
    doc.save(path);
    QFile file(path);
    file.open(QIODevice::ReadOnly);
    const QJsonValue stripped = withoutKey(QJsonDocument::fromJson(file.readAll()).object(), "moveType");
    file.close();
    file.open(QIODevice::WriteOnly | QIODevice::Truncate);
    file.write(QJsonDocument(stripped.toObject()).toJson());
    file.close();
    FissionDocument old;
    const bool loaded = old.load(path);
    b = loaded && old.bodyCount() ? bounds(old, 0) : std::vector<double>(6, 0.0);
    check(loaded && near(b[0], 90, 1e-4) && near(b[3], 100, 1e-4) && near(b[4], 20, 1e-4),
          "move: documents without a move type keep rotate-then-translate", status(&old));
}

void testSketchMove() {
    FissionDocument doc;
    auto *s = qobject_cast<FissionSketch *>(doc.addFeature("Sketch", {{"plane", "XY"}}));
    const int first = s->addRectangleLines(0, 0, 20, 10);
    doc.rebuild();
    const int conBefore = s->get("constraints").toList().size();
    QVariantMap r = s->moveEntities({first + 2}, 0, 5, 0, 0, 0, false);
    const QVariantMap top = s->entity(first + 2), bottom = s->entity(first), right = s->entity(first + 1);
    check(r.value("ok").toBool() && near(num(top, "y1"), 15) && near(num(top, "y2"), 15) && near(num(bottom, "y1"), 0)
              && near(num(right, "y2"), 10),
          "sketch move: one line of a rectangle moves on its own",
          QJsonDocument::fromVariant(QVariantList{r, top, right}).toJson(QJsonDocument::Compact));
    check(s->get("constraints").toList().size() == conBefore - 2, "sketch move: its corner links are dropped, the rest kept",
          QString("%1 -> %2").arg(conBefore).arg(s->get("constraints").toList().size()));

    const int c = s->addCircle(50, 0, 5);
    const int n = s->get("entities").toList().size();
    r = s->moveEntities({c}, 10, 0, 0, 0, 0, true);
    const QVariantMap copy = s->entity(n);
    check(r.value("ok").toBool() && s->get("entities").toList().size() == n + 1 && near(num(copy, "cx"), 60)
              && near(num(s->entity(c), "cx"), 50),
          "sketch move: Create Copy adds moved copies",
          QJsonDocument::fromVariant(copy).toJson(QJsonDocument::Compact));

    const int l = s->addLine(100, 0, 110, 0);
    s->addConstraint({{"type", "horizontal"}, {"a", QVariantMap{{"e", l}}}});
    r = s->moveEntities({l}, 0, 0, 90, 100, 0, false);
    const QVariantMap turned = s->entity(l);
    check(r.value("ok").toBool() && near(num(turned, "x2"), 100) && near(num(turned, "y2"), 10),
          "sketch move: rotating about a pivot drops the horizontal constraint",
          QJsonDocument::fromVariant(QVariantList{r, turned}).toJson(QJsonDocument::Compact));
}

void testMcMasterCarr() {
    check(McMasterCarrClient::extractPartNumber(QStringLiteral("91251a542 - socket head")) == "91251A542",
          "mcmaster: extract PN from Suggest name");
    check(McMasterCarrClient::extractPartNumber(QStringLiteral("/ImageCache/912/91251A540_abc@100p.png")) == "91251A540",
          "mcmaster: extract PN from image path");
    check(McMasterCarrClient::partNumberFromUrl(QStringLiteral("https://www.mcmaster.com/91251A194/"))
              == "91251A194",
          "mcmaster: PN from product URL");
    check(McMasterCarrClient::partNumberFromUrl(QStringLiteral("https://www.mcmaster.com/91251a194"))
              == "91251A194",
          "mcmaster: PN from product URL (no slash)");
    check(McMasterCarrClient::partNumberFromUrl(
              QStringLiteral("https://www.mcmaster.com/products/hex-head-screws/")).isEmpty(),
          "mcmaster: category URL is not a product");
    check(McMasterCarrClient::looksLikeBotChallenge(QByteArray("<!-- v8rD4psG challenge -->")),
          "mcmaster: detect Akamai challenge HTML");
    check(!McMasterCarrClient::looksLikeBotChallenge(QByteArray("{\"SrchSuggRsltsDat\":{}}")),
          "mcmaster: JSON is not a challenge");
    QVariantMap product;
    QVariantMap cad;
    QVariantMap avail;
    QVariantList threeD;
    threeD.append(QVariantMap{{"DisplayName", "3-D STEP"},
                              {"FilePath", "/mvD/Library/CAD2/x/PN.STEP"}});
    threeD.append(QVariantMap{{"DisplayName", "3-D STEP no threads"},
                              {"FilePath", "/mvD/Library/CAD2/x/PN_NO THREADS.STEP"}});
    avail.insert(QStringLiteral("ThreeDDownloads"), threeD);
    cad.insert(QStringLiteral("AvailableCAD"), avail);
    product.insert(QStringLiteral("cadControlDat"), cad);
    check(McMasterCarrClient::pickStepPath(product, false).endsWith(QStringLiteral("PN.STEP")),
          "mcmaster: prefer threaded STEP by default");
    check(McMasterCarrClient::pickStepPath(product, true).contains(QStringLiteral("NO THREADS")),
          "mcmaster: prefer no-threads STEP when asked");
    check(!McMasterCarrClient::instance()->cacheRoot().isEmpty(), "mcmaster: cache root is set");
}

void testPhase2And3() {
    auto bounds = [](FissionDocument &doc, int i) {
        double b[6];
        GeomUtils::boundingBox(doc.body(i)->shape()).Get(b[0], b[1], b[2], b[3], b[4], b[5]);
        return std::vector<double>(b, b + 6);
    };
    {
        // Revolve a body face: the 10 x 10 face at y = 0 turned 90 deg about Z (centroid radius 15)
        FissionDocument doc;
        box(&doc, 10, 0, 10, 10, 10);
        const auto front = [](const gp_Pnt &p) { return near(p.Y(), 0, 1e-6); };
        auto *rv = doc.addFeature("Revolve", {{"sketch", ""}, {"profiles", QVariantList{faceAt(&doc, 0, front)}},
                                              {"axis", "Z"}, {"extent", "angle"}, {"angle", "90 deg"},
                                              {"operation", "newBody"}});
        check(rv->isHealthy() && doc.bodyCount() == 2 && near(doc.body(1)->volume(), 100 * M_PI / 2 * 15, 2e-3),
              "revolve: a body face as the profile", status(&doc));
    }
    {
        // Variable fillet 2 -> 4 mm removes more than a constant 2 mm and less than a constant 4 mm
        FissionDocument doc;
        box(&doc, 0, 0, 40, 20, 10, "join");
        const auto ridge = [](const gp_Pnt &p) { return near(p.X(), 20) && near(p.Y(), 0, 1e-6) && near(p.Z(), 10); };
        auto *f = doc.addFeature("Fillet", {{"edges", QVariantList{edgeAt(&doc, 0, ridge)}}, {"radiusType", "variable"},
                                            {"radius", "2 mm"}, {"radius2", "4 mm"}});
        const double removed = 8000 - totalVolume(&doc);
        check(f->isHealthy() && removed > (4 - M_PI) * 40 * 1.05 && removed < (16 - 4 * M_PI) * 40 * 0.95,
              "fillet: variable radius along the edge", QString("removed %1, %2").arg(removed).arg(status(&doc)));
    }
    {
        // Twisting a square along a straight path keeps its area along the way
        FissionDocument doc;
        auto *profile = rectSketch(&doc, "XZ", -3, -3, 6, 6);
        auto *path = qobject_cast<FissionSketch *>(doc.addFeature("Sketch", {{"plane", "XY"}}));
        path->addLine(0, 0, 0, 50);
        doc.rebuild();
        auto *sw = doc.addFeature("Sweep", {{"sketch", profile->id()}, {"path", path->id()}, {"twist", "90 deg"}});
        check(sw->isHealthy() && near(totalVolume(&doc), 36 * 50, 2e-2) && faceCount(&doc, 0) == 6,
              "sweep: twist angle", QString("faces %1, %2").arg(doc.bodyCount() ? faceCount(&doc, 0) : 0).arg(status(&doc)));
    }
    {
        FissionDocument doc;
        auto *a = rectSketch(&doc, "XY", -10, -10, 20, 20);
        auto *plane = doc.addFeature("ConstructionPlane", {{"mode", "offset"}, {"base", "XY"}, {"offset", "30 mm"}});
        auto *b = qobject_cast<FissionSketch *>(doc.addFeature("Sketch", {{"plane", "feature:" + plane->id()}}));
        b->addRectangle(-5, -5, 10, 10);
        auto *spine = qobject_cast<FissionSketch *>(doc.addFeature("Sketch", {{"plane", "XZ"}}));
        spine->addLine(0, 0, 0, 30);
        auto *rail = qobject_cast<FissionSketch *>(doc.addFeature("Sketch", {{"plane", "XZ"}}));
        rail->addLine(10, 0, 5, 30);
        doc.rebuild();
        const QVariantList secs{QVariantMap{{"sketch", a->id()}}, QVariantMap{{"sketch", b->id()}}};
        auto *lf = doc.addFeature("Loft", {{"sections", secs}, {"guide", "centerline"}, {"centerline", spine->id()}});
        const double frustum = 30.0 / 3.0 * (400 + 100 + 200);
        check(lf->isHealthy() && totalVolume(&doc) > frustum * 0.8 && totalVolume(&doc) < frustum * 1.2,
              "loft: centerline guide", status(&doc));
        lf->set("guide", "rail");
        lf->set("rail", rail->id());
        doc.rebuild();
        check(lf->isHealthy() && totalVolume(&doc) > frustum * 0.8 && totalVolume(&doc) < frustum * 1.2,
              "loft: rail guide", status(&doc));
        lf->set("rail", a->id());
        doc.rebuild();
        check(!lf->isHealthy(), "loft: a section sketch cannot be the guide", lf->error());
    }
    {
        // Standard hole sizes: ISO 273 normal clearance for M6 is 6.6 mm; the M6 tap drill is 6 - 1.0825
        FissionDocument doc;
        box(&doc, 0, 0, 40, 20, 10, "join");
        const auto top = [](const gp_Pnt &p) { return near(p.Z(), 10, 1e-6); };
        auto *h = doc.addFeature("Hole", {{"face", faceAt(&doc, 0, top)}, {"center", QVariantList{"20", "10", "10"}},
                                          {"standard", "isoCoarse"}, {"size", "M6"}, {"fit", "normal"},
                                          {"depth", "5 mm"}, {"tipAngle", "0 deg"}});
        checkVolume(&doc, 8000 - M_PI * 3.3 * 3.3 * 5, "hole: M6 normal clearance size", 1e-4);
        h->set("tapped", true);
        h->set("modeled", false);
        doc.rebuild();
        const double r = (6 - 1.0825) / 2;
        checkVolume(&doc, 8000 - M_PI * r * r * 5, "hole: tapped M6 drills the tap drill", 1e-4);
        h->set("standard", "unc");
        h->set("size", "1/4-20");
        doc.rebuild();
        const double ru = (6.35 - 1.0825 * 25.4 / 20) / 2;
        check(h->isHealthy() && near(totalVolume(&doc), 8000 - M_PI * ru * ru * 5, 1e-4)
                  && h->property("designation").toString().contains("1/4-20"),
              "hole: inch UNC size", h->property("designation").toString() + " " + status(&doc));
        auto sizes = doc.threadSizes("unf");
        check(!sizes.isEmpty() && sizes.first().toMap().value("name") == "#2-64", "thread table: UNF sizes listed",
              QString::number(sizes.size()));
    }
    {
        FissionDocument rod;
        auto *c = qobject_cast<FissionSketch *>(rod.addFeature("Sketch", {{"plane", "XY"}}));
        c->addCircle(0, 0, 5);
        rod.rebuild();
        extrude(&rod, c, "20 mm");
        const auto cyl = [](const gp_Pnt &p) { return near(p.Z(), 10, 1e-6); };
        auto *th = rod.addFeature("Thread", {{"face", faceAt(&rod, 0, cyl)}, {"standard", "isoFine"}, {"modeled", false}});
        check(th->isHealthy() && th->property("designation").toString().startsWith("M10x1.25"),
              "thread: fine standard picks the size that fits", th->property("designation").toString());
    }
    {
        // Draft pulled along a straight edge; Offset Face shows its arrow even at zero distance
        FissionDocument doc;
        box(&doc, 0, 0, 10, 10, 10, "join");
        const double v0 = totalVolume(&doc);
        const auto upright = [](const gp_Pnt &p) { return near(p.X(), 10, 1e-6) && near(p.Y(), 10, 1e-6); };
        const auto side = [](const gp_Pnt &p) { return near(p.X(), 0, 1e-6); };
        auto *dr = doc.addFeature("Draft", {{"plane", edgeAt(&doc, 0, upright)}, {"faces", QVariantList{faceAt(&doc, 0, side)}},
                                            {"angle", "5 deg"}});
        check(dr->isHealthy() && !near(totalVolume(&doc), v0, 1e-4), "draft: a straight edge as the pull", status(&doc));
        doc.removeFeature(dr);
        auto *of = doc.addFeature("OffsetFace", {{"faces", QVariantList{faceAt(&doc, 0, side)}}, {"distance", "0 mm"}});
        check(of->isHealthy() && of->manipulator().value("key") == "distance", "offset face: arrow at zero distance",
              status(&doc));
    }
    {
        // Joint: the small block's bottom snaps onto the big block's top, centre on centre
        FissionDocument doc;
        box(&doc, 0, 0, 10, 10, 10);
        box(&doc, 30, 0, 10, 10, 5);
        const auto top = [](const gp_Pnt &p) { return near(p.Z(), 10, 1e-6); };
        const auto under = [](const gp_Pnt &p) { return near(p.Z(), 0, 1e-6) && p.X() > 20; };
        auto *j = doc.addFeature("Joint", {{"bodies", QVariantList{1}}, {"from", faceAt(&doc, 1, under)},
                                           {"to", faceAt(&doc, 0, top)}});
        auto b = bounds(doc, 1);
        check(j->isHealthy() && j->name() == "Joint1" && near(b[2], 10, 1e-4) && near(b[5], 15, 1e-4)
                  && near((b[0] + b[3]) / 2, 5, 1e-4) && near((b[1] + b[4]) / 2, 5, 1e-4),
              "joint: snaps one origin onto the other", status(&doc));
        j->set("offset", "2 mm");
        doc.rebuild();
        b = bounds(doc, 1);
        check(near(b[2], 12, 1e-4) && j->manipulator().value("key") == "offset", "joint: offset Z along the joint axis",
              status(&doc));
        j->set("from", faceAt(&doc, 0, top));
        doc.rebuild();
        check(!j->isHealthy(), "joint: the moving origin must be on the moving part", j->error());
        doc.removeFeature(j);

        auto *ab = doc.addFeature("Joint", {{"bodies", QVariantList{1}}, {"asBuilt", true}, {"motion", "revolute"},
                                            {"to", faceAt(&doc, 0, top)}, {"angle", "180 deg"}});
        b = bounds(doc, 1);
        check(ab->isHealthy() && ab->name() == "As-built Joint1" && near(b[0], -30, 1e-4) && near(b[3], -20, 1e-4)
                  && near(b[2], 0, 1e-4),
              "as-built joint: stays in place, turns about the joint axis", status(&doc));
    }
}

}   // namespace

namespace SelfTest {

int run() {
    g_failures = g_passes = 0;
    FissionMeshConvert::setBackgroundTriangles(std::numeric_limits<int>::max());   // results right away
    testExpressions();
    testExtrudeAndParameters();
    testJoinMultiProfileExtrude();
    testProfilesWithHoles();
    testFilletChamferShell();
    testRevolveLoftSweep();
    testPatternsAndMirror();
    testFacePatternCut();
    testSketchOnFaceAndFiles();
    testConstraints();
    testLoftFromFace();
    testHandlesAndMeshExport();
    testExtrudeFace();
    testDocumentSettings();
    testTangentChain();
    testSketchEditing();
    testSolidTools();
    testHoleAndThread();
    testMeshBodies();
    testInspect();
    testRemoveDraftHoleEdges();
    testSectionAnalyses();
    testMeshFitting();
    testImportCAD();
    testLinkedDesign();
    testSheetMetal();
    testSheetMetalPhase7();
    testScripts();
    testMoveTypes();
    testSketchMove();
    testPhase2And3();
    testMcMasterCarr();
    testMeshRepairShellCombine();
    testMeshFaceGroupsRemesh();
    testProgress();
    testBackgroundJobs();
    testDrawing();
    testLargeFileSave();
    testDocumentVersionsAndAutosave();
    testGifWriter();
    testViewportBackground();
    std::printf("\n%d passed, %d failed\n", g_passes, g_failures);
    std::fflush(stdout);
    return g_failures;
}

int stepBench(const QString &path) {
    FissionDocument doc;
    if (doc.importCAD(path).isEmpty()) { std::printf("import failed: %s\n", qPrintable(doc.lastError())); return 1; }
    std::printf("imported %d bodies\n", doc.bodyCount());
    std::fflush(stdout);
    int failures = 0;
    for (int b = 0; b < doc.bodyCount(); ++b) {
        const TopoDS_Shape body = doc.body(b)->shape();
        const auto faces = GeomUtils::faceMap(body);
        std::printf("body %d: %d faces, valid %d\n", b, faces.Extent(), int(BRepCheck_Analyzer(body).IsValid()));
        for (int i = 0; i < faces.Extent(); ++i) {
            const BRepAdaptor_Surface s(TopoDS::Face(faces(i + 1)));
            const bool planar = s.GetType() == GeomAbs_Plane;
            for (const char *op : {"join", "cut", "newBody"}) {
                std::printf("  face %d (%s, type %d) %s ... ", i, planar ? "plane" : "curved", int(s.GetType()), op);
                std::fflush(stdout);
                auto *f = doc.addFeature("Extrude", {{"profiles", QVariantList{GeomUtils::faceRef(b, body, i)}},
                                                     {"distance", QString(op) == "cut" ? "-1 mm" : "2 mm"},
                                                     {"operation", op}});
                const bool ok = f && f->isHealthy();
                std::printf("%s %s\n", ok ? "ok" : "error:", f ? qPrintable(f->error()) : "no feature");
                std::fflush(stdout);
                if (planar && !ok) ++failures;
                if (f) doc.removeFeature(f);
            }
        }
    }
    std::printf("%d planar-face failures\n", failures);
    return failures ? 1 : 0;
}

int meshBench(const QString &path) {
    QElapsedTimer t;
    t.start();
    MeshUtils::Mesh m;
    QString err;
    if (!MeshUtils::readFile(path, 1.0, m, err)) { std::printf("read failed: %s\n", qPrintable(err)); return 1; }
    std::printf("read: %zu triangles, %zu nodes, closed %d, %lld ms\n", m.tris.size(), m.nodes.size(),
                int(MeshUtils::isClosed(m)), t.restart());
    const TopoDS_Shape face = MeshUtils::toFace(m);
    const MeshUtils::Mesh back = MeshUtils::fromShape(face);
    std::printf("toFace + fromShape: %lld ms\n", t.restart());
    std::fflush(stdout);
    // MESHBENCH_QUICK: only the grouped prismatic conversions
    const bool quick = qEnvironmentVariableIsSet("MESHBENCH_QUICK");
    for (const bool accurate : {false, true}) {
        MeshUtils::Mesh g = back;
        QString why;
        // The dialog's defaults: Fast min size 0.1 %; Accurate boundary tolerance 0.001 (× diagonal)
        const int n = MeshUtils::generateFaceGroups(g, accurate, 20, 0.001, why, accurate ? 0.001 : 0);
        std::printf("face groups %s: %d groups in %lld ms %s\n", accurate ? "accurate" : "fast", n, t.restart(), qPrintable(why));
        QString summary;
        const TopoDS_Shape s = MeshUtils::toFittedBRep(g, true, why, &summary);
        std::printf("  prismatic with these groups: %s in %lld ms  %s\n", s.IsNull() ? "FAILED" : "ok", t.restart(),
                    qPrintable(s.IsNull() ? why : summary));
        if (!s.IsNull()) {
            // Edges reaching outside the mesh box show as stray lines and circles
            Bnd_Box meshBox;
            for (const gp_Pnt &p : g.nodes) meshBox.Add(p);
            const double slack = std::sqrt(meshBox.SquareExtent()) * 0.05;
            meshBox.Enlarge(slack);
            int stray = 0;
            for (TopExp_Explorer ex(s, TopAbs_EDGE); ex.More(); ex.Next()) {
                Bnd_Box eb;
                BndLib_Add3dCurve::Add(BRepAdaptor_Curve(TopoDS::Edge(ex.Current())), 0.0, eb);
                if (eb.IsVoid() || !meshBox.IsOut(eb)) {
                    double x0, y0, z0, x1, y1, z1, a0, b0, c0, a1, b1, c1;
                    eb.Get(x0, y0, z0, x1, y1, z1);
                    meshBox.Get(a0, b0, c0, a1, b1, c1);
                    if (x0 >= a0 && y0 >= b0 && z0 >= c0 && x1 <= a1 && y1 <= b1 && z1 <= c1) continue;
                }
                if (++stray <= 8) {
                    BRepAdaptor_Curve ac(TopoDS::Edge(ex.Current()));
                    double x0, y0, z0, x1, y1, z1;
                    eb.Get(x0, y0, z0, x1, y1, z1);
                    std::printf("    stray edge: curve type %d, range %.4g..%.4g, box (%.1f %.1f %.1f)-(%.1f %.1f %.1f)\n",
                                int(ac.GetType()), ac.FirstParameter(), ac.LastParameter(), x0, y0, z0, x1, y1, z1);
                }
            }
            std::printf("  edges outside the mesh: %d\n", stray);
        }
        std::fflush(stdout);
    }
    if (quick) return 0;
    for (const bool prismatic : {true, false}) {
        QString why, summary;
        const TopoDS_Shape s = MeshUtils::toFittedBRep(back, prismatic, why, &summary);
        std::printf("%s: %s in %lld ms  %s\n", prismatic ? "prismatic" : "faceted", s.IsNull() ? "FAILED" : "ok", t.restart(),
                    qPrintable(s.IsNull() ? why : summary));
        std::fflush(stdout);
        if (!s.IsNull()) {
            BRepMesh_IncrementalMesh(s, 0.05, Standard_False, 0.3);
            std::printf("  display tessellation: %lld ms\n", t.restart());
            std::fflush(stdout);
        }
    }
    const TopoDS_Shape tri = MeshUtils::toBRep(back, err);
    std::printf("one face per triangle: %s in %lld ms  %s\n", tri.IsNull() ? "FAILED" : "ok", t.restart(), qPrintable(err));
    std::fflush(stdout);
    return 0;
}

void buildDemo(FissionDocument *doc) {
    doc->setName("Demo Bracket");
    doc->addParameter("length", "80 mm", "mm", "plate length");
    doc->addParameter("width", "50 mm", "mm", "plate width");
    doc->addParameter("thickness", "8 mm", "mm", "plate thickness");
    doc->addParameter("hole_d", "8 mm", "mm", "mounting hole diameter");

    auto *base = qobject_cast<FissionSketch *>(doc->addFeature("Sketch", {{"plane", "XY"}}, "Base Sketch"));
    base->addRectangle("-length/2", "-width/2", "length", "width");
    doc->rebuild();
    doc->addFeature("Extrude", {{"sketch", base->id()}, {"distance", "thickness"}}, "Plate");

    auto *holes = qobject_cast<FissionSketch *>(doc->addFeature("Sketch", {{"plane", "XY"}}, "Holes"));
    holes->addCircle("-length/2 + 10", "-width/2 + 10", "hole_d/2");
    doc->rebuild();
    doc->addFeature("Extrude", {{"sketch", holes->id()}, {"extent", "throughAll"}, {"operation", "cut"}}, "Hole");

    auto *boss = qobject_cast<FissionSketch *>(doc->addFeature("Sketch", {{"plane", "XZ"}}, "Boss Profile"));
    boss->addPolyline({QVariantList{0, "thickness"}, QVariantList{16, "thickness"}, QVariantList{12, 34},
                       QVariantList{0, 34}}, true);
    doc->rebuild();
    doc->addFeature("Revolve", {{"sketch", boss->id()}, {"axis", "Z"}}, "Boss");

    auto *bore = qobject_cast<FissionSketch *>(doc->addFeature("Sketch", {{"plane", "XY"}}, "Bore"));
    bore->addCircle(0, 0, 6);
    doc->rebuild();
    doc->addFeature("Extrude", {{"sketch", bore->id()}, {"extent", "throughAll"}, {"operation", "cut"}}, "Bore");

    auto *corner = qobject_cast<FissionSketch *>(doc->addFeature("Sketch", {{"plane", "XY"}}, "Corner Holes"));
    corner->addCircle("length/2 - 10", "-width/2 + 10", "hole_d/2");
    corner->addCircle("length/2 - 10", "width/2 - 10", "hole_d/2");
    corner->addCircle("-length/2 + 10", "width/2 - 10", "hole_d/2");
    doc->rebuild();
    doc->addFeature("Extrude", {{"sketch", corner->id()}, {"extent", "throughAll"}, {"operation", "cut"}}, "Corner Holes");
    doc->addFeature("ConstructionPlane", {{"mode", "offset"}, {"base", "XY"}, {"offset", "34 mm"}}, "Top Plane");
}

}
