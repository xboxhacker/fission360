// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#include "BRepRenderer.h"
#include "kernel/FissionSketch.h"
#include "kernel/FissionConstruction.h"
#include "kernel/GeomUtils.h"
#include "kernel/MeshUtils.h"
#include "kernel/render/HdrImage.h"

#include <QFileInfo>
#include <QImage>
#include <QOpenGLBuffer>
#include <QOpenGLExtraFunctions>
#include <QOpenGLFramebufferObject>
#include <QOpenGLShaderProgram>
#include <QOpenGLTexture>
#include <QOpenGLVertexArrayObject>
#include <QQuickOpenGLUtils>
#include <memory>
#include <QElapsedTimer>
#include <QHash>
#include <QPolygonF>
#include <QSet>

#include <BRepAdaptor_Curve.hxx>
#include <Bnd_Box.hxx>
#include <gp_Pln.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <BRepMesh_IncrementalMesh.hxx>
#include <BRep_Tool.hxx>
#include <GCPnts_TangentialDeflection.hxx>
#include <GeomLProp_SLProps.hxx>
#include <Poly_Triangulation.hxx>
#include <Standard_Failure.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopoDS.hxx>
#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

#ifndef GL_PROGRAM_POINT_SIZE
#define GL_PROGRAM_POINT_SIZE 0x8642
#endif

namespace {

const QColor kEdgeColor(28, 30, 34);
const QColor kMeshWireColor(70, 76, 86);
const QColor kSketchColor(110, 180, 255);
const QColor kSketchFixedColor(236, 240, 245);
const QColor kSketchConstructionColor(240, 140, 40);
const QColor kSketchProjectedColor(196, 120, 255);
const char *const kOriginPlanes[] = {"XY", "XZ", "YZ"};
const char *const kOriginAxes[] = {"X", "Y", "Z"};
const QColor kAxisColors[] = {QColor(225, 70, 70), QColor(80, 200, 90), QColor(80, 130, 235)};
const QColor kProfileColor(255, 170, 70);
const QColor kConstructionColor(240, 150, 50);
const QColor kSelectColor(40, 130, 255);
const QColor kHoverColor(120, 190, 255);
const QColor kDefaultBodyColor(170, 175, 182);

const char *kMeshVS = R"(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec4 aColor;
layout(location = 3) in vec2 aMat;   // roughness, metalness
uniform mat4 uMvp;
out vec3 vPos; out vec3 vNormal; out vec4 vColor; out vec2 vMat;
void main() {
    vPos = aPos; vNormal = aNormal; vColor = aColor; vMat = aMat;
    gl_Position = uMvp * vec4(aPos, 1.0);
}
)";

// uMode: 0 shaded, 1 zebra stripes, 2 draft analysis (curvature colours arrive as vertex colours).
// uClip: fragments beyond the section plane are dropped; inside surfaces seen through the cut are tinted.
// uLight*: sun direction from the document's RenderSettings (viewport / in-canvas look).
const char *kMeshFS = R"(#version 330 core
in vec3 vPos; in vec3 vNormal; in vec4 vColor; in vec2 vMat;
uniform vec3 uEye; uniform vec3 uUp; uniform vec3 uViewDir; uniform bool uOrtho;
uniform int uMode; uniform vec3 uPull; uniform float uDraftSin;
uniform bool uClip; uniform vec4 uClipPlane;
uniform vec3 uLightDir; uniform vec3 uLightColor; uniform float uLightIntensity;
uniform sampler2D uEnvMap; uniform bool uHasEnv; uniform float uEnvIntensity; uniform float uEnvRotation;
out vec4 fragColor;
vec2 envUV(vec3 d) {
    d = normalize(d);
    float ang = atan(d.x, -d.y) + uEnvRotation;
    return vec2(fract(ang / (2.0 * 3.14159265) + 0.5), acos(clamp(d.z, -1.0, 1.0)) / 3.14159265);
}
vec3 sampleEnv(vec3 d) {
    if (!uHasEnv) return vec3(0.22, 0.24, 0.27);
    return texture(uEnvMap, envUV(d)).rgb * uEnvIntensity;
}
void main() {
    if (uClip && dot(vPos, uClipPlane.xyz) > uClipPlane.w) discard;
    vec3 V = uOrtho ? -uViewDir : normalize(uEye - vPos);
    vec3 N0 = normalize(vNormal);
    vec3 N = N0;
    bool inside = dot(N, V) < 0.0;
    if (inside) N = -N;
    vec3 base = vColor.rgb;
    if (uMode == 2) {
        float s = dot(N0, normalize(uPull));
        base = s > uDraftSin ? vec3(0.25, 0.75, 0.32) : s < -uDraftSin ? vec3(0.86, 0.27, 0.25) : vec3(0.96, 0.80, 0.22);
    }
    if (uClip && inside) base = vec3(0.42, 0.44, 0.47);
    float rough = clamp(vMat.x, 0.04, 1.0);
    float metal = clamp(vMat.y, 0.0, 1.0);
    vec3 L = length(uLightDir) > 0.01 ? normalize(uLightDir) : normalize(V + 0.45 * uUp);
    float intensity = max(uLightIntensity, 0.15);
    float diff = max(dot(N, L), 0.0);
    float shininess = mix(8.0, 160.0, pow(1.0 - rough, 1.5));
    float specAmt = mix(0.10, 0.72, metal) * (1.0 - 0.65 * rough);
    float spec = pow(max(dot(N, normalize(L + V)), 0.0), shininess) * specAmt;
    vec3 diffuseCol = mix(base, base * 0.18, metal);
    vec3 specCol = mix(vec3(1.0), base, metal) * uLightColor;
    vec3 ambient = sampleEnv(N) * mix(0.35, 0.08, metal) * base;
    vec3 R = reflect(-V, N);
    vec3 envSpec = sampleEnv(normalize(mix(R, N, rough * 0.85))) * mix(0.15, 0.85, metal) * (1.0 - rough * 0.5);
    vec3 c = ambient + diffuseCol * (0.22 + 0.68 * diff * intensity) + specCol * (spec * intensity) + envSpec * specCol;
    if (uMode == 1 && !(uClip && inside)) {
        float band = fract(asin(clamp(R.z, -1.0, 1.0)) * 4.0);
        float w = fwidth(band) * 1.5;
        float stripe = smoothstep(0.5 - w, 0.5 + w, band);
        c = mix(vec3(0.06), vec3(0.96), stripe) * (0.75 + 0.25 * diff);
    }
    fragColor = vec4(c, vColor.a);
}
)";

// Equirectangular environment as the viewport background (replaces the gradient when an HDRI is active)
const char *kSkyVS = R"(#version 330 core
layout(location = 0) in vec3 aPos;
out vec2 vUv;
void main() { vUv = aPos.xy * 0.5 + 0.5; gl_Position = vec4(aPos.xy, 0.999, 1.0); }
)";
const char *kSkyFS = R"(#version 330 core
in vec2 vUv;
uniform sampler2D uEnvMap;
uniform vec3 uEye; uniform vec3 uTarget; uniform vec3 uUp; uniform float uAspect;
uniform bool uOrtho; uniform float uEnvIntensity; uniform float uEnvRotation;
out vec4 fragColor;
vec2 envUV(vec3 d) {
    d = normalize(d);
    float ang = atan(d.x, -d.y) + uEnvRotation;
    return vec2(fract(ang / (2.0 * 3.14159265) + 0.5), acos(clamp(d.z, -1.0, 1.0)) / 3.14159265);
}
void main() {
    vec3 forward = normalize(uTarget - uEye);
    vec3 right = normalize(cross(forward, uUp));
    vec3 up = cross(right, forward);
    vec2 ndc = vUv * 2.0 - 1.0;
    vec3 dir = uOrtho ? forward : normalize(forward + ndc.x * right * uAspect + ndc.y * up);
    vec3 c = texture(uEnvMap, envUV(dir)).rgb * uEnvIntensity;
    c = c / (1.0 + c);
    c = pow(c, vec3(1.0 / 2.2));
    fragColor = vec4(c, 1.0);
}
)";

const char *kLineVS = R"(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 2) in vec4 aColor;
uniform mat4 uMvp; uniform float uPointSize;
out vec4 vColor; out vec3 vPos;
void main() { vColor = aColor; vPos = aPos; gl_Position = uMvp * vec4(aPos, 1.0); gl_PointSize = uPointSize; }
)";

const char *kLineFS = R"(#version 330 core
in vec4 vColor; in vec3 vPos; out vec4 fragColor;
uniform bool uClip; uniform vec4 uClipPlane;
void main() {
    if (uClip && dot(vPos, uClipPlane.xyz) > uClipPlane.w) discard;
    fragColor = vColor;
}
)";

// Section cap: light green with diagonal hatching, so cut material reads apart from the outside surfaces
const char *kCapFS = R"(#version 330 core
in vec4 vColor; in vec3 vPos; out vec4 fragColor;
void main() {
    float h = mod(gl_FragCoord.x - gl_FragCoord.y, 9.0);
    fragColor = h < 1.5 ? vec4(0.33, 0.55, 0.33, 1.0) : vec4(0.68, 0.86, 0.62, 1.0);
}
)";

class FboRenderer : public QQuickFramebufferObject::Renderer, protected QOpenGLExtraFunctions {
public:
    FboRenderer() {
        initializeOpenGLFunctions();
        m_mesh.addShaderFromSourceCode(QOpenGLShader::Vertex, kMeshVS);
        m_mesh.addShaderFromSourceCode(QOpenGLShader::Fragment, kMeshFS);
        m_mesh.link();
        m_line.addShaderFromSourceCode(QOpenGLShader::Vertex, kLineVS);
        m_line.addShaderFromSourceCode(QOpenGLShader::Fragment, kLineFS);
        m_line.link();
        m_cap.addShaderFromSourceCode(QOpenGLShader::Vertex, kLineVS);
        m_cap.addShaderFromSourceCode(QOpenGLShader::Fragment, kCapFS);
        m_cap.link();
        m_sky.addShaderFromSourceCode(QOpenGLShader::Vertex, kSkyVS);
        m_sky.addShaderFromSourceCode(QOpenGLShader::Fragment, kSkyFS);
        m_sky.link();

        m_bgTop = QColor(60, 65, 72);
        m_bgBottom = QColor(30, 33, 37);
        rebuildBackground();
    }

    void ensureEnvTexture(const QString &path) {
        if (path == m_envPath && m_envTex) return;
        m_envPath = path;
        m_envTex.reset();
        if (path.isEmpty() || !QFileInfo::exists(path)) return;
        HdrImage hdr;
        QImage img;
        if (path.endsWith(QLatin1String(".hdr"), Qt::CaseInsensitive) && hdr.load(path))
            img = hdr.toToneMappedImage(1.0f);
        else
            img = QImage(path).convertToFormat(QImage::Format_RGB888);
        if (img.isNull()) return;
        m_envTex = std::make_unique<QOpenGLTexture>(img.mirrored(false, true));
        m_envTex->setMinificationFilter(QOpenGLTexture::LinearMipMapLinear);
        m_envTex->setMagnificationFilter(QOpenGLTexture::Linear);
        m_envTex->setWrapMode(QOpenGLTexture::Repeat);
        m_envTex->generateMipMaps();
    }

    void rebuildBackground() {
        auto rgba = [](const QColor &c, float out[4]) {
            out[0] = float(c.redF()); out[1] = float(c.greenF());
            out[2] = float(c.blueF()); out[3] = 1.f;
        };
        float top[4], bottom[4];
        rgba(m_bgTop, top);
        rgba(m_bgBottom, bottom);
        QVector<float> bg;
        auto v = [&](float x, float y, const float *c) { bg << x << y << 0.999f << c[0] << c[1] << c[2] << c[3]; };
        v(-1, -1, bottom); v(1, -1, bottom); v(1, 1, top);
        v(-1, -1, bottom); v(1, 1, top); v(-1, 1, top);
        upload(m_background, bg, false);
    }

    QOpenGLFramebufferObject *createFramebufferObject(const QSize &size) override {
        QOpenGLFramebufferObjectFormat fmt;
        fmt.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
        fmt.setSamples(4);
        return new QOpenGLFramebufferObject(size, fmt);
    }

