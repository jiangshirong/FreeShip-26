// SPDX-License-Identifier: GPL-2.0-or-later
// Independent RGB expectations exercised through the actual CPU/GPU views.
#include "render/hull_view.h"
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <algorithm>
#include <cstdio>
#include <stdexcept>

static std::shared_ptr<fs::ModelSnapshot> fixture(int alpha,bool hydro=true,int layerAlpha=255){
    auto m=std::make_shared<fs::ModelSnapshot>();m->documentId=QString("water-%1-%2-%3").arg(alpha).arg(hydro).arg(layerAlpha);
    m->revision=1;m->units="metric";m->length=m->beam=2;m->draft=1;m->waterlineZ=3;
    m->minimum={0,0,0};m->maximum={2,2,2};m->bothSides=true;m->shadeUnderwater=true;
    m->underwaterColor=0xdc6414;m->underwaterColorAlpha=alpha;
    m->layers={{1,"Hull",0x143cb4,true,true}};m->layers[0].useInHydrostatics=hydro;m->layers[0].alphaBlend=layerAlpha;
    QVector<fs::Vec3> p{{0,0,0},{2,0,0},{2,2,0},{0,2,0},{0,0,2},{2,0,2},{2,2,2},{0,2,2}};
    QVector<QVector<int>> faces{{0,3,2,1},{4,5,6,7},{0,1,5,4},{1,2,6,5},{2,3,7,6},{3,0,4,7}};
    auto topology=std::make_shared<fs::SurfaceTopology>();topology->points=p;topology->faces=faces;topology->types.fill(3,p.size());m->surfaceTopology=topology;
    for(int i=0;i<p.size();++i)m->points.append({fs::EntityId(i+1),p[i]});
    for(const auto& f:faces){QVector<fs::EntityId> ids;for(auto i:f)ids.append(i+1);m->faces.append({fs::EntityId(m->faces.size()+1),1,ids});
        for(int i=2;i<f.size();++i)m->triangles.append({1,{{p[f[0]],p[f[i-1]],p[f[i]]}}});}
    return m;
}
int main(int argc,char** argv){
    QApplication app(argc,argv);if(argc!=2)return 3;const auto out=app.arguments()[1];QDir().mkpath(out);
    QJsonObject report;QJsonArray cases;bool failed=false;int gpuRuns=0;
    auto check=[&](QString name,bool good){cases.append(QJsonObject{{"id",name},{"status",good?"pass":"fail"}});failed|=!good;
        std::fprintf(stderr,"%s %s\n",good?"pass":"fail",name.toUtf8().constData());std::fflush(stderr);};
    try{
        for(int alpha:{0,127,128,234,255})for(bool hydro:{false,true}){
            const auto m=fixture(alpha,hydro);
            // Independent integer expectation, including deliberately different G/B.
            const QColor expected=hydro?QColor((180*(255-alpha)+20*alpha)/255,(60*(255-alpha)+100*alpha)/255,(20*(255-alpha)+220*alpha)/255):QColor(180,60,20);
            for(auto projection:{HullProjection::Profile,HullProjection::Plan,HullProjection::Body,HullProjection::Perspective}){
                HullView view;view.resize(480,360);view.setAnimationsEnabled(false);view.setControlNetVisible(false);view.setGridVisible(false);view.setHydroVisible(false);
                view.setSnapshot(m);view.setProjection(projection);view.fit();
                // Projection selection installs its own lighting defaults.
                HullLight light;light.enabled=false;view.setLight(light);view.setGridVisible(false);view.setHydroVisible(false);
                auto cpu=view.thumbnailImage();const QString key=QString("a%1-h%2-p%3").arg(alpha).arg(hydro).arg(int(projection));int pixels=0;
                for(int y=0;y<cpu.height();++y)for(int x=0;x<cpu.width();++x)if(cpu.pixelColor(x,y)==expected)++pixels;
                check(key+".cpu-color",pixels>1000);
                const auto gpu=view.image(); // First draw lazily creates the GL renderer.
                if(view.gpuReady()){
                    ++gpuRuns;check(key+".gpu-used",view.viewSettingsEvidence().value("gpu").toBool());int tested=0,wrong=0;
                    for(int y=40;y<cpu.height()-55;y+=3)for(int x=5;x<cpu.width()-5;x+=3){
                        bool inside=true;for(QPoint d:{QPoint{},QPoint(-2,0),QPoint(2,0),QPoint(0,-2),QPoint(0,2)})inside&=cpu.pixelColor(QPoint(x,y)+d)==expected;
                        if(!inside)continue;++tested;const auto c=gpu.pixelColor(x,y);
                        if(std::max({std::abs(c.red()-expected.red()),std::abs(c.green()-expected.green()),std::abs(c.blue()-expected.blue())})>1)++wrong;
                    }
                    check(key+".gpu-color",tested>100&&wrong==0);
                    if(alpha==127&&hydro&&projection==HullProjection::Profile)check("gpu-evidence",gpu.save(out+"/gpu.png"));
                }
                view.panBy({9,7});view.zoomAt(view.project({.37,2,.4}),1.1);check(key+".camera-keeps-geometry",m->revision==1&&m->triangles.size()==12);
                const auto one=view.softwareImage();const auto count=view.viewSettingsEvidence().value("fill_renders");view.softwareImage();
                check(key+".cached-repaint",count==view.viewSettingsEvidence().value("fill_renders")&&one==view.softwareImage());
            }
        }
        auto m=fixture(127);m->waterlineZ=1;HullView view;view.resize(600,400);view.setSnapshot(m);view.setProjection(HullProjection::Profile);view.fit();
        auto image=view.thumbnailImage();check("waterline.wet",image.pixelColor(view.project({.37,2,.4}).toPoint())==QColor(100,79,119));
        check("waterline.dry",image.pixelColor(view.project({.37,2,1.6}).toPoint())==QColor(180,60,20));check("waterline-evidence",image.save(out+"/waterline.png"));
        m=fixture(0);view.setSnapshot(m);view.fit();const auto opaque=view.thumbnailImage();
        auto transparent=fixture(0,true,0);view.setSnapshot(transparent);check("water-opacity-zero-is-not-transparency",view.thumbnailImage()!=opaque);
        for(auto mode:{HullShade::Gaussian,HullShade::Developable,HullShade::Zebra}){
            view.setSnapshot(fixture(0));view.fit();check("analysis-mode-available",view.setShadeMode(mode));const auto first=view.softwareImage();
            view.setSnapshot(fixture(255));view.fit();check("analysis-colors-independent-of-water-opacity",view.softwareImage()==first);
        }
        report["gpu_runs"]=gpuRuns;if(!gpuRuns)cases.append(QJsonObject{{"id","gpu-unavailable"},{"status","not_run"}});
        report["status"]=failed?"fail":"pass";
    }catch(const std::exception& e){report["status"]="error";report["error"]=e.what();}
    report["cases"]=cases;QFile file(out+"/report.json");if(!file.open(QIODevice::WriteOnly))return 3;file.write(QJsonDocument(report).toJson());return report["status"]=="pass"?0:1;
}
