// SPDX-License-Identifier: GPL-2.0-or-later
#include "model_snapshot.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QSet>
#include <QCryptographicHash>
#include <QUuid>
#include <cmath>
#include <stdexcept>

namespace fs {
namespace {
constexpr quint64 maxJsonInteger = 9007199254740991ULL;
constexpr double maxCoordinate = 1.0e9;
constexpr qint64 maxSnapshotBytes = 256LL * 1024 * 1024;
[[noreturn]] void invalid(const QString& message) {
    throw std::runtime_error((QStringLiteral("Invalid model snapshot: ") + message).toStdString());
}
QJsonObject object(const QJsonValue& value, const QString& name) {
    if (!value.isObject()) invalid(name + " must be an object");
    return value.toObject();
}
QString string(const QJsonValue& value, const QString& name, bool allowEmpty = false) {
    if (!value.isString()) invalid(name + " must be a string");
    const auto result = value.toString();
    if ((!allowEmpty && result.isEmpty()) || result.contains(QChar(0))) invalid(name + " is empty or contains NUL");
    return result;
}
bool boolean(const QJsonValue& value, const QString& name) {
    if (!value.isBool()) invalid(name + " must be a boolean");
    return value.toBool();
}
double number(const QJsonValue& value, const QString& name) {
    if (!value.isDouble()) invalid(name + " must be a number");
    const double result = value.toDouble();
    if (!std::isfinite(result) || std::abs(result) > maxCoordinate) invalid(name + " is not a finite bounded number");
    return result;
}
quint64 integer(const QJsonValue& value, const QString& name, quint64 minimum = 1,
                quint64 maximum = maxJsonInteger) {
    if (!value.isDouble()) invalid(name + " must be an integer");
    const double n = value.toDouble();
    if (!std::isfinite(n) || n < static_cast<double>(minimum) ||
        n > static_cast<double>(maximum) || std::trunc(n) != n) invalid(name + " is outside the integer range");
    return static_cast<quint64>(n);
}
QJsonArray array(const QJsonValue& value, const QString& name, qsizetype maximum) {
    if (!value.isArray()) invalid(name + " must be an array");
    const auto result = value.toArray();
    if (result.size() > maximum) invalid(name + " exceeds the supported collection limit");
    return result;
}
Vec3 vector(const QJsonValue& value, const QString& name) {
    const auto v = array(value, name, 3);
    if (v.size() != 3) invalid(name + " must contain exactly three coordinates");
    return {number(v[0], name + "[0]"), number(v[1], name + "[1]"), number(v[2], name + "[2]")};
}
void finiteVector(const Vec3& v, const char* name) {
    for (double x : {v.x, v.y, v.z})
        if (!std::isfinite(x) || std::abs(x) > maxCoordinate) invalid(QString::fromLatin1(name) + " is not finite or exceeds 1e9");
}
void uniqueId(QSet<EntityId>& ids, EntityId id, const char* collection) {
    if (id == 0 || id > maxJsonInteger || ids.contains(id)) invalid(QString::fromLatin1(collection) + " contains an invalid or duplicate ID");
    ids.insert(id);
}
QByteArray surfaceDigest(const QJsonObject& root) {
    QJsonArray curves;
    for (auto value : root["control_curves"].toArray()) {
        auto c=value.toObject();
        curves.append(QJsonObject{{"id",c["id"]},{"spline",c["spline"]}});
    }
    return QCryptographicHash::hash(QJsonDocument(QJsonObject{
        {"triangles",root["triangles"]},{"surface_normals",root["surface_normals"]},
        {"control_curves",curves}}).toJson(QJsonDocument::Compact),QCryptographicHash::Sha256);
}
}

static QJsonObject checkedSurfaceDisplay(const QByteArray& data,const QByteArray& expected) {
    if(data.size()>maxSnapshotBytes)invalid("surface display exceeds file size limit");
    QJsonParseError error;
    const auto doc=QJsonDocument::fromJson(data,&error);
    const auto root=doc.object();
    if (error.error!=QJsonParseError::NoError || !doc.isObject() ||
        root["format"]!="FreeShipSurfaceDisplay" || root["version"]!=1 ||
        !root["triangles"].isArray() || !root["surface_normals"].isArray() || !root["control_curves"].isArray() ||
        surfaceDigest(root)!=expected)
        invalid("native surface geometry does not match snapshot");
    return root;
}
void ModelSnapshot::verifySurfaceDisplay(const QByteArray& data) const {
    checkedSurfaceDisplay(data,surfaceDigest_);
}

void ModelSnapshot::loadSurfaceDisplay(const QByteArray& data) {
    const auto root=checkedSurfaceDisplay(data,surfaceDigest_);
    if(!root.contains("analysis_topology")){surfaceTopology.reset();return;}
    const auto topology=object(root["analysis_topology"],"analysis_topology");
    auto result=std::make_shared<SurfaceTopology>();
    for(const auto& value:array(topology["points"],"analysis points",5000000)){
        const auto p=array(value,"analysis point",4);if(p.size()!=4)invalid("analysis point needs position and type");
        result->points.append({number(p[0],"analysis x"),number(p[1],"analysis y"),number(p[2],"analysis z")});
        result->types.append(int(integer(p[3],"analysis type",0,3)));
    }
    qsizetype triangle=0,corners=0;
    for(const auto& value:array(topology["faces"],"analysis faces",5000000)){
        const auto polygon=array(value,"analysis polygon",1000000);if(polygon.size()<3)invalid("analysis polygon has fewer than three points");
        corners+=polygon.size();if(corners>15000000||result->points.isEmpty())invalid("analysis topology exceeds limits");
        QVector<int> face;QSet<int> unique;
        for(const auto& index:polygon){int p=int(integer(index,"analysis index",0,result->points.size()-1));if(unique.contains(p))invalid("repeated analysis polygon point");unique.insert(p);face.append(p);}
        for(int k=2;k<face.size();++k){
            if(triangle>=triangles.size())invalid("extra analysis triangles");
            const int indices[]{face[0],face[k-1],face[k]};
            for(int j=0;j<3;++j){const auto a=result->points[indices[j]],b=triangles[triangle].vertices[j];
                if(a.x!=b.x||a.y!=b.y||a.z!=b.z)invalid("analysis topology does not match display triangles");}
            ++triangle;
        }
        result->faces.append(std::move(face));
    }
    if(triangle!=triangles.size())invalid("missing analysis triangles");
    surfaceTopology=std::move(result);
}

ModelSnapshot ModelSnapshot::load(const QString& filename) {
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly)) invalid("cannot open " + filename + ": " + file.errorString());
    if (file.size() < 2 || file.size() > maxSnapshotBytes) invalid("file size is outside the supported range");
    const auto data = file.readAll();
    if (file.error() != QFile::NoError) invalid("cannot read " + filename + ": " + file.errorString());
    QJsonParseError error;
    const auto json = QJsonDocument::fromJson(data, &error);
    if (error.error != QJsonParseError::NoError || !json.isObject())
        invalid("JSON parse failed: " + error.errorString());
    const auto root = json.object();
    if (string(root["format"], "format") != "FreeShipModelSnapshot" || integer(root["version"], "version") != 1)
        invalid("unsupported format or version");
    ModelSnapshot result;
    result.surfaceDigest_=surfaceDigest(root);
    result.documentId = string(root["document_id"], "document_id");
    result.revision = integer(root["revision"], "revision");
    result.name = string(root["name"], "name", true);
    result.units = string(root["units"], "units");
    const auto bounds = object(root["bounds"], "bounds");
    result.minimum = vector(bounds["min"], "bounds.min");
    result.maximum = vector(bounds["max"], "bounds.max");
    const auto settings = object(root["settings"], "settings");
    // Optional additions keep the frozen M1 artifacts readable. When present,
    // every new field uses the same strict domains as an edited M2 snapshot.
    if (settings.contains("name") && string(settings["name"], "settings.name", true) != result.name) invalid("conflicting project names");
    if (settings.contains("units") && string(settings["units"], "settings.units") != result.units) invalid("conflicting project units");
    if (settings.contains("designer")) result.designer = string(settings["designer"], "settings.designer", true);
    if (settings.contains("comment")) result.comment = string(settings["comment"], "settings.comment", true);
    if (settings.contains("created_by")) result.createdBy = string(settings["created_by"], "settings.created_by", true);
    if (settings.contains("particulars_set")) result.particularsSet = boolean(settings["particulars_set"], "settings.particulars_set");
    if (settings.contains("text_encoding")) result.textEncoding = string(settings["text_encoding"], "settings.text_encoding");
    if (settings.contains("water_density")) result.waterDensity = number(settings["water_density"], "settings.water_density");
    if (settings.contains("appendage_coefficient")) result.appendageCoefficient = number(settings["appendage_coefficient"], "settings.appendage_coefficient");
    if (settings.contains("split_section_location")) result.splitSectionLocation = number(settings["split_section_location"], "settings.split_section_location");
    if (settings.contains("use_default_split_section_location")) result.useDefaultSplitSectionLocation = boolean(settings["use_default_split_section_location"], "settings.use_default_split_section_location");
    if (settings.contains("simplify_intersections")) result.simplifyIntersections = boolean(settings["simplify_intersections"], "settings.simplify_intersections");
    if (settings.contains("save_preview")) result.savePreview = boolean(settings["save_preview"], "settings.save_preview");
    if(settings.contains("coefficient_basis"))result.coefficientBasis=int(integer(settings["coefficient_basis"],"settings.coefficient_basis",0,1));
    const std::array<const char*,5> hydroFields{"hydro_displacement","hydro_lateral_area","hydro_sectional_areas","hydro_metacentric_height","hydro_lcf"};
    for(size_t i=0;i<hydroFields.size();++i)if(settings.contains(hydroFields[i]))result.hydroFeatures[i]=boolean(settings[hydroFields[i]],hydroFields[i]);
    if (settings.contains("precision")) result.precision = static_cast<int>(integer(settings["precision"], "settings.precision", 0, 3));
    result.length = number(settings["length"], "settings.length");
    result.beam = number(settings["beam"], "settings.beam");
    result.draft = number(settings["draft"], "settings.draft");
    result.waterlineZ = number(settings["waterline_z"], "settings.waterline_z");
    result.shadeUnderwater = boolean(settings["shade_underwater"], "settings.shade_underwater");
    result.underwaterColor = static_cast<quint32>(integer(settings["underwater_color"], "settings.underwater_color", 0, 0xffffff));
    if(settings.contains("underwater_color_alpha"))result.underwaterColorAlpha=integer(settings["underwater_color_alpha"],"settings.underwater_color_alpha",0,255);
    if(settings.contains("generate_auxiliary_reports"))result.generateAuxiliaryReports=boolean(settings["generate_auxiliary_reports"],"settings.generate_auxiliary_reports");
    const auto pointArray = array(root["control_points"], "control_points", 1000000);
    result.points.reserve(pointArray.size());
    for (const auto& item : pointArray) {
        const auto p = object(item, "control point");
        ControlPoint point;point.id=integer(p["id"], "point.id");point.position=vector(p["p"], "point.p");point.locked=boolean(p["locked"], "point.locked");
        point.batchMovable = p.contains("batch_movable") ? boolean(p["batch_movable"], "point.batch_movable") : !point.locked;
        if(p.contains("name"))point.name=string(p["name"],"point.name",true);
        if(p.contains("vertex_type"))point.vertexType=int(integer(p["vertex_type"],"point.vertex_type",0,3));
        if(p.contains("leak"))point.leak=boolean(p["leak"],"point.leak");
        if(p.contains("anchor"))point.anchor=integer(p["anchor"],"point.anchor",0);
        if(p.contains("line_a"))point.lineA=integer(p["line_a"],"point.line_a",0);
        if(p.contains("line_b"))point.lineB=integer(p["line_b"],"point.line_b",0);
        if(p.contains("hard_anchor"))point.hardAnchor=boolean(p["hard_anchor"],"point.hard_anchor");
        result.points.append(point);
    }
    const auto edgeArray = array(root["control_edges"], "control_edges", 2000000);
    result.edges.reserve(edgeArray.size());
    for (const auto& item : edgeArray) {
        const auto e = object(item, "control edge");
        result.edges.append({integer(e["id"], "edge.id"), integer(e["a"], "edge.a"), integer(e["b"], "edge.b"), boolean(e["crease"], "edge.crease")});
    }
    const auto faceArray = array(root["control_faces"], "control_faces", 1000000);
    result.faces.reserve(faceArray.size());
    for (const auto& item : faceArray) {
        const auto f = object(item, "control face");
        ControlFace face;
        face.id = integer(f["id"], "face.id"); face.layer = integer(f["layer"], "face.layer");
        const auto ids = array(f["points"], "face.points", 100000);
        face.points.reserve(ids.size());
        for (const auto& id : ids) face.points.append(integer(id, "face point ID"));
        result.faces.append(std::move(face));
    }
    const auto layerArray = array(root["layers"], "layers", 100000);
    result.layers.reserve(layerArray.size());
    for (const auto& item : layerArray) {
        const auto l = object(item, "layer");
        Layer layer{integer(l["id"], "layer.id"), string(l["name"], "layer.name", true),
                    static_cast<quint32>(integer(l["color"], "layer.color", 0, 0xffffff)),
                    boolean(l["visible"], "layer.visible"), boolean(l["symmetric"], "layer.symmetric")};
        if (l.contains("developable")) layer.developable = boolean(l["developable"], "layer.developable");
        if (l.contains("use_in_hydrostatics")) layer.useInHydrostatics = boolean(l["use_in_hydrostatics"], "layer.use_in_hydrostatics");
        if (l.contains("show_in_linesplan")) layer.showInLinesplan = boolean(l["show_in_linesplan"], "layer.show_in_linesplan");
        if (l.contains("use_for_intersections")) layer.useForIntersections = boolean(l["use_for_intersections"], "layer.use_for_intersections");
        if (l.contains("thickness")) layer.thickness = number(l["thickness"], "layer.thickness");
        if (l.contains("material_density")) layer.materialDensity = number(l["material_density"], "layer.material_density");
        if (l.contains("alpha_blend")) layer.alphaBlend=int(integer(l["alpha_blend"],"layer.alpha_blend",0,255));
        result.layers.append(layer);
    }
    if(root.contains("layer_order")){
        const auto order=array(root["layer_order"],"layer_order",100000);QVector<Layer> sorted;QSet<EntityId> seen;
        if(order.size()!=result.layers.size())invalid("Incomplete layer order");
        for(const auto& value:order){auto id=integer(value,"layer order ID");const auto* l=result.layer(id);if(!l||seen.contains(id))invalid("Invalid layer order");sorted.append(*l);seen.insert(id);}
        result.layers=std::move(sorted);
    }
    const auto triangleArray = array(root["triangles"], "triangles", 5000000);
    result.triangles.reserve(triangleArray.size());
    for (const auto& item : triangleArray) {
        const auto t = object(item, "triangle");
        const auto vertices = array(t["p"], "triangle.p", 3);
        if (vertices.size() != 3) invalid("triangle must have exactly three vertices");
        result.triangles.append({integer(t["layer"], "triangle.layer"),
                                {vector(vertices[0], "triangle.p[0]"), vector(vertices[1], "triangle.p[1]"), vector(vertices[2], "triangle.p[2]")}});
    }
    if (root.contains("sections")) {
        QSet<quint64> keys;
        for (const auto& item : array(root["sections"], "sections", 10000)) {
            const auto o=object(item,"section");Section section;
            section.kind=int(integer(o["kind"],"section.kind",1,4));
            section.index=int(integer(o["index"],"section.index",0,9999));
            const auto key=quint64(section.kind)*10000+section.index;
            if (keys.contains(key)) invalid("duplicate section");
            keys.insert(key);
            section.distance=number(o["distance"],"section.distance");
            section.showCurvature=boolean(o["show_curvature"],"section.show_curvature");
            section.color=quint32(integer(o["color"],"section.color",0,0xffffff));
            const auto plane=array(o["plane"],"section.plane",4);
            if(plane.size()!=4)invalid("section plane size");
            for(int i=0;i<4;++i)section.plane[i]=number(plane[i],"section.plane");
            if(section.plane[0]==0&&section.plane[1]==0&&section.plane[2]==0)invalid("empty plane normal");
            result.sections.append(std::move(section));
        }
    }
    if(root.contains("display")){
        const auto d=object(root["display"],"display");
        result.bothSides=boolean(d["both_sides"],"display.both_sides");
        result.showCurvature=boolean(d["show_curvature"],"display.show_curvature");
        if(d.contains("show_control_curves"))result.showControlCurves=boolean(d["show_control_curves"],"display.show_control_curves");
        if(d.contains("show_flowlines"))result.showFlowlines=boolean(d["show_flowlines"],"display.show_flowlines");
        if(d.contains("show_normals"))result.showNormals=boolean(d["show_normals"],"display.show_normals");
        if(d.contains("show_markers"))result.showMarkers=boolean(d["show_markers"],"display.show_markers");
        result.curvatureScale=number(d["curvature_scale"],"display.curvature_scale");
        if(result.curvatureScale<0||result.curvatureScale>1e9)invalid("display curvature scale");
        result.curvatureColor=quint32(integer(d["curvature_color"],"display.curvature_color",0,0xffffff));
    }
    qsizetype auxiliaryPointCount=0;
    QSet<EntityId> pointIds;for(const auto& p:result.points)pointIds.insert(p.id);
    for(const auto& p:result.points)for(auto id:{p.anchor,p.lineA,p.lineB})if(id&&!pointIds.contains(id))invalid("unknown constraint point");
    if(root.contains("surface_normals")){
        result.normalColor=quint32(integer(root["normal_color"],"normal_color",0,0xffffff));
        QSet<EntityId> faces;for(const auto& f:result.faces)faces.insert(f.id);
        for(const auto& value:array(root["surface_normals"],"surface_normals",4000000)){
            const auto n=object(value,"surface_normal");auto face=integer(n["face"],"surface_normal.face");
            if(!faces.contains(face))invalid("normal references unknown face");
            result.surfaceNormals.append({face,vector(n["p"],"normal.p"),vector(n["n"],"normal.n")});
        }
    }
    if(root.contains("point_groups"))for(const auto& value:array(root["point_groups"],"point_groups",100000)){
        auto item=object(value,"point_group");PointGroup group;
        group.index=int(integer(item["index"],"group.index",0,100000));
        if(group.index!=result.pointGroups.size())invalid("point group index mismatch");
        group.name=string(item["name"],"group.name",true);group.locked=boolean(item["locked"],"group.locked");QSet<EntityId> seen;
        for(auto value:array(item["points"],"group.points",100000)){
            auto id=integer(value,"group.point");if(!pointIds.contains(id)||seen.contains(id))invalid("invalid group point");seen.insert(id);group.points.append(id);
        }
        result.pointGroups.append(std::move(group));
    }
    if(root.contains("markers"))for(const auto& value:array(root["markers"],"markers",4096)){
        auto item=object(value,"marker");ReferenceMarker marker;marker.index=int(integer(item["index"],"marker.index",0,4095));
        if(marker.index!=result.markers.size())invalid("marker index mismatch");
        marker.visible=boolean(item["visible"],"marker.visible");marker.color=quint32(integer(item["color"],"marker.color",0,0xffffff));
        auto spline=object(item["spline"],"marker.spline");auto points=array(spline["points"],"marker.points",1000000),flags=array(spline["knuckles"],"marker.knuckles",1000000);
        if(points.size()!=flags.size())invalid("marker spline flags mismatch");
        auxiliaryPointCount+=points.size();if(auxiliaryPointCount>1000000)invalid("auxiliary spline point limit");
        for(int i=0;i<points.size();++i){marker.spline.points.append(vector(points[i],"marker.point"));marker.spline.knuckles.append(boolean(flags[i],"marker.knuckle"));}
        result.markers.append(std::move(marker));
    }
    for(bool flow:{false,true}){
        const auto key=flow?"flowlines":"control_curves";if(!root.contains(key))continue;
        QSet<EntityId> ids;
        for(const auto& value:array(root[key],key,4096)){
            auto item=object(value,key);AuxiliaryCurve curve;curve.id=integer(item["id"],"auxiliary.id");uniqueId(ids,curve.id,key);
            curve.name=string(item["name"],"auxiliary.name",true);curve.color=quint32(integer(item["color"],"auxiliary.color",0,0xffffff));
            if(flow){
                curve.sourceView=int(integer(item["view"],"flow.view",0,2));auto p=array(item["source"],"flow.source",2);
                if(p.size()!=2)invalid("flow source size");for(int i=0;i<2;++i)curve.source[i]=number(p[i],"flow.source");
            }else{
                for(auto id:array(item["controls"],"curve.controls",100000)){auto point=integer(id,"curve.control");if(!pointIds.contains(point))invalid("unknown curve control point");curve.controls.append(point);}
                if(curve.controls.size()<2)invalid("short control curve");
            }
            auto spline=object(item["spline"],"auxiliary.spline");const auto points=array(spline["points"],"auxiliary.points",1000000),flags=array(spline["knuckles"],"auxiliary.knuckles",1000000);
            if(points.size()!=flags.size())invalid("auxiliary spline flags mismatch");
            auxiliaryPointCount+=points.size();if(auxiliaryPointCount>1000000)invalid("auxiliary spline point limit");
            for(int i=0;i<points.size();++i){curve.spline.points.append(vector(points[i],"auxiliary.point"));curve.spline.knuckles.append(boolean(flags[i],"auxiliary.knuckle"));}
            (flow?result.flowlines:result.controlCurves).append(std::move(curve));
        }
    }
    if(root.contains("backgrounds"))for(const auto& item:array(root["backgrounds"],"backgrounds",3)){
        auto b=BackgroundImage::fromJson(object(item,"background"));
        if(result.background(b.view))invalid("duplicate background view");result.backgrounds.append(std::move(b));
    }
    if(root.contains("overlay")){
        auto o=object(root["overlay"],"overlay");auto& overlay=result.overlay;overlay.present=true;
        overlay.grid=boolean(o["show_grid"],"overlay.grid");overlay.hydro=boolean(o["show_hydrostatics"],"overlay.hydro");
        overlay.mainParticulars=boolean(o["main_particulars"],"overlay.particulars");
        overlay.gridColor=quint32(integer(o["grid_color"],"overlay.grid_color",0,0xffffff));
        overlay.gridFontColor=quint32(integer(o["grid_font_color"],"overlay.grid_font_color",0,0xffffff));
        overlay.hydroColor=quint32(integer(o["hydro_color"],"overlay.hydro_color",0,0xffffff));
        overlay.pointSize=int(integer(o["point_size"],"overlay.point_size",1,100));
        overlay.fontSize=int(integer(o["font_size"],"overlay.font_size",1,100));
        auto h=object(o["hydro"],"overlay.hydro");overlay.hydroAvailable=boolean(h["available"],"hydro.available");QSet<QString> keys;
        for(auto item:array(h["markers"],"hydro.markers",32)){
            auto m=object(item,"hydro.marker");auto key=string(m["key"],"hydro.key");
            if(keys.contains(key))invalid("duplicate hydro marker");
            keys.insert(key);
            overlay.markers.append({key,string(m["text"],"hydro.text"),vector(m["p"],"hydro.p")});
        }
        for(auto item:array(h["areas"],"hydro.areas",10000)){
            auto a=object(item,"hydro.area");overlay.areas.append({vector(a["p"],"hydro.area.p"),number(a["value"],"hydro.area.value")});
        }
        for(auto item:array(h["path"],"hydro.path",100000))overlay.areaPath.append(vector(item,"hydro.path.p"));
        if(!overlay.hydroAvailable&&(!overlay.markers.isEmpty()||!overlay.areas.isEmpty()||!overlay.areaPath.isEmpty()))invalid("unavailable hydro data contains results");
    }
    result.validate();
    return result;
}