    void synchronize(QQuickFramebufferObject *item) override {
        auto *r = static_cast<BRepRenderer *>(item);
        if (r->dataVersion() != m_version) {
            m_pending = r->renderData();
            m_version = r->dataVersion();
        }
        if (r->hoverVersion() != m_hoverVersion) {
            m_pendingHover = r->hoverData();
            m_hoverVersion = r->hoverVersion();
        }
        if (auto *cam = r->camera()) {
            const double aspect = item->width() / std::max(1.0, item->height());
            m_mvp = cam->projectionMatrix(aspect) * cam->viewMatrix();
            m_eye = cam->eye();
            m_up = cam->up();
            m_target = cam->target();
            m_viewDir = (cam->target() - cam->eye()).normalized();
            m_ortho = cam->orthographic();
            m_aspect = float(item->width() / std::max(1.0, item->height()));
        }
        const QString a = r->analysis();
        m_mode = a == "zebra" ? 1 : a == "draft" ? 2 : 0;
        m_pull = r->pullDirection().normalized();
        m_draftSin = float(std::sin(r->draftAngle() * M_PI / 180.0));
        m_clip = r->sectionEnabled();
        const QVector3D n = r->sectionNormal().normalized();
        m_clipPlane = QVector4D(n, float(r->sectionOffset()));
        if (r->backgroundTop() != m_bgTop || r->backgroundBottom() != m_bgBottom) {
            m_bgTop = r->backgroundTop();
            m_bgBottom = r->backgroundBottom();
            rebuildBackground();
        }
        // Sun + environment from document RenderSettings
        m_lightIntensity = 1.0f;
        m_lightColor = QVector3D(1, 1, 1);
        m_lightDir = QVector3D(0.45f, 0.35f, 0.8f);
        m_envIntensity = 1.0f;
        m_envRotation = 0.0f;
        m_wantEnvBg = false;
        QString envPath;
        if (auto *doc = r->document()) {
            if (auto *rs = doc->renderSettings()) {
                const double ang = rs->lightAngle() * M_PI / 180.0;
                const double alt = rs->lightAltitude() * M_PI / 180.0;
                m_lightDir = QVector3D(float(std::cos(alt) * std::cos(ang)),
                                       float(std::cos(alt) * std::sin(ang)),
                                       float(std::sin(alt)));
                if (!rs->useSunLight()) m_lightDir = QVector3D(0.2f, 0.3f, 1.0f);
                m_lightIntensity = float(rs->lightIntensity());
                const QColor lc = rs->lightColor();
                m_lightColor = QVector3D(float(lc.redF()), float(lc.greenF()), float(lc.blueF()));
                envPath = rs->environmentPath();
                m_envIntensity = float(rs->environmentIntensity());
                m_envRotation = float(rs->environmentRotation() * M_PI / 180.0);
                m_wantEnvBg = rs->sceneBackgroundType() == QLatin1String("environment") && !envPath.isEmpty();
            }
        }
        // Only load local files (downloaded HDRIs); skip http URLs until cached
        if (!envPath.isEmpty() && !envPath.startsWith(QLatin1String("http")))
            ensureEnvTexture(envPath);
        else
            ensureEnvTexture(QString());
    }

    void render() override {
        if (m_pending) {
            upload(m_tris, m_pending->triangles, true);
            upload(m_transparent, m_pending->transparent, true);
            upload(m_lines, m_pending->lines, false);
            upload(m_overlay, m_pending->overlayLines, false);
            upload(m_grid, m_pending->gridLines, false);
            upload(m_points, m_pending->points, false);
            upload(m_capSources, m_pending->capSources, false);
            m_boundsMin = m_pending->boundsMin;
            m_boundsMax = m_pending->boundsMax;
            m_pending.reset();
        }
        if (m_pendingHover) {
            upload(m_hoverTris, m_pendingHover->triangles, true);
            upload(m_hoverTransparent, m_pendingHover->transparent, true);
            upload(m_hoverLines, m_pendingHover->lines, false);
            upload(m_hoverOverlay, m_pendingHover->overlayLines, false);
            m_pendingHover.reset();
        }

        glClearColor(float(m_bgBottom.redF()), float(m_bgBottom.greenF()), float(m_bgBottom.blueF()), 1.f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glDisable(GL_DEPTH_TEST);
        if (m_wantEnvBg && m_envTex) {
            m_sky.bind();
            m_sky.setUniformValue("uEnvMap", 0);
            m_sky.setUniformValue("uEye", m_eye);
            m_sky.setUniformValue("uTarget", m_target);
            m_sky.setUniformValue("uUp", m_up);
            m_sky.setUniformValue("uAspect", m_aspect);
            m_sky.setUniformValue("uOrtho", m_ortho);
            m_sky.setUniformValue("uEnvIntensity", m_envIntensity);
            m_sky.setUniformValue("uEnvRotation", m_envRotation);
            m_envTex->bind(0);
            draw(m_background, GL_TRIANGLES);
            m_envTex->release();
        } else {
            m_line.bind();
            m_line.setUniformValue("uClip", false);
            m_line.setUniformValue("uMvp", QMatrix4x4());
            draw(m_background, GL_TRIANGLES);
        }

        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);
        m_line.setUniformValue("uMvp", m_mvp);

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);
        draw(m_grid, GL_LINES);
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);

        m_mesh.bind();
        m_mesh.setUniformValue("uMvp", m_mvp);
        m_mesh.setUniformValue("uEye", m_eye);
        m_mesh.setUniformValue("uUp", m_up);
        m_mesh.setUniformValue("uViewDir", m_viewDir);
        m_mesh.setUniformValue("uOrtho", m_ortho);
        m_mesh.setUniformValue("uMode", m_mode);
        m_mesh.setUniformValue("uPull", m_pull);
        m_mesh.setUniformValue("uDraftSin", m_draftSin);
        m_mesh.setUniformValue("uClip", m_clip);
        m_mesh.setUniformValue("uClipPlane", m_clipPlane);
        m_mesh.setUniformValue("uLightDir", m_lightDir);
        m_mesh.setUniformValue("uLightColor", m_lightColor);
        m_mesh.setUniformValue("uLightIntensity", m_lightIntensity);
        m_mesh.setUniformValue("uHasEnv", m_envTex != nullptr);
        m_mesh.setUniformValue("uEnvIntensity", m_envIntensity);
        m_mesh.setUniformValue("uEnvRotation", m_envRotation);
        m_mesh.setUniformValue("uEnvMap", 0);
        if (m_envTex) m_envTex->bind(0);
        // Push faces back slightly so edges drawn on them win the depth test
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(1.0f, 1.0f);
        draw(m_tris, GL_TRIANGLES);
        draw(m_hoverTris, GL_TRIANGLES);   // same depth as the face under it, so LEQUAL lets it through
        glDisable(GL_POLYGON_OFFSET_FILL);

        if (m_clip && m_capSources.count && QVector3D::dotProduct(m_eye, m_clipPlane.toVector3D()) > m_clipPlane.w())
            drawCaps();

        m_line.bind();
        m_line.setUniformValue("uClip", m_clip);
        m_line.setUniformValue("uClipPlane", m_clipPlane);
        draw(m_lines, GL_LINES);
        draw(m_hoverLines, GL_LINES);
        m_line.setUniformValue("uClip", false);

        glEnable(GL_BLEND);
        glDepthMask(GL_FALSE);
        m_mesh.bind();
        m_mesh.setUniformValue("uMode", 0);
        m_mesh.setUniformValue("uClip", false);
        draw(m_transparent, GL_TRIANGLES);
        draw(m_hoverTransparent, GL_TRIANGLES);
        glDepthMask(GL_TRUE);

        m_line.bind();
        draw(m_overlay, GL_LINES);
        draw(m_hoverOverlay, GL_LINES);
        glEnable(GL_PROGRAM_POINT_SIZE);
        m_line.setUniformValue("uPointSize", 7.0f);
        draw(m_points, GL_POINTS);
        glDisable(GL_PROGRAM_POINT_SIZE);
        glDisable(GL_BLEND);
        m_line.release();

        QQuickOpenGLUtils::resetOpenGLState();
    }

private:
    struct Batch {
        QOpenGLVertexArrayObject vao;
        QOpenGLBuffer vbo{QOpenGLBuffer::VertexBuffer};
        int count = 0;
    };

    // Stencil capping, seen from the removed side: every kept surface behind a pixel flips its stencil bit,
    // so an odd count means the plane passes through material there. A quad on the plane is then drawn
    // only on those pixels. Works for any closed body, solids and meshes alike.
    void drawCaps() {
        const QVector3D n = m_clipPlane.toVector3D();
        const QVector3D c = (m_boundsMin + m_boundsMax) / 2;
        const float size = std::max(1.0f, (m_boundsMax - m_boundsMin).length());
        const QVector3D p = c - n * (QVector3D::dotProduct(c, n) - m_clipPlane.w());
        const QVector3D u = QVector3D::crossProduct(n, std::fabs(n.z()) < 0.9f ? QVector3D(0, 0, 1) : QVector3D(1, 0, 0))
                                .normalized() * size;
        const QVector3D v = QVector3D::crossProduct(n, u);
        QVector<float> quad;
        for (const QVector3D &q : {p - u - v, p + u - v, p + u + v, p - u - v, p + u + v, p - u + v})
            quad << q.x() << q.y() << q.z() << 0.f << 0.f << 0.f << 1.f;
        upload(m_capQuad, quad, false);

        glEnable(GL_STENCIL_TEST);
        glStencilMask(0x01);
        glClearStencil(0);
        glClear(GL_STENCIL_BUFFER_BIT);
        glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
        glDepthMask(GL_FALSE);
        glDisable(GL_DEPTH_TEST);
        glStencilFunc(GL_ALWAYS, 0, 0x01);
        glStencilOp(GL_KEEP, GL_KEEP, GL_INVERT);
        m_line.bind();
        m_line.setUniformValue("uMvp", m_mvp);
        m_line.setUniformValue("uClip", true);
        m_line.setUniformValue("uClipPlane", m_clipPlane);
        draw(m_capSources, GL_TRIANGLES);

        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glDepthMask(GL_TRUE);
        glEnable(GL_DEPTH_TEST);
        glStencilFunc(GL_EQUAL, 1, 0x01);
        glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
        m_cap.bind();
        m_cap.setUniformValue("uMvp", m_mvp);
        draw(m_capQuad, GL_TRIANGLES);
        glStencilMask(0xFF);
        glDisable(GL_STENCIL_TEST);
    }

    void upload(Batch &b, const QVector<float> &data, bool withNormal) {
        if (!b.vao.isCreated()) { b.vao.create(); b.vbo.create(); }
        // Mesh: pos3 normal3 rgba4 rough/metal2 = 12. Lines/points: pos3 rgba4 = 7.
        const int stride = withNormal ? 12 : 7;
        b.vao.bind();
        b.vbo.bind();
        b.vbo.allocate(data.constData(), int(data.size() * sizeof(float)));
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride * sizeof(float), nullptr);
        if (withNormal) {
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride * sizeof(float), reinterpret_cast<void *>(3 * sizeof(float)));
            glEnableVertexAttribArray(3);
            glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, stride * sizeof(float), reinterpret_cast<void *>(10 * sizeof(float)));
        } else {
            glDisableVertexAttribArray(3);
        }
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, stride * sizeof(float),
                              reinterpret_cast<void *>((withNormal ? 6 : 3) * sizeof(float)));
        b.vao.release();
        b.vbo.release();
        b.count = data.size() / stride;
    }

    void draw(Batch &b, GLenum mode) {
        if (b.count == 0) return;
        b.vao.bind();
        glDrawArrays(mode, 0, b.count);
        b.vao.release();
    }

    QOpenGLShaderProgram m_mesh, m_line, m_cap, m_sky;
    Batch m_background, m_tris, m_transparent, m_lines, m_overlay, m_grid, m_points, m_capSources, m_capQuad;
    Batch m_hoverTris, m_hoverTransparent, m_hoverLines, m_hoverOverlay;
    QVector3D m_boundsMin, m_boundsMax;
    std::shared_ptr<const RenderData> m_pending;
    std::shared_ptr<const HoverData> m_pendingHover;
    quint64 m_version = std::numeric_limits<quint64>::max();
    quint64 m_hoverVersion = std::numeric_limits<quint64>::max();
    QMatrix4x4 m_mvp;
    QVector3D m_eye, m_up, m_viewDir, m_target;
    float m_aspect = 1.0f;
    bool m_ortho = false;
    int m_mode = 0;
    QVector3D m_pull{0, 0, 1};
    float m_draftSin = 0.017f;
    bool m_clip = false;
    QVector4D m_clipPlane;
    QColor m_bgTop{60, 65, 72}, m_bgBottom{30, 33, 37};
    QVector3D m_lightDir{0.45f, 0.35f, 0.8f};
    QVector3D m_lightColor{1, 1, 1};
    float m_lightIntensity = 1.0f;
    QString m_envPath;
    std::unique_ptr<QOpenGLTexture> m_envTex;
    float m_envIntensity = 1.0f;
    float m_envRotation = 0.0f;
    bool m_wantEnvBg = false;
};

QVector3D toQt(const gp_Pnt &p) { return QVector3D(float(p.X()), float(p.Y()), float(p.Z())); }

void pushColor(QVector<float> &v, const QColor &c, float alpha) {
    v << float(c.redF()) << float(c.greenF()) << float(c.blueF()) << alpha;
}

void pushPbr(QVector<float> &v, const QColor &c, float alpha, float roughness, float metalness) {
    pushColor(v, c, alpha);
    v << roughness << metalness;
}

// Triplanar sample of an albedo map (world-space); used when a body has a downloaded texture
QColor sampleTriplanar(const QImage &img, const QVector3D &p, const QVector3D &n, float scaleMm = 50.0f) {
    if (img.isNull()) return Qt::white;
    auto sample = [&](float u, float v) {
        const int x = int(std::floor(u * img.width())) % img.width();
        const int y = int(std::floor(v * img.height())) % img.height();
        return img.pixelColor((x + img.width()) % img.width(), (y + img.height()) % img.height());
    };
    const QVector3D an(std::fabs(n.x()), std::fabs(n.y()), std::fabs(n.z()));
    const float sum = an.x() + an.y() + an.z() + 1e-5f;
    const QColor cx = sample(p.y() / scaleMm, p.z() / scaleMm);
    const QColor cy = sample(p.x() / scaleMm, p.z() / scaleMm);
    const QColor cz = sample(p.x() / scaleMm, p.y() / scaleMm);
    const float wx = an.x() / sum, wy = an.y() / sum, wz = an.z() / sum;
    return QColor::fromRgbF(
        cx.redF() * wx + cy.redF() * wy + cz.redF() * wz,
        cx.greenF() * wx + cy.greenF() * wy + cz.greenF() * wz,
        cx.blueF() * wx + cy.blueF() * wy + cz.blueF() * wz);
}

