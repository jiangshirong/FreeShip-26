// GPL-2.0-or-later. FBM field/default provenance: FreeShip Plus LoadBinary,
// ModelService.WriteModel, NativeLinesplanInput and OverlaySnapshot.inc.
// Geometry remains in the existing native worker; no UI object is constructed.
#include "publication.h"
#include "geometry_input.h"
#include "fbm_text.h"
#include "hydrostatics.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QProcess>
#include <QSaveFile>
#include <QtEndian>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
#include <stdexcept>
#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace fs::document {
namespace {
[[noreturn]] void bad(const char *message) {throw std::runtime_error(message);}
struct Reader {
    QByteArray bytes;qsizetype at=0;
    QByteArray take(qsizetype n) {if(n<0 || n>bytes.size()-at)bad("Truncated publication field");auto b=bytes.mid(at,n);at+=n;return b;}
    int integer() {auto b=take(4);return qFromLittleEndian<qint32>(b.constData());}
    float real() {auto bits=quint32(integer());float n;std::memcpy(&n,&bits,4);if(!std::isfinite(n))bad("Non-finite publication field");return n;}
    bool flag() {return take(1)[0]!=0;}
    QByteArray string() {return take(integer());}
    QJsonArray point() {const float x=real(),y=real(),z=real();return {x,y,z};}
    int count() {int n=integer();if(n<0 || n>Fbm::MaximumCollectionItems)bad("Publication collection limit exceeded");return n;}
};
QJsonArray point(native_geometry::Vec3 p) {return {p.x,p.y,p.z};}
void save(const QString &name,const QByteArray &bytes) {
    QSaveFile f(name);f.setDirectWriteFallback(false);
    if(!f.open(QIODevice::WriteOnly) || f.write(bytes)!=bytes.size() || !f.commit())bad("Cannot write publication artifact");
}
void save(const QString &name,const QJsonObject &object) {save(name,QJsonDocument(object).toJson(QJsonDocument::Compact));}
QJsonObject readJson(const QString &name,qint64 limit=Fbm::MaximumBytes) {
    QFile f(name);if(!f.open(QIODevice::ReadOnly) || f.size()>limit)bad("Cannot read publication artifact");
    QJsonParseError error;const auto doc=QJsonDocument::fromJson(f.readAll(),&error);
    if(f.error()!=QFile::NoError || error.error!=QJsonParseError::NoError || !doc.isObject())bad("Invalid publication JSON");return doc.object();
}
int resolved(int raw,const QJsonObject &bindings) {
    if(raw>=0 && raw<=0xffffff)return raw;
    const auto value=bindings["resolved_colors"].toObject()[QString::number(raw)];
    if(!value.isDouble() || value.toInteger(-1)<0 || value.toInteger()>0xffffff)bad("Missing system color resolution");return value.toInt();
}
QJsonObject color(int raw,const QJsonObject &bindings) {
    const auto palette=bindings["plate_context"].toObject()["palette"].toArray();
    if(palette.size()!=255)bad("Missing DXF palette");int index=1,best=1000000;
    for(int i=0;i<255;++i) {
        const auto c=palette[i].toInt();int sum=0;
        for(int shift:{0,8,16}) {const int d=((quint32(raw)>>shift)&255)-((quint32(c)>>shift)&255);sum+=d*d;}
        // Squared integral distances have the same ordering as legacy Single
        // sqrt, including ties (all components are 8-bit).
        if(sum<best) {best=sum;index=i+1;}
    }
    return {{"raw",raw},{"value",resolved(raw,bindings)},{"dxf",index}};
}
QJsonArray ordered(const QJsonArray &array) {
    std::vector<QJsonObject> rows;for(auto value:array)rows.push_back(value.toObject());
    std::sort(rows.begin(),rows.end(),[](const auto &a,const auto &b){return a["id"].toInteger()<b["id"].toInteger();});
    QJsonArray out;for(const auto &row:rows)out.append(row);return out;
}
std::vector<QString> names(const Fbm &source,const char *block,size_t size,const QString &encoding) {
    std::vector<QString> result(size);if(source.version()<41)return result;
    Reader r{source.blockBytes(block)};const int n=r.count();
    for(int i=0;i<n;++i) {const int index=r.integer();const auto name=decodeFbmText(r.string(),encoding);
        if(index<0 || size_t(index)>=size)bad("Invalid name index");result[size_t(index)]=name;}
    return result;
}
QJsonObject spline(Reader &r) {
    QJsonArray points,knuckles;const int n=r.count();
    for(int i=0;i<n;++i) {points.append(r.point());knuckles.append(r.flag());}
    return {{"points",points},{"knuckles",knuckles}};
}
}

