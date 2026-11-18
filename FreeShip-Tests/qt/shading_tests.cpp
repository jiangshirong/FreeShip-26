// SPDX-License-Identifier: GPL-2.0-or-later
// Independent analytic surfaces, topology safety and both production renderers.
#include "render/hull_view.h"
#include "render/hull_gpu.h"
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QElapsedTimer>
#include <QOpenGLFunctions>
#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <algorithm>

using fs::Vec3;
static std::shared_ptr<fs::ModelSnapshot> mesh(QVector<Vec3> points,QVector<QVector<int>> faces,int type=0){
    auto m=std::make_shared<fs::ModelSnapshot>();m->documentId="independent-analysis";m->revision=1;m->units="metric";m->length=m->beam=2;m->draft=1;m->bothSides=false;m->shadeUnderwater=false;
    m->useDefaultSplitSectionLocation=false;m->splitSectionLocation=0;
    m->layers={{1,"Surface",0x78b4dc,true,false}};auto topology=std::make_shared<fs::SurfaceTopology>();topology->points=points;topology->faces=faces;topology->types.fill(type,points.size());m->surfaceTopology=topology;
    for(int i=0;i<points.size();++i)m->points.append({fs::EntityId(i+1),points[i]});
    for(const auto& face:faces)for(int i=2;i<face.size();++i)m->triangles.append({1,{{points[face[0]],points[face[i-1]],points[face[i]]}}});return m;
}
static std::shared_ptr<fs::ModelSnapshot> fan(bool saddle){return mesh({{0,0,0},{1,0,saddle?1.:0.},{0,1,saddle?-1.:0.},{-1,0,saddle?1.:0.},{0,-1,saddle?-1.:0.}},{{0,1,2},{0,2,3},{0,3,4},{0,4,1}});}
static QJsonObject compare(const QImage& a,const QImage& b){
    int samples=0,error=0,differences=0;QPoint worst;QJsonArray errors;const auto background=a.pixelColor(2,40);
    for(int y=35;y<a.height()-55;y+=2)for(int x=4;x<a.width()-4;x+=2){const auto p=a.pixelColor(x,y);if(p==background)continue;bool inside=true;
        for(QPoint delta:{QPoint(-2,0),QPoint(2,0),QPoint(0,-2),QPoint(0,2)}){const auto c=a.pixelColor(QPoint(x,y)+delta);inside&=c!=background&&std::abs(c.red()-p.red())<5&&std::abs(c.green()-p.green())<5&&std::abs(c.blue()-p.blue())<5;}
        if(!inside)continue;++samples;const auto q=b.pixelColor(x,y);const int e=std::max({std::abs(p.red()-q.red()),std::abs(p.green()-q.green()),std::abs(p.blue()-q.blue())});if(e>1){++differences;if(errors.size()<100)errors.append(QJsonObject{{"x",x},{"y",y},{"error",e},{"cpu",p.name()},{"gpu",q.name()}});}if(e>error){error=e;worst={x,y};}
    }return {{"samples",samples},{"max_channel_error",error},{"pixels_over_one",differences},{"errors",errors},{"worst_pixel",QJsonArray{worst.x(),worst.y()}},{"cpu_worst",a.pixelColor(worst).name()},{"gpu_worst",b.pixelColor(worst).name()}};
}
static int rasterBits(){
    QSurfaceFormat format;format.setVersion(2,0);format.setRenderableType(QSurfaceFormat::OpenGL);format.setDepthBufferSize(24);
    QOpenGLContext context;context.setFormat(format);if(!context.create())return 0;QOffscreenSurface surface;surface.setFormat(context.format());surface.create();if(!surface.isValid()||!context.makeCurrent(&surface))return 0;
    auto* gl=context.functions();gl->initializeOpenGLFunctions();int bits=0;gl->glGetIntegerv(GL_SUBPIXEL_BITS,&bits);context.doneCurrent();return bits;
}
static double nearestEdge(const HullView& view,const fs::ModelSnapshot& model,QPointF sample){
    double distance=1e100;
    for(const auto& t:model.triangles)for(int mirror=0;mirror<2;++mirror){const auto* layer=model.layer(t.layer);if(!layer||!layer->visible||(mirror&&(!model.bothSides||!layer->symmetric)))continue;
        for(int i=0;i<3;++i){auto p=t.vertices[i],q=t.vertices[(i+1)%3];if(mirror){p.y=-p.y;q.y=-q.y;}const auto a=view.project(p),b=view.project(q),d=b-a,v=sample-a;
            const double length=d.x()*d.x()+d.y()*d.y(),f=length>0?std::clamp((v.x()*d.x()+v.y()*d.y())/length,0.,1.):0;const auto offset=v-f*d;distance=std::min(distance,std::hypot(offset.x(),offset.y()));}
    }return distance;
}
int main(int argc,char** argv){
    QApplication app(argc,argv);if(argc!=4)return 3;const QString out=QString::fromLocal8Bit(argv[3]);QDir().mkpath(out);QJsonArray cases;QJsonObject report;bool failed=false;
    auto check=[&](QString name,bool good){cases.append(QJsonObject{{"id",name},{"status",good?"pass":"fail"}});failed|=!good;std::fprintf(stderr,"%s %s\n",good?"pass":"fail",name.toUtf8().constData());std::fflush(stderr);};
    try{
        const auto planar=fan(false),saddle=fan(true);const auto plane=analyzeSurface(*planar),negative=analyzeSurface(*saddle);
        check("flat-interior-angle-deficit-zero",plane->valid[0]&&std::abs(plane->angleDefect[0])<1e-10);
        check("open-boundary-is-unknown-not-green",plane->boundary==4&&!plane->valid[1]&&plane->triangles[0][1].developable==QColor(150,150,150));
        check("saddle-angle-deficit-minus-120",negative->valid[0]&&std::abs(negative->angleDefect[0]+120)<1e-10);
        const QVector<Vec3> octaPoints{{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
        const QVector<QVector<int>> octaFaces{{0,2,4},{2,1,4},{1,3,4},{3,0,4},{2,0,5},{1,2,5},{3,1,5},{0,3,5}};
        auto octa=mesh(octaPoints,octaFaces);const auto positive=analyzeSurface(*octa);bool convex=true;double total=0;
        for(int i=0;i<6;++i){convex&=positive->valid[i]&&std::abs(positive->angleDefect[i]-120)<1e-10;total+=positive->angleDefect[i];}
        check("octahedron-deficit-120-at-all-vertices",convex);check("closed-sphere-total-angle-deficit-720",std::abs(total-720)<1e-9);
        check("smooth-normal-keeps-negative-y",std::abs(positive->normals[3].y+1)<1e-12);
        auto corners=mesh(octaPoints,octaFaces,3);auto corner=analyzeSurface(*corners);check("corner-uses-face-normal",std::abs(corner->triangles[0][0].normal.x-1/std::sqrt(3.))<1e-12);
        QVector<Vec3> cylinderPoints;QVector<QVector<int>> cylinderFaces;const double pi=std::acos(-1.);
        for(int z=0;z<3;++z)for(int j=0;j<16;++j)cylinderPoints.append({std::cos(2*pi*j/16),std::sin(2*pi*j/16),double(z)});
        for(int z=0;z<2;++z)for(int j=0;j<16;++j)cylinderFaces.append({z*16+j,z*16+(j+1)%16,(z+1)*16+(j+1)%16,(z+1)*16+j});
        auto cylinder=mesh(cylinderPoints,cylinderFaces);auto round=analyzeSurface(*cylinder);bool developable=true;
        for(int j=16;j<32;++j)developable&=round->valid[j]&&std::abs(round->angleDefect[j])<1e-10;
        check("cylinder-interior-is-developable",developable);
        auto points=planar->surfaceTopology->points;points+=saddle->surfaceTopology->points;auto faces=planar->surfaceTopology->faces;
        for(auto f:saddle->surfaceTopology->faces){for(int& i:f)i+=5;faces.append(f);}auto disconnected=mesh(points,faces);auto separate=analyzeSurface(*disconnected);
        check("coincident-disconnected-vertices-stay-distinct",separate->valid[0]&&separate->valid[5]&&std::abs(separate->angleDefect[0])<1e-10&&std::abs(separate->angleDefect[5]+120)<1e-10);
        for(int i=4;i<faces.size();++i)for(int& p:faces[i])if(p==5)p=0;auto bowtie=mesh(points,faces);
        check("disconnected-vertex-fans-are-unknown",!analyzeSurface(*bowtie)->valid[0]);
        auto badFaces=planar->surfaceTopology->faces;badFaces.append({0,1,3});auto bad=mesh(planar->surfaceTopology->points,badFaces);check("nonmanifold-edge-is-unknown",!analyzeSurface(*bad)->valid[0]);
        check("analysis-shared-for-same-immutable-topology",analyzeSurface(*planar)==plane);
        check("sign-colors-follow-original-spectrum",gaussianColor(120,-120,120)==QColor(255,0,0)&&gaussianColor(-120,-120,120).blue()==255&&gaussianColor(0,-120,120).green()==255);
        // The analytic cylinder above is intentionally open. For renderer checks,
        // add caps so the plan camera has a nonzero projected surface to sample.
        QVector<int> top,bottom;for(int i=0;i<16;++i){top.append(32+i);bottom.append(15-i);}auto cappedFaces=cylinderFaces;cappedFaces.append(top);cappedFaces.append(bottom);auto capped=mesh(cylinderPoints,cappedFaces);
        HullView view;view.setAttribute(Qt::WA_DontShowOnScreen);view.resize(800,600);view.setAnimationsEnabled(false);view.setProjection(HullProjection::Perspective);view.setSnapshot(capped);view.setGridVisible(false);view.setHydroVisible(false);view.setControlNetVisible(false);view.image();
        check("ordinary-render-does-not-build-analysis",!view.viewSettingsEvidence().value("analysis_built").toBool());
        QJsonArray comparisons;
        for(auto projection:{HullProjection::Profile,HullProjection::Plan,HullProjection::Body,HullProjection::Perspective}){
            view.setProjection(projection);view.fit();
            for(int mode:{1,2,3}){check(QString("select-%1-%2").arg(int(projection)).arg(mode),view.setShadeMode(HullShade(mode)));const auto cpu=view.softwareImage(),gpu=view.image();
                const auto name=QString("mode-%1-projection-%2").arg(mode).arg(int(projection));gpu.save(out+"/"+name+".png");
                if(view.gpuReady()){auto compared=compare(cpu,gpu);compared["mode"]=mode;compared["projection"]=int(projection);comparisons.append(compared);check(name+"-gpu-cpu",compared["samples"].toInt()>100&&compared["max_channel_error"].toInt()<=1);}
                const auto count=view.viewSettingsEvidence().value("fill_renders");for(int i=0;i<5;++i)view.image();check(name+"-cache",view.viewSettingsEvidence().value("fill_renders")==count);
            }
        }
        report["gpu_available"]=view.gpuReady();report["gpu_comparisons"]=comparisons;
        if(!view.gpuReady())cases.append(QJsonObject{{"id","gpu-comparisons"},{"status","not_run"},{"message","No OpenGL context on this Qt platform."}});
        else{
            HullRenderMesh render;render.extent=1;render.triangles.append({{{{-.8,-.8,0},{.8,-.8,0},{0,.8,0}}},{0,0,1},Qt::white});HullGpu gpu(render);
            SurfaceShadeTriangle attrs;attrs[0]={{0,0,1},Qt::red,Qt::red};attrs[1]={{0,0,1},Qt::green,Qt::green};attrs[2]={{0,0,1},Qt::blue,Qt::blue};check("gpu-analysis-upload",gpu.setAnalysis({attrs}));
            HullStyle style;style.mode=HullShade::Gaussian;const auto image=gpu.render({128,128},1,QMatrix4x4{},HullLighting{},style);const int x=71,y=63;
            const double nx=2*(x+.5)/128-1,ny=1-2*(y+.5)/128,w=(ny+.8)/1.6,v=(1-w+nx/.8)/2,u=1-v-w;const auto pixel=image.pixelColor(x,y);
            check("gpu-independent-vertex-color-interpolation",std::abs(pixel.red()-qRound(255*u))<=1&&std::abs(pixel.green()-qRound(255*v))<=1&&std::abs(pixel.blue()-qRound(255*w))<=1);
        }
        QFile sidecar(QString::fromLocal8Bit(argv[2]));if(!sidecar.open(QIODevice::ReadOnly))throw std::runtime_error("No surface artifact");const auto data=sidecar.readAll();auto actual=std::make_shared<fs::ModelSnapshot>(fs::ModelSnapshot::load(QString::fromLocal8Bit(argv[1])));actual->loadSurfaceDisplay(data);
        QElapsedTimer analysisTimer;analysisTimer.start();const auto actualAnalysis=analyzeSurface(*actual);report["analysis_build_ms"]=analysisTimer.nsecsElapsed()/1e6;
        check("actual-topology-bound-to-published-triangles",bool(actual->surfaceTopology)&&actualAnalysis->triangles.size()==actual->triangles.size());
        auto root=QJsonDocument::fromJson(data).object();auto topology=root["analysis_topology"].toObject();auto values=topology["points"].toArray();auto p=values[0].toArray();p[0]=p[0].toDouble()+1;values[0]=p;topology["points"]=values;root["analysis_topology"]=topology;
        const auto beforeTopology=actual->surfaceTopology;bool rejected=false;try{actual->loadSurfaceDisplay(QJsonDocument(root).toJson());}catch(const std::exception&){rejected=true;}check("tampered-topology-rejected",rejected&&actual->surfaceTopology==beforeTopology);
        view.setSnapshot(actual);view.setProjection(HullProjection::Perspective);view.setShadeMode(HullShade::Zebra);view.image();const auto built=analyzeSurface(*actual);QElapsedTimer timer;timer.start();
        for(int i=0;i<12;++i){view.panBy({2,1});view.image();}report["real_zebra_pan_mean_ms"]=timer.nsecsElapsed()/1e6/12;report["triangles"]=actual->triangles.size();check("navigation-reuses-analysis",analyzeSurface(*actual)==built);view.image().save(out+"/actual-zebra.png");
        QJsonArray realComparisons;const int bits=view.gpuReady()?rasterBits():0;report["raster_subpixel_bits"]=bits;
        // GL_SUBPIXEL_BITS describes device raster positioning precision (Khronos
        // glGet). Keep every discrepancy. A pixel may be reviewed separately only
        // inside that subpixel edge band AND if it chooses an adjacent CPU color.
        // Surface interiors keep the original one-channel-value comparison.
        const double edgeBand=bits>=4&&bits<=32?std::sqrt(2.)*std::ldexp(1.,-bits):0;
        for(int mode:{1,2,3}){view.setShadeMode(HullShade(mode));const auto cpu=view.softwareImage(),gpu=view.image();gpu.save(out+QString("/actual-mode-%1.png").arg(mode));cpu.save(out+QString("/actual-mode-%1-cpu.png").arg(mode));if(view.gpuReady()){
            auto result=compare(cpu,gpu);result["mode"]=mode;auto errors=result["errors"].toArray();int interiorError=std::min(1,result["max_channel_error"].toInt()),edgePixels=0;
            for(int i=0;i<errors.size();++i){auto e=errors[i].toObject();const int x=e["x"].toInt(),y=e["y"].toInt();const auto color=gpu.pixelColor(x,y);bool adjacent=false;
                for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx){const auto c=cpu.pixelColor(x+dx,y+dy);adjacent|=std::abs(color.red()-c.red())<=1&&std::abs(color.green()-c.green())<=1&&std::abs(color.blue()-c.blue())<=1;}
                const double distance=nearestEdge(view,*actual,{x+.5,y+.5});const bool edge=distance<=edgeBand&&adjacent;e["nearest_edge_px"]=distance;e["adjacent_cpu_color"]=adjacent;e["edge_review"]=edge;errors[i]=e;
                if(edge)++edgePixels;else interiorError=std::max(interiorError,e["error"].toInt());
            }
            result["errors"]=errors;result["max_interior_channel_error"]=interiorError;result["edge_review_pixels"]=edgePixels;result["edge_band_px"]=edgeBand;realComparisons.append(result);
            check("actual-gpu-cpu-interiors-"+QString::number(mode),result["samples"].toInt()-edgePixels>100&&errors.size()==result["pixels_over_one"].toInt()&&interiorError<=1);
            if(edgePixels)cases.append(QJsonObject{{"id","actual-raster-edges-"+QString::number(mode)},{"status","review"},{"message","Adjacent-face color choices inside the queried GL subpixel edge band; full differences are retained in actual_gpu_comparisons."},{"pixels",edgePixels}});
        }}
        report["actual_gpu_comparisons"]=realComparisons;
        cases.append(QJsonObject{{"id","desktop-visual-acceptance"},{"status","not_run"},{"message","Hidden renderer checks only."}});
    }catch(const std::exception& e){report["error"]=e.what();}
    report["cases"]=cases;report["status"]=report.contains("error")?"error":failed?"fail":"partial";QFile output(out+"/report.json");output.open(QIODevice::WriteOnly);output.write(QJsonDocument(report).toJson());return report.contains("error")?3:failed?1:2;
}