// Blue (flat) -> green -> red (radius about 1/50 of the model or tighter)
QColor curvatureColor(float k, double modelDiag) {
    const double t = std::clamp(std::sqrt(k * modelDiag / 50.0), 0.0, 1.0);
    return QColor::fromHsvF(float((1.0 - t) * 0.66), 0.8f, 0.95f);
}

} // namespace

BRepRenderer::BRepRenderer(QQuickItem *parent) : QQuickFramebufferObject(parent) {
    setMirrorVertically(true);
    setAcceptHoverEvents(false);
    m_data = std::make_shared<RenderData>();
    m_tessellateTimer.setSingleShot(true);
    // Coalesce rapid displayChanged bursts (timeline rebuild) instead of remeshing every tick
    m_tessellateTimer.setInterval(32);
    connect(&m_tessellateTimer, &QTimer::timeout, this, &BRepRenderer::tessellate);
    assemble();
}

QQuickFramebufferObject::Renderer *BRepRenderer::createRenderer() const {
    return new FboRenderer();
}

void BRepRenderer::setDocument(FissionDocument *doc) {
    if (m_doc == doc) return;
    if (m_doc) m_doc->disconnect(this);
    m_doc = doc;
    if (m_doc) {
        connect(m_doc, &FissionDocument::displayChanged, this, [this] { m_tessellateTimer.start(); });
        // Live Render / HDRI / sun changes must refresh the viewport without a save-PNG dialog
        connect(m_doc, &FissionDocument::renderSettingsChanged, this, [this] { update(); });
    }
    m_hovered.clear();
    emit documentChanged();
    tessellate();
}

void BRepRenderer::setCamera(ViewportController *cam) {
    if (m_camera == cam) return;
    if (m_camera) m_camera->disconnect(this);
    m_camera = cam;
    if (m_camera) connect(m_camera, &ViewportController::changed, this, &QQuickItem::update);
    emit cameraChanged();
    update();
}

void BRepRenderer::setSelection(SelectionManager *sel) {
    if (m_selection == sel) return;
    if (m_selection) m_selection->disconnect(this);
    m_selection = sel;
    if (m_selection) connect(m_selection, &SelectionManager::selectionChanged, this, &BRepRenderer::assemble);
    emit selectionChanged();
    assemble();
}

void BRepRenderer::setShowGrid(bool s) {
    if (m_showGrid != s) { m_showGrid = s; emit displayOptionsChanged(); assemble(); }
}

void BRepRenderer::setSnapToGrid(bool s) {
    if (m_snapToGrid != s) { m_snapToGrid = s; emit displayOptionsChanged(); }
}

void BRepRenderer::setShowEdges(bool s) {
    if (m_showEdges != s) { m_showEdges = s; emit displayOptionsChanged(); assemble(); }
}

void BRepRenderer::setComponentColors(bool on) {
    if (m_componentColors != on) { m_componentColors = on; emit displayOptionsChanged(); assemble(); }
}

void BRepRenderer::setMeshDisplay(const QString &mode) {
    if (m_meshDisplay != mode) { m_meshDisplay = mode; emit displayOptionsChanged(); assemble(); }
}

void BRepRenderer::setOriginItems(const QStringList &items) {
    if (m_originItems == items) return;
    m_originItems = items;
    emit displayOptionsChanged();
    tessellate();
}

void BRepRenderer::setAnalysis(const QString &a) {
    if (m_analysis == a) return;
    const bool curvature = a == "curvature" || m_analysis == "curvature";
    m_analysis = a;
    emit displayOptionsChanged();
    if (curvature) tessellate();   // curvature is sampled while meshing
    else update();
}

void BRepRenderer::setPullDirection(const QVector3D &d) {
    if (m_pull == d || d.isNull()) return;
    m_pull = d;
    emit displayOptionsChanged();
    update();
}

void BRepRenderer::setDraftAngle(double deg) {
    if (qFuzzyCompare(m_draftAngle, deg)) return;
    m_draftAngle = deg;
    emit displayOptionsChanged();
    update();
}

void BRepRenderer::setSectionEnabled(bool on) {
    if (m_sectionEnabled == on) return;
    m_sectionEnabled = on;
    emit displayOptionsChanged();
    assemble();   // the cap pass needs the body triangles on their own
}

void BRepRenderer::setSectionNormal(const QVector3D &n) {
    if (m_sectionNormal == n || n.isNull()) return;
    m_sectionNormal = n;
    emit displayOptionsChanged();
    update();
}

void BRepRenderer::setSectionOffset(double d) {
    if (qFuzzyCompare(m_sectionOffset + 1.0, d + 1.0)) return;
    m_sectionOffset = d;
    emit displayOptionsChanged();
    update();
}

void BRepRenderer::setBackgroundTop(const QColor &c) {
    if (!c.isValid() || m_bgTop == c) return;
    m_bgTop = c;
    emit backgroundChanged();
    update();
}

void BRepRenderer::setBackgroundBottom(const QColor &c) {
    if (!c.isValid() || m_bgBottom == c) return;
    m_bgBottom = c;
    emit backgroundChanged();
    update();
}

QVariantList BRepRenderer::bounds() const {
    if (!m_hasBounds) return {};
    return {m_boundsMin.x(), m_boundsMin.y(), m_boundsMin.z(), m_boundsMax.x(), m_boundsMax.y(), m_boundsMax.z()};
}

// --- Tessellation (GUI thread) ----------------------------------------------

void BRepRenderer::addFace(const TopoDS_Shape &shape, const PickTag &tag, const QColor &color, float alpha,
                           double deflection) {
    const TopoDS_Face face = TopoDS::Face(shape);
    TopLoc_Location loc;
    Handle(Poly_Triangulation) tri = BRep_Tool::Triangulation(face, loc);
    if (tri.IsNull()) {
        BRepMesh_IncrementalMesh(face, deflection, Standard_False, 0.3);
        tri = BRep_Tool::Triangulation(face, loc);
        if (tri.IsNull()) return;
    }
    const gp_Trsf trsf = loc.Transformation();
    TopLoc_Location surfLoc;
    Handle(Geom_Surface) surf = BRep_Tool::Surface(face, surfLoc);
    const bool reversed = face.Orientation() == TopAbs_REVERSED;

    FaceMesh m;
    m.tag = tag;
    m.color = color;
    m.alpha = alpha;
    if (surf.IsNull()) {
        // Mesh body: flat shaded, each triangle with its own normal, so the facets read clearly
        m.mesh = true;
        m.pos.reserve(tri->NbTriangles() * 3);
        m.nrm.reserve(tri->NbTriangles() * 3);
        m.tris.reserve(tri->NbTriangles() * 3);
        for (int t = 1; t <= tri->NbTriangles(); ++t) {
            int a, b, c;
            tri->Triangle(t).Get(a, b, c);
            if (reversed) std::swap(b, c);
            const QVector3D pa = toQt(tri->Node(a).Transformed(trsf)), pb = toQt(tri->Node(b).Transformed(trsf)),
                            pc = toQt(tri->Node(c).Transformed(trsf));
            const QVector3D fn = QVector3D::normal(pa, pb, pc);
            for (const QVector3D &p : {pa, pb, pc}) {
                m.tris << int(m.pos.size());
                m.pos << p;
                m.nrm << fn;
            }
        }
        m_faces.append(m);
        return;
    }
    const int n = tri->NbNodes();
    m.pos.resize(n);
    m.nrm.fill(QVector3D(), n);
    const bool curvature = m_analysis == "curvature" && tag.kind == PickTag::BodyFace;
    if (curvature) m.curv.fill(0.0f, n);
    QVector<bool> hasNormal(n, false);
    for (int i = 1; i <= n; ++i) {
        m.pos[i - 1] = toQt(tri->Node(i).Transformed(trsf));
        if (tri->HasUVNodes() && !surf.IsNull()) {
            const gp_Pnt2d uv = tri->UVNode(i);
            GeomLProp_SLProps props(surf, uv.X(), uv.Y(), curvature ? 2 : 1, 1e-7);
            if (curvature && props.IsCurvatureDefined())
                m.curv[i - 1] = float(std::max(std::fabs(props.MaxCurvature()), std::fabs(props.MinCurvature())));
            if (props.IsNormalDefined()) {
                gp_Dir nd = props.Normal();
                nd.Transform(surfLoc.Transformation());
                if (reversed) nd.Reverse();
                m.nrm[i - 1] = QVector3D(float(nd.X()), float(nd.Y()), float(nd.Z()));
                hasNormal[i - 1] = true;
            }
        }
    }
    m.tris.reserve(tri->NbTriangles() * 3);
    for (int t = 1; t <= tri->NbTriangles(); ++t) {
        int a, b, c;
        tri->Triangle(t).Get(a, b, c);
        if (reversed) std::swap(b, c);
        m.tris << a - 1 << b - 1 << c - 1;
    }
    // Fall back to averaged triangle normals where the surface normal is undefined (apexes, poles)
    for (int t = 0; t + 2 < m.tris.size(); t += 3) {
        const QVector3D fn = QVector3D::normal(m.pos[m.tris[t]], m.pos[m.tris[t + 1]], m.pos[m.tris[t + 2]]);
        for (int k = 0; k < 3; ++k)
            if (!hasNormal[m.tris[t + k]]) m.nrm[m.tris[t + k]] += fn;
    }
    for (int i = 0; i < n; ++i)
        if (!hasNormal[i]) m.nrm[i].normalize();
    m_faces.append(m);
}

void BRepRenderer::addEdge(const TopoDS_Shape &shape, const PickTag &tag, const QColor &color, bool overlay,
                           double deflection) {
    const TopoDS_Edge edge = TopoDS::Edge(shape);
    if (BRep_Tool::Degenerated(edge)) return;
    try {
        BRepAdaptor_Curve curve(edge);
        if (curve.GetType() == GeomAbs_Line) {
            Polyline pl;
            pl.tag = tag;
            pl.color = color;
            pl.overlay = overlay;
            pl.pts = {toQt(curve.Value(curve.FirstParameter())), toQt(curve.Value(curve.LastParameter()))};
            m_polylines.append(pl);
            return;
        }
        GCPnts_TangentialDeflection disc(curve, 0.12, deflection * 0.5);
        if (disc.NbPoints() < 2) return;
        Polyline pl;
        pl.tag = tag;
        pl.color = color;
        pl.overlay = overlay;
        for (int i = 1; i <= disc.NbPoints(); ++i) pl.pts.append(toQt(disc.Value(i)));
        m_polylines.append(pl);
    } catch (const Standard_Failure &) {
    }
}

// Every triangle edge of a mesh body once, drawn as the body's edges
void BRepRenderer::addMeshWire(const TopoDS_Shape &shape, const PickTag &tag) {
    for (TopExp_Explorer e(shape, TopAbs_FACE); e.More(); e.Next()) {
        TopLoc_Location loc;
        const Handle(Poly_Triangulation) tri = BRep_Tool::Triangulation(TopoDS::Face(e.Current()), loc);
        if (tri.IsNull()) continue;
        const gp_Trsf trsf = loc.Transformation();
        QHash<quint64, int> uses;   // triangles per edge: 1 = border of this face group
        uses.reserve(tri->NbTriangles() * 2);
        Polyline pl, border;
        pl.tag = border.tag = tag;
        pl.color = kMeshWireColor;
        border.color = kEdgeColor;
        pl.segments = border.segments = true;
        pl.mesh = Polyline::Wire;
        border.mesh = Polyline::GroupBorder;
        pl.pts.reserve(tri->NbTriangles() * 3);
        for (int t = 1; t <= tri->NbTriangles(); ++t) {
            int v[3];
            tri->Triangle(t).Get(v[0], v[1], v[2]);
            for (int k = 0; k < 3; ++k) {
                const int a = std::min(v[k], v[(k + 1) % 3]), b = std::max(v[k], v[(k + 1) % 3]);
                if (a == b) continue;
                const quint64 key = (quint64(quint32(a)) << 32) | quint32(b);
                if (uses[key]++) continue;
                pl.pts << toQt(tri->Node(a).Transformed(trsf)) << toQt(tri->Node(b).Transformed(trsf));
            }
        }
        for (auto it = uses.cbegin(); it != uses.cend(); ++it)
            if (it.value() == 1) {
                const int a = int(it.key() >> 32), b = int(it.key() & 0xFFFFFFFF);
                border.pts << toQt(tri->Node(a).Transformed(trsf)) << toQt(tri->Node(b).Transformed(trsf));
            }
        m_polylines.append(pl);
        if (!border.pts.isEmpty()) m_polylines.append(border);
    }
}