void ModelSnapshot::loadSectionDisplay(const QByteArray& data) {
    if(data.size()>maxSnapshotBytes)invalid("section display size");
    QJsonParseError error;auto document=QJsonDocument::fromJson(data,&error);
    if(error.error!=QJsonParseError::NoError||!document.isObject())invalid("section display JSON");
    auto root=document.object();
    if(root["format"]!="FreeShipSectionDisplay"||integer(root["version"],"section display version")!=1)invalid("section display format");
    auto list=array(root["sections"],"section display",10000);
    if(list.size()!=sections.size())invalid("section display count mismatch");
    auto next=sections;QSet<int> seen;qsizetype count=0,controlCount=0;
    for(auto item:list){
        auto o=object(item,"section display");
        auto kind=integer(o["kind"],"section display kind",1,4),index=integer(o["index"],"section display index",0,9999);
        int found=-1;for(int i=0;i<next.size();++i)if(next[i].kind==int(kind)&&next[i].index==int(index)){found=i;break;}
        if(found<0||seen.contains(found))invalid("unknown or duplicate display section");
        seen.insert(found);
        auto& s=next[found];s.paths.clear();s.splines.clear();auto plane=array(o["plane"],"display plane",4);
        if(plane.size()!=4)invalid("display plane size");
        // Both planes originate as IEEE Single; compare the value, not decimal formatting.
        for(int i=0;i<4;++i)if(float(number(plane[i],"display plane"))!=float(s.plane[i]))invalid("display plane mismatch");
        for(auto path:array(o["paths"],"section paths",100000)){
            auto points=array(path,"section points",1000000);
            if(points.size()<2)invalid("short section path");
            count+=points.size();if(count>4000000)invalid("section display point limit");
            QVector<Vec3> curve;curve.reserve(points.size());
            for(auto p:points)curve.append(vector(p,"section point"));
            s.paths.append(std::move(curve));
        }
        if(o.contains("splines")){
            const auto splines=array(o["splines"],"section splines",100000);
            if(splines.size()!=s.paths.size())invalid("section spline count mismatch");
            for(auto value:splines){
                const auto spline=object(value,"section spline");SectionSpline curve;
                const auto points=array(spline["points"],"spline points",1000000);
                const auto flags=array(spline["knuckles"],"spline knuckles",1000000);
                if(points.size()<2||points.size()!=flags.size())invalid("section spline controls");
                controlCount+=points.size();if(controlCount>4000000)invalid("section spline point limit");
                for(int i=0;i<points.size();++i){curve.points.append(vector(points[i],"spline point"));curve.knuckles.append(boolean(flags[i],"spline knuckle"));}
                s.splines.append(std::move(curve));
            }
        }
    }
    sections=std::move(next);
}

