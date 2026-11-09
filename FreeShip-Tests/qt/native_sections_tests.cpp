// Independent test-only JSON driver. The calculation library itself has no Qt.
#include "native/sections.h"
#include "native/linesplan.h"
#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <cmath>
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

QJsonArray flags(const std::vector<bool>& values){QJsonArray a;for(bool b:values)a.append(b);return a;}
QJsonArray floats(const std::vector<float>& values){QJsonArray a;for(float f:values)a.append(double(f));return a;}
QJsonObject samples(const ng::SectionCurve& curve,QJsonArray parameters){
    ng::Spline spline(curve.points,curve.knuckles,curve.fragments);QJsonArray values,first,second;
    for(auto value:parameters){const double t=number(value);values.append(json(spline.value(t)));first.append(json(spline.firstDerivative(float(t))));second.append(json(spline.secondDerivative(float(t))));}
    return {{"parameters",parameters},{"values",values},{"first",first},{"second",second},{"knots",floats(spline.parameters())},{"total_length",double(spline.totalChordLength())},{"fragments",double(curve.fragments)}};
}
QJsonObject json(const ng::SectionCurve& curve,QJsonArray parameters={},bool sample=false){
    QJsonArray points,ids;for(auto p:curve.points)points.append(json(p));for(auto s:curve.sourceSegments)ids.append(double(s));
    QJsonObject result{{"points",points},{"knuckles",flags(curve.knuckles)},{"closed",curve.closed},{"fragments",double(curve.fragments)}};
    if(sample)result.insert("samples",samples(curve,parameters));
    return result;
}
QJsonArray chains(const std::vector<ng::SectionCurve>& curves,QJsonArray parameters={},bool sample=false){QJsonArray a;for(const auto& c:curves)a.append(json(c,parameters,sample));return a;}
QJsonObject endpoint(const ng::SectionEndpoint& point){return {{"position",json(point.position)},{"knuckle",point.knuckle},{"edge",double(point.edge)}};}
QJsonObject json(const ng::SectionResult& r,QJsonArray parameters={}){
    QJsonArray segments,diagnostics;
    for(const auto& s:r.segments){segments.append(QJsonObject{{"start",endpoint(s.start)},{"end",endpoint(s.end)},{"face",double(s.face)},{"root_control_face",double(s.rootControlFace)}});
        diagnostics.append(QJsonObject{{"start_vertex",s.start.vertex?QJsonValue(double(*s.start.vertex)):QJsonValue()},{"end_vertex",s.end.vertex?QJsonValue(double(*s.end.vertex)):QJsonValue()}});}
    auto lineage=[](const std::vector<ng::SectionCurve>& curves){QJsonArray result;for(const auto& c:curves){QJsonArray a;for(auto id:c.sourceSegments)a.append(double(id));result.append(a);}return result;};
    return {{"segments",segments},{"connected_curves",chains(r.connectedCurves)},{"joined_curves",chains(r.joinedCurves)},
        {"curves",chains(r.curves,parameters,true)},{"found",r.found},{"selected_control_faces",json(r.selectedControlFaces)},
        {"diagnostics",QJsonObject{{"surface_box_intersects",r.surfaceBoxIntersects},{"near_duplicate_endpoints_removed",double(r.nearDuplicateEndpointsRemoved)},
            {"join_count",double(r.joinCount)},{"discarded_tiny_curves",double(r.discardedTinyCurves)},{"endpoint_vertices",diagnostics},{"connected_source_segments",lineage(r.connectedCurves)},{"joined_source_segments",lineage(r.joinedCurves)},{"final_source_segments",lineage(r.curves)}}}};
}
ng::Plane plane(QJsonValue value){auto a=array(value);if(a.size()!=4)invalid("Plane needs four coefficients");return {scalar(a[0]),scalar(a[1]),scalar(a[2]),scalar(a[3])};}
ng::SectionCurve curve(QJsonObject value){ng::SectionCurve c;for(auto p:array(value["points"]))c.points.push_back(vector(p));for(auto k:array(value["knuckles"]))c.knuckles.push_back(boolean(k));
    c.fragments=value.contains("fragments")?integer(value["fragments"],0,1000000):(c.points.empty()?0:c.points.size()-1);return c;}