void BRepRenderer::tessellate() {
    QElapsedTimer timer;
    timer.start();
    m_faces.clear();
    m_polylines.clear();
    m_points.clear();
    m_overlayIds.clear();
    m_bodyBoxes.clear();
    m_hasBounds = false;

    if (m_doc) {
        Bnd_Box all;
        const auto bodies = m_doc->bodies();
        const auto overlays = m_doc->overlayFeatures();
        const bool input = m_doc->showingInput();
        auto bodyVisible = [&](int i) {
            FissionBody *b = m_doc->body(i);
            return b ? b->isVisible() : true;
        };
        QHash<const void *, QPair<TopoDS_Shape, Bnd_Box>> boxes;
        for (int i = 0; i < m_doc->displayBodyCount(); ++i) {
            if (!bodyVisible(i)) continue;
            const TopoDS_Shape shape = m_doc->displayShape(i);
            const void *key = shape.TShape().get();
            auto hit = m_boxCache.constFind(key);
            const Bnd_Box box = hit != m_boxCache.constEnd() && hit->first.IsEqual(shape) ? hit->second
                                                                                          : GeomUtils::boundingBox(shape);
            boxes.insert(key, {shape, box});
            all.Add(box);
            if (box.IsVoid()) continue;
            double x0, y0, z0, x1, y1, z1;
            box.Get(x0, y0, z0, x1, y1, z1);
            m_bodyBoxes.append({QVector3D(float(x0), float(y0), float(z0)), QVector3D(float(x1), float(y1), float(z1))});
        }
        m_boxCache = boxes;
        for (auto *f : overlays) all.Add(GeomUtils::boundingBox(f->overlayShape()));
        const double diag = all.IsVoid() ? 100.0 : std::max(1.0, std::sqrt(all.SquareExtent()));
        const double defl = std::max(0.005, diag * 0.0008);
        m_deflection = defl;
        auto mesh = [&](const TopoDS_Shape &s) {
            if (MeshUtils::isMesh(s)) return true;   // already triangles only
            // Solids converted from meshes carry their triangles; re-meshing tens of thousands of faces takes long
            int nFaces = 0;
            bool allMeshed = true;
            for (TopExp_Explorer e(s, TopAbs_FACE); e.More() && allMeshed; e.Next(), ++nFaces) {
                TopLoc_Location l;
                allMeshed = !BRep_Tool::Triangulation(TopoDS::Face(e.Current()), l).IsNull();
            }
            if (allMeshed && nFaces > 500) return true;
            try {
                BRepMesh_IncrementalMesh(s, defl, Standard_False, 0.3, Standard_True);
                return true;
            } catch (const Standard_Failure &) {
                return false;
            }
        };

        // While a feature is edited its result is drawn as the shaded solid (not pickable) and the
        // input bodies stay pickable underneath: their faces are only drawn when hovered or selected
        if (input) {
            for (int i = 0; i < bodies.size(); ++i) {
                FissionBody *b = bodies[i];
                if (!b->isVisible() || b->shape().IsNull() || !mesh(b->shape())) continue;
                const TopTools_IndexedMapOfShape faces = GeomUtils::faceMap(b->shape());
                for (int j = 1; j <= faces.Extent(); ++j) {
                    addFace(faces(j), {PickTag::Preview, i, j - 1}, b->color(), 1.0f, defl);
                    if (!m_faces.isEmpty()) {
                        m_faces.last().roughness = float(b->roughness());
                        m_faces.last().metalness = float(b->metalness());
                    }
                }
                const TopTools_IndexedMapOfShape edges = GeomUtils::edgeMap(b->shape());
                for (int k = 1; k <= edges.Extent(); ++k)
                    addEdge(edges(k), {PickTag::Preview, i, k - 1}, kEdgeColor, false, defl);
            }
        }
        // Unchanged bodies reuse their display geometry: rebuilding it for a big body takes seconds
        const bool curvature = m_analysis == "curvature";
        QSet<const void *> usedKeys;
        for (int i = 0; i < m_doc->displayBodyCount(); ++i) {
            const TopoDS_Shape shape = m_doc->displayShape(i);
            if (!bodyVisible(i) || shape.IsNull()) continue;
            FissionBody *b = m_doc->body(i);
            const QColor color = b ? b->color() : kDefaultBodyColor;
            const float alpha = input ? 0.0f : b ? float(b->opacity()) : 1.0f;
            const float roughness = b ? float(b->roughness()) : 0.55f;
            const float metalness = b ? float(b->metalness()) : 0.0f;
            auto applyLook = [&](FaceMesh &fm) {
                fm.tag.owner = i;
                if (b && fm.tag.kind == PickTag::BodyFace) {
                    const auto look = b->effectiveLook(fm.tag.index);
                    fm.color = look.color;
                    fm.alpha = input ? 0.0f : float(look.opacity);
                    fm.roughness = float(look.roughness);
                    fm.metalness = float(look.metalness);
                } else {
                    fm.color = color;
                    fm.alpha = alpha;
                    fm.roughness = roughness;
                    fm.metalness = metalness;
                }
            };
            const void *key = shape.TShape().get();
            usedKeys.insert(key);
            auto it = m_bodyCache.find(key);
            if (it != m_bodyCache.end() && it->shape.IsEqual(shape) && it->curvature == curvature && it->input == input) {
                for (FaceMesh fm : std::as_const(it->faces)) {
                    applyLook(fm);
                    m_faces.append(fm);
                }
                for (Polyline pl : std::as_const(it->lines)) {
                    pl.tag.owner = i;
                    m_polylines.append(pl);
                }
                continue;
            }
            if (!mesh(shape)) continue;
            const int f0 = m_faces.size(), p0 = m_polylines.size();
            const TopTools_IndexedMapOfShape faces = GeomUtils::faceMap(shape);
            for (int j = 1; j <= faces.Extent(); ++j) {
                QColor fc = color;
                float fa = alpha, fr = roughness, fmtal = metalness;
                if (b) {
                    const auto look = b->effectiveLook(j - 1);
                    fc = look.color;
                    fa = input ? 0.0f : float(look.opacity);
                    fr = float(look.roughness);
                    fmtal = float(look.metalness);
                }
                addFace(faces(j), {PickTag::BodyFace, i, j - 1}, fc, fa, defl);
                if (!m_faces.isEmpty()) {
                    m_faces.last().roughness = fr;
                    m_faces.last().metalness = fmtal;
                }
            }
            if (MeshUtils::isMesh(shape)) {
                if (!input) addMeshWire(shape, {PickTag::Preview, i, -1});
            } else {
                const TopTools_IndexedMapOfShape edges = GeomUtils::edgeMap(shape);
                for (int k = 1; k <= edges.Extent(); ++k)
                    addEdge(edges(k), {PickTag::BodyEdge, i, k - 1}, kEdgeColor, false, defl);
            }
            for (int k = f0; k < m_faces.size(); ++k) buildChunks(m_faces[k]);
            for (int k = p0; k < m_polylines.size(); ++k) buildChunks(m_polylines[k]);
            m_bodyCache.insert(key, {shape, curvature, input, m_faces.mid(f0), m_polylines.mid(p0)});
        }
        for (auto it = m_bodyCache.begin(); it != m_bodyCache.end();)
            it = usedKeys.contains(it.key()) ? std::next(it) : m_bodyCache.erase(it);

        for (FissionFeature *f : overlays) {
            const int oi = m_overlayIds.size();
            m_overlayIds.append(f->id());
            if (auto *sk = qobject_cast<FissionSketch *>(f)) {
                const auto &ents = sk->entityEdges();
                for (int n = 0; n < ents.size(); ++n) {
                    const QVariantMap ent = sk->entity(n);
                    const QColor col = ent.value("construction").toBool() ? kSketchConstructionColor
                                     : ent.value("projected").toBool()    ? kSketchProjectedColor
                                     : sk->entityFullyConstrained(n)      ? kSketchFixedColor
                                                                          : kSketchColor;
                    for (const auto &e : ents[n]) addEdge(e, {PickTag::SketchCurve, oi, n}, col, true, defl);
                }
                for (TopExp_Explorer exp(sk->overlayShape(), TopAbs_VERTEX, TopAbs_EDGE); exp.More(); exp.Next())
                    m_points.append(toQt(BRep_Tool::Pnt(TopoDS::Vertex(exp.Current()))));
                for (int p = 0; p < sk->profileCount(); ++p)
                    addFace(sk->profiles()[p], {PickTag::Profile, oi, p}, kProfileColor, 0.22f, defl);
            } else if (qobject_cast<FissionConstructionPlane *>(f)) {
                const TopoDS_Shape s = f->overlayShape();
                for (TopExp_Explorer exp(s, TopAbs_FACE); exp.More(); exp.Next())
                    addFace(exp.Current(), {PickTag::Plane, oi, 0}, kConstructionColor, 0.16f, defl);
                for (TopExp_Explorer exp(s, TopAbs_EDGE); exp.More(); exp.Next())
                    addEdge(exp.Current(), {PickTag::Plane, oi, 0}, kConstructionColor, true, defl);
            } else if (qobject_cast<FissionConstructionAxis *>(f)) {
                for (TopExp_Explorer exp(f->overlayShape(), TopAbs_EDGE); exp.More(); exp.Next())
                    addEdge(exp.Current(), {PickTag::Axis, oi, 0}, kConstructionColor, true, defl);
            } else if (auto *cp = qobject_cast<FissionConstructionPoint *>(f)) {
                m_points.append(toQt(cp->point()));
            }
        }

        // Origin: planes as squares in the positive quadrants, axes from the origin, the origin point
        const double S = all.IsVoid() ? 30.0 : std::max(10.0, 0.3 * diag);
        for (int k = 0; k < 3; ++k) {
            if (!m_originItems.contains(kOriginPlanes[k])) continue;
            const gp_Pnt o(0, 0, 0);
            const gp_Pnt a = k == 2 ? gp_Pnt(0, S, 0) : gp_Pnt(S, 0, 0);
            const gp_Pnt b = k == 0 ? gp_Pnt(0, S, 0) : gp_Pnt(0, 0, S);
            const gp_Pnt ab(a.X() + b.X(), a.Y() + b.Y(), a.Z() + b.Z());
            BRepBuilderAPI_MakePolygon poly(o, a, ab, b, Standard_True);
            const TopoDS_Face face = BRepBuilderAPI_MakeFace(poly.Wire(), Standard_True).Face();
            if (!mesh(face)) continue;
            addFace(face, {PickTag::OriginPlane, -1, k}, kConstructionColor, 0.22f, defl);
            for (TopExp_Explorer exp(face, TopAbs_EDGE); exp.More(); exp.Next())
                addEdge(exp.Current(), {PickTag::OriginPlane, -1, k}, kConstructionColor, true, defl);
        }
        for (int k = 0; k < 3; ++k) {
            if (!m_originItems.contains(kOriginAxes[k])) continue;
            const gp_Pnt end(k == 0 ? 1.4 * S : 0, k == 1 ? 1.4 * S : 0, k == 2 ? 1.4 * S : 0);
            addEdge(BRepBuilderAPI_MakeEdge(gp_Pnt(0, 0, 0), end).Edge(), {PickTag::OriginAxis, -1, k}, kAxisColors[k],
                    true, defl);
        }
        if (m_originItems.contains("O")) {
            Polyline pl;
            pl.tag = {PickTag::OriginPoint, -1, 0};
            pl.color = Qt::white;
            pl.overlay = true;
            pl.pts = {QVector3D(0, 0, 0), QVector3D(0, 0, 0)};
            m_polylines.append(pl);
            m_points.append(QVector3D(0, 0, 0));
        }

        if (!all.IsVoid()) {
            double x0, y0, z0, x1, y1, z1;
            all.Get(x0, y0, z0, x1, y1, z1);
            m_boundsMin = QVector3D(float(x0), float(y0), float(z0));
            m_boundsMax = QVector3D(float(x1), float(y1), float(z1));
            m_hasBounds = true;
        }
    }
    for (FaceMesh &m : m_faces)
        if (m.chunks.isEmpty()) buildChunks(m);
    for (Polyline &pl : m_polylines)
        if (pl.chunks.isEmpty()) buildChunks(pl);
    if (timer.elapsed() > 50) qInfo("viewport: tessellating took %lld ms", timer.elapsed());
    assemble();
}

void BRepRenderer::buildChunks(FaceMesh &m) {
    constexpr int kTris = 128;
    m.chunks.clear();
    const int n = int(m.tris.size()) / 3;
    if (n > kTris) {
        // Triangles in file order (STL) are scattered; Morton order keeps each chunk's box small
        QVector3D lo = m.pos[m.tris[0]], hi = lo;
        for (const QVector3D &p : std::as_const(m.pos)) {
            lo = QVector3D(std::min(lo.x(), p.x()), std::min(lo.y(), p.y()), std::min(lo.z(), p.z()));
            hi = QVector3D(std::max(hi.x(), p.x()), std::max(hi.y(), p.y()), std::max(hi.z(), p.z()));
        }
        const QVector3D span = hi - lo;
        auto spread = [](quint32 v) {   // 10 bits -> every third bit
            v = (v | (v << 16)) & 0x030000FF;
            v = (v | (v << 8)) & 0x0300F00F;
            v = (v | (v << 4)) & 0x030C30C3;
            return (v | (v << 2)) & 0x09249249;
        };
        auto cell = [](float v, float lo, float span) {
            return quint32(std::clamp(span > 0 ? (v - lo) / span * 1023.0f : 0.0f, 0.0f, 1023.0f));
        };
        std::vector<std::pair<quint32, int>> order(n);
        for (int t = 0; t < n; ++t) {
            const QVector3D c = (m.pos[m.tris[3 * t]] + m.pos[m.tris[3 * t + 1]] + m.pos[m.tris[3 * t + 2]]) / 3.0f;
            order[t] = {spread(cell(c.x(), lo.x(), span.x())) | (spread(cell(c.y(), lo.y(), span.y())) << 1)
                            | (spread(cell(c.z(), lo.z(), span.z())) << 2),
                        t};
        }
        std::sort(order.begin(), order.end());
        QVector<int> sorted(m.tris.size());
        for (int t = 0; t < n; ++t)
            for (int k = 0; k < 3; ++k) sorted[3 * t + k] = m.tris[3 * order[t].second + k];
        m.tris = sorted;
    }
    for (int b = 0; b + 2 < m.tris.size(); b += 3 * kTris) {
        Chunk c;
        c.begin = b;
        c.end = std::min<int>(m.tris.size(), b + 3 * kTris);
        c.lo = c.hi = m.pos[m.tris[b]];
        for (int i = b; i < c.end; ++i) {
            const QVector3D &p = m.pos[m.tris[i]];
            c.lo = QVector3D(std::min(c.lo.x(), p.x()), std::min(c.lo.y(), p.y()), std::min(c.lo.z(), p.z()));
            c.hi = QVector3D(std::max(c.hi.x(), p.x()), std::max(c.hi.y(), p.y()), std::max(c.hi.z(), p.z()));
        }
        m.chunks.append(c);
    }
}