const ControlPoint* ModelSnapshot::point(EntityId id) const {
    for (const auto& p : points) if (p.id == id) return &p;
    return nullptr;
}
const Layer* ModelSnapshot::layer(EntityId id) const {
    for (const auto& l : layers) if (l.id == id) return &l;
    return nullptr;
}
const BackgroundImage* ModelSnapshot::background(int view)const{
    for(const auto& b:backgrounds)if(b.view==view)return &b;return nullptr;
}
void ModelSnapshot::validate() const {
    if(backgrounds.size()>3)invalid("too many backgrounds");
    QSet<int> backgroundViews;for(const auto& b:backgrounds){b.validate();if(backgroundViews.contains(b.view))invalid("duplicate background view");backgroundViews.insert(b.view);}
    if (documentId.isEmpty() || QUuid(documentId).isNull()) invalid("document_id is not a UUID");
    if (revision == 0 || revision > maxJsonInteger) invalid("invalid revision");
    if (units != "metric" && units != "imperial") invalid("unsupported units");
    if (name.contains(QChar(0)) || designer.contains(QChar(0)) || comment.contains(QChar(0)) ||
        textEncoding.isEmpty() || textEncoding.contains(QChar(0))) invalid("invalid project text");
    finiteVector(minimum, "bounds.min"); finiteVector(maximum, "bounds.max");
    if (minimum.x > maximum.x || minimum.y > maximum.y || minimum.z > maximum.z) invalid("reversed bounds");
    for (double n : {length, beam, draft})
        if (!std::isfinite(n) || n < 0 || n > maxCoordinate) invalid("invalid project dimensions");
    if (!std::isfinite(waterlineZ) || std::abs(waterlineZ) > maxCoordinate) invalid("invalid waterline");
    if (underwaterColor > 0xffffff) invalid("invalid underwater color");
    if(underwaterColorAlpha<0||underwaterColorAlpha>255)invalid("invalid underwater opacity");
    for (double n : {waterDensity, appendageCoefficient})
        if (!std::isfinite(n) || n < 0 || n > maxCoordinate) invalid("invalid project coefficient or density");
    if (!std::isfinite(splitSectionLocation) || std::abs(splitSectionLocation) > maxCoordinate ||
        precision < 0 || precision > 3) invalid("invalid project split location or precision");
    QSet<EntityId> pointIds, edgeIds, faceIds, layerIds;
    pointIds.reserve(points.size()); edgeIds.reserve(edges.size()); faceIds.reserve(faces.size()); layerIds.reserve(layers.size());
    for (const auto& p : points) { uniqueId(pointIds, p.id, "points"); finiteVector(p.position, "point position"); }
    for (const auto& l : layers) {
        uniqueId(layerIds, l.id, "layers");
        if (l.color > 0xffffff) invalid("invalid layer color");
        if(l.alphaBlend<0||l.alphaBlend>255)invalid("invalid layer opacity");
        if (l.name.contains(QChar(0))) invalid("invalid layer name");
        for (double n : {l.thickness, l.materialDensity})
            if (!std::isfinite(n) || n < 0 || n > maxCoordinate) invalid("invalid layer thickness or density");
    }
    for (const auto& e : edges) {
        uniqueId(edgeIds, e.id, "edges");
        if (e.a == e.b || !pointIds.contains(e.a) || !pointIds.contains(e.b)) invalid("edge references missing or repeated points");
    }
    for (const auto& f : faces) {
        uniqueId(faceIds, f.id, "faces");
        if (!layerIds.contains(f.layer) || f.points.size() < 3) invalid("face layer or point count is invalid");
        QSet<EntityId> used;
        for (EntityId p : f.points) {
            if (!pointIds.contains(p) || used.contains(p)) invalid("face references missing or repeated points");
            used.insert(p);
        }
    }
    for (const auto& t : triangles) {
        if (!layerIds.contains(t.layer)) invalid("triangle references a missing layer");
        for (const auto& p : t.vertices) finiteVector(p, "triangle vertex");
    }
}
}
