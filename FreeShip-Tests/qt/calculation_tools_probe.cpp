// Regression probe; file checks do not claim that a calculator was executed.
#include "document/calculation_tools.h"
#include "app/preferences_store.h"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <iostream>
using namespace fs::document;
int main(int argc,char**argv){
    QCoreApplication app(argc,argv);if(argc!=2)return 2;
    auto root=QDir::cleanPath(QString::fromLocal8Bit(argv[1]));QDir().mkpath(root+"/软件 目录/Exec");QDir().mkpath(root+"/软件 目录/BD");
    QJsonArray cases;auto test=[&](const QString& name,bool ok){cases.append(QJsonObject{{"id",name},{"ok",ok}});};
    auto write=[&](const QString& name,const QByteArray& b){QFile f(root+"/"+name);if(!f.open(QIODevice::WriteOnly)||f.write(b)!=b.size())throw std::runtime_error("probe write failed");};
    const auto exec=root+"/软件 目录/Exec";
    test("selected-root",calculationToolDirectory(root+"/软件 目录")==exec);
    test("selected-exec",calculationToolDirectory(exec)==exec);
    write("calculation-tools.json",R"({"directory":"软件 目录"})");
    const auto old=QDir::currentPath();QDir::setCurrent(QDir::tempPath());
    test("config-relative-to-app",calculationToolDirectory({},root)==exec);QDir::setCurrent(old);
    write("calculation-tools.json","broken");test("invalid-config",calculationToolDirectory({},root).isEmpty());
    test("explicit-wins",calculationToolDirectory(exec,root)==exec);
    QFile::remove(root+"/calculation-tools.json");QDir().mkpath(root+"/CalculationTools/Exec");
    test("portable-discovery",calculationToolDirectory({},root)==root+"/CalculationTools/Exec");
    write("软件 目录/Exec/hship.EXE","fixture-file-not-executed");write("软件 目录/Exec/RESIST.CNF","database:BD\\");
    for(int i=1;i<=4;++i)write(QString("软件 目录/BD/RESIST%1.BD").arg(i),QByteArray::number(i));
    auto good=requireCalculationTool(exec,"hship.EXE");test("hship-database-complete",good["files"].toArray().size()==6);
    QFile::remove(root+"/软件 目录/BD/RESIST4.BD");bool rejected=false;try{requireCalculationTool(exec,"hship.EXE");}catch(const std::exception&){rejected=true;}test("missing-database-rejected",rejected);
    write("软件 目录/Exec/CALCPROP.EXE",{});test("empty-program-rejected",!inspectCalculationTool(exec,"CALCPROP.EXE")["ready"].toBool());
    auto inventory=inspectCalculationTools(exec);test("optional-viewer",!inventory["optional_database_viewer"].toObject()["ready"].toBool());
    write("软件 目录/Exec/Ishercof.exe","fixture-file-not-executed");
    test("wind-does-not-require-optional-viewer",requireCalculationTool(exec,"Ishercof.exe")["ready"].toBool());
    QFile::remove(root+"/软件 目录/Exec/Ishercof.exe");
    QString missingWind;try{requireCalculationTool(exec,"Ishercof.exe");}catch(const std::exception& e){missingWind=QString::fromUtf8(e.what());}
    test("wind-missing-tool-identifies-only-required-file",missingWind.contains("Ishercof.exe")&&!missingWind.contains("DBFview"));
    test("missing-tool-points-to-existing-preferences",missingWind.contains("首选项")&&!missingWind.contains("检查工具"));
    test("inventory-hash",good["files"].toArray()[0].toObject()["sha256"].toString().size()==64);
    const auto filename=root+"/preferences.json";PreferencesStore store(filename);store.read();
    store.save({{"theme","StudioLight"},{"directories",QJsonObject{{"solver","old"},{"save",root}}}});
    savePreferenceDirectory("solver",exec,filename);auto values=PreferencesStore(filename).read();
    test("settings-persist-and-live",values["directories"].toObject()["solver"]==exec&&preferenceDirectory("solver")==exec);
    test("settings-preserve-others",values["theme"]=="StudioLight"&&values["directories"].toObject()["save"]==root);
    // Integrity independently constructed from the documented compact JSON format.
    auto hash=[](QByteArray b){return QString::fromLatin1(QCryptographicHash::hash(b,QCryptographicHash::Sha256).toHex());};
    auto json=[](QJsonObject o){return QJsonDocument(o).toJson(QJsonDocument::Compact);};
    write("RESIST.dat","1 2 3");write("RESISTp.dat","header");QJsonObject payload{{"task1",QJsonObject{}}};
    write("RESIST-native.json",json({{"version",1},{"payload",payload},{"resist_sha256",hash("1 2 3")},{"resistp_sha256",hash("header")},{"payload_sha256",hash(json(payload))}}));
    validateCalculationCache(root);write("RESIST.dat","9 2 3");rejected=false;try{validateCalculationCache(root);}catch(const std::exception&){rejected=true;}test("resistance-tamper-rejected",rejected);
    write("RESIST.dat","1 2 3");QJsonObject inputs{{"Edit2",1}};write("Propres.dat","propeller");
    write("Propres-native.json",json({{"version",1},{"inputs",inputs},{"legacy_sha256",hash("propeller")},{"inputs_sha256",hash(json(inputs))}}));
    validateCalculationCache(root,true);write("Propres.dat","other");rejected=false;try{validateCalculationCache(root,true);}catch(const std::exception&){rejected=true;}test("propulsion-tamper-rejected",rejected);
    std::cout<<QJsonDocument(cases).toJson().toStdString();return 0;
}