void BRepRenderer::buildChunks(Polyline &pl) {
    constexpr int kPts = 64;   // even, so segment pairs never straddle two chunks
    pl.chunks.clear();
    for (int b = 0; b < pl.pts.size(); b += kPts) {
        Chunk c;
        c.begin = b;
        // A strip's chunk also holds the first point of the next one, so every segment is in one chunk
        c.end = std::min<int>(pl.pts.size(), b + kPts + (pl.segments ? 0 : 1));
        c.lo = c.hi = pl.pts[b];
        for (int i = b; i < c.end; ++i) {
            const QVector3D &p = pl.pts[i];
            c.lo = QVector3D(std::min(c.lo.x(), p.x()), std::min(c.lo.y(), p.y()), std::min(c.lo.z(), p.z()));
            c.hi = QVector3D(std::max(c.hi.x(), p.x()), std::max(c.hi.y(), p.y()), std::max(c.hi.z(), p.z()));
        }
        pl.chunks.append(c);
    }
}

int BRepRenderer::sectionCapCount() const {
    if (!m_sectionEnabled || (m_doc && m_doc->showingInput())) return 0;
    const QVector3D n = m_sectionNormal.normalized();
    int count = 0;
    for (const auto &box : m_bodyBoxes) {
        float lo = 0, hi = 0;
        for (int k = 0; k < 8; ++k) {
            const QVector3D c((k & 1) ? box.second.x() : box.first.x(), (k & 2) ? box.second.y() : box.first.y(),
                              (k & 4) ? box.second.z() : box.first.z());
            const float d = QVector3D::dotProduct(c, n) - float(m_sectionOffset);
            lo = k ? std::min(lo, d) : d;
            hi = k ? std::max(hi, d) : d;
        }
        if (lo < 0 && hi > 0) ++count;
    }
    return count;
}

// --- Highlighting and GPU buffers ------------------------------------------

QString BRepRenderer::tagKey(const PickTag &t) const {
    const QString owner = (t.kind == PickTag::BodyFace || t.kind == PickTag::BodyEdge)
                              ? QString::number(t.owner) : m_overlayIds.value(t.owner);
    switch (t.kind) {
        case PickTag::BodyFace: return QString("face|%1|%2").arg(owner).arg(t.index);
        case PickTag::BodyEdge: return QString("edge|%1|%2").arg(owner).arg(t.index);
        case PickTag::Profile: return QString("profile|%1|%2").arg(owner).arg(t.index);
        case PickTag::Plane: return QString("plane|%1").arg(owner);
        case PickTag::SketchCurve: return QString("sketchcurve|%1|%2").arg(owner).arg(t.index);
        case PickTag::Axis: return QString("axis|%1").arg(owner);
        case PickTag::Point: return QString("point|%1").arg(owner);
        case PickTag::Preview: return {};
        case PickTag::OriginPoint: return QStringLiteral("originpoint|O");
        case PickTag::OriginAxis: return QString("originaxis|%1").arg(kOriginAxes[t.index]);
        case PickTag::OriginPlane: return QString("originplane|%1").arg(kOriginPlanes[t.index]);
    }
    return {};
}

QString BRepRenderer::refKey(const QVariantMap &r) const {
    const QString kind = r.value("kind").toString();
    if (kind == "face" || kind == "edge") return QString("%1|%2|%3").arg(kind).arg(r["body"].toInt()).arg(r["index"].toInt());
    if (kind == "profile") return QString("profile|%1|%2").arg(r["sketch"].toString()).arg(r["index"].toInt());
    if (kind == "sketchcurve") return QString("sketchcurve|%1|%2").arg(r["sketch"].toString()).arg(r["entity"].toInt());
    if (kind == "plane" || kind == "axis" || kind == "point" || kind == "originplane" || kind == "originaxis"
        || kind == "originpoint")
        return QString("%1|%2").arg(kind, r["id"].toString());
    if (kind == "body") return QString("body|%1").arg(r["body"].toInt());
    return {};
}

quint64 BRepRenderer::tagCode(const PickTag &t) {
    if (t.kind == PickTag::Preview) return 0;
    const bool noIndex = t.kind == PickTag::Plane || t.kind == PickTag::Axis || t.kind == PickTag::Point
                         || t.kind == PickTag::OriginPoint;
    return (quint64(t.kind + 1) << 56) | (quint64(quint32(t.owner + 1) & 0xFFFFFFFu) << 28)
           | quint64(quint32((noIndex ? 0 : t.index) + 1) & 0xFFFFFFFu);
}

void BRepRenderer::refCode(const QVariantMap &r, quint64 &code, int &body) const {
    code = 0;
    body = -1;
    const QString kind = r.value("kind").toString();
    const int overlay = m_overlayIds.indexOf(r.value(kind == "profile" || kind == "sketchcurve" ? "sketch" : "id").toString());
    auto originIndex = [&](const char *const names[3]) {
        for (int k = 0; k < 3; ++k)
            if (r.value("id").toString() == names[k]) return k;
        return -1;
    };
    PickTag t{PickTag::Preview, -1, 0};
    if (kind == "body") { body = r.value("body", -1).toInt(); return; }
    if (kind == "face") t = {PickTag::BodyFace, r.value("body", -1).toInt(), r.value("index", -1).toInt()};
    else if (kind == "edge") t = {PickTag::BodyEdge, r.value("body", -1).toInt(), r.value("index", -1).toInt()};
    else if (kind == "profile" && overlay >= 0) t = {PickTag::Profile, overlay, r.value("index", -1).toInt()};
    else if (kind == "sketchcurve" && overlay >= 0) t = {PickTag::SketchCurve, overlay, r.value("entity", -1).toInt()};
    else if (kind == "plane" && overlay >= 0) t = {PickTag::Plane, overlay, 0};
    else if (kind == "axis" && overlay >= 0) t = {PickTag::Axis, overlay, 0};
    else if (kind == "point" && overlay >= 0) t = {PickTag::Point, overlay, 0};
    else if (kind == "originpoint") t = {PickTag::OriginPoint, -1, 0};
    else if (kind == "originaxis") t = {PickTag::OriginAxis, -1, originIndex(kOriginAxes)};
    else if (kind == "originplane") t = {PickTag::OriginPlane, -1, originIndex(kOriginPlanes)};
    else return;
    code = tagCode(t);
}

void BRepRenderer::assembleHover() {
    auto data = std::make_shared<HoverData>();
    quint64 code = 0;
    int body = -1;
    refCode(m_hovered, code, body);
    QSet<quint64> selected;
    QSet<int> selectedBodies;
    if (m_selection)
        for (const QVariant &v : m_selection->items()) {
            quint64 c; int b;
            refCode(v.toMap(), c, b);
            if (c) selected.insert(c);
            if (b >= 0) selectedBodies.insert(b);
        }
    auto hovered = [&](const PickTag &t) {
        const quint64 c = tagCode(t);
        if (!c) return false;
        const bool bodyTag = t.kind == PickTag::BodyFace || t.kind == PickTag::BodyEdge;
        if (selected.contains(c) || (bodyTag && selectedBodies.contains(t.owner))) return false;   // shown as selected
        return c == code || (bodyTag && body >= 0 && t.owner == body);
    };
    if (code || body >= 0) {
        for (const FaceMesh &m : m_faces) {
            if (!hovered(m.tag)) continue;
            const QColor c = m.alpha < 1 ? kHoverColor : m.color.lighter(118);
            const float alpha = m.alpha < 1 ? 0.35f : 1.0f;
            QVector<float> &out = alpha < 1.0f ? data->transparent : data->triangles;
            for (int idx : m.tris) {
                const QVector3D &p = m.pos[idx], &n = m.nrm[idx];
                out << p.x() << p.y() << p.z() << n.x() << n.y() << n.z();
                pushPbr(out, c, alpha, m.roughness, m.metalness);
            }
        }
        for (const Polyline &pl : m_polylines) {
            if (!hovered(pl.tag)) continue;
            const bool onTop = pl.overlay || (m_doc && m_doc->showingInput() && pl.tag.kind == PickTag::BodyEdge);
            QVector<float> &out = onTop ? data->overlayLines : data->lines;
            for (int i = 0; i + 1 < pl.pts.size(); i += pl.segments ? 2 : 1)
                for (const QVector3D &p : {pl.pts[i], pl.pts[i + 1]}) {
                    out << p.x() << p.y() << p.z();
                    pushColor(out, kHoverColor, 1.0f);
                }
        }
    }
    m_hoverData = data;
    ++m_hoverVersion;
    update();
}

