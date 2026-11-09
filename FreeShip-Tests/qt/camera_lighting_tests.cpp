// SPDX-License-Identifier: GPL-2.0-or-later
// Independent pinhole, Lambert and cache checks against production renderers.
#include "render/hull_view.h"
#include "render/hull_gpu.h"
#include <QApplication>
#include <QAction>
#include <QContextMenuEvent>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMenu>
#include <QLineF>
#include <QElapsedTimer>
#include <cmath>
#include <cstdio>
#include <limits>

static std::shared_ptr<fs::ModelSnapshot> fixture(){
    auto m=std::make_shared<fs::ModelSnapshot>();m->documentId="camera-light-independent";m->revision=1;m->units="metric";m->length=m->beam=2;m->draft=1;m->bothSides=false;m->shadeUnderwater=false;m->minimum={-1,-1,-1};m->maximum={1,1,1};
    m->layers={{1,"Hull",0x3c78b4,true,false}};
    m->points={{1,{-1,-1,-1}},{2,{1,-1,-1}},{3,{1,1,-1}},{4,{-1,1,-1}},{5,{-1,-1,1}},{6,{1,-1,1}},{7,{1,1,1}},{8,{-1,1,1}}};
    m->faces={{1,1,{1,4,3,2}},{2,1,{5,6,7,8}},{3,1,{1,2,6,5}},{4,1,{2,3,7,6}},{5,1,{3,4,8,7}},{6,1,{4,1,5,8}}};
    for(const auto& f:m->faces)for(int i=2;i<f.points.size();++i)m->triangles.append({1,{{m->point(f.points[0])->position,m->point(f.points[i-1])->position,m->point(f.points[i])->position}}});return m;
}
// Skip antialiased boundaries and labels; compare constant face interiors.
static QJsonObject compareImages(const QImage& cpu,const QImage& gpu){
    int maxError=0,samples=0;const auto background=cpu.pixelColor(2,2);
    for(int y=30;y<cpu.height()-3;y+=3)for(int x=3;x<cpu.width()-3;x+=3){auto a=cpu.pixelColor(x,y),b=gpu.pixelColor(x,y);bool interior=a!=background;
        for(QPoint d:{QPoint(-2,0),QPoint(2,0),QPoint(0,-2),QPoint(0,2)})interior&=cpu.pixelColor(QPoint(x,y)+d)==a;
        if(interior){++samples;maxError=std::max({maxError,std::abs(a.red()-b.red()),std::abs(a.green()-b.green()),std::abs(a.blue()-b.blue())});}
    }
    return {{"samples",samples},{"max_channel_error",maxError}};
}
int main(int argc,char** argv){
    QApplication app(argc,argv);if(argc!=3)return 3;const QString out=QString::fromLocal8Bit(argv[2]);QDir().mkpath(out);QJsonObject report;QJsonArray cases;bool failed=false;
    auto check=[&](QString id,bool pass){cases.append(QJsonObject{{"id",id},{"status",pass?"pass":"fail"}});failed|=!pass;std::fprintf(stderr,"%s %s\n",pass?"pass":"fail",id.toUtf8().constData());std::fflush(stderr);};
    try{
        HullLighting light;light.enabled=true;light.position={0,0,10};light.ambient=.2;light.intensity=.6;
        auto near=[](double a,double b){return std::abs(a-b)<1e-9;};
        check("lambert-front",near(light.factor({0,0,1},{}),.8));check("lambert-back-is-two-sided",near(light.factor({0,0,-1},{}),.8));check("lambert-tangent",near(light.factor({1,0,0},{}),.2));
        check("finite-point-light-uses-face-center",near(light.factor({0,0,1},{std::sqrt(3.),0,9}),.5));light.ambient=.8;light.intensity=.8;check("brightness-saturates",light.factor({0,0,1},{})==1);light.enabled=false;check("disabled-light-preserves-color",light.factor({0,0,1},{})==1);
        auto m=fixture();HullView view,other;for(auto* v:{&view,&other}){v->setAttribute(Qt::WA_DontShowOnScreen);v->resize(800,600);v->setAnimationsEnabled(false);v->setSnapshot(m);v->setGridVisible(false);v->setHydroVisible(false);v->setControlNetVisible(false);}
        view.setProjection(HullProjection::Perspective);const auto baseline=view.project({1,1,1});
        const double pi=std::acos(-1.),yaw=-30*pi/180,pitch=25*pi/180;
        fs::Vec3 right{std::cos(yaw),std::sin(yaw),0},depth{std::sin(yaw)*std::cos(pitch),-std::cos(yaw)*std::cos(pitch),std::sin(pitch)};
        const auto point=[&](double z){return fs::Vec3{.4*right.x+z*depth.x,.4*right.y+z*depth.y,.4*right.z+z*depth.z};};
        QJsonArray ratios,lensComparisons;
        for(int focal:{28,50,90,130,200}){
            check("lens-supported-"+QString::number(focal),view.setFocalLength(focal));const auto origin=view.project({});const double ratio=QLineF(origin,view.project(point(.5))).length()/QLineF(origin,view.project(point(-.5))).length();
            const double distance=6.*focal/50.;check("pinhole-depth-ratio-"+QString::number(focal),near(ratio,(distance+.5)/(distance-.5)));ratios.append(ratio);
            bool contained=true;for(const auto& p:m->points)contained&=view.rect().contains(view.project(p.position).toPoint());check("lens-fit-contains-hull-"+QString::number(focal),contained);
            const auto cpu=view.softwareImage();cpu.save(out+"/lens-"+QString::number(focal)+".png");const auto gpu=view.image();
            if(view.gpuReady()){auto result=compareImages(cpu,gpu);result["focal_mm"]=focal;lensComparisons.append(result);check("gpu-software-lens-"+QString::number(focal),result["samples"].toInt()>100&&result["max_channel_error"].toInt()<=1);}
        }
        report["pinhole_ratios"]=ratios;report["lens_gpu_comparisons"]=lensComparisons;view.setFocalLength(50);check("standard-lens-restores-accepted-projection",QLineF(baseline,view.project({1,1,1})).length()<1e-9);
        view.panBy({17,31});const auto p=view.project({1,1,1});const auto camera=view.viewSettingsEvidence().value("camera_revision");view.setFocalLength(50);check("same-lens-does-not-refit",view.project({1,1,1})==p&&view.viewSettingsEvidence().value("camera_revision")==camera);
        view.zoomAt(p,1.4);check("zoom-preserves-picked-anchor-at-any-lens",QLineF(p,view.project({1,1,1})).length()<1e-9);
        check("invalid-lens-is-rejected",!view.setFocalLength(0)&&!view.setFocalLength(20)&&view.focalLength()==50);
        other.setProjection(HullProjection::Plan);auto unchanged=other.softwareImage();check("orthographic-lens-is-inapplicable",!other.setFocalLength(28)&&other.softwareImage()==unchanged);
        view.setProjection(HullProjection::Plan);view.fit();auto original=view.softwareImage();const auto at=view.project({.2,.3,1}).toPoint();check("orthographic-default-color-preserved",original.pixelColor(at)==QColor(180,120,60));
        HullLight settings;settings.enabled=true;settings.direction={0,0,0};settings.ambient=51;settings.intensity=153;const auto revision=view.viewSettingsEvidence().value("camera_revision");
        view.setLight(settings);auto ambient=view.softwareImage();check("zero-direction-is-finite-ambient-only",ambient.pixelColor(at)==QColor(36,24,12));
        check("light-does-not-change-projection",view.viewSettingsEvidence().value("camera_revision")==revision);check("light-is-view-local",other.softwareImage()==unchanged);
        auto count=view.viewSettingsEvidence().value("fill_renders");view.setLight(settings);for(int i=0;i<5;++i)view.softwareImage();check("no-op-and-repaint-reuse-fill",view.viewSettingsEvidence().value("fill_renders")==count);
        auto invalid=settings;invalid.direction.x=std::numeric_limits<double>::quiet_NaN();check("nan-light-rejected",!view.setLight(invalid)&&view.light()==settings);invalid=settings;invalid.intensity=256;check("out-of-range-light-rejected",!view.setLight(invalid)&&view.light()==settings);
        settings.ambient=0;settings.intensity=0;view.setLight(settings);check("zero-light-is-black",view.softwareImage().pixelColor(at)==Qt::black);
        settings.direction={100,20,50};settings.ambient=64;settings.intensity=192;view.setLight(settings);auto lit=view.softwareImage();lit.save(out+"/light.png");check("directional-light-changes-shading",lit!=ambient);
        auto beforePan=lit.pixelColor(at);view.panBy({10,20});check("pan-keeps-light-on-same-surface",view.softwareImage().pixelColor(view.project({.2,.3,1}).toPoint())==beforePan);
        view.zoomAt(view.project({.2,.3,1}),1.3);check("zoom-keeps-light-on-same-surface",view.softwareImage().pixelColor(view.project({.2,.3,1}).toPoint())==beforePan);
        view.setProjection(HullProjection::Perspective);view.fit();settings.enabled=true;view.setLight(settings);
        const auto cpu=view.softwareImage(),gpu=view.image();report["gpu_available"]=view.gpuReady();
        if(view.gpuReady()){
            const auto comparison=compareImages(cpu,gpu);const int maxError=comparison["max_channel_error"].toInt(),samples=comparison["samples"].toInt();
            report["gpu_cpu_samples"]=samples;report["gpu_cpu_max_channel_error"]=maxError;check("gpu-software-light-and-camera-agree",samples>100&&maxError<=1);gpu.save(out+"/gpu.png");
            HullRenderMesh mesh;mesh.minimum={-1,-1,0};mesh.maximum={1,1,0};mesh.center={};mesh.extent=1;mesh.triangles.append({{{{-0.8,-0.8,0},{0.8,-0.8,0},{0,1.6,0}}},{0,0,1},QColor(180,120,60)});
            HullGpu exact(mesh);HullLighting analytic;analytic.enabled=true;analytic.position={0,0,10};analytic.ambient=.2;analytic.intensity=.6;
            auto pixels=exact.render({128,128},1,QMatrix4x4{},analytic);check("gpu-independent-flat-lambert-value",!pixels.isNull()&&pixels.pixelColor(64,64)==QColor(144,96,48));
        }else cases.append(QJsonObject{{"id","gpu-comparison"},{"status","not_run"},{"message","OpenGL context unavailable on this Qt platform."}});
        auto actual=std::make_shared<fs::ModelSnapshot>(fs::ModelSnapshot::load(QString::fromLocal8Bit(argv[1])));view.setSnapshot(actual);view.fit();view.setFocalLength(90);view.image();report["model_triangle_count"]=actual->triangles.size();report["image_size"]=QJsonArray{view.width(),view.height()};
        QElapsedTimer clock;clock.start();for(int i=0;i<12;++i){auto current=view.light();current.direction.y=i*10-50;view.setLight(current);view.image();}report["light_update_mean_ms"]=clock.nsecsElapsed()/1e6/12;
        const auto before=view.viewSettingsEvidence().value("fill_renders");clock.restart();for(int i=0;i<30;++i)view.image();report["cached_paint_mean_ms"]=clock.nsecsElapsed()/1e6/30;check("real-model-light-cache-reused",view.viewSettingsEvidence().value("fill_renders")==before);
        check("render-settings-keep-immutable-model",view.snapshot()==actual&&m->revision==1);view.image(2).save(out+"/real-hidpi.png");check("hidpi-render-size",view.image(2).size()==QSize(1600,1200));
        cases.append(QJsonObject{{"id","desktop-and-visual-acceptance"},{"status","not_run"},{"message","Hidden renderer evidence; no physical desktop input."}});
    }catch(const std::exception& e){report["error"]=e.what();}
    report["status"]=report.contains("error")?"error":failed?"fail":"partial";report["cases"]=cases;QFile file(out+"/report.json");file.open(QIODevice::WriteOnly);file.write(QJsonDocument(report).toJson());return report.contains("error")?3:failed?1:2;
}
