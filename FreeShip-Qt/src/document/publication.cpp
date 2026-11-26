// GPL-2.0-or-later. FBM field/default provenance: FreeShip Plus LoadBinary,
// ModelService.WriteModel, NativeLinesplanInput and OverlaySnapshot.inc.
// Geometry remains in the existing native worker; no UI object is constructed.
#include "publication.h"
#include "group_fixes.h"
#include "core/platform_process.h"
#include "core/snapshot_surface.h"
#include "geometry_input.h"
#include "fbm_text.h"
#include "hydrostatics.h"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QProcess>
#include <QSaveFile>
#include <QScopeGuard>
#include <QThread>
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
QByteArray readBytes(const QString &name,qint64 limit) {
    QFile f(name);if(!f.open(QIODevice::ReadOnly) || f.size()>limit)bad("Cannot read publication artifact");
    auto bytes=f.readAll();if(f.error()!=QFile::NoError||bytes.size()>limit)bad("Cannot read publication artifact");
    return bytes;
}
QJsonObject readJson(const QString &name,qint64 limit=Fbm::MaximumBytes) {
    QJsonParseError error;const auto doc=QJsonDocument::fromJson(readBytes(name,limit),&error);
    if(error.error!=QJsonParseError::NoError || !doc.isObject())bad("Invalid publication JSON");
    return doc.object();
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

Publication::Publication(const Fbm &source,const QJsonObject &bindings,const Publication* previous,bool parallelPresentation,bool placement) {
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
    projectJson["principal_dimensions"]=source.calculationParameters()["principal_dimensions"].toObject();
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
    if(previous&&previous->hydro_->updateCompatibleSource(source,bindings)){hydro_=previous->hydro_;hydroReused_=true;}
    else hydro_=std::make_shared<Hydrostatics>(source,bindings);
    QJsonObject annotations;
    if(parallelPresentation){
        const auto split=hydro_->presentationCreaseSplit();annotations["crease_split"]=split;
        // The annotation task owns its solver until publication has finished;
        // no other caller can use hydro_ until generate() joins this future.
        presentation_=std::async(std::launch::async,[hydro=hydro_,flags,context,split]{return hydro->presentation(flags,context,split);});
    }else annotations=hydro_->presentation(flags,context);
    overlay["hydro"]=annotations["hydro"];model_["overlay"]=overlay;

    QJsonArray points,edges,faces,layers,curves,gPoints,gEdges,gFaces,gLayers,gCurves;
    std::vector<bool> attached(net.points.size(),false),leak(net.points.size(),false),restricted(net.points.size(),false);
    struct FaceCounts { int original=0, mirrored=0; };
    std::map<std::pair<int,int>,size_t> edgeIndex;
    std::vector<FaceCounts> hydroFaceCounts(net.edges.size());
    const auto onCenterline=[&](int p){return std::abs(net.points[p].value.y)<=1e-4;};
    for(size_t i=0;i<net.edges.size();++i) {const auto &e=net.edges[i];edgeIndex.emplace(std::minmax(e.a,e.b),i);attached[e.a]=attached[e.b]=true;}
    for(const auto &f:net.faces) {
        const auto &layer=geometry.layers[f.layer<0?0:size_t(f.layer)];
        const bool mirror=layer.useInHydrostatics&&layer.symmetric&&
            std::any_of(f.points.begin(),f.points.end(),[&](int p){return !onCenterline(p);});
        for(size_t i=0;i<f.points.size();++i) {
            const int a=f.points[i],b=f.points[(i+1)%f.points.size()];
            attached[a]=true;
            if(!layer.useInHydrostatics)continue;
            const auto edge=edgeIndex.find(std::minmax(a,b));
            if(edge==edgeIndex.end())bad("Missing face edge");
            auto &counts=hydroFaceCounts[edge->second];
            ++counts.original;
            counts.mirrored+=mirror;
        }
    }
    for(size_t i=0;i<net.edges.size();++i) {
        const auto &edge=net.edges[i];
        const auto &counts=hydroFaceCounts[i];
        // Reflected faces share a centerline edge. Off the centerline, the
        // original and reflected edges can have different face counts.
        const bool open=onCenterline(edge.a)&&onCenterline(edge.b)
            ? counts.original+counts.mirrored==1
            : counts.original==1||counts.mirrored==1;
        if(open)leak[edge.a]=leak[edge.b]=true;
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
    int lastLayerNumber=bindings.value("last_used_layer_id").toInt(-1);
    for(size_t i=0;i<geometry.layers.size();++i) {
        lastLayerNumber=std::max(lastLayerNumber,geometry.layers[i].layerId);
        const auto &l=geometry.layers[i];auto label=decodeFbmText(QByteArray::fromStdString(l.name),encoding);
        const bool generated=label.isEmpty();
        if(generated)label=bindings["layer_name_prefix"].toString()+' '+QString::number(l.layerId);
        layers.append(QJsonObject{{"id",layerIds[qsizetype(i)]},{"name",label},{"color",resolved(l.color,bindings)},
            {"visible",l.surfaceVisible},{"symmetric",l.symmetric},{"developable",l.developable},{"use_in_hydrostatics",l.useInHydrostatics},
            {"show_in_linesplan",l.showInLinesplan},{"use_for_intersections",l.useForIntersections},{"thickness",l.thickness},{"material_density",l.materialDensity}});
        if(generated||l.alphaBlend!=255){auto item=layers.last().toObject();
            if(generated)item["default_name_number"]=l.layerId;
            if(l.alphaBlend!=255)item["alpha_blend"]=int(l.alphaBlend);
            layers[layers.size()-1]=item;}
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
    model_["last_layer_number"]=lastLayerNumber;
    model_["new_layer_color"]=resolved(bindings["default_layer_color"].toInt(),bindings);
    model_["control_points"]=ordered(points);model_["control_edges"]=ordered(edges);model_["control_faces"]=ordered(faces);model_["layers"]=ordered(layers);
    bool sortedLayers=true;for(qsizetype i=1;i<layerIds.size();++i)if(layerIds[i-1].toDouble()>layerIds[i].toDouble())sortedLayers=false;
    if(!sortedLayers)model_["layer_order"]=layerIds;
    model_["control_curves"]=curves;model_["normal_color"]=context["normal_color"];
    QJsonArray groups,markers,backgrounds,flows,flowDefinitions,sections,sectionInput;
    if(version>=42) {
        const auto fixes=GroupFixes::read(source.calculationParameters());
        Reader r{source.blockBytes("point_groups")};const int n=r.count();
        for(int i=0;i<n;++i) {
            const auto label=decodeFbmText(r.string(),encoding);const int count=r.count();QJsonArray ids;const bool locked=GroupFixes::has(fixes.fixed,i);
            for(int j=0;j<count;++j) {bool ok=false;const int index=r.string().toInt(&ok);if(!ok || index<0 || index>=pointIds.size())bad("Invalid group point");
                ids.append(pointIds[index]);}
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
    const auto underwaterDisplay=context["underwater_display"].toObject();
    if(!underwaterDisplay.isEmpty())model_["underwater_display"]=underwaterDisplay;
    auto inputColors=context["colors"].toObject();
    inputColors["underwater"]=color(underwaterDisplay["color"].toInt(underwater),bindings);
    QJsonArray layerFaces=bindings["layer_faces"].toArray();
    if(layerFaces.isEmpty())for(qsizetype l=0;l<layerIds.size();++l) {
        QJsonArray ids;for(size_t i=0;i<geometry.faces.size();++i)if(geometry.faces[i].layer==size_t(l))ids.append(faceIds[qsizetype(i)]);layerFaces.append(ids);
    }
    input_={{"format","FreeShipLinesplanInput"},{"version",1},
        {"geometry",QJsonObject{{"format","FreeShipGeometryInput"},{"version",1},{"subdivision_mode",int(geometry.subdivisionMode)},
            {"precision",source.precision()},{"desired_level",int(geometry.desiredLevel)},
            {"points",gPoints},{"edges",gEdges},{"faces",gFaces},{"layers",gLayers},{"curves",gCurves}}},
        {"settings",QJsonObject{{"draft",settings.draft},{"split",settings.split},{"crease_split",annotations["crease_split"]},
            {"imperial",settings.imperial},{"simplify",settings.simplifyIntersections},
            {"shade_underwater",underwaterDisplay["shade"].toBool(shade)}}},
        {"colors",inputColors},{"texts",context["texts"]},{"sections",sectionInput},
        {"surface_display",QJsonObject{{"show_normals",normals},{"faces",faceIds},{"layers",layerIds},{"curves",curveIds},
            {"flows",flowDefinitions},{"layer_faces",layerFaces}}}};
    if(previous&&placement&&previous->canReuseMetadata()&&!net.faces.empty()&&geometry.desiredLevel>0&&
       gPoints.size()==previous->input_["geometry"].toObject()["points"].toArray().size()+1){
        // The add-point command introduces only an unattached vertex. Compare
        // the entire drawing input after removing that one appended vertex:
        // face/edge/curve bindings, styles, cut planes and hydro settings must
        // all match. Refined surfaces only emit referenced vertices. Empty
        // and unsubdivided models still use the ordinary generator.
        auto drawingInput=input_;
        auto drawingGeometry=drawingInput["geometry"].toObject();
        gPoints.removeLast();drawingGeometry["points"]=gPoints;drawingInput["geometry"]=drawingGeometry;
        placementGeometryReused_=drawingInput==previous->input_;
    }
    if(previous&&(input_==previous->input_||placementGeometryReused_)) {
        artifacts_=previous->artifacts_;generatedSurface_=previous->generatedSurface_;generatedMetadata_=previous->generatedMetadata_;
    }
}

QJsonObject Publication::generate(const QString &directory,const QString &cacheDirectory,const QString &documentId,quint64 revision,const std::function<void(const QJsonObject&)>& displayReady,bool displayOnly,QJsonObject* timings,std::unique_ptr<fs::SnapshotSurface>* parsedSurface) const {
    const auto joinPresentation=qScopeGuard([this]{if(presentation_.valid())presentation_.wait();});
    QElapsedTimer clock;clock.start();
    auto input=input_,model=model_;input["document_id"]=model["document_id"]=documentId;input["revision"]=model["revision"]=qint64(revision);
    const QDir dir(directory);if(displayReady)input["first_frame"]=true;if(displayOnly)input["display_only"]=true;save(dir.filePath("native-input.json"),input);
    const bool reuse=!displayOnly&&!artifacts_.empty();
    bool modelWritten=false;const bool parallelPresentation=presentation_.valid();
    double modelWriteMs=0;
    const auto completeDisplay=[&]{
        if(modelWritten)return;QElapsedTimer part;part.start();
        double checkpoint=0;
        const auto mark=[&](const char* key){
            if(timings){const double now=part.nsecsElapsed()/1e6;timings->insert(QLatin1String(key),now-checkpoint);checkpoint=now;}
        };
    // Decode the worker's result while the independently computed annotations
    // can still run, and retain these exact bytes only for this transaction.
    const auto surface=[&]{
        if(reuse&&!placementGeometryReused_)return generatedSurface_;
        const auto path=dir.filePath("surface.json");
        constexpr auto displayLimit=64*1024*1024;
        const auto size=QFileInfo(path).size();
        if(!parsedSurface||size<1024*1024||size>displayLimit)return readJson(path);
        *parsedSurface=std::make_unique<fs::SnapshotSurface>(fs::SnapshotSurface::parse(readBytes(path,displayLimit)));
        return (*parsedSurface)->object();
    }();
    mark("publication_surface_load_ms");
    if(presentation_.valid()){
        const auto annotations=presentation_.get();auto overlay=model["overlay"].toObject();
        overlay["hydro"]=annotations["hydro"];model["overlay"]=overlay;
        // Retain the same completed metadata for later edit/reuse comparisons.
        model_["overlay"]=overlay;
    }
    mark("publication_annotations_wait_ms");
    generatedSurface_=surface;
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
    mark("publication_model_assemble_ms");
    const auto bytes=parsedSurface&&*parsedSurface?(*parsedSurface)->encodeModel(model):QJsonDocument(model).toJson(QJsonDocument::Compact);
    mark("publication_model_encode_ms");
    save(dir.filePath("model.json"),bytes);
    mark("publication_model_write_ms");
    generatedMetadata_=bytes.size()<=64LL*1024*1024?fs::SnapshotMetadata::capture(bytes,model["settings"].toObject(),model["name"].toString()):fs::SnapshotMetadata{};
    mark("publication_metadata_index_ms");
        modelWriteMs+=part.nsecsElapsed()/1e6;modelWritten=true;
    };
    const auto deliverDisplay=[&]{
        if(!displayReady||modelWritten||!QFileInfo::exists(dir.filePath("display-ready.json")))return;
        completeDisplay();
        displayReady(QJsonObject{{"model",dir.filePath("model.json")},{"sections",dir.filePath("sections.json")},
            {"surface",dir.filePath("surface.json")},{"native_report",dir.filePath("display-ready.json")}});
    };
    if(reuse)writeArtifacts(directory,documentId,revision);
    else {
    QProcess worker;worker.setProgram(fs::platform::siblingExecutable("FreeShipNativeLinesplan"));
    worker.setArguments({dir.filePath("native-input.json"),directory,cacheDirectory,QString::number(QCoreApplication::applicationPid())});
    worker.setStandardOutputFile(QProcess::nullDevice());worker.setStandardErrorFile(dir.filePath("worker.log"));
#ifdef Q_OS_WIN
    worker.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments *a){a->flags|=CREATE_NO_WINDOW;});
#endif
    worker.start();if(!worker.waitForStarted(10000))bad("Cannot start native scene worker");
    QElapsedTimer workerTimeout;workerTimeout.start();
    while(!worker.waitForFinished(10)){
        deliverDisplay();
        if(workerTimeout.elapsed()>120000){worker.kill();worker.waitForFinished(2000);bad("Native scene worker timed out");}
    }
    deliverDisplay();
    if(worker.exitStatus()!=QProcess::NormalExit || worker.exitCode()!=0) {
        auto report=readJson(dir.filePath("native-report.json"),1024*1024);
        throw std::runtime_error(("Native scene generation failed: "+report["error"].toString()).toStdString());
    }
    }
    const double workerMs=clock.nsecsElapsed()/1e6-modelWriteMs;
    completeDisplay();
    return {{"document_id",documentId},{"revision",qint64(revision)},{"model",dir.filePath("model.json")},
        {"drawing_deferred",displayOnly},{"scene",displayOnly?QString{}:dir.filePath("scene.fslp")},{"dxf",displayOnly?QString{}:dir.filePath("reference.dxf")},{"native_report",dir.filePath("native-report.json")},
        {"sections",dir.filePath("sections.json")},{"surface",dir.filePath("surface.json")},{"snapshot_engine","cpp-publication"},{"artifacts_reused",reuse},{"worker_ms",reuse?0:workerMs},{"snapshot_write_ms",modelWriteMs},
        {"presentation_parallel",parallelPresentation},{"display_early",bool(displayReady)},
        {"placement_geometry_reused",placementGeometryReused_}};
}
std::optional<QJsonObject> Publication::metadataChanges(const Publication &previous) const {
    if(artifacts_.empty()||input_!=previous.input_)return {};
    auto a=model_,b=previous.model_;auto sa=a["settings"].toObject(),sb=b["settings"].toObject();QJsonObject changes;
    for(const char *key:{"name","designer","comment","created_by","save_preview","generate_auxiliary_reports"}) {
        if(sa[key]!=sb[key])changes[key]=sa[key];sa.remove(key);sb.remove(key);
    }
    a.remove("name");b.remove("name");a["settings"]=sa;b["settings"]=sb;
    if(a!=b)return {};
    return changes;
}
namespace {
QByteArray replayReport(const QByteArray &bytes,const QString &documentId,quint64 revision) {
    auto report=QJsonDocument::fromJson(bytes).object();
    report["document_id"]=documentId;report["revision"]=qint64(revision);report["artifacts_reused"]=true;
    for(auto i=report.begin();i!=report.end();++i)if(i.key().endsWith("_ms"))i.value()=0;
    return QJsonDocument(report).toJson(QJsonDocument::Compact);
}
// A replay writes only to the caller's new private transaction directory.
// The service still promotes the complete set after every write has succeeded.
void writeReplayFile(const QString &path,const QByteArray &bytes) {
    QFile file(path);
    if(!file.open(QIODevice::WriteOnly|QIODevice::NewOnly)||file.write(bytes)!=bytes.size()||!file.flush())
        bad("Cannot write history publication artifact");
    file.close();
    if(file.error()!=QFile::NoError)bad("Cannot close history publication artifact");
}
struct ReplayWork {
    double inflate=0,encode=0,write=0;
    void add(const ReplayWork &other) {inflate+=other.inflate;encode+=other.encode;write+=other.write;}
};
QJsonObject frameResult(const QDir &dir,const QString &documentId,quint64 revision,double elapsed) {
    return {{"document_id",documentId},{"revision",qint64(revision)},{"model",dir.filePath("model.json")},
        {"scene",dir.filePath("scene.fslp")},{"dxf",dir.filePath("reference.dxf")},{"native_report",dir.filePath("native-report.json")},
        {"sections",dir.filePath("sections.json")},{"surface",dir.filePath("surface.json")},{"snapshot_engine","cpp-publication"},
        {"worker_ms",0},{"snapshot_write_ms",elapsed},{"artifacts_reused",true}};
}
}
void PublicationFrame::writeArtifacts(const QString &directory,const QString &documentId,quint64 revision) const {
    const QDir dir(directory);
    for(const auto &[name,bytes]:artifacts_)
        save(dir.filePath(name),name=="native-report.json"?replayReport(bytes,documentId,revision):bytes);
}
void Publication::retainValidatedArtifacts(const QString &directory) {
    if(generatedMetadata_.empty()){artifacts_.clear();return;}
    std::map<QString,QByteArray> retained;qsizetype total=0;
    for(const char *name:{"scene.fslp","reference.dxf","sections.json","surface.json","native-report.json"}) {
        QFile f(QDir(directory).filePath(name));
        // This optimization must not create an unbounded second geometry cache.
        if(!f.open(QIODevice::ReadOnly)||f.size()>64*1024*1024-total){artifacts_.clear();return;}
        auto bytes=f.readAll();if(f.error()!=QFile::NoError){artifacts_.clear();return;}
        total+=bytes.size();retained.emplace(name,std::move(bytes));
    }
    artifacts_=std::move(retained);
}
QJsonObject Publication::generateMetadata(const QString &directory,const QJsonObject &texts,
                                        const QString &documentId,quint64 revision) {
    if(!canReuseMetadata())bad("No validated metadata publication");
    QElapsedTimer clock;clock.start();
    const auto baseHash=QString::fromLatin1(QCryptographicHash::hash(generatedMetadata_.bytes(),QCryptographicHash::Sha256).toHex());
    auto settings=model_["settings"].toObject();
    for(auto i=texts.begin();i!=texts.end();++i) {
        if(i.key()=="save_preview"||i.key()=="generate_auxiliary_reports") {if(!i.value().isBool())bad("Invalid project flag");}
        else {
            if(i.key()!="name"&&i.key()!="designer"&&i.key()!="comment"&&i.key()!="created_by")bad("Non-text metadata edit");
            if(!i.value().isString()||i.value().toString().contains(QChar(0)))bad("Invalid project text");
        }
        settings[i.key()]=i.value();
    }
    auto next=generatedMetadata_.updated(texts,revision);
    auto result=writeFrame(directory,documentId,revision,next);
    generatedMetadata_=std::move(next);
    model_["settings"]=settings;model_["name"]=settings["name"];
    result["metadata_reused"]=true;result["metadata_base_sha256"]=baseHash;result["metadata_patch"]=texts;
    result["snapshot_write_ms"]=clock.nsecsElapsed()/1e6;return result;
}
PublicationFrame PublicationFrame::packedForHistory() const {
    auto frame=*this;frame.packed_=true;
    // These two buffers dominate large history entries. Pack them independently
    // of the model, using exactly the same codec and existing history budget.
    std::map<QString,std::future<QByteArray>> workers;
    if(QThread::idealThreadCount()>=4){
        for(const auto& name:{QStringLiteral("scene.fslp"),QStringLiteral("surface.json")}) {
            const auto it=artifacts_.find(name);
            if(it!=artifacts_.end()&&it->second.size()>=1024*1024){
                try{workers.emplace(name,std::async(std::launch::async,[bytes=it->second]{return qCompress(bytes,1);}));}
                catch(const std::system_error&){/* Pack synchronously if a worker is unavailable. */}
            }
        }
    }
    frame.generatedMetadata_=generatedMetadata_.packedForHistory();
    for(auto& [name,data]:frame.artifacts_){
        const auto worker=workers.find(name);
        data=worker==workers.end()?qCompress(data,1):worker->second.get();
    }
    return frame;
}
qsizetype PublicationFrame::retainedBytes() const {
    qsizetype n=generatedMetadata_.bytes().size()+QJsonDocument(input_).toJson(QJsonDocument::Compact).size();
    for(const auto &item:artifacts_)n+=item.second.size();return n;
}
int PublicationFrame::restoredLayerNumber() const {
    // Restoring a file uses its layer IDs, not the live allocation high-water mark.
    int file=-1;
    for(const auto &v:input_["geometry"].toObject()["layers"].toArray())file=std::max(file,v.toObject()["layer_id"].toInt());
    return file;
}
QJsonObject PublicationFrame::replay(const QString &directory,const QString &id,quint64 revision) const {
    QElapsedTimer total;total.start();const QDir dir(directory);
    const auto writeArtifact=[packed=packed_,&dir,&id,revision](const QString &name,const QByteArray &stored){
        QElapsedTimer clock;clock.start();ReplayWork work;
        auto bytes=packed?qUncompress(stored):stored;
        if(bytes.isEmpty())bad("Invalid history artifact");
        work.inflate=packed?clock.nsecsElapsed()/1e6:0;clock.restart();
        if(name=="native-report.json")bytes=replayReport(bytes,id,revision);
        work.encode=clock.nsecsElapsed()/1e6;clock.restart();
        writeReplayFile(dir.filePath(name),bytes);
        work.write=clock.nsecsElapsed()/1e6;return work;
    };
    // Keep the large, independent display files off the model metadata lane.
    // At most two workers run; a failure joins them before the stage is removed.
    std::array<std::future<ReplayWork>,2> display;
    for(const auto &name:{QStringLiteral("scene.fslp"),QStringLiteral("surface.json")}) {
        auto found=artifacts_.find(name);
        if(packed_&&found!=artifacts_.end()&&found->second.size()>=256*1024)
            display[name=="scene.fslp"?0:1]=std::async(std::launch::async,writeArtifact,name,found->second);
    }
    QElapsedTimer clock;clock.start();ReplayWork work;
    const auto expanded=packed_?generatedMetadata_.expandedFromHistory():generatedMetadata_;
    work.inflate=packed_?clock.nsecsElapsed()/1e6:0;clock.restart();
    const auto layer=restoredLayerNumber();
    const auto model=expanded.updated({},revision,layer);
    auto input=input_;input["document_id"]=id;input["revision"]=qint64(revision);
    const auto inputBytes=QJsonDocument(input).toJson(QJsonDocument::Compact);
    work.encode=clock.nsecsElapsed()/1e6;clock.restart();
    writeReplayFile(dir.filePath("model.json"),model.bytes());
    writeReplayFile(dir.filePath("native-input.json"),inputBytes);
    work.write=clock.nsecsElapsed()/1e6;
    for(const auto &[name,bytes]:artifacts_) {
        if((name=="scene.fslp"&&display[0].valid())||(name=="surface.json"&&display[1].valid()))continue;
        work.add(writeArtifact(name,bytes));
    }
    for(auto &job:display)if(job.valid())work.add(job.get());
    auto result=frameResult(dir,id,revision,total.nsecsElapsed()/1e6);
    // Summed per-lane time; these stages can overlap on dense hulls.
    result["history_layer_number"]=layer;
    result["history_inflate_work_ms"]=work.inflate;result["history_encode_work_ms"]=work.encode;
    result["history_file_write_work_ms"]=work.write;
    return result;
}
QJsonObject PublicationFrame::writeFrame(const QString &directory,const QString &documentId,quint64 revision,const fs::SnapshotMetadata &model) const {
    QElapsedTimer clock;clock.start();const QDir dir(directory);
    auto input=input_;input["document_id"]=documentId;input["revision"]=qint64(revision);
    writeArtifacts(directory,documentId,revision);
    save(dir.filePath("native-input.json"),input);save(dir.filePath("model.json"),model.bytes());
    return frameResult(dir,documentId,revision,clock.nsecsElapsed()/1e6);
}
}