void BRepRenderer::assemble() {
    QElapsedTimer timer;
    timer.start();
    auto data = std::make_shared<RenderData>();

    QSet<quint64> selected;
    QSet<int> selectedBodies;
    if (m_selection)
        for (const QVariant &v : m_selection->items()) {
            quint64 c; int b;
            refCode(v.toMap(), c, b);
            if (c) selected.insert(c);
            if (b >= 0) selectedBodies.insert(b);
        }
    const double modelDiag = m_hasBounds ? std::max(1.0f, (m_boundsMax - m_boundsMin).length()) : 100.0;

    auto state = [&](const PickTag &tag, bool &sel, bool &hov) {
        hov = false;
        const quint64 c = tagCode(tag);
        const bool bodyTag = tag.kind == PickTag::BodyFace || tag.kind == PickTag::BodyEdge;
        sel = (c && selected.contains(c)) || (bodyTag && selectedBodies.contains(tag.owner));
    };

    const bool meshWireframe = m_meshDisplay == "wireframe", meshGroups = m_meshDisplay == "faceGroups";
    struct PbrMaps { QImage albedo, rough, metal, normal; };
    QHash<int, PbrMaps> bodyMaps;
    QHash<QPair<int, int>, PbrMaps> faceMaps;
    auto loadMap = [](const QString &path) -> QImage {
        if (path.isEmpty() || !QFileInfo::exists(path)) return {};
        return QImage(path).convertToFormat(QImage::Format_RGB888);
    };
    if (m_doc) {
        for (int i = 0; i < m_doc->bodyCount(); ++i) {
            FissionBody *b = m_doc->body(i);
            if (!b) continue;
            PbrMaps maps;
            maps.albedo = loadMap(b->texturePath());
            maps.rough = loadMap(b->roughnessPath());
            maps.metal = loadMap(b->metalnessPath());
            maps.normal = loadMap(b->normalPath());
            if (!maps.albedo.isNull() || !maps.rough.isNull() || !maps.metal.isNull() || !maps.normal.isNull())
                bodyMaps.insert(i, maps);
            const QVariantMap faces = b->faceAppearancesMap();
            for (auto it = faces.cbegin(); it != faces.cend(); ++it) {
                bool ok = false;
                const int fi = it.key().toInt(&ok);
                if (!ok) continue;
                const QVariantMap ov = it.value().toMap();
                PbrMaps fm;
                fm.albedo = loadMap(ov.value("texturePath").toString());
                fm.rough = loadMap(ov.value("roughnessPath").toString());
                fm.metal = loadMap(ov.value("metalnessPath").toString());
                fm.normal = loadMap(ov.value("normalPath").toString());
                if (!fm.albedo.isNull() || !fm.rough.isNull() || !fm.metal.isNull() || !fm.normal.isNull())
                    faceMaps.insert({i, fi}, fm);
            }
        }
    }
    auto perturbNormal = [](const QVector3D &n, const QColor &nSample) {
        // OpenGL normal map in tangent-ish world blend: remap [0,1]→[-1,1] and mix
        QVector3D tn(nSample.redF() * 2.f - 1.f, nSample.greenF() * 2.f - 1.f, nSample.blueF() * 2.f - 1.f);
        if (tn.lengthSquared() < 1e-6f) return n;
        tn.normalize();
        return (n + tn * 0.55f).normalized();
    };
    for (const FaceMesh &m : m_faces) {
        bool sel = false, hov = false;
        state(m.tag, sel, hov);
        if (m.alpha <= 0.0f && !sel) continue;   // pick-only input face
        if (m.mesh && meshWireframe && !sel) continue;
        QColor c = m.color;
        float alpha = m.alpha;
        if (m.mesh && meshGroups && m.tag.kind == PickTag::BodyFace) {
            // Well separated hues per group (golden-ratio steps), muted so the shading still reads
            const double hue = std::fmod(0.08 + std::max(0, m.tag.index) * 0.618033988749895, 1.0);
            c = QColor::fromHsvF(float(hue), 0.45f, 0.86f);
        }
        const bool cycled = m_componentColors && (m.tag.kind == PickTag::BodyFace || m.tag.kind == PickTag::Preview);
        if (cycled) {
            const double hue = std::fmod(0.13 + std::max(0, m.tag.owner) * 0.618033988749895, 1.0);
            c = QColor::fromHsvF(float(hue), 0.55f, 0.9f);
        }
        if (sel) { c = m.alpha < 1 ? kSelectColor : QColor::fromRgbF(0.45f * c.redF() + 0.55f * kSelectColor.redF(),
                                                                      0.45f * c.greenF() + 0.55f * kSelectColor.greenF(),
                                                                      0.45f * c.blueF() + 0.55f * kSelectColor.blueF());
                   if (alpha < 1) alpha = 0.45f; }
        QVector<float> &out = alpha < 1.0f ? data->transparent : data->triangles;
        const bool curvature = !m.curv.isEmpty() && !sel;
        PbrMaps maps;
        if (m.tag.kind == PickTag::BodyFace && !cycled) {
            const auto faceIt = faceMaps.constFind({m.tag.owner, m.tag.index});
            if (faceIt != faceMaps.cend()) maps = faceIt.value();
            else {
                const auto bodyIt = bodyMaps.constFind(m.tag.owner);
                if (bodyIt != bodyMaps.cend()) maps = bodyIt.value();
            }
        }
        for (int idx : m.tris) {
            QVector3D p = m.pos[idx], n = m.nrm[idx];
            QColor col = curvature ? curvatureColor(m.curv[idx], modelDiag) : c;
            float rough = m.roughness, metal = m.metalness;
            if (!curvature && !sel) {
                if (!maps.albedo.isNull()) {
                    const QColor t = sampleTriplanar(maps.albedo, p, n);
                    col = QColor::fromRgbF(col.redF() * t.redF(), col.greenF() * t.greenF(), col.blueF() * t.blueF());
                }
                if (!maps.rough.isNull())
                    rough = std::clamp(float(sampleTriplanar(maps.rough, p, n).redF()), 0.04f, 1.f);
                if (!maps.metal.isNull())
                    metal = std::clamp(float(sampleTriplanar(maps.metal, p, n).redF()), 0.f, 1.f);
                if (!maps.normal.isNull())
                    n = perturbNormal(n, sampleTriplanar(maps.normal, p, n));
            }
            out << p.x() << p.y() << p.z() << n.x() << n.y() << n.z();
            pushPbr(out, col, alpha, rough, metal);
        }
        if (m_sectionEnabled && m.tag.kind == PickTag::BodyFace && m.alpha >= 1.0f)
            for (int idx : m.tris) {
                const QVector3D &p = m.pos[idx];
                data->capSources << p.x() << p.y() << p.z() << 0.f << 0.f << 0.f << 1.f;
            }
    }
    data->boundsMin = m_boundsMin;
    data->boundsMax = m_boundsMax;

    for (const Polyline &pl : m_polylines) {
        bool sel = false, hov = false;
        state(pl.tag, sel, hov);
        if (pl.mesh == Polyline::Wire) {
            // Facet outlines: always in wireframe, never in plain shaded / face groups
            if (!meshWireframe && (m_meshDisplay == "shaded" || meshGroups || !m_showEdges)) continue;
        } else if (pl.mesh == Polyline::GroupBorder) {
            if (!meshGroups) continue;
        } else if ((pl.tag.kind == PickTag::BodyEdge || pl.tag.kind == PickTag::Preview) && !m_showEdges && !sel && !hov) {
            continue;
        }
        const QColor c = sel ? kSelectColor : hov ? kHoverColor : pl.color;
        // Picked edges must stay visible above the feature-preview outline
        const bool onTop = pl.overlay || (m_doc && m_doc->showingInput() && pl.tag.kind == PickTag::BodyEdge && (sel || hov));
        QVector<float> &out = onTop ? data->overlayLines : data->lines;
        for (int i = 0; i + 1 < pl.pts.size(); i += pl.segments ? 2 : 1) {
            for (const QVector3D &p : {pl.pts[i], pl.pts[i + 1]}) {
                out << p.x() << p.y() << p.z();
                pushColor(out, c, 1.0f);
            }
        }
    }

    for (const QVector3D &p : m_points) {
        data->points << p.x() << p.y() << p.z();
        pushColor(data->points, kSketchColor, 1.0f);
    }

    if (m_showGrid) {
        double extent = 100.0;
        if (m_hasBounds)
            for (const QVector3D &v : {m_boundsMin, m_boundsMax})
                extent = std::max({extent, 1.5 * std::fabs(v.x()), 1.5 * std::fabs(v.y())});
        const double step = std::pow(10.0, std::floor(std::log10(extent / 5.0)));
        const int n = int(std::ceil(extent / step));
        const float e = float(n * step);
        auto line = [&](float x0, float y0, float x1, float y1, const QColor &c, float a) {
            data->gridLines << x0 << y0 << 0.f; pushColor(data->gridLines, c, a);
            data->gridLines << x1 << y1 << 0.f; pushColor(data->gridLines, c, a);
        };
        for (int i = -n; i <= n; ++i) {
            if (i == 0) continue;
            const float v = float(i * step);
            const float a = (i % 5 == 0) ? 0.16f : 0.07f;
            line(-e, v, e, v, Qt::white, a);
            line(v, -e, v, e, Qt::white, a);
        }
        line(-e, 0, e, 0, QColor(220, 70, 70), 0.85f);    // X axis
        line(0, -e, 0, e, QColor(90, 200, 90), 0.85f);    // Y axis
        data->gridLines << 0.f << 0.f << 0.f; pushColor(data->gridLines, QColor(80, 130, 230), 0.85f);
        data->gridLines << 0.f << 0.f << float(e * 0.3); pushColor(data->gridLines, QColor(80, 130, 230), 0.85f);
    }

    m_data = data;
    ++m_dataVersion;
    if (timer.elapsed() > 50) qInfo("viewport: assembling %d faces, %d edges took %lld ms", int(m_faces.size()),
                                    int(m_polylines.size()), timer.elapsed());
    assembleHover();
}

// --- Picking ---------------------------------------------------------------

bool BRepRenderer::allows(const QString &filter, const QString &kind) {
    if (filter.isEmpty()) return true;
    for (const QString &k : filter.split(',', Qt::SkipEmptyParts))
        if (k.trimmed() == kind) return true;
    return false;
}

const BRepRenderer::ShapeMaps &BRepRenderer::shapeMaps(const TopoDS_Shape &s) const {
    const void *key = s.TShape().get();
    auto it = m_mapCache.find(key);
    if (it == m_mapCache.end() || !it->shape.IsEqual(s)) {
        if (m_mapCache.size() > 32) m_mapCache.clear();
        it = m_mapCache.insert(key, {s, GeomUtils::faceMap(s), GeomUtils::edgeMap(s)});
    }
    return *it;
}

QVariantMap BRepRenderer::tagRef(const PickTag &t, const QString &filter) const {
    const QString owner = m_overlayIds.value(t.owner);
    switch (t.kind) {
        case PickTag::BodyFace: {
            const TopoDS_Shape s = m_doc ? m_doc->displayShape(t.owner) : TopoDS_Shape();
            if (s.IsNull()) return {};
            if (!allows(filter, "face")) return {{"kind", "body"}, {"body", t.owner}};
            const TopTools_IndexedMapOfShape &faces = shapeMaps(s).faces;
            QVariantMap ref{{"kind", "face"}, {"body", t.owner}, {"index", t.index}, {"count", faces.Extent()}};
            if (t.index >= 0 && t.index < faces.Extent()) {
                const gp_Pnt p = GeomUtils::faceCentroid(TopoDS::Face(faces(t.index + 1)));
                ref["x"] = p.X(); ref["y"] = p.Y(); ref["z"] = p.Z();
            }
            return ref;
        }
        case PickTag::BodyEdge: {
            const TopoDS_Shape s = m_doc ? m_doc->displayShape(t.owner) : TopoDS_Shape();
            if (s.IsNull()) return {};
            const TopTools_IndexedMapOfShape &edges = shapeMaps(s).edges;
            QVariantMap ref{{"kind", "edge"}, {"body", t.owner}, {"index", t.index}, {"count", edges.Extent()}};
            if (t.index >= 0 && t.index < edges.Extent()) {
                const gp_Pnt p = GeomUtils::edgeMidpoint(TopoDS::Edge(edges(t.index + 1)));
                ref["x"] = p.X(); ref["y"] = p.Y(); ref["z"] = p.Z();
            }
            return ref;
        }
        case PickTag::Preview: return {};
        case PickTag::OriginPoint: return {{"kind", "originpoint"}, {"id", "O"}};
        case PickTag::OriginAxis: return {{"kind", "originaxis"}, {"id", kOriginAxes[t.index]}};
        case PickTag::OriginPlane: return {{"kind", "originplane"}, {"id", kOriginPlanes[t.index]}};
        case PickTag::Profile: return {{"kind", "profile"}, {"sketch", owner}, {"index", t.index}};
        case PickTag::Plane: return {{"kind", "plane"}, {"id", owner}};
        case PickTag::SketchCurve: return {{"kind", "sketchcurve"}, {"sketch", owner}, {"entity", t.index}};
        case PickTag::Axis: return {{"kind", "axis"}, {"id", owner}};
        case PickTag::Point: return {{"kind", "point"}, {"id", owner}};
    }
    return {};
}

static QString tagFilterKind(const PickTag &t) {
    switch (t.kind) {
        case PickTag::BodyFace: return "face";
        case PickTag::BodyEdge: return "edge";
        case PickTag::Profile: return "profile";
        case PickTag::Plane: return "plane";
        case PickTag::SketchCurve: return "sketchcurve";
        case PickTag::Axis: return "axis";
        case PickTag::Point: return "point";
        case PickTag::Preview: return {};
        case PickTag::OriginPoint: return "originpoint";
        case PickTag::OriginAxis: return "originaxis";
        case PickTag::OriginPlane: return "originplane";
    }
    return {};
}

