// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#pragma once
#include <QQuickFramebufferObject>
#include <QColor>
#include <QElapsedTimer>
#include <QPointer>
#include <QTimer>
#include <QVector3D>
#include <QVariantMap>
#include <memory>
#include <Bnd_Box.hxx>
#include <TopoDS_Shape.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <QHash>
#include "ViewportController.h"
#include "SelectionManager.h"
#include "kernel/FissionDocument.h"

class FissionSketch;

// Interleaved vertex data handed from the GUI thread to the render thread
struct RenderData {
    QVector<float> triangles;      // opaque: pos3 normal3 rgba4
    QVector<float> transparent;    // pos3 normal3 rgba4
    QVector<float> lines;          // depth-tested: pos3 rgba4
    QVector<float> overlayLines;   // sketch/construction curves: pos3 rgba4
    QVector<float> gridLines;      // pos3 rgba4, blended
    QVector<float> points;         // pos3 rgba4
    QVector<float> capSources;     // section on: the shown bodies' triangles (pos3 rgba4) for the cap pass
    QVector3D boundsMin, boundsMax;
};

// The hovered item only, drawn over RenderData; small, so hovering never re-sends the whole scene
struct HoverData {
    QVector<float> triangles, transparent;   // pos3 normal3 rgba4
    QVector<float> lines, overlayLines;      // pos3 rgba4
};

// What a triangle or polyline belongs to, for picking and highlighting
struct PickTag {
    // Preview: never picked. Origin*: index = 0..2 (XY / XZ / YZ, X / Y / Z), owner unused
    enum Kind { BodyFace, BodyEdge, Profile, Plane, SketchCurve, Axis, Point, Preview, OriginPoint, OriginAxis, OriginPlane };
    Kind kind;
    int owner;    // body index, or index into m_overlayIds
    int index;    // face/edge index, profile index, or sketch entity
};

class BRepRenderer : public QQuickFramebufferObject {
    Q_OBJECT
    Q_PROPERTY(FissionDocument *document READ document WRITE setDocument NOTIFY documentChanged)
    Q_PROPERTY(ViewportController *camera READ camera WRITE setCamera NOTIFY cameraChanged)
    Q_PROPERTY(SelectionManager *selection READ selection WRITE setSelection NOTIFY selectionChanged)
    Q_PROPERTY(bool showGrid READ showGrid WRITE setShowGrid NOTIFY displayOptionsChanged)
    // When true (default), free sketch clicks snap to the grid after endpoint snaps fail
    Q_PROPERTY(bool snapToGrid READ snapToGrid WRITE setSnapToGrid NOTIFY displayOptionsChanged)
    Q_PROPERTY(bool showEdges READ showEdges WRITE setShowEdges NOTIFY displayOptionsChanged)
    // Component colour cycling (Shift+N): every body drawn in its own hue instead of its appearance
    Q_PROPERTY(bool componentColors READ componentColors WRITE setComponentColors NOTIFY displayOptionsChanged)
    // How mesh bodies are drawn: "shadedEdges" (facets outlined), "shaded", "wireframe", "faceGroups"
    // (each face group in its own colour with its border)
    Q_PROPERTY(QString meshDisplay READ meshDisplay WRITE setMeshDisplay NOTIFY displayOptionsChanged)
    Q_PROPERTY(QVariantMap hovered READ hovered NOTIFY hoveredChanged)
    // Origin geometry to draw and pick: any of "O", "X", "Y", "Z", "XY", "XZ", "YZ".
    // Picks give {kind: "originpoint" | "originaxis" | "originplane", id}
    Q_PROPERTY(QStringList originItems READ originItems WRITE setOriginItems NOTIFY displayOptionsChanged)
    // Surface analysis drawn on the bodies: "" (shaded), "zebra", "draft", "curvature"
    Q_PROPERTY(QString analysis READ analysis WRITE setAnalysis NOTIFY displayOptionsChanged)
    // Draft analysis: faces leaning more than draftAngle (degrees) along +pull are green, against it red,
    // in between yellow
    Q_PROPERTY(QVector3D pullDirection READ pullDirection WRITE setPullDirection NOTIFY displayOptionsChanged)
    Q_PROPERTY(double draftAngle READ draftAngle WRITE setDraftAngle NOTIFY displayOptionsChanged)
    // Section analysis: bodies are cut away where dot(p, sectionNormal) > sectionOffset
    Q_PROPERTY(bool sectionEnabled READ sectionEnabled WRITE setSectionEnabled NOTIFY displayOptionsChanged)
    Q_PROPERTY(QVector3D sectionNormal READ sectionNormal WRITE setSectionNormal NOTIFY displayOptionsChanged)
    Q_PROPERTY(double sectionOffset READ sectionOffset WRITE setSectionOffset NOTIFY displayOptionsChanged)
    // Viewport canvas gradient (top → bottom). Change on the fly; persisted by the UI via settings.
    Q_PROPERTY(QColor backgroundTop READ backgroundTop WRITE setBackgroundTop NOTIFY backgroundChanged)
    Q_PROPERTY(QColor backgroundBottom READ backgroundBottom WRITE setBackgroundBottom NOTIFY backgroundChanged)

public:
    explicit BRepRenderer(QQuickItem *parent = nullptr);
    Renderer *createRenderer() const override;

