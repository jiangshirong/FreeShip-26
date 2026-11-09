#include "gpu_fill.h"
#include <QOpenGLFunctions>
#include <QElapsedTimer>
#include <QVector4D>

GpuFill::GpuFill(const Scene&s){
    QElapsedTimer timer;timer.start();
    QSurfaceFormat format;format.setVersion(2,0);format.setRenderableType(QSurfaceFormat::OpenGL);
    context_.setFormat(format);if(!context_.create())return;
    surface_.setFormat(context_.format());surface_.create();
    if(!surface_.isValid()||!context_.makeCurrent(&surface_))return;
    auto*f=context_.functions();f->initializeOpenGLFunctions();
    device=QString::fromLatin1(reinterpret_cast<const char*>(f->glGetString(GL_RENDERER)));
    program_=std::make_unique<QOpenGLShaderProgram>();
    bool ok=program_->addShaderFromSourceCode(QOpenGLShader::Vertex,
        "attribute vec2 position; attribute vec3 color; uniform vec4 camera; varying vec3 rgb;"
        "void main(){gl_Position=vec4(position*camera.xy+camera.zw,0.0,1.0);rgb=color;}")&&
        program_->addShaderFromSourceCode(QOpenGLShader::Fragment,
        "varying vec3 rgb; void main(){gl_FragColor=vec4(rgb,1.0);}")&&program_->link();
    if(!ok){device+=" / "+program_->log();context_.doneCurrent();return;}
    center_=s.bounds.center();extent_=qMax(s.bounds.width(),s.bounds.height());
    QVector<float> data;data.reserve(s.triangles*3*5);
    for(const auto&p:s.primitives)if(p.kind==1){auto c=legacyColor(p.color);for(const auto&v:p.points){
        data.append(float((v.x()-center_.x())/extent_));data.append(float((v.y()-center_.y())/extent_));
        data.append(float(c.redF()));data.append(float(c.greenF()));data.append(float(c.blueF()));
    }}
    vertices_=data.size()/5;
    if(buffer_.create()&&buffer_.bind()){buffer_.setUsagePattern(QOpenGLBuffer::StaticDraw);buffer_.allocate(data.constData(),data.size()*sizeof(float));ready_=f->glGetError()==GL_NO_ERROR;buffer_.release();}
    context_.doneCurrent();initializationMs=timer.nsecsElapsed()/1e6;
}
GpuFill::~GpuFill(){if(context_.makeCurrent(&surface_)){fbo_.reset();buffer_.destroy();program_.reset();context_.doneCurrent();}}
QImage GpuFill::render(QSize size,qreal dpr,const QTransform&camera){
    // Beyond this range, float screen transforms may become visibly imprecise.
    if(!ready_||camera.m11()*extent_>1e6||!context_.makeCurrent(&surface_))return {};
    auto*f=context_.functions();QSize physical(qRound(size.width()*dpr),qRound(size.height()*dpr));
    if(!fbo_||fbo_->size()!=physical)fbo_=std::make_unique<QOpenGLFramebufferObject>(physical);
    if(!fbo_->isValid()||!fbo_->bind()){context_.doneCurrent();return {};}
    f->glViewport(0,0,physical.width(),physical.height());f->glDisable(GL_DEPTH_TEST);f->glDisable(GL_BLEND);f->glDisable(GL_CULL_FACE);f->glDisable(GL_DITHER);
    f->glClearColor(1,1,1,1);f->glClear(GL_COLOR_BUFFER_BIT);program_->bind();buffer_.bind();
    auto origin=camera.map(center_);
    program_->setUniformValue("camera",QVector4D(2*camera.m11()*extent_/size.width(),-2*camera.m22()*extent_/size.height(),2*origin.x()/size.width()-1,1-2*origin.y()/size.height()));
    int pos=program_->attributeLocation("position"),col=program_->attributeLocation("color");
    program_->enableAttributeArray(pos);program_->enableAttributeArray(col);
    program_->setAttributeBuffer(pos,GL_FLOAT,0,2,5*sizeof(float));program_->setAttributeBuffer(col,GL_FLOAT,2*sizeof(float),3,5*sizeof(float));
    f->glDrawArrays(GL_TRIANGLES,0,vertices_);
    program_->disableAttributeArray(pos);program_->disableAttributeArray(col);buffer_.release();program_->release();
    QImage result=fbo_->toImage();fbo_->release();context_.doneCurrent();result.setDevicePixelRatio(dpr);return result;
}
