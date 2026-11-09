// Production protocol/formula entry points; independent expectations in Python.
#include "document/added_mass.h"
#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <iostream>
int main(int argc,char** argv){QCoreApplication app(argc,argv);if(argc!=2)return 2;QFile file(app.arguments()[1]);if(!file.open(QIODevice::ReadOnly))return 2;QJsonArray output;for(auto v:QJsonDocument::fromJson(file.readAll()).array()){auto r=v.toObject();QJsonObject result{{"name",r["name"]}};try{if(r["method"]=="parse"){QFile f(r["path"].toString());if(!f.open(QIODevice::ReadOnly))throw std::runtime_error("Missing fixture");result["result"]=fs::document::parseAddedMass(f.readAll(),r["values"].toObject());}else result["result"]=fs::document::addedMassEmpirical(r["type"].toInt(),r["L"].toDouble(),r["B"].toDouble(),r["T"].toDouble(),r["cb"].toDouble(),r["values"].toObject());result["ok"]=true;}catch(const std::exception& e){result["ok"]=false;result["error"]=QString::fromUtf8(e.what());}output.append(result);}std::cout<<QJsonDocument(output).toJson(QJsonDocument::Compact).constData();return 0;}
