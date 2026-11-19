#include "app/ui_language.h"
#include "app/math_notation.h"
#include "hull_view.h"
#include "text_style.h"
#include "underwater_color.h"
#include "background_geometry.h"
#include "body_projection.h"
#include "curvature_comb.h"
#include "auxiliary_geometry.h"
#include <QPainterPathStroker>
#include <QThreadPool>
#include <QThread>
#include <future>
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
#include <QApplication>
#include <QResizeEvent>
#include <QSet>
#include <QShortcut>
#include <QWheelEvent>
#include <QAbstractAnimation>
#ifdef Q_OS_WIN
#include <QtCore/qt_windows.h>
#endif
#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <unordered_map>
#include <vector>
#include <tuple>

namespace {
// Screen display only. Bound each replaced span's deviation from its chord.
// Chunking also caps worst-case work on noisy/self-intersecting paths.
QPolygonF screenStroke(const QPolygonF& points,double tolerance){
    if(points.size()<4)return points;
    QPolygonF result;result.append(points.first());
    const auto reduce=[&](auto&& self,int first,int last)->void{
        const auto a=points[first],delta=points[last]-a;
        const double length2=QPointF::dotProduct(delta,delta);
        int split=-1;double worst=tolerance*tolerance;
        for(int i=first+1;i<last;++i){
            const auto v=points[i]-a;
            const auto t=length2>0?std::clamp(QPointF::dotProduct(v,delta)/length2,0.,1.):0.;
            const auto diff=v-delta*t;const auto e=QPointF::dotProduct(diff,diff);
            if(e>worst){worst=e;split=i;}
        }
        if(split!=-1){self(self,first,split);self(self,split,last);}else result.append(points[last]);
    };
    for(int start=0;start<points.size()-1;start+=96)reduce(reduce,start,std::min(start+96,int(points.size()-1)));
    return result;
}
QPointF pixelPhase(QPointF origin){return {origin.x()-std::floor(origin.x()),origin.y()-std::floor(origin.y())};}
bool hullMotionEnabled(){
    if(qApp){const auto setting=qApp->property("studio_motion_enabled");if(setting.isValid())return setting.toBool();}
#ifdef Q_OS_WIN
    BOOL animate=TRUE;if(SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION,0,&animate,0))return animate;
#endif
    return true;
}
qint64 interactionTime(){static QElapsedTimer clock;static const bool started=[](){clock.start();return true;}();Q_UNUSED(started);return clock.elapsed();}
double interactionEase(double t){t=std::clamp(t,0.,1.);return 1-std::pow(1-t,3);}
class InteractionDriver final:public QAbstractAnimation {
public:
    explicit InteractionDriver(std::function<void()> tick):tick_(std::move(tick)){}
    int duration()const override{return -1;}
protected:
    void updateCurrentTime(int)override{tick_();}
private:std::function<void()> tick_;
};
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
struct DisplayEdge { fs::EntityId a, b; bool crease, mirror; fs::EntityId id; qsizetype ia,ib; };
struct DisplayFace {fs::EntityId id;QVector<int> points;};
struct DisplayCurve {fs::EntityId id;bool flow;QColor color;QVector<fs::presentation::Cubic> cubics;fs::Vec3 source;bool hasSource=false;bool marker=false,visible=true;};
struct DisplayData {
    // A metadata-only successor can keep this display alive after its original
    // model is replaced. Keep the cache key's owner alive too; otherwise a new
    // geometry allocation could reuse that address and hit stale display data.
    std::shared_ptr<const fs::ModelSnapshot> owner;
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

std::shared_ptr<const DisplayData> displayData(std::shared_ptr<const fs::ModelSnapshot> owner,bool bothSides,bool splitBody,const QSet<fs::EntityId>& hidden,
        const QSet<fs::EntityId>& flipped,const DisplayData* previous) {
    const auto &snapshot=*owner;
    // Four cameras share geometry. Only live displays retain their source;
    // this weak index does not keep unused revisions alive.
    static std::map<std::tuple<const fs::ModelSnapshot*,bool,bool,std::vector<fs::EntityId>,std::vector<fs::EntityId>>, std::weak_ptr<const DisplayData>> cache;
    for (auto it=cache.begin(); it!=cache.end();) {
        if (it->second.expired()) it=cache.erase(it); else ++it;
    }
    std::vector<fs::EntityId> hiddenIds(hidden.begin(),hidden.end());std::sort(hiddenIds.begin(),hiddenIds.end());
    std::vector<fs::EntityId> flips(flipped.begin(),flipped.end());std::sort(flips.begin(),flips.end());
    auto key=std::make_tuple(&snapshot,bothSides,splitBody,hiddenIds,flips);auto found=cache.find(key);
    if (found!=cache.end()) if (auto existing=found->second.lock()) return existing;
    auto result=std::make_shared<DisplayData>();
    result->owner=std::move(owner);
    if(previous)result->curves=previous->curves; // unchanged spline data, implicitly shared
    else {
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
    const auto visibleLayer=[&](const fs::Layer* layer){return layer&&(layer->visible!=flipped.contains(layer->id));};
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
        if (!visibleLayer(layer)) continue;
        QColor color=sourceColor(layer->color);color.setAlpha(layer->alphaBlend);result->translucent|=layer->alphaBlend!=255;
        if(splitBody){
            const auto polygons=fs::presentation::bodyPolygons({triangle.vertices[0],triangle.vertices[1],triangle.vertices[2]},fs::presentation::splitLocation(snapshot));
            for(const auto& p:polygons){bool reflected=std::any_of(p.begin(),p.end(),[&](auto v){return v.x<fs::presentation::splitLocation(snapshot);});
                if(!reflected)reflected=std::all_of(triangle.vertices.begin(),triangle.vertices.end(),[&](auto v){return v.x<=fs::presentation::splitLocation(snapshot);});
                for(int i=1;i+1<p.size();++i)appendTriangle({p[0],p[i],p[i+1]},color,source,reflected);
            }
        }else appendTriangle(triangle.vertices,color,source,false);
        if (bothSides && layer->symmetric && (triangle.vertices[0].y!=0 || triangle.vertices[1].y!=0 ||
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
            if (visibleLayer(layer) && !hidden.contains(layer->id)) visible.insert(point);
        }
        if (!face.points.isEmpty()) {
            for (qsizetype i=0;i<face.points.size();++i) {
                const auto key=edgeKey(face.points[i],face.points[(i+1)%face.points.size()]);
                ownedEdges.insert(key);
                if (visibleLayer(layer) && !hidden.contains(layer->id)) {
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
        const bool mirror=bothSides&&symmetricEdges.contains(edgeKey(edge.a,edge.b));
        result->edges.append({edge.a,edge.b,edge.crease,mirror,edge.id,result->pointIndices.value(edge.a),result->pointIndices.value(edge.b)});
        if (mirror) {
            include(mirrored(result->points[result->pointIndices.value(edge.a)].position));
            include(mirrored(result->points[result->pointIndices.value(edge.b)].position));
        }
    }
    for(const auto& face:snapshot.faces) {
        const auto* layer=layers.value(face.layer,nullptr);if(!visibleLayer(layer) || hidden.contains(layer->id))continue;
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
    std::shared_ptr<const fs::ModelSnapshot> committedSnapshot, snapshot;
    std::shared_ptr<const DisplayData> display;
    HullProjection projection=HullProjection::Profile;
    fs::EntityId selected=0;
    QSet<fs::EntityId> selection;
    HullEntity kind=HullEntity::Point;
    QSet<fs::EntityId> entities;
    bool studio=false,fillVisible=true,controlNetVisible=true;
    mutable bool paintedHull=false;
    QSet<fs::EntityId> hiddenControlLayers,visibilityChanges;
    std::shared_ptr<const DisplayData> previousVisibilityDisplay;
    QJsonObject preferenceColors;
    QColor preferred(const QString& key,QColor fallback)const{
        return preferenceColors.contains(key)?QColor::fromRgb(preferenceColors[key].toInt()):fallback;
    }
    bool gridVisible=true,hydroVisible=true,animations=true;
    int coordinateAxis=0;
    mutable fs::presentation::Entrance entrance;
    struct SectionLayer {
        QPixmap pixels;
        std::weak_ptr<const fs::ModelSnapshot> source;
        quint64 camera=0;
        QSize size;
        std::array<bool,4> visibility{};
        int highlightedKind=0;
        QSet<int> highlightedSections;
        bool grid=false,fill=false;
    };
    mutable SectionLayer entranceSections;
    struct ProjectedSection {QPolygonF line, mirror; bool initialized=false;};
    mutable std::weak_ptr<const fs::ModelSnapshot> sectionSource;
    mutable HullProjection sectionProjection=HullProjection::Profile;
    mutable fs::Vec3 sectionCenter{};
    mutable double sectionExtent=0,sectionYaw=0,sectionPitch=0,sectionCap=0;
    mutable int sectionFocal=0;
    mutable QHash<quintptr,ProjectedSection> sectionLines;
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
    struct CurvePath {QPainterPath path,hit;quint64 hitRevision=~quint64(0);};
    mutable QVector<CurvePath> curvePaths;
    mutable QTransform curvePathTransform;
    mutable QPointF curvePathOffset;
    mutable double curvePathScale=1;
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
    // Resize is delivered to the four widgets before their paints. Paint independent
    // section layers concurrently, in the exact new camera, not a stretched old image.
    struct ResizeLayer {
        std::shared_future<QImage> image;
        quint64 revision=0;
        QSize size; qreal dpr=0; QPointF phase;
        bool grid=false,fill=false;
        std::array<bool,4> visible{};
        std::weak_ptr<const fs::ModelSnapshot> source;
    } resizeLayer;
    bool layerMatches(QSize size,qreal dpr) const {
        return resizeLayer.image.valid() && resizeLayer.revision==cameraRevision &&
            resizeLayer.source.lock()==snapshot && resizeLayer.size==size && resizeLayer.dpr==dpr &&
            resizeLayer.visible==sectionVisible && resizeLayer.grid==gridVisible && resizeLayer.fill==fillVisible &&
            highlightedSections.isEmpty() && !entrance.active();
    }
    void scheduleSections(QSize size,qreal dpr,QPointF phase){
        resizeLayer={};
        const QSize pixels(qCeil(size.width()*dpr+phase.x()),qCeil(size.height()*dpr+phase.y()));
        if(!snapshot||snapshot->sections.size()<8||pixels.isEmpty()||qint64(pixels.width())*pixels.height()>4*1024*1024||
           entrance.active()||!highlightedSections.isEmpty()||sectionSource.lock()!=snapshot||sectionProjection!=projection||
           !samePosition(sectionCenter,cameraCenter)||sectionExtent!=cameraExtent||sectionYaw!=yaw||sectionPitch!=pitch||
           sectionFocal!=focalLength||scale*dpr>sectionCap||scale*dpr*4<sectionCap)return;
        struct Stroke {QColor color;QPolygonF path;};
        QVector<Stroke> paths;
        for(const auto& section:snapshot->sections){
            if(!sectionVisible[section.kind-1])continue;
            if(snapshot->showCurvature&&section.showCurvature&&!fillVisible)return;
            const bool dotted=(section.kind==1&&(projection==HullProjection::Profile||projection==HullProjection::Plan))||
                (section.kind==2&&(projection==HullProjection::Body||projection==HullProjection::Plan))||
                (section.kind==3&&(projection==HullProjection::Profile||projection==HullProjection::Body));
            if(dotted){if(gridVisible&&snapshot->overlay.present)continue;return;}
            const auto color=sourceColor(section.color);
            for(const auto& path:section.paths){
                const auto found=sectionLines.constFind(reinterpret_cast<quintptr>(&path));
                if(found==sectionLines.cend()||!found->initialized)return;
                if(!found->line.isEmpty())paths.append({color,found->line});
                if(!found->mirror.isEmpty())paths.append({color,found->mirror});
            }
        }
        if(paths.isEmpty())return;
        const QTransform transform(scale,0,0,scale,offset.x(),offset.y());
        // Workers own their paint device and implicitly shared immutable polygons;
        // never capture a widget or model pointer. A saturated pool falls back inline.
        auto task=std::make_shared<std::packaged_task<QImage()>>([pixels,dpr,phase,transform,paths=std::move(paths)]{
            QImage image(pixels,QImage::Format_ARGB32_Premultiplied);
            if(image.isNull())return image;
            image.setDevicePixelRatio(dpr);image.fill(Qt::transparent);
            QPainter painter(&image);painter.setRenderHint(QPainter::Antialiasing);painter.translate(phase/dpr);
            for(const auto& stroke:paths){painter.setPen(QPen(stroke.color,1));painter.drawPolyline(transform.map(stroke.path));}
            painter.end();return image;
        });
        auto future=task->get_future().share();
        struct SectionPool : QThreadPool {SectionPool(){setMaxThreadCount(std::clamp(QThread::idealThreadCount()-1,1,4));}};
        static SectionPool pool;
        if(pool.tryStart([task]{(*task)();}))
            resizeLayer={std::move(future),cameraRevision,size,dpr,phase,gridVisible,fillVisible,sectionVisible,snapshot};
    }
    struct CameraPose {double scale=1,extent=1;QPointF offset;fs::Vec3 center;};
    struct CameraVelocity {double logScale=0,extent=0;QPointF offset;fs::Vec3 center;};
    struct CameraMotion {bool active=false;CameraPose from,to;qint64 start=0;int duration=120;CameraVelocity fromVelocity,velocity;} cameraMotion;
    struct HoverMotion {fs::EntityId id=0;HullEntity kind=HullEntity::Point;double alpha=0,from=0,target=0;qint64 start=0;int duration=100;};
    QVector<HoverMotion> hoverMotion;
    fs::EntityId hovered=0;
    QSet<fs::EntityId> emphasized;HullEntity emphasizedKind=HullEntity::Point;qint64 emphasizedAt=-1000;
    QSet<fs::EntityId> announcedSelection;
    std::unique_ptr<InteractionDriver> interactionDriver;
    QRectF interactionDirty;
    CameraPose cameraPose()const{return {scale,cameraExtent,offset,cameraCenter};}
    void applyCamera(const CameraPose& pose){
        const auto revision=cameraRevision;const bool rebased=!samePosition(cameraCenter,pose.center)||cameraExtent!=pose.extent;
        scale=pose.scale;offset=pose.offset;cameraCenter=pose.center;cameraExtent=pose.extent;
        ++cameraRevision;if(rebased)curvePathRevision=~quint64(0);else reuseCurvePaths(revision);
    }
    void sampleCamera(qint64 now){
        if(!cameraMotion.active)return;
        const double time=std::clamp(double(now-cameraMotion.start)/cameraMotion.duration,0.,1.);
        const auto& a=cameraMotion.from;const auto& b=cameraMotion.to;
        const auto spring=[&](double from,double to,double velocity,double& nextVelocity){
            const double delta=from-to,carry=velocity*cameraMotion.duration+9*delta,decay=std::exp(-9*time);
            nextVelocity=(carry-9*(delta+carry*time))*decay/cameraMotion.duration;
            return to+(delta+carry*time)*decay;
        };
        const auto& initial=cameraMotion.fromVelocity;auto& velocity=cameraMotion.velocity;CameraPose value;
        const double logScale=spring(std::log(a.scale),std::log(b.scale),initial.logScale,velocity.logScale);
        const double extent=display?display->mesh.extent:1;
        value.scale=qBound(1e-4/extent,std::exp(logScale),1e7/extent);
        if(value.scale!=std::exp(logScale))velocity.logScale=0;
        value.extent=qMax(1e-12,spring(a.extent,b.extent,initial.extent,velocity.extent));
        double vx=0,vy=0;
        value.offset={spring(a.offset.x(),b.offset.x(),initial.offset.x(),vx),spring(a.offset.y(),b.offset.y(),initial.offset.y(),vy)};velocity.offset={vx,vy};
        value.center={spring(a.center.x,b.center.x,initial.center.x,velocity.center.x),spring(a.center.y,b.center.y,initial.center.y,velocity.center.y),spring(a.center.z,b.center.z,initial.center.z,velocity.center.z)};
        if(time>=1){value=b;velocity={};cameraMotion.active=false;}applyCamera(value);
    }
    QPainterPath interactionShape(HullEntity kind,fs::EntityId id)const{
        QPainterPath path;if(!display)return path;
        if(kind==HullEntity::Point){const auto index=display->pointIndices.constFind(id);if(index!=display->pointIndices.cend())path.addEllipse(projectPoint(controlPosition(displayedPosition(display->points[*index]))).pixel,6,6);}
        else if(kind==HullEntity::Edge){for(const auto& edge:display->edges)if(edge.id==id){for(const auto& span:controlSegments(display->points[display->pointIndices.value(edge.a)].position,display->points[display->pointIndices.value(edge.b)].position)){path.moveTo(projectPoint(span[0]).pixel);path.lineTo(projectPoint(span[1]).pixel);}break;}}
        else if(kind==HullEntity::Face){for(const auto& face:display->faces)if(face.id==id){QVector<fs::Vec3> points;for(auto index:face.points)points.append(display->points[index].position);for(const auto& polygon:controlPolygons(points)){QPolygonF pixels;for(auto point:polygon)pixels.append(projectPoint(point).pixel);path.addPolygon(pixels);}break;}}
        else {prepareCurvePaths();for(int i=0;i<display->curves.size();++i)if(display->curves[i].id==id&&display->curves[i].flow==(kind==HullEntity::Flowline)){path=curvePathTransform.map(curvePaths[i].path);break;}}
        return path;
    }
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
    void rebuildDisplay(bool visibilityOnly=false){
        // Keep just the preceding visibility combination for an immediate toggle back.
        const auto next=snapshot?displayData(committedSnapshot,snapshot->bothSides,splitBody(),hiddenControlLayers,
            visibilityChanges,visibilityOnly?display.get():nullptr):nullptr;
        previousVisibilityDisplay=visibilityOnly?display:nullptr;display=next;
    }
    fs::Vec3 controlPosition(fs::Vec3 p)const{return splitBody()?fs::presentation::bodyPoint(p,fs::presentation::splitLocation(*snapshot)):p;}
    QVector<fs::presentation::Segment> controlSegments(fs::Vec3 a,fs::Vec3 b)const{
        return splitBody()?fs::presentation::bodySegments(a,b,fs::presentation::splitLocation(*snapshot)):QVector<fs::presentation::Segment>{{a,b}};
    }
    QVector<QVector<fs::Vec3>> controlPolygons(const QVector<fs::Vec3>& p)const{
        return splitBody()?fs::presentation::bodyPolygons(p,fs::presentation::splitLocation(*snapshot)):QVector<QVector<fs::Vec3>>{p};
    }
    bool curveVisible(const DisplayCurve& c)const{return snapshot&&c.visible&&(c.marker?snapshot->showMarkers:c.flow?snapshot->showFlowlines&&!fillVisible:snapshot->showControlCurves);}
    void reuseCurvePaths(quint64 previousRevision){
        if(curvePathRevision!=previousRevision||projection==HullProjection::Perspective)return;
        const double ratio=scale/curvePathScale;
        const auto translation=offset-curvePathOffset*ratio;
        curvePathTransform=QTransform(ratio,0,0,ratio,translation.x(),translation.y());
        curvePathRevision=cameraRevision;
    }
    void prepareCurvePaths(bool original=false)const{
        // Export keeps the original per-control-point projection and arithmetic.
        if(original&&!curvePathTransform.isIdentity())curvePathRevision=~quint64(0);
        if(curvePathRevision==cameraRevision)return;
        curvePaths.clear();
        curvePathTransform.reset();curvePathOffset=offset;curvePathScale=scale;
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
    for(int axis=1;axis<=3;++axis){
        const auto key=Qt::Key(Qt::Key_1+axis-1);
        auto* shortcut=new QShortcut(QKeySequence(QKeyCombination(Qt::ControlModifier|Qt::ShiftModifier,key)),this);
        shortcut->setContext(Qt::WidgetShortcut);
        shortcut->setAutoRepeat(false);
        shortcut->setProperty("coordinate_shortcut_axis",axis);
        shortcut->setProperty("shortcut_target","当前视图");
        connect(shortcut,&QShortcut::activated,this,[this,axis]{
            if(state_->snapshot)chooseCoordinateAxis(axis==state_->coordinateAxis?0:axis);
        });
    }
    setMouseTracking(true);
    setMinimumSize(100,80);
    bindUiText(this,"accessibleName",projectionSourceKey());
    state_->clock.start();state_->timer.setInterval(16);state_->timer.setTimerType(Qt::PreciseTimer);
    connect(&state_->timer,&QTimer::timeout,this,[this]{if(isVisible())update();else state_->timer.stop();});
    state_->interactionDriver=std::make_unique<InteractionDriver>([this]{advanceInteractionMotion();});
}
HullView::~HullView() = default;

void HullView::setSnapshot(std::shared_ptr<const fs::ModelSnapshot> snapshot,
                           std::shared_ptr<const fs::ModelSnapshot> presentation,bool drawingUnchanged,const QSet<fs::EntityId>& visibilityChanges) {
    if (snapshot==state_->committedSnapshot){setLayerVisibilityPreview(visibilityChanges);return;}
    const bool maskChanged=state_->visibilityChanges!=visibilityChanges;
    state_->visibilityChanges=visibilityChanges;
    const bool sameDocument=snapshot && state_->committedSnapshot && snapshot->documentId==state_->committedSnapshot->documentId;
    const bool sameUnits=snapshot && state_->committedSnapshot && snapshot->units==state_->committedSnapshot->units;
    if(!sameDocument){state_->cameraMotion.active=false;state_->hovered=0;state_->hoverMotion.clear();state_->emphasized.clear();state_->announcedSelection.clear();state_->interactionDriver->stop();}
    if(drawingUnchanged&&sameDocument&&sameUnits&&snapshot->sameDrawing(*state_->committedSnapshot)) {
        const bool completePreview=state_->committedSnapshot->isCachedPreview()&&!snapshot->isCachedPreview();
        state_->committedSnapshot=std::move(snapshot);
        state_->snapshot=presentation?std::move(presentation):state_->committedSnapshot;
        if(completePreview||maskChanged){state_->rebuildDisplay();++state_->cameraRevision;}
        update();return;
    }
    cancelGesture();
    state_->committedSnapshot=std::move(snapshot);
    state_->snapshot=presentation?std::move(presentation):state_->committedSnapshot;
    if(!sameDocument){state_->flowPlacement=false;state_->pointPlacement=false;state_->hiddenControlLayers.clear();}
    if(!sameDocument||!background()||!background()->visible)state_->backgroundMode=BackgroundMode::None;
    state_->rebuildDisplay();
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
std::shared_ptr<const fs::ModelSnapshot> HullView::snapshot() const { return state_->committedSnapshot; }
void HullView::setDisplayPreview(std::shared_ptr<const fs::ModelSnapshot> presentation) {
    if(!state_->committedSnapshot)return;
    if(!presentation)presentation=state_->committedSnapshot;
    if(state_->snapshot==presentation)return;
    if(presentation->documentId!=state_->committedSnapshot->documentId||
       presentation->revision!=state_->committedSnapshot->revision)return;
    const auto& previous=*state_->snapshot;
    if(previous.bothSides==presentation->bothSides&&previous.showNormals==presentation->showNormals&&
       previous.showMarkers==presentation->showMarkers&&previous.showCurvature==presentation->showCurvature&&
       previous.showControlCurves==presentation->showControlCurves&&previous.showFlowlines==presentation->showFlowlines&&
       previous.curvatureScale==presentation->curvatureScale){
        state_->snapshot=std::move(presentation);
        return;
    }
    const bool mirrorChanged=previous.bothSides!=presentation->bothSides;
    state_->snapshot=std::move(presentation);
    if(mirrorChanged){
        cancelGesture();
        state_->rebuildDisplay();
        ++state_->cameraRevision;
        state_->labels.clear();
    }
    finishPresentation();
}
void HullView::setProjection(HullProjection projection) {
    if (state_->projection==projection) return;
    cancelGesture();
    state_->projection=projection;
    state_->light.enabled=projection==HullProjection::Perspective;++state_->lightRevision;
    state_->rebuildDisplay();
    state_->backgroundMode=BackgroundMode::None;
    bindUiText(this,"accessibleName",projectionSourceKey());
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
    const auto previous=state_->selection;state_->selection=std::move(ids);
    state_->pruneSelection();
    emphasizeSelection(previous,state_->selection,HullEntity::Point);
    update();
}
QSet<fs::EntityId> HullView::selectedPoints() const { return state_->selection; }
void HullView::setSelectionKind(HullEntity kind) {
    updateInteractionHover(0);cancelGesture();state_->kind=kind;state_->entities.clear();state_->emphasized.clear();update();
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
    const auto previous=state_->entities;state_->entities=std::move(valid);emphasizeSelection(previous,state_->entities,state_->kind);update();
}
void HullView::setStudioAppearance(bool enabled){state_->studio=enabled;bindUiText(this,"accessibleName",projectionSourceKey());update();}
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
void HullView::setLayerVisibilityPreview(const QSet<fs::EntityId>& changes){
    if(changes==state_->visibilityChanges)return;
    state_->visibilityChanges=changes;state_->rebuildDisplay(true);
    state_->pruneSelection();setSelectedEntities(state_->entities);
    // The camera and projected reference curves did not move. Only the surface
    // fill and the label obstacles depend on the newly visible geometry.
    state_->fillImage={};state_->labels.clear();finishPresentation();
}
void HullView::setHiddenControlLayers(const QSet<fs::EntityId>& ids){
    if(state_->hiddenControlLayers==ids)return;state_->hiddenControlLayers=ids;
    state_->rebuildDisplay();
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
void HullView::chooseCoordinateAxis(int axis){
    if(axis!=0&&onShowControlNet)onShowControlNet();
    setCoordinateAxis(axis);
}
void HullView::setAnimationsEnabled(bool enabled){state_->animations=enabled;if(!enabled){finishPresentation();finishInteractionCamera();state_->hovered=0;state_->hoverMotion.clear();state_->emphasized.clear();state_->interactionDriver->stop();update();}}
void HullView::restartPresentation(){if(!state_->animations)return;state_->labels.clear();state_->entrance.restart();state_->timer.start();update();}
void HullView::finishPresentation(){state_->entrance.finish();state_->entranceSections.pixels={};state_->timer.stop();update();}
void HullView::freezePresentation(bool frozen){if(frozen){state_->entrance.pause(state_->clock.elapsed());state_->timer.stop();}else{state_->entrance.resume(state_->clock.elapsed());if(state_->entrance.active())state_->timer.start();update();}}
bool HullView::presentationActive()const{return state_->entrance.active();}
QJsonObject HullView::overlayEvidence()const{return state_->evidence;}
QJsonObject HullView::interactionMotionEvidence()const{
    QJsonArray hovered,selected;
    for(const auto& layer:state_->hoverMotion)hovered.append(QJsonObject{{"id",double(layer.id)},{"kind",int(layer.kind)},{"opacity",layer.alpha},{"target",layer.target}});
    for(auto id:state_->emphasized)selected.append(double(id));
    const auto& camera=state_->cameraMotion;
    return {{"hover",hovered},{"selected_ids",selected},{"selected_kind",int(state_->emphasizedKind)},
        {"selection_elapsed_ms",double(interactionTime()-state_->emphasizedAt)},{"camera_active",camera.active},
        {"camera_scale",state_->scale},{"camera_target_scale",camera.active?camera.to.scale:state_->scale},
        {"camera_offset_x",state_->offset.x()},{"camera_offset_y",state_->offset.y()},
        {"camera_log_velocity",camera.velocity.logScale},{"camera_initial_log_velocity",camera.fromVelocity.logScale},
        {"driver_running",state_->interactionDriver->state()==QAbstractAnimation::Running},
        {"motion_enabled",state_->animations&&hullMotionEnabled()}};
}
QImage HullView::presentationImageAt(qint64 milliseconds){
    state_->frameTime=milliseconds;state_->entrance.beginFrame(milliseconds,width());
    QImage result(size(),QImage::Format_ARGB32_Premultiplied);result.fill(Qt::transparent);QPainter painter(&result);draw(painter,1,false);state_->entrance.endFrame();return result;
}
void HullView::hideEvent(QHideEvent* event){freezePresentation(true);finishInteractionCamera();state_->hoverMotion.clear();state_->hovered=0;state_->emphasized.clear();state_->interactionDriver->stop();QWidget::hideEvent(event);}
void HullView::showEvent(QShowEvent* event){freezePresentation(false);QWidget::showEvent(event);}
void HullView::populateBackgroundMenu(QMenu* backgroundMenu){
        backgroundMenu->setProperty("menu_role","background");
        backgroundMenu->setEnabled(bool(state_->snapshot));
        const auto* b=background();
        for(const auto& item:QList<QPair<QString,QString>>{{"import",uiText("导入")},{"origin",uiText("原点")},{"scale",uiText("设置比例")},{"frame",uiText("调整图片边框")},{"transparent_color",uiText("透明色")},{"transparent",uiText("启用透明色")},{"alpha",uiText("混合")},{"tolerance",uiText("容差")},{"visible",uiText("显示")},{"export",uiText("保存")},{"remove",uiText("清除")}}){
            auto* a=backgroundMenu->addAction(item.second);a->setProperty("background_operation",item.first);
            a->setProperty("studio_opens_dialog",QStringList{"import","alpha","tolerance","export","remove"}.contains(item.first));
            const bool manipulate=item.first=="origin"||item.first=="scale"||item.first=="frame"||item.first=="transparent_color";
            a->setEnabled(state_->editingEnabled&&(item.first=="import"||(b&&(!manipulate||b->visible)&&(item.first!="tolerance"||b->transparent))));
            if(item.first=="visible"||item.first=="transparent"){a->setCheckable(true);a->setChecked(b&&(item.first=="visible"?b->visible:b->transparent));}
            connect(a,&QAction::triggered,this,[this,operation=item.first]{
                if(operation=="origin")setBackgroundMode(BackgroundMode::Origin);
                else if(operation=="frame")setBackgroundMode(BackgroundMode::Frame);
                else if(operation=="scale")setBackgroundMode(BackgroundMode::Scale);
                else if(operation=="transparent_color")setBackgroundMode(BackgroundMode::TransparentColor);
                else if(onBackgroundAction)onBackgroundAction(operation,{});
            },a->property("studio_opens_dialog").toBool()?Qt::QueuedConnection:Qt::DirectConnection);
        }
}

void HullView::contextMenuEvent(QContextMenuEvent* event){
    if(event->reason()==QContextMenuEvent::Mouse&&state_->lastRightDragged){event->accept();return;}
    finishPresentation();
    auto* menu=new QMenu(this);menu->setAttribute(Qt::WA_DeleteOnClose);
    menu->setAttribute(Qt::WA_DontShowOnScreen,testAttribute(Qt::WA_DontShowOnScreen));
    auto add=[&](QMenu* owner,const QString& text,const QString& operation,auto callback){
        auto* action=owner->addAction(text);
        action->setProperty("view_operation",operation);
        action->setProperty("studio_opens_dialog",operation=="light_settings"||operation=="print"||operation=="save_image");
        action->setEnabled(bool(state_->snapshot));
        // A modal event loop must not run inside QMenu's mouse-release handler:
        // the closing popup may be destroyed while Qt still dispatches its event.
        connect(action,&QAction::triggered,this,callback,
                action->property("studio_opens_dialog").toBool()?Qt::QueuedConnection:Qt::DirectConnection);
        return action;
    };
    if(state_->projection==HullProjection::Perspective){
        auto* cameras=menu->addMenu(uiText("相机"));
        cameras->setProperty("menu_role","camera");
        cameras->setEnabled(bool(state_->snapshot));
        auto* lenses=new QActionGroup(cameras);
        const QStringList lensNames{uiText("广角 28 mm"),uiText("标准 50 mm"),uiText("短焦远摄 90 mm"),uiText("中焦远摄 130 mm"),uiText("远摄 200 mm")};
        int lensIndex=0;
        for(int focal:{28,50,90,130,200}){
            auto* lens=cameras->addAction(lensNames[lensIndex++]);
            lens->setProperty("focal_mm",focal);
            lens->setCheckable(true);lens->setChecked(state_->focalLength==focal);lenses->addAction(lens);
            connect(lens,&QAction::triggered,this,[this,focal]{setFocalLength(focal);});
        }
    }
    auto* modeMenu=menu->addMenu(uiText("模式"));
    modeMenu->setProperty("menu_role","render_mode");
    auto* modes=new QActionGroup(modeMenu);
    for(bool fill:{false,true}){
        auto* mode=add(modeMenu,fill?uiText("填色"):uiText("线框"),fill?"fill":"wireframe",[this,fill]{setFillVisible(fill);});
        mode->setCheckable(true);mode->setChecked(fill?(state_->fillVisible&&state_->shadeMode==HullShade::Flat):!state_->fillVisible);modes->addAction(mode);
    }
    const QStringList shadeNames{uiText("高斯曲率"),uiText("可展性检查"),uiText("斑马纹")};
    for(int i=1;i<=3;++i){
        auto* mode=add(modeMenu,shadeNames[i-1],"shade_"+QString::number(i),[this,i]{setShadeMode(HullShade(i));});
        mode->setProperty("shade_mode",i);mode->setEnabled(state_->snapshot&&bool(state_->snapshot->surfaceTopology));
        mode->setCheckable(true);mode->setChecked(state_->fillVisible&&int(state_->shadeMode)==i);modes->addAction(mode);
    }
    if(state_->fillVisible) add(menu,uiText("亮度"),"light_settings",[this]{if(onViewAction)onViewAction("light_settings");});
    if(backgroundView()<3)populateBackgroundMenu(menu->addMenu(uiText("背景影像")));
    menu->addSeparator();
    add(menu,uiText("打印预览"),"print",[this]{if(onViewAction)onViewAction("print");});
    add(menu,uiText("保存图片"),"save_image",[this]{if(onViewAction)onViewAction("save_image");});
    if(onPopulateContextMenu)onPopulateContextMenu(*menu);
    auto* axes=menu->addMenu(uiText("显示控制点坐标"));
    axes->setProperty("menu_role","coordinates");
    axes->setEnabled(bool(state_->snapshot));
    auto* group=new QActionGroup(axes);
    const QStringList names{uiText("关闭","visibility"),"X","Y","Z"};
    for(int axis=0;axis<names.size();++axis){
        auto* action=axes->addAction(names[axis]);
        if(axis==0)bindUiText(action,"text","关闭","visibility");
        action->setCheckable(true);action->setChecked(state_->coordinateAxis==axis);
        action->setProperty("coordinate_axis",axis);group->addAction(action);
        connect(action,&QAction::triggered,this,[this,axis]{chooseCoordinateAxis(axis);});
    }
    menu->popup(event->globalPos());event->accept();
}
fs::EntityId HullView::entityAt(QPointF pixel) const {
    if(!state_->display || !state_->snapshot)return 0;
    if(state_->kind==HullEntity::Point)return state_->hit(pixel);
    if(state_->kind==HullEntity::Curve||state_->kind==HullEntity::Flowline){
        state_->prepareCurvePaths();
        for(int i=0;i<state_->display->curves.size();++i){const auto& c=state_->display->curves[i];
            if(c.marker||c.flow!=(state_->kind==HullEntity::Flowline)||!state_->curveVisible(c))continue;
            auto& cached=state_->curvePaths[i];if(cached.hitRevision!=state_->cameraRevision){QPainterPathStroker stroke;stroke.setWidth(14);cached.hit=stroke.createStroke(state_->curvePathTransform.map(cached.path));cached.hitRevision=state_->cameraRevision;}
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
    emphasizeSelection(state_->announcedSelection,state_->selection,HullEntity::Point);state_->announcedSelection=state_->selection;
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
QString HullView::projectionLabel() const {return uiText(projectionSourceKey());}
QString HullView::projectionSourceKey() const {
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

void HullView::finishInteractionCamera(){
    if(state_->cameraMotion.active){state_->applyCamera(state_->cameraMotion.to);state_->cameraMotion.active=false;state_->cameraMotion.velocity={};update();}
}
void HullView::updateInteractionHover(fs::EntityId id){
    if(state_->hovered==id)return;
    const auto now=interactionTime();state_->hovered=id;
    for(auto& layer:state_->hoverMotion){layer.from=layer.alpha;layer.target=0;layer.start=now;layer.duration=80;}
    if(id){
        auto it=std::find_if(state_->hoverMotion.begin(),state_->hoverMotion.end(),[&](const auto& layer){return layer.id==id&&layer.kind==state_->kind;});
        if(it==state_->hoverMotion.end()){state_->hoverMotion.append({id,state_->kind,0,0,1,now,100});}
        else{it->from=it->alpha;it->target=1;it->start=now;it->duration=100;}
    }
    while(state_->hoverMotion.size()>4)state_->hoverMotion.removeFirst();
    if(!state_->animations||!hullMotionEnabled()){
        for(auto& layer:state_->hoverMotion)layer.alpha=layer.target;
        state_->hoverMotion.erase(std::remove_if(state_->hoverMotion.begin(),state_->hoverMotion.end(),[](const auto& layer){return layer.target==0;}),state_->hoverMotion.end());update();
    }else if(state_->interactionDriver->state()!=QAbstractAnimation::Running)state_->interactionDriver->start();
}
void HullView::emphasizeSelection(const QSet<fs::EntityId>& previous,const QSet<fs::EntityId>& current,HullEntity kind){
    state_->emphasized.clear();
    if(kind==HullEntity::Point)state_->announcedSelection=current;
    if(!isVisible()||!state_->animations||!hullMotionEnabled()||current.size()>16)return;
    state_->emphasized=current-previous;state_->emphasizedKind=kind;state_->emphasizedAt=interactionTime();
    if(!state_->emphasized.isEmpty()&&state_->interactionDriver->state()!=QAbstractAnimation::Running)state_->interactionDriver->start();
}
void HullView::advanceInteractionMotion(){
    if(!isVisible()){state_->interactionDriver->stop();return;}
    const auto now=interactionTime();const bool camera=state_->cameraMotion.active;state_->sampleCamera(now);
    QRectF dirty=state_->interactionDirty;bool active=state_->cameraMotion.active;
    for(auto& layer:state_->hoverMotion){
        const double time=std::clamp(double(now-layer.start)/layer.duration,0.,1.);
        layer.alpha=layer.from+(layer.target-layer.from)*interactionEase(time);active|=time<1;
        dirty=dirty.united(state_->interactionShape(layer.kind,layer.id).boundingRect().adjusted(-10,-10,10,10));
    }
    state_->hoverMotion.erase(std::remove_if(state_->hoverMotion.begin(),state_->hoverMotion.end(),[](const auto& layer){return layer.alpha<.001&&layer.target==0;}),state_->hoverMotion.end());
    if(now-state_->emphasizedAt>=240)state_->emphasized.clear();
    else for(auto id:state_->emphasized){active=true;dirty=dirty.united(state_->interactionShape(state_->emphasizedKind,id).boundingRect().adjusted(-12,-12,12,12));}
    state_->interactionDirty=dirty;
    if(camera)update();else if(!dirty.isEmpty())update(dirty.toAlignedRect().intersected(rect()));
    if(!active){state_->interactionDriver->stop();state_->interactionDirty={};}
}

void HullView::fit(){fitToViewport(false);}
void HullView::fitToViewport(bool resizing) {
    state_->sampleCamera(interactionTime());
    const auto from=state_->cameraPose();
    const auto velocity=state_->cameraMotion.active?state_->cameraMotion.velocity:State::CameraVelocity{};
    const bool animate=isVisible()&&state_->display&&!state_->fitted&&state_->animations&&hullMotionEnabled();
    state_->cameraMotion.active=false;
    finishPresentation();
    if(resizing)state_->labels.viewportResized();else state_->labels.clear();
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
    if(animate){
        state_->cameraMotion={true,from,state_->cameraPose(),interactionTime(),240};state_->applyCamera(from);
        state_->cameraMotion.fromVelocity=velocity;state_->cameraMotion.velocity=velocity;
        if(state_->interactionDriver->state()!=QAbstractAnimation::Running)state_->interactionDriver->start();
    }
    update();
}
void HullView::panBy(QPointF delta) {
    if (!std::isfinite(delta.x()) || !std::isfinite(delta.y())) return;
    state_->sampleCamera(interactionTime());state_->cameraMotion.active=false;
    finishPresentation();state_->labels.translate(delta);
    const auto previousRevision=state_->cameraRevision;
    state_->offset+=delta;state_->fitted=false;++state_->cameraRevision;state_->reuseCurvePaths(previousRevision);update();
}
void HullView::zoomAt(QPointF anchor,double factor) {
    if (!std::isfinite(factor) || factor<=0 || !std::isfinite(anchor.x()) || !std::isfinite(anchor.y())) return;
    finishPresentation();
    state_->sampleCamera(interactionTime());const auto from=state_->cameraPose();
    const auto velocity=state_->cameraMotion.active?state_->cameraMotion.velocity:State::CameraVelocity{};
    const auto target=state_->cameraMotion.active?state_->cameraMotion.to:from;
    const double extent=state_->display?state_->display->mesh.extent:1;
    const double next=qBound(1e-4/extent,target.scale*factor,1e7/extent);
    auto to=target;to.offset=anchor+(target.offset-anchor)*(next/target.scale);to.scale=next;
    state_->fitted=false;
    if(isVisible()&&state_->animations&&hullMotionEnabled()){
        state_->cameraMotion={true,from,to,interactionTime(),120};
        state_->cameraMotion.fromVelocity=velocity;state_->cameraMotion.velocity=velocity;
        if(state_->interactionDriver->state()!=QAbstractAnimation::Running)state_->interactionDriver->start();
    }else{state_->cameraMotion.active=false;state_->applyCamera(to);}
    update();
}
QPointF HullView::project(const fs::Vec3& point) const { return state_->projectPoint(point).pixel; }
QPointF HullView::projectControlPoint(fs::EntityId id)const{
    const auto* point=state_->snapshot?state_->snapshot->point(id):nullptr;
    return point?project(state_->controlPosition(point->position)):QPointF{};
}
QImage HullView::image(qreal dpr) const {
    const_cast<HullView*>(this)->finishInteractionCamera();
    if (!std::isfinite(dpr) || dpr<=0 || width()*dpr>16384 || height()*dpr>16384) return {};
    QImage image(QSize(qMax(1,qRound(width()*dpr)),qMax(1,qRound(height()*dpr))),QImage::Format_RGB32);
    image.setDevicePixelRatio(dpr);
    QPainter painter(&image);
    draw(painter,dpr);
    return image;
}

QImage HullView::softwareImage(qreal dpr)const{
    const_cast<HullView*>(this)->finishInteractionCamera();
    if(!std::isfinite(dpr)||dpr<=0||width()*dpr>16384||height()*dpr>16384)return {};
    QImage result(QSize(qMax(1,qRound(width()*dpr)),qMax(1,qRound(height()*dpr))),QImage::Format_RGB32);result.setDevicePixelRatio(dpr);QPainter painter(&result);draw(painter,dpr,true,true);return result;
}
QPicture HullView::printPicture(qreal rasterRatio)const{
    const_cast<HullView*>(this)->finishInteractionCamera();
    if(!std::isfinite(rasterRatio)||rasterRatio<=0)return {};
    // Bound only raster layers, never the vector line/text resolution.
    const qreal ratio=std::min({rasterRatio,16384./qMax(1,width()),16384./qMax(1,height()),
        std::sqrt(16e6/(qreal(qMax(1,width()))*qMax(1,height())))});
    QPicture picture;QPainter painter(&picture);painter.setFont(font());draw(painter,ratio,true,true);painter.end();picture.setBoundingRect(rect());return picture;
}
QImage HullView::thumbnailImage(qreal dpr)const{
    if(!std::isfinite(dpr)||dpr<=0||width()*dpr>16384||height()*dpr>16384)return {};
    QImage result(QSize(qMax(1,qRound(width()*dpr)),qMax(1,qRound(height()*dpr))),QImage::Format_RGB32);
    result.setDevicePixelRatio(dpr);result.fill(state_->preferred("viewportcolor",QColor(232,239,236)));
    QPainter painter(&result);painter.drawImage(QPoint(0,0),state_->softwareImage(size(),dpr));return result;
}
int HullView::verifyResizeSectionPixels() const {
    const auto dpr=devicePixelRatioF();
    if(!state_->layerMatches(size(),dpr))return -1;
    QImage tmp(QSize(qCeil(width()*dpr),qCeil(height()*dpr)),QImage::Format_ARGB32_Premultiplied);
    tmp.setDevicePixelRatio(dpr);QPainter painter(&tmp);painter.translate(state_->resizeLayer.phase/dpr);int result=-1;
    draw(painter,dpr,false,false,&result);return result;
}
void HullView::draw(QPainter& painter,qreal dpr,bool final,bool forceSoftware,int* verifySections) const {
    auto titleFont=QApplication::font();titleFont.setPointSizeF(std::max(11.,titleFont.pointSizeF()));
    titleFont.setWeight(QFont::Normal);titleFont.setStyleStrategy(QFont::StyleStrategy(QFont::PreferAntialias|QFont::PreferQuality));
    const int titleHeight=std::max(32,QFontMetrics(titleFont).height()+12);
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
        if(!state_->fillImage.isNull()&&!data->mesh.triangles.isEmpty())state_->paintedHull=true;
        }

        painter.setRenderHint(QPainter::Antialiasing,true);
        using namespace fs::presentation;
        const bool preview=!final&&state_->entrance.preview();
        const auto snapshot=state_->snapshot;
        QFont annotationFont=studioTextFont(QFont("Arial",snapshot?snapshot->overlay.fontSize:9));
        auto overlay=snapshot?makeOverlay(*snapshot,int(state_->projection),state_->gridVisible,state_->hydroVisible&&!state_->fillVisible,annotationFont,[this](fs::Vec3 p){return project(p);},state_->sectionVisible):ViewOverlay{};
        auto strokes=[&](const QVector<Stroke>& lines){
            QVector<QLineF> batch;
            for(qsizetype start=0;start<lines.size();){
                const auto& first=lines[start];
                batch.clear();
                do{batch.append(lines[start++].line);}
                while(start<lines.size() && lines[start].color==first.color && lines[start].width==first.width);
                painter.setPen(QPen(first.color,first.width));painter.drawLines(batch);
            }
        };
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
        const auto drawSections=[&](QPainter& painter){
            if(!preview&&snapshot)for(const auto& section:snapshot->sections) {
                if(!state_->sectionVisible[section.kind-1])continue;
                const bool selected=state_->highlightedKind==section.kind&&state_->highlightedSections.contains(section.index);
                const bool dotted=(section.kind==1&&(state_->projection==HullProjection::Profile||state_->projection==HullProjection::Plan))||
                    (section.kind==2&&(state_->projection==HullProjection::Body||state_->projection==HullProjection::Plan))||
                    (section.kind==3&&(state_->projection==HullProjection::Profile||state_->projection==HullProjection::Body));
                const bool comb=snapshot->showCurvature&&section.showCurvature&&!state_->fillVisible;
                if(dotted&&state_->gridVisible&&snapshot->overlay.present&&!comb)continue;
                painter.setPen(QPen(selected?QColor(160,67,34):dotted?QColor(150,160,157):sourceColor(section.color),selected?2:1,dotted?Qt::DotLine:Qt::SolidLine));
                for(int index=0;index<section.paths.size();++index){
                    const auto& path=section.paths[index];
                    if(!final&&!comb&&!dotted&&!selected&&!state_->entrance.active()){
                        // Never use a clipped screenshot: cache the full path in projected units,
                        // including geometry beyond the widget. Preserve dash and comb geometry.
                        const double pixelScale=state_->scale*dpr;
                        if(state_->sectionSource.lock()!=snapshot||state_->sectionProjection!=state_->projection||
                           !samePosition(state_->sectionCenter,state_->cameraCenter)||state_->sectionExtent!=state_->cameraExtent||
                           state_->sectionYaw!=state_->yaw||state_->sectionPitch!=state_->pitch||state_->sectionFocal!=state_->focalLength||
                           pixelScale>state_->sectionCap||pixelScale*4<state_->sectionCap){
                            state_->sectionLines.clear();state_->sectionSource=snapshot;state_->sectionProjection=state_->projection;
                            state_->sectionCenter=state_->cameraCenter;state_->sectionExtent=state_->cameraExtent;
                            state_->sectionYaw=state_->yaw;state_->sectionPitch=state_->pitch;state_->sectionFocal=state_->focalLength;
                            state_->sectionCap=pixelScale*2;
                        }
                        auto& cached=state_->sectionLines[reinterpret_cast<quintptr>(&path)];
                        if(!cached.initialized){
                            const auto sides=sectionSides(*snapshot,int(state_->projection),section.kind,path);
                            const auto build=[&](bool mirror){
                                QPolygonF projected;projected.reserve(path.size());
                                for(const auto& p:path)projected.append((project(mirror?mirrored(p):p)-state_->offset)/state_->scale);
                                return screenStroke(projected,.15/state_->sectionCap);
                            };
                            if(sides.first)cached.line=build(false);
                            if(sides.second)cached.mirror=build(true);
                            cached.initialized=true;
                        }
                        const auto scale=state_->scale;const auto origin=state_->offset;
                        const QTransform transform(scale,0,0,scale,origin.x(),origin.y());
                        if(!cached.line.isEmpty())painter.drawPolyline(transform.map(cached.line));
                        if(!cached.mirror.isEmpty())painter.drawPolyline(transform.map(cached.mirror));
                        continue;
                    }
                    auto sides=sectionSides(*snapshot,int(state_->projection),section.kind,path);
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
        };
        auto& layer=state_->entranceSections;
        const QSize pixels(qCeil(width()*dpr),qCeil(height()*dpr));
        // The wave advances underneath the stationary section lines. Rasterize
        // just those lines at the actual display resolution during the reveal.
        if(!final&&!preview&&state_->entrance.active()&&snapshot&&!snapshot->sections.isEmpty()&&
           !pixels.isEmpty()&&qint64(pixels.width())*pixels.height()<=4*1024*1024){
            if(layer.pixels.isNull()||layer.source.lock()!=snapshot||layer.camera!=state_->cameraRevision||
               layer.size!=size()||layer.pixels.devicePixelRatio()!=dpr||layer.visibility!=state_->sectionVisible||
               layer.highlightedKind!=state_->highlightedKind||layer.highlightedSections!=state_->highlightedSections||
               layer.grid!=state_->gridVisible||layer.fill!=state_->fillVisible){
                layer.pixels=QPixmap(pixels);layer.pixels.setDevicePixelRatio(dpr);layer.pixels.fill(Qt::transparent);
                if(!layer.pixels.isNull()){
                    QPainter sectionPainter(&layer.pixels);sectionPainter.setRenderHints(painter.renderHints());
                    drawSections(sectionPainter);
                }
                layer.source=snapshot;layer.camera=state_->cameraRevision;layer.size=size();layer.visibility=state_->sectionVisible;
                layer.highlightedKind=state_->highlightedKind;layer.highlightedSections=state_->highlightedSections;
                layer.grid=state_->gridVisible;layer.fill=state_->fillVisible;
            }
            if(layer.pixels.isNull())drawSections(painter);else painter.drawPixmap(QPoint(),layer.pixels);
        }else if(!final&&!preview&&state_->layerMatches(size(),dpr)&&
                 pixelPhase(painter.deviceTransform().map(QPointF{}))==state_->resizeLayer.phase){
            const auto& image=state_->resizeLayer.image.get();
            if(verifySections&&!image.isNull()){
                QImage reference(image.size(),image.format());reference.setDevicePixelRatio(dpr);reference.fill(Qt::transparent);
                QPainter referencePainter(&reference);referencePainter.setRenderHints(painter.renderHints());
                referencePainter.translate(state_->resizeLayer.phase/dpr);drawSections(referencePainter);referencePainter.end();*verifySections=(reference==image);
            }
            // Blit on an integer physical pixel even on fractional Windows scaling.
            if(image.isNull())drawSections(painter);else painter.drawImage(-state_->resizeLayer.phase/dpr,image);
        }else{
            if(!final)layer.pixels={};
            drawSections(painter);
        }
        if(!preview){
            state_->prepareCurvePaths(final);
            for(int i=0;i<data->curves.size();++i){const auto& c=data->curves[i];if(!state_->curveVisible(c))continue;
                const bool selected=!c.marker&&state_->kind==(c.flow?HullEntity::Flowline:HullEntity::Curve)&&state_->entities.contains(c.id);
                painter.save();painter.setTransform(state_->curvePathTransform,true);
                QPen curvePen(selected?QColor(160,67,34):c.color,selected?2:1);curvePen.setCosmetic(true);
                painter.setPen(curvePen);painter.setBrush(Qt::NoBrush);painter.drawPath(state_->curvePaths[i].path);painter.restore();
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
        // The annotation collision pass uses the same unmirrored screen edges and
        // points as the control net. Collect them once, in original model order.
        QVector<QLineF> labelEdges;
        QVector<QPointF> labelPoints;
        const bool collectObstacles=state_->controlNetVisible&&!preview&&snapshot&&snapshot->overlay.present&&
            (final||state_->entrance.textOpacity(false)>0);
        if(collectObstacles)labelEdges.reserve(data->edges.size());
        if(state_->controlNetVisible) {
        QVector<QLineF> edges,creases,mirrors;
        labelPoints.reserve(data->points.size());
        for(const auto& point:data->points)labelPoints.append(project(state_->controlPosition(state_->displayedPosition(point))));
        edges.reserve(data->edges.size());creases.reserve(data->edges.size()/3);
        const QRectF clip=QRectF(rect()).adjusted(-8,-8,8,8);
        for (const auto& edge:data->edges) {
            auto a=state_->displayedPosition(data->points[edge.ia]);
            auto b=state_->displayedPosition(data->points[edge.ib]);
            if(preview){auto span=std::max(1e-6,snapshot->maximum.x-snapshot->minimum.x);auto t=std::clamp(state_->entrance.previewFraction()*1.6-((a.x+b.x)*.5-snapshot->minimum.x)/span*.6,0.,1.);t=1-(1-t)*(1-t);if(t<=0)continue;b={a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t,a.z+(b.z-a.z)*t};}
            const auto appendLine=[&](QLineF line){
                if(collectObstacles)labelEdges.append(line);
                if(clip.intersects(QRectF(line.p1(),line.p2()).normalized().adjusted(-1,-1,1,1)))(edge.crease?creases:edges).append(line);
            };
            if(!preview&&!state_->splitBody())appendLine({labelPoints[edge.ia],labelPoints[edge.ib]});
            else for(const auto& span:state_->controlSegments(a,b))appendLine({project(span[0]),project(span[1])});
            if (edge.mirror && (a.y!=0 || b.y!=0)) {
                const QLineF mirror(project(mirrored(a)),project(mirrored(b)));
                if (clip.intersects(QRectF(mirror.p1(),mirror.p2()).normalized().adjusted(-1,-1,1,1)))
                    mirrors.append(mirror);
            }
        }
        painter.setPen(QPen(QColor(91,112,127,145),0.8,Qt::DashLine));
        painter.drawLines(mirrors);
        const auto hasVisibleLine=[](const QVector<QLineF>& lines){return std::any_of(lines.begin(),lines.end(),[](const auto& line){return line.length()>=1.;});};
        state_->paintedHull|=hasVisibleLine(edges)||hasVisibleLine(creases)||hasVisibleLine(mirrors);
        if(!state_->studio){painter.setPen(QPen(QColor(255,255,255,155),2.5));painter.drawLines(edges);painter.drawLines(creases);}
        painter.setPen(QPen(state_->studio?state_->preferred("edgecolor",QColor(102,133,125)):QColor(18,74,123),state_->studio?1.3:0.95));painter.drawLines(edges);
        painter.setPen(QPen(state_->studio?state_->preferred("creaseedgecolor",QColor(160,67,34)):QColor(13,39,64),1.3));painter.drawLines(creases);
        QVector<QPointF> points;
        QVector<QRectF> locked;
        points.reserve(data->points.size());
        for (const auto& point:data->points) {
            const auto pixel=labelPoints[&point-data->points.constData()];
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
            layout.begin(QRectF(rect()).adjusted(3,state_->studio?titleHeight+4:3,-3,-3),state_->frameTime,!final&&state_->animations);
            const bool showText=final||state_->entrance.textOpacity(false)>0;
            bool obstaclesReady=false;
            const auto prepareObstacles=[&]{
                if(obstaclesReady)return;
                obstaclesReady=true;
                const int r=snapshot->overlay.pointSize+3;
                for(auto at:std::as_const(labelPoints))layout.reserve(QRectF(at-QPointF(r,r),QSizeF(2*r+1,2*r+1)));
                for(const auto& edge:std::as_const(labelEdges))layout.segment(edge);
            };
            painter.setFont(annotationFont);QFontMetricsF metrics(annotationFont);QJsonArray evidence;
            auto annotation=[&](const Annotation& a){
                if(!showText)return;
                prepareObstacles();
                auto size=notation::size(a.text,annotationFont);auto placed=layout.place(a.key,a.text,a.anchor,a.preferred,size,a.kind);
                if(!placed.visible)return;
                const bool numeric=!a.text.isEmpty()&&(a.text[0].isDigit()||a.text[0]=='-'||a.text[0]=='+');
                painter.save();painter.setOpacity(final?1:state_->entrance.textOpacity(numeric));painter.setPen(a.color);
                if(a.symbol){painter.setBrush(Qt::white);painter.drawEllipse(a.anchor,4,4);painter.setBrush(Qt::black);painter.drawPie(QRectF(a.anchor-QPointF(4,4),QSizeF(8,8)),0,90*16);painter.drawPie(QRectF(a.anchor-QPointF(4,4),QSizeF(8,8)),180*16,90*16);}
                if(a.kind==LabelKind::Point){auto end=QPointF(std::clamp(a.anchor.x(),placed.clip.left(),placed.clip.right()),std::clamp(a.anchor.y(),placed.clip.top(),placed.clip.bottom()));if(QLineF(a.anchor,end).length()>=28)painter.drawLine(a.anchor,end);}
                auto clip=placed.clip;if(!final)clip=clip.intersected(QRectF(placed.position,QSizeF(size.width()*state_->entrance.textFraction(numeric),size.height())));
                painter.setClipRect(clip);notation::drawBaseline(painter,placed.position+QPointF(0,metrics.ascent()),a.text);painter.restore();
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
                if(showText)for(bool selected:{true,false})for(const auto& p:data->points){
                    if((p.name.isEmpty()&&!state_->coordinateAxis)||state_->selection.contains(p.id)!=selected)continue;
                    auto at=labelPoints[&p-data->points.constData()];if(!rect().contains(at.toPoint())||(!final&&!state_->entrance.pointVisible(at.x())))continue;
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
        state_->evidence.insert("show_control_curves",snapshot->showControlCurves);
        state_->evidence.insert("show_flowlines",snapshot->showFlowlines);
        state_->evidence.insert("show_markers",snapshot->showMarkers);
        state_->evidence.insert("auxiliary_path_builds",state_->curvePathBuilds);state_->evidence.insert("auxiliary_curves",data->curves.size());
    }
    if(!final){
        painter.save();painter.setClipRect(rect());painter.setRenderHint(QPainter::Antialiasing,true);
        for(const auto& hover:state_->hoverMotion)if(hover.alpha>.001){
            painter.setOpacity(hover.alpha);painter.setPen(QPen(QColor("#008f80"),hover.kind==HullEntity::Point?2.2:3,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));
            if(hover.kind==HullEntity::Face)painter.setBrush(QColor(0,143,128,30));else painter.setBrush(Qt::NoBrush);painter.drawPath(state_->interactionShape(hover.kind,hover.id));
        }
        const double pulse=std::clamp(1-double(interactionTime()-state_->emphasizedAt)/240.,0.,1.);
        if(pulse>0){
            painter.setOpacity(pulse);painter.setPen(QPen(QColor("#e49335"),2+2*pulse,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));painter.setBrush(Qt::NoBrush);
            for(auto id:state_->emphasized)painter.drawPath(state_->interactionShape(state_->emphasizedKind,id));
        }
        painter.restore();
    }
    painter.setRenderHint(QPainter::Antialiasing,false);
    if(!final&&backgroundData&&backgroundData->visible&&state_->backgroundMode==BackgroundMode::Frame){
        painter.setPen(QPen(Qt::yellow,3,Qt::DashLine));painter.setBrush(Qt::NoBrush);painter.drawRect(backgroundData->targetRect(project({}),state_->scale));
    }
    if(state_->studio)painter.fillRect(QRect(0,0,width(),titleHeight),QColor(224,236,232));
    painter.setRenderHint(QPainter::TextAntialiasing,true);painter.setFont(titleFont);
    painter.setPen(state_->studio?QColor(0,61,69):QColor(44,57,68));
    const auto title=projectionLabel();
    painter.drawText(QRect(12,0,width()-24,titleHeight),Qt::AlignLeft|Qt::AlignVCenter,title);
    state_->evidence.insert("projection_title_text",title);
    state_->evidence.insert("projection_title_text_width",QFontMetricsF(titleFont).horizontalAdvance(title));
    state_->evidence.insert("projection_title_available_width",width()-24);
    state_->evidence.insert("projection_title_font",titleFont.family());
    state_->evidence.insert("projection_title_point_size",titleFont.pointSizeF());
    state_->evidence.insert("projection_title_height",titleHeight);
    state_->evidence.insert("projection_title_antialias",painter.testRenderHint(QPainter::TextAntialiasing));
    if(state_->fillVisible&&state_->shadeMode!=HullShade::Flat&&data&&data->analysis){
        QString text;
        if(state_->shadeMode==HullShade::Gaussian)text=QString(uiText("角亏（°）：%1 至 %2；灰色：边界或无法评估")).arg(data->analysis->minimum,0,'g',4).arg(data->analysis->maximum,0,'g',4);
        else if(state_->shadeMode==HullShade::Developable)text=uiText("绿：角亏接近零；红：非零；灰：无法评估");
        else text=uiText("斑马纹 · 检查曲面法向连续性");
        painter.save();painter.setFont(QApplication::font());const auto box=painter.fontMetrics().boundingRect(QRect(8,0,qMax(1,width()-16),100),Qt::TextWordWrap,text);
        QRect area(4,height()-box.height()-10,width()-8,box.height()+6);painter.fillRect(area,QColor(246,249,247,235));painter.setPen(QColor(30,53,50));painter.drawText(area.adjusted(4,3,-4,-3),Qt::TextWordWrap,text);painter.restore();
    }
    painter.setPen(QColor(215,222,228));painter.setBrush(Qt::NoBrush);
    painter.drawRect(rect().adjusted(0,0,-1,-1));
}

void HullView::paintEvent(QPaintEvent*) {
    QElapsedTimer timer;timer.start();state_->frameTime=state_->clock.elapsed();state_->entrance.beginFrame(state_->frameTime,width());
    state_->paintedHull=false;
    QPainter painter(this);draw(painter,devicePixelRatioF(),false);state_->entrance.endFrame();
    if(!state_->entrance.active())state_->entranceSections.pixels={};
    if((state_->entrance.active()||state_->labels.animating())&&!state_->entrance.frozen()){
        if(!state_->timer.isActive())state_->timer.start();
    }else state_->timer.stop();
    lastPaintMs=timer.nsecsElapsed()/1e6;++paintCount;
    if(state_->snapshot&&(!state_->entrance.preview()||state_->entrance.lineFraction()>0)&&onHullFrame)onHullFrame();
    if(state_->paintedHull&&onHullVisibleFrame)onHullVisibleFrame();
}
void HullView::resizeEvent(QResizeEvent* event) {
    if (state_->fitted) fitToViewport(true);
    else if (event->oldSize().isValid())
        state_->offset+=QPointF((event->size().width()-event->oldSize().width())/2.0,
                               (event->size().height()-event->oldSize().height())/2.0);
    ++state_->cameraRevision;
    if(isVisible()){
        const auto dpr=devicePixelRatioF();
        state_->scheduleSections(size(),dpr,pixelPhase(QPointF(mapTo(window(),QPoint()))*dpr));
    }
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
    state_->sampleCamera(interactionTime());state_->cameraMotion.active=false;updateInteractionHover(0);
    finishPresentation();
    cancelGesture();
    state_->press=state_->last=event->position();
    if(state_->pointPlacement){
        if(event->button()==Qt::RightButton){setPointPlacement(false);state_->lastRightDragged=true;state_->moved=true;if(onPointPlacementEnded)onPointPlacementEnded();event->accept();return;}
        if(!state_->editingEnabled){event->accept();return;}
        const auto p=flowSource(event->position());fs::Vec3 position{};
        switch(state_->projection){case HullProjection::Profile:position={p.x(),0,p.y()};break;case HullProjection::Plan:position={p.x(),p.y(),0};break;case HullProjection::Body:position={state_->snapshot->maximum.x,p.x(),p.y()};break;default:event->accept();return;}
        // Placement stays active across coordinate confirmation. Esc, right
        // click or the explicit state action ends the mode.
        if(onPointCreationRequested)onPointCreationRequested(position);event->accept();return;
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
    if(event->button()==Qt::LeftButton && state_->kind!=HullEntity::Point) {
        const auto id=entityAt(event->position());
        if(!id&&state_->projection==HullProjection::Perspective&&event->modifiers()==Qt::NoModifier){
            state_->gesture=State::Gesture::Rotate;
            setCursor(Qt::SizeAllCursor);
            event->accept();
            return;
        }
        const bool additive=event->modifiers().testFlag(Qt::ControlModifier);
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
        const bool additive=event->modifiers().testFlag(Qt::ControlModifier);
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
        if (state_->projection==HullProjection::Perspective && !id && event->modifiers()==Qt::NoModifier) {
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
        else {
            const auto id=entityAt(event->position());updateInteractionHover(id);
            if(id)setCursor(Qt::PointingHandCursor);else unsetCursor();
        }
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
    const auto key=event->key();
    if(key==Qt::Key_Escape){
        const bool cancelling=state_->gesture!=State::Gesture::None||state_->backgroundMode!=BackgroundMode::None||state_->flowPlacement||state_->pointPlacement;
        setBackgroundMode(BackgroundMode::None);setFlowPlacement(false);setPointPlacement(false);
        if(onFlowPlacementEnded)onFlowPlacementEnded();
        if(onPointPlacementEnded)onPointPlacementEnded();
        if(!cancelling&&onViewAction)onViewAction("clear_selection");
        event->accept();
    }else QWidget::keyPressEvent(event);
}
void HullView::focusOutEvent(QFocusEvent* event) {cancelGesture();QWidget::focusOutEvent(event);}
void HullView::leaveEvent(QEvent* event){updateInteractionHover(0);QWidget::leaveEvent(event);}
