// GPL-2.0-or-later. New hull template/sampling and import initialization from
// current Plus Model_New, NewHull.inc and ExchangeImport.inc. Original authors
// Martijn van Engeland, Timoshenko V.F. and Mark Malakanov; FreeShip_26 adaptation.
#include "document_create.h"
#include "geometry_input.h"
#include "project_edit.h"
#include "definition_edit.h"
#include "attribute_edit.h"
#include "fbm_text.h"
#include "native/geometry.h"
#include "native/sections.h"
#include <QJsonArray>
#include <QtEndian>
#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <stdexcept>

namespace fs::document {
namespace {
[[noreturn]] void bad(const char *s){throw std::invalid_argument(s);}
float real(QJsonValue v) {const double x=v.toDouble();if(!v.isDouble()||!std::isfinite(x)||std::abs(x)>1e9)bad("Invalid document dimension");return float(x);}
int integer(QJsonValue v,int low,int high){const double n=v.toDouble();if(!v.isDouble()||!std::isfinite(n)||n<low||n>high||std::floor(n)!=n)bad("Invalid document integer");return int(n);}
bool boolean(QJsonValue v){if(!v.isBool())bad("Invalid document boolean");return v.toBool();}
QJsonArray array(QJsonValue v,int minimum,int maximum){if(!v.isArray())bad("Expected document array");auto a=v.toArray();if(a.size()<minimum||a.size()>maximum)bad("Invalid document array size");return a;}
QByteArray int32(int n){QByteArray bytes(4,0);qToLittleEndian<qint32>(n,bytes.data());return bytes;}
QByteArray text(QJsonValue v,const QJsonObject& c){if(!v.isString())bad("Invalid document text");auto bytes=encodeFbmText(v.toString(),c["fbm_encoding"].toString());return int32(bytes.size())+bytes;}
Fbm creator(const Fbm& input,QJsonValue value,bool particulars,const QJsonObject& context){
    auto bytes=input.blockBytes("project_settings");qsizetype at=0;
    auto skipString=[&]{auto n=qFromLittleEndian<qint32>(bytes.constData()+at);at+=4+n;};
    skipString();skipString();bytes[at+12]=char(particulars);at+=36;skipString();
    auto count=qFromLittleEndian<qint32>(bytes.constData()+at);
    bytes.replace(at,4+count,text(value,context));return input.withBlocks({{"project_settings",bytes}});
}
const native_geometry::Vec3 HullTemplate[7][5]={
    {{0,0,1.56754f},{0,.05280f,1.59170f},{0,.22171f,1.77284f},{0,.28506f,2.64108f},{0,.29135f,3.48932f}},
    {{.20880f,0,.49656f},{.20881f,.18796f,.53622f},{.20880f,.33700f,.97840f},{.20880f,.45607f,2.05422f},{.20882f,.47184f,3.44280f}},
    {{.41765f,0,0},{.41765f,.23565f,.07524f},{.41765f,.41555f,.67735f},{.41765f,.49421f,1.91004f},{.41737f,.51468f,3.45474f}},
    {{.58471f,0,0},{.58472f,.24072f,.02507f},{.58472f,.39528f,.71080f},{.58488f,.45356f,2.04881f},{.58472f,.46756f,3.54662f}},
    {{.75179f,0,.28284f},{.75178f,.13715f,.44098f},{.75179f,.20950f,.87760f},{.75179f,.30538f,2.38232f},{.75177f,.34473f,3.67786f}},
    {{.90672f,0,.81860f},{.90681f,.01887f,.98650f},{.90658f,.04671f,1.29873f},{.90637f,.11195f,2.83107f},{.90672f,.14523f,3.81697f}},
    {{.91580f,0,.85643f},{.92562f,0,1.17444f},{.93387f,0,1.44618f},{.97668f,0,3.03482f},{1,0,3.91366f}}
};
}
TopologyEdit newDocument(const QJsonObject &request,const QJsonObject &context) {
    const float length=real(request["length"]),beam=real(request["beam"]),draft=real(request["draft"]);
    if(length<=0||beam<=0||draft<=0)bad("Hull dimensions must be positive");
    const int columns=integer(request["columns"],2,100),rows=integer(request["rows"],2,100),units=integer(request["units"],0,1);
    auto seed=emptyDocument(context);auto bindings=documentBindings(seed,context);
    auto settings=editProject(seed,bindings,{{"method","update_project"},{"changes",QJsonObject{
        {"length",double(length)},{"beam",double(beam)},{"draft",double(draft)},{"units",units?"imperial":"metric"},{"precision",1},{"save_preview",true}}}});
    std::vector<native_geometry::Spline> transverse;
    for(auto &station:HullTemplate){std::vector<native_geometry::Vec3> points;for(auto p:station)points.push_back({p.x*length,p.y*beam,p.z*draft});transverse.emplace_back(points);}
    std::vector<FbmPosition> points;
    for(int i=0;i<rows;++i){std::vector<native_geometry::Vec3> row;
        for(auto &s:transverse)row.push_back(s.value(double(i)/(rows-1)));
        native_geometry::Spline longitudinal(row);
        for(int j=0;j<columns;++j){auto p=longitudinal.value(double(j)/(columns-1));points.push_back({p.x,p.y,p.z});}
    }
    auto result=constructHullNet(settings.document,bindings,points,columns,rows);
    auto geometry=documentGeometry(result.document);native_geometry::SubdivisionSurface surface(geometry);
    const auto &mesh=surface.level(geometry.desiredLevel);native_geometry::SectionIndex index(geometry,mesh);
    FbmPosition lo=points.front(),hi=lo;for(auto p:points){lo.x=std::min(lo.x,p.x);lo.y=std::min(lo.y,p.y);lo.z=std::min(lo.z,p.z);hi.x=std::max(hi.x,p.x);hi.y=std::max(hi.y,p.y);hi.z=std::max(hi.z,p.z);}
    for(int kind=1;kind<=3;++kind){QJsonArray positions;int divisions=kind==1?20:kind==2?7:10;float span=kind==1?hi.x-lo.x:kind==2?hi.y-lo.y:hi.z-lo.z;
        for(int i=kind==2?1:0;i<=(kind==2?6:divisions);++i){float d=float(double(i)/divisions*span);
            native_geometry::Plane plane(kind==1?1:0,kind==2?1:0,kind==3?1:0,-d);
            if(!index.intersect(plane).curves.empty())positions.append(double(d));
        }
        if(!positions.isEmpty())result=editDefinitions(result.document,result.bindings,{{"method","sections"},{"kind",kind},{"operation","add"},{"positions",positions}});
    }
    result.changed=true;return result;
}

TopologyEdit importDocument(const QJsonObject &root,const QJsonObject &context) {
    if(root["format"]!="FreeShipExchange"||integer(root["version"],1,1)!=1)bad("Unsupported model exchange");
    const auto ps=array(root["points"],1,100000),es=array(root["edges"],1,300000),fs=array(root["faces"],1,100000),ls=array(root["layers"],1,1000);
    if(!root["project"].isObject())bad("Invalid exchange project");
    auto document=emptyDocument(context);auto bindings=documentBindings(document,context);
    document=editProject(document,bindings,{{"method","update_project"},{"changes",root["project"]}}).document;
    document=creator(document,root["creator"],boolean(root["particulars_set"]),context);
    for(int i=0;i<ls.size();++i){
        if(!ls[i].isObject())bad("Invalid exchange layer");
        if(i){auto added=editDefinitions(document,bindings,{{"method","manage_layers"},{"operation","create"},{"changes",QJsonObject{}}});document=std::move(added.document);bindings=added.bindings;}
        auto edited=editDefinitions(document,bindings,{{"method","update_layer"},{"layer_id",i+1},{"changes",ls[i]}});document=std::move(edited.document);bindings=edited.bindings;
    }
    FbmControlNet net;
    for(auto value:ps){if(!value.isObject())bad("Invalid exchange point");auto p=value.toObject();auto a=array(p["p"],3,3);FbmControlPoint v;v.x=real(a[0]);v.y=real(a[1]);v.z=real(a[2]);v.vertexType=integer(p["vertex_type"],0,3);if(p.contains("locked"))v.locked=boolean(p["locked"]);net.points.push_back({v,-1});}
    using Key=std::pair<int,int>;std::map<Key,int> uses;
    for(auto value:es){auto e=array(value,3,3);int a=integer(e[0],0,ps.size()-1),b=integer(e[1],0,ps.size()-1);if(a==b||!uses.emplace(std::minmax(a,b),0).second)bad("Repeated exchange edge");net.edges.push_back({a,b,boolean(e[2]),false,-1});}
    qint64 corners=0;
    for(auto value:fs){if(!value.isObject())bad("Invalid exchange face");auto f=value.toObject();auto p=array(f["points"],3,4096);corners+=p.size();if(corners*2*(qint64(1)<<(2*document.precision()))>1000000)bad("Exchange subdivision exceeds one million triangles");
        FbmNetFace face;face.layer=integer(f["layer"],0,ls.size()-1);std::set<int> seen;
        for(auto point:p){int id=integer(point,0,ps.size()-1);if(!seen.insert(id).second)bad("Repeated exchange face point");face.points.push_back(id);}
        for(size_t i=0;i<face.points.size();++i){auto it=uses.find(std::minmax(face.points[i],face.points[(i+1)%face.points.size()]));if(it==uses.end()||++it->second>2)bad("Missing or non-manifold exchange edge");}
        net.faces.push_back(std::move(face));
    }
    if(root.contains("control_curves")){
        std::set<Key> assigned;
        for(auto value:array(root["control_curves"],0,4096)){
            auto points=array(value,2,100000);FbmNetCurve curve;std::set<int> visited;
            for(auto point:points){const int id=integer(point,0,ps.size()-1);if(!visited.insert(id).second)bad("Repeated exchange curve point");
                if(!curve.points.empty()){const auto key=std::minmax(curve.points.back(),id);if(!uses.count(key)||!assigned.insert(key).second)bad("Missing or reused exchange curve edge");}
                curve.points.push_back(id);
            }
            net.curves.push_back(std::move(curve));
        }
    }
    // Boundary/free edges are always creases in the old public setter. Point
    // vertex types are restored from the file after that initialization.
    for(auto &e:net.edges)if(uses.at(std::minmax(e.a,e.b))<2)e.crease=true;
    document=document.withControlNet(net);bindings=documentBindings(document,context);
    if(root.contains("markers")){
        const auto paths=array(root["markers"],0,4096);
        if(!paths.isEmpty())document=editAttributes(document,bindings,{{"method","modeling"},{"operation","add_markers"},{"paths",paths}}).document;
    }
    if(root.contains("stations")){
        const auto positions=array(root["stations"],0,10000);
        if(!positions.isEmpty())document=editDefinitions(document,bindings,{{"method","sections"},{"kind",1},{"operation","add"},{"positions",positions}}).document;
    }
    return {document,bindings,true};
}
TopologyEdit appendDocument(const Fbm& source,const QJsonObject& context,const QJsonObject& root){
    auto incoming=importDocument(root,context);const auto settings=documentGeometrySettings(source);
    // Reuse complete native unit conversion, including material density and
    // thickness (mm/inches), rather than converting coordinates alone.
    auto converted=editProject(incoming.document,incoming.bindings,{{"method","update_project"},{"changes",QJsonObject{{"units",settings.imperial?"imperial":"metric"}}}});
    auto addition=converted.document.controlNet();auto net=source.controlNet();auto document=source;auto bindings=context;
    if(root["wave_patch"].toBool()){
        const float bottom=native_geometry::hydrostaticKeel(documentGeometry(source));
        double originX=settings.length*.5,originY=0;
        if(root.contains("wave_origin_m")){
            if(!root["wave_origin_m"].isObject())bad("Invalid wave origin");
            auto origin=root["wave_origin_m"].toObject();real(origin["x"]);real(origin["y"]);
            const double metres=settings.imperial?.3048:1.;originX=origin["x"].toDouble()/metres;originY=origin["y"].toDouble()/metres;
        }
        for(auto& p:addition.points){p.value.x=real(originX-p.value.x);p.value.y=real(originY+p.value.y);p.value.z=real(bottom+settings.draft+p.value.z);}
        // Mapping X to -X changes handedness; reverse winding to retain normals.
        for(auto& f:addition.faces)std::reverse(f.points.begin(),f.points.end());
    }
    const int pointOffset=net.points.size(),layerOffset=bindings["layers"].toArray().size();
    if(pointOffset+addition.points.size()>100000||net.faces.size()+addition.faces.size()>100000)bad("Combined document geometry exceeds import limits");
    qint64 corners=0;for(const auto& f:net.faces)corners+=f.points.size();for(const auto& f:addition.faces)corners+=f.points.size();
    if(corners*2*(qint64(1)<<(2*source.precision()))>1000000)bad("Combined document subdivision exceeds one million triangles");
    auto layers=root["layers"].toArray();const bool incomingImperial=root["project"].toObject()["units"]=="imperial";
    for(auto value:layers){auto changes=value.toObject();if(incomingImperial!=settings.imperial){double density=1000*.3048*.3048*.3048/.45359237;
            changes["material_density"]=changes["material_density"].toDouble()*(settings.imperial?density:1/density);changes["thickness"]=changes["thickness"].toDouble()*(settings.imperial?1/25.4:25.4);}
        auto added=editDefinitions(document,bindings,{{"method","manage_layers"},{"operation","create"},{"changes",changes}});document=std::move(added.document);bindings=added.bindings;
    }
    for(auto p:addition.points){p.sourceIndex=-1;p.value.selected=false;net.points.push_back(p);}
    for(auto e:addition.edges){e.a+=pointOffset;e.b+=pointOffset;e.sourceIndex=-1;e.selected=false;net.edges.push_back(e);}
    for(auto f:addition.faces){for(auto& p:f.points)p+=pointOffset;f.layer+=layerOffset;f.sourceIndex=-1;f.selected=false;net.faces.push_back(f);}
    for(auto c:addition.curves){for(auto& p:c.points)p+=pointOffset;c.sourceIndex=-1;c.selected=false;net.curves.push_back(c);}
    document=document.withControlNet(net);auto generated=documentBindings(document,bindings);auto high=bindings["high_water"].toObject();
    for(const auto* key:{"points","edges","faces","curves"}){auto ids=bindings[key].toArray();while(ids.size()<generated[key].toArray().size()){int next=integer(high[key],0,2147483646)+1;high[key]=next;ids.append(next);}bindings[key]=ids;}
    bindings["high_water"]=high;
    // Preserve old ordering; only append disjoint new topology/dependencies.
    for(auto pair:{std::pair{"point_edges","edges"},std::pair{"point_anchors","points"},std::pair{"point_lines","points"},std::pair{"layer_faces","faces"}}){
        auto rows=bindings[pair.first].toArray(),fresh=generated[pair.first].toArray(),ids=bindings[pair.second].toArray();
        int first=QString(pair.first)=="layer_faces"?layerOffset:pointOffset;
        while(rows.size()>first)rows.removeLast();
        for(int i=first;i<fresh.size();++i){QJsonArray row;for(auto v:fresh[i].toArray())row.append(ids[v.toInt()-1]);rows.append(row);}bindings[pair.first]=rows;
    }
    return {document,bindings,true};
}
}