QVariantMap BRepRenderer::pick(double x, double y, const QString &filterArg) const {
    QString filter = filterArg.isEmpty() ? (m_selection ? m_selection->filter() : QString()) : filterArg;
    if (filter == "*") filter.clear();
    if (!m_camera || width() < 1 || height() < 1) return {};

    const double w = width(), h = height();
    const QMatrix4x4 mvp = m_camera->projectionMatrix(w / h) * m_camera->viewMatrix();
    const QMatrix4x4 inv = mvp.inverted();
    const float nx = float(2.0 * x / w - 1.0), ny = float(1.0 - 2.0 * y / h);
    const QVector3D origin = (inv * QVector4D(nx, ny, -1, 1)).toVector3DAffine();
    const QVector3D farP = (inv * QVector4D(nx, ny, 1, 1)).toVector3DAffine();
    const QVector3D dir = (farP - origin).normalized();
    const float tol = float(m_camera->distance() * 0.004);

    bool allowedKinds[PickTag::OriginPlane + 1];
    for (int k = 0; k <= PickTag::OriginPlane; ++k) {
        const PickTag t{PickTag::Kind(k), 0, 0};
        allowedKinds[k] = t.kind == PickTag::Preview       ? false
                        : t.kind == PickTag::OriginPlane ? allows(filter, "plane") || allows(filter, "originplane")
                        : t.kind == PickTag::OriginAxis  ? allows(filter, "axis") || allows(filter, "originaxis")
                        : t.kind == PickTag::OriginPoint ? allows(filter, "point") || allows(filter, "originpoint")
                        : t.kind == PickTag::BodyFace    ? allows(filter, "face") || allows(filter, "body")
                                                         : allows(filter, tagFilterKind(t));
    }
    auto kindAllowed = [&](const PickTag &t) { return allowedKinds[t.kind]; };
    auto priority = [](const PickTag &t) {
        return t.kind == PickTag::Profile ? 2 : (t.kind == PickTag::Plane || t.kind == PickTag::OriginPlane) ? 1 : 0;
    };

    // Nearest triangle along the ray (Moller-Trumbore)
    float bestT = std::numeric_limits<float>::max();
    float occluderT = std::numeric_limits<float>::max();   // nearest solid face, whatever the filter
    const PickTag *bestTri = nullptr;
    const QVector3D invDir(std::fabs(dir.x()) > 1e-12f ? 1.0f / dir.x() : 1e12f, std::fabs(dir.y()) > 1e-12f ? 1.0f / dir.y() : 1e12f,
                           std::fabs(dir.z()) > 1e-12f ? 1.0f / dir.z() : 1e12f);
    auto rayHitsBox = [&](const Chunk &c) {
        const float eps = tol * 0.01f;
        float t0 = -std::numeric_limits<float>::max(), t1 = std::numeric_limits<float>::max();
        for (int k = 0; k < 3; ++k) {
            const float a = (c.lo[k] - eps - origin[k]) * invDir[k], b = (c.hi[k] + eps - origin[k]) * invDir[k];
            t0 = std::max(t0, std::min(a, b));
            t1 = std::min(t1, std::max(a, b));
        }
        return t1 >= std::max(t0, 0.0f);
    };
    auto testTris = [&](const FaceMesh &m, bool allowed, int begin, int end) {
        for (int i = begin; i + 2 < end; i += 3) {
            const QVector3D &a = m.pos[m.tris[i]], &b = m.pos[m.tris[i + 1]], &c = m.pos[m.tris[i + 2]];
            const QVector3D e1 = b - a, e2 = c - a, p = QVector3D::crossProduct(dir, e2);
            const float det = QVector3D::dotProduct(e1, p);
            if (std::fabs(det) < 1e-12f) continue;
            const float invDet = 1.0f / det;
            const QVector3D s = origin - a;
            const float u = QVector3D::dotProduct(s, p) * invDet;
            if (u < 0 || u > 1) continue;
            const QVector3D q = QVector3D::crossProduct(s, e1);
            const float v = QVector3D::dotProduct(dir, q) * invDet;
            if (v < 0 || u + v > 1) continue;
            const float t = QVector3D::dotProduct(e2, q) * invDet;
            if (t <= 0) continue;
            if (m_sectionEnabled && m.alpha >= 1.0f
                && QVector3D::dotProduct(origin + dir * t, m_sectionNormal.normalized()) > m_sectionOffset)
                continue;   // cut away by the section view
            if (m.tag.kind == PickTag::BodyFace) occluderT = std::min(occluderT, t);
            if (!allowed) continue;
            const bool closer = t < bestT - tol;
            const bool tieWins = std::fabs(t - bestT) <= tol && bestTri && priority(m.tag) > priority(*bestTri);
            if (closer || tieWins) { bestT = std::min(t, bestT); bestTri = &m.tag; }
        }
    };
    for (const FaceMesh &m : m_faces) {
        const bool allowed = kindAllowed(m.tag);
        if (!allowed && m.tag.kind != PickTag::BodyFace) continue;
        if (m.chunks.isEmpty()) testTris(m, allowed, 0, int(m.tris.size()));
        for (const Chunk &c : m.chunks)
            if (rayHitsBox(c)) testTris(m, allowed, c.begin, c.end);
    }

    // Nearest visible polyline within a few pixels on screen
    const PickTag *bestEdge = nullptr;
    double bestPx = 7.0;
    auto toScreen = [&](const QVector3D &p, bool &ok) {
        const QVector4D c = mvp * QVector4D(p, 1.0f);
        ok = c.w() > 1e-6f;
        return QPointF((c.x() / c.w() + 1.0) * 0.5 * w, (1.0 - c.y() / c.w()) * 0.5 * h);
    };
    // Chunks whose on-screen box is further than bestPx from the cursor hold no closer segment
    auto chunkNear = [&](const Chunk &c) {
        double x0 = 1e300, y0 = 1e300, x1 = -1e300, y1 = -1e300;
        for (int k = 0; k < 8; ++k) {
            bool ok = false;
            const QPointF s = toScreen(QVector3D(k & 1 ? c.hi.x() : c.lo.x(), k & 2 ? c.hi.y() : c.lo.y(),
                                                 k & 4 ? c.hi.z() : c.lo.z()), ok);
            if (!ok) return true;   // crosses the eye plane: test it segment by segment
            x0 = std::min(x0, s.x()); x1 = std::max(x1, s.x());
            y0 = std::min(y0, s.y()); y1 = std::max(y1, s.y());
        }
        return x >= x0 - bestPx && x <= x1 + bestPx && y >= y0 - bestPx && y <= y1 + bestPx;
    };
    auto testSegments = [&](const Polyline &pl, int begin, int end) {
        for (int i = begin; i + 1 < end; i += pl.segments ? 2 : 1) {
            bool okA = false, okB = false;
            const QPointF a = toScreen(pl.pts[i], okA), b = toScreen(pl.pts[i + 1], okB);
            if (!okA || !okB) continue;
            const QPointF ab = b - a;
            const double len2 = QPointF::dotProduct(ab, ab);
            double s = len2 > 0 ? QPointF::dotProduct(QPointF(x, y) - a, ab) / len2 : 0.0;
            s = std::clamp(s, 0.0, 1.0);
            const QPointF closest = a + ab * s;
            const double d = std::hypot(closest.x() - x, closest.y() - y);
            if (d >= bestPx) continue;
            const QVector3D p3 = pl.pts[i] + (pl.pts[i + 1] - pl.pts[i]) * float(s);
            const float t = QVector3D::dotProduct(p3 - origin, dir);
            if (t > std::min(bestT, occluderT) + tol * 3) continue;   // hidden behind a face
            bestPx = d;
            bestEdge = &pl.tag;
        }
    };
    for (const Polyline &pl : m_polylines) {
        if (!kindAllowed(pl.tag)) continue;
        if (pl.tag.kind == PickTag::BodyEdge && !m_showEdges) continue;
        if (pl.chunks.isEmpty()) testSegments(pl, 0, int(pl.pts.size()));
        for (const Chunk &c : pl.chunks)
            if (chunkNear(c)) testSegments(pl, c.begin, c.end);
    }

    if (bestEdge) return tagRef(*bestEdge, filter);
    if (bestTri) {
        QVariantMap ref = tagRef(*bestTri, filter);
        if (ref.value("kind") == "face") {
            const QVector3D hp = origin + dir * bestT;
            ref["hit"] = QVariantList{hp.x(), hp.y(), hp.z()};
        }
        return ref;
    }
    return {};
}

QVariantMap BRepRenderer::snapPoint(double x, double y) const {
    if (!m_doc || !m_camera || width() < 1 || height() < 1) return {};
    const QMatrix4x4 m = mvp();
    const QVector3D eye = m_camera->eye(), view = (m_camera->target() - eye).normalized();
    const double diag = m_hasBounds ? std::max(1.0f, (m_boundsMax - m_boundsMin).length()) : 100.0;
    const QVariantMap face = pick(x, y, "face");
    QVector3D hit;
    float hitDepth = std::numeric_limits<float>::max();
    if (face.contains("hit")) {
        const QVariantList h = face.value("hit").toList();
        hit = QVector3D(h[0].toFloat(), h[1].toFloat(), h[2].toFloat());
        hitDepth = QVector3D::dotProduct(hit - eye, view);
    }
    double bestPx = 12;
    QVector3D best;
    QString bestLabel;
    auto consider = [&](const gp_Pnt &gp, const char *label) {
        const QVector3D p = toQt(gp);
        QPointF s;
        if (!toScreen(m, p, s)) return;
        const double d = std::hypot(s.x() - x, s.y() - y);
        if (d >= bestPx) return;
        if (QVector3D::dotProduct(p - eye, view) > hitDepth + diag * 2e-3) return;   // behind the face under the cursor
        bestPx = d;
        best = p;
        bestLabel = label;
    };
    consider(gp_Pnt(0, 0, 0), "Origin");
    for (int i = 0; i < m_doc->displayBodyCount(); ++i) {
        FissionBody *b = m_doc->body(i);
        const TopoDS_Shape shape = m_doc->displayShape(i);
        if ((b && !b->isVisible()) || shape.IsNull() || MeshUtils::isMesh(shape)) continue;
        TopTools_IndexedMapOfShape verts, edges;
        TopExp::MapShapes(shape, TopAbs_VERTEX, verts);
        TopExp::MapShapes(shape, TopAbs_EDGE, edges);
        if (edges.Extent() > 6000) continue;   // converted meshes: far too many to be useful
        for (int k = 1; k <= verts.Extent(); ++k) consider(BRep_Tool::Pnt(TopoDS::Vertex(verts(k))), "Vertex");
        for (int k = 1; k <= edges.Extent(); ++k) {
            const TopoDS_Edge e = TopoDS::Edge(edges(k));
            if (BRep_Tool::Degenerated(e)) continue;
            try {
                BRepAdaptor_Curve c(e);
                if (c.GetType() == GeomAbs_Circle) consider(c.Circle().Location(), "Center");
                consider(c.Value((c.FirstParameter() + c.LastParameter()) / 2), "Midpoint");
            } catch (const Standard_Failure &) {
            }
        }
    }
    if (!bestLabel.isEmpty()) return {{"x", best.x()}, {"y", best.y()}, {"z", best.z()}, {"label", bestLabel}};
    if (face.contains("hit")) return {{"x", hit.x()}, {"y", hit.y()}, {"z", hit.z()}, {"label", "On face"}};
    return {};
}

static bool segmentsCross(const QPointF &a, const QPointF &b, const QPointF &c, const QPointF &d) {
    auto cross = [](const QPointF &o, const QPointF &p, const QPointF &q) {
        return (p.x() - o.x()) * (q.y() - o.y()) - (p.y() - o.y()) * (q.x() - o.x());
    };
    const double d1 = cross(c, d, a), d2 = cross(c, d, b), d3 = cross(a, b, c), d4 = cross(a, b, d);
    return ((d1 > 0) != (d2 > 0)) && ((d3 > 0) != (d4 > 0));
}

QVariantList BRepRenderer::pickRegion(const QVariantList &polygon, bool crossing, const QString &filterArg) const {
    QString filter = filterArg.isEmpty() ? (m_selection ? m_selection->filter() : QString()) : filterArg;
    if (filter == "*") filter.clear();
    if (!m_camera || width() < 1 || height() < 1) return {};
    QPolygonF poly;
    for (const QVariant &v : polygon) {
        const QVariantList xy = v.toList();
        if (xy.size() == 2) poly << QPointF(xy[0].toDouble(), xy[1].toDouble());
    }
    if (poly.size() < 3) return {};
    const QRectF bounds = poly.boundingRect();
    const QMatrix4x4 m = mvp();
    auto inside = [&](const QPointF &p) { return bounds.contains(p) && poly.containsPoint(p, Qt::OddEvenFill); };
    auto crossesOutline = [&](const QPointF &a, const QPointF &b) {
        for (int i = 0; i < poly.size(); ++i)
            if (segmentsCross(a, b, poly[i], poly[(i + 1) % poly.size()])) return true;
        return false;
    };

    // Coarse depth grid over the region: nearest opaque body face per cell (view depth), so items hidden
    // behind others are skipped like in Fusion. At most ~384 cells across keeps this cheap on big meshes.
    const QMatrix4x4 view = m_camera->viewMatrix();
    auto depthOf = [&](const QVector3D &p) { return -(view * QVector4D(p, 1.0f)).z(); };
    const QRectF area = bounds.intersected(QRectF(0, 0, width(), height()));
    const double cell = std::max(1.0, std::max(area.width(), area.height()) / 384.0);
    const int gw = std::max(1, int(std::ceil(area.width() / cell))), gh = std::max(1, int(std::ceil(area.height() / cell)));
    std::vector<float> depth(size_t(gw) * gh, std::numeric_limits<float>::max());
    const float depthTol = float(m_camera->distance() * 0.012);
    for (const FaceMesh &fm : m_faces) {
        if (fm.tag.kind != PickTag::BodyFace || fm.alpha < 1.0f) continue;
        QVector<QPointF> s(fm.pos.size());
        QVector<float> z(fm.pos.size());
        QVector<char> ok(fm.pos.size());
        for (int i = 0; i < fm.pos.size(); ++i) {
            ok[i] = toScreen(m, fm.pos[i], s[i]);
            z[i] = depthOf(fm.pos[i]);
        }
        for (int i = 0; i + 2 < fm.tris.size(); i += 3) {
            const int a = fm.tris[i], b = fm.tris[i + 1], c = fm.tris[i + 2];
            if (!ok[a] || !ok[b] || !ok[c]) continue;
            if (m_sectionEnabled
                && QVector3D::dotProduct((fm.pos[a] + fm.pos[b] + fm.pos[c]) / 3.0f, m_sectionNormal.normalized()) > m_sectionOffset)
                continue;
            const double x0 = std::min({s[a].x(), s[b].x(), s[c].x()}), x1 = std::max({s[a].x(), s[b].x(), s[c].x()});
            const double y0 = std::min({s[a].y(), s[b].y(), s[c].y()}), y1 = std::max({s[a].y(), s[b].y(), s[c].y()});
            if (x1 < area.left() || x0 > area.right() || y1 < area.top() || y0 > area.bottom()) continue;
            const double den = (s[b].y() - s[c].y()) * (s[a].x() - s[c].x()) + (s[c].x() - s[b].x()) * (s[a].y() - s[c].y());
            if (std::fabs(den) < 1e-12) continue;
            const int cx0 = std::max(0, int((x0 - area.left()) / cell)), cx1 = std::min(gw - 1, int((x1 - area.left()) / cell));
            const int cy0 = std::max(0, int((y0 - area.top()) / cell)), cy1 = std::min(gh - 1, int((y1 - area.top()) / cell));
            for (int cy = cy0; cy <= cy1; ++cy)
                for (int cx = cx0; cx <= cx1; ++cx) {
                    const double px = area.left() + (cx + 0.5) * cell, py = area.top() + (cy + 0.5) * cell;
                    const double l1 = ((s[b].y() - s[c].y()) * (px - s[c].x()) + (s[c].x() - s[b].x()) * (py - s[c].y())) / den;
                    const double l2 = ((s[c].y() - s[a].y()) * (px - s[c].x()) + (s[a].x() - s[c].x()) * (py - s[c].y())) / den;
                    const double l3 = 1 - l1 - l2;
                    if (l1 < 0 || l2 < 0 || l3 < 0) continue;
                    float &d = depth[size_t(cy) * gw + cx];
                    d = std::min(d, float(l1 * z[a] + l2 * z[b] + l3 * z[c]));
                }
        }
    }
    // Visible when not behind the nearest face in this cell or a neighbour (silhouettes are forgiving)
    auto visible = [&](const QPointF &p, float z) {
        const int cx = int((p.x() - area.left()) / cell), cy = int((p.y() - area.top()) / cell);
        float limit = -std::numeric_limits<float>::max();
        bool any = false;
        for (int dy = -1; dy <= 1; ++dy)
            for (int dx = -1; dx <= 1; ++dx) {
                const int x = cx + dx, y = cy + dy;
                if (x < 0 || y < 0 || x >= gw || y >= gh) continue;
                limit = std::max(limit, depth[size_t(y) * gw + x]);
                any = true;
            }
        return !any || z <= limit + depthTol + std::fabs(z) * 0.002f;
    };
    // Sketch curves / profiles stay pickable behind bodies (they are drawn on top while sketching)
    auto occludable = [](const PickTag &t) { return t.kind != PickTag::SketchCurve && t.kind != PickTag::Profile; };

    // Several meshes / polylines can map to one selectable item (every face of a body), so judge per item.
    // seen = some sample inside the outline is not hidden behind a face
    struct Group { PickTag tag; bool all = true; bool any = false; bool seen = false; };
    QHash<QString, Group> groups;
    QStringList order;
    auto groupFor = [&](const PickTag &t) -> Group & {
        const bool asBody = t.kind == PickTag::BodyFace && !allows(filter, "face");
        const QString key = asBody ? QString("body|%1").arg(t.owner) : tagKey(t);
        if (!groups.contains(key)) { groups.insert(key, Group{t}); order << key; }
        return groups[key];
    };
    auto allowed = [&](const PickTag &t) {
        switch (t.kind) {
            case PickTag::Preview: case PickTag::OriginPoint: case PickTag::OriginAxis: case PickTag::OriginPlane:
                return false;
            case PickTag::BodyFace: return allows(filter, "face") || allows(filter, "body");
            case PickTag::BodyEdge: return m_showEdges && allows(filter, "edge");
            default: return allows(filter, tagFilterKind(t));
        }
    };

    for (const FaceMesh &fm : m_faces) {
        if (!allowed(fm.tag)) continue;
        QVector<QPointF> s(fm.pos.size());
        QVector<char> ok(fm.pos.size());
        for (int i = 0; i < fm.pos.size(); ++i) ok[i] = toScreen(m, fm.pos[i], s[i]);
        Group &g = groupFor(fm.tag);
        const bool occl = occludable(fm.tag);
        for (int i = 0; i < fm.pos.size(); ++i) {
            const bool in = ok[i] && inside(s[i]);
            g.all = g.all && in;
            g.any = g.any || in;
            if (in && !g.seen) g.seen = !occl || visible(s[i], depthOf(fm.pos[i]));
        }
        // Triangle centres catch big flat faces whose corners are all hidden
        for (int i = 0; i + 2 < fm.tris.size() && g.any && !g.seen; i += 3) {
            const int a = fm.tris[i], b = fm.tris[i + 1], c = fm.tris[i + 2];
            if (!ok[a] || !ok[b] || !ok[c]) continue;
            const QPointF mid = (s[a] + s[b] + s[c]) / 3.0;
            if (inside(mid)) g.seen = visible(mid, depthOf((fm.pos[a] + fm.pos[b] + fm.pos[c]) / 3.0f));
        }
        if (crossing && !g.any) {
            // The outline may cut through a face without enclosing any of its vertices
            for (int i = 0; i + 2 < fm.tris.size() && !g.any; i += 3) {
                const int a = fm.tris[i], b = fm.tris[i + 1], c = fm.tris[i + 2];
                if (!ok[a] || !ok[b] || !ok[c]) continue;
                const QPolygonF tri({s[a], s[b], s[c]});
                g.any = tri.containsPoint(poly[0], Qt::OddEvenFill) || crossesOutline(s[a], s[b])
                        || crossesOutline(s[b], s[c]) || crossesOutline(s[c], s[a]);
                if (g.any && !g.seen)
                    g.seen = !occl || visible((s[a] + s[b] + s[c]) / 3.0, depthOf((fm.pos[a] + fm.pos[b] + fm.pos[c]) / 3.0f));
            }
        }
    }
    for (const Polyline &pl : m_polylines) {
        if (!allowed(pl.tag)) continue;
        Group &g = groupFor(pl.tag);
        const bool occl = occludable(pl.tag);
        QPointF prev;
        bool prevOk = false;
        for (int i = 0; i < pl.pts.size(); ++i) {
            QPointF p;
            const bool ok = toScreen(m, pl.pts[i], p);
            const bool in = ok && inside(p);
            g.all = g.all && in;
            g.any = g.any || in;
            if (in && !g.seen) g.seen = !occl || visible(p, depthOf(pl.pts[i]));
            if (crossing && !g.any && ok && prevOk && crossesOutline(prev, p)) {
                g.any = true;
                if (!g.seen) g.seen = !occl || visible(p, depthOf(pl.pts[i]));
            }
            prev = p;
            prevOk = ok;
        }
    }

    QVariantList out;
    QSet<QString> seen;
    for (const QString &key : order) {
        const Group &g = groups[key];
        if (!(crossing ? g.any : (g.all && g.any)) || !g.seen) continue;
        const QVariantMap ref = tagRef(g.tag, filter);
        const QString k = refKey(ref);
        if (ref.isEmpty() || seen.contains(k)) continue;
        seen.insert(k);
        out << ref;
    }
    return out;
}