ng::SectionResult cut(const ng::GeometryInput& g,const ng::SubdivisionLevel& mesh,QJsonObject query){
    auto p=plane(query["coefficients"]);ng::SectionResult result;
    const auto route=query["route"].toString("surface");
    if(route=="surface")ng::intersectSections(g,mesh,p,{query.contains("hydrostatics_layers_only")?boolean(query["hydrostatics_layers_only"]):false},result);
    else if(route=="faces")ng::calculateSections(mesh,p,indices(query["control_faces"]),result);
    else invalid("Unknown section route");
    return result;
}
QJsonObject runCase(QJsonObject item){
    QJsonArray parameters=item.contains("sample_parameters")?array(item["sample_parameters"]):QJsonArray{0,.125,.25,.375,.5,.625,.75,.875,1};
    if(item["kind"]=="linesplan"){
        const auto c=curve(object(item["chain"]));ng::Spline spline(c.points,c.knuckles,c.fragments);
        const auto view=static_cast<ng::LinesplanView>(integer(item["view"],0,3));
        ng::LinesplanPlacement placement{vector(item["origin"]),scalar(item["split"]),boolean(item["mirror_plan"]),boolean(item["has_diagonals"])};
        auto path=[](const ng::CurvePath& points){QJsonArray result;for(auto p:points)result.append(json(p));return result;};
        auto paths=[&](const std::vector<ng::CurvePath>& values){QJsonArray result;for(const auto& p:values)result.append(path(p));return result;};
        const auto sampled=path(ng::samplePolyline(spline));
        const auto display=paths(ng::linesplanCurve(spline,view,placement));
        const auto exported=paths(ng::linesplanCurve(spline,view,placement,ng::CurvePurpose::DXF));
        return {{"samples",sampled},{"display_paths",display},{"dxf_paths",exported},{"fragments_after_export",double(spline.fragments())},
            {"display_after_export",paths(ng::linesplanCurve(spline,view,placement))}};
    }
    if(item["kind"]=="simplify"){
        const auto c=curve(object(item["chain"]));ng::Spline spline(c.points,c.knuckles,c.fragments);
        if(item["prebuild"].toBool(true))(void)spline.totalChordLength();
        const bool ok=spline.simplify(scalar(item["criterion"]));
        std::vector<ng::SectionCurve> curves{{spline.points(),spline.knuckles(),{},spline.fragments(),false}};
        ng::joinSectionCurves(curves);
        return {{"curves",chains(curves,parameters,true)},{"simplified",ok}};
    }
    if(item["kind"]=="join"){
        std::vector<ng::SectionCurve> curves;for(auto v:array(item["chains"]))curves.push_back(curve(object(v)));
        auto original=curves;const float error=item.contains("join_error")?scalar(item["join_error"]):.01f;
        const bool force=item.contains("force_to_one")?boolean(item["force_to_one"]):false;
        auto count=ng::joinSectionCurves(curves,error,force);ng::joinSectionCurves(original,error,force);
        return {{"curves",chains(curves,parameters,true)},{"repeat_equal",chains(curves,parameters,true)==chains(original,parameters,true)},{"join_count",double(count)}};
    }
    const auto g=input(object(item["geometry"]));ng::SubdivisionSurface surface(g);const auto& mesh=surface.level(integer(item["level"],0,4));QJsonArray results;
    for(auto v:array(item["planes"])){
        const auto query=object(v);QJsonObject result{{"id",query["id"]}};
        try {const auto r=cut(g,mesh,query);result=json(r,parameters);result.insert("id",query["id"]);result.insert("status","ok");result.insert("repeat_equal",result==([&]{auto q=json(cut(g,mesh,query),parameters);q.insert("id",query["id"]);q.insert("status","ok");return q;})());}
        catch(const std::exception& e){result.insert("status","error");result.insert("error",QString::fromUtf8(e.what()));}
        results.append(result);
    }
    return {{"planes",results},{"mesh",json(mesh)}};
}
ng::GeometryInput square(){ng::GeometryInput g;g.layers.push_back({});g.points={{{-1,-1,0},ng::VertexType::Corner},{{1,-1,0},ng::VertexType::Corner},{{1,1,0},ng::VertexType::Corner},{{-1,1,0},ng::VertexType::Corner}};
    g.edges={{{0,1},true,-1},{{1,2},true,-1},{{2,3},true,-1},{{3,0},true,-1}};g.faces={{{0,1,2,3},0}};return g;}
