// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

namespace fs::document::report_language {
// Only caption-table or explicitly owned native literals enter this translator. Completed report lines,
// numeric values, model names, paths and external solver output never do.
struct Catalog {QString id;QJsonObject selected,english;};
inline thread_local const Catalog* active=nullptr;
inline QJsonObject read(const QString& id){
    QFile file(QCoreApplication::applicationDirPath()+"/languages/"+id+".json");
    if(!file.open(QIODevice::ReadOnly)||file.size()>4*1024*1024)return {};
    const auto object=QJsonDocument::fromJson(file.readAll()).object();
    return object["format"]=="FreeShipQtLanguage"&&object["id"]==id?object["messages"].toObject():QJsonObject{};
}
class Scope {
    Catalog catalog_;const Catalog* previous_=active;
public:
    explicit Scope(const QJsonObject& request){
        catalog_.id=request["report_language"].toString("zh");
        if(!QRegularExpression("^[a-z]{2,3}(-[A-Za-z0-9]{2,8})?$").match(catalog_.id).hasMatch())catalog_.id="zh";
        if(catalog_.id=="zh"){
            catalog_.selected=read("zh");
        }else{
            catalog_.english=read("en");catalog_.selected=catalog_.id=="en"?catalog_.english:read(catalog_.id);
        }
        active=&catalog_;
    }
    ~Scope(){active=previous_;}
    Scope(const Scope&)=delete;Scope& operator=(const Scope&)=delete;
};
inline QString text(const QString& source,const QString& context){
    if(!active)return source;
    const auto key=context+QChar(4)+source;
    if(active->id=="zh"){
        auto value=active->selected[key].toString();
        if(value.isEmpty())value=active->selected[source].toString();
        return value.isEmpty()?source:value;
    }
    auto value=active->selected[key].toString();if(value.isEmpty())value=active->english[key].toString();
    if(value.isEmpty())value=active->selected[source].toString();if(value.isEmpty())value=active->english[source].toString();
    return value.isEmpty()?source:value;
}
}