void BRepRenderer::hover(double x, double y) {
    QElapsedTimer timer;
    timer.start();
    const QVariantMap ref = pick(x, y);
    const qint64 picked = timer.elapsed();
    if (refKey(ref) == refKey(m_hovered)) {
        if (picked > 30) qInfo("viewport: hover pick took %lld ms", picked);
        return;
    }
    m_hovered = ref;
    emit hoveredChanged();
    const qint64 signalled = timer.elapsed();
    assembleHover();
    if (timer.elapsed() > 30)
        qInfo("viewport: hover took %lld ms (pick %lld, handlers %lld, highlight %lld)", timer.elapsed(), picked,
              signalled - picked, timer.elapsed() - signalled);
}

void BRepRenderer::clearHover() {
    if (m_hovered.isEmpty()) return;
    m_hovered.clear();
    emit hoveredChanged();
    assembleHover();
}

FissionSketch *BRepRenderer::sketch(const QString &id) const {
    return m_doc ? qobject_cast<FissionSketch *>(m_doc->featureById(id)) : nullptr;
}

QMatrix4x4 BRepRenderer::mvp() const {
    return m_camera->projectionMatrix(width() / std::max(1.0, height())) * m_camera->viewMatrix();
}

bool BRepRenderer::toScreen(const QMatrix4x4 &m, const QVector3D &p, QPointF &out) const {
    const QVector4D c = m * QVector4D(p, 1.0f);
    if (c.w() <= 1e-6f) return false;
    out = QPointF((c.x() / c.w() + 1.0) * 0.5 * width(), (1.0 - c.y() / c.w()) * 0.5 * height());
    return true;
}

double BRepRenderer::snapStep() const {
    if (!m_camera) return 1.0;
    // 1-2-5 series, roughly 100 grid cells across the view
    const double raw = m_camera->distance() / 120.0;
    const double mag = std::pow(10.0, std::floor(std::log10(raw)));
    for (double f : {1.0, 2.0, 5.0, 10.0})
        if (f * mag >= raw) return f * mag;
    return 10 * mag;
}

QVariantMap BRepRenderer::sketchPoint(double x, double y, const QString &sketchId, bool snap) const {
    FissionSketch *s = sketch(sketchId);
    if (!s || !m_camera || width() < 1 || height() < 1) return {};
    const gp_Ax3 &pl = s->plane();
    const QMatrix4x4 m = mvp();
    const QMatrix4x4 inv = m.inverted();
    const float nx = float(2.0 * x / width() - 1.0), ny = float(1.0 - 2.0 * y / height());
    const QVector3D o = (inv * QVector4D(nx, ny, -1, 1)).toVector3DAffine();
    const QVector3D f = (inv * QVector4D(nx, ny, 1, 1)).toVector3DAffine();
    const gp_Pnt O = pl.Location();
    const gp_Dir N = pl.Direction(), X = pl.XDirection(), Y = pl.YDirection();
    const QVector3D n(N.X(), N.Y(), N.Z()), origin(O.X(), O.Y(), O.Z());
    const QVector3D dir = f - o;
    const float denom = QVector3D::dotProduct(dir, n);
    if (std::fabs(denom) < 1e-9f) return {};
    const float t = QVector3D::dotProduct(origin - o, n) / denom;
    const QVector3D hit = o + dir * t;
    const QVector3D rel = hit - origin;
    double u = QVector3D::dotProduct(rel, QVector3D(X.X(), X.Y(), X.Z()));
    double v = QVector3D::dotProduct(rel, QVector3D(Y.X(), Y.Y(), Y.Z()));
    if (!snap) return {{"u", u}, {"v", v}, {"snap", ""}};

    // Entity points (ends, centres, midpoints) and other curve ends within 10 px. Entity points win ties
    // so the caller learns which entity / role it snapped to.
    double best = 10.0;
    bool found = false;
    int snapEntity = -2;
    QString snapRole;
    auto consider = [&](const gp_Pnt &p, int entity, const QString &role, double bias) {
        QPointF sp;
        if (!toScreen(m, QVector3D(p.X(), p.Y(), p.Z()), sp)) return;
        const double d = std::hypot(sp.x() - x, sp.y() - y) + bias;
        if (d >= best) return;
        best = d;
        const gp_Vec r(O, p);
        u = r.Dot(gp_Vec(X));
        v = r.Dot(gp_Vec(Y));
        found = true;
        snapEntity = entity;
        snapRole = role;
    };
    auto onPlane = [&](double pu, double pv) { return O.Translated(gp_Vec(X) * pu + gp_Vec(Y) * pv); };
    consider(O, -1, "origin", 0);
    for (const QVariant &hv : s->handles()) {
        const QVariantMap h = hv.toMap();
        consider(onPlane(h["u"].toDouble(), h["v"].toDouble()), h["entity"].toInt(), h["role"].toString(), 0);
    }
    for (const auto &edges : s->entityEdges()) {
        for (const TopoDS_Edge &e : edges) {
            BRepAdaptor_Curve c(e);
            consider(c.Value(c.FirstParameter()), -2, QString(), 0.5);
            consider(c.Value(c.LastParameter()), -2, QString(), 0.5);
        }
    }
    if (found) {
        QVariantMap out{{"u", u}, {"v", v}, {"snap", "endpoint"}};
        if (snapEntity >= -1) { out["entity"] = snapEntity; out["role"] = snapRole; }
        return out;
    }
    if (!m_snapToGrid)
        return {{"u", u}, {"v", v}, {"snap", ""}};
    const double step = snapStep();
    return {{"u", std::round(u / step) * step}, {"v", std::round(v / step) * step}, {"snap", "grid"}};
}

QVariantList BRepRenderer::projectSketch(const QString &sketchId, const QVariantList &uv) const {
    FissionSketch *s = sketch(sketchId);
    QVariantList out;
    if (!s || !m_camera) return out;
    const gp_Ax3 &pl = s->plane();
    const QMatrix4x4 m = mvp();
    for (const QVariant &item : uv) {
        const QVariantList p = item.toList();
        if (p.size() < 2) continue;
        const gp_Pnt w = pl.Location().Translated(gp_Vec(pl.XDirection()) * p[0].toDouble()
                                                  + gp_Vec(pl.YDirection()) * p[1].toDouble());
        QPointF sp;
        if (toScreen(m, QVector3D(w.X(), w.Y(), w.Z()), sp)) out.append(QVariant(QVariantList{sp.x(), sp.y()}));
    }
    return out;
}

QVariantList BRepRenderer::projectPoint(const QVector3D &p) const {
    QPointF sp;
    if (!m_camera || !toScreen(mvp(), p, sp)) return {};
    return {sp.x(), sp.y()};
}

QVariant BRepRenderer::axisParameter(double x, double y, const QVector3D &origin, const QVector3D &dir) const {
    if (!m_camera || width() < 1 || height() < 1) return {};
    const QMatrix4x4 inv = mvp().inverted();
    const float nx = float(2.0 * x / width() - 1.0), ny = float(1.0 - 2.0 * y / height());
    const QVector3D o = (inv * QVector4D(nx, ny, -1, 1)).toVector3DAffine();
    const QVector3D r = ((inv * QVector4D(nx, ny, 1, 1)).toVector3DAffine() - o).normalized();
    const QVector3D a = dir.normalized();
    // Closest points between the axis line and the cursor ray
    const float b = QVector3D::dotProduct(a, r);
    const float denom = 1.0f - b * b;
    if (denom < 1e-4f) return {};
    const QVector3D w = origin - o;
    const float d = QVector3D::dotProduct(a, w), e = QVector3D::dotProduct(r, w);
    return double((b * e - d) / denom);
}

QVariant BRepRenderer::planePoint(double x, double y, const QVector3D &origin, const QVector3D &normal) const {
    if (!m_camera || width() < 1 || height() < 1) return {};
    const QMatrix4x4 inv = mvp().inverted();
    const float nx = float(2.0 * x / width() - 1.0), ny = float(1.0 - 2.0 * y / height());
    const QVector3D o = (inv * QVector4D(nx, ny, -1, 1)).toVector3DAffine();
    const QVector3D r = ((inv * QVector4D(nx, ny, 1, 1)).toVector3DAffine() - o).normalized();
    const QVector3D n = normal.normalized();
    const float denom = QVector3D::dotProduct(n, r);
    if (std::fabs(denom) < 1e-5f) return {};
    const float t = QVector3D::dotProduct(n, origin - o) / denom;
    return QVariant::fromValue(o + r * t);
}

void BRepRenderer::lookAtSketch(const QString &sketchId, bool animate) {
    FissionSketch *s = sketch(sketchId);
    if (!s || !m_camera) return;
    const gp_Ax3 &pl = s->plane();
    const gp_Dir n = pl.Direction();
    const gp_Pnt o = pl.Location();
    m_camera->lookAlong(QVector3D(n.X(), n.Y(), n.Z()), QVector3D(o.X(), o.Y(), o.Z()), animate);
}

void BRepRenderer::fitAll(bool animate) {
    if (!m_camera) return;
    if (m_hasBounds) m_camera->fitBox(m_boundsMin, m_boundsMax, animate);
    else m_camera->fitBox(QVector3D(-50, -50, 0), QVector3D(50, 50, 20), animate);
}