QJsonArray apiChecks(){
    QJsonArray checks;auto check=[&](const char* id,bool ok){checks.append(QJsonObject{{"id",id},{"status",ok?"pass":"fail"}});};
    auto throws=[](auto f){try{f();return false;}catch(const std::exception&){return true;}};
    auto g=square();ng::SubdivisionSurface s(g);auto mesh=s.level(1);auto r=ng::intersectSections(g,mesh,ng::Plane(1,0,0,-.25f));const auto valid=json(r);
    check("nonempty-result",r.found&&!r.curves.empty());
    ng::SectionIndex index(g,mesh);
    for(float x:{-2.f,-1.f,-.25f,0.f,.25f,1.f,2.f})
        check("indexed-section-exact",json(index.intersect(ng::Plane(1,0,0,-x)))==json(ng::intersectSections(g,mesh,ng::Plane(1,0,0,-x))));
    check("indexed-invalid-plane-rejected",throws([&]{index.intersect(ng::Plane(0,0,0,0));}));
    check("indexed-failure-keeps-valid-data",json(index.intersect(ng::Plane(1,0,0,-.25f)))==valid);
    check("invalid-plane-rejected",throws([&]{ng::intersectSections(g,mesh,ng::Plane(0,0,0,0),{},r);}));check("failed-cut-preserves-result",json(r)==valid);
    auto bad=mesh;bad.edges[0].faces.clear();check("broken-edge-incidence-rejected",throws([&]{ng::intersectSections(g,bad,ng::Plane(1,0,0,0),{},r);}));check("bad-mesh-preserves-result",json(r)==valid);
    check("bad-root-selection-rejected",throws([&]{ng::calculateSections(mesh,ng::Plane(1,0,0,0),{99},r);}));check("bad-selection-preserves-result",json(r)==valid);
    check("duplicate-root-rejected",throws([&]{ng::calculateSections(mesh,ng::Plane(1,0,0,0),{0,0},r);}));
    ng::intersectSections(g,mesh,ng::Plane(0,0,1,-2),{},r);check("successful-empty-replaces-result",!r.found&&r.curves.empty()&&r.segments.empty());
    auto chains0=std::vector<ng::SectionCurve>{{{{0,0,0},{1,0,0}},{false,false},{},1,false},{{{1,0,0},{2,0,0}},{false,false},{},1,false}};auto saved=chains(chains0);
    check("negative-join-error-rejected",throws([&]{ng::joinSectionCurves(chains0,-1);}));check("failed-join-preserves-input",chains(chains0)==saved);
    check("nonfinite-join-error-rejected",throws([&]{ng::joinSectionCurves(chains0,std::numeric_limits<float>::quiet_NaN());}));
    auto wrong=chains0;wrong[1].knuckles.clear();auto before=chains(wrong);check("knuckle-count-rejected",throws([&]{ng::joinSectionCurves(wrong);}));check("failed-knuckles-preserve-input",chains(wrong)==before);
    wrong=chains0;wrong[1].points[0].x=std::numeric_limits<float>::infinity();check("nonfinite-chain-rejected",throws([&]{ng::joinSectionCurves(wrong);}));
    chains0[0].closed=true;ng::joinSectionCurves(chains0);check("closed-is-recomputed",chains0.size()==1&&!chains0[0].closed);
    ng::Spline simplifier({{0,0,0},{.01f,0,0},{1,0,0}});
    check("simplify-negative-rejected",throws([&]{simplifier.simplify(-1);}));
    check("simplify-nonfinite-rejected",throws([&]{simplifier.simplify(std::numeric_limits<float>::quiet_NaN());}));
    check("simplify-failure-preserves-points",!simplifier.built()&&simplifier.points().size()==3&&simplifier.points()[1].x==.01f);
    ng::Spline overflowing({{-std::numeric_limits<float>::max(),0,0},{0,0,0},{std::numeric_limits<float>::max(),0,0}});
    check("simplify-overflow-preserves-state",throws([&]{overflowing.simplify(2);})&&!overflowing.built()&&overflowing.points().size()==3);
    ng::Spline warm({{0,0,0},{3,4,0},{6,0,0}});
    const auto warmValue=warm.value(.5);const auto builtCount=warm.rebuildCount();
    warm.simplify(96000008);
    check("simplify-invalidates-cached-curve",warmValue.y==4&&!warm.built()&&warm.rebuildCount()==builtCount&&warm.value(.5).y==0&&warm.rebuildCount()==builtCount+1);
    ng::Spline cold({{0,0,0},{3,4,0},{6,0,0}});
    cold.simplify(96000008);
    check("simplify-skips-unneeded-cubic-build",cold.points().size()==2&&!cold.built()&&cold.rebuildCount()==0&&cold.value(.5).x==3);
    ng::Spline empty, single({{1,2,3}});
    check("simplify-short-curves",empty.simplify(2)&&empty.points().empty()&&single.simplify(2)&&single.points().size()==1);
    ng::Spline line({{0,0,0},{1,2,3}},{},1);const ng::LinesplanPlacement placement;
    check("polyline-single-sample-rejected",throws([&]{ng::samplePolyline(line);}));
    line.setFragments(0);check("polyline-zero-samples-rejected",throws([&]{ng::samplePolyline(line);}));
    check("polyline-empty-and-single",ng::samplePolyline(empty).empty()&&ng::samplePolyline(single).empty());
    line.setFragments(8);
    check("projection-unknown-view-rejected",throws([&]{ng::linesplanCurve(line,static_cast<ng::LinesplanView>(99),placement);}));
    auto invalidPlacement=placement;invalidPlacement.origin.x=std::numeric_limits<float>::infinity();
    check("projection-nonfinite-rejected",throws([&]{ng::linesplanCurve(line,ng::LinesplanView::Plan,invalidPlacement);}));
    check("projection-failure-preserves-density",line.fragments()==8&&line.points().size()==2);
    ng::Spline corners({{0,0,0},{1,2,0},{2,-2,0},{3,0,0},{4,1,0}},{false,true,true,true,false},2);
    const auto complete=ng::samplePolyline(corners,true);
    bool allCorners=complete.size()==5;
    for(std::size_t i=0;i<complete.size()&&i<corners.points().size();++i)
        allCorners=allCorners&&complete[i].x==corners.points()[i].x&&complete[i].y==corners.points()[i].y;
    check("display-preserves-every-marked-corner",allCorners);
    check("corner-preservation-keeps-export-policy",ng::samplePolyline(corners).size()==3&&corners.fragments()==2);
    return checks;
}
}
int main(int argc,char** argv){
    QCoreApplication application(argc,argv);if(argc!=3)return 3;QJsonObject report{{"format","FreeShipNativeSectionsResults"},{"version",1}};
    try{QFile file(application.arguments()[1]);if(!file.open(QIODevice::ReadOnly))invalid("Cannot open input");QJsonParseError parse;const auto document=QJsonDocument::fromJson(file.readAll(),&parse);
        if(parse.error!=QJsonParseError::NoError||!document.isObject())invalid("Malformed JSON input");
        auto root=document.object();if(root["format"]!="FreeShipNativeSectionsCases"||root["version"].toInt()!=1)invalid("Wrong input format");QJsonArray cases;
        for(auto value:array(root["cases"])){const auto item=object(value);QJsonObject result{{"id",item["id"]}};try{result=runCase(item);result.insert("id",item["id"]);result.insert("status","ok");}catch(const std::exception& e){result.insert("status","error");result.insert("error",QString::fromUtf8(e.what()));}cases.append(result);}
        report.insert("cases",cases);report.insert("api_checks",apiChecks());
    }catch(const std::exception& e){report.insert("error",QString::fromUtf8(e.what()));}
    QSaveFile output(application.arguments()[2]);output.setDirectWriteFallback(false);if(!output.open(QIODevice::WriteOnly)||output.write(QJsonDocument(report).toJson())<0||!output.commit())return 3;return report.contains("error")?3:0;
}
