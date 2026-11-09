#pragma once
#include "scene.h"
#include <QOpenGLContext>
#include <QOffscreenSurface>
#include <QOpenGLBuffer>
#include <QOpenGLShaderProgram>
#include <QOpenGLFramebufferObject>
#include <QImage>
#include <memory>

// Retained full-model triangles. Only a four-float camera uniform changes per frame.
// Readback keeps the prototype's widget/text path independently testable.
class GpuFill {
public:
    explicit GpuFill(const Scene &scene);
    ~GpuFill();
    bool ready()const{return ready_;}
    QString device;
    double initializationMs=0;
    QImage render(QSize size,qreal dpr,const QTransform &camera);
private:
    QOpenGLContext context_;
    QOffscreenSurface surface_;
    std::unique_ptr<QOpenGLShaderProgram> program_;
    QOpenGLBuffer buffer_;
    std::unique_ptr<QOpenGLFramebufferObject> fbo_;
    QPointF center_;
    double extent_=1;
    int vertices_=0;
    bool ready_=false;
};
