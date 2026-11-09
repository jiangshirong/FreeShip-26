// SPDX-License-Identifier: GPL-2.0-or-later
// Retained production renderer probe; generated images are inspection evidence.
#include "render/model_preview.h"
#include <QApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <stdexcept>
int main(int argc,char** argv){
    QApplication app(argc,argv);if(argc!=5)return 3;const auto out=app.arguments()[4];QDir().mkpath(out);
    QJsonObject report;QJsonArray cases;bool failed=false;
    auto check=[&](const QString& id,bool ok){cases.append(QJsonObject{{"id",id},{"status",ok?"pass":"fail"}});failed|=!ok;};
    try{
        auto model=std::make_shared<fs::ModelSnapshot>(fs::ModelSnapshot::load(app.arguments()[1]));
        QFile surface(app.arguments()[2]),sections(app.arguments()[3]);
        if(!surface.open(QIODevice::ReadOnly)||!sections.open(QIODevice::ReadOnly))throw std::runtime_error("Missing published surface/section inputs");
        model->loadSurfaceDisplay(surface.readAll());model->loadSectionDisplay(sections.readAll());
        QJsonObject stages;QElapsedTimer clock;clock.start();const auto jpeg=modelPreview(model,{},&stages);report["preview_ms"]=clock.nsecsElapsed()/1e6;report["stages"]=stages;
        auto image=QImage::fromData(jpeg,"JPEG");check("jpeg-dimensions-and-bounded-payload",image.size()==QSize(400,300)&&jpeg.size()<512*1024);
        check("same-model-deterministic",modelPreview(model)==jpeg);
        bool visible=false;for(int y=0;y<image.height();++y)for(int x=0;x<image.width();++x)if(image.pixel(x,y)!=image.pixel(0,0))visible=true;
        check("nonempty-model-visible",model->points.isEmpty()||visible);
        QFile file(out+"/preview.jpg");check("preview-file",file.open(QIODevice::WriteOnly)&&file.write(jpeg)==jpeg.size());
        check("preview-evidence",image.save(out+"/preview.png"));
        const auto empty=std::make_shared<const fs::ModelSnapshot>();check("empty-document-preview",QImage::fromData(modelPreview(empty),"JPEG").size()==QSize(400,300));
        report["artifacts"]=QJsonObject{{"jpeg",out+"/preview.jpg"},{"image",out+"/preview.png"}};report["status"]=failed?"fail":"pass";
    }catch(const std::exception& e){report["status"]="error";report["error"]=e.what();}
    report["cases"]=cases;QFile file(out+"/report.json");if(!file.open(QIODevice::WriteOnly))return 3;file.write(QJsonDocument(report).toJson());return report["status"]=="pass"?0:report["status"]=="fail"?1:3;
}