    FissionDocument *document() const { return m_doc; }
    void setDocument(FissionDocument *doc);
    ViewportController *camera() const { return m_camera; }
    void setCamera(ViewportController *cam);
    SelectionManager *selection() const { return m_selection; }
    void setSelection(SelectionManager *sel);
    bool showGrid() const { return m_showGrid; }
    void setShowGrid(bool s);
    bool snapToGrid() const { return m_snapToGrid; }
    void setSnapToGrid(bool s);
    bool showEdges() const { return m_showEdges; }
    QString meshDisplay() const { return m_meshDisplay; }
    void setMeshDisplay(const QString &mode);
    void setShowEdges(bool s);
    bool componentColors() const { return m_componentColors; }
    void setComponentColors(bool on);
    QVariantMap hovered() const { return m_hovered; }
    QStringList originItems() const { return m_originItems; }
    void setOriginItems(const QStringList &items);
    QString analysis() const { return m_analysis; }
    void setAnalysis(const QString &a);
    QVector3D pullDirection() const { return m_pull; }
    void setPullDirection(const QVector3D &d);
    double draftAngle() const { return m_draftAngle; }
    void setDraftAngle(double deg);
    bool sectionEnabled() const { return m_sectionEnabled; }
    void setSectionEnabled(bool on);
    QVector3D sectionNormal() const { return m_sectionNormal; }
    void setSectionNormal(const QVector3D &n);
    double sectionOffset() const { return m_sectionOffset; }
    void setSectionOffset(double d);
    QColor backgroundTop() const { return m_bgTop; }
    void setBackgroundTop(const QColor &c);
    QColor backgroundBottom() const { return m_bgBottom; }
    void setBackgroundBottom(const QColor &c);
    // Model bounds [xmin, ymin, zmin, xmax, ymax, zmax] (empty when nothing is shown)
    Q_INVOKABLE QVariantList bounds() const;
    // Number of shown bodies the section plane cuts through, each getting a hatched cap (0 when off)
    Q_INVOKABLE int sectionCapCount() const;

    // Picks at item coordinates. filter: comma-separated kinds ("face,edge,profile,plane,sketchcurve,axis,body"),
    // empty = selection->filter(), "*" = anything.
    Q_INVOKABLE QVariantMap pick(double x, double y, const QString &filter = QString()) const;
    // 3D point under the cursor for point inputs: the nearest visible vertex, circle center or edge midpoint
    // within 12 px, else where the cursor meets a face. {x, y, z, label} or empty.
    Q_INVOKABLE QVariantMap snapPoint(double x, double y) const;
    // Window / freeform selection. polygon = [[x, y], ...] item coordinates. crossing = false takes items lying
    // completely inside, true also takes items the outline touches. Origin geometry is never included.
    Q_INVOKABLE QVariantList pickRegion(const QVariantList &polygon, bool crossing, const QString &filter = QString()) const;
    Q_INVOKABLE void hover(double x, double y);
    Q_INVOKABLE void clearHover();
    Q_INVOKABLE void fitAll(bool animate = true);

    // Sketch interaction. sketchPoint maps the cursor onto the sketch plane:
    // {u, v, snap:"endpoint"|"grid"|"", entity, role}; entity / role name the snapped sketch point
    // (entity -1 = sketch origin). Empty when the plane is edge-on.
    Q_INVOKABLE QVariantMap sketchPoint(double x, double y, const QString &sketchId, bool snap = true) const;
    // [[u, v], ...] sketch coordinates -> [[x, y], ...] item coordinates
    Q_INVOKABLE QVariantList projectSketch(const QString &sketchId, const QVariantList &uv) const;
    Q_INVOKABLE void lookAtSketch(const QString &sketchId, bool animate = true);
    // Grid spacing used for snapping at the current zoom
    Q_INVOKABLE double snapStep() const;

    // World point -> [x, y] item coordinates; empty when behind the camera
    Q_INVOKABLE QVariantList projectPoint(const QVector3D &p) const;
    // Signed distance along the axis (origin, dir) of the point closest to the cursor ray;
    // undefined when the axis points straight at the camera
    // Where the cursor ray meets the plane through `origin` with `normal` (undefined when parallel)
    Q_INVOKABLE QVariant planePoint(double x, double y, const QVector3D &origin, const QVector3D &normal) const;
    Q_INVOKABLE QVariant axisParameter(double x, double y, const QVector3D &origin, const QVector3D &dir) const;

