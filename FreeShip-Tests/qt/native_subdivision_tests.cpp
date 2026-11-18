// Independent test-only JSON driver. The calculation library itself has no Qt.
#include "native/subdivision.h"
#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace ng=fs::native_geometry;
namespace {
[[noreturn]] void invalid(const char* reason){throw std::invalid_argument(reason);}
double number(QJsonValue value){if(!value.isDouble()||!std::isfinite(value.toDouble()))invalid("Expected finite number");return value.toDouble();}
float scalar(QJsonValue value){const double d=number(value);const float f=static_cast<float>(d);if(!std::isfinite(f))invalid("Input exceeds Single range");return f;}
int integer(QJsonValue value,int minimum=-2147483647,int maximum=2147483647){const double d=number(value);if(std::floor(d)!=d||d<minimum||d>maximum)invalid("Invalid integer");return static_cast<int>(d);}
bool boolean(QJsonValue value){if(!value.isBool())invalid("Expected Boolean");return value.toBool();}
QJsonArray array(QJsonValue value){if(!value.isArray())invalid("Expected array");return value.toArray();}
QJsonObject object(QJsonValue value){if(!value.isObject())invalid("Expected object");return value.toObject();}
ng::Vec3 vector(QJsonValue value){const auto a=array(value);if(a.size()!=3)invalid("Expected three coordinates");return {scalar(a[0]),scalar(a[1]),scalar(a[2])};}
QJsonArray json(ng::Vec3 p){return {double(p.x),double(p.y),double(p.z)};}
std::vector<std::size_t> indices(QJsonValue value){std::vector<std::size_t> result;for(auto v:array(value))result.push_back(integer(v,0,1000000));return result;}
QJsonArray json(const std::vector<std::size_t>& values){QJsonArray result;for(auto v:values)result.append(double(v));return result;}
QJsonArray json(std::array<std::size_t,2> values){return {double(values[0]),double(values[1])};}
std::array<std::size_t,2> pair(QJsonValue value){const auto a=indices(value);if(a.size()!=2)invalid("Edge needs two indices");return {a[0],a[1]};}

ng::LayerInput layer(QJsonObject value){
    ng::LayerInput result;
    result.symmetric=boolean(value["symmetric"]);result.surfaceVisible=boolean(value["surface_visible"]);
    result.controlNetVisible=boolean(value["control_net_visible"]);result.useForIntersections=boolean(value["use_for_intersections"]);
    result.useInHydrostatics=boolean(value["use_in_hydrostatics"]);result.showInLinesplan=boolean(value["show_in_linesplan"]);
    result.developable=boolean(value["developable"]);result.color=integer(value["color"],std::numeric_limits<int>::min());
    result.alphaBlend=integer(value["alpha_blend"],0,255);result.materialDensity=scalar(value["material_density"]);
    result.thickness=scalar(value["thickness"]);result.name=value["name"].toString().toUtf8().toStdString();
    result.layerId=integer(value["layer_id"],std::numeric_limits<int>::min());return result;
}
QJsonObject json(const ng::LayerInput& v){
    return {{"symmetric",v.symmetric},{"surface_visible",v.surfaceVisible},{"control_net_visible",v.controlNetVisible},
        {"use_for_intersections",v.useForIntersections},{"use_in_hydrostatics",v.useInHydrostatics},
        {"show_in_linesplan",v.showInLinesplan},{"developable",v.developable},{"color",v.color},
        {"alpha_blend",int(v.alphaBlend)},{"material_density",double(v.materialDensity)},
        {"thickness",double(v.thickness)},{"name",QString::fromUtf8(v.name.c_str())},{"layer_id",v.layerId}};
}
ng::GeometryInput input(QJsonObject value){
    if(value["format"]!="FreeShipGeometryInput"||value["version"].toInt()!=1)invalid("Unsupported geometry input");
    ng::GeometryInput result;result.subdivisionMode=static_cast<ng::SubdivisionMode>(integer(value["subdivision_mode"],0,1));
    result.precision=integer(value["precision"],0,10);result.desiredLevel=integer(value["desired_level"],0,4);
    for(auto item:array(value["points"])) {const auto v=object(item);result.points.push_back({vector(v["position"]),static_cast<ng::VertexType>(integer(v["type"],0,3))});}
    for(auto item:array(value["edges"])) {const auto v=object(item);result.edges.push_back({pair(v["points"]),boolean(v["crease"]),integer(v["curve"],-1,1000000)});}
    for(auto item:array(value["faces"])) {const auto v=object(item);result.faces.push_back({indices(v["points"]),std::size_t(integer(v["layer"],0,1000000))});}
    for(auto item:array(value["layers"]))result.layers.push_back(layer(object(item)));
    for(auto item:array(value["curves"])) {const auto v=object(item);result.curves.push_back({indices(v["points"]),boolean(v["visible"])});}
    return result;
}
QJsonObject json(const ng::GeometryInput& value){
    QJsonArray points,edges,faces,layers,curves;
    for(const auto& p:value.points)points.append(QJsonObject{{"position",json(p.position)},{"type",int(p.type)}});
    for(const auto& e:value.edges)edges.append(QJsonObject{{"points",json(e.points)},{"crease",e.crease},{"curve",e.curve}});
    for(const auto& f:value.faces)faces.append(QJsonObject{{"points",json(f.points)},{"layer",double(f.layer)}});
    for(const auto& l:value.layers)layers.append(json(l));
    for(const auto& c:value.curves)curves.append(QJsonObject{{"points",json(c.points)},{"visible",c.visible}});
    return {{"format","FreeShipGeometryInput"},{"version",1},{"subdivision_mode",int(value.subdivisionMode)},
        {"precision",value.precision},{"desired_level",int(value.desiredLevel)},{"points",points},
        {"edges",edges},{"faces",faces},{"layers",layers},{"curves",curves}};
}
QJsonObject json(const ng::SubdivisionLevel& level){
    QJsonArray points,faces,edges,curves,layers,loosePoints,looseEdges;
    for(const auto& p:level.points)points.append(QJsonObject{{"position",json(p.position)},{"type",int(p.type)}});
    for(const auto& f:level.faces)faces.append(QJsonObject{{"points",json(f.points)},{"root_control_face",double(f.rootControlFace)},
        {"child_index",f.childIndex},{"layer",double(f.layer)}});
    for(const auto& e:level.edges)edges.append(QJsonObject{{"points",json(e.points)},{"crease",e.crease},
        {"curve",e.curve},{"faces",json(e.faces)},{"control_descendant",e.controlDescendant}});
    for(const auto& c:level.curves){QJsonArray knuckles;for(bool k:c.knuckles)knuckles.append(k);
        curves.append(QJsonObject{{"points",json(c.points)},{"visible",c.visible},{"free_standing",c.freeStanding},{"knuckles",knuckles}});}
    for(const auto& l:level.layers)layers.append(json(l));
    for(const auto& p:level.looseControlPoints)loosePoints.append(QJsonObject{{"control_point",double(p.controlPoint)},
        {"position",json(p.position)},{"type",int(p.type)}});
    for(const auto& e:level.looseControlEdges)looseEdges.append(QJsonObject{{"control_edge",double(e.controlEdge)},
        {"points",json(e.points)},{"crease",e.crease},{"curve",e.curve}});
    return {{"level",int(level.level)},{"points",points},{"faces",faces},{"edges",edges},{"curves",curves},
        {"layers",layers},{"loose_control_points",loosePoints},{"loose_control_edges",looseEdges},
        {"bounds",QJsonObject{{"min",json(level.minimum)},{"max",json(level.maximum)}}},{"empty",level.empty}};
}
bool exactMesh(const ng::SubdivisionLevel& a,const ng::SubdivisionLevel& b){
    if(json(a)!=json(b))return false;
    const auto same=[](ng::Vec3 x,ng::Vec3 y){return std::memcmp(&x.x,&y.x,4)==0&&std::memcmp(&x.y,&y.y,4)==0&&std::memcmp(&x.z,&y.z,4)==0;};
    if(!same(a.minimum,b.minimum)||!same(a.maximum,b.maximum))return false;
    for(std::size_t i=0;i<a.points.size();++i)if(!same(a.points[i].position,b.points[i].position))return false;
    for(std::size_t i=0;i<a.edges.size();++i)if(a.edges[i].reversed!=b.edges[i].reversed)return false;
    for(std::size_t i=0;i<a.looseControlPoints.size();++i)if(!same(a.looseControlPoints[i].position,b.looseControlPoints[i].position))return false;
    return true;
}
QJsonObject incrementalCase(QJsonObject item){
    const auto original=input(object(item["geometry"]));QJsonArray checks;
    for(auto wanted:array(item["levels"])){
        const auto level=unsigned(integer(wanted,0,4));auto g=original;auto program=ng::compileSubdivision(g,level);
        auto check=[&](const char* edit,bool noOp=false){
            ng::SubdivisionSurface full(g);const bool exact=exactMesh(program.mesh,full.level(level));
            checks.append(QJsonObject{{"level",int(level)},{"edit",edit},{"exact",exact},
                {"zero_work",!noOp||program.evaluated==0},{"evaluated",double(program.evaluated)},
                {"operations",double(program.operations.size())},{"changed_points",double(program.changedPoints)}});
        };
        check("cold");program.update(g);check("no-op",true);
        if(!g.points.empty()){
            g.points[0].position.z+=.125f;program.update(g);check("first-control");
            g.points.back().position.x-=.375f;program.update(g);check("last-control");
            for(std::size_t i=0;i<g.points.size();i+=3)g.points[i].position.y+=float(i+1)*.0625f;
            program.update(g);check("batch-controls");g=original;program.update(g);check("undo-all");
            for(auto& p:g.points){if(p.position.x==0)p.position.x=-0.f;if(p.position.y==0)p.position.y=-0.f;if(p.position.z==0)p.position.z=-0.f;}
            program.update(g);check("signed-zero");program.update(g);check("repeat",true);
        }
    }
    return {{"checks",checks}};
}
QJsonObject runCase(QJsonObject item){
    auto geometry=input(object(item["geometry"]));QJsonArray levels;
    for(auto wanted:array(item["levels"])){
        // Recreate the root for every requested level, as the Pascal oracle does.
        ng::SubdivisionSurface surface(geometry);levels.append(json(surface.level(integer(wanted,0,4))));
    }
    // A different request order on one instance must not change any level.
    ng::SubdivisionSurface reused(geometry),forward(geometry),initial(geometry);QJsonArray repeated,incremental;
    const auto wanted=array(item["levels"]);
    for(int i=wanted.size()-1;i>=0;--i)repeated.prepend(json(reused.level(integer(wanted[i],0,4))));
    for(auto level:wanted)incremental.append(json(forward.level(integer(level,0,4))));
    return {{"levels",levels},{"levels_repeat",repeated},{"levels_forward",incremental},{"loaded_geometry",json(initial.input())}};
}
ng::GeometryInput square(){
    ng::GeometryInput g;g.layers.push_back({});
    g.points={{{0,0,0},ng::VertexType::Crease},{{2,0,0},ng::VertexType::Crease},{{2,2,0},ng::VertexType::Crease},{{0,2,0},ng::VertexType::Crease}};
    g.edges={{{0,1},true,-1},{{1,2},true,-1},{{2,3},true,-1},{{3,0},true,-1}};
    g.faces={{{0,1,2,3},0}};return g;
}
QJsonArray apiChecks(){
    QJsonArray results;auto check=[&](const char* id,bool ok){results.append(QJsonObject{{"id",id},{"status",ok?"pass":"fail"}});};
    auto throws=[](auto operation){try{operation();return false;}catch(const std::exception&){return true;}};
    ng::SubdivisionSurface surface(square());const auto first=json(surface.level(1));const auto builds=surface.rebuildCount();
    check("repeated-level-reuses-cache",json(surface.level(1))==first&&surface.rebuildCount()==builds);
    surface.setPoint(0,{0,0,0});check("no-op-point-keeps-cache",surface.built(1)&&surface.rebuildCount()==builds);
    const float nan=std::numeric_limits<float>::quiet_NaN();
    check("nonfinite-point-mutation-rejected",throws([&]{surface.setPoint(0,{nan,0,0});}));
    check("bad-index-mutation-rejected",throws([&]{surface.setPoint(4,{0,0,0});}));
    check("failed-mutation-preserves-cache",surface.built(1)&&surface.rebuildCount()==builds&&json(surface.level(1))==first);
    surface.setPoint(0,{0,0,1});check("point-change-invalidates-level",!surface.built(1));
    check("changed-point-rebuilds-geometry",json(surface.level(1))!=first&&surface.rebuildCount()==builds+1);
    auto changedInput=square();changedInput.points[0].position={0,0,1};ng::SubdivisionSurface fresh(changedInput);
    check("changed-cached-level-equals-fresh-control-net",json(surface.level(2))==json(fresh.level(2)));
    check("unsupported-level-rejected",throws([&]{surface.level(5);}));
    auto bad=square();bad.faces[0].points[2]=999;
    check("out-of-range-face-index-rejected",throws([&]{ng::SubdivisionSurface s(bad);s.level(1);}));
    bad=square();bad.points[0].position.x=nan;
    check("nonfinite-input-point-rejected",throws([&]{ng::SubdivisionSurface s(bad);s.level(1);}));
    bad=square();bad.edges.pop_back();
    check("missing-face-edge-rejected",throws([&]{ng::SubdivisionSurface s(bad);s.level(1);}));
    bad=square();bad.edges.push_back(bad.edges[0]);
    check("duplicate-control-edge-rejected",throws([&]{ng::SubdivisionSurface s(bad);s.level(1);}));
    bad=square();bad.faces[0].points={0,1,1,3};
    check("repeated-face-vertex-rejected",throws([&]{ng::SubdivisionSurface s(bad);s.level(1);}));
    bad=square();bad.faces[0].layer=1;
    check("unknown-layer-rejected",throws([&]{ng::SubdivisionSurface s(bad);s.level(1);}));
    auto huge=square();for(auto& point:huge.points)point.position.x=std::numeric_limits<float>::max();
    ng::SubdivisionSurface recoverable(huge);const auto low=json(recoverable.level(0));const auto lowBuilds=recoverable.rebuildCount();
    check("higher-level-overflow-rejected",throws([&]{recoverable.level(1);}));
    check("failed-higher-level-preserves-lower-cache",recoverable.built(0)&&!recoverable.built(1)&&recoverable.rebuildCount()==lowBuilds&&json(recoverable.level(0))==low);
    const auto original=square();for(std::size_t i=0;i<original.points.size();++i)recoverable.setPoint(i,original.points[i].position);
    ng::SubdivisionSurface reference(original);
    check("failed-higher-level-can-recover",json(recoverable.level(1))==json(reference.level(1)));
    return results;
}
}
int main(int argc,char** argv){
    QCoreApplication application(argc,argv);if(argc!=3)return 3;
    QJsonObject report{{"format","FreeShipNativeSubdivisionResults"},{"version",1}};
    try{
        QFile file(application.arguments()[1]);if(!file.open(QIODevice::ReadOnly))invalid("Cannot open input");
        QJsonParseError parse;const auto doc=QJsonDocument::fromJson(file.readAll(),&parse);
        if(parse.error!=QJsonParseError::NoError||!doc.isObject())invalid("Malformed JSON input");
        const auto root=doc.object();if(root["format"]!="FreeShipNativeSubdivisionCases"||root["version"].toInt()!=1)invalid("Unsupported case schema");
        QJsonArray cases;
        for(auto value:array(root["cases"])){
            const auto item=object(value);QJsonObject result{{"id",item["id"]}};
            try{result=root["incremental_only"].toBool()?incrementalCase(item):runCase(item);result.insert("id",item["id"]);result.insert("status","ok");}
            catch(const std::exception& error){result.insert("status","error");result.insert("error",QString::fromUtf8(error.what()));}
            cases.append(result);
        }
        report.insert("cases",cases);report.insert("api_checks",apiChecks());
    }catch(const std::exception& error){report.insert("error",QString::fromUtf8(error.what()));}
    QSaveFile output(application.arguments()[2]);output.setDirectWriteFallback(false);
    if(!output.open(QIODevice::WriteOnly)||output.write(QJsonDocument(report).toJson())<0||!output.commit())return 3;
    return report.contains("error")?3:0;
}
