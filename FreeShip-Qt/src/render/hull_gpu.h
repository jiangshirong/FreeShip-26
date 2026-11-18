#pragma once

#include "core/model_snapshot.h"
#include "view_lighting.h"
#include "surface_analysis.h"
#include <QColor>
#include <QImage>
#include <QMatrix4x4>
#include <QOffscreenSurface>
#include <QOpenGLBuffer>
#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>
#include <QOpenGLShaderProgram>
#include <memory>

struct HullRenderTriangle {
    std::array<fs::Vec3, 3> vertices;
    fs::Vec3 normal;
    QColor color;
    int sourceTriangle=-1;
    bool reflected=false;
    bool useUnderwater=true;
};

struct HullRenderMesh {
    QVector<HullRenderTriangle> triangles;
    fs::Vec3 minimum, maximum, center;
    double extent = 1;
    bool shadeUnderwater = false;
    double waterlineZ = 0;
    QColor underwaterColor;
    int underwaterAlpha=255;
};

// Uploaded once per immutable model revision. Camera changes replace uniforms;
// they do not tessellate, sort, or re-upload the hull.
class HullGpu {
public:
    explicit HullGpu(const HullRenderMesh& mesh);
    ~HullGpu();
    bool ready() const { return ready_; }
    bool setAnalysis(const QVector<SurfaceShadeTriangle>& triangles);
    QImage render(QSize logicalSize, qreal dpr, const QMatrix4x4& camera,
                  const HullLighting& lighting,const HullStyle& style={});
private:
    QOpenGLContext context_;
    QOffscreenSurface surface_;
    QOpenGLBuffer buffer_;
    QOpenGLBuffer analysisBuffer_;
    bool analysisReady_=false;
    std::unique_ptr<QOpenGLShaderProgram> program_;
    std::unique_ptr<QOpenGLFramebufferObject> fbo_;
    int vertexCount_ = 0;
    bool ready_ = false;
    bool shadeUnderwater_ = false;
    float waterline_ = 0;
};
