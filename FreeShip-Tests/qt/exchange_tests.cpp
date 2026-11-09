// GPL-2.0-or-later. Thin probe of the production codec; expectations are in
// qt_exchange.py and independent OBJ/STL/FEF readers, not this serializer.
#include "core/model_exchange.h"
#include "core/cad_export.h"
#include "core/ghs_export.h"
#include "core/grid_export.h"
#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QDir>
#include <QSaveFile>
int main(int argc,char**argv){
    QCoreApplication app(argc,argv);if(argc!=3)return 2;
    QFile input(app.arguments()[1]);if(!input.open(QIODevice::ReadOnly))return 3;
    const auto request=QJsonDocument::fromJson(input.readAll()).array();QJsonArray results;
    for(auto value:request){auto r=value.toObject();QJsonObject result{{"name",r["name"]}};
        try{
            fs::exchange::Options o;auto options=r["options"].toObject();o.units=options["units"].toString("m");o.legacyAxes=options["legacy_axes"].toBool();o.controlNet=options["control_net"].toBool();o.symmetric=options["symmetric"].toBool();o.fullHull=options["full_hull"].toBool(true);o.includeHidden=options["include_hidden"].toBool();o.binaryStl=options["binary_stl"].toBool(true);
            o.textEncoding=options["text_encoding"].toString("cp1252");for(auto v:options["face_ids"].toArray())o.faceIds.append(v.toInteger());
            o.columns=options["columns"].toInt(0);o.rows=options["rows"].toInt(10);o.intermediateBulkheads=options["intermediate_bulkheads"].toBool(true);
            if(r["method"]=="ghs_curve"||r["method"]=="waterline"){
                fs::native_geometry::SectionCurve c;for(auto v:r["points"].toArray()){auto p=v.toArray();c.points.push_back({float(p[0].toDouble()),float(p[1].toDouble()),float(p[2].toDouble())});}for(auto v:r["knuckles"].toArray())c.knuckles.push_back(v.toBool());
                QJsonArray points;if(r["method"]=="waterline"){for(auto y:fs::grid::sectionBreadths({c},float(r["z"].toDouble())))points.append(y);}else for(auto p:fs::ghs::sampleSection(c,r["tolerance"].toDouble()))points.append(QJsonArray{p[0],p[1]});result["points"]=points;
            }else if(r["method"]=="read"){auto d=fs::exchange::read(r["path"].toString(),r["format"].toString(),o);result["data"]=d.bridgeData();result["notes"]=QJsonArray::fromStringList(d.notes);}
            else{
                auto model=fs::ModelSnapshot::load(r["model"].toString());
                if(r.contains("sections")){QFile f(r["sections"].toString());if(!f.open(QIODevice::ReadOnly))throw std::runtime_error("Cannot read section artifact");model.loadSectionDisplay(f.readAll());}
                if(r["camera_half"].toBool())model.bothSides=false;
                if(fs::cad::supports(r["format"].toString())){
                    QJsonObject data;if(r.contains("patches")){QFile f(r["patches"].toString());if(!f.open(QIODevice::ReadOnly))throw std::runtime_error("Cannot read patch artifact");data=QJsonDocument::fromJson(f.readAll()).object();}
                    fs::cad::write(r["path"].toString(),r["format"].toString(),model,o,data);
                }else fs::exchange::write(r["path"].toString(),r["format"].toString(),model,o);
            }
            result["ok"]=true;
        }catch(const std::exception&e){result["ok"]=false;result["error"]=QString::fromUtf8(e.what());}
        results.append(result);
    }
    QSaveFile output(app.arguments()[2]);const auto bytes=QJsonDocument(results).toJson();if(!output.open(QIODevice::WriteOnly)||output.write(bytes)!=bytes.size()||!output.commit())return 4;return 0;
}
