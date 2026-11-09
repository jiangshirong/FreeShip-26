#include "hull_view.h"
#include "underwater_color.h"
#include "background_geometry.h"
#include "body_projection.h"
#include "curvature_comb.h"
#include "auxiliary_geometry.h"
#include <QPainterPathStroker>
#include "hull_gpu.h"
#include "view_overlay.h"
#include <QTimer>
#include <QHideEvent>
#include <QShowEvent>
#include <QJsonArray>
#include <QActionGroup>
#include <QContextMenuEvent>
#include <QMenu>
#include <QElapsedTimer>
#include <QFocusEvent>
#include <QHash>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QSet>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <unordered_map>
#include <vector>
#include <tuple>

namespace {
constexpr double Pi = 3.14159265358979323846;
const QColor Background(249, 250, 251);
fs::Vec3 subtract(fs::Vec3 a, fs::Vec3 b) { return {a.x-b.x, a.y-b.y, a.z-b.z}; }
double dot(fs::Vec3 a, fs::Vec3 b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
fs::Vec3 cross(fs::Vec3 a, fs::Vec3 b) {
    return {a.y*b.z-a.z*b.y, a.z*b.x-a.x*b.z, a.x*b.y-a.y*b.x};
}
fs::Vec3 mirrored(fs::Vec3 point) { point.y = -point.y; return point; }
QColor sourceColor(quint32 color) {
    return QColor(color & 255, (color >> 8) & 255, (color >> 16) & 255);
}
bool samePosition(fs::Vec3 a, fs::Vec3 b) { return a.x==b.x && a.y==b.y && a.z==b.z; }
fs::Vec3 weights(fs::Vec3 p,const std::array<fs::Vec3,3>& vertices){
    if(samePosition(p,vertices[0]))return {1,0,0};if(samePosition(p,vertices[1]))return {0,1,0};if(samePosition(p,vertices[2]))return {0,0,1};
    const auto a=subtract(vertices[1],vertices[0]),b=subtract(vertices[2],vertices[0]),c=subtract(p,vertices[0]);
    const double aa=dot(a,a),ab=dot(a,b),bb=dot(b,b),ac=dot(a,c),bc=dot(b,c),d=aa*bb-ab*ab;
    if(std::abs(d)<1e-30)return {1,0,0};const double v=(bb*ac-ab*bc)/d,w=(aa*bc-ab*ac)/d;return {1-v-w,v,w};
}

struct DisplayPoint { fs::EntityId id; fs::Vec3 position; bool locked,batchMovable; QString name; int vertexType; bool leak; };
struct DisplayEdge { fs::EntityId a, b; bool crease, mirror; fs::EntityId id; };
struct DisplayFace {fs::EntityId id;QVector<int> points;};
struct DisplayCurve {fs::EntityId id;bool flow;QColor color;QVector<fs::presentation::Cubic> cubics;fs::Vec3 source;bool hasSource=false;bool marker=false,visible=true;};
struct DisplayData {
    HullRenderMesh mesh;
    QVector<DisplayPoint> points;
    QVector<DisplayEdge> edges;
    QVector<DisplayFace> faces;
    QHash<fs::EntityId, int> pointIndices;
    mutable std::unique_ptr<HullGpu> gpu;
    mutable bool gpuAttempted=false;
    mutable QHash<quint64,QVector<fs::presentation::CombPoint>> combs;
    mutable QHash<fs::EntityId,QVector<fs::presentation::CombPoint>> curveCombs;
    mutable int curvatureBuilds=0,curvatureErrors=0;
    QVector<DisplayCurve> curves;
    bool translucent=false;
    mutable std::shared_ptr<const SurfaceAnalysis> analysis;
    mutable QVector<SurfaceShadeTriangle> shading;
};
void prepareAnalysis(const DisplayData& data,const fs::ModelSnapshot& snapshot){
    if(data.analysis)return;
    auto analysis=analyzeSurface(snapshot);QVector<SurfaceShadeTriangle> shades;shades.reserve(data.mesh.triangles.size());
    for(const auto& t:data.mesh.triangles){const auto& source=analysis->triangles.at(t.sourceTriangle);SurfaceShadeTriangle shade;
        for(int j=0;j<3;++j){auto original=t.vertices[j];if(t.reflected)original.y=-original.y;const auto w=weights(original,snapshot.triangles[t.sourceTriangle].vertices);const double f[]{w.x,w.y,w.z};auto& v=shade[j];double g[3]{},d[3]{};
            for(int k=0;k<3;++k){const auto& s=source[k];v.normal.x+=f[k]*s.normal.x;v.normal.y+=f[k]*s.normal.y;v.normal.z+=f[k]*s.normal.z;
                g[0]+=f[k]*s.gaussian.red();g[1]+=f[k]*s.gaussian.green();g[2]+=f[k]*s.gaussian.blue();d[0]+=f[k]*s.developable.red();d[1]+=f[k]*s.developable.green();d[2]+=f[k]*s.developable.blue();}
            if(t.reflected)v.normal.y=-v.normal.y;auto color=[](double* rgb){return QColor(qBound(0,qRound(rgb[0]),255),qBound(0,qRound(rgb[1]),255),qBound(0,qRound(rgb[2]),255));};v.gaussian=color(g);v.developable=color(d);
        }shades.append(shade);
    }
    data.shading=std::move(shades);data.analysis=std::move(analysis);
}

std::shared_ptr<const DisplayData> displayData(const fs::ModelSnapshot& snapshot,bool splitBody,const QSet<fs::EntityId>& hidden={}) {
    // The four cameras share the same immutable CPU geometry. Expired revisions
    // are removed here; there is no lifetime extension of old documents.
    static std::map<std::tuple<const fs::ModelSnapshot*,bool,std::vector<fs::EntityId>>, std::weak_ptr<const DisplayData>> cache;
    for (auto it=cache.begin(); it!=cache.end();) {
        if (it->second.expired()) it=cache.erase(it); else ++it;
    }
    std::vector<fs::EntityId> hiddenIds(hidden.begin(),hidden.end());std::sort(hiddenIds.begin(),hiddenIds.end());
    auto key=std::make_tuple(&snapshot,splitBody,hiddenIds);auto found=cache.find(key);
    if (found!=cache.end()) if (auto existing=found->second.lock()) return existing;
    auto result=std::make_shared<DisplayData>();
    for(bool flow:{false,true})for(const auto& curve:(flow?snapshot.flowlines:snapshot.controlCurves)){
        auto cubics=fs::presentation::splineCubics(curve.spline);
        if(splitBody)cubics=fs::presentation::splitBodyCubics(cubics,fs::presentation::splitLocation(snapshot));
        const bool hasSource=flow&&!curve.spline.points.isEmpty();auto source=hasSource?curve.spline.points[0]:fs::Vec3{};
        if(splitBody&&source.x<fs::presentation::splitLocation(snapshot))source.y=-source.y;
        result->curves.append({curve.id,flow,sourceColor(curve.color),std::move(cubics),source,hasSource});
    }
    for(const auto& marker:snapshot.markers){
        auto cubics=fs::presentation::splineCubics(marker.spline);
        if(splitBody)cubics=fs::presentation::splitBodyCubics(cubics,fs::presentation::splitLocation(snapshot));
        result->curves.append({fs::EntityId(marker.index+1),false,sourceColor(marker.color),std::move(cubics),{},false,true,marker.visible});
    }
    auto& mesh=result->mesh;
    mesh.minimum={std::numeric_limits<double>::max(),std::numeric_limits<double>::max(),
                  std::numeric_limits<double>::max()};
    mesh.maximum={-mesh.minimum.x,-mesh.minimum.y,-mesh.minimum.z};
    bool hasBounds=false;
    auto include=[&](fs::Vec3 p) {
        hasBounds=true;
        mesh.minimum={qMin(mesh.minimum.x,p.x),qMin(mesh.minimum.y,p.y),qMin(mesh.minimum.z,p.z)};
        mesh.maximum={qMax(mesh.maximum.x,p.x),qMax(mesh.maximum.y,p.y),qMax(mesh.maximum.z,p.z)};
    };
    QHash<fs::EntityId,const fs::Layer*> layers;
    for (const auto& layer:snapshot.layers) layers.insert(layer.id,&layer);
    auto appendTriangle=[&](std::array<fs::Vec3,3> vertices,QColor color,int source,bool reflected) {
        auto normal=cross(subtract(vertices[1],vertices[0]),subtract(vertices[2],vertices[0]));
        const double norm=std::sqrt(dot(normal,normal));
        if (norm>0) { normal.x/=norm;normal.y/=norm;normal.z/=norm; }
        else normal={0,0,1};
        HullRenderTriangle t{vertices,normal,color};t.sourceTriangle=source;t.reflected=reflected;
        t.useUnderwater=layers.value(snapshot.triangles[source].layer)->useInHydrostatics;
        mesh.triangles.append(t);
        for (const auto& point:vertices) include(point);
    };
    mesh.triangles.reserve(snapshot.triangles.size()*2);
    for (int source=0;source<snapshot.triangles.size();++source) {const auto& triangle=snapshot.triangles[source];
        const auto* layer=layers.value(triangle.layer,nullptr);
        if (!layer || !layer->visible) continue;
        QColor color=sourceColor(layer->color);color.setAlpha(layer->alphaBlend);result->translucent|=layer->alphaBlend!=255;
        if(splitBody){
            const auto polygons=fs::presentation::bodyPolygons({triangle.vertices[0],triangle.vertices[1],triangle.vertices[2]},fs::presentation::splitLocation(snapshot));
            for(const auto& p:polygons){bool reflected=std::any_of(p.begin(),p.end(),[&](auto v){return v.x<fs::presentation::splitLocation(snapshot);});
                if(!reflected)reflected=std::all_of(triangle.vertices.begin(),triangle.vertices.end(),[&](auto v){return v.x<=fs::presentation::splitLocation(snapshot);});
                for(int i=1;i+1<p.size();++i)appendTriangle({p[0],p[i],p[i+1]},color,source,reflected);
            }
        }else appendTriangle(triangle.vertices,color,source,false);
        if (snapshot.bothSides && layer->symmetric && (triangle.vertices[0].y!=0 || triangle.vertices[1].y!=0 ||
                                 triangle.vertices[2].y!=0)) {
            appendTriangle({mirrored(triangle.vertices[0]),mirrored(triangle.vertices[2]),
                            mirrored(triangle.vertices[1])},color,source,true);
        }
    }
    QSet<fs::EntityId> owned,visible;
    QSet<QPair<fs::EntityId,fs::EntityId>> ownedEdges,visibleEdges,symmetricEdges;
    auto edgeKey=[](fs::EntityId a,fs::EntityId b) {return qMakePair(qMin(a,b),qMax(a,b));};
    for (const auto& face:snapshot.faces) {
        const auto* layer=layers.value(face.layer,nullptr);
        for (auto point:face.points) {
            owned.insert(point);
            if (layer && layer->visible && !hidden.contains(layer->id)) visible.insert(point);
        }
        if (!face.points.isEmpty()) {
            for (qsizetype i=0;i<face.points.size();++i) {
                const auto key=edgeKey(face.points[i],face.points[(i+1)%face.points.size()]);
                ownedEdges.insert(key);
                if (layer && layer->visible && !hidden.contains(layer->id)) {
                    visibleEdges.insert(key);
                    if (layer->symmetric) symmetricEdges.insert(key);
                }
            }
        }
    }
    for (const auto& point:snapshot.points) {
        if (owned.contains(point.id) && !visible.contains(point.id)) continue;
        result->pointIndices.insert(point.id,int(result->points.size()));
        result->points.append({point.id,point.position,point.locked,point.batchMovable,point.name,point.vertexType,point.leak});
        include(splitBody?fs::presentation::bodyPoint(point.position,fs::presentation::splitLocation(snapshot)):point.position);
    }
    for (const auto& edge:snapshot.edges) {
        if (!result->pointIndices.contains(edge.a) || !result->pointIndices.contains(edge.b)) continue;
        if (ownedEdges.contains(edgeKey(edge.a,edge.b)) && !visibleEdges.contains(edgeKey(edge.a,edge.b))) continue;
        const bool mirror=snapshot.bothSides&&symmetricEdges.contains(edgeKey(edge.a,edge.b));
        result->edges.append({edge.a,edge.b,edge.crease,mirror,edge.id});
        if (mirror) {
            include(mirrored(result->points[result->pointIndices.value(edge.a)].position));
            include(mirrored(result->points[result->pointIndices.value(edge.b)].position));
        }
    }
    for(const auto& face:snapshot.faces) {
        const auto* layer=layers.value(face.layer,nullptr);if(!layer || !layer->visible || hidden.contains(layer->id))continue;
        DisplayFace item{face.id,{}};
        for(auto id:face.points)item.points.append(result->pointIndices.value(id));
        result->faces.append(std::move(item));
    }
    if (!hasBounds) { mesh.minimum={-0.5,-0.5,-0.5};mesh.maximum={0.5,0.5,0.5}; }
    mesh.center={(mesh.minimum.x+mesh.maximum.x)/2,(mesh.minimum.y+mesh.maximum.y)/2,
                 (mesh.minimum.z+mesh.maximum.z)/2};
    mesh.extent=std::max({mesh.maximum.x-mesh.minimum.x,mesh.maximum.y-mesh.minimum.y,
                         mesh.maximum.z-mesh.minimum.z});
    if (mesh.extent<=0) mesh.extent=std::max({snapshot.length,snapshot.beam,snapshot.draft,1.0});
    mesh.shadeUnderwater=snapshot.shadeUnderwater;
    mesh.waterlineZ=snapshot.waterlineZ;
    mesh.underwaterColor=sourceColor(snapshot.underwaterColor);
    mesh.underwaterAlpha=snapshot.underwaterColorAlpha;
    cache[key]=result;
    return result;
}
}

struct HullView::State {
    std::shared_ptr<const fs::ModelSnapshot> snapshot;
    std::shared_ptr<const DisplayData> display;
    HullProjection projection=HullProjection::Profile;
    fs::EntityId selected=0;
    QSet<fs::EntityId> selection;
    HullEntity kind=HullEntity::Point;
    QSet<fs::EntityId> entities;
    bool studio=false,fillVisible=true,controlNetVisible=true;
    QSet<fs::EntityId> hiddenControlLayers;
    QJsonObject preferenceColors;
    QColor preferred(const QString& key,QColor fallback)const{
        return preferenceColors.contains(key)?QColor::fromRgb(preferenceColors[key].toInt()):fallback;
    }
    bool gridVisible=true,hydroVisible=true,animations=true;
    int coordinateAxis=0;
    mutable fs::presentation::Entrance entrance;
    mutable fs::presentation::RevealGraph graph;
    mutable fs::presentation::LabelLayout labels;
    QElapsedTimer clock;QTimer timer;
    mutable QJsonObject evidence;
    mutable qint64 frameTime=0;
    std::array<bool,4> sectionVisible{true,true,true,true};
    int highlightedKind=0;QSet<int> highlightedSections;
    bool editingEnabled=true;
    bool flowPlacement=false;
    bool pointPlacement=false;
    struct CurvePath {QPainterPath path,hit;bool hitReady=false;};
    mutable QVector<CurvePath> curvePaths;
    mutable quint64 curvePathRevision=std::numeric_limits<quint64>::max();
    mutable int curvePathBuilds=0;
    double scale=1;
    fs::Vec3 cameraCenter;
    double cameraExtent=1;
    int focalLength=50;
    HullLight light;
    HullShade shadeMode=HullShade::Flat;
    quint64 lightRevision=0;
    mutable quint64 fillLightRevision=std::numeric_limits<quint64>::max(),fillBuilds=0;
    mutable bool fillSoftware=false,usedGpu=false;
    double cameraDistanceFactor()const{return 3.*focalLength/50.;}
    quint64 cameraRevision=0;
    mutable quint64 fillRevision=std::numeric_limits<quint64>::max();
    mutable QSize fillSize;
    mutable qreal fillDpr=0;
    mutable QImage fillImage;
    QPointF offset;
    bool fitted=true;
    double yaw=-30*Pi/180,pitch=25*Pi/180;
    enum class Gesture { None, Pan, Rotate, Move, Box, BlockedMove, Background } gesture=Gesture::None;
    BackgroundMode backgroundMode=BackgroundMode::None;
    fs::BackgroundImage backgroundStart,backgroundPreview;
    bool backgroundPreviewing=false;int backgroundSide=0;
    QPointF press,last;
    QVector<fs::PointMove> dragStarts;
    fs::Vec3 dragDelta;
    double dragBodySign=1;
    QSet<fs::EntityId> boxBase,boxPreview;
    QString blockedReason;
    bool moved=false,lastRightDragged=false;
    bool splitBody()const{return snapshot&&!snapshot->bothSides&&projection==HullProjection::Body;}
    fs::Vec3 controlPosition(fs::Vec3 p)const{return splitBody()?fs::presentation::bodyPoint(p,fs::presentation::splitLocation(*snapshot)):p;}
    QVector<fs::presentation::Segment> controlSegments(fs::Vec3 a,fs::Vec3 b)const{
        return splitBody()?fs::presentation::bodySegments(a,b,fs::presentation::splitLocation(*snapshot)):QVector<fs::presentation::Segment>{{a,b}};
    }
    QVector<QVector<fs::Vec3>> controlPolygons(const QVector<fs::Vec3>& p)const{
        return splitBody()?fs::presentation::bodyPolygons(p,fs::presentation::splitLocation(*snapshot)):QVector<QVector<fs::Vec3>>{p};
    }
    bool curveVisible(const DisplayCurve& c)const{return snapshot&&c.visible&&(c.marker?snapshot->showMarkers:c.flow?snapshot->showFlowlines&&!fillVisible:snapshot->showControlCurves);}
    void prepareCurvePaths()const{
        if(curvePathRevision==cameraRevision)return;
        curvePaths.clear();
        if(display)for(const auto& c:display->curves){
            auto path=fs::presentation::projectCubics(c.cubics,[this](fs::Vec3 p){return projectPoint(p).pixel;},projection==HullProjection::Perspective);
            if(snapshot->bothSides&&projection!=HullProjection::Profile)path.addPath(fs::presentation::projectCubics(c.cubics,[this](fs::Vec3 p){p.y=-p.y;return projectPoint(p).pixel;},projection==HullProjection::Perspective));
            curvePaths.append({path,{}});
        }
        curvePathRevision=cameraRevision;++curvePathBuilds;
    }