    // Snapshot consumed by the render thread
    std::shared_ptr<const RenderData> renderData() const { return m_data; }
    quint64 dataVersion() const { return m_dataVersion; }
    std::shared_ptr<const HoverData> hoverData() const { return m_hoverData; }
    quint64 hoverVersion() const { return m_hoverVersion; }

signals:
    void documentChanged();
    void cameraChanged();
    void selectionChanged();
    void displayOptionsChanged();
    void backgroundChanged();
    void hoveredChanged();

private:
    // Bounding box of a run of triangles (index range into tris) or points, so picking skips what the ray misses
    struct Chunk {
        QVector3D lo, hi;
        int begin = 0, end = 0;
    };
    struct FaceMesh {
        PickTag tag;
        QVector<QVector3D> pos, nrm;
        QVector<int> tris;     // 3 indices per triangle
        QVector<float> curv;   // largest principal curvature per node (curvature analysis only)
        QColor color;
        float alpha = 1.0f;
        float roughness = 0.55f;
        float metalness = 0.0f;
        bool mesh = false;       // triangles of a mesh body (tag.index = face group)
        QVector<Chunk> chunks;
    };
    struct Polyline {
        PickTag tag;
        QVector<QVector3D> pts;
        QColor color;
        bool overlay = false;
        bool segments = false;   // pts are separate segment pairs (mesh wireframe) instead of one strip
        enum MeshLines { NotMesh, Wire, GroupBorder } mesh = NotMesh;
        QVector<Chunk> chunks;
    };
    static void buildChunks(FaceMesh &m);
    static void buildChunks(Polyline &pl);
    // Face / edge numbering of a displayed shape, kept while the shape is shown (picking needs it every move)
    struct ShapeMaps {
        TopoDS_Shape shape;
        TopTools_IndexedMapOfShape faces, edges;
    };
    const ShapeMaps &shapeMaps(const TopoDS_Shape &s) const;
    mutable QHash<const void *, ShapeMaps> m_mapCache;

    void tessellate();
    // Lets the status bar move and the window paint while a large model is meshed.
    void pumpDisplay(const QString &label, double fraction);
    void assemble();        // everything, with the selection highlighted
    void assembleHover();   // just the hovered item
    // Integer identity of a pick tag (0 = never matches) and the tag / whole body a selection ref names
    static quint64 tagCode(const PickTag &tag);
    void refCode(const QVariantMap &ref, quint64 &code, int &body) const;
    void addFace(const TopoDS_Shape &face, const PickTag &tag, const QColor &color, float alpha, double deflection);
    void addEdge(const TopoDS_Shape &edge, const PickTag &tag, const QColor &color, bool overlay, double deflection);
    void addMeshWire(const TopoDS_Shape &meshFace, const PickTag &tag);
    QString tagKey(const PickTag &tag) const;
    QString refKey(const QVariantMap &ref) const;
    QVariantMap tagRef(const PickTag &tag, const QString &filter) const;
    FissionSketch *sketch(const QString &id) const;
    QMatrix4x4 mvp() const;
    bool toScreen(const QMatrix4x4 &mvp, const QVector3D &p, QPointF &out) const;
    static bool allows(const QString &filter, const QString &kind);

    QPointer<FissionDocument> m_doc;
    QPointer<ViewportController> m_camera;
    QPointer<SelectionManager> m_selection;
    bool m_showGrid = true;
    bool m_snapToGrid = true;
    bool m_showEdges = true;
    bool m_componentColors = false;
    QString m_meshDisplay = QStringLiteral("shadedEdges");
    QStringList m_originItems;
    QString m_analysis;
    QVector3D m_pull{0, 0, 1};
    double m_draftAngle = 1.0;
    bool m_sectionEnabled = false;
    QVector3D m_sectionNormal{0, 0, 1};
    double m_sectionOffset = 0.0;
    QColor m_bgTop{60, 65, 72};       // Fusion-like dark grey gradient (default)
    QColor m_bgBottom{30, 33, 37};

    QVector<FaceMesh> m_faces;
    // Display geometry of each shown body by its TShape; reused while the body is unchanged
    struct BodyGeometry {
        TopoDS_Shape shape;
        bool curvature = false, input = false;
        QVector<FaceMesh> faces;
        QVector<Polyline> lines;
    };
    QHash<const void *, BodyGeometry> m_bodyCache;
    QHash<const void *, QPair<TopoDS_Shape, Bnd_Box>> m_boxCache;
    QVector<QPair<QVector3D, QVector3D>> m_bodyBoxes;   // min / max of each shown body
    double m_deflection = 0.1;
    QVector<Polyline> m_polylines;
    QVector<QVector3D> m_points;
    QStringList m_overlayIds;
    QVector3D m_boundsMin, m_boundsMax;
    bool m_hasBounds = false;
    QVariantMap m_hovered;

    std::shared_ptr<const RenderData> m_data;
    quint64 m_dataVersion = 0;
    std::shared_ptr<const HoverData> m_hoverData;
    quint64 m_hoverVersion = 0;
    QTimer m_tessellateTimer;
    QElapsedTimer m_uiPump;
    bool m_inTessellate = false;
    bool m_tessellateAgain = false;
    bool m_streamDisplay = false;
};
