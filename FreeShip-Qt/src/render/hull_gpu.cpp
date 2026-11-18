#include "hull_gpu.h"
#include "underwater_color.h"
#include <QOpenGLFunctions>
#include <limits>

HullGpu::HullGpu(const HullRenderMesh& mesh) {
    QSurfaceFormat format;
    format.setVersion(2, 0);
    format.setRenderableType(QSurfaceFormat::OpenGL);
    format.setDepthBufferSize(24);
    context_.setFormat(format);
    if (!context_.create()) return;
    surface_.setFormat(context_.format());
    surface_.create();
    if (!surface_.isValid() || !context_.makeCurrent(&surface_)) return;
    auto* gl = context_.functions();
    gl->initializeOpenGLFunctions();
    program_ = std::make_unique<QOpenGLShaderProgram>();
    const bool linked = program_->addShaderFromSourceCode(QOpenGLShader::Vertex,
        "attribute vec3 position; attribute vec3 normal; attribute vec3 color; attribute vec3 center;"
        "attribute vec3 wetColor; varying vec3 wetRgb;"
        "uniform mat4 camera; varying vec3 rgb; varying float height; varying float lightFactor;"
        "attribute vec3 smoothNormal; attribute vec3 gaussianColor; attribute vec3 developColor;"
        "uniform int shadeMode; uniform vec3 eye; uniform vec3 viewDirection; uniform bool perspective; varying float zebraValue;"
        "uniform bool lighting; uniform vec3 lightPosition; uniform float ambient; uniform float intensity;"
        "void main(){ gl_Position=camera*vec4(position,1.0); rgb=color; wetRgb=wetColor;"
        // A face has one normal and center. Compute its light at the vertices,
        // not repeatedly for every covered pixel. Each vertex gets the same value.
        "height=position.z; lightFactor=1.0;"
        "if(lighting){vec3 d=lightPosition-center;float len=length(d);"
        "float cosine=len>0.000000000001?abs(dot(normal,d/len)):0.0;"
        "lightFactor=clamp(ambient+intensity*cosine,0.0,1.0);}"
        "if(shadeMode==1){rgb=gaussianColor;lightFactor=1.0;} else if(shadeMode==2)rgb=developColor;"
        "zebraValue=0.0;if(shadeMode==3){vec3 sight=perspective?eye-position:viewDirection;float len=length(sight);"
        "zebraValue=clamp(len>0.000000000001?abs(dot(sight/len,smoothNormal)):0.0,0.0,1.0);} }") &&
        program_->addShaderFromSourceCode(QOpenGLShader::Fragment,
        "varying vec3 rgb; varying float height; varying float lightFactor;"
        "varying float zebraValue; uniform int shadeMode; uniform vec3 zebraColor;"
        "uniform bool shadeUnderwater;"
        "uniform float waterline; varying vec3 wetRgb;"
        "void main(){ vec3 c=(shadeMode==0 && shadeUnderwater && height<waterline)"
        "?wetRgb:rgb; if(shadeMode==3 && mod(floor(zebraValue/0.02),2.0)>0.5)c=zebraColor;"
        "gl_FragColor=vec4(c*lightFactor,1.0); }") &&
        program_->link();
    if (!linked) { context_.doneCurrent(); return; }

    // Buffer byte sizes are signed ints in Qt's OpenGL wrapper.
    if (mesh.triangles.size() > std::numeric_limits<int>::max() / (45 * int(sizeof(float)))) {
        context_.doneCurrent();
        return;
    }
    QVector<float> data;
    data.reserve(mesh.triangles.size() * 45);
    for (const auto& triangle : mesh.triangles) {
        const auto wet=triangle.useUnderwater?underwaterTint(triangle.color,mesh.underwaterColor,mesh.underwaterAlpha):triangle.color;
        fs::Vec3 center{};for(const auto& p:triangle.vertices){center.x+=(p.x-mesh.center.x)/(3*mesh.extent);center.y+=(p.y-mesh.center.y)/(3*mesh.extent);center.z+=(p.z-mesh.center.z)/(3*mesh.extent);}
        for (const auto& point : triangle.vertices) {
            data.append(float((point.x - mesh.center.x) / mesh.extent));
            data.append(float((point.y - mesh.center.y) / mesh.extent));
            data.append(float((point.z - mesh.center.z) / mesh.extent));
            data.append(float(triangle.normal.x));
            data.append(float(triangle.normal.y));
            data.append(float(triangle.normal.z));
            data.append(float(triangle.color.redF()));
            data.append(float(triangle.color.greenF()));
            data.append(float(triangle.color.blueF()));
            data.append(float(center.x));data.append(float(center.y));data.append(float(center.z));
            data.append(float(wet.redF()));data.append(float(wet.greenF()));data.append(float(wet.blueF()));
        }
    }
    vertexCount_ = int(data.size() / 15);
    shadeUnderwater_ = mesh.shadeUnderwater;
    waterline_ = float((mesh.waterlineZ - mesh.center.z) / mesh.extent);
    if (buffer_.create() && buffer_.bind()) {
        buffer_.setUsagePattern(QOpenGLBuffer::StaticDraw);
        buffer_.allocate(data.constData(), int(data.size() * sizeof(float)));
        ready_ = gl->glGetError() == GL_NO_ERROR;
        buffer_.release();
    }
    context_.doneCurrent();
}

HullGpu::~HullGpu() {
    if (context_.makeCurrent(&surface_)) {
        fbo_.reset();
        buffer_.destroy();
        analysisBuffer_.destroy();
        program_.reset();
        context_.doneCurrent();
    }
}