    void basis(fs::Vec3& right,fs::Vec3& up,fs::Vec3& depth) const {
        switch (projection) {
        case HullProjection::Profile: right={1,0,0};up={0,0,1};depth={0,1,0};break;
        case HullProjection::Plan: right={1,0,0};up={0,1,0};depth={0,0,1};break;
        case HullProjection::Body: right={0,1,0};up={0,0,1};depth={1,0,0};break;
        case HullProjection::Perspective:
            right={std::cos(yaw),std::sin(yaw),0};
            up={-std::sin(yaw)*std::sin(pitch),std::cos(yaw)*std::sin(pitch),std::cos(pitch)};
            depth={std::sin(yaw)*std::cos(pitch),-std::cos(yaw)*std::cos(pitch),std::sin(pitch)};
            break;
        }
    }
    struct Projected { QPointF pixel;double depth,reciprocalW; };
    Projected projectPoint(fs::Vec3 point) const {
        fs::Vec3 right,up,depth;
        basis(right,up,depth);
        const fs::Vec3 relative=subtract(point,cameraCenter);
        const double z=dot(relative,depth)/cameraExtent;
        const double w=projection==HullProjection::Perspective?1-z/cameraDistanceFactor():1;
        const double inverse=1/qMax(w,1e-6);
        return {{offset.x()+scale*dot(relative,right)*inverse,
                 offset.y()-scale*dot(relative,up)*inverse},-z*inverse/2,inverse};
    }
    QMatrix4x4 camera(QSize size) const {
        fs::Vec3 right,up,depth;
        basis(right,up,depth);
        const double extent=display?display->mesh.extent:1;
        const auto origin=display?subtract(display->mesh.center,cameraCenter):fs::Vec3{};
        const double sx=2*scale*extent/qMax(1,size.width());
        const double sy=2*scale*extent/qMax(1,size.height());
        const double tx=2*offset.x()/qMax(1,size.width())-1;
        const double ty=1-2*offset.y()/qMax(1,size.height());
        const double perspective=projection==HullProjection::Perspective?-extent/(cameraDistanceFactor()*cameraExtent):0;
        const fs::Vec3 w={perspective*depth.x,perspective*depth.y,perspective*depth.z};
        const double w0=projection==HullProjection::Perspective?1-dot(origin,depth)/(cameraDistanceFactor()*cameraExtent):1;
        const double x0=2*scale*dot(origin,right)/qMax(1,size.width());
        const double y0=2*scale*dot(origin,up)/qMax(1,size.height());
        const double depthExtent=qMax(cameraExtent,extent+std::abs(dot(origin,depth)));
        const double depthScale=extent/(2*depthExtent);
        QMatrix4x4 matrix;
        matrix.setRow(0,QVector4D(sx*right.x+tx*w.x,sx*right.y+tx*w.y,sx*right.z+tx*w.z,x0+tx*w0));
        matrix.setRow(1,QVector4D(sy*up.x+ty*w.x,sy*up.y+ty*w.y,sy*up.z+ty*w.z,y0+ty*w0));
        matrix.setRow(2,QVector4D(-depth.x*depthScale,-depth.y*depthScale,-depth.z*depthScale,
                                  -dot(origin,depth)/(2*depthExtent)));
        matrix.setRow(3,QVector4D(w.x,w.y,w.z,w0));
        return matrix;
    }
    HullLighting lighting()const{
        HullLighting result;result.enabled=light.enabled;result.ambient=light.ambient/255.;result.intensity=light.intensity/255.;
        if(!display)return result;
        const auto direction=light.direction;const double norm=std::hypot(direction.x,direction.y,direction.z);
        // No direction means ambient-only illumination, not a NaN or an
        // unintended point light inside the hull.
        if(norm<1e-12){result.intensity=0;return result;}
        fs::Vec3 right,up,depth;basis(right,up,depth);const auto& mesh=display->mesh;
        const double distance=7.5*std::hypot(mesh.maximum.x-mesh.minimum.x,mesh.maximum.y-mesh.minimum.y,mesh.maximum.z-mesh.minimum.z)/mesh.extent;
        const auto center=subtract(cameraCenter,mesh.center);
        result.position={center.x/mesh.extent+distance*(direction.x*depth.x+direction.y*right.x+direction.z*up.x)/norm,
            center.y/mesh.extent+distance*(direction.x*depth.y+direction.y*right.y+direction.z*up.y)/norm,
            center.z/mesh.extent+distance*(direction.x*depth.z+direction.y*right.z+direction.z*up.z)/norm};
        return result;
    }
    HullStyle style()const{
        HullStyle result;result.mode=shadeMode;result.perspective=projection==HullProjection::Perspective;
        fs::Vec3 right,up,depth;basis(right,up,depth);result.direction=depth;result.zebra=preferred("zebracolor",Qt::black);
        if(display){const auto center=subtract(cameraCenter,display->mesh.center);const double extent=display->mesh.extent,distance=cameraDistanceFactor()*cameraExtent;
            result.eye={(center.x+distance*depth.x)/extent,(center.y+distance*depth.y)/extent,(center.z+distance*depth.z)/extent};}
        return result;
    }
    fs::Vec3 movedPosition(fs::Vec3 point) const {
        // Assign only the displayed axes, preserving each point's hidden value.
        switch(projection) {
        case HullProjection::Profile:point.x+=dragDelta.x;point.z+=dragDelta.z;break;
        case HullProjection::Plan:point.x+=dragDelta.x;point.y+=dragDelta.y;break;
        case HullProjection::Body:point.y+=dragDelta.y;point.z+=dragDelta.z;break;
        case HullProjection::Perspective:break;
        }
        return point;
    }
    fs::Vec3 displayedPosition(const DisplayPoint& point) const {
        return gesture==Gesture::Move && moved && selection.contains(point.id)?movedPosition(point.position):point.position;
    }
    void pruneSelection() {
        for (auto it=selection.begin();it!=selection.end();) {
            if (!display || !display->pointIndices.contains(*it)) it=selection.erase(it); else ++it;
        }
        if (!selection.contains(selected))
            selected=selection.isEmpty()?0:*std::min_element(selection.cbegin(),selection.cend());
    }
    fs::EntityId hit(QPointF at) const {
        if (!display) return 0;
        double distance=8*8;
        double nearestDepth=std::numeric_limits<double>::max();
        fs::EntityId best=0;
        for (const auto& point:display->points) {
            const auto projected=projectPoint(controlPosition(point.position));
            const auto difference=projected.pixel-at;
            const double squared=QPointF::dotProduct(difference,difference);
            if (squared<distance-0.01 || (std::abs(squared-distance)<=0.01 && projected.depth<nearestDepth)) {
                distance=squared;nearestDepth=projected.depth;best=point.id;
            }
        }
        return best;
    }

