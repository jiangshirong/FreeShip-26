// SPDX-License-Identifier: GPL-2.0-or-later
#include "file_history.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLockFile>
#include <QSaveFile>
#include <QStandardPaths>
namespace {
bool same(const QString& a,const QString& b){
#ifdef Q_OS_WIN
    return a.compare(b,Qt::CaseInsensitive)==0;
#else
    return a==b;
#endif
}
bool valid(const QString& path){return !path.isEmpty()&&path.size()<32768&&!path.contains(QChar(0))&&QFileInfo(path).isAbsolute();}
}
FileHistory::FileHistory(QString filename):filename_(std::move(filename)){}
QString FileHistory::defaultFilename(){
    const auto root=QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    return root.isEmpty()?QString{}:root+"/recent-files.json";
}
RecentFiles FileHistory::read()const{
    QFile file(filename_);RecentFiles result;
    if(file.size()>1024*1024||!file.open(QIODevice::ReadOnly))return result;
    const auto root=QJsonDocument::fromJson(file.readAll()).object();
    if(root["format"]!="FreeShipRecentFiles"||root["version"]!=1)return result;
    result.last=root["last"].toString();if(!valid(result.last))result.last.clear();
    for(const auto& item:root["recent"].toArray()){
        const auto path=item.toString();if(!valid(path))continue;
        bool found=false;for(const auto& old:result.paths)found|=same(old,path);
        if(!found)result.paths.append(path);if(result.paths.size()==10)break;
    }
    return result;
}
bool FileHistory::remember(const QString& path){return change(path,true);}
bool FileHistory::stopRestore(const QString& path){return change(path,false);}
bool FileHistory::change(const QString& path,bool remember){
    if(filename_.isEmpty()||!valid(path)||(remember&&!QFileInfo(path).isFile()))return false;
    if(!QDir().mkpath(QFileInfo(filename_).absolutePath()))return false;
    QLockFile lock(filename_+".lock");if(!lock.tryLock(0))return false;
    auto state=read();
    if(remember){
        state.last=path;
        for(auto it=state.paths.begin();it!=state.paths.end();)if(same(*it,path))it=state.paths.erase(it);else ++it;
        state.paths.prepend(path);while(state.paths.size()>10)state.paths.removeLast();
    }else{
        // Another running window may already have opened a different file.
        if(!same(state.last,path))return true;
        state.last.clear();
    }
    QJsonArray paths;for(const auto& recent:state.paths)paths.append(recent);
    const auto data=QJsonDocument(QJsonObject{{"format","FreeShipRecentFiles"},{"version",1},{"last",state.last},{"recent",paths}}).toJson();
    QSaveFile output(filename_);output.setDirectWriteFallback(false);
    return output.open(QIODevice::WriteOnly)&&output.write(data)==data.size()&&output.commit();
}