Publication::Publication(const Fbm &source,const QJsonObject &bindings) {
    const auto context=bindings["presentation_context"].toObject();
    if(!context["colors"].isObject() || !context["texts"].isObject())bad("Missing document presentation context");
    const auto encoding=bindings["fbm_encoding"].toString();const auto net=source.controlNet();
    const auto geometry=documentGeometry(source);const auto settings=documentGeometrySettings(source);const int version=source.version();
    if(!net.faces.empty() && settings.length<=0)bad("非空船体的工程船长必须大于零，才能建立有效计算平面。");
    const auto pointIds=bindings["points"].toArray(),edgeIds=bindings["edges"].toArray(),faceIds=bindings["faces"].toArray(),
               layerIds=bindings["layers"].toArray(),curveIds=bindings["curves"].toArray(),flowIds=bindings["flows"].toArray();
    if(size_t(pointIds.size())!=net.points.size() || size_t(edgeIds.size())!=net.edges.size() || size_t(faceIds.size())!=net.faces.size() ||
       size_t(layerIds.size())!=geometry.layers.size() || size_t(curveIds.size())!=net.curves.size())bad("Publication identity count mismatch");
    const auto nameAt=names(source,"point_names",net.points.size(),encoding),curveNames=names(source,"curve_names",net.curves.size(),encoding);
    QString name,designer,comment,createdBy;bool particulars=false,shade=true;int underwater=bindings["default_underwater_color"].toInt();
    Reader project{source.blockBytes("project_settings")};
    if(version>=2) {
        name=decodeFbmText(project.string(),encoding);designer=decodeFbmText(project.string(),encoding);
        project.take(12);particulars=project.flag();project.take(8);shade=project.flag();underwater=project.integer();project.integer();
        if(version>=6)project.take(5);if(version>=7)project.flag();
        comment=decodeFbmText(project.string(),encoding);createdBy=decodeFbmText(project.string(),encoding);
    }
    model_={{"format","FreeShipModelSnapshot"},{"version",1},{"name",name},{"units",settings.imperial?"imperial":"metric"}};
    QJsonObject projectJson{{"name",name},{"designer",designer},{"comment",comment},{"created_by",createdBy},
        {"units",model_["units"]},{"length",settings.length},{"beam",settings.beam},{"draft",settings.draft},
        {"water_density",settings.density},{"appendage_coefficient",settings.appendage},{"split_section_location",settings.split},
        {"use_default_split_section_location",settings.defaultSplit},{"simplify_intersections",settings.simplifyIntersections},
        {"precision",source.precision()},{"shade_underwater",shade},{"underwater_color",resolved(underwater,bindings)},
        {"save_preview",source.inspect()["preview"].toObject()["enabled"].toBool()},
        {"coefficient_basis",settings.actualCoefficients?1:0},
        {"particulars_set",particulars},{"text_encoding",encoding}};
    Reader alpha{source.blockBytes("project_settings").right(4)};
    const auto projectBytes=source.blockBytes("project_settings");
    projectJson["generate_auxiliary_reports"]=version>=39&&projectBytes.at(projectBytes.size()-(version>=44?5:1))!=0;
    projectJson["underwater_color_alpha"]=version>=44?alpha.integer():bindings.value("default_underwater_alpha").toInt(bindings.value("initial_underwater_alpha").toInt(255));
    Reader visibility{source.blockBytes("visibility")};QJsonObject display,flags;
    display["both_sides"]=visibility.integer()==1;visibility.take(5);
    const bool normals=visibility.flag(),grid=visibility.flag(),diagonals=visibility.flag();Q_UNUSED(diagonals);
    display["show_normals"]=normals;display["show_markers"]=visibility.flag();display["show_curvature"]=visibility.flag();
    display["curvature_scale"]=visibility.real();display["show_control_curves"]=version>=12?visibility.flag():true;
    if(version>=16)visibility.take(4);
    for(const char *key:{"show_hydrostatics","displacement","lateral_area","sectional_areas","metacentric_height","lcf"})flags[key]=version>=17?visibility.flag():true;
    for(const char *key:{"displacement","lateral_area","sectional_areas","metacentric_height","lcf"})projectJson[QString("hydro_")+key]=flags[key];
    display["show_flowlines"]=version>=20?visibility.flag():true;display["curvature_color"]=context["curvature_color"];
    model_["display"]=display;
    QJsonObject overlay{{"show_grid",grid},{"show_hydrostatics",flags["show_hydrostatics"]},{"main_particulars",particulars},
        {"grid_color",context["colors"].toObject()["grid"].toObject()["value"]},{"grid_font_color",context["grid_font_color"]},
        {"hydro_color",context["hydro_color"]},{"point_size",context["point_size"]},{"font_size",context["font_size"]}};
    Hydrostatics hydro(source,bindings);const auto annotations=hydro.presentation(flags,context);overlay["hydro"]=annotations["hydro"];model_["overlay"]=overlay;

    QJsonArray points,edges,faces,layers,curves,gPoints,gEdges,gFaces,gLayers,gCurves;
    std::vector<bool> attached(net.points.size(),false),leak(net.points.size(),false),restricted(net.points.size(),false);
    std::map<std::pair<int,int>,size_t> edgeIndex;std::vector<int> hydroFaceCount(net.edges.size(),0);
    for(size_t i=0;i<net.edges.size();++i) {const auto &e=net.edges[i];edgeIndex.emplace(std::minmax(e.a,e.b),i);attached[e.a]=attached[e.b]=true;}
    for(const auto &f:net.faces)for(size_t i=0;i<f.points.size();++i) {
        const int a=f.points[i],b=f.points[(i+1)%f.points.size()];attached[a]=true;
        if(geometry.layers[f.layer<0?0:size_t(f.layer)].useInHydrostatics) {
            const auto e=edgeIndex.find(std::minmax(a,b));if(e==edgeIndex.end())bad("Missing face edge");++hydroFaceCount[e->second];
        }
    }
    for(size_t i=0;i<net.edges.size();++i)if(hydroFaceCount[i]==1) {
        const auto &e=net.edges[i];for(int p:{e.a,e.b})if(std::abs(net.points[p].value.y)>1e-4)leak[p]=true;
    }
    for(size_t i=0;i<net.points.size();++i) {
        const auto &p=net.points[i].value;
        if(p.locked || p.hardAnchor || p.anchor>=0 || p.lineA>=0 || p.lineB>=0)restricted[i]=true;
        for(int j:{p.anchor,p.lineA,p.lineB})if(j>=0)restricted[size_t(j)]=true;
    }
    native_geometry::Vec3 minimum{},maximum=net.points.empty()?native_geometry::Vec3{1,1,1}:native_geometry::Vec3{};bool first=true;
    for(size_t i=0;i<net.points.size();++i) {
        const auto &p=net.points[i].value;auto position=point({p.x,p.y,p.z});
        const auto reference=[&](int index)->QJsonValue {return index<0?QJsonValue(0):pointIds[index];};
        points.append(QJsonObject{{"id",pointIds[qsizetype(i)]},{"p",position},{"locked",p.locked},{"batch_movable",!restricted[i]},
            {"name",nameAt[i]},{"vertex_type",p.vertexType},{"leak",bool(leak[i])},{"anchor",reference(p.anchor)},
            {"line_a",reference(p.lineA)},{"line_b",reference(p.lineB)},{"hard_anchor",p.hardAnchor}});
        gPoints.append(QJsonObject{{"position",position},{"type",p.vertexType}});
        if(attached[i]) {
            if(first) {minimum=maximum={p.x,p.y,p.z};first=false;}
            else {minimum={std::min(minimum.x,p.x),std::min(minimum.y,p.y),std::min(minimum.z,p.z)};
                maximum={std::max(maximum.x,p.x),std::max(maximum.y,p.y),std::max(maximum.z,p.z)};}
        }
    }
    model_["bounds"]=QJsonObject{{"min",point(minimum)},{"max",point(maximum)}};
    projectJson["waterline_z"]=float(native_geometry::hydrostaticKeel(geometry)+settings.draft);model_["settings"]=projectJson;
    for(size_t i=0;i<net.edges.size();++i) {
        const auto &e=net.edges[i];edges.append(QJsonObject{{"id",edgeIds[qsizetype(i)]},{"a",pointIds[e.a]},{"b",pointIds[e.b]},{"crease",e.crease}});
        gEdges.append(QJsonObject{{"points",QJsonArray{e.a,e.b}},{"crease",e.crease},{"curve",geometry.edges[i].curve}});
    }
    for(size_t i=0;i<net.faces.size();++i) {
        const auto &f=net.faces[i];const int l=f.layer<0?0:f.layer;QJsonArray controls,indices;
        for(int p:f.points) {controls.append(pointIds[p]);indices.append(p);}
        faces.append(QJsonObject{{"id",faceIds[qsizetype(i)]},{"layer",layerIds[l]},{"points",controls}});
        gFaces.append(QJsonObject{{"points",indices},{"layer",l}});
    }
    for(size_t i=0;i<geometry.layers.size();++i) {
        const auto &l=geometry.layers[i];auto label=decodeFbmText(QByteArray::fromStdString(l.name),encoding);
        if(label.isEmpty())label=bindings["layer_name_prefix"].toString()+' '+QString::number(l.layerId);
        layers.append(QJsonObject{{"id",layerIds[qsizetype(i)]},{"name",label},{"color",resolved(l.color,bindings)},
            {"visible",l.surfaceVisible},{"symmetric",l.symmetric},{"developable",l.developable},{"use_in_hydrostatics",l.useInHydrostatics},
            {"show_in_linesplan",l.showInLinesplan},{"use_for_intersections",l.useForIntersections},{"thickness",l.thickness},{"material_density",l.materialDensity}});
        if(l.alphaBlend!=255){auto item=layers.last().toObject();item["alpha_blend"]=int(l.alphaBlend);layers[layers.size()-1]=item;}
        gLayers.append(QJsonObject{{"name",label},{"layer_id",l.layerId},{"color",l.color},{"alpha_blend",int(l.alphaBlend)},
            {"symmetric",l.symmetric},{"surface_visible",l.surfaceVisible},{"control_net_visible",l.controlNetVisible},
            {"use_for_intersections",l.useForIntersections},{"use_in_hydrostatics",l.useInHydrostatics},{"show_in_linesplan",l.showInLinesplan},
            {"developable",l.developable},{"material_density",l.materialDensity},{"thickness",l.thickness}});
    }
    for(size_t i=0;i<net.curves.size();++i) {
        QJsonArray controls,indices;for(int p:net.curves[i].points) {controls.append(pointIds[p]);indices.append(p);}
        curves.append(QJsonObject{{"id",curveIds[qsizetype(i)]},{"name",curveNames[i]},{"controls",controls},{"color",context["control_curve_color"]}});
        gCurves.append(QJsonObject{{"points",indices},{"visible",geometry.curves[i].visible}});
    }
    model_["control_points"]=ordered(points);model_["control_edges"]=ordered(edges);model_["control_faces"]=ordered(faces);model_["layers"]=ordered(layers);
    bool sortedLayers=true;for(qsizetype i=1;i<layerIds.size();++i)if(layerIds[i-1].toDouble()>layerIds[i].toDouble())sortedLayers=false;
    if(!sortedLayers)model_["layer_order"]=layerIds;
    model_["control_curves"]=curves;model_["normal_color"]=context["normal_color"];
    QJsonArray groups,markers,backgrounds,flows,flowDefinitions,sections,sectionInput;
    if(version>=42) {
        Reader r{source.blockBytes("point_groups")};const int n=r.count();
        for(int i=0;i<n;++i) {
            const auto label=decodeFbmText(r.string(),encoding);const int count=r.count();QJsonArray ids;bool locked=count>0;
            for(int j=0;j<count;++j) {bool ok=false;const int index=r.string().toInt(&ok);if(!ok || index<0 || index>=pointIds.size())bad("Invalid group point");
                ids.append(pointIds[index]);locked=locked && net.points[size_t(index)].value.locked;}
            groups.append(QJsonObject{{"index",i},{"name",label},{"locked",locked},{"points",ids}});
        }
    }
    if(version>=11) {
        Reader r{source.blockBytes("markers")};const int n=r.count();
        for(int i=0;i<n;++i) {
            const bool visible=r.flag();if(version>=21)r.flag();r.flag();r.real();
            markers.append(QJsonObject{{"index",i},{"visible",visible},{"color",context["marker_color"]},{"spline",spline(r)}});
        }
    }
    if(version>=20) {
        Reader r{source.blockBytes("background_images")};int n=r.count();
        for(int i=0;i<n;++i) {
            const int view=r.integer();const bool visible=r.flag();const int quality=r.integer(),x=r.integer(),y=r.integer();
            const float scale=r.real();const int alpha=r.integer();const bool transparent=r.flag();const int raw=r.integer(),tolerance=r.integer();
            r.take(8);const auto jpeg=r.take(r.integer());
            backgrounds.append(QJsonObject{{"view",view},{"visible",visible},{"quality",quality},{"origin",QJsonArray{x,y}},
                {"scale",scale},{"alpha",alpha},{"transparent",transparent},{"transparent_color",resolved(raw,bindings)},
                {"tolerance",tolerance},{"jpeg",QString::fromLatin1(jpeg.toBase64())}});
        }
        r={source.blockBytes("flowlines")};n=r.count();if(n!=flowIds.size())bad("Flow identity count mismatch");
        for(int i=0;i<n;++i) {
            const float x=r.real(),y=r.real();const int view=r.integer();r.flag();r.flag();r.take(qsizetype(r.count())*13);
            QJsonObject definition{{"id",flowIds[i]},{"view",view},{"source",QJsonArray{x,y}}};flowDefinitions.append(definition);
            definition["name"]="";definition["color"]=0xff0000;flows.append(definition);
        }
    }
    const char *blocks[]{"stations","buttocks","waterlines","diagonals"};const char *colors[]{"station","buttock","waterline","diagonal"};
    for(int kind=1;kind<=4;++kind) {
        if(kind==4 && version<9)continue;Reader r{source.blockBytes(blocks[kind-1])};const int n=r.count();QJsonArray input;
        for(int i=0;i<n;++i) {
            const int storedKind=r.integer();const bool curvature=version>=11?r.flag():false;
            const float a=r.real(),b=r.real(),c=r.real(),d=r.real();const QJsonArray plane{a,b,c,d};r.flag();const int splines=r.count();
            const int dimensions=version>=6 && storedKind>=1 && storedKind<=3?2:3;
            for(int j=0;j<splines;++j)r.take(qsizetype(r.count())*(dimensions*4+1));
            sections.append(QJsonObject{{"kind",kind},{"index",i},{"plane",plane},{"distance",kind==4?-d/c:-d},{"show_curvature",curvature},
                {"color",context["colors"].toObject()[colors[kind-1]].toObject()["value"]}});
            input.append(QJsonObject{{"kind",kind},{"order",i},{"plane",plane}});
        }
        for(qsizetype i=input.size();i>0;--i)sectionInput.prepend(input[i-1]);
    }
    model_["point_groups"]=groups;model_["markers"]=markers;model_["backgrounds"]=backgrounds;model_["flowlines"]=flows;model_["sections"]=sections;
    auto inputColors=context["colors"].toObject();inputColors["underwater"]=color(underwater,bindings);
    QJsonArray layerFaces=bindings["layer_faces"].toArray();
    if(layerFaces.isEmpty())for(qsizetype l=0;l<layerIds.size();++l) {
        QJsonArray ids;for(size_t i=0;i<geometry.faces.size();++i)if(geometry.faces[i].layer==size_t(l))ids.append(faceIds[qsizetype(i)]);layerFaces.append(ids);
    }
    input_={{"format","FreeShipLinesplanInput"},{"version",1},
        {"geometry",QJsonObject{{"format","FreeShipGeometryInput"},{"version",1},{"subdivision_mode",int(geometry.subdivisionMode)},
            {"precision",source.precision()},{"desired_level",int(geometry.desiredLevel)},
            {"points",gPoints},{"edges",gEdges},{"faces",gFaces},{"layers",gLayers},{"curves",gCurves}}},
        {"settings",QJsonObject{{"draft",settings.draft},{"split",settings.split},{"crease_split",annotations["crease_split"]},
            {"imperial",settings.imperial},{"simplify",settings.simplifyIntersections},{"shade_underwater",shade}}},
        {"colors",inputColors},{"texts",context["texts"]},{"sections",sectionInput},
        {"surface_display",QJsonObject{{"show_normals",normals},{"faces",faceIds},{"layers",layerIds},{"curves",curveIds},
            {"flows",flowDefinitions},{"layer_faces",layerFaces}}}};
}