    QImage softwareImage(QSize logical,qreal dpr) const {
        const QSize physical(qMax(1,qRound(logical.width()*dpr)),qMax(1,qRound(logical.height()*dpr)));
        QImage image(physical,QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        if (!display || display->mesh.triangles.isEmpty()) { image.setDevicePixelRatio(dpr);return image; }
        // A depth buffer avoids the painter's ordering errors on overlapping hull
        // surfaces. This fallback uses the same original mesh and camera as GL.
        std::vector<float> depths(size_t(physical.width())*size_t(physical.height()),
                                  std::numeric_limits<float>::infinity());
        const auto& mesh=display->mesh;
        const auto illumination=lighting();const auto appearance=style();
        struct Fragment {size_t pixel;float depth;QRgb color;int alpha;};std::vector<Fragment> fragments;
        for(int pass=0;pass<(display->translucent?2:1);++pass)for (int index=0;index<mesh.triangles.size();++index) {const auto& triangle=mesh.triangles[index];
            const int alpha=triangle.color.alpha();if(alpha==0 || (alpha<255)!=(pass==1))continue;
            const auto p0=projectPoint(triangle.vertices[0]);
            const auto p1=projectPoint(triangle.vertices[1]);
            const auto p2=projectPoint(triangle.vertices[2]);
            const QPointF a=p0.pixel*dpr,b=p1.pixel*dpr,c=p2.pixel*dpr;
            if (std::max({a.x(),b.x(),c.x()})<0 || std::min({a.x(),b.x(),c.x()})>=physical.width() ||
                std::max({a.y(),b.y(),c.y()})<0 || std::min({a.y(),b.y(),c.y()})>=physical.height()) continue;
            const double denominator=(b.y()-c.y())*(a.x()-c.x())+(c.x()-b.x())*(a.y()-c.y());
            if (std::abs(denominator)<1e-15) continue;
            const int minX=int(qBound(0.0,std::floor(std::min({a.x(),b.x(),c.x()})),double(physical.width()-1)));
            const int maxX=int(qBound(0.0,std::ceil(std::max({a.x(),b.x(),c.x()})),double(physical.width()-1)));
            const int minY=int(qBound(0.0,std::floor(std::min({a.y(),b.y(),c.y()})),double(physical.height()-1)));
            const int maxY=int(qBound(0.0,std::ceil(std::max({a.y(),b.y(),c.y()})),double(physical.height()-1)));
            fs::Vec3 center{};for(const auto& p:triangle.vertices){center.x+=(p.x-mesh.center.x)/(3*mesh.extent);center.y+=(p.y-mesh.center.y)/(3*mesh.extent);center.z+=(p.z-mesh.center.z)/(3*mesh.extent);}
            const double light=shadeMode==HullShade::Gaussian?1:illumination.factor(triangle.normal,center);
            std::array<QColor,3> colors;double stripe[3]{};
            if(shadeMode!=HullShade::Flat)for(int k=0;k<3;++k){const auto& v=display->shading[index][k];
                colors[k]=shadeMode==HullShade::Gaussian?v.gaussian:v.developable;
                if(shadeMode==HullShade::Zebra){const auto p=triangle.vertices[k];fs::Vec3 direction=appearance.direction;
                    if(appearance.perspective)direction={appearance.eye.x-(p.x-mesh.center.x)/mesh.extent,appearance.eye.y-(p.y-mesh.center.y)/mesh.extent,appearance.eye.z-(p.z-mesh.center.z)/mesh.extent};
                    const double length=std::hypot(direction.x,direction.y,direction.z);stripe[k]=length>1e-12?std::clamp(std::abs(dot(direction,v.normal)/length),0.,1.):0;}
            }
            auto lit=[light](QColor color) { return qRgb(qRound(color.red()*light),
                qRound(color.green()*light),qRound(color.blue()*light)); };
            const QRgb color=lit(triangle.color),underwater=lit(triangle.useUnderwater?
                underwaterTint(triangle.color,mesh.underwaterColor,mesh.underwaterAlpha):triangle.color);
            const double z0=triangle.vertices[0].z*p0.reciprocalW;
            const double z1=triangle.vertices[1].z*p1.reciprocalW;
            const double z2=triangle.vertices[2].z*p2.reciprocalW;
            for (int y=minY;y<=maxY;++y) {
                auto* scan=reinterpret_cast<QRgb*>(image.scanLine(y));
                auto* depthRow=depths.data()+size_t(y)*size_t(physical.width());
                for (int x=minX;x<=maxX;++x) {
                    const double u=((b.y()-c.y())*(x+0.5-c.x())+(c.x()-b.x())*(y+0.5-c.y()))/denominator;
                    const double v=((c.y()-a.y())*(x+0.5-c.x())+(a.x()-c.x())*(y+0.5-c.y()))/denominator;
                    const double w=1-u-v;
                    if (u<0 || v<0 || w<0) continue;
                    if(alpha<255){
                        // Half-open shared edges prevent applying opacity twice
                        // along the diagonal of two adjacent triangles.
                        auto topLeft=[&](QPointF p,QPointF q){if(denominator<0)std::swap(p,q);auto d=q-p;return d.y()<0||(d.y()==0&&d.x()>0);};
                        if((u<=1e-12&&!topLeft(b,c))||(v<=1e-12&&!topLeft(c,a))||(w<=1e-12&&!topLeft(a,b)))continue;
                    }
                    const float z=float(u*p0.depth+v*p1.depth+w*p2.depth);
                    if (z>depthRow[x]) continue;
                    if(alpha==255)depthRow[x]=z;
                    const double height=(u*z0+v*z1+w*z2)/(u*p0.reciprocalW+v*p1.reciprocalW+w*p2.reciprocalW);
                    QRgb rgb=mesh.shadeUnderwater && height<mesh.waterlineZ?underwater:color;
                    if(shadeMode!=HullShade::Flat){
                        const double reciprocal=u*p0.reciprocalW+v*p1.reciprocalW+w*p2.reciprocalW;
                        const double bary[]{u*p0.reciprocalW/reciprocal,v*p1.reciprocalW/reciprocal,w*p2.reciprocalW/reciprocal};
                        if(shadeMode==HullShade::Zebra){const double value=bary[0]*stripe[0]+bary[1]*stripe[1]+bary[2]*stripe[2];rgb=(int(std::floor(value/.02))%2)?lit(appearance.zebra):color;}
                        else{double red=0,green=0,blue=0;for(int k=0;k<3;++k){red+=bary[k]*colors[k].red();green+=bary[k]*colors[k].green();blue+=bary[k]*colors[k].blue();}
                            rgb=qRgb(qBound(0,qRound(red*light),255),qBound(0,qRound(green*light),255),qBound(0,qRound(blue*light),255));}
                    }
                    if(alpha==255)scan[x]=rgb;else fragments.push_back({size_t(y)*size_t(physical.width())+size_t(x),z,rgb,alpha});
                }
            }
        }
        // Sort actual fragments, not triangle centers: intersecting translucent
        // surfaces have a different front/back order at different pixels.
        std::stable_sort(fragments.begin(),fragments.end(),[](const auto& a,const auto& b){return a.pixel!=b.pixel?a.pixel<b.pixel:a.depth>b.depth;});
        for(const auto& f:fragments){auto& dst=reinterpret_cast<QRgb*>(image.bits())[f.pixel];const int a=f.alpha,rest=255-a;
            auto channel=[&](int src,int dest){return (src*a+dest*rest+127)/255;};
            dst=qRgba(channel(qRed(f.color),qRed(dst)),channel(qGreen(f.color),qGreen(dst)),channel(qBlue(f.color),qBlue(dst)),a+(qAlpha(dst)*rest+127)/255);
        }
        image.setDevicePixelRatio(dpr);
        return image;
    }
};

HullView::HullView(QWidget* parent) : QWidget(parent),state_(std::make_unique<State>()) {
    setAttribute(Qt::WA_OpaquePaintEvent);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setMinimumSize(100,80);
    setAccessibleName(projectionLabel());
    state_->clock.start();state_->timer.setInterval(16);state_->timer.setTimerType(Qt::PreciseTimer);
    connect(&state_->timer,&QTimer::timeout,this,[this]{if(isVisible())update();else state_->timer.stop();});
}
HullView::~HullView() = default;

void HullView::setSnapshot(std::shared_ptr<const fs::ModelSnapshot> snapshot) {
    if (snapshot==state_->snapshot) return;
    const bool sameDocument=snapshot && state_->snapshot && snapshot->documentId==state_->snapshot->documentId;
    const bool sameUnits=snapshot && state_->snapshot && snapshot->units==state_->snapshot->units;
    cancelGesture();
    state_->snapshot=std::move(snapshot);
    if(!sameDocument){state_->flowPlacement=false;state_->pointPlacement=false;state_->hiddenControlLayers.clear();}
    if(!sameDocument||!background()||!background()->visible)state_->backgroundMode=BackgroundMode::None;
    state_->display=state_->snapshot?displayData(*state_->snapshot,state_->splitBody(),state_->hiddenControlLayers):nullptr;
    ++state_->cameraRevision;
    if (!sameDocument) {
        state_->selected=0;state_->selection.clear();state_->entities.clear();
        state_->yaw=-30*Pi/180;state_->pitch=25*Pi/180;
        fit();
    } else {
        state_->pruneSelection();
        setSelectedEntities(state_->entities);
        if (!sameUnits) fit();
    }
    state_->labels.clear();
    if(!sameDocument&&state_->snapshot){
        state_->gridVisible=state_->snapshot->overlay.grid;state_->hydroVisible=state_->snapshot->overlay.hydro;
        if(state_->studio&&state_->snapshot->overlay.present)restartPresentation();
    }else finishPresentation();
    update();
}
std::shared_ptr<const fs::ModelSnapshot> HullView::snapshot() const { return state_->snapshot; }
void HullView::setProjection(HullProjection projection) {
    if (state_->projection==projection) return;
    cancelGesture();
    state_->projection=projection;
    state_->light.enabled=projection==HullProjection::Perspective;++state_->lightRevision;
    state_->display=state_->snapshot?displayData(*state_->snapshot,state_->splitBody(),state_->hiddenControlLayers):nullptr;
    state_->backgroundMode=BackgroundMode::None;
    setAccessibleName(projectionLabel());
    fit();
}
HullProjection HullView::projection() const { return state_->projection; }
void HullView::setSelectedPoint(fs::EntityId id) {
    setSelectedPoints(id?QSet<fs::EntityId>{id}:QSet<fs::EntityId>{});
}
fs::EntityId HullView::selectedPoint() const { return state_->selected; }
void HullView::setSelectedPoints(QSet<fs::EntityId> ids) {
    if (state_->selection==ids) return;
    cancelGesture();
    state_->selection=std::move(ids);
    state_->pruneSelection();
    update();
}
QSet<fs::EntityId> HullView::selectedPoints() const { return state_->selection; }
void HullView::setSelectionKind(HullEntity kind) {
    cancelGesture();state_->kind=kind;state_->entities.clear();update();
}
HullEntity HullView::selectionKind() const{return state_->kind;}
QSet<fs::EntityId> HullView::selectedEntities() const{return state_->entities;}
void HullView::setSelectedEntities(QSet<fs::EntityId> ids) {
    QSet<fs::EntityId> valid;
    if(state_->snapshot) {
        if(state_->kind==HullEntity::Edge && state_->display)
            for(const auto& edge:state_->display->edges)if(ids.contains(edge.id))valid.insert(edge.id);
        if(state_->kind==HullEntity::Face && state_->display)
            for(const auto& face:state_->display->faces)if(ids.contains(face.id))valid.insert(face.id);
        if(state_->display&&(state_->kind==HullEntity::Curve||state_->kind==HullEntity::Flowline))
            for(const auto& curve:state_->display->curves)if(!curve.marker&&curve.flow==(state_->kind==HullEntity::Flowline)&&state_->curveVisible(curve)&&ids.contains(curve.id))valid.insert(curve.id);
    }
    state_->entities=std::move(valid);update();
}
void HullView::setStudioAppearance(bool enabled){state_->studio=enabled;update();}
void HullView::setGraphicPreferences(const QJsonObject& colors){state_->preferenceColors=colors;++state_->lightRevision;update();}
void HullView::setFillVisible(bool enabled){state_->fillVisible=enabled;if(enabled){state_->shadeMode=HullShade::Flat;++state_->lightRevision;}update();}
bool HullView::setShadeMode(HullShade mode){
    if(int(mode)<0||int(mode)>3)return false;
    if(mode!=HullShade::Flat){if(!state_->snapshot||!state_->snapshot->surfaceTopology||!state_->display)return false;
        try{prepareAnalysis(*state_->display,*state_->snapshot);}catch(const std::exception& e){if(onEditRejected)onEditRejected(QString::fromUtf8(e.what()));return false;}}
    if(state_->fillVisible&&state_->shadeMode==mode)return true;state_->fillVisible=true;state_->shadeMode=mode;++state_->lightRevision;finishPresentation();update();return true;
}
HullShade HullView::shadeMode()const{return state_->shadeMode;}
bool HullView::setFocalLength(int millimeters){
    if(millimeters!=28&&millimeters!=50&&millimeters!=90&&millimeters!=130&&millimeters!=200)return false;
    if(state_->projection!=HullProjection::Perspective)return false;
    if(state_->focalLength==millimeters)return true;
    state_->focalLength=millimeters;fit();if(onViewSettingsChanged)onViewSettingsChanged();return true;
}
int HullView::focalLength()const{return state_->focalLength;}
HullLight HullView::light()const{return state_->light;}
bool HullView::setLight(const HullLight& light){
    if(!light.valid())return false;if(light==state_->light)return true;
    state_->light=light;++state_->lightRevision;update();if(onViewSettingsChanged)onViewSettingsChanged();return true;
}
QJsonObject HullView::viewSettingsEvidence()const{
    const auto& light=state_->light;
    return {{"focal_mm",state_->focalLength},{"camera_distance_extents",state_->cameraDistanceFactor()},
        {"light_direction",QJsonArray{light.direction.x,light.direction.y,light.direction.z}},{"ambient",light.ambient},{"intensity",light.intensity},{"lighting_enabled",light.enabled},
        {"fill_renders",double(state_->fillBuilds)},{"gpu",state_->usedGpu},{"camera_revision",double(state_->cameraRevision)},{"curve_path_builds",state_->curvePathBuilds},
        {"shade_mode",int(state_->shadeMode)},{"analysis_built",state_->display&&bool(state_->display->analysis)}};
}
void HullView::setControlNetVisible(bool enabled){state_->controlNetVisible=enabled;update();}
void HullView::setHiddenControlLayers(const QSet<fs::EntityId>& ids){
    if(state_->hiddenControlLayers==ids)return;state_->hiddenControlLayers=ids;
    state_->display=state_->snapshot?displayData(*state_->snapshot,state_->splitBody(),ids):nullptr;
    state_->pruneSelection();++state_->cameraRevision;update();
}
void HullView::setSectionVisible(int kind,bool enabled){if(kind>=1&&kind<=4){state_->sectionVisible[kind-1]=enabled;update();}}
void HullView::highlightSections(int kind,QSet<int> indices){state_->highlightedKind=kind;state_->highlightedSections=std::move(indices);update();}
void HullView::setGridVisible(bool enabled){finishPresentation();state_->gridVisible=enabled;state_->labels.clear();update();}
void HullView::setHydroVisible(bool enabled){finishPresentation();state_->hydroVisible=enabled;state_->labels.clear();update();}
bool HullView::gridVisible()const{return state_->gridVisible;}
bool HullView::hydroVisible()const{return state_->hydroVisible;}
void HullView::setCoordinateAxis(int axis){if(axis<0||axis>3)return;finishPresentation();state_->coordinateAxis=axis;state_->labels.clear();update();}
int HullView::coordinateAxis()const{return state_->coordinateAxis;}
void HullView::setAnimationsEnabled(bool enabled){state_->animations=enabled;if(!enabled)finishPresentation();}
void HullView::restartPresentation(){if(!state_->animations)return;state_->labels.clear();state_->entrance.restart();state_->timer.start();update();}
void HullView::finishPresentation(){state_->entrance.finish();state_->timer.stop();update();}
void HullView::freezePresentation(bool frozen){if(frozen){state_->entrance.pause(state_->clock.elapsed());state_->timer.stop();}else{state_->entrance.resume(state_->clock.elapsed());if(state_->entrance.active())state_->timer.start();update();}}
bool HullView::presentationActive()const{return state_->entrance.active();}
QJsonObject HullView::overlayEvidence()const{return state_->evidence;}
QImage HullView::presentationImageAt(qint64 milliseconds){
    state_->frameTime=milliseconds;state_->entrance.beginFrame(milliseconds,width());
    QImage result(size(),QImage::Format_ARGB32_Premultiplied);result.fill(Qt::transparent);QPainter painter(&result);draw(painter,1,false);state_->entrance.endFrame();return result;
}
void HullView::hideEvent(QHideEvent* event){freezePresentation(true);QWidget::hideEvent(event);}
void HullView::showEvent(QShowEvent* event){freezePresentation(false);QWidget::showEvent(event);}
void HullView::contextMenuEvent(QContextMenuEvent* event){
    if(event->reason()==QContextMenuEvent::Mouse&&state_->lastRightDragged){event->accept();return;}
    finishPresentation();
    auto* menu=new QMenu(this);menu->setAttribute(Qt::WA_DeleteOnClose);
    menu->setAttribute(Qt::WA_DontShowOnScreen,testAttribute(Qt::WA_DontShowOnScreen));
    auto add=[&](const QString& text,const QString& operation,auto callback){auto* action=menu->addAction(text);action->setProperty("view_operation",operation);action->setEnabled(bool(state_->snapshot));connect(action,&QAction::triggered,this,callback);return action;};
    add("缩放至适合窗口","fit",[this]{fit();});
    add("放大","zoom_in",[this]{zoomAt(rect().center(),1.2);});
    add("缩小","zoom_out",[this]{zoomAt(rect().center(),1/1.2);});
    auto* modes=new QActionGroup(menu);
    for(bool fill:{false,true}){auto* a=add(fill?"填色":"线框",fill?"fill":"wireframe",[this,fill]{setFillVisible(fill);});a->setCheckable(true);a->setChecked(fill?(state_->fillVisible&&state_->shadeMode==HullShade::Flat):!state_->fillVisible);modes->addAction(a);}
    const QStringList shadeNames{"高斯曲率","可展性检查","斑马纹"};
    for(int i=1;i<=3;++i){auto* a=add(shadeNames[i-1],"shade_"+QString::number(i),[this,i]{setShadeMode(HullShade(i));});a->setProperty("shade_mode",i);a->setEnabled(state_->snapshot&&bool(state_->snapshot->surfaceTopology));a->setCheckable(true);a->setChecked(state_->fillVisible&&int(state_->shadeMode)==i);modes->addAction(a);}
    menu->addSeparator();
    auto* cameras=menu->addMenu("镜头");cameras->setEnabled(state_->snapshot&&state_->projection==HullProjection::Perspective);auto* lenses=new QActionGroup(cameras);
    const QStringList lensNames{"广角 28 mm","标准 50 mm","短焦远摄 90 mm","中焦远摄 130 mm","远摄 200 mm"};int lensIndex=0;
    for(int focal:{28,50,90,130,200}){auto* a=cameras->addAction(lensNames[lensIndex++]);a->setProperty("focal_mm",focal);a->setCheckable(true);a->setChecked(state_->focalLength==focal);lenses->addAction(a);connect(a,&QAction::triggered,this,[this,focal]{setFocalLength(focal);});}
    auto* light=add("启用光照","lighting_enabled",[this]{auto light=state_->light;light.enabled=!light.enabled;setLight(light);});light->setCheckable(true);light->setChecked(state_->light.enabled);
    add("光源…","light_settings",[this]{if(onViewAction)onViewAction("light_settings");});
    menu->addSeparator();
    auto* axes=menu->addMenu("控制点坐标");auto* group=new QActionGroup(axes);
    const QStringList names{"不显示坐标","X 坐标","Y 坐标","Z 坐标"};
    for(int i=0;i<4;++i){auto* a=axes->addAction(names[i]);a->setCheckable(true);a->setChecked(state_->coordinateAxis==i);group->addAction(a);a->setProperty("coordinate_axis",i);
        connect(a,&QAction::triggered,this,[this,i]{setCoordinateAxis(i);});}
    for(bool hydro:{false,true}){auto* a=menu->addAction(hydro?"静水力数据":"网格");a->setCheckable(true);a->setChecked(hydro?hydroVisible():gridVisible());a->setProperty("overlay_kind",hydro?"hydro":"grid");
        connect(a,&QAction::toggled,this,[this,hydro](bool show){if(hydro)setHydroVisible(show);else setGridVisible(show);});}
    auto* backgroundMenu=menu->addMenu("背景影像");backgroundMenu->setEnabled(state_->snapshot&&backgroundView()<3);
    const auto* b=background();
    for(const auto& item:QList<QPair<QString,QString>>{{"import","导入…"},{"origin","原点"},{"scale","设置比例"},{"frame","调整图片边框"},{"transparent_color","透明色"},{"transparent","启用透明色"},{"alpha","混合…"},{"tolerance","容差…"},{"visible","显示"},{"export","保存…"},{"remove","清除"}}){
        auto* a=backgroundMenu->addAction(item.second);a->setProperty("background_operation",item.first);
        bool manipulate=item.first=="origin"||item.first=="scale"||item.first=="frame"||item.first=="transparent_color";
        a->setEnabled(state_->editingEnabled&&(item.first=="import"||(b&&(!manipulate||b->visible))));
        if(item.first=="visible"||item.first=="transparent"){a->setCheckable(true);a->setChecked(b&&(item.first=="visible"?b->visible:b->transparent));}
        connect(a,&QAction::triggered,this,[this,operation=item.first]{
            if(operation=="origin")setBackgroundMode(BackgroundMode::Origin);
            else if(operation=="frame")setBackgroundMode(BackgroundMode::Frame);
            else if(operation=="scale")setBackgroundMode(BackgroundMode::Scale);
            else if(operation=="transparent_color")setBackgroundMode(BackgroundMode::TransparentColor);
            else if(onBackgroundAction)onBackgroundAction(operation,{});
        });
    }
    menu->addSeparator();
    add("保存图片…","save_image",[this]{if(onViewAction)onViewAction("save_image");});
    add("打印预览…","print",[this]{if(onViewAction)onViewAction("print");});
    menu->popup(event->globalPos());event->accept();
}
fs::EntityId HullView::entityAt(QPointF pixel) const {
    if(!state_->display || !state_->snapshot)return 0;
    if(state_->kind==HullEntity::Point)return state_->hit(pixel);
    if(state_->kind==HullEntity::Curve||state_->kind==HullEntity::Flowline){
        state_->prepareCurvePaths();
        for(int i=0;i<state_->display->curves.size();++i){const auto& c=state_->display->curves[i];
            if(c.marker||c.flow!=(state_->kind==HullEntity::Flowline)||!state_->curveVisible(c))continue;
            auto& cached=state_->curvePaths[i];if(!cached.hitReady){QPainterPathStroker stroke;stroke.setWidth(14);cached.hit=stroke.createStroke(cached.path);cached.hitReady=true;}
            if(cached.hit.contains(pixel))return c.id;}
        return 0;
    }
    fs::EntityId result=0;
    if(state_->kind==HullEntity::Edge) {
        double nearest=7*7,nearestDepth=std::numeric_limits<double>::infinity();
        for(const auto& edge:state_->display->edges) {
            for(const auto& span:state_->controlSegments(state_->display->points[state_->display->pointIndices.value(edge.a)].position,state_->display->points[state_->display->pointIndices.value(edge.b)].position)){
            const auto pa=state_->projectPoint(span[0]);const auto pb=state_->projectPoint(span[1]);
            const auto a=pa.pixel,b=pb.pixel;
            auto delta=b-a;double length=QPointF::dotProduct(delta,delta);
            double t=length>0?std::clamp(QPointF::dotProduct(pixel-a,delta)/length,0.0,1.0):0;
            auto difference=pixel-(a+t*delta);double distance=QPointF::dotProduct(difference,difference);
            const auto depth=pa.depth+t*(pb.depth-pa.depth);
            if(distance<nearest-1e-8 || (result && std::abs(distance-nearest)<=1e-8 && depth<nearestDepth)) {
                nearest=distance;nearestDepth=depth;result=edge.id;
            }
            }
        }
    } else {
        double best=-std::numeric_limits<double>::infinity();
        for(const auto& face:state_->display->faces) {
            QVector<fs::Vec3> points;for(auto index:face.points)points.append(state_->display->points[index].position);
            for(const auto& points:state_->controlPolygons(points)){
            QPolygonF polygon;double depth=0;
            for(const auto& p:points) {
                const auto projected=state_->projectPoint(p);
                polygon.append(projected.pixel);depth-=projected.depth;
            }
            if(polygon.containsPoint(pixel,Qt::OddEvenFill) && depth/polygon.size()>best){
                best=depth/polygon.size();result=face.id;
            }
            }
        }
    }
    return result;
}
void HullView::notifySelectionChanged() {
    if (onSelectionChanged) onSelectionChanged(state_->selection);
    else if (onPointSelected) onPointSelected(state_->selected);
}
void HullView::setEditingEnabled(bool enabled) {
    if(enabled&&!state_->editingEnabled){state_->backgroundPreviewing=false;state_->backgroundStart={};state_->backgroundPreview={};}
    state_->editingEnabled=enabled;
    if (!enabled && (state_->gesture==State::Gesture::Move || state_->gesture==State::Gesture::BlockedMove || state_->gesture==State::Gesture::Background)) cancelGesture();
}
bool HullView::editingEnabled() const { return state_->editingEnabled; }
void HullView::setFlowPlacement(bool enabled){cancelGesture();state_->flowPlacement=enabled&&state_->snapshot&&state_->projection!=HullProjection::Perspective;if(state_->flowPlacement){state_->backgroundMode=BackgroundMode::None;setCursor(Qt::CrossCursor);}else unsetCursor();}
bool HullView::flowPlacement()const{return state_->flowPlacement;}
void HullView::setPointPlacement(bool enabled){cancelGesture();state_->pointPlacement=enabled&&state_->snapshot&&state_->projection!=HullProjection::Perspective;if(state_->pointPlacement){state_->backgroundMode=BackgroundMode::None;setCursor(Qt::CrossCursor);}else unsetCursor();}
bool HullView::pointPlacement()const{return state_->pointPlacement;}
QPointF HullView::flowSource(QPointF screen)const{
    // A flow source is a continuous world coordinate, independent of any
    // background image, its pixel grid, scale or calibration.
    const auto offset=(screen-project({}))/state_->scale;
    QPointF point(offset.x(),-offset.y());
    if(state_->snapshot&&state_->snapshot->bothSides&&state_->projection==HullProjection::Body)point.setX(std::abs(point.x()));
    return point;
}
int HullView::backgroundView()const{
    switch(state_->projection){case HullProjection::Body:return 0;case HullProjection::Profile:return 1;case HullProjection::Plan:return 2;case HullProjection::Perspective:return 3;}return 3;
}
const fs::BackgroundImage* HullView::background()const{return state_->snapshot?state_->snapshot->background(backgroundView()):nullptr;}
QRectF HullView::backgroundRect()const{const auto* b=background();return b?b->targetRect(project({}),state_->scale):QRectF{};}
double HullView::backgroundImportScale(int width)const{
    if(width<=0)return 1;
    if(state_->snapshot&&!state_->snapshot->backgrounds.isEmpty())return state_->snapshot->backgrounds.back().scale;
    return double(float(this->width()/(state_->scale*width)));
}
BackgroundMode HullView::backgroundMode()const{return state_->backgroundMode;}
bool HullView::setBackgroundMode(BackgroundMode mode){
    cancelGesture();finishPresentation();
    if(mode!=BackgroundMode::None&&(!state_->editingEnabled||!background()||!background()->visible)){state_->backgroundMode=BackgroundMode::None;return false;}
    state_->backgroundMode=mode;if(mode!=BackgroundMode::None)setCursor(Qt::CrossCursor);update();return true;
}
QPointF HullView::backgroundReference(QPointF screen)const{
    const auto* b=background();if(!b)return {};
    auto offset=(screen-project({}))/(state_->scale*b->scale);return {std::nearbyint(offset.x())*b->scale,-std::nearbyint(offset.y())*b->scale};
}
bool HullView::calibrateBackgroundAt(QPointF screen,QPointF coordinate){
    const auto* b=background();double scale;
    if(!state_->editingEnabled||!b||!b->visible||!fs::calibrateBackground(*b,(screen-project({}))/(state_->scale*b->scale),coordinate,scale))return false;
    if(onBackgroundChanged)onBackgroundChanged({{"scale",scale}});return true;
}
QString HullView::projectionLabel() const {
    if(state_->studio){
        if(state_->projection==HullProjection::Body)return QStringLiteral("船体横剖面型线视图");
        if(state_->projection==HullProjection::Profile)return QStringLiteral("纵剖线图");
    }
    switch(state_->projection) {
    case HullProjection::Profile:return QStringLiteral("侧视图");
    case HullProjection::Plan:return QStringLiteral("俯视图");
    case HullProjection::Body:return QStringLiteral("正视图");
    case HullProjection::Perspective:return QStringLiteral("透视图");
    }
    return {};
}
bool HullView::gpuReady() const { return state_->display && state_->display->gpu && state_->display->gpu->ready(); }

void HullView::fit() {
    finishPresentation();state_->labels.clear();
    ++state_->cameraRevision;
    if (!state_->display) {
        state_->scale=1;state_->offset=QPointF(width()/2.0,height()/2.0);
        state_->fitted=true;update();return;
    }
    state_->scale=1;state_->offset={};
    const auto& mesh=state_->display->mesh;
    state_->cameraCenter=mesh.center;state_->cameraExtent=mesh.extent;
    double left=std::numeric_limits<double>::max(),right=-left,top=left,bottom=-left;
    for (int corner=0;corner<8;++corner) {
        const auto point=state_->projectPoint({corner&1?mesh.maximum.x:mesh.minimum.x,
            corner&2?mesh.maximum.y:mesh.minimum.y,corner&4?mesh.maximum.z:mesh.minimum.z}).pixel;
        left=qMin(left,point.x());right=qMax(right,point.x());
        top=qMin(top,point.y());bottom=qMax(bottom,point.y());
    }
    const double spanX=qMax(right-left,mesh.extent*0.03);
    const double spanY=qMax(bottom-top,mesh.extent*0.03);
    // Leave room for the original dimension strings at both ends of the grid.
    // A tight mesh-only fit otherwise clips DWL/baseline labels on first open.
    const int horizontal=state_->studio?std::min(160,width()/3):56;
    state_->scale=qMin(qMax(1,width()-horizontal)/spanX,qMax(1,height()-(state_->studio?112:72))/spanY);
    state_->offset={width()/2.0-(left+right)*state_->scale/2,
                    height()/2.0+9-(top+bottom)*state_->scale/2};
    state_->fitted=true;
    update();
}
void HullView::panBy(QPointF delta) {
    if (!std::isfinite(delta.x()) || !std::isfinite(delta.y())) return;
    finishPresentation();state_->labels.translate(delta);
    state_->offset+=delta;state_->fitted=false;++state_->cameraRevision;update();
}
void HullView::zoomAt(QPointF anchor,double factor) {
    if (!std::isfinite(factor) || factor<=0 || !std::isfinite(anchor.x()) || !std::isfinite(anchor.y())) return;
    finishPresentation();
    const double extent=state_->display?state_->display->mesh.extent:1;
    const double next=qBound(1e-4/extent,state_->scale*factor,1e7/extent);
    state_->offset=anchor+(state_->offset-anchor)*(next/state_->scale);
    state_->scale=next;state_->fitted=false;++state_->cameraRevision;update();
}
QPointF HullView::project(const fs::Vec3& point) const { return state_->projectPoint(point).pixel; }
QPointF HullView::projectControlPoint(fs::EntityId id)const{
    const auto* point=state_->snapshot?state_->snapshot->point(id):nullptr;
    return point?project(state_->controlPosition(point->position)):QPointF{};
}
QImage HullView::image(qreal dpr) const {
    if (!std::isfinite(dpr) || dpr<=0 || width()*dpr>16384 || height()*dpr>16384) return {};
    QImage image(QSize(qMax(1,qRound(width()*dpr)),qMax(1,qRound(height()*dpr))),QImage::Format_RGB32);
    image.setDevicePixelRatio(dpr);
    QPainter painter(&image);
    draw(painter,dpr);
    return image;
}

QImage HullView::softwareImage(qreal dpr)const{
    if(!std::isfinite(dpr)||dpr<=0||width()*dpr>16384||height()*dpr>16384)return {};
    QImage result(QSize(qMax(1,qRound(width()*dpr)),qMax(1,qRound(height()*dpr))),QImage::Format_RGB32);result.setDevicePixelRatio(dpr);QPainter painter(&result);draw(painter,dpr,true,true);return result;
}
QImage HullView::thumbnailImage(qreal dpr)const{
    if(!std::isfinite(dpr)||dpr<=0||width()*dpr>16384||height()*dpr>16384)return {};
    QImage result(QSize(qMax(1,qRound(width()*dpr)),qMax(1,qRound(height()*dpr))),QImage::Format_RGB32);
    result.setDevicePixelRatio(dpr);result.fill(state_->preferred("viewportcolor",QColor(232,239,236)));
    QPainter painter(&result);painter.drawImage(QPoint(0,0),state_->softwareImage(size(),dpr));return result;
}
void HullView::draw(QPainter& painter,qreal dpr,bool final,bool forceSoftware) const {
    painter.fillRect(rect(),state_->studio?state_->preferred("viewportcolor",QColor(232,239,236)):Background);
    const auto* backgroundData=!final&&state_->backgroundPreviewing?&state_->backgroundPreview:background();
    if(backgroundData&&backgroundData->visible){
        painter.save();painter.setClipRect(rect());painter.setOpacity(backgroundData->alpha/255.0);
        painter.setRenderHint(QPainter::SmoothPixmapTransform);
        const auto target=backgroundData->targetRect(project({}),state_->scale);const auto clipped=target.intersected(QRectF(rect()));
        if(!clipped.isEmpty()){
            const double sx=backgroundData->pixels.width()/target.width(),sy=backgroundData->pixels.height()/target.height();
            const QRectF source((clipped.x()-target.x())*sx,(clipped.y()-target.y())*sy,clipped.width()*sx,clipped.height()*sy);
            painter.drawImage(clipped,backgroundData->maskedPixels(),source);
        }painter.restore();
    }
    const auto& data=state_->display;
    if (data) {
        if (state_->fillVisible) {
        if(state_->shadeMode!=HullShade::Flat){
            if(state_->snapshot->surfaceTopology)prepareAnalysis(*data,*state_->snapshot);
            else state_->shadeMode=HullShade::Flat;
        }
        if (!forceSoftware && !data->translucent && !data->gpuAttempted && !data->mesh.triangles.isEmpty()) {
            data->gpuAttempted=true;
            data->gpu=std::make_unique<HullGpu>(data->mesh);
        }
        if (state_->fillImage.isNull() || state_->fillRevision!=state_->cameraRevision ||
            state_->fillSize!=size() || state_->fillDpr!=dpr || state_->fillLightRevision!=state_->lightRevision || state_->fillSoftware!=forceSoftware) {
            QImage fill;
            // Keep extreme close-up coordinates in double precision on the CPU.
            if (!forceSoftware && !data->translucent && data->gpu && data->gpu->ready() && state_->scale*data->mesh.extent<1e6)
                if(state_->shadeMode==HullShade::Flat||data->gpu->setAnalysis(data->shading))fill=data->gpu->render(size(),dpr,state_->camera(size()),state_->lighting(),state_->style());
            state_->usedGpu=!fill.isNull();
            if (fill.isNull()) fill=state_->softwareImage(size(),dpr);
            state_->fillImage=std::move(fill);
            state_->fillRevision=state_->cameraRevision;
            state_->fillLightRevision=state_->lightRevision;state_->fillSoftware=forceSoftware;++state_->fillBuilds;
            state_->fillSize=size();state_->fillDpr=dpr;
        }
        painter.drawImage(QPoint(0,0),state_->fillImage);
        }

        painter.setRenderHint(QPainter::Antialiasing,true);
        using namespace fs::presentation;
        const bool preview=!final&&state_->entrance.preview();
        const auto snapshot=state_->snapshot;
        QFont annotationFont("Arial",snapshot?snapshot->overlay.fontSize:9);
        auto overlay=snapshot?makeOverlay(*snapshot,int(state_->projection),state_->gridVisible,state_->hydroVisible&&!state_->fillVisible,annotationFont,[this](fs::Vec3 p){return project(p);},state_->sectionVisible):ViewOverlay{};
        auto strokes=[&](const QVector<Stroke>& lines){for(const auto& line:lines){painter.setPen(QPen(line.color,line.width));painter.drawLine(line.line);}};
        if(!preview){
            strokes(overlay.grid);
            if(final||!state_->entrance.active())strokes(overlay.wave);
            else{
                // Clip before graph construction; extreme zoom must not allocate
                // subdivisions along millions of off-screen pixels.
                QVector<Stroke> clipped;QRectF viewport=rect();
                for(auto s:overlay.wave){double lo=0,hi=1;auto l=s.line;auto cut=[&](double a,double d,double minimum,double maximum){if(std::abs(d)<1e-8)return a>=minimum&&a<=maximum;auto u=(minimum-a)/d,v=(maximum-a)/d;lo=std::max(lo,std::min(u,v));hi=std::min(hi,std::max(u,v));return lo<=hi;};
                    if(cut(l.x1(),l.dx(),viewport.left(),viewport.right())&&cut(l.y1(),l.dy(),viewport.top(),viewport.bottom())){s.line={l.pointAt(lo),l.pointAt(hi)};clipped.append(s);}}
                state_->graph.prepare(clipped,rect().center());strokes(state_->graph.visible(state_->entrance.lineFraction()));
            }
        }
        if(state_->snapshot)for(const auto& section:state_->snapshot->sections) {
            if(preview)continue;
            if(!state_->sectionVisible[section.kind-1])continue;
            const bool selected=state_->highlightedKind==section.kind&&state_->highlightedSections.contains(section.index);
            const bool dotted=(section.kind==1&&(state_->projection==HullProjection::Profile||state_->projection==HullProjection::Plan))||
                (section.kind==2&&(state_->projection==HullProjection::Body||state_->projection==HullProjection::Plan))||
                (section.kind==3&&(state_->projection==HullProjection::Profile||state_->projection==HullProjection::Body));
            const bool comb=snapshot->showCurvature&&section.showCurvature&&!state_->fillVisible;
            if(dotted&&state_->gridVisible&&snapshot->overlay.present&&!comb)continue;
            painter.setPen(QPen(selected?QColor(160,67,34):dotted?QColor(150,160,157):sourceColor(section.color),selected?2:1,dotted?Qt::DotLine:Qt::SolidLine));
            for(int index=0;index<section.paths.size();++index){
                const auto& path=section.paths[index];auto sides=sectionSides(*snapshot,int(state_->projection),section.kind,path);
                const QVector<CombPoint>* samples=nullptr;
                if(comb&&index<section.splines.size()){
                    const quint64 key=(quint64(section.kind*10000+section.index)<<32)|quint32(index);
                    if(!data->combs.contains(key)){
                        QVector<CombPoint> values;
                        try{values=curvatureComb(section.splines[index],snapshot->precision);}catch(const std::exception&){++data->curvatureErrors;}
                        data->combs.insert(key,std::move(values));++data->curvatureBuilds;
                    }
                    samples=&data->combs[key];
                }
                for(bool mirror:{false,true}){
                    if(mirror?!sides.second:!sides.first)continue;
                    auto pixel=[&](fs::Vec3 p){return project(mirror?mirrored(p):p);};
                    QPolygonF pixels;
                    if(samples&&!samples->isEmpty()){
                        QPolygonF tips;QVector<QLineF> ribs;
                        for(const auto& p:*samples){auto a=pixel(p.base),b=pixel(combTip(p,float(snapshot->curvatureScale)));pixels.append(a);tips.append(b);if(p.rib)ribs.append({a,b});}
                        painter.save();painter.setPen(QPen(sourceColor(snapshot->curvatureColor),1));painter.drawLines(ribs);painter.drawPolyline(tips);painter.restore();
                    }else{pixels.reserve(path.size());for(auto p:path)pixels.append(pixel(p));}
                    painter.drawPolyline(pixels);
                }
            }
        }
        if(!preview){
            state_->prepareCurvePaths();
            for(int i=0;i<data->curves.size();++i){const auto& c=data->curves[i];if(!state_->curveVisible(c))continue;
                const bool selected=!c.marker&&state_->kind==(c.flow?HullEntity::Flowline:HullEntity::Curve)&&state_->entities.contains(c.id);
                painter.setPen(QPen(selected?QColor(160,67,34):c.color,selected?2:1));painter.setBrush(Qt::NoBrush);painter.drawPath(state_->curvePaths[i].path);
                if(selected&&!c.flow&&snapshot->showCurvature&&!state_->fillVisible){
                    if(!data->curveCombs.contains(c.id)){
                        QVector<CombPoint> comb;
                        for(const auto& curve:snapshot->controlCurves)if(curve.id==c.id){try{comb=curvatureComb(curve.spline,snapshot->precision);}catch(const std::exception&){++data->curvatureErrors;}break;}
                        data->curveCombs.insert(c.id,std::move(comb));++data->curvatureBuilds;
                    }
                    for(bool mirror:{false,true}){if(mirror&&(!snapshot->bothSides||state_->projection==HullProjection::Profile))continue;
                        auto pixel=[&](fs::Vec3 p){p=state_->controlPosition(p);return project(mirror?mirrored(p):p);};QPolygonF tips;QVector<QLineF> ribs;
                        for(const auto& sample:data->curveCombs[c.id]){auto tip=pixel(combTip(sample,float(snapshot->curvatureScale)));tips.append(tip);if(sample.rib)ribs.append({pixel(sample.base),tip});}
                        painter.save();painter.setPen(QPen(sourceColor(snapshot->curvatureColor),1));painter.drawLines(ribs);painter.drawPolyline(tips);painter.restore();
                    }
                }
                if(c.hasSource){painter.setPen(QColor(65,65,65));painter.setBrush(Qt::white);painter.drawEllipse(project(c.source),3,3);
                    if(snapshot->bothSides&&state_->projection!=HullProjection::Profile)painter.drawEllipse(project(mirrored(c.source)),3,3);}
            }
        }
        if(snapshot&&snapshot->showNormals&&!preview&&state_->kind==HullEntity::Face&&!state_->entities.isEmpty()){
            painter.save();painter.setPen(QPen(sourceColor(snapshot->normalColor),1));
            QSet<fs::EntityId> symmetric;for(const auto& f:snapshot->faces)if(snapshot->layer(f.layer)->symmetric)symmetric.insert(f.id);
            for(const auto& normal:snapshot->surfaceNormals){
                if(!state_->entities.contains(normal.face))continue;
                const auto center=normal.position,n=normal.direction;const double scale=data->mesh.extent*.025;
                fs::Vec3 tip{center.x+n.x*scale,center.y+n.y*scale,center.z+n.z*scale};
                auto spans=state_->controlSegments(center,tip);
                if(snapshot->bothSides&&state_->projection!=HullProjection::Profile&&symmetric.contains(normal.face))spans.append({mirrored(center),mirrored(tip)});
                for(const auto& span:spans){
                    QLineF line(project(span[0]),project(span[1]));painter.drawLine(line);const auto d=line.p2()-line.p1();const double size=std::hypot(d.x(),d.y());
                    if(size>4){auto u=d/size;QPointF v(-u.y(),u.x());painter.drawLine(line.p2(),line.p2()-u*5+v*2);painter.drawLine(line.p2(),line.p2()-u*5-v*2);}
                }
            }
            painter.restore();
        }
        if(state_->controlNetVisible) {
        QVector<QLineF> edges,creases,mirrors;
        edges.reserve(data->edges.size());creases.reserve(data->edges.size()/3);
        const QRectF clip=QRectF(rect()).adjusted(-8,-8,8,8);
        for (const auto& edge:data->edges) {
            auto a=state_->displayedPosition(data->points[data->pointIndices.value(edge.a)]);
            auto b=state_->displayedPosition(data->points[data->pointIndices.value(edge.b)]);
            if(preview){auto span=std::max(1e-6,snapshot->maximum.x-snapshot->minimum.x);auto t=std::clamp(state_->entrance.previewFraction()*1.6-((a.x+b.x)*.5-snapshot->minimum.x)/span*.6,0.,1.);t=1-(1-t)*(1-t);if(t<=0)continue;b={a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t,a.z+(b.z-a.z)*t};}
            for(const auto& span:state_->controlSegments(a,b)){
                const QLineF line(project(span[0]),project(span[1]));const QRectF bounds(line.p1(),line.p2());
                if(clip.intersects(bounds.normalized().adjusted(-1,-1,1,1)))(edge.crease?creases:edges).append(line);
            }
            if (edge.mirror && (a.y!=0 || b.y!=0)) {
                const QLineF mirror(project(mirrored(a)),project(mirrored(b)));
                if (clip.intersects(QRectF(mirror.p1(),mirror.p2()).normalized().adjusted(-1,-1,1,1)))
                    mirrors.append(mirror);
            }
        }
        painter.setPen(QPen(QColor(91,112,127,145),0.8,Qt::DashLine));
        painter.drawLines(mirrors);
        if(!state_->studio){painter.setPen(QPen(QColor(255,255,255,155),2.5));painter.drawLines(edges);painter.drawLines(creases);}
        painter.setPen(QPen(state_->studio?state_->preferred("edgecolor",QColor(102,133,125)):QColor(18,74,123),state_->studio?1.3:0.95));painter.drawLines(edges);
        painter.setPen(QPen(state_->studio?state_->preferred("creaseedgecolor",QColor(160,67,34)):QColor(13,39,64),1.3));painter.drawLines(creases);
        QVector<QPointF> points;
        QVector<QRectF> locked;
        points.reserve(data->points.size());
        for (const auto& point:data->points) {
            const auto pixel=project(state_->controlPosition(state_->displayedPosition(point)));
            if (!clip.contains(pixel)) continue;
            if(!final&&!state_->entrance.pointVisible(pixel.x()))continue;
            if(state_->studio&&snapshot->overlay.present){
                QColor base=point.locked?QColor(128,128,128):point.leak?state_->preferred("leakpointcolor",QColor(0,255,153)):point.vertexType==1?state_->preferred("creasepointcolor",QColor(98,77,50)):point.vertexType==2?state_->preferred("dartpointcolor",QColor(Qt::magenta)):point.vertexType==3?state_->preferred("cornerpointcolor",QColor(82,65,58)):state_->preferred("regularpointcolor",QColor(49,93,85));
                if(!final)base=state_->entrance.pointColor(point.id,base);
                int radius=std::clamp(snapshot->overlay.pointSize,1,100);painter.fillRect(QRectF(pixel-QPointF(radius,radius),QSizeF(radius*2+1,radius*2+1)),base);continue;
            }
            if (point.locked) locked.append(QRectF(pixel-QPointF(2.5,2.5),QSizeF(5,5)));
            else points.append(pixel);
        }
        painter.setPen(QPen(QColor(245,249,252),5.5,Qt::SolidLine,Qt::RoundCap));painter.drawPoints(points);
        painter.setPen(QPen(state_->studio?QColor(49,93,85):QColor(12,75,134),3.2,Qt::SolidLine,state_->studio?Qt::SquareCap:Qt::RoundCap));painter.drawPoints(points);
        painter.setPen(QPen(QColor(69,77,84),1));painter.setBrush(QColor(236,239,242));painter.drawRects(locked);
        const auto& selection=state_->gesture==State::Gesture::Box?state_->boxPreview:state_->selection;
        QVector<QPointF> selected;
        QVector<QRectF> selectedLocked;
        selected.reserve(selection.size());
        for (auto id:selection) {
            const auto found=data->pointIndices.constFind(id);
            if (found==data->pointIndices.cend()) continue;
            const auto& point=data->points[found.value()];
            const auto pixel=project(state_->controlPosition(state_->displayedPosition(point)));
            if (!clip.contains(pixel)) continue;
            if(!final&&!state_->entrance.pointVisible(pixel.x()))continue;
            if (point.locked) selectedLocked.append(QRectF(pixel-QPointF(4,4),QSizeF(8,8)));
            else selected.append(pixel);
        }
        painter.setPen(QPen(QColor(255,255,255),11,Qt::SolidLine,Qt::RoundCap));painter.drawPoints(selected);
        painter.setPen(QPen(state_->preferred("selectcolor",QColor(224,101,25)),8,Qt::SolidLine,Qt::RoundCap));painter.drawPoints(selected);
        painter.setPen(QPen(QColor(255,255,255),2));painter.setBrush(state_->preferred("selectcolor",QColor(224,101,25)));
        painter.drawRects(selectedLocked);
        }
        if(!preview&&snapshot&&snapshot->overlay.present){
            LabelLayout exportLayout;auto& layout=final?exportLayout:state_->labels;
            layout.begin(QRectF(rect()).adjusted(3,state_->studio?30:3,-3,-3),state_->frameTime,!final&&state_->animations);
            if(state_->controlNetVisible)for(const auto& p:data->points){auto at=project(state_->controlPosition(state_->displayedPosition(p)));int r=snapshot->overlay.pointSize+3;layout.reserve(QRectF(at-QPointF(r,r),QSizeF(2*r+1,2*r+1)));}
            if(state_->controlNetVisible)for(const auto& edge:data->edges)for(const auto& span:state_->controlSegments(state_->displayedPosition(data->points[data->pointIndices.value(edge.a)]),state_->displayedPosition(data->points[data->pointIndices.value(edge.b)])))layout.segment({project(span[0]),project(span[1])});
            painter.setFont(annotationFont);QFontMetricsF metrics(annotationFont);QJsonArray evidence;
            auto annotation=[&](const Annotation& a){
                auto size=QSizeF(metrics.horizontalAdvance(a.text),metrics.height());auto placed=layout.place(a.key,a.text,a.anchor,a.preferred,size,a.kind);
                if(!placed.visible)return;
                const bool numeric=!a.text.isEmpty()&&(a.text[0].isDigit()||a.text[0]=='-'||a.text[0]=='+');
                painter.save();painter.setOpacity(final?1:state_->entrance.textOpacity(numeric));painter.setPen(a.color);
                if(a.symbol){painter.setBrush(Qt::white);painter.drawEllipse(a.anchor,4,4);painter.setBrush(Qt::black);painter.drawPie(QRectF(a.anchor-QPointF(4,4),QSizeF(8,8)),0,90*16);painter.drawPie(QRectF(a.anchor-QPointF(4,4),QSizeF(8,8)),180*16,90*16);}
                if(a.kind==LabelKind::Point){auto end=QPointF(std::clamp(a.anchor.x(),placed.clip.left(),placed.clip.right()),std::clamp(a.anchor.y(),placed.clip.top(),placed.clip.bottom()));if(QLineF(a.anchor,end).length()>=28)painter.drawLine(a.anchor,end);}
                auto clip=placed.clip;if(!final)clip=clip.intersected(QRectF(placed.position,QSizeF(size.width()*state_->entrance.textFraction(numeric),size.height())));
                painter.setClipRect(clip);painter.drawText(placed.position+QPointF(0,metrics.ascent()),a.text);painter.restore();
                evidence.append(QJsonObject{{"key",a.key},{"text",a.text},{"rect",QJsonArray{placed.clip.x(),placed.clip.y(),placed.clip.width(),placed.clip.height()}},{"anchor",QJsonArray{a.anchor.x(),a.anchor.y()}}});
            };
            for(const auto& a:overlay.labels)if(a.kind==LabelKind::Named)annotation(a);
            if(state_->controlNetVisible){
                if(!state_->fillVisible)for(const auto& group:snapshot->pointGroups){
                    if(group.points.isEmpty()||group.name.isEmpty())continue;
                    fs::Vec3 center{};int count=0;bool selected=false;
                    for(auto id:group.points){auto at=data->pointIndices.constFind(id);if(at==data->pointIndices.cend())continue;
                        const auto p=state_->displayedPosition(data->points[*at]);center.x+=p.x;center.y+=p.y;center.z+=p.z;++count;selected|=state_->selection.contains(id);}
                    if(!count)continue;center.x/=count;center.y/=count;center.z/=count;
                    const auto at=project(state_->controlPosition(center));const auto color=selected?QColor(160,67,34):QColor(49,93,85);
                    painter.setPen(color);painter.setBrush(Qt::NoBrush);painter.drawEllipse(at,2,2);painter.drawEllipse(at,4,4);
                    annotation({"group:"+QString::number(group.index),group.name,at,at+QPointF(6,3),color,LabelKind::Point,false});
                }
                // Selected points claim coincident labels first, without copying
                // and sorting the control net on every animation frame.
                for(bool selected:{true,false})for(const auto& p:data->points){
                    if(state_->selection.contains(p.id)!=selected)continue;
                    auto at=project(state_->controlPosition(state_->displayedPosition(p)));if(!rect().contains(at.toPoint())||(!final&&!state_->entrance.pointVisible(at.x())))continue;
                    QString text=p.name;if(state_->coordinateAxis){const auto v=state_->displayedPosition(p);text=QString::number(state_->coordinateAxis==1?v.x:state_->coordinateAxis==2?v.y:v.z,'f',3);if(!layout.claim(at,text))continue;}
                    if(text.isEmpty())continue;
                    annotation({"point:"+QString::number(p.id),text,at,at+QPointF(snapshot->overlay.pointSize+4,3),selected?QColor(160,67,34):QColor(49,93,85),LabelKind::Point,false});
                }
            }
            for(const auto& a:overlay.labels)if(a.kind!=LabelKind::Named)annotation(a);
            state_->evidence={{"labels",evidence},{"grid_lines",overlay.grid.size()},{"wave_lines",overlay.wave.size()},{"graph_builds",state_->graph.buildCount()},{"axis",state_->coordinateAxis},{"animation_active",state_->entrance.active()}};
        }
        if(state_->kind==HullEntity::Edge) {
            painter.setPen(QPen(QColor(160,67,34),3));
            for(const auto& edge:data->edges)if(state_->entities.contains(edge.id))
                for(const auto& span:state_->controlSegments(data->points[data->pointIndices.value(edge.a)].position,data->points[data->pointIndices.value(edge.b)].position))painter.drawLine(project(span[0]),project(span[1]));
        } else if(state_->kind==HullEntity::Face && state_->snapshot) {
            painter.setPen(QPen(QColor(160,67,34),2));painter.setBrush(QColor(160,67,34,48));
            for(const auto& face:data->faces)if(state_->entities.contains(face.id)) {
                QVector<fs::Vec3> points;for(auto index:face.points)points.append(data->points[index].position);
                for(const auto& p:state_->controlPolygons(points)){QPolygonF polygon;for(auto point:p)polygon.append(project(point));painter.drawPolygon(polygon);}
            }
        }
        if (state_->gesture==State::Gesture::Box && state_->moved) {
            painter.setPen(QPen(QColor(36,106,175),1,Qt::DashLine));
            painter.setBrush(QColor(36,106,175,28));
            painter.drawRect(QRectF(state_->press,state_->last).normalized());
        }
        state_->evidence.insert("curvature_builds",data->curvatureBuilds);state_->evidence.insert("curvature_errors",data->curvatureErrors);
        state_->evidence.insert("both_sides",snapshot->bothSides);state_->evidence.insert("split_body",state_->splitBody());
        state_->evidence.insert("auxiliary_path_builds",state_->curvePathBuilds);state_->evidence.insert("auxiliary_curves",data->curves.size());
    } else {
        painter.setPen(QColor(105,114,123));
        painter.drawText(rect(),Qt::AlignCenter,QStringLiteral("打开船型文件"));
    }
    painter.setRenderHint(QPainter::Antialiasing,false);
    if(!final&&backgroundData&&backgroundData->visible&&state_->backgroundMode==BackgroundMode::Frame){
        painter.setPen(QPen(Qt::yellow,3,Qt::DashLine));painter.setBrush(Qt::NoBrush);painter.drawRect(backgroundData->targetRect(project({}),state_->scale));
    }
    if(state_->studio)painter.fillRect(QRect(0,0,width(),25),QColor(224,236,232));
    painter.setPen(state_->studio?QColor(0,61,69):QColor(44,57,68));
    painter.drawText(QRect(12,8,width()-24,22),Qt::AlignLeft|Qt::AlignVCenter,projectionLabel());
    if(state_->fillVisible&&state_->shadeMode!=HullShade::Flat&&data&&data->analysis){
        QString text;
        if(state_->shadeMode==HullShade::Gaussian)text=QString("角亏（°）：%1 至 %2；灰色：边界或无法评估").arg(data->analysis->minimum,0,'g',4).arg(data->analysis->maximum,0,'g',4);
        else if(state_->shadeMode==HullShade::Developable)text="绿：角亏接近零；红：非零；灰：无法评估";
        else text="斑马纹 · 检查曲面法向连续性";
        painter.save();painter.setFont(QFont("Microsoft YaHei UI",8));const auto box=painter.fontMetrics().boundingRect(QRect(8,0,qMax(1,width()-16),100),Qt::TextWordWrap,text);
        QRect area(4,height()-box.height()-10,width()-8,box.height()+6);painter.fillRect(area,QColor(246,249,247,235));painter.setPen(QColor(30,53,50));painter.drawText(area.adjusted(4,3,-4,-3),Qt::TextWordWrap,text);painter.restore();
    }
    painter.setPen(QColor(215,222,228));painter.setBrush(Qt::NoBrush);
    painter.drawRect(rect().adjusted(0,0,-1,-1));
}

void HullView::paintEvent(QPaintEvent*) {
    QElapsedTimer timer;timer.start();state_->frameTime=state_->clock.elapsed();state_->entrance.beginFrame(state_->frameTime,width());
    QPainter painter(this);draw(painter,devicePixelRatioF(),false);state_->entrance.endFrame();
    if((state_->entrance.active()||state_->labels.animating())&&!state_->entrance.frozen())state_->timer.start();else state_->timer.stop();
    lastPaintMs=timer.nsecsElapsed()/1e6;++paintCount;
}
void HullView::resizeEvent(QResizeEvent* event) {
    if (state_->fitted) fit();
    else if (event->oldSize().isValid())
        state_->offset+=QPointF((event->size().width()-event->oldSize().width())/2.0,
                               (event->size().height()-event->oldSize().height())/2.0);
    ++state_->cameraRevision;
    QWidget::resizeEvent(event);
}
void HullView::cancelGesture() {
    state_->gesture=State::Gesture::None;state_->moved=false;
    state_->backgroundPreviewing=false;
    state_->backgroundStart={};state_->backgroundPreview={};
    state_->dragStarts.clear();state_->dragDelta={};state_->blockedReason.clear();
    state_->boxBase.clear();state_->boxPreview.clear();
    unsetCursor();update();
}
void HullView::mousePressEvent(QMouseEvent* event) {
    if (event->button()!=Qt::LeftButton && event->button()!=Qt::RightButton) {
        QWidget::mousePressEvent(event);return;
    }
    setFocus(Qt::MouseFocusReason);
    finishPresentation();
    cancelGesture();
    state_->press=state_->last=event->position();
    if(state_->pointPlacement){
        if(event->button()==Qt::RightButton){setPointPlacement(false);state_->lastRightDragged=true;state_->moved=true;if(onPointPlacementEnded)onPointPlacementEnded();event->accept();return;}
        if(!state_->editingEnabled){event->accept();return;}
        const auto p=flowSource(event->position());fs::Vec3 position{};
        switch(state_->projection){case HullProjection::Profile:position={p.x(),0,p.y()};break;case HullProjection::Plan:position={p.x(),p.y(),0};break;case HullProjection::Body:position={state_->snapshot->maximum.x,p.x(),p.y()};break;default:event->accept();return;}
        setPointPlacement(false);if(onPointPlacementEnded)onPointPlacementEnded();if(onPointCreationRequested)onPointCreationRequested(position);event->accept();return;
    }
    if(state_->flowPlacement){
        if(event->button()==Qt::RightButton){setFlowPlacement(false);state_->lastRightDragged=true;state_->moved=true;if(onFlowPlacementEnded)onFlowPlacementEnded();event->accept();return;}
        if(!state_->editingEnabled){event->accept();return;}
        const auto source=flowSource(event->position());
        if(onFlowRequested)onFlowRequested(backgroundView(),source);event->accept();return;
    }
    if(state_->backgroundMode!=BackgroundMode::None){
        if(event->button()==Qt::RightButton){state_->backgroundMode=BackgroundMode::None;state_->lastRightDragged=true;state_->moved=true;event->accept();return;}
        const auto* b=background();
        if(!state_->editingEnabled||!b||!b->visible){setBackgroundMode(BackgroundMode::None);event->accept();return;}
        if(state_->backgroundMode==BackgroundMode::Scale){state_->backgroundMode=BackgroundMode::None;if(onBackgroundAction)onBackgroundAction("scale_at",event->position());event->accept();return;}
        if(state_->backgroundMode==BackgroundMode::TransparentColor){
            auto p=b->imagePoint(event->position(),project({}),state_->scale);int x=int(std::floor(p.x())),y=int(std::floor(p.y()));
            if(b->pixels.rect().contains(x,y)){auto c=b->pixels.pixel(x,y);state_->backgroundMode=BackgroundMode::None;if(onBackgroundChanged)onBackgroundChanged({{"transparent",true},{"transparent_color",qRed(c)|(qGreen(c)<<8)|(qBlue(c)<<16)}});}
            event->accept();return;
        }
        state_->backgroundSide=state_->backgroundMode==BackgroundMode::Frame?fs::backgroundSide(backgroundRect(),event->position()):0;
        if(state_->backgroundSide>=0){state_->backgroundStart=*b;state_->backgroundPreview=*b;state_->backgroundPreviewing=true;state_->gesture=State::Gesture::Background;setCursor(Qt::SizeAllCursor);}
        event->accept();return;
    }
    if(event->button()==Qt::LeftButton && state_->kind!=HullEntity::Point &&
       !event->modifiers().testFlag(Qt::AltModifier)) {
        const auto id=entityAt(event->position());
        const bool additive=event->modifiers().testFlag(Qt::ControlModifier)||event->modifiers().testFlag(Qt::ShiftModifier);
        auto ids=additive?state_->entities:QSet<fs::EntityId>{};
        if(id){if(ids.contains(id))ids.remove(id);else ids.insert(id);}
        setSelectedEntities(ids);
        if(onEntitySelectionChanged)onEntitySelectionChanged(state_->kind,state_->entities);
        event->accept();return;
    }
    if (event->button()==Qt::RightButton) {
        state_->lastRightDragged=false;
        state_->gesture=State::Gesture::Pan;setCursor(Qt::ClosedHandCursor);
    } else {
        auto id=state_->hit(event->position());
        const bool additive=event->modifiers().testFlag(Qt::ControlModifier) || event->modifiers().testFlag(Qt::ShiftModifier);
        QVector<fs::EntityId> groupPick;fs::Vec3 groupCenter{};
        if(!id&&state_->snapshot&&state_->display&&state_->controlNetVisible&&!state_->fillVisible){
            for(const auto& g:state_->snapshot->pointGroups){
                if(g.points.isEmpty()||g.name.isEmpty())continue;fs::Vec3 center{};bool visible=true;
                for(auto p:g.points){auto at=state_->display->pointIndices.constFind(p);if(at==state_->display->pointIndices.cend()){visible=false;break;}
                    auto pos=state_->displayedPosition(state_->display->points[*at]);center.x+=pos.x;center.y+=pos.y;center.z+=pos.z;}
                if(!visible)continue;center.x/=g.points.size();center.y/=g.points.size();center.z/=g.points.size();
                if(QLineF(project(state_->controlPosition(center)),event->position()).length()<=7){groupPick=g.points;groupCenter=center;id=g.points.first();break;}
            }
        }
        if (state_->projection==HullProjection::Perspective && (!id || event->modifiers().testFlag(Qt::AltModifier))) {
            state_->gesture=State::Gesture::Rotate;setCursor(Qt::SizeAllCursor);
        } else if (!id) {
            state_->gesture=State::Gesture::Box;
            state_->boxBase=additive?state_->selection:QSet<fs::EntityId>{};
            state_->boxPreview=state_->boxBase;
            setCursor(Qt::CrossCursor);
        } else {
            if(!groupPick.isEmpty()){
                QSet<fs::EntityId> members(groupPick.begin(),groupPick.end());
                if(!additive)state_->selection=members;
                else if((members-state_->selection).isEmpty())state_->selection.subtract(members);
                else state_->selection.unite(members);
            }else if (additive) {
                if (state_->selection.contains(id)) state_->selection.remove(id);
                else state_->selection.insert(id);
            } else if (!state_->selection.contains(id)) state_->selection={id};
            if (state_->selection.contains(id)) state_->selected=id;
            state_->pruneSelection();
            notifySelectionChanged();
            // Modifier clicks toggle membership; they do not also start a move.
            if (!additive && state_->editingEnabled && state_->selection.contains(id) &&
                state_->display && state_->projection!=HullProjection::Perspective) {
                auto ids=state_->selection.values();
                std::sort(ids.begin(),ids.end());
                for (auto selected:ids) {
                    const auto found=state_->display->pointIndices.constFind(selected);
                    if (found==state_->display->pointIndices.cend()) {
                        state_->blockedReason=QStringLiteral("所选点包含不可见点，不能整体移动。");break;
                    }
                    const auto& point=state_->display->points[found.value()];
                    if (point.locked) {
                        state_->blockedReason=QStringLiteral("所选点包含锁定点，不能整体移动。");break;
                    }
                    if (ids.size()>1 && !point.batchMovable) {
                        state_->blockedReason=QStringLiteral("所选点包含约束点，不能整体移动，请逐点编辑。");break;
                    }
                    state_->dragStarts.append({point.id,point.position});
                }
                if (state_->blockedReason.isEmpty()) {
                    const auto* anchor=state_->snapshot->point(id);
                    state_->dragBodySign=state_->splitBody()&&anchor&&anchor->position.x<=fs::presentation::splitLocation(*state_->snapshot)?-1:1;
                    if(!groupPick.isEmpty())state_->dragBodySign=state_->splitBody()&&groupCenter.x<=fs::presentation::splitLocation(*state_->snapshot)?-1:1;
                    state_->gesture=State::Gesture::Move;setCursor(Qt::CrossCursor);
                } else {
                    state_->dragStarts.clear();state_->gesture=State::Gesture::BlockedMove;
                }
            }
            update();
        }
    }
    event->accept();
}
void HullView::mouseMoveEvent(QMouseEvent* event) {
    const auto delta=event->position()-state_->last;
    switch(state_->gesture) {
    case State::Gesture::Background:{
        const auto total=event->position()-state_->press;fs::BackgroundCalibration next;
        if(total.manhattanLength()>=3&&fs::resizeBackground(state_->backgroundStart,project({}),state_->scale,state_->backgroundSide,total,next)){
            state_->backgroundPreview.origin=next.origin;state_->backgroundPreview.scale=next.scale;state_->moved=true;update();
        }break;
    }
    case State::Gesture::Pan:state_->moved|=(event->position()-state_->press).manhattanLength()>=3;panBy(delta);break;
    case State::Gesture::Rotate:
        state_->yaw+=delta.x()*0.008;
        state_->pitch=qBound(-Pi*0.48,state_->pitch+delta.y()*0.008,Pi*0.48);
        state_->fitted=false;++state_->cameraRevision;update();break;
    case State::Gesture::Move: {
        const auto total=event->position()-state_->press;
        // A selection click does not create an undo step due to small hand jitter.
        if (!state_->moved && total.manhattanLength()<3) break;
        state_->moved=true;state_->dragDelta={};
        switch(state_->projection) {
        case HullProjection::Profile:state_->dragDelta.x=total.x()/state_->scale;state_->dragDelta.z=-total.y()/state_->scale;break;
        case HullProjection::Plan:state_->dragDelta.x=total.x()/state_->scale;state_->dragDelta.y=-total.y()/state_->scale;break;
        case HullProjection::Body:state_->dragDelta.y=state_->dragBodySign*total.x()/state_->scale;state_->dragDelta.z=-total.y()/state_->scale;break;
        case HullProjection::Perspective:break;
        }
        update();break;
    }
    case State::Gesture::Box: {
        const auto total=event->position()-state_->press;
        if (!state_->moved && total.manhattanLength()<3) break;
        state_->moved=true;state_->boxPreview=state_->boxBase;
        const auto box=QRectF(state_->press,event->position()).normalized().intersected(QRectF(rect()));
        if (state_->display) for (const auto& point:state_->display->points)
            if (box.contains(project(state_->controlPosition(point.position)))) state_->boxPreview.insert(point.id);
        update();break;
    }
    case State::Gesture::BlockedMove:
        if ((event->position()-state_->press).manhattanLength()>=3) {
            const auto reason=state_->blockedReason;
            cancelGesture();
            if (onEditRejected) onEditRejected(reason);
        }
        break;
    case State::Gesture::None:
        if(state_->backgroundMode!=BackgroundMode::None||state_->flowPlacement||state_->pointPlacement)setCursor(Qt::CrossCursor);
        else if (state_->hit(event->position())) setCursor(Qt::PointingHandCursor);else unsetCursor();
        break;
    }
    state_->last=event->position();event->accept();
}
void HullView::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button()!=Qt::LeftButton && event->button()!=Qt::RightButton) {
        QWidget::mouseReleaseEvent(event);return;
    }
    // Qt can compress move messages; use the actual release position as well.
    if (state_->gesture!=State::Gesture::None && event->position()!=state_->last) mouseMoveEvent(event);
    QVector<fs::PointMove> moves;
    QJsonObject backgroundChanges;
    if(state_->gesture==State::Gesture::Background&&state_->moved&&state_->editingEnabled){
        const auto& next=state_->backgroundPreview;const auto& before=state_->backgroundStart;
        if(next.origin!=before.origin)backgroundChanges.insert("origin",QJsonArray{next.origin.x(),next.origin.y()});
        if(next.scale!=before.scale)backgroundChanges.insert("scale",next.scale);
    }
    if(state_->backgroundMode==BackgroundMode::Origin)state_->backgroundMode=BackgroundMode::None;
    if (state_->editingEnabled && state_->gesture==State::Gesture::Move && state_->moved &&
        !samePosition(state_->dragDelta,{})) {
        moves.reserve(state_->dragStarts.size());
        bool anyChanged=false;
        for (const auto& point:state_->dragStarts) {
            const auto position=state_->movedPosition(point.position);
            anyChanged=anyChanged || !samePosition(point.position,position);
            moves.append({point.id,position});
        }
        if (!anyChanged) moves.clear();
    }
    const bool select=state_->gesture==State::Gesture::Box;
    const auto acceptedBackgroundPreview=state_->backgroundPreview;
    if(event->button()==Qt::RightButton)state_->lastRightDragged=state_->moved;
    if (select) {state_->selection=state_->boxPreview;state_->pruneSelection();}
    cancelGesture();
    if(!backgroundChanges.isEmpty()&&onBackgroundChanged){
        // Keep the released preview until the committed snapshot arrives. A
        // failed request re-enables editing and clears it without changing data.
        state_->backgroundPreview=acceptedBackgroundPreview;state_->backgroundPreviewing=true;onBackgroundChanged(backgroundChanges);
    }
    if (select) notifySelectionChanged();
    if (!moves.isEmpty()) {
        if (onPointsMoved) onPointsMoved(std::move(moves));
        else if (moves.size()==1 && onPointMoved) onPointMoved(moves[0].id,moves[0].position);
        else if (onEditRejected) onEditRejected(QStringLiteral("当前操作入口不支持多点移动。"));
    }
    event->accept();
}
void HullView::mouseDoubleClickEvent(QMouseEvent* event) {
    if(state_->backgroundMode!=BackgroundMode::None||state_->flowPlacement||state_->pointPlacement){event->accept();return;}
    if (event->button()==Qt::LeftButton && !state_->hit(event->position())) {cancelGesture();fit();event->accept();}
    else QWidget::mouseDoubleClickEvent(event);
}
void HullView::wheelEvent(QWheelEvent* event) {
    const double delta=!event->pixelDelta().isNull()?event->pixelDelta().y()*2.0:event->angleDelta().y();
    if (delta!=0) {cancelGesture();zoomAt(event->position(),std::pow(1.0015,delta));}
    event->accept();
}
void HullView::keyPressEvent(QKeyEvent* event) {
    if (event->key()==Qt::Key_Escape) {setBackgroundMode(BackgroundMode::None);setFlowPlacement(false);setPointPlacement(false);if(onFlowPlacementEnded)onFlowPlacementEnded();if(onPointPlacementEnded)onPointPlacementEnded();event->accept();}
    else if (event->key()==Qt::Key_Home || event->key()==Qt::Key_F) {cancelGesture();fit();event->accept();}
    else QWidget::keyPressEvent(event);
}
void HullView::focusOutEvent(QFocusEvent* event) {cancelGesture();QWidget::focusOutEvent(event);}
