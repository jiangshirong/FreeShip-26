// SPDX-License-Identifier: GPL-2.0-or-later
#include "core/ghs_export.h"
#include "core/ship_formats.h"
#include "core/grid_export.h"
#include <QCoreApplication>
#include <QFile>
#include <QSaveFile>
#include <QJsonDocument>
#include <QElapsedTimer>
#include <QCryptographicHash>
#include <thread>
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
namespace {
QJsonObject read(const QString& path){QFile f(path);if(!f.open(QIODevice::ReadOnly)||f.size()>256*1024*1024)throw std::runtime_error("Cannot read geometry input");QJsonParseError e;auto d=QJsonDocument::fromJson(f.readAll(),&e);if(e.error!=QJsonParseError::NoError||!d.isObject())throw std::runtime_error("Invalid geometry input");return d.object();}
void write(const QString& path,const QByteArray& data){QSaveFile f(path);f.setDirectWriteFallback(false);if(!f.open(QIODevice::WriteOnly)||f.write(data)!=data.size()||!f.commit())throw std::runtime_error("Cannot commit export file");}
}
int main(int argc,char**argv){
    QCoreApplication app(argc,argv);if(argc!=3&&argc!=4)return 3;
#ifdef Q_OS_WIN
    if(argc==4){bool ok=false;auto pid=app.arguments()[3].toUInt(&ok);if(!ok||!pid)return 3;auto parent=OpenProcess(SYNCHRONIZE,FALSE,pid);if(!parent)return 3;
        std::thread([parent]{if(WaitForSingleObject(parent,INFINITE)==WAIT_OBJECT_0)TerminateProcess(GetCurrentProcess(),1);CloseHandle(parent);}).detach();}
#endif
    QElapsedTimer timer;timer.start();QJsonObject report{{"status","error"}};int exit=1;
    try{auto request=read(app.arguments()[1]);auto model=fs::ModelSnapshot::load(request["model"].toString());auto input=fs::native_geometry::DocumentInput::read(read(request["input"].toString()));
        if(request["document_id"]!=model.documentId||request["revision"].toDouble(-1)!=double(model.revision)||!request["close_deck"].isBool())throw std::runtime_error("Export request does not match document revision");
        auto format=request["format"].toString("ghs");
        if(request.contains("preview")&&!request["preview"].isBool())throw std::runtime_error("Invalid preview request");
        const bool previewRequested=request["preview"].toBool();QJsonObject preview;
        if(previewRequested&&format!="michlet"&&format!="addmass")throw std::runtime_error("This format has no offset preview");
        if(request.contains("references")&&!request["references"].isObject())throw std::runtime_error("Invalid export reference positions");
        auto result=format=="ghs"?fs::ghs::build(input,model,request["units"].toString(),request["close_deck"].toBool()):
            (format=="michlet"||format=="addmass"||format=="offset_table"||format=="dxfcrease")?fs::grid::build(input,model,format,request["units"].toString(),request["references"].toObject(),previewRequested?&preview:nullptr):fs::ship::build(input,model,format,request["units"].toString(),request["close_deck"].toBool(),request["references"].toObject());
        write(request["output"].toString(),result.data);report=result.diagnostics;report["status"]="pass";report["document_id"]=model.documentId;report["revision"]=double(model.revision);report["sha256"]=QString::fromLatin1(QCryptographicHash::hash(result.data,QCryptographicHash::Sha256).toHex());if(previewRequested)report["preview"]=preview;exit=0;
    }catch(const std::exception& e){report["error"]=QString::fromUtf8(e.what());}
    report["total_ms"]=timer.nsecsElapsed()/1e6;try{write(app.arguments()[2],QJsonDocument(report).toJson());}catch(...){return 2;}return exit;
}