QJsonObject Publication::generate(const QString &directory,const QString &cacheDirectory,const QString &documentId,quint64 revision) const {
    auto input=input_,model=model_;input["document_id"]=model["document_id"]=documentId;input["revision"]=model["revision"]=qint64(revision);
    const QDir dir(directory);save(dir.filePath("native-input.json"),input);
    QProcess worker;worker.setProgram(QCoreApplication::applicationDirPath()+"/FreeShipNativeLinesplan.exe");
    worker.setArguments({dir.filePath("native-input.json"),directory,cacheDirectory,QString::number(QCoreApplication::applicationPid())});
    worker.setStandardOutputFile(QProcess::nullDevice());worker.setStandardErrorFile(dir.filePath("worker.log"));
#ifdef Q_OS_WIN
    worker.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments *a){a->flags|=CREATE_NO_WINDOW;});
#endif
    worker.start();if(!worker.waitForStarted(10000))bad("Cannot start native scene worker");
    if(!worker.waitForFinished(120000)) {worker.kill();worker.waitForFinished(2000);bad("Native scene worker timed out");}
    if(worker.exitStatus()!=QProcess::NormalExit || worker.exitCode()!=0) {
        auto report=readJson(dir.filePath("native-report.json"),1024*1024);
        throw std::runtime_error(("Native scene generation failed: "+report["error"].toString()).toStdString());
    }
    const auto surface=readJson(dir.filePath("surface.json"));
    // Surface geometry is revision-neutral in the worker cache. Its checksum
    // and document identity are carried by native-report, validated by Service.
    if(surface["format"]!="FreeShipSurfaceDisplay" || surface["version"]!=1)bad("Unsupported native display format");
    model["triangles"]=surface["triangles"];model["surface_normals"]=surface["surface_normals"];
    for(const char *kind:{"control_curves","flowlines"}) {
        auto array=model[kind].toArray();const auto generated=surface[kind].toArray();
        if(array.size()!=generated.size())bad("Native auxiliary curve count mismatch");
        for(qsizetype i=0;i<array.size();++i) {auto item=array[i].toObject();const auto data=generated[i].toObject();
            if(item["id"]!=data["id"])bad("Native auxiliary curve identity mismatch");item["spline"]=data["spline"];array[i]=item;}
        model[kind]=array;
    }
    model["diagnostics"]=QJsonObject{{"legacy_triangle_count",0},{"surface_engine","cpp-native"},{"flow_engine","cpp-native"},
        {"source_child_faces",surface["child_faces"]},{"source_triangle_count",surface["triangles"].toArray().size()},
        {"forms_created",0},{"viewports_created",0},{"snapshot_engine","cpp-publication"}};
    save(dir.filePath("model.json"),model);
    return {{"document_id",documentId},{"revision",qint64(revision)},{"model",dir.filePath("model.json")},
        {"scene",dir.filePath("scene.fslp")},{"dxf",dir.filePath("reference.dxf")},{"native_report",dir.filePath("native-report.json")},
        {"sections",dir.filePath("sections.json")},{"surface",dir.filePath("surface.json")},{"snapshot_engine","cpp-publication"}};
}
}