bool HullGpu::setAnalysis(const QVector<SurfaceShadeTriangle>& triangles){
    if(analysisReady_)return true;
    if(!ready_||triangles.size()*3!=vertexCount_||triangles.size()>std::numeric_limits<int>::max()/(27*int(sizeof(float)))||!context_.makeCurrent(&surface_))return false;
    QVector<float> data;data.reserve(triangles.size()*27);
    for(const auto& triangle:triangles)for(const auto& v:triangle){data.append(float(v.normal.x));data.append(float(v.normal.y));data.append(float(v.normal.z));
        for(auto c:{v.gaussian,v.developable}){data.append(float(c.redF()));data.append(float(c.greenF()));data.append(float(c.blueF()));}}
    if(analysisBuffer_.create()&&analysisBuffer_.bind()){analysisBuffer_.setUsagePattern(QOpenGLBuffer::StaticDraw);analysisBuffer_.allocate(data.constData(),int(data.size()*sizeof(float)));analysisReady_=context_.functions()->glGetError()==GL_NO_ERROR;analysisBuffer_.release();}
    context_.doneCurrent();return analysisReady_;
}
QImage HullGpu::render(QSize logicalSize, qreal dpr, const QMatrix4x4& camera, const HullLighting& lighting,const HullStyle& style) {
    if(style.mode!=HullShade::Flat&&!analysisReady_)return {};
    if (!ready_ || logicalSize.isEmpty() || !context_.makeCurrent(&surface_)) return {};
    auto* gl = context_.functions();
    const QSize physical(qMax(1, qRound(logicalSize.width() * dpr)),
                         qMax(1, qRound(logicalSize.height() * dpr)));
    if (!fbo_ || fbo_->size() != physical) {
        QOpenGLFramebufferObjectFormat format;
        format.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
        fbo_ = std::make_unique<QOpenGLFramebufferObject>(physical, format);
    }
    if (!fbo_->isValid() || !fbo_->bind()) { context_.doneCurrent(); return {}; }
    gl->glViewport(0, 0, physical.width(), physical.height());
    gl->glEnable(GL_DEPTH_TEST);
    gl->glDepthFunc(GL_LEQUAL);
    gl->glDisable(GL_BLEND);
    gl->glDisable(GL_CULL_FACE);
    gl->glDisable(GL_DITHER);
    gl->glClearColor(0, 0, 0, 0); // Only hull fragments cover the calibrated background.
    gl->glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    program_->bind();
    buffer_.bind();
    program_->setUniformValue("camera", camera);
    program_->setUniformValue("shadeUnderwater", shadeUnderwater_);
    program_->setUniformValue("waterline", waterline_);
    program_->setUniformValue("lighting", lighting.enabled);
    program_->setUniformValue("lightPosition",QVector3D(lighting.position.x,lighting.position.y,lighting.position.z));
    program_->setUniformValue("ambient",float(lighting.ambient));program_->setUniformValue("intensity",float(lighting.intensity));
    program_->setUniformValue("shadeMode",int(style.mode));program_->setUniformValue("perspective",style.perspective);
    program_->setUniformValue("eye",QVector3D(style.eye.x,style.eye.y,style.eye.z));program_->setUniformValue("viewDirection",QVector3D(style.direction.x,style.direction.y,style.direction.z));
    program_->setUniformValue("zebraColor",QVector3D(style.zebra.redF(),style.zebra.greenF(),style.zebra.blueF()));
    const int position = program_->attributeLocation("position");
    const int normal = program_->attributeLocation("normal");
    const int color = program_->attributeLocation("color");
    const int center=program_->attributeLocation("center");
    const int wet=program_->attributeLocation("wetColor");
    program_->enableAttributeArray(position);
    program_->enableAttributeArray(normal);
    program_->enableAttributeArray(color);
    program_->enableAttributeArray(center);
    program_->enableAttributeArray(wet);
    program_->setAttributeBuffer(position, GL_FLOAT, 0, 3, 15 * sizeof(float));
    program_->setAttributeBuffer(normal, GL_FLOAT, 3 * sizeof(float), 3, 15 * sizeof(float));
    program_->setAttributeBuffer(color, GL_FLOAT, 6 * sizeof(float), 3, 15 * sizeof(float));
    program_->setAttributeBuffer(center,GL_FLOAT,9*sizeof(float),3,15*sizeof(float));
    program_->setAttributeBuffer(wet,GL_FLOAT,12*sizeof(float),3,15*sizeof(float));
    const int smooth=program_->attributeLocation("smoothNormal"),gauss=program_->attributeLocation("gaussianColor"),develop=program_->attributeLocation("developColor");
    if(style.mode!=HullShade::Flat){
        analysisBuffer_.bind();for(int location:{smooth,gauss,develop})program_->enableAttributeArray(location);
        program_->setAttributeBuffer(smooth,GL_FLOAT,0,3,9*sizeof(float));program_->setAttributeBuffer(gauss,GL_FLOAT,3*sizeof(float),3,9*sizeof(float));program_->setAttributeBuffer(develop,GL_FLOAT,6*sizeof(float),3,9*sizeof(float));
    }else for(int location:{smooth,gauss,develop}){program_->disableAttributeArray(location);program_->setAttributeValue(location,QVector3D{});}
    gl->glDrawArrays(GL_TRIANGLES, 0, vertexCount_);
    program_->disableAttributeArray(position);
    program_->disableAttributeArray(normal);
    program_->disableAttributeArray(color);
    program_->disableAttributeArray(center);
    program_->disableAttributeArray(wet);
    for(int location:{smooth,gauss,develop})program_->disableAttributeArray(location);
    analysisBuffer_.release();
    buffer_.release();
    program_->release();
    QImage image = fbo_->toImage();
    fbo_->release();
    context_.doneCurrent();
    image.setDevicePixelRatio(dpr);
    return image;
}
