#include "app/ui_language.h"
#include "app/math_notation.h"
#include "hull_view.h"
#include "station_plane_editor.h"
#include "text_style.h"
#include "underwater_color.h"
#include "background_geometry.h"
#include "body_projection.h"
#include "curvature_comb.h"
#include "curve_edit_motion.h"
#include "auxiliary_geometry.h"
#include <QScopedValueRollback>
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
#include <QToolButton>
#include <QWheelEvent>
#include <QAbstractAnimation>
#ifdef Q_OS_WIN
#include <QtCore/qt_windows.h>
#endif
#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <optional>
#include <unordered_map>
#include <vector>
#include <queue>
#include <tuple>

namespace {
struct DisplayModeEntry { const char* key; const char* text; const char* icon; HullShade shade=HullShade::Flat; };
constexpr DisplayModeEntry displayModes[]{
    {"wireframe","线框","Wireframe"}, {"fill","填色","FillColor"},
    {"inspect_faces","控制面分布","InspectControlFaces"}, {"inspect_lines","线型辨识","InspectLineTypes"},
    {"shade_1","高斯曲率","GaussianCurvature",HullShade::Gaussian},
    {"shade_2","可展性检查","Developability",HullShade::Developable},
    {"shade_3","斑马纹","Zebra",HullShade::Zebra},
};
const DisplayModeEntry* displayModeEntry(const QString& key){
    for(const auto& entry:displayModes)if(key==QLatin1String(entry.key))return &entry;
    return nullptr;
}
// Rasterize the union of round-ended segments into an 8-bit coverage mask. Work
// across the narrow stroke, even for a long diagonal. All positions are physical pixels.
static void shellStroke(QImage& mask,QPointF a,QPointF b,double radius){
    const double margin=radius+.5;const bool steep=std::abs(b.y()-a.y())>std::abs(b.x()-a.x());
    if(steep){a={a.y(),a.x()};b={b.y(),b.x()};}if(a.x()>b.x())std::swap(a,b);
    const int width=steep?mask.height():mask.width(),height=steep?mask.width():mask.height();
    if(b.x()+margin<0||a.x()-margin>width||std::min(a.y(),b.y())-margin>height||std::max(a.y(),b.y())+margin<0)return;
    const double dx=b.x()-a.x(),dy=b.y()-a.y(),slope=dx>0?dy/dx:0,band=margin*(1+std::abs(slope));
    const double length2=dx*dx+dy*dy,inv=length2>0?1./length2:0;
    const int lo=int(std::clamp(std::floor(a.x()-margin),0.,double(width))),hi=int(std::clamp(std::ceil(b.x()+margin),0.,double(width)));
    auto* base=mask.bits();const auto stride=mask.bytesPerLine();
    for(int x=lo;x<hi;++x){const double rx=x+.5-a.x(),cy=a.y()+std::clamp(rx,0.,dx)*slope;
        const int y0=int(std::clamp(std::floor(cy-band),0.,double(height))),y1=int(std::clamp(std::ceil(cy+band),0.,double(height)));
        for(int y=y0;y<y1;++y){const double ry=y+.5-a.y(),t=std::clamp((rx*dx+ry*dy)*inv,0.,1.);
            const double ex=rx-t*dx,ey=ry-t*dy,dd=ex*ex+ey*ey;if(dd>=margin*margin)continue;
            auto& v=base[steep?x*stride+y:y*stride+x];v=std::max(v,uchar(std::min(1.,margin-std::sqrt(dd))*255+.5));}}
}
const QColor controlSelectionInk("#1455B0");
const QColor controlSelectionKeyline("#F6FAFF");
const QColor controlFaceSelectionFill(78,146,239,46);
constexpr qreal controlEdgeSelectionWidth=2.6;
constexpr qreal controlFaceSelectionWidth=1.8;
bool usesControlSelectionInk(HullEntity kind){return kind==HullEntity::Edge||kind==HullEntity::Face;}
QPen controlSelectionPen(const QColor& color,qreal width){
    return {color,width,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin};
}
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
struct MotionKnot {
    QPointF numerator;
    double weight=1;
    QPointF pixel()const{return numerator/qMax(weight,1e-6);}
};
struct MotionVertex {
    MotionKnot from,to;
    QPointF pixel(double t)const{
        return (from.numerator+(to.numerator-from.numerator)*t)/qMax(from.weight+(to.weight-from.weight)*t,1e-6);
    }
};
QVector<MotionVertex> reduceMotionStroke(const QVector<MotionVertex>& vertices,double tolerance,bool perspective){
    if(vertices.size()<4)return vertices;
    struct Sample {QPointF from,to;double ratio=1;};
    QVector<Sample> samples;samples.reserve(vertices.size());
    for(const auto& vertex:vertices){
        // Do not reduce projected curves that enter the camera's clipped range.
        if(vertex.from.weight<=1e-6||vertex.to.weight<=1e-6)return vertices;
        samples.append({vertex.from.pixel(),vertex.to.pixel(),vertex.to.weight/vertex.from.weight});
    }
    const auto squared=[](QPointF p){return QPointF::dotProduct(p,p);};
    const auto weightDifference=[](double a,double b){
        return std::min(1.,std::abs(a-b)*.25/(std::min(1.,a)*std::min(1.,b)));
    };
    QVector<MotionVertex> result;result.append(vertices.front());
    const auto append=[&](auto&& self,qsizetype first,qsizetype last)->void{
        const auto& a=samples[first];const auto& b=samples[last];
        const auto fromSpan=b.from-a.from,toSpan=b.to-a.to;
        const double extent=squared(fromSpan)+squared(toSpan);
        const double leftTravel=perspective?std::sqrt(squared(a.to-a.from)):0.;
        const double rightTravel=perspective?std::sqrt(squared(b.to-b.from)):0.;
        double worst=1.;qsizetype split=-1;
        for(qsizetype i=first+1;i<last;++i){
            const auto& p=samples[i];
            const auto from=p.from-a.from,to=p.to-a.to;
            const auto along=extent>0?std::clamp((QPointF::dotProduct(from,fromSpan)+QPointF::dotProduct(to,toSpan))/extent,0.,1.):0.;
            // The same chord parameter at both endpoints bounds the entire
            // affine motion. For perspective add a bound for the difference
            // between the homogeneous interpolation weights.
            const double extra=perspective
                ?(1.-along)*leftTravel*weightDifference(p.ratio,a.ratio)+along*rightTravel*weightDifference(p.ratio,b.ratio)
                :0.;
            const double remaining=tolerance-extra;
            const double error=std::max(squared(from-fromSpan*along),squared(to-toSpan*along));
            const double score=remaining>0?error/(remaining*remaining):2.+extra/tolerance;
            if(score>worst){worst=score;split=i;}
        }
        if(split>=0){self(self,first,split);self(self,split,last);}
        else result.append(vertices[last]);
    };
    for(qsizetype start=0;start<vertices.size()-1;start+=96)
        append(append,start,std::min(start+96,vertices.size()-1));
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
struct DisplayEdge { fs::EntityId a, b; bool crease, mirror; fs::EntityId id; qsizetype ia,ib;int faceCount; };
struct DisplayFace {fs::EntityId id;QVector<int> points;bool mirror;};
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
    mutable std::optional<QVector<DisplayFace>> emptyCells;
    QVector<fs::presentation::Segment> surfaceCreases, mirroredSurfaceCreases;
    QHash<fs::EntityId, int> pointIndices;

    mutable QHash<quint64,QVector<fs::presentation::CombPoint>> combs;
    mutable QHash<fs::EntityId,QVector<fs::presentation::CombPoint>> curveCombs;
    mutable int curvatureBuilds=0,curvatureErrors=0;
    QVector<DisplayCurve> curves;
    bool translucent=false;
    mutable std::shared_ptr<const SurfaceAnalysis> analysis;
    mutable QVector<SurfaceShadeTriangle> shading;
};
// Derive display-only cells from connected model edges. Never close a gap
// between points just because they happen to coincide in the current view.
const QVector<DisplayFace>& emptyControlCells(const DisplayData& data){
    if(data.emptyCells)return *data.emptyCells;
    auto& result=data.emptyCells.emplace();
    using Pair=QPair<int,int>;
    const auto key=[](int a,int b)->Pair{return {std::min(a,b),std::max(a,b)};};
    QHash<Pair,const DisplayEdge*> edges;
    QHash<int,QVector<int>> neighbours;
    for(const auto& e:data.edges){
        const int a=e.ia,b=e.ib;
        if(a==b || edges.contains(key(a,b)))continue;
        edges.insert(key(a,b),&e);
        if(e.faceCount<=1){neighbours[a].append(b);neighbours[b].append(a);}
    }
    if(neighbours.isEmpty())return result;
    const auto position=[&](int i){return data.points[i].position;};
    const auto center=[&](const QVector<int>& ring){
        fs::Vec3 c{};
        for(int i:ring){const auto p=position(i);c.x+=p.x;c.y+=p.y;c.z+=p.z;}
        return fs::Vec3{c.x/ring.size(),c.y/ring.size(),c.z/ring.size()};
    };
    QHash<Pair,fs::Vec3> occupiedSide;
    for(const auto& face:data.faces){
        const auto& ring=face.points;
        std::optional<fs::Vec3> c;
        for(int i=0;i<ring.size();++i){
            const auto pair=key(ring[i],ring[(i+1)%ring.size()]);
            const auto* edge=edges.value(pair);
            if(edge && edge->faceCount==1){
                if(!c)c=center(ring);
                occupiedSide.insert(pair,*c);
            }
        }
    }
    const auto consider=[&](const QVector<int>& ring){
        const auto c=center(ring);
        fs::Vec3 normal{};
        bool canMirror=true,hasSupport=false;
        for(int i=0;i<ring.size();++i){
            const int a=ring[i],b=ring[(i+1)%ring.size()];
            const auto pair=key(a,b);
            const auto* edge=edges.value(pair);
            const auto along=subtract(position(b),position(a));
            const auto inside=cross(along,subtract(c,position(a)));
            if(dot(inside,inside)==0)return;
            if(i==0)normal=inside;
            else if(dot(normal,inside)<=0)return; // Folded or crossing boundary.
            if(edge->faceCount==1){
                hasSupport=true;canMirror &= edge->mirror;
                const auto other=occupiedSide.constFind(pair);
                if(other==occupiedSide.cend() ||
                   dot(inside,cross(along,subtract(*other,position(a))))>=0)return;
            }
        }
        result.append({0,ring,hasSupport && canMirror});
    };
    QSet<int> visited;
    for(auto it=neighbours.cbegin();it!=neighbours.cend();++it){
        const int start=it.key();
        if(visited.contains(start))continue;
        QVector<int> component{start};visited.insert(start);
        bool simple=true;
        for(int i=0;i<component.size();++i){
            const auto& adjacent=neighbours.constFind(component[i]).value();
            simple &= adjacent.size()==2;
            for(int v:adjacent)if(!visited.contains(v)){visited.insert(v);component.append(v);}
        }
        if(simple){
            QVector<int> ring{start};
            int prev=start,next=it.value().front();
            while(next!=start){
                ring.append(next);
                const auto& adjacent=neighbours.constFind(next).value();
                const int after=adjacent[0]==prev?adjacent[1]:adjacent[0];
                prev=next;next=after;
            }
            if(ring.size()>=3)consider(ring);
            continue;
        }
        // Adjacent empty grid cells share wire edges. Only accept elementary
        // triangles and quads here; a wider circuit could enclose several cells.
        for(int a:component)for(int b:neighbours.constFind(a).value()){
            if(b<=a)continue;
            for(int c:neighbours.constFind(b).value()){
                if(c<=a)continue;
                if(const auto* ac=edges.value(key(a,c))){
                    if(c>b && ac->faceCount<=1)consider({a,b,c});
                    continue;
                }
                for(int d:neighbours.constFind(c).value()){
                    if(d<=b || edges.contains(key(b,d)))continue;
                    const auto* ad=edges.value(key(a,d));
                    if(ad && ad->faceCount<=1)consider({a,b,c,d});
                }
            }
        }
    }
    return result;
}
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
    if (const auto& topology = snapshot.surfaceTopology) {
        const double split = fs::presentation::splitLocation(snapshot);
        for (const auto& crease : topology->creases) {
            bool visible = false, symmetric = false;
            for (const auto id : crease.layers) {
                const auto* layer = layers.value(id);
                if (!visibleLayer(layer)) continue;
                visible = true;
                symmetric |= layer->symmetric;
            }
            if (!visible) continue;
            const auto a = topology->points[crease.points[0]], b = topology->points[crease.points[1]];
            if (splitBody) {
                result->surfaceCreases.append(fs::presentation::bodySegments(a,b,split));
            } else {
                result->surfaceCreases.append(fs::presentation::Segment{a,b});
                if (bothSides && symmetric && (a.y != 0 || b.y != 0))
                    result->mirroredSurfaceCreases.append(fs::presentation::Segment{mirrored(a),mirrored(b)});
            }
        }
    }
    QSet<fs::EntityId> owned,visible;
    QHash<QPair<fs::EntityId,fs::EntityId>,int> faceCounts;
    QSet<QPair<fs::EntityId,fs::EntityId>> visibleEdges,symmetricEdges;
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
                ++faceCounts[key];
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
        if (faceCounts.contains(edgeKey(edge.a,edge.b)) && !visibleEdges.contains(edgeKey(edge.a,edge.b))) continue;
        const bool mirror=bothSides&&symmetricEdges.contains(edgeKey(edge.a,edge.b));
        result->edges.append({edge.a,edge.b,edge.crease,mirror,edge.id,result->pointIndices.value(edge.a),result->pointIndices.value(edge.b),faceCounts.value(edgeKey(edge.a,edge.b))});
        if (mirror) {
            include(mirrored(result->points[result->pointIndices.value(edge.a)].position));
            include(mirrored(result->points[result->pointIndices.value(edge.b)].position));
        }
    }
    for(const auto& face:snapshot.faces) {
        const auto* layer=layers.value(face.layer,nullptr);if(!visibleLayer(layer) || hidden.contains(layer->id))continue;
        DisplayFace item{face.id,{},bothSides&&layer->symmetric};
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
    HullEntity kind=HullEntity::Point, globalKind=HullEntity::Point;
    QSet<fs::EntityId> entities, entityPoints;
    const QSet<fs::EntityId>& transformPoints()const{return kind==HullEntity::Point?selection:entityPoints;}
    QString plateDiagnosticDocument;
    quint64 plateDiagnosticRevision=0;
    QVector<QVector<fs::Vec3>> plateDiagnosticFaces;
    enum class Inspection { Off, Faces, Lines } inspection=Inspection::Off;
    bool studio=false,fillVisible=true,controlNetVisible=true;
    mutable bool paintedHull=false;
    struct DotStamp {QPixmap image;qreal dpr=0;QColor color;};
    mutable DotStamp haloStamp,selectedStamp;
    QSet<fs::EntityId> hiddenControlLayers,visibilityChanges;
    std::shared_ptr<const DisplayData> previousVisibilityDisplay;
    QJsonObject preferenceColors;
    QColor preferred(const QString& key,QColor fallback)const{
        return preferenceColors.contains(key)?QColor::fromRgb(preferenceColors[key].toInt()):fallback;
    }
    std::optional<bool> gridOverride,hydroOverride;
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
    QSet<fs::EntityId> undoingPlacements;
    bool pointPlacement=false;QVector<fs::Vec3> pendingPlacements;
    StationPlaneEditor* stationEditor=nullptr;
    struct CurvePath {QPainterPath path,hit;quint64 hitRevision=~quint64(0);};
    mutable QVector<CurvePath> curvePaths;
    mutable QTransform curvePathTransform;
    mutable QPointF curvePathOffset;
    mutable double curvePathScale=1;
    mutable quint64 curvePathRevision=std::numeric_limits<quint64>::max();
    mutable int curvePathBuilds=0;
    std::shared_ptr<fs::curveEdit::Motion> curveEditMotion;
    QMetaObject::Connection curveEditFrameConnection;
    struct AnimatedSectionStroke {
        fs::curveEdit::Key key;
        QColor color;
        double fromOpacity,toOpacity;
        QVector<MotionVertex> vertices;
    };
    struct AnimatedSectionProjection {
        quint64 camera=~quint64(0);
        qreal dpr=0;
        bool grid=false;
        std::array<bool,4> visible{};
        QVector<AnimatedSectionStroke> strokes;
        QVector<std::pair<fs::curveEdit::Key,const fs::curveEdit::Motion::Track*>> exact;
    };
    mutable AnimatedSectionProjection animatedSections;
    struct AnimatedOverlay {
        std::weak_ptr<const fs::ModelSnapshot> model;
        quint64 camera=~quint64(0);
        bool grid=false,hydro=false;
        std::array<bool,4> visible{};
        QFont font;
        QString language;
        fs::presentation::ViewOverlay geometry;
    };
    mutable AnimatedOverlay animatedOverlay;
    double scale=1;
    double orbitFitScale=1;
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
    // Keep the renderer with the camera. An edit changes vertices, not the GL
    // context, shaders or framebuffer; analysis attributes belong to each mesh.
    mutable std::unique_ptr<HullGpu> gpu;
    mutable std::weak_ptr<const DisplayData> gpuMesh;
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
        if(kind==HullEntity::Point){if(undoingPlacements.contains(id))return path;const auto index=display->pointIndices.constFind(id);if(index!=display->pointIndices.cend())path.addEllipse(projectPoint(controlPosition(displayedPosition(display->points[*index]))).pixel,6,6);}
        else if(kind==HullEntity::Edge){for(const auto& edge:display->edges)if(edge.id==id){for(const auto& span:selectableEdgeSpans(edge)){path.moveTo(projectPoint(span[0]).pixel);path.lineTo(projectPoint(span[1]).pixel);}break;}}
        else if(kind==HullEntity::Face){for(const auto& face:display->faces)if(face.id==id){for(const auto& polygon:faceParts(face)){QPolygonF pixels;for(auto point:polygon)pixels.append(projectPoint(point).pixel);path.addPolygon(pixels);path.closeSubpath();}break;}}
        else {prepareCurvePaths();for(int i=0;i<display->curves.size();++i)if(display->curves[i].id==id&&display->curves[i].flow==(kind==HullEntity::Flowline)){path=curvePathTransform.map(curvePaths[i].path);break;}}
        return path;
    }
    bool fitted=true;
    double yaw=-30*Pi/180,pitch=25*Pi/180;
    enum class Gesture { None, Pan, PendingRotate, Rotate, Move, Box, Stroke, BlockedMove, Background } gesture=Gesture::None;
    BackgroundMode backgroundMode=BackgroundMode::None;
    fs::BackgroundImage backgroundStart,backgroundPreview;
    bool backgroundPreviewing=false;int backgroundSide=0;
    QPointF press,last;
    QVector<fs::PointMove> dragStarts;
    std::shared_ptr<const QVector<fs::Section>> previewSections;
    DragSections compareSections;DragShell compareShell;
    std::optional<QVector<fs::presentation::Segment>> releasedCreases;
    struct ShellPaintCache {
        DragShell source; quint64 camera=0; QSize size; qreal dpr=0; QPointF phase;
        QImage context, current, original; QSet<QPair<fs::EntityId,fs::EntityId>> edges;
    } shellPaint;

    fs::Vec3 dragDelta;
    bool precise=false,soft=false,fromHandle=false;int axis=0,hoverAxis=0;
    QString digits;double radius=0;fs::Vec3 pivot;QHash<fs::EntityId,double> weights;
    QPointF axisVector(int a,fs::Vec3 at) const {
        const double h=std::max(1e-8,cameraExtent*1e-6);auto p=controlPosition(at),q=p;
        if(a==1)q.x+=h;else if(a==2)q.y+=(splitBody() && at.x<=fs::presentation::splitLocation(*snapshot))?-h:h;else q.z+=h;
        return (projectPoint(q).pixel-projectPoint(p).pixel)/h;
    }
    QHash<fs::EntityId,fs::Vec3> pendingPointPositions;
    double dragBodySign=1;
    QSet<fs::EntityId> boxBase,boxPreview;
    QPolygonF stroke;bool strokeAdditive=false;
    struct ProjectedFace {fs::EntityId id;QPolygonF polygon;QRectF bounds;double depth;};
    QVector<ProjectedFace> strokeFaces;
    QVector<QPair<fs::EntityId,QLineF>> strokeEdges;
    QVector<QVector<fs::Vec3>> faceParts(const DisplayFace& face)const{
        QVector<fs::Vec3> pts;bool offCenter=false;
        for(auto i:face.points){auto p=displayedPosition(display->points[i]);pts.append(p);offCenter|=p.y!=0;}
        auto parts=controlPolygons(pts);
        if(face.mirror&&offCenter&&projection!=HullProjection::Profile){for(auto& p:pts)p=mirrored(p);parts.append(pts);}
        return parts;
    }
    void prepareStroke(){
        if(!display)return;
        if(kind==HullEntity::Edge){for(const auto& edge:display->edges)for(const auto& span:selectableEdgeSpans(edge))
            strokeEdges.append({edge.id,QLineF(projectPoint(span[0]).pixel,projectPoint(span[1]).pixel)});}
        else for(const auto& face:display->faces)for(const auto& part:faceParts(face)){
            QPolygonF poly;double depth=0;for(auto p:part){auto q=projectPoint(p);poly.append(q.pixel);depth+=q.depth;}
            if(poly.size()>=3)strokeFaces.append({face.id,poly,poly.boundingRect(),depth/poly.size()});
        }
    }
    void extendStroke(QPointF pixel){
        QLineF line(stroke.last(),pixel);if(line.isNull())return;stroke.append(pixel);
        const auto box=QRectF(line.p1(),line.p2()).normalized().adjusted(-2,-2,2,2);
        auto nearSegment=[](QPointF p,QLineF l){auto d=l.p2()-l.p1();double n=QPointF::dotProduct(d,d);
            auto t=n?std::clamp(QPointF::dotProduct(p-l.p1(),d)/n,0.,1.):0.;return QLineF(p,l.pointAt(t)).length()<=2.;};
        for(const auto& [id,edge]:strokeEdges){if(!box.intersects(QRectF(edge.p1(),edge.p2()).normalized().adjusted(-.01,-.01,.01,.01)))continue;
            if(line.intersects(edge,nullptr)==QLineF::BoundedIntersection||nearSegment(edge.p1(),line)||nearSegment(edge.p2(),line)||nearSegment(line.p1(),edge)||nearSegment(line.p2(),edge))boxPreview.insert(id);
        }
        // Split at actual polygon crossings, including narrow faces between delivered mouse events.
        // At each interval pick the front face by the same depth rule as a single click.
        QVector<double> cuts{0,1};QVector<const ProjectedFace*> candidates;
        const auto d=line.p2()-line.p1();const double n=QPointF::dotProduct(d,d);
        for(const auto& face:strokeFaces){if(!box.intersects(face.bounds))continue;candidates.append(&face);
            for(int i=0;i<face.polygon.size();++i){QPointF x;if(line.intersects({face.polygon[i],face.polygon[(i+1)%face.polygon.size()]},&x)==QLineF::BoundedIntersection)
                cuts.append(std::clamp(QPointF::dotProduct(x-line.p1(),d)/n,0.,1.));}}
        std::sort(cuts.begin(),cuts.end());for(int i=1;i<cuts.size();++i){if(cuts[i]<=cuts[i-1])continue;
            auto p=line.pointAt((cuts[i]+cuts[i-1])*.5);const ProjectedFace* front=nullptr;
            for(auto* f:candidates)if((!front||f->depth<front->depth)&&f->bounds.contains(p)&&f->polygon.containsPoint(p,Qt::OddEvenFill))front=f;
            if(front)boxPreview.insert(front->id);
        }
    }
    QString blockedReason;
    bool moved=false,lastRightDragged=false;
    bool contextMenuOnRelease=false;
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
    QVector<fs::presentation::Segment> selectableEdgeSpans(const DisplayEdge &edge)const{
        const auto a=displayedPosition(display->points[edge.ia]),b=displayedPosition(display->points[edge.ib]);
        auto spans=controlSegments(a,b);
        if(edge.mirror && (a.y!=0 || b.y!=0))spans.append(fs::presentation::Segment{mirrored(a),mirrored(b)});
        return spans;
    }
    QVector<QVector<fs::Vec3>> controlPolygons(const QVector<fs::Vec3>& p)const{
        return splitBody()?fs::presentation::bodyPolygons(p,fs::presentation::splitLocation(*snapshot)):QVector<QVector<fs::Vec3>>{p};
    }
    struct Projected { QPointF pixel;double depth,reciprocalW; };
    struct FacePatch { QPolygonF polygon; QRectF bounds; double depth=0; bool empty=false; };
    void projectInspectionPoints(bool final,QVector<Projected>& pixels,QVector<Projected>& reflected)const{
        if(!splitBody()){
            pixels.reserve(display->points.size());
            const bool mirror=snapshot->bothSides && projection!=HullProjection::Profile;
            if(mirror)reflected.reserve(display->points.size());
            for(const auto& point:display->points){
                const auto p=displayedPosition(point,final);
                pixels.append(projectPoint(p));
                if(mirror)reflected.append(projectPoint(mirrored(p)));
            }
        }
    }
    QVector<FacePatch> projectFacePatches(bool final)const{
        QVector<Projected> pixels,reflected;
        projectInspectionPoints(final,pixels,reflected);
        QVector<FacePatch> patches;
        patches.reserve(display->faces.size());
        auto collect=[&](const auto& vertices,auto projectVertex,bool empty){
            if(vertices.size()<3)return;
            FacePatch patch;
            patch.empty=empty;
            patch.polygon.reserve(vertices.size());
            for(const auto& vertex:vertices){
                const auto projected=projectVertex(vertex);
                patch.polygon.append(projected.pixel);
                patch.depth+=projected.depth;
            }
            patch.depth/=vertices.size();
            patch.bounds=patch.polygon.boundingRect().adjusted(-2,-2,2,2);
            patches.append(std::move(patch));
        };
        auto appendFace=[&](const DisplayFace& face,bool empty){
            if(splitBody()){
                for(const auto& part:faceParts(face))
                    collect(part,[&](auto p){return projectPoint(p);},empty);
            }else{
                collect(face.points,[&](int i){return pixels[i];},empty);
                if(face.mirror && !reflected.isEmpty())
                    collect(face.points,[&](int i){return reflected[i];},empty);
            }
        };
        for(const auto& cell:emptyControlCells(*display))appendFace(cell,true);
        for(const auto& face:display->faces)appendFace(face,false);
        std::stable_sort(patches.begin(),patches.end(),[](const auto& a,const auto& b){
            return a.depth>b.depth;
        });
        return patches;
    }
    struct FaceTileCache {
        std::weak_ptr<const DisplayData> display;
        quint64 camera=0;
        qreal dpr=0;
        QPointF origin;
        QVector<FacePatch> patches;
        QHash<QPoint,QPixmap> tiles;
    };
    mutable FaceTileCache faceTiles;
    mutable quint64 faceProjectionBuilds=0;
    static QColor faceFill(){return QColor(181,215,221);}
    static QColor faceStroke(){return QColor(53,86,100);}
    static void paintFacePatch(QPainter& painter,const FacePatch& patch){
        painter.setBrush(patch.empty?QColor(Qt::white):faceFill());
        painter.drawPolygon(patch.polygon);
    }
    void drawFaceTiles(QPainter& painter,QRect viewport,qreal dpr)const{
        auto& cache=faceTiles;
        if(cache.display.lock()!=display || cache.camera!=cameraRevision || cache.dpr!=dpr){
            cache={display,cameraRevision,dpr,offset,projectFacePatches(false),{}};
            ++faceProjectionBuilds;
        }
        // Work in device pixels so neighbouring tiles meet even at fractional DPI.
        constexpr int side=256;
        const QPoint shift=((offset-cache.origin)*dpr).toPoint();
        const int left=qFloor(-double(shift.x())/side),top=qFloor(-double(shift.y())/side);
        const int right=qCeil((viewport.width()*dpr-shift.x())/side);
        const int bottom=qCeil((viewport.height()*dpr-shift.y())/side);
        const QRect retained(left-1,top-1,right-left+2,bottom-top+2);
        cache.tiles.removeIf([&](QHash<QPoint,QPixmap>::iterator it){return !retained.contains(it.key());});
        for(int y=top;y<bottom;++y)for(int x=left;x<right;++x){
            const QPoint tile(x,y);
            const QPointF origin=QPointF(tile*side)/dpr;
            auto found=cache.tiles.find(tile);
            if(found==cache.tiles.end()){
                QPixmap image(side,side);image.setDevicePixelRatio(dpr);image.fill(Qt::transparent);
                QPainter target(&image);target.setRenderHint(QPainter::Antialiasing);
                target.setPen(QPen(faceStroke(),1.5,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));
                target.translate(-origin);
                const QRectF bounds(origin,QSizeF(side/dpr,side/dpr));
                for(const auto& patch:std::as_const(cache.patches))
                    if(bounds.intersects(patch.bounds))paintFacePatch(target,patch);
                target.end();found=cache.tiles.insert(tile,std::move(image));
            }
            painter.drawPixmap(origin+QPointF(shift)/dpr,found.value());
        }
    }
    struct InspectionLayer {
        QPixmap pixels;
        std::weak_ptr<const DisplayData> display;
        std::weak_ptr<const fs::ModelSnapshot> model;
        Inspection mode=Inspection::Off;
        quint64 camera=0;
        std::array<bool,4> sections{};
        QFont font;
        QString language;
    };
    mutable InspectionLayer inspectionLayer;
    QRect inspectionCornerButton;
    Qt::Corner inspectionCorner=Qt::BottomLeftCorner;
    void paintInspection(QPainter& painter,QRect viewport,qreal dpr,bool final)const{
        const bool liveMove=gesture==Gesture::Move && moved;
        if(final || liveMove || !pendingPointPositions.isEmpty() || previewSections){
            drawInspection(painter,viewport,final);
            return;
        }
        if(inspection==Inspection::Faces){
            drawFaceTiles(painter,viewport,dpr);
            drawInspection(painter,viewport,false,true);
            return;
        }
        auto& layer=inspectionLayer;
        const QSize size(qCeil(viewport.width()*dpr),qCeil(viewport.height()*dpr));
        if(layer.pixels.size()!=size || layer.pixels.devicePixelRatio()!=dpr ||
           layer.display.lock()!=display || layer.model.lock()!=snapshot || layer.camera!=cameraRevision ||
           layer.mode!=inspection || layer.sections!=sectionVisible || layer.font!=painter.font() || layer.language!=uiLanguage()){
            layer={QPixmap(size),display,snapshot,inspection,cameraRevision,sectionVisible,painter.font(),uiLanguage()};
            layer.pixels.setDevicePixelRatio(dpr);
            layer.pixels.fill(Qt::transparent);
            QPainter target(&layer.pixels);
            target.setFont(layer.font);
            drawInspection(target,viewport,false);
        }
        painter.drawPixmap(viewport.topLeft(),layer.pixels);
    }
    void drawInspection(QPainter& painter,QRect viewport,bool final,bool legendOnly=false)const{
        struct Key {const char* text;QColor color;Qt::PenStyle style=Qt::SolidLine;};
        static const Key keys[]={{"已有控制面",faceFill()},{"内部边",QColor("#82929a")},{"边界边",QColor("#14699e")},
            {"无面边",QColor("#a12677"),Qt::DotLine},{"多面共边",QColor("#ba2735"),Qt::DashDotLine},{"折痕",QColor("#ac4a00"),Qt::DashLine},
            {"横剖线",QColor("#00a9bc"),Qt::DashDotDotLine},{"纵剖线",QColor("#00836e"),Qt::DashLine},{"水线",QColor("#6539cd"),Qt::DotLine},
            {"斜剖线",QColor("#7b551f"),Qt::DashDotDotLine},{"控制曲线",QColor("#db1355")},{"流线",QColor("#347525"),Qt::DashDotLine},
            {"参考曲线",QColor("#292149"),Qt::DashDotLine},{"无控制面",Qt::white}};
        static const QColor faceBorder=faceStroke();
        bool used[std::size(keys)]{};const bool faces=inspection==Inspection::Faces;
        static constexpr double widths[]={0,1,3,3,4,2.4,2.1,2.5,2.6,2.4,3,2.5,2.8};
        auto pen=[&](int k){used[k]=true;return QPen(keys[k].color,widths[k],keys[k].style,Qt::FlatCap,Qt::RoundJoin);};
        painter.save();painter.setRenderHint(QPainter::Antialiasing);painter.setPen(Qt::NoPen);
        if(faces){
            used[0]=true;
            used[13]=!emptyControlCells(*display).isEmpty();
            if(!legendOnly){
                painter.setPen(QPen(faceBorder,1.5,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));
                for(const auto& patch:projectFacePatches(final))paintFacePatch(painter,patch);
            }
        }else{
        QVector<Projected> pixels,reflected;
        projectInspectionPoints(final,pixels,reflected);
        painter.setBrush(Qt::NoBrush);std::array<QVector<QLineF>,6> lines;
        for(const auto& edge:display->edges){
            const int k=edge.faceCount==0?3:edge.faceCount==1?2:edge.faceCount==2?1:4;
            auto append=[&](QPointF a,QPointF b){lines[k].append({a,b});if(edge.crease)lines[5].append({a,b});};
            if(splitBody()){
                for(auto span:selectableEdgeSpans(edge))append(projectPoint(span[0]).pixel,projectPoint(span[1]).pixel);
            }else{
                append(pixels[edge.ia].pixel,pixels[edge.ib].pixel);
                if(edge.mirror && !reflected.isEmpty())append(reflected[edge.ia].pixel,reflected[edge.ib].pixel);
            }
        }
        for(int k=1;k<=5;++k)if(!lines[k].isEmpty()){painter.setPen(pen(k));painter.drawLines(lines[k]);}
            const auto& sections=!final&&previewSections?*previewSections:snapshot->sections;
            for(const auto& sec:sections){if(sec.kind<1||sec.kind>4||!sectionVisible[sec.kind-1])continue;
                for(const auto& path:sec.paths){auto sides=fs::presentation::sectionSides(*snapshot,int(projection),sec.kind,path);
                    for(bool mirror:{false,true}){if((mirror?!sides.second:!sides.first)||path.size()<2)continue;QPolygonF poly;
                        for(auto p:path)poly.append(projectPoint(mirror?mirrored(p):p).pixel);painter.setPen(pen(5+sec.kind));painter.drawPolyline(final?poly:screenStroke(poly,.15));}}}
            prepareCurvePaths(final);for(int i=0;i<display->curves.size();++i){const auto& c=display->curves[i];if(!curveVisible(c))continue;
                painter.setPen(pen(c.marker?12:c.flow?11:10));painter.drawPath(curvePathTransform.map(curvePaths[i].path));}
        }
        const auto fm=painter.fontMetrics();const int row=fm.height()+6;
        constexpr int gap=4;
        struct Label {int k;int width;QString text;};
        QVector<Label> labels;
        for(int k=0;k<int(std::size(keys));++k)if(used[k]){
            const QString text=uiText(keys[k].text);
            labels.append({k,fm.horizontalAdvance(text)+53,text});
        }
        const auto paintLabel=[&](const Label& label,QRect box,bool wrapped=false){
            const int k=label.k;
            painter.fillRect(box,QColor(255,255,255,235));
            const QPoint center(box.x()+3,box.y()+box.height()/2);
            if(faces){painter.setPen(faceBorder);painter.setBrush(keys[k].color);painter.drawRect(QRect(center-QPoint(0,5),QSize(37,10)));}
            else{painter.setPen(pen(k));painter.drawLine(center,center+QPoint(40,0));}
            painter.setPen(QColor("#283845"));
            painter.drawText(box.adjusted(47,0,-(wrapped?6:0),0),Qt::AlignVCenter|Qt::AlignLeft|
                (wrapped?Qt::TextWrapAnywhere:0),label.text);
        };
        const auto button=inspectionCornerButton;
        if(!final&&!button.isEmpty()){
            const bool growLeft=inspectionCorner==Qt::TopRightCorner||inspectionCorner==Qt::BottomRightCorner;
            const bool bottom=inspectionCorner==Qt::BottomLeftCorner||inspectionCorner==Qt::BottomRightCorner;
            const int left=viewport.left()+6,right=viewport.right()+1-6;
            // Horizontal order starts at the button. Overflow wraps inward,
            // into a row clear of the button on the same side of the view.
            int cursor=growLeft?button.left()-gap:button.right()+1+gap;
            int anchor=button.y()+(button.height()-row)/2+(bottom?row:0);
            int extent=row;
            bool besideButton=true,hasLabel=false;
            for(const auto& label:labels){
                if((growLeft?cursor-label.width<left:cursor+label.width>right)&&(besideButton||hasLabel)){
                    anchor+=bottom?-extent-gap:extent+gap;
                    if(besideButton)anchor=bottom?std::min(anchor,button.top()-gap):std::max(anchor,button.bottom()+1+gap);
                    cursor=growLeft?right:left;
                    extent=row;besideButton=false;hasLabel=false;
                }
                const int width=std::min(label.width,right-left);
                if(width<=53)break;
                const bool wrapped=width<label.width;
                const int height=wrapped?fm.boundingRect(QRect(0,0,width-53,0),Qt::TextWrapAnywhere,label.text).height()+6:row;
                paintLabel(label,QRect(growLeft?cursor-width:cursor,bottom?anchor-height:anchor,width,height),wrapped);
                extent=std::max(extent,height);
                cursor+=growLeft?-(width+gap):width+gap;
                hasLabel=true;
            }
        }else{
            // Exported views keep their bottom-left legend and omit the mode control.
            QVector<QPoint> positions;int x=7,y=0;
            for(const auto& label:labels){
                if(x>7&&x+label.width>viewport.width()-4){x=7;y+=row;}
                positions.append(QPoint(x,y));x+=label.width+gap;
            }
            painter.translate(0,viewport.height()-y-row-4);
            for(int i=0;i<labels.size();++i)paintLabel(labels[i],QRect(positions[i],QSize(labels[i].width,row)));
        }
        painter.restore();
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

    mutable double basisYaw=std::numeric_limits<double>::quiet_NaN(),basisPitch=0;
    mutable fs::Vec3 perspectiveRight,perspectiveUp,perspectiveDepth;
    void basis(fs::Vec3& right,fs::Vec3& up,fs::Vec3& depth) const {
        switch (projection) {
        case HullProjection::Profile: right={1,0,0};up={0,0,1};depth={0,1,0};break;
        case HullProjection::Plan: right={1,0,0};up={0,1,0};depth={0,0,1};break;
        case HullProjection::Body: right={0,1,0};up={0,0,1};depth={1,0,0};break;
        case HullProjection::Perspective:
            if(basisYaw!=yaw||basisPitch!=pitch){basisYaw=yaw;basisPitch=pitch;
                const auto cy=std::cos(yaw),sy=std::sin(yaw),cp=std::cos(pitch),sp=std::sin(pitch);
                perspectiveRight={cy,sy,0};perspectiveUp={-sy*sp,cy*sp,cp};perspectiveDepth={sy*cp,-cy*cp,sp};}
            right=perspectiveRight;up=perspectiveUp;depth=perspectiveDepth;
            break;
        }
    }
    bool transformCenter(fs::Vec3 &p)const{
        const auto& selection=transformPoints();
        p={};if(selection.isEmpty()||selection.intersects(undoingPlacements))return false;
        for(auto id:selection){auto i=display->pointIndices.constFind(id);if(i==display->pointIndices.cend())return false;
            const auto &q=display->points[*i];if(q.locked||!q.batchMovable)return false;
            const auto current=displayedPosition(q);
            p.x+=current.x;p.y+=current.y;p.z+=current.z;}
        p={p.x/selection.size(),p.y/selection.size(),p.z/selection.size()};return true;
    }
    struct Handle { QPointF start,neck,tip,label,side; };
    bool axisHandle(int a,fs::Vec3 base,Handle &h)const{
        auto dir=axisVector(a,base);const double n=std::hypot(dir.x(),dir.y());if(n<scale*.01)return false;
        dir/=n;const auto o=projectPoint(controlPosition(base)).pixel;
        h={o+dir*12,o+dir*46,o+dir*59,o+dir*72,QPointF(-dir.y(),dir.x())};return true;
    }
    int transformHandleAt(QPointF pos,fs::Vec3 base)const{
        if(QLineF(pos,projectPoint(controlPosition(base)).pixel).length()<11)return 0;
        int hit=0;double best=1.;
        for(int a=1;a<=3;++a){Handle h;if(!axisHandle(a,base,h))continue;
            auto d=h.tip-h.start;double t=std::clamp(QPointF::dotProduct(pos-h.start,d)/QPointF::dotProduct(d,d),0.,1.);
            double dist=std::min(QLineF(pos,h.start+t*d).length()/7.,QLineF(pos,h.label).length()/10.5);
            if(dist<best){best=dist;hit=a;}}
        return hit;
    }
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
    QRectF projectedMeshBounds() const {
        const auto& mesh=display->mesh;
        double left=std::numeric_limits<double>::max(),right=-left,top=left,bottom=-left;
        for(int corner=0;corner<8;++corner){
            const auto point=projectPoint({corner&1?mesh.maximum.x:mesh.minimum.x,
                corner&2?mesh.maximum.y:mesh.minimum.y,corner&4?mesh.maximum.z:mesh.minimum.z}).pixel;
            left=qMin(left,point.x());right=qMax(right,point.x());
            top=qMin(top,point.y());bottom=qMax(bottom,point.y());
        }
        return {QPointF(left,top),QPointF(right,bottom)};
    }
    QSizeF fitSpan(const QRectF& bounds) const {
        const double minimum=display->mesh.extent*.03;
        return {qMax(bounds.width()/scale,minimum),qMax(bounds.height()/scale,minimum)};
    }
    double fitScale(QSize viewport,QSizeF span) const {
        // Leave room for the dimension strings at both ends of the grid.
        const int horizontal=studio?std::min(160,viewport.width()/3):56;
        return qMin(qMax(1,viewport.width()-horizontal)/span.width(),
                    qMax(1,viewport.height()-(studio?112:72))/span.height());
    }
    void prepareAnimatedSections(const fs::curveEdit::Motion& motion,qreal dpr)const{
        const bool gridMasksSections=gridVisible&&snapshot->overlay.present;
        if(animatedSections.camera==cameraRevision&&animatedSections.dpr==dpr&&
           animatedSections.grid==gridMasksSections&&animatedSections.visible==sectionVisible)return;
        animatedSections={cameraRevision,dpr,gridMasksSections,sectionVisible,{},{}};
        const bool perspective=projection==HullProjection::Perspective;
        fs::Vec3 right,up,depth;basis(right,up,depth);
        const double inverseDistance=perspective?1./(cameraExtent*cameraDistanceFactor()):0.;
        const auto knot=[&](fs::Vec3 p,bool mirror){
            if(mirror)p.y=-p.y;
            const auto relative=subtract(p,cameraCenter);
            return MotionKnot{{scale*dot(relative,right),-scale*dot(relative,up)},1.-dot(relative,depth)*inverseDistance};
        };
        for(const auto& [key,tracks]:motion.tracks()){
            if(key.kind<1)continue;
            if(!sectionVisible[key.kind-1]||(sectionDotted(key.kind)&&gridMasksSections))continue;
            for(const auto& track:tracks){
                const auto sides=fs::presentation::sectionSides(*snapshot,int(projection),key.kind,track.from.path);
                if(track.morph&&sides!=fs::presentation::sectionSides(*snapshot,int(projection),key.kind,track.to.path)){
                    animatedSections.exact.append({key,&track});continue;
                }
                for(bool mirror:{false,true}){
                    if(mirror?!sides.second:!sides.first)continue;
                    QVector<MotionVertex> vertices;vertices.reserve(track.from.path.size());
                    for(qsizetype i=0;i<track.from.path.size();++i){
                        const auto from=knot(track.from.path[i],mirror);
                        vertices.append({from,track.morph?knot(track.to.path[i],mirror):from});
                    }
                    animatedSections.strokes.append({key,track.from.color,track.from.opacity,track.to.opacity,
                        reduceMotionStroke(vertices,.20/dpr,perspective)});
                }
            }
        }
    }
    bool sectionDotted(int kind)const{
        return (kind==1&&(projection==HullProjection::Profile||projection==HullProjection::Plan))||
            (kind==2&&(projection==HullProjection::Body||projection==HullProjection::Plan))||
            (kind==3&&(projection==HullProjection::Profile||projection==HullProjection::Body));
    }
    fs::presentation::ViewOverlay viewOverlay(const QFont& font,const fs::curveEdit::Motion* motion)const{
        if(!snapshot)return {};
        const bool hydro=hydroVisible&&!fillVisible;
        const auto project=[this](auto point){return projectPoint(point).pixel;};
        const auto build=[&]{return fs::presentation::makeOverlay(*snapshot,int(projection),gridVisible,hydro,font,project,sectionVisible);};
        if(!motion)return build();
        const auto language=uiLanguage();
        if(animatedOverlay.model.lock()!=snapshot||animatedOverlay.camera!=cameraRevision||
           animatedOverlay.grid!=gridVisible||animatedOverlay.hydro!=hydro||animatedOverlay.visible!=sectionVisible||
           animatedOverlay.font!=font||animatedOverlay.language!=language)
            animatedOverlay={snapshot,cameraRevision,gridVisible,hydro,sectionVisible,font,language,build()};
        if(const auto frame=motion->referenceFrame(motion->progress()))
            fs::presentation::updateHorizontalReferences(animatedOverlay.geometry,*snapshot,int(projection),font,project,*frame);
        return animatedOverlay.geometry;
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
    fs::Vec3 movedPosition(fs::Vec3 p,fs::EntityId id=0) const {
        const double w=precise&&id?weights.value(id,0):1.;
        return {p.x+w*dragDelta.x,p.y+w*dragDelta.y,p.z+w*dragDelta.z};
    }
    fs::Vec3 displayedPosition(const DisplayPoint& point,bool final=false) const {
        if(final)return point.position;
        if(!pendingPointPositions.isEmpty()){
            const auto pending=pendingPointPositions.constFind(point.id);
            if(pending!=pendingPointPositions.cend())return pending.value();
        }
        return gesture==Gesture::Move && moved && (precise?weights.contains(point.id):selection.contains(point.id))?movedPosition(point.position,point.id):point.position;
    }
    fs::curveEdit::PointPositions curvePreviewPositions()const{
        auto points=pendingPointPositions;
        if(gesture==Gesture::Move&&moved)
            for(const auto& point:dragStarts)points.insert(point.id,movedPosition(point.position,point.id));
        return points;
    }
    QPainterPath projectEditCurve(const QVector<fs::presentation::Cubic>& world,bool symmetric=true)const{
        using namespace fs::presentation;
        const auto segments=splitBody()?splitBodyCubics(world,splitLocation(*snapshot)):world;
        const bool perspective=projection==HullProjection::Perspective;
        auto path=projectCubics(segments,[this](auto p){return projectPoint(p).pixel;},perspective);
        if(symmetric&&snapshot->bothSides&&projection!=HullProjection::Profile)
            path.addPath(projectCubics(segments,[this](auto p){return projectPoint(mirrored(p)).pixel;},perspective));
        return path;
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
            if(undoingPlacements.contains(point.id))continue;
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
    state_->releasedCreases.reset();
    if(state_->precise)cancelGesture();
    if(state_->previewSections||state_->compareSections)setMoveSectionPreview({});
    const bool maskChanged=state_->visibilityChanges!=visibilityChanges;
    state_->visibilityChanges=visibilityChanges;
    const bool sameDocument=snapshot && state_->committedSnapshot && snapshot->documentId==state_->committedSnapshot->documentId;
    const bool sameUnits=snapshot && state_->committedSnapshot && snapshot->units==state_->committedSnapshot->units;
    if(state_->stationEditor && (!sameDocument||!sameUnits)){
        state_->stationEditor->reset();if(snapshot)state_->stationEditor->setUnits(snapshot->units);
        if(!sameDocument)state_->stationEditor->hide();
    }
    if(!sameDocument){state_->cameraMotion.active=false;state_->hovered=0;state_->hoverMotion.clear();state_->emphasized.clear();state_->announcedSelection.clear();state_->interactionDriver->stop();}
    if(drawingUnchanged&&sameDocument&&sameUnits&&snapshot->sameDrawing(*state_->committedSnapshot)) {
        const bool completePreview=state_->committedSnapshot->isCachedPreview()&&!snapshot->isCachedPreview();
        state_->committedSnapshot=std::move(snapshot);
        state_->snapshot=presentation?std::move(presentation):state_->committedSnapshot;
        if(completePreview||maskChanged){state_->rebuildDisplay();++state_->cameraRevision;}
        state_->pendingPointPositions.clear();
        refreshDisplayModeButton();
        update();return;
    }
    cancelGesture();
    state_->committedSnapshot=std::move(snapshot);
    state_->snapshot=presentation?std::move(presentation):state_->committedSnapshot;
    if(!sameDocument){state_->flowPlacement=false;state_->pointPlacement=false;state_->hiddenControlLayers.clear();}
    if(!sameDocument||!background()||!background()->visible)state_->backgroundMode=BackgroundMode::None;
    state_->rebuildDisplay();
    state_->pendingPointPositions.clear();
    ++state_->cameraRevision;
    if (!sameDocument) {
        state_->selected=0;state_->selection.clear();state_->entities.clear();state_->entityPoints.clear();
        state_->yaw=-30*Pi/180;state_->pitch=25*Pi/180;
        fit();
    } else {
        state_->pruneSelection();
        setSelectedEntities(state_->entities);
        if (!sameUnits) fit();
    }
    state_->labels.clear();
    if(!sameDocument&&state_->snapshot){
        state_->gridVisible=state_->gridOverride.value_or(state_->snapshot->overlay.grid);
        state_->hydroVisible=state_->hydroOverride.value_or(state_->snapshot->overlay.hydro);
        if(state_->studio&&state_->snapshot->overlay.present)restartPresentation();
    }else finishPresentation();
    refreshDisplayModeButton();
    update();
}
std::shared_ptr<const fs::ModelSnapshot> HullView::snapshot() const { return state_->committedSnapshot; }
void HullView::setPlateDiagnosticFaces(const QString& document, quint64 revision, QVector<QVector<fs::Vec3>> faces){
    if(faces.isEmpty()&&state_->plateDiagnosticFaces.isEmpty())return;
    state_->plateDiagnosticDocument=document;
    state_->plateDiagnosticRevision=revision;
    state_->plateDiagnosticFaces=std::move(faces);
    update();
}
std::shared_ptr<const QVector<fs::Section>> HullView::moveSectionPreview()const{return state_->previewSections;}
void HullView::setMoveSectionPreview(std::shared_ptr<const QVector<fs::Section>> sections){if(!sections)setDragSectionComparison({});if(state_->previewSections!=sections){state_->previewSections=std::move(sections);update();}}
void HullView::setDragSectionComparison(DragSections pairs,DragShell shell){if(state_->compareSections!=pairs||state_->compareShell!=shell){state_->compareSections=std::move(pairs);state_->compareShell=std::move(shell);if(!state_->compareShell)state_->shellPaint={};update();}}
// Preserve the last displayed subdivision knots in model space. The normal
// wireframe uses straight segments between these same knots, not cubic strokes.
void HullView::retainReleasedCreases(){
    auto& s=*state_;s.releasedCreases.reset();if(!s.compareShell||s.compareShell->isEmpty()||!s.display)return;
    QSet<QPair<fs::EntityId,fs::EntityId>> keys;
    for(const auto& edge:s.display->edges)if(edge.faceCount && (edge.crease||edge.faceCount==1))keys.insert({std::min(edge.a,edge.b),std::max(edge.a,edge.b)});
    auto& lines=s.releasedCreases.emplace();
    for(const auto& trace:*s.compareShell){if(!keys.contains({std::min(trace.a,trace.b),std::max(trace.a,trace.b)}))continue;
        const auto& knots=trace.after.points;
        for(qsizetype i=1;i<knots.size();++i){const auto a=knots[i-1],b=knots[i];
            if(s.splitBody())lines+=s.controlSegments(a,b);else {lines.append(fs::presentation::Segment{a,b});
                if(s.snapshot->bothSides&&trace.symmetric&&s.projection!=HullProjection::Profile&&(a.y!=0||b.y!=0))lines.append(fs::presentation::Segment{mirrored(a),mirrored(b)});}}
    }
}
DragShell HullView::dragShellComparison()const{return state_->compareShell;}
DragSections HullView::dragSectionComparison()const{return state_->compareSections;}
fs::Vec3 HullView::pointMoveAnchor()const{return state_->pivot;}
bool HullView::axisPointMove()const{return state_->precise;}
void HullView::setPendingPointMoves(const QVector<fs::PointMove>& moves){
    if(moves.isEmpty())state_->releasedCreases.reset();
    if(moves.isEmpty()&&state_->pendingPointPositions.isEmpty())return;
    state_->pendingPointPositions.clear();
    state_->pendingPointPositions.reserve(moves.size());
    for(const auto& move:moves)state_->pendingPointPositions.insert(move.id,move.position);
    update();
}
bool HullView::curveEditMotionEnabled()const{return state_->animations&&hullMotionEnabled();}
void HullView::setCurveEditMotion(std::shared_ptr<fs::curveEdit::Motion> motion){
    if(!curveEditMotionEnabled())motion.reset();
    if(state_->curveEditMotion==motion)return;
    QObject::disconnect(state_->curveEditFrameConnection);
    state_->animatedSections={};
    state_->animatedOverlay={};
    state_->curveEditMotion=std::move(motion);
    if(state_->curveEditMotion)
        state_->curveEditFrameConnection=state_->curveEditMotion->onFrame(this,[this]{if(isVisible())update();});
    update();
}
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
    if(state_->pointPlacement)setPointPlacement(projection!=HullProjection::Perspective);
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
    state_->globalKind=kind;
    kind=state_->inspection==State::Inspection::Faces?HullEntity::Face:state_->inspection==State::Inspection::Lines?HullEntity::Edge:kind;
    updateInteractionHover(0);cancelGesture();state_->kind=kind;state_->entities.clear();state_->entityPoints.clear();state_->emphasized.clear();update();
}
HullEntity HullView::selectionKind() const{return state_->kind;}
QSet<fs::EntityId> HullView::selectedEntities() const{return state_->entities;}
void HullView::setSelectedEntities(QSet<fs::EntityId> ids) {
    QSet<fs::EntityId> valid;state_->entityPoints.clear();
    if(!ids.isEmpty()&&state_->snapshot) {
        if(state_->kind==HullEntity::Edge && state_->display)
            for(const auto& edge:state_->display->edges)if(ids.contains(edge.id)){valid.insert(edge.id);state_->entityPoints.insert(edge.a);state_->entityPoints.insert(edge.b);}
        if(state_->kind==HullEntity::Face && state_->display)
            for(const auto& face:state_->display->faces)if(ids.contains(face.id)){valid.insert(face.id);for(auto i:face.points)state_->entityPoints.insert(state_->display->points[i].id);}
        if(state_->display&&(state_->kind==HullEntity::Curve||state_->kind==HullEntity::Flowline))
            for(const auto& curve:state_->display->curves)if(!curve.marker&&curve.flow==(state_->kind==HullEntity::Flowline)&&state_->curveVisible(curve)&&ids.contains(curve.id))valid.insert(curve.id);
    }
    const auto previous=state_->entities;state_->entities=std::move(valid);emphasizeSelection(previous,state_->entities,state_->kind);update();
}
void HullView::setStudioAppearance(bool enabled){
    state_->studio=enabled;
    if(enabled){
        for(const auto* key:{"projection_title_text","projection_title_text_width","projection_title_available_width","projection_title_rect"})
            state_->evidence.remove(key);
    }
    bindUiText(this,"accessibleName",projectionSourceKey());
    update();
}
void HullView::setGraphicPreferences(const QJsonObject& colors){state_->preferenceColors=colors;++state_->lightRevision;update();}
void HullView::setFillVisible(bool enabled){state_->faceTiles={};state_->inspection=State::Inspection::Off;if(enabled&&state_->precise)cancelGesture();state_->fillVisible=enabled;if(enabled){state_->shadeMode=HullShade::Flat;++state_->lightRevision;}refreshDisplayModeButton();update();}
bool HullView::setShadeMode(HullShade mode){
    if(int(mode)<0||int(mode)>3)return false;
    if(mode!=HullShade::Flat){if(!state_->snapshot||!state_->snapshot->surfaceTopology||!state_->display)return false;
        try{prepareAnalysis(*state_->display,*state_->snapshot);}catch(const std::exception& e){if(onEditRejected)onEditRejected(QString::fromUtf8(e.what()));return false;}}
    const bool inspecting=state_->inspection!=State::Inspection::Off;state_->inspection=State::Inspection::Off;
    if(!inspecting&&state_->fillVisible&&state_->shadeMode==mode)return true;state_->fillVisible=true;state_->shadeMode=mode;++state_->lightRevision;finishPresentation();refreshDisplayModeButton();update();return true;
}
HullShade HullView::shadeMode()const{return state_->shadeMode;}
bool HullView::setFocalLength(int millimeters){
    if(millimeters!=28&&millimeters!=50&&millimeters!=90&&millimeters!=130&&millimeters!=200)return false;
    if(state_->projection!=HullProjection::Perspective)return false;
    if(state_->focalLength==millimeters)return true;
    state_->focalLength=millimeters;fit();if(onViewSettingsChanged)onViewSettingsChanged("lens");return true;
}
int HullView::focalLength()const{return state_->focalLength;}
HullLight HullView::light()const{return state_->light;}
bool HullView::setLight(const HullLight& light){
    if(!light.valid())return false;if(light==state_->light)return true;
    state_->light=light;++state_->lightRevision;update();if(onViewSettingsChanged)onViewSettingsChanged("light");return true;
}
QString HullView::displayModeKey()const{
    if(state_->inspection==State::Inspection::Faces)return "inspect_faces";
    if(state_->inspection==State::Inspection::Lines)return "inspect_lines";
    if(!state_->fillVisible)return "wireframe";
    return state_->shadeMode==HullShade::Flat?QStringLiteral("fill"):"shade_"+QString::number(int(state_->shadeMode));
}
void HullView::restoreDisplayMode(const QString& mode){
    if(mode=="fill" || mode=="wireframe")setFillVisible(mode=="fill");
    else if(mode=="inspect_faces" || mode=="inspect_lines"){
        setFillVisible(false);
        state_->inspection=mode=="inspect_faces"?State::Inspection::Faces:State::Inspection::Lines;
    }else for(int i=1;i<=3;++i)if(mode=="shade_"+QString::number(i)){
        setFillVisible(true);
        state_->shadeMode=HullShade(i);
    }
    refreshDisplayModeButton();
}
bool HullView::selectDisplayMode(const QString& mode){
    const auto* entry=displayModeEntry(mode);
    if(!entry||!state_->snapshot)return false;
    if(mode==displayModeKey())return true;
    if(entry->shade!=HullShade::Flat){
        if(!setShadeMode(entry->shade))return false;
    }else if(mode=="inspect_faces"||mode=="inspect_lines"){
        cancelGesture();restoreDisplayMode(mode);finishPresentation();update();
    }else setFillVisible(mode=="fill");
    if(onViewSettingsChanged)onViewSettingsChanged("mode");
    return true;
}
void HullView::installDisplayModeButton(const std::function<QIcon(const QString&)>& icon){
    if(displayModeButton_)return;
    auto* button=displayModeButton_=new QToolButton(this);
    button->setObjectName("HullDisplayMode");
    button->setFixedSize(28,28);button->setIconSize(QSize(19,19));
    button->setFocusPolicy(Qt::StrongFocus);
    button->setStyleSheet(
        "QToolButton#HullDisplayMode {background:#f6f9f7;border:1px solid #c7dbd2;border-radius:4px;padding:2px;}"
        "QToolButton#HullDisplayMode:hover,QToolButton#HullDisplayMode:focus {background:#e5f2ec;border-color:#84aa9c;}"
        "QToolButton#HullDisplayMode:pressed {background:#d6ebe2;}"
        "QToolButton#HullDisplayMode:disabled {background:#eef3f0;}"
        "QToolButton#HullDisplayMode::menu-indicator {width:0;height:0;image:none;}");
    auto* menu=new QMenu(button);menu->setObjectName("HullDisplayModeMenu");
    menu->setProperty("menu_role","render_mode");
    button->setMenu(menu);button->setPopupMode(QToolButton::InstantPopup);
    auto* group=new QActionGroup(menu);
    for(const auto& entry:displayModes){
        auto* item=menu->addAction(icon(QLatin1String(entry.icon)),uiText(entry.text));
        const QString key=QLatin1String(entry.key);
        item->setProperty("view_operation",key);item->setCheckable(true);group->addAction(item);
        if(entry.shade!=HullShade::Flat)item->setProperty("shade_mode",int(entry.shade));
        bindUiText(item,"text",entry.text);
        connect(item,&QAction::triggered,this,[this,key]{selectDisplayMode(key);refreshDisplayModeButton();});
    }
    connect(menu,&QMenu::aboutToShow,this,[this,menu]{
        menu->setAttribute(Qt::WA_DontShowOnScreen,testAttribute(Qt::WA_DontShowOnScreen));
        refreshDisplayModeButton();
    });
    bindUiRefresh(button,[this]{refreshDisplayModeButton();});
    refreshDisplayModeButton();
    positionDisplayModeButton();
    button->show();
}
void HullView::refreshDisplayModeButton(){
    const auto effective=state_->inspection==State::Inspection::Faces?HullEntity::Face:state_->inspection==State::Inspection::Lines?HullEntity::Edge:state_->globalKind;
    if(effective!=state_->kind)setSelectionKind(state_->globalKind);
    if(!displayModeButton_)return;
    const auto current=displayModeKey();
    displayModeButton_->setEnabled(bool(state_->snapshot));
    for(auto* action:displayModeButton_->menu()->actions()){
        action->setEnabled(state_->snapshot&&(!action->property("shade_mode").isValid()||state_->snapshot->surfaceTopology));
        const bool active=action->property("view_operation")==current;
        action->setChecked(active);
        if(active){
            displayModeButton_->setIcon(action->icon());
            const auto* entry=displayModeEntry(current);
            const auto label=uiText("模式")+QStringLiteral(" · ")+uiText(entry->text);
            displayModeButton_->setAccessibleName(label);
        }
    }
    displayModeButton_->setProperty("display_mode",current);
}
void HullView::setDisplayModeButtonCorner(Qt::Corner corner){
    if(displayModeCorner_==corner)return;
    displayModeCorner_=corner;positionDisplayModeButton();
}
void HullView::positionDisplayModeButton(){
    if(!displayModeButton_)return;
    const bool right=displayModeCorner_==Qt::TopRightCorner||displayModeCorner_==Qt::BottomRightCorner;
    const bool bottom=displayModeCorner_==Qt::BottomLeftCorner||displayModeCorner_==Qt::BottomRightCorner;
    constexpr int margin=6;
    displayModeButton_->move(right?width()-displayModeButton_->width()-margin:margin,
                             bottom?height()-displayModeButton_->height()-margin:margin);
    const auto reserve=displayModeButton_->geometry();
    if(reserve!=state_->inspectionCornerButton||displayModeCorner_!=state_->inspectionCorner){
        state_->inspectionCornerButton=reserve;
        state_->inspectionCorner=displayModeCorner_;
        state_->inspectionLayer={};update();
    }
}
QJsonObject HullView::viewSettingsEvidence()const{
    const auto& light=state_->light;
    return {{"focal_mm",state_->focalLength},{"camera_distance_extents",state_->cameraDistanceFactor()},
        {"light_direction",QJsonArray{light.direction.x,light.direction.y,light.direction.z}},{"ambient",light.ambient},{"intensity",light.intensity},{"lighting_enabled",light.enabled},
        {"face_projection_builds",double(state_->faceProjectionBuilds)},{"fill_renders",double(state_->fillBuilds)},{"gpu",state_->usedGpu},{"camera_revision",double(state_->cameraRevision)},{"curve_path_builds",state_->curvePathBuilds},
        {"shade_mode",int(state_->shadeMode)},{"analysis_built",state_->display&&bool(state_->display->analysis)}};
}
void HullView::setControlNetVisible(bool enabled){if(!enabled&&state_->precise)cancelGesture();state_->controlNetVisible=enabled;update();}
void HullView::setLayerVisibilityPreview(const QSet<fs::EntityId>& changes){
    if(changes==state_->visibilityChanges)return;
    if(state_->precise||state_->gesture==State::Gesture::Stroke)cancelGesture();state_->visibilityChanges=changes;state_->rebuildDisplay(true);
    state_->pruneSelection();setSelectedEntities(state_->entities);
    // The camera and projected reference curves did not move. Only the surface
    // fill and the label obstacles depend on the newly visible geometry.
    state_->fillImage={};state_->labels.clear();finishPresentation();
}
void HullView::setHiddenControlLayers(const QSet<fs::EntityId>& ids){
    if(state_->hiddenControlLayers==ids)return;if(state_->precise||state_->gesture==State::Gesture::Stroke)cancelGesture();state_->hiddenControlLayers=ids;
    state_->rebuildDisplay();
    state_->pruneSelection();++state_->cameraRevision;update();
}
void HullView::setSectionVisible(int kind,bool enabled){if(kind>=1&&kind<=4){state_->sectionVisible[kind-1]=enabled;update();}}
void HullView::highlightSections(int kind,QSet<int> indices){state_->highlightedKind=kind;state_->highlightedSections=std::move(indices);update();}
void HullView::setGridVisible(bool enabled){state_->gridOverride=enabled;finishPresentation();state_->gridVisible=enabled;state_->labels.clear();update();}
void HullView::setHydroVisible(bool enabled){state_->hydroOverride=enabled;finishPresentation();state_->hydroVisible=enabled;state_->labels.clear();update();}
bool HullView::gridVisible()const{return state_->gridVisible;}
bool HullView::hydroVisible()const{return state_->hydroVisible;}
void HullView::setCoordinateAxis(int axis){if(axis<0||axis>3)return;finishPresentation();state_->coordinateAxis=axis;state_->labels.clear();update();}
int HullView::coordinateAxis()const{return state_->coordinateAxis;}
void HullView::chooseCoordinateAxis(int axis){
    if(axis!=0&&onShowControlNet)onShowControlNet();
    setCoordinateAxis(axis);
    if(onViewSettingsChanged)onViewSettingsChanged("axis");
}
void HullView::setAnimationsEnabled(bool enabled){state_->animations=enabled;if(!enabled){setCurveEditMotion({});finishPresentation();finishInteractionCamera();state_->hovered=0;state_->hoverMotion.clear();state_->emphasized.clear();state_->interactionDriver->stop();update();}}
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
        {"camera_yaw",state_->yaw},{"camera_pitch",state_->pitch},
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
    // Some platforms request the mouse menu on press, before a drag can start.
    if(event->reason()==QContextMenuEvent::Mouse && state_->gesture==State::Gesture::Pan){
        state_->contextMenuOnRelease=true;
        event->accept();return;
    }
    if(event->reason()==QContextMenuEvent::Mouse&&state_->lastRightDragged){event->accept();return;}
    finishPresentation();
    auto* menu=new QMenu(this);menu->setAttribute(Qt::WA_DeleteOnClose);
    menu->setAttribute(Qt::WA_DontShowOnScreen,testAttribute(Qt::WA_DontShowOnScreen));
    if(onPopulateSelectionMenu&&onPopulateSelectionMenu(*menu)){
        menu->popup(event->globalPos());event->accept();return;
    }
    auto add=[&](QMenu* owner,const QString& text,const QString& operation,auto callback){
        auto* action=owner->addAction(text);
        action->setProperty("view_operation",operation);
        action->setProperty("studio_opens_dialog",operation=="light_settings"||operation=="export_pdf"||operation=="save_image");
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
    if(state_->fillVisible) add(menu,uiText("亮度"),"light_settings",[this]{if(onViewAction)onViewAction("light_settings");});
    if(backgroundView()<3)populateBackgroundMenu(menu->addMenu(uiText("背景影像")));
    menu->addSeparator();
    add(menu,uiText("导出 ")+QStringLiteral("PDF…"),"export_pdf",[this]{if(onViewAction)onViewAction("export_pdf");});
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
            for(const auto& span:state_->selectableEdgeSpans(edge)){
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
            for(const auto& points:state_->faceParts(face)){
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
void HullView::setPointPlacement(bool enabled){
    cancelGesture();state_->pointPlacement=enabled&&state_->snapshot&&state_->projection!=HullProjection::Perspective;
    if(state_->pointPlacement){state_->backgroundMode=BackgroundMode::None;setCursor(Qt::CrossCursor);}else unsetCursor();
    const bool plane=state_->pointPlacement&&state_->projection==HullProjection::Body;
    if(plane&&!state_->stationEditor)state_->stationEditor=new StationPlaneEditor(this,[this]{return snapshot();});
    if(auto* editor=state_->stationEditor){
        if(plane){editor->setUnits(state_->snapshot->units);editor->adjustSize();editor->raise();}editor->setVisible(plane);
    }
}
void HullView::setPlacementUndoPreview(const QSet<fs::EntityId>& ids){if(state_->undoingPlacements!=ids){state_->undoingPlacements=ids;update();}}
void HullView::setPendingPlacements(const QVector<fs::Vec3>& p){if(p.isEmpty()&&state_->pendingPlacements.isEmpty())return;state_->pendingPlacements=p;update();}
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
bool HullView::gpuReady() const { return state_->display && state_->gpuMesh.lock()==state_->display && state_->gpu && state_->gpu->ready(); }

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
        state_->scale=state_->orbitFitScale=1;state_->offset=QPointF(width()/2.0,height()/2.0);
        state_->fitted=true;update();return;
    }
    state_->scale=1;state_->offset={};
    const auto& mesh=state_->display->mesh;
    state_->cameraCenter=mesh.center;state_->cameraExtent=mesh.extent;
    const auto bounds=state_->projectedMeshBounds();
    state_->scale=state_->orbitFitScale=state_->fitScale(size(),state_->fitSpan(bounds));
    state_->offset=QPointF(width()/2.0,height()/2.0+9)-bounds.center()*state_->scale;
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
    state_->offset+=delta;state_->fitted=false;++state_->cameraRevision;state_->reuseCurvePaths(previousRevision);
    if(state_->faceTiles.camera==previousRevision)state_->faceTiles.camera=state_->cameraRevision;
    update();
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
std::optional<fs::Vec3> HullView::displayedControlPoint(fs::EntityId id)const{
    if(state_->display){
        const auto found=state_->display->pointIndices.constFind(id);
        if(found!=state_->display->pointIndices.cend())return state_->displayedPosition(state_->display->points[*found]);
    }
    const auto* point=state_->snapshot?state_->snapshot->point(id):nullptr;
    return point?std::optional{point->position}:std::nullopt;
}
QPointF HullView::projectControlPoint(fs::EntityId id)const{
    const auto point=displayedControlPoint(id);
    return point?project(state_->controlPosition(*point)):QPointF{};
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
    QPicture picture;QPainter painter(&picture);painter.setFont(font());draw(painter,ratio,true,true);
    if(state_->studio){
        // Printed sheets retain their view identification; the editor canvas and PNG do not carry it.
        auto captionFont=QApplication::font();captionFont.setPointSizeF(std::max(11.,captionFont.pointSizeF()));
        painter.setFont(captionFont);painter.setPen(QColor(0,61,69));painter.setRenderHint(QPainter::TextAntialiasing,true);
        painter.drawText(QRect(12,8,std::max(1,width()-24),height()-16),Qt::AlignLeft|Qt::AlignTop|Qt::TextWordWrap,projectionLabel());
    }
    painter.end();picture.setBoundingRect(rect());return picture;
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
        const auto snapshot=state_->snapshot;
        if(state_->inspection!=State::Inspection::Off){
            state_->paintInspection(painter,rect(),dpr,final);
            if(state_->kind==HullEntity::Point && !state_->selection.isEmpty()){
                painter.save();
                painter.setRenderHint(QPainter::Antialiasing);
                painter.setPen(Qt::NoPen);
                painter.setBrush(controlSelectionInk);
                for(auto id:state_->selection){
                    auto index=data->pointIndices.constFind(id);
                    if(index==data->pointIndices.cend())continue;
                    const auto& point=data->points[*index];
                    const auto pos=project(state_->controlPosition(state_->displayedPosition(point,final)));
                    if(point.locked)painter.drawRect(QRectF(pos-QPointF(3.5,3.5),QSizeF(7,7)));
                    else painter.drawEllipse(pos,3.5,3.5);
                }
                painter.restore();
            }
            state_->paintedHull=!data->faces.isEmpty();
        }
        else {
        if (state_->fillVisible) {
        QScopedValueRollback<HullShade> restoreShade(state_->shadeMode);
        if(state_->shadeMode!=HullShade::Flat){
            if(state_->snapshot->surfaceTopology)prepareAnalysis(*data,*state_->snapshot);
            else state_->shadeMode=HullShade::Flat;
        }
        if (!forceSoftware && !data->translucent && !data->mesh.triangles.isEmpty() && state_->gpuMesh.lock()!=data) {
            if(state_->gpu)state_->gpu->setMesh(data->mesh);
            else state_->gpu=std::make_unique<HullGpu>(data->mesh);
            state_->gpuMesh=data;
        }
        if (state_->fillImage.isNull() || state_->fillRevision!=state_->cameraRevision ||
            state_->fillSize!=size() || state_->fillDpr!=dpr || state_->fillLightRevision!=state_->lightRevision || state_->fillSoftware!=forceSoftware) {
            QImage fill;
            // Keep extreme close-up coordinates in double precision on the CPU.
            if (!forceSoftware && !data->translucent && state_->gpuMesh.lock()==data && state_->gpu && state_->gpu->ready() && state_->scale*data->mesh.extent<1e6)
                if(state_->shadeMode==HullShade::Flat||state_->gpu->setAnalysis(data->shading))fill=state_->gpu->render(size(),dpr,state_->camera(size()),state_->lighting(),state_->style());
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
        const bool shellFocus=!final&&!preview&&state_->compareShell&&!state_->compareShell->isEmpty();
        const auto liveSections=final?nullptr:state_->previewSections;
        const auto* editMotion=!liveSections&&!final&&state_->curveEditMotion&&state_->curveEditMotion->active()?state_->curveEditMotion.get():nullptr;
        const double editProgress=editMotion?editMotion->progress():0.;
        QFont annotationFont=studioTextFont(QFont("Arial",snapshot?snapshot->overlay.fontSize:9));
        const auto overlay=state_->viewOverlay(annotationFont,editMotion);
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
        painter.save();if(shellFocus)painter.setOpacity(.22);
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
        const auto sectionDotted=[&](int kind){return state_->sectionDotted(kind);};
        const auto sectionPen=[&](int kind,fs::EntityId index,bool dotted,QColor color){
            const bool selected=state_->highlightedKind==kind&&state_->highlightedSections.contains(int(index));
            return QPen(selected?QColor(160,67,34):dotted?QColor(150,160,157):color,selected?2:1,dotted?Qt::DotLine:Qt::SolidLine);
        };
        const auto drawSections=[&](QPainter& painter){
            if(!preview&&snapshot)for(const auto& section:(liveSections?*liveSections:snapshot->sections)) {
                if(!state_->sectionVisible[section.kind-1])continue;
                if(editMotion&&editMotion->contains({section.kind,fs::EntityId(section.index)}))continue;
                const bool selected=state_->highlightedKind==section.kind&&state_->highlightedSections.contains(section.index);
                const bool dotted=sectionDotted(section.kind);
                const bool comb=!liveSections&&snapshot->showCurvature&&section.showCurvature&&!state_->fillVisible;
                if(dotted&&state_->gridVisible&&snapshot->overlay.present&&!comb)continue;
                painter.setPen(sectionPen(section.kind,section.index,dotted,sourceColor(section.color)));
                for(int index=0;index<section.paths.size();++index){
                    const auto& path=section.paths[index];
                    if(!liveSections&&!final&&!comb&&!dotted&&!selected&&!state_->entrance.active()){
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
            if(preview||!snapshot||!editMotion)return;
            state_->prepareAnimatedSections(*editMotion,dpr);
            const auto visible=[&](int kind){
                return state_->sectionVisible[kind-1]&&!(sectionDotted(kind)&&state_->gridVisible&&snapshot->overlay.present);
            };
            painter.save();
            QPolygonF pixels;
            for(const auto& stroke:state_->animatedSections.strokes){
                const auto& key=stroke.key;
                if(!visible(key.kind))continue;
                const double opacity=stroke.fromOpacity+(stroke.toOpacity-stroke.fromOpacity)*editProgress;
                if(opacity<=0)continue;
                painter.setOpacity(opacity);
                painter.setPen(sectionPen(key.kind,key.id,sectionDotted(key.kind),stroke.color));
                pixels.clear();pixels.reserve(stroke.vertices.size());
                for(const auto& vertex:stroke.vertices)pixels.append(state_->offset+vertex.pixel(editProgress));
                painter.drawPolyline(pixels);
            }
            for(const auto& [key,track]:state_->animatedSections.exact){
                if(!visible(key.kind))continue;
                const bool dotted=sectionDotted(key.kind);
                const auto shape=fs::curveEdit::Motion::sample(*track,editProgress);
                if(shape.opacity<=0)continue;
                const auto sides=sectionSides(*snapshot,int(state_->projection),key.kind,shape.path);
                painter.setOpacity(shape.opacity);
                painter.setPen(sectionPen(key.kind,key.id,dotted,shape.color));
                for(bool mirror:{false,true}){
                    if(mirror?!sides.second:!sides.first)continue;
                    pixels.clear();pixels.reserve(shape.path.size());
                    for(auto p:shape.path)pixels.append(project(mirror?mirrored(p):p));
                    painter.drawPolyline(pixels);
                }
            }
            painter.restore();
        };
        auto& layer=state_->entranceSections;
        const QSize pixels(qCeil(width()*dpr),qCeil(height()*dpr));
        // The wave advances underneath the stationary section lines. Rasterize
        // just those lines at the actual display resolution during the reveal.
        if(!liveSections&&!editMotion&&!final&&!preview&&state_->entrance.active()&&snapshot&&!snapshot->sections.isEmpty()&&
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
        }else if(!liveSections&&!editMotion&&!final&&!preview&&state_->layerMatches(size(),dpr)&&
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
        painter.restore();
        // Paint comparison geometry above the straight control cage, below its handles.
        QVector<QPainterPath> beforePaths,afterPaths,contextPaths;
        const bool comparing=!final&&!preview&&snapshot;
        const auto sectionsToDraw=state_->compareSections;
        if(comparing&&sectionsToDraw){
            const bool perspective=state_->projection==HullProjection::Perspective;
            const auto collect=[&](const fs::Section& s,QVector<QPainterPath>& out){
                if(sectionDotted(s.kind))return;
                for(qsizetype i=0;i<s.paths.size();++i){
                    const auto& samples=s.paths[i];
                    const auto sides=sectionSides(*snapshot,int(state_->projection),s.kind,samples);
                    const auto cubics=i<s.splines.size()?splineCubics(s.splines[i]):QVector<Cubic>{};
                    for(bool mirror:{false,true}){
                        if(mirror?!sides.second:!sides.first)continue;
                        auto pixel=[&](fs::Vec3 p){return project(mirror?mirrored(p):p);};
                        if(!cubics.isEmpty())out.append(projectCubics(cubics,pixel,perspective));
                        else if(!samples.isEmpty()){
                            QPainterPath path(pixel(samples.front()));
                            for(qsizetype j=1;j<samples.size();++j)path.lineTo(pixel(samples[j]));
                            out.append(std::move(path));
                        }
                    }
                }
            };
            for(const auto& pair:*sectionsToDraw){if(state_->compareSections)collect(pair.before,beforePaths);collect(pair.after,afterPaths);}
        }
        const auto shell=comparing?state_->compareShell:DragShell{};
        auto& cache=state_->shellPaint;
        const auto phase=pixelPhase(painter.deviceTransform().map(QPointF{}));
        const QSize shellPixels(qCeil(width()*dpr)+1,qCeil(height()*dpr)+1);
        const bool raster=shell&&!shellPixels.isEmpty()&&qint64(shellPixels.width())*shellPixels.height()<=4*1024*1024;
        QPen original(QColor(150,100,67,205),1.6,Qt::DashLine,Qt::RoundCap);original.setDashPattern({4.,4.});
        const QPen current(QColor(19,115,107),2.,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin);
        if(shell){
            const bool sameView=raster&&cache.camera==state_->cameraRevision&&cache.dpr==dpr&&cache.size==shellPixels&&cache.phase==phase;
            if(!sameView||cache.source!=shell){
                bool keep[3]={sameView,sameView,sameView};
                cache.edges.clear();cache.edges.reserve(shell->size());
                if(!cache.source||cache.source->size()!=shell->size())std::fill_n(keep,3,false);
                for(qsizetype k=0;k<shell->size();++k){const auto& e=shell->at(k);cache.edges.insert({std::min(e.a,e.b),std::max(e.a,e.b)});
                    if(keep[0]||keep[1]||keep[2]){const auto& old=cache.source->at(k);
                        if(e.context!=old.context||e.symmetric!=old.symmetric){std::fill_n(keep,3,false);continue;}
                        keep[e.context?0:1]&=e.afterCubics.constData()==old.afterCubics.constData();
                        if(!e.context)keep[2]&=e.beforeCubics.constData()==old.beforeCubics.constData();}}
                auto bake=[&](int layer,QImage& image,QVector<QPainterPath>& fallback){if(keep[layer]&&!image.isNull())return;
                    image=raster?QImage(shellPixels,QImage::Format_ARGB32_Premultiplied):QImage{};
                    if(layer==1&&!image.isNull()){
                        // The current trace is opaque and solid: stroke all projected edges once
                        // into one coverage buffer rather than constructing a QPainter stroker for each.
                        QImage mask(shellPixels,QImage::Format_Grayscale8);
                        if(!mask.isNull()){
                            mask.fill(0);const bool split=state_->splitBody();const double splitX=split?splitLocation(*snapshot):0.;
                            fs::Vec3 right,up,depth;state_->basis(right,up,depth);const bool perspective=state_->projection==HullProjection::Perspective;
                            const auto pix=[&](fs::Vec3 p){auto r=subtract(p,state_->cameraCenter);const double w=perspective?std::max(1e-6,1-dot(r,depth)/(state_->cameraExtent*state_->cameraDistanceFactor())):1.;
                                return (state_->offset+QPointF(dot(r,right),-dot(r,up))*(state_->scale/w))*dpr+phase;};
                            for(const auto& e:*shell){if(e.context)continue;const auto cubics=split?splitBodyCubics(e.afterCubics,splitX):e.afterCubics;
                                for(bool mirror:{false,true}){if(mirror&&(!e.symmetric||!snapshot->bothSides||state_->projection==HullProjection::Profile))continue;
                                    auto projectKnot=[&](fs::Vec3 p){return pix(mirror?mirrored(p):p);};
                                    auto mid=[](fs::Vec3 a,fs::Vec3 b){return fs::Vec3{(a.x+b.x)*.5,(a.y+b.y)*.5,(a.z+b.z)*.5};};
                                    auto strokeCubic=[&](auto&& self,const fs::presentation::Cubic& c,QPointF a,QPointF b,int depth)->void{
                                        const auto ab=b-a;const double len=QPointF::dotProduct(ab,ab);
                                        auto flat=[&](QPointF p){auto ap=p-a;const double t=len>0?std::clamp(QPointF::dotProduct(ap,ab)/len,0.,1.):0.;const auto diff=ap-t*ab;return QPointF::dotProduct(diff,diff)<=.04;};
                                        if(depth==24||(flat(projectKnot(c[1]))&&flat(projectKnot(c[2])))){shellStroke(mask,a,b,dpr);return;}
                                        auto x=mid(c[0],c[1]),y=mid(c[1],c[2]),z=mid(c[2],c[3]),l=mid(x,y),r=mid(y,z),m=mid(l,r);auto pm=projectKnot(m);
                                        self(self,{c[0],x,l,m},a,pm,depth+1);self(self,{m,r,z,c[3]},pm,b,depth+1);
                                    };
                                    for(const auto& c:cubics)strokeCubic(strokeCubic,c,projectKnot(c[0]),projectKnot(c[3]),0);}}

                            QRgb table[256];for(int a=0;a<256;++a)table[a]=qPremultiply(qRgba(19,115,107,a));
                            for(int y=0;y<image.height();++y){auto* dst=reinterpret_cast<QRgb*>(image.scanLine(y));const auto* src=mask.constScanLine(y);for(int x=0;x<image.width();++x)dst[x]=table[src[x]];}
                            image.setDevicePixelRatio(dpr);return;
                        }
                    }
                    QVector<QPainterPath> paths;paths.reserve(shell->size());
                    for(const auto& e:*shell)if(e.context==(layer==0))paths.append(state_->projectEditCurve(layer==2?e.beforeCubics:e.afterCubics,e.symmetric));
                    if(image.isNull()){fallback+=paths;return;}
                    image.setDevicePixelRatio(dpr);image.fill(Qt::transparent);QPainter ink(&image);ink.setRenderHint(QPainter::Antialiasing);ink.translate(phase/dpr);
                    auto stroke=[&](QPen pen){ink.setPen(pen);for(const auto& p:paths)ink.drawPath(p);};
                    if(layer==0){stroke(QPen(QColor(255,255,255,155),2.4));stroke(QPen(QColor(79,100,118),1.25));}
                    else stroke(layer==1?current:original);
                };
                bake(0,cache.context,contextPaths);bake(1,cache.current,afterPaths);bake(2,cache.original,beforePaths);
                cache.source=(raster&&!cache.context.isNull()&&!cache.current.isNull()&&!cache.original.isNull())?shell:DragShell{};
                cache.camera=state_->cameraRevision;cache.size=shellPixels;cache.dpr=dpr;cache.phase=phase;
            }
        }
        const auto drawComparison=[&]{
            if(!shell&&beforePaths.isEmpty()&&afterPaths.isEmpty()&&contextPaths.isEmpty())return;
            painter.save();painter.setBrush(Qt::NoBrush);
            if(shell&&!cache.context.isNull())painter.drawImage(-phase/dpr,cache.context);
            if(!contextPaths.isEmpty()){
                painter.setPen(QPen(QColor(255,255,255,155),2.4));for(const auto& path:contextPaths)painter.drawPath(path);
                painter.setPen(QPen(QColor(79,100,118),1.25));for(const auto& path:contextPaths)painter.drawPath(path);
            }
            if(shell&&!shell->isEmpty())state_->paintedHull=true;
            if(shell&&!cache.current.isNull())painter.drawImage(-phase/dpr,cache.current);
            painter.setPen(current);
            for(const auto& path:afterPaths)painter.drawPath(path);
            if(shell&&!cache.original.isNull())painter.drawImage(-phase/dpr,cache.original);
            painter.setPen(original);for(const auto& path:beforePaths)painter.drawPath(path);
            painter.restore();
        };
        if(!preview){
            const auto previewPoints=final?fs::curveEdit::PointPositions{}:state_->curvePreviewPositions();
            QHash<fs::EntityId,const fs::AuxiliaryCurve*> previewCurves;
            if(snapshot&&!previewPoints.isEmpty())
                for(const auto& curve:snapshot->controlCurves)
                    if(fs::curveEdit::affects(curve,previewPoints))previewCurves.insert(curve.id,&curve);
            const auto curvePen=[&](fs::EntityId id,bool flow,QColor color){
                const bool selected=state_->kind==(flow?HullEntity::Flowline:HullEntity::Curve)&&state_->entities.contains(id);
                QPen pen(selected?QColor(160,67,34):color,selected?2:1);pen.setCosmetic(true);return pen;
            };
            const auto flowSource=[&](fs::Vec3 source){
                painter.setPen(QColor(65,65,65));painter.setBrush(Qt::white);painter.drawEllipse(project(source),3,3);
                if(snapshot->bothSides&&state_->projection!=HullProjection::Profile)painter.drawEllipse(project(mirrored(source)),3,3);
            };
            state_->prepareCurvePaths(final);
            for(int i=0;i<data->curves.size();++i){const auto& c=data->curves[i];if(!state_->curveVisible(c))continue;
                if(!c.marker&&editMotion&&editMotion->contains({c.flow?-1:0,c.id}))continue;
                if(!c.marker&&!c.flow)if(const auto* curve=previewCurves.value(c.id)){
                    const auto basis=fs::presentation::splineCubics(curve->spline);
                    const auto cubics=fs::curveEdit::preview(*curve,*snapshot,previewPoints,basis);
                    if(comparing&&((state_->gesture==State::Gesture::Move&&state_->moved)||state_->compareSections)){
                        beforePaths.append(state_->projectEditCurve(basis));afterPaths.append(state_->projectEditCurve(cubics));continue;
                    }
                    painter.setPen(curvePen(c.id,false,c.color));painter.setBrush(Qt::NoBrush);
                    painter.drawPath(state_->projectEditCurve(cubics));continue;
                }
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
                if(c.hasSource)flowSource(c.source);
            }
            if(snapshot&&editMotion)for(const auto& [key,tracks]:editMotion->tracks()){
                if(key.kind>0)break;
                const bool flow=key.kind<0;
                if(flow?(!snapshot->showFlowlines||state_->fillVisible):!snapshot->showControlCurves)continue;
                for(const auto& track:tracks){
                    auto shape=fs::curveEdit::Motion::sample(track,editProgress);
                    if(shape.opacity<=0)continue;
                    auto cubics=std::move(shape.cubics);
                    if(!flow)if(const auto* curve=previewCurves.value(key.id))
                        cubics=fs::curveEdit::preview(*curve,*snapshot,previewPoints,cubics);
                    if(cubics.isEmpty())continue;
                    painter.save();painter.setOpacity(shape.opacity);
                    painter.setPen(curvePen(key.id,flow,shape.color));painter.setBrush(Qt::NoBrush);
                    painter.drawPath(state_->projectEditCurve(cubics));
                    if(flow){
                        auto source=cubics.front().front();
                        if(state_->splitBody()&&source.x<fs::presentation::splitLocation(*snapshot))source.y=-source.y;
                        flowSource(source);
                    }
                    painter.restore();
                }
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
        if (!state_->fillVisible && !preview) {
            QVector<QLineF> lines;
            const QRectF clip = QRectF(rect()).adjusted(-1,-1,1,1);
            const auto addCreases = [&](const auto& segments) {
                for (const auto& segment : segments) {
                    const QLineF line(project(segment[0]), project(segment[1]));
                    if (line.length() >= 1. && clip.intersects(
                            QRectF(line.p1(), line.p2()).normalized().adjusted(-1,-1,1,1)))
                        lines.append(line);
                }
            };
            if(!final&&state_->releasedCreases)addCreases(*state_->releasedCreases);
            else {addCreases(data->surfaceCreases);
                if(state_->projection!=HullProjection::Profile)addCreases(data->mirroredSurfaceCreases);}
            if (!lines.isEmpty()) {
                painter.save();if(shellFocus)painter.setOpacity(.27);painter.setPen(QPen(state_->preferred("creasecolor", Qt::black), 1));
                painter.drawLines(lines);painter.restore();
                state_->paintedHull = true;
            }
        }
        // The annotation collision pass uses the same unmirrored screen edges and
        // points as the control net. Collect them once, in original model order.
        QVector<QLineF> labelEdges;
        QVector<QPointF> labelPoints;
        const bool collectObstacles=state_->controlNetVisible&&!preview&&snapshot&&snapshot->overlay.present&&
            (final||state_->entrance.textOpacity(false)>0);
        if(!state_->controlNetVisible)drawComparison();
        if(collectObstacles)labelEdges.reserve(data->edges.size());
        if(state_->controlNetVisible) {
        QVector<QLineF> edges,creases,mirrors;
        labelPoints.reserve(data->points.size());
        for(const auto& point:data->points)labelPoints.append(project(state_->controlPosition(state_->displayedPosition(point,final))));
        edges.reserve(data->edges.size());creases.reserve(data->edges.size()/3);
        const QRectF clip=QRectF(rect()).adjusted(-8,-8,8,8);
        for (const auto& edge:data->edges) {
            // The corresponding subdivided edge is already visible in both poses.
            if(shell&&cache.edges.contains({std::min(edge.a,edge.b),std::max(edge.a,edge.b)}))continue;
            auto a=state_->displayedPosition(data->points[edge.ia],final);
            auto b=state_->displayedPosition(data->points[edge.ib],final);
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
        painter.save();if(shellFocus)painter.setOpacity(.3);
        painter.setPen(QPen(QColor(91,112,127,145),0.8,Qt::DashLine));
        painter.drawLines(mirrors);
        const auto hasVisibleLine=[](const QVector<QLineF>& lines){return std::any_of(lines.begin(),lines.end(),[](const auto& line){return line.length()>=1.;});};
        state_->paintedHull|=hasVisibleLine(edges)||hasVisibleLine(creases)||hasVisibleLine(mirrors);
        if(!state_->studio){painter.setPen(QPen(QColor(255,255,255,155),2.5));painter.drawLines(edges);painter.drawLines(creases);}
        painter.setPen(QPen(state_->studio?state_->preferred("edgecolor",QColor(102,133,125)):QColor(18,74,123),state_->studio?1.3:0.95));painter.drawLines(edges);
        painter.setPen(QPen(state_->studio?state_->preferred("creaseedgecolor",QColor(160,67,34)):QColor(13,39,64),1.3));painter.drawLines(creases);
        painter.restore();drawComparison();
        QVector<QPointF> points;
        QVector<QRectF> locked;
        points.reserve(data->points.size());
        const auto& selection=state_->gesture==State::Gesture::Box?state_->boxPreview:state_->selection;
        // The opaque selection halo covers small base markers completely. Keep
        // larger markers (and all export paths) and draw selection in its usual order.
        const bool smallMarkers=!state_->studio||!snapshot->overlay.present||snapshot->overlay.pointSize<=3;
        painter.setPen(Qt::NoPen);
        for (const auto& point:data->points) {
            if(!final&&smallMarkers&&!point.locked&&selection.contains(point.id))continue;
            if(!final&&state_->undoingPlacements.contains(point.id))continue;
            const auto pixel=labelPoints[&point-data->points.constData()];
            if (!clip.contains(pixel)) continue;
            if(!final&&!state_->entrance.pointVisible(pixel.x()))continue;
            if(state_->studio&&snapshot->overlay.present){
                QColor base=point.locked?QColor(128,128,128):point.leak?state_->preferred("leakpointcolor",QColor(0,255,153)):point.vertexType==1?state_->preferred("creasepointcolor",QColor(98,77,50)):point.vertexType==2?state_->preferred("dartpointcolor",QColor(Qt::magenta)):point.vertexType==3?state_->preferred("cornerpointcolor",QColor(82,65,58)):state_->preferred("regularpointcolor",QColor(49,93,85));
                if(!final)base=state_->entrance.pointColor(point.id,base);
                const int radius=std::clamp(snapshot->overlay.pointSize,1,100);
                const QRectF marker(pixel-QPointF(radius,radius),QSizeF(radius*2+1,radius*2+1));
                if(point.locked)painter.fillRect(marker,base);
                else{painter.setBrush(base);painter.drawEllipse(marker);}
                continue;
            }
            if (point.locked) locked.append(QRectF(pixel-QPointF(2.5,2.5),QSizeF(5,5)));
            else points.append(pixel);
        }
        painter.setPen(QPen(QColor(245,249,252),5.5,Qt::SolidLine,Qt::RoundCap));painter.drawPoints(points);
        painter.setPen(QPen(state_->studio?QColor(49,93,85):QColor(12,75,134),3.2,Qt::SolidLine,Qt::RoundCap));painter.drawPoints(points);
        painter.setPen(QPen(QColor(69,77,84),1));painter.setBrush(QColor(236,239,242));painter.drawRects(locked);
        QVector<QPointF> selected;
        QVector<QRectF> selectedLocked;
        selected.reserve(selection.size());
        for (auto id:selection) {
            if(!final&&state_->undoingPlacements.contains(id))continue;
            const auto found=data->pointIndices.constFind(id);
            if (found==data->pointIndices.cend()) continue;
            const auto& point=data->points[found.value()];
            const auto pixel=labelPoints[found.value()];
            if (!clip.contains(pixel)) continue;
            if(!final&&!state_->entrance.pointVisible(pixel.x()))continue;
            if (point.locked) selectedLocked.append(QRectF(pixel-QPointF(4,4),QSizeF(8,8)));
            else selected.append(pixel);
        }
        const QColor selectedColor=state_->preferred("selectcolor",QColor(224,101,25));
        if(!final&&selected.size()>=64){
            // Rasterise each opaque marker once per display scale. Keep the two
            // passes separate so overlapping points have the usual shared halo.
            auto dots=[&](auto& cache,QColor color,int diameter){
                const qreal ratio=painter.device()->devicePixelRatioF();
                if(cache.dpr!=ratio||cache.color!=color){
                    int side=qCeil((diameter+2)*ratio);cache.image=QPixmap(side,side);cache.image.setDevicePixelRatio(ratio);cache.image.fill(Qt::transparent);
                    QPainter stamp(&cache.image);stamp.setRenderHint(QPainter::Antialiasing);stamp.setPen(Qt::NoPen);stamp.setBrush(color);
                    const qreal half=side/(2*ratio);stamp.drawEllipse(QPointF(half,half),diameter*.5,diameter*.5);cache.dpr=ratio;cache.color=color;
                }
                const auto half=cache.image.deviceIndependentSize()*.5;
                for(auto p:selected)painter.drawPixmap(p-QPointF(half.width(),half.height()),cache.image);
            };dots(state_->haloStamp,Qt::white,11);dots(state_->selectedStamp,selectedColor,8);
        }else{
            painter.setPen(QPen(QColor(255,255,255),11,Qt::SolidLine,Qt::RoundCap));painter.drawPoints(selected);
            painter.setPen(QPen(selectedColor,8,Qt::SolidLine,Qt::RoundCap));painter.drawPoints(selected);
        }
        painter.setPen(QPen(QColor(255,255,255),2));painter.setBrush(state_->preferred("selectcolor",QColor(224,101,25)));
        painter.drawRects(selectedLocked);
        }
        if(!preview&&snapshot&&snapshot->overlay.present){
            LabelLayout exportLayout;auto& layout=final?exportLayout:state_->labels;
            layout.begin(QRectF(rect()).adjusted(3,3,-3,-3),state_->frameTime,!final&&state_->animations);
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
                painter.save();painter.setOpacity((shellFocus?.22:1.)*(final?1:state_->entrance.textOpacity(numeric)));painter.setPen(a.color);
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
                        const auto p=state_->displayedPosition(data->points[*at],final);center.x+=p.x;center.y+=p.y;center.z+=p.z;++count;selected|=state_->selection.contains(id);}
                    if(!count)continue;center.x/=count;center.y/=count;center.z/=count;
                    const auto at=project(state_->controlPosition(center));const auto color=selected?QColor(160,67,34):QColor(49,93,85);
                    painter.setPen(color);painter.setBrush(Qt::NoBrush);painter.drawEllipse(at,2,2);painter.drawEllipse(at,4,4);
                    annotation({"group:"+QString::number(group.index),group.name,at,at+QPointF(6,3),color,LabelKind::Point,false});
                }
                // Selected points claim coincident labels first, without copying
                // and sorting the control net on every animation frame.
                if(showText)for(bool selected:{true,false})for(const auto& p:data->points){
                    if(!final&&state_->undoingPlacements.contains(p.id))continue;
                    if((p.name.isEmpty()&&!state_->coordinateAxis)||state_->selection.contains(p.id)!=selected)continue;
                    auto at=labelPoints[&p-data->points.constData()];if(!rect().contains(at.toPoint())||(!final&&!state_->entrance.pointVisible(at.x())))continue;
                    QString text=p.name;if(state_->coordinateAxis){const auto v=state_->displayedPosition(p,final);text=QString::number(state_->coordinateAxis==1?v.x:state_->coordinateAxis==2?v.y:v.z,'f',3);if(!layout.claim(at,text))continue;}
                    if(text.isEmpty())continue;
                    annotation({"point:"+QString::number(p.id),text,at,at+QPointF(snapshot->overlay.pointSize+4,3),selected?QColor(160,67,34):QColor(49,93,85),LabelKind::Point,false});
                }
            }
            for(const auto& a:overlay.labels)if(a.kind!=LabelKind::Named)annotation(a);
            state_->evidence={{"labels",evidence},{"grid_lines",overlay.grid.size()},{"wave_lines",overlay.wave.size()},{"graph_builds",state_->graph.buildCount()},{"axis",state_->coordinateAxis},{"animation_active",state_->entrance.active()}};
        }
        } // normal hull rendering
        const bool stroking=!final&&state_->gesture==State::Gesture::Stroke&&state_->moved;
        const auto& selectedInk=stroking?state_->boxPreview:state_->entities;
        if(!selectedInk.isEmpty()&&usesControlSelectionInk(state_->kind)){
            painter.save();painter.setRenderHint(QPainter::Antialiasing,true);
            QPainterPath outline;
            const bool faceMode=state_->kind==HullEntity::Face;
            if(!faceMode){
                for(const auto& edge:data->edges)if(selectedInk.contains(edge.id))
                    for(const auto& span:state_->selectableEdgeSpans(edge)){
                        outline.moveTo(project(span[0]));outline.lineTo(project(span[1]));
                    }
            }else{
                painter.setPen(Qt::NoPen);painter.setBrush(controlFaceSelectionFill);
                for(const auto& face:data->faces)if(selectedInk.contains(face.id)){
                    for(const auto& p:state_->faceParts(face)){
                        QPolygonF polygon;for(auto point:p)polygon.append(project(point));
                        painter.drawPolygon(polygon);outline.addPolygon(polygon);outline.closeSubpath();
                    }
                }
            }
            const auto width=faceMode?controlFaceSelectionWidth:controlEdgeSelectionWidth;
            painter.strokePath(outline,controlSelectionPen(controlSelectionKeyline,width+1.8));
            painter.strokePath(outline,controlSelectionPen(controlSelectionInk,width));
            painter.restore();
        }
        if(!final&&!state_->plateDiagnosticFaces.isEmpty()&&snapshot->documentId==state_->plateDiagnosticDocument&&
           snapshot->revision==state_->plateDiagnosticRevision){
            painter.save();
            painter.setPen(QPen(QColor(189,100,11),2));
            painter.setBrush(QColor(246,155,41,75));
            for(const auto& face:state_->plateDiagnosticFaces){
                if(!snapshot->bothSides&&std::all_of(face.begin(),face.end(),[](auto p){return p.y<0;}))continue;
                for(const auto& part:state_->controlPolygons(face)){
                    QPolygonF polygon;
                    for(auto point:part)polygon.append(project(point));
                    painter.drawPolygon(polygon);
                }
            }
            painter.restore();
        }
        if(stroking){painter.save();painter.setBrush(Qt::NoBrush);painter.setRenderHint(QPainter::Antialiasing);
            painter.setPen(QPen(Qt::white,3));painter.drawPolyline(state_->stroke);
            painter.setPen(QPen(controlSelectionInk,1.6,Qt::DashLine));painter.drawPolyline(state_->stroke);painter.restore();}
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
            if(hover.kind==state_->kind&&usesControlSelectionInk(hover.kind)&&state_->entities.contains(hover.id))continue;
            painter.setOpacity(hover.alpha);painter.setPen(QPen(QColor("#008f80"),hover.kind==HullEntity::Point?2.2:3,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));
            if(hover.kind==HullEntity::Face)painter.setBrush(QColor(0,143,128,30));else painter.setBrush(Qt::NoBrush);painter.drawPath(state_->interactionShape(hover.kind,hover.id));
        }
        const double pulse=std::clamp(1-double(interactionTime()-state_->emphasizedAt)/240.,0.,1.);
        if(pulse>0){
            const auto kind=state_->emphasizedKind;
            const auto selectedWidth=kind==HullEntity::Face?controlFaceSelectionWidth:controlEdgeSelectionWidth;
            const auto pen=usesControlSelectionInk(kind)?controlSelectionPen(controlSelectionInk,selectedWidth+1.4*pulse):
                QPen(QColor("#e49335"),2+2*pulse,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin);
            painter.setOpacity(pulse);painter.setPen(pen);painter.setBrush(Qt::NoBrush);
            for(auto id:state_->emphasized)painter.drawPath(state_->interactionShape(kind,id));
        }
        painter.restore();
    }
    painter.setRenderHint(QPainter::Antialiasing,false);
    if(!final&&backgroundData&&backgroundData->visible&&state_->backgroundMode==BackgroundMode::Frame){
        painter.setPen(QPen(Qt::yellow,3,Qt::DashLine));painter.setBrush(Qt::NoBrush);painter.drawRect(backgroundData->targetRect(project({}),state_->scale));
    }
    if(!state_->studio){
        auto titleFont=QApplication::font();titleFont.setPointSizeF(std::max(11.,titleFont.pointSizeF()));
        titleFont.setWeight(QFont::Normal);
        titleFont.setStyleStrategy(QFont::StyleStrategy(QFont::PreferAntialias|QFont::PreferQuality));
        const auto title=projectionLabel();
        const QRect titleRect(12,0,width()-24,std::max(32,QFontMetrics(titleFont).height()+12));
        painter.save();painter.setRenderHint(QPainter::TextAntialiasing,true);painter.setFont(titleFont);
        painter.setPen(QColor(44,57,68));
        painter.drawText(titleRect,Qt::AlignLeft|Qt::AlignVCenter,title);painter.restore();
        state_->evidence.insert("projection_title_text",title);
        state_->evidence.insert("projection_title_text_width",QFontMetricsF(titleFont).horizontalAdvance(title));
        state_->evidence.insert("projection_title_available_width",titleRect.width());
        state_->evidence.insert("projection_title_rect",QJsonArray{titleRect.x(),titleRect.y(),titleRect.width(),titleRect.height()});
    }
    painter.setPen(QColor(215,222,228));painter.setBrush(Qt::NoBrush);
    painter.drawRect(rect().adjusted(0,0,-1,-1));
}

void HullView::paintEvent(QPaintEvent*) {
    QElapsedTimer timer;timer.start();state_->frameTime=state_->clock.elapsed();state_->entrance.beginFrame(state_->frameTime,width());
    state_->paintedHull=false;
    QPainter painter(this);draw(painter,devicePixelRatioF(),false);drawTransform(painter);
    for(const auto &pending:state_->pendingPlacements){const auto at=project(state_->controlPosition(pending));painter.save();painter.setRenderHint(QPainter::Antialiasing);painter.setPen(QPen(Qt::white,2));painter.setBrush(QColor(27,126,106));painter.drawEllipse(at,4.5,4.5);painter.restore();}
    state_->entrance.endFrame();
    if(!state_->entrance.active())state_->entranceSections.pixels={};
    if(state_->curveEditMotion&&!state_->curveEditMotion->active())setCurveEditMotion({});
    if((state_->entrance.active()||state_->labels.animating())&&!state_->entrance.frozen()){
        if(!state_->timer.isActive())state_->timer.start();
    }else state_->timer.stop();
    lastPaintMs=timer.nsecsElapsed()/1e6;++paintCount;
    if(state_->snapshot&&(!state_->entrance.preview()||state_->entrance.lineFraction()>0)&&onHullFrame)onHullFrame();
    if(state_->paintedHull&&onHullVisibleFrame)onHullVisibleFrame();
}
void HullView::resizeEvent(QResizeEvent* event) {
    positionDisplayModeButton();
    if (state_->fitted) fitToViewport(true);
    else if(event->oldSize().isValid()){
        state_->labels.viewportResized();
        double ratio=1.;
        if(state_->display){
            const auto span=state_->fitSpan(state_->projectedMeshBounds());
            ratio=state_->fitScale(event->size(),span)/state_->fitScale(event->oldSize(),span);
        }
        const QPointF oldCenter(event->oldSize().width()/2.,event->oldSize().height()/2.);
        const QPointF newCenter(event->size().width()/2.,event->size().height()/2.);
        const auto resizePose=[&](State::CameraPose& pose){
            pose.scale*=ratio;
            pose.offset=newCenter+(pose.offset-oldCenter)*ratio;
        };
        auto pose=state_->cameraPose();
        resizePose(pose);
        state_->orbitFitScale*=ratio;
        if(auto& motion=state_->cameraMotion;motion.active){
            resizePose(motion.from);resizePose(motion.to);
            motion.fromVelocity.offset*=ratio;motion.velocity.offset*=ratio;
        }
        state_->applyCamera(pose);
    }
    if(isVisible()){
        const auto dpr=devicePixelRatioF();
        state_->scheduleSections(size(),dpr,pixelPhase(QPointF(mapTo(window(),QPoint()))*dpr));
    }
    QWidget::resizeEvent(event);
}
#include "point_transform.inc"
void HullView::cancelGesture(bool committingPointMove) {
    const bool restorePointPreview=!committingPointMove&&state_->gesture==State::Gesture::Move&&state_->moved;
    repeatPoint_=0;
    if(!committingPointMove && (state_->precise||restorePointPreview) && onTransformPreview)onTransformPreview({});
    state_->precise=state_->soft=state_->fromHandle=false;state_->axis=state_->hoverAxis=0;state_->digits.clear();state_->weights.clear();
    state_->gesture=State::Gesture::None;state_->moved=false;
    state_->backgroundPreviewing=false;
    state_->backgroundStart={};state_->backgroundPreview={};
    state_->dragStarts.clear();state_->dragDelta={};state_->blockedReason.clear();
    state_->boxBase.clear();state_->boxPreview.clear();state_->stroke.clear();state_->strokeFaces.clear();state_->strokeEdges.clear();
    if(restorePointPreview&&onPointMovePreviewChanged)onPointMovePreviewChanged();
    unsetCursor();update();
}
void HullView::mousePressEvent(QMouseEvent* event) {
    if (event->button()!=Qt::LeftButton && event->button()!=Qt::RightButton) {
        QWidget::mousePressEvent(event);return;
    }
    if(state_->precise){
        if(event->button()==Qt::RightButton){cancelGesture();state_->lastRightDragged=true;}
        else {auto &s=*state_;auto pos=event->position();const int hit=s.transformHandleAt(pos,s.movedPosition(s.pivot));
            if(hit&&!s.moved){s.axis=hit;s.fromHandle=true;s.press=s.last=pos;s.digits.clear();update();}else if(s.axis)finishTransform();}
        event->accept();return;
    }
    if(event->button()==Qt::LeftButton && !event->modifiers() && canTransform()){
        fs::Vec3 center;const int axis=state_->transformCenter(center)?state_->transformHandleAt(event->position(),center):0;
        if(axis){updateInteractionHover(0);setFocus(Qt::MouseFocusReason);beginTransform(axis,event->position(),true);event->accept();return;}}
    setFocus(Qt::MouseFocusReason);
    state_->sampleCamera(interactionTime());state_->cameraMotion.active=false;updateInteractionHover(0);
    finishPresentation();
    cancelGesture();
    state_->press=state_->last=event->position();
    if(state_->pointPlacement){
        if(event->button()==Qt::RightButton){setPointPlacement(false);state_->lastRightDragged=true;state_->moved=true;if(onPointPlacementEnded)onPointPlacementEnded();event->accept();return;}
        if(!state_->editingEnabled){event->accept();return;}
        const auto p=flowSource(event->position());fs::Vec3 position{};
        switch(state_->projection){case HullProjection::Profile:position={p.x(),0,p.y()};break;case HullProjection::Plan:position={p.x(),p.y(),0};break;case HullProjection::Body:{
            // Invert the same half-body projection used to draw this station.
            const double x=state_->stationEditor?state_->stationEditor->station():0.;
            const auto origin=project({});auto target=event->position();
            if(event->modifiers().testFlag(Qt::ShiftModifier)){
                // Pixel distance is independent of zoom. Snap only this click;
                // never add constraints, or snap to a reference on another station.
                const auto cursor=target;double best=8.*8.;
                auto offer=[&](QPointF q){const auto d=q-cursor;const double d2=QPointF::dotProduct(d,d);if(d2<=best){best=d2;target=q;}};
                offer({origin.x(),cursor.y()});
                const double tol=8*std::numeric_limits<float>::epsilon()*std::max(1.,std::abs(x));
                if(state_->display&&state_->snapshot->showMarkers)for(const auto& c:state_->display->curves){
                    if(!c.marker||!state_->curveVisible(c)||c.cubics.isEmpty())continue;
                    bool onPlane=true;for(const auto& cubic:c.cubics)for(const auto& v:cubic)if(std::abs(v.x-x)>tol)onPlane=false;
                    if(!onPlane)continue;
                    for(bool mirror:{false,true}){if(mirror&&!state_->snapshot->bothSides)continue;
                        const auto path=fs::presentation::projectCubics(c.cubics,[&](fs::Vec3 v){return project(mirror?mirrored(v):v);},false);
                        for(const auto& line:path.toSubpathPolygons())for(qsizetype j=1;j<line.size();++j){
                            const auto a=line[j-1],ab=line[j]-a;const double len2=QPointF::dotProduct(ab,ab);
                            offer(a+ab*(len2>0?std::clamp(QPointF::dotProduct(cursor-a,ab)/len2,0.,1.):0.));
                        }
                    }
                }
            }
            position=state_->controlPosition({x,(target.x()-origin.x())/state_->scale,(origin.y()-target.y())/state_->scale});
        }break;default:event->accept();return;}
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
    if (event->button()==Qt::RightButton) {
        state_->lastRightDragged=false;
        state_->contextMenuOnRelease=false;
        state_->gesture=State::Gesture::Pan;setCursor(Qt::ClosedHandCursor);
    } else {
        const bool control=event->modifiers().testFlag(Qt::ControlModifier);
        if(control&&state_->projection==HullProjection::Perspective)
            state_->gesture=State::Gesture::PendingRotate;
        else if(usesControlSelectionInk(state_->kind)){
            state_->gesture=State::Gesture::Stroke;state_->strokeAdditive=control;
            state_->stroke.append(event->position());if(control)state_->boxPreview=state_->entities;
        }else selectWithLeftButton(event->position(),control);
    }
    event->accept();
}
void HullView::selectWithLeftButton(QPointF position,bool additive) {
    if(state_->kind!=HullEntity::Point) {
        const auto id=entityAt(position);
        const bool repeat=!additive&&id&&usesControlSelectionInk(state_->kind)&&state_->entities.size()==1&&state_->entities.contains(id);
        auto ids=additive?state_->entities:QSet<fs::EntityId>{};
        if(id){if(ids.contains(id))ids.remove(id);else ids.insert(id);}
        setSelectedEntities(ids);
        if(onEntitySelectionChanged)onEntitySelectionChanged(state_->kind,state_->entities);
        if(repeat&&onRepeatEntityClick)onRepeatEntityClick(state_->kind,id,position.toPoint());
        return;
    }
    auto id=state_->hit(position);
    QVector<fs::EntityId> groupPick;fs::Vec3 groupCenter{};
    if(!id&&state_->snapshot&&state_->display&&state_->controlNetVisible&&!state_->fillVisible){
        for(const auto& g:state_->snapshot->pointGroups){
            if(g.points.isEmpty()||g.name.isEmpty())continue;fs::Vec3 center{};bool visible=true;
            for(auto p:g.points){auto at=state_->display->pointIndices.constFind(p);if(at==state_->display->pointIndices.cend()){visible=false;break;}
                auto pos=state_->displayedPosition(state_->display->points[*at]);center.x+=pos.x;center.y+=pos.y;center.z+=pos.z;}
            if(!visible)continue;center.x/=g.points.size();center.y/=g.points.size();center.z/=g.points.size();
            if(QLineF(project(state_->controlPosition(center)),position).length()<=7){groupPick=g.points;groupCenter=center;id=g.points.first();break;}
        }
    }
    repeatPoint_=!additive && groupPick.isEmpty() && state_->selection.size()==1 && state_->selection.contains(id)?id:0;
    if (!id) {
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
        if (!additive && state_->editingEnabled && state_->pendingPointPositions.isEmpty() && state_->selection.contains(id) &&
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
                state_->pivot=!groupPick.isEmpty()?groupCenter:anchor?anchor->position:state_->dragStarts.first().position;
                state_->gesture=State::Gesture::Move;setCursor(Qt::CrossCursor);
            } else {
                state_->dragStarts.clear();state_->gesture=State::Gesture::BlockedMove;
            }
        }
        update();
    }
}
void HullView::mouseMoveEvent(QMouseEvent* event) {
    if(repeatPoint_ && (event->position()-state_->press).manhattanLength()>=3)repeatPoint_=0;
    if(state_->precise){if(state_->axis)previewTransform(event->position());else {state_->last=event->position();int axis=state_->transformHandleAt(state_->last,state_->pivot);
        if(axis!=state_->hoverAxis){state_->hoverAxis=axis;update();}}event->accept();return;}
    const auto delta=event->position()-state_->last;
    switch(state_->gesture) {
    case State::Gesture::Stroke:{
        if(!state_->moved){if((event->position()-state_->press).manhattanLength()<3)break;
            state_->moved=true;setCursor(Qt::CrossCursor);state_->prepareStroke();}
        state_->extendStroke(event->position());update();break;
    }
    case State::Gesture::Background:{
        const auto total=event->position()-state_->press;fs::BackgroundCalibration next;
        if(total.manhattanLength()>=3&&fs::resizeBackground(state_->backgroundStart,project({}),state_->scale,state_->backgroundSide,total,next)){
            state_->backgroundPreview.origin=next.origin;state_->backgroundPreview.scale=next.scale;state_->moved=true;update();
        }break;
    }
    case State::Gesture::Pan:state_->moved|=(event->position()-state_->press).manhattanLength()>=3;panBy(delta);break;
    case State::Gesture::PendingRotate:
    case State::Gesture::Rotate: {
        auto rotationDelta=delta;
        if(state_->gesture==State::Gesture::PendingRotate){
            // Distinguish Ctrl-click selection from a drag without selecting
            // the object underneath the start of a camera gesture.
            rotationDelta=event->position()-state_->press;
            if(rotationDelta.manhattanLength()<3)break;
            state_->gesture=State::Gesture::Rotate;setCursor(Qt::SizeAllCursor);
        }
        // Dampen both axes in proportion to magnification above the fitted view.
        // Keep the baseline through a drag; orbiting alone must not alter sensitivity.
        const double radiansPerPixel=.008*std::min(1.,state_->orbitFitScale/state_->scale);
        state_->yaw-=rotationDelta.x()*radiansPerPixel;
        state_->pitch=qBound(-Pi*0.48,state_->pitch+rotationDelta.y()*radiansPerPixel,Pi*0.48);
        state_->fitted=false;++state_->cameraRevision;update();break;
    }
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
        if(onTransformPreview)onTransformPreview(transformMoves());
        if(onPointMovePreviewChanged)onPointMovePreviewChanged();
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
        if(state_->backgroundMode!=BackgroundMode::None||state_->flowPlacement)setCursor(Qt::CrossCursor);
        else {
            fs::Vec3 center;const int axis=!event->modifiers() && canTransform() && state_->transformCenter(center)
                ?state_->transformHandleAt(event->position(),center):0;
            if(axis!=state_->hoverAxis){state_->hoverAxis=axis;update();}
            if(axis){updateInteractionHover(0);setCursor(Qt::PointingHandCursor);break;}
            if(state_->pointPlacement){updateInteractionHover(0);setCursor(Qt::CrossCursor);break;}
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
    if(state_->precise){if(state_->fromHandle && event->button()==Qt::LeftButton){previewTransform(event->position());finishTransform();}event->accept();return;}
    // Qt can compress move messages; use the actual release position as well.
    if (state_->gesture!=State::Gesture::None && event->position()!=state_->last) mouseMoveEvent(event);
    if(event->button()==Qt::LeftButton&&state_->gesture==State::Gesture::PendingRotate)
        selectWithLeftButton(state_->press,true);
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
    if(state_->gesture==State::Gesture::Stroke&&event->button()==Qt::LeftButton){
        const bool drag=state_->moved,add=state_->strokeAdditive;const auto start=state_->press;auto ids=state_->boxPreview;cancelGesture();
        if(drag){setSelectedEntities(std::move(ids));if(onEntitySelectionChanged)onEntitySelectionChanged(state_->kind,state_->entities);}
        else selectWithLeftButton(start,add);event->accept();return;
    }
    const bool select=state_->gesture==State::Gesture::Box;
    const auto acceptedBackgroundPreview=state_->backgroundPreview;
    const bool openClickMenu=event->button()==Qt::RightButton && state_->contextMenuOnRelease &&
                             state_->gesture==State::Gesture::Pan && !state_->moved;
    if(event->button()==Qt::RightButton){
        state_->lastRightDragged=state_->moved;
        state_->contextMenuOnRelease=false;
    }
    const auto repeat=event->button()==Qt::LeftButton&&!event->modifiers()&&!state_->moved && (event->position()-state_->press).manhattanLength()<3?repeatPoint_:0;
    if (select) {state_->selection=state_->boxPreview;state_->pruneSelection();}
    cancelGesture(!moves.isEmpty());
    if(repeat && onRepeatPointClick)onRepeatPointClick(repeat);
    if(openClickMenu){
        QContextMenuEvent menu(QContextMenuEvent::Mouse,event->position().toPoint(),
                               event->globalPosition().toPoint(),event->modifiers());
        contextMenuEvent(&menu);
    }
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
    if(event->button()==Qt::LeftButton&&state_->projection==HullProjection::Perspective&&
        event->modifiers().testFlag(Qt::ControlModifier)){mousePressEvent(event);return;}
    if (event->button()==Qt::LeftButton && !state_->hit(event->position())) {cancelGesture();fit();event->accept();}
    else QWidget::mouseDoubleClickEvent(event);
}
void HullView::wheelEvent(QWheelEvent* event) {
    const double delta=!event->pixelDelta().isNull()?event->pixelDelta().y()*2.0:event->angleDelta().y();
    if(state_->precise){if(state_->soft&&delta!=0){const double extent=std::max(state_->display->mesh.extent,1e-6);
            state_->radius=std::clamp(state_->radius*std::pow(1.25,delta/120.),extent*.00001,extent*10.);rebuildInfluence();previewTransform(state_->last);}event->accept();return;}
    if (delta!=0) {cancelGesture();zoomAt(event->position(),std::pow(1.0015,delta));}
    event->accept();
}
void HullView::keyPressEvent(QKeyEvent* event) {
    const auto key=event->key();
    if(state_->precise){auto &s=*state_;
        if(key==Qt::Key_Escape){cancelGesture();}
        else if(key==Qt::Key_Return||key==Qt::Key_Enter){if(s.axis)finishTransform();}
        else if(key==Qt::Key_X||key==Qt::Key_Y||key==Qt::Key_Z){s.axis=key-Qt::Key_X+1;s.press=s.last;s.digits.clear();previewTransform(s.last);}
        else if(key==Qt::Key_O){s.soft=!s.soft;rebuildInfluence();previewTransform(s.last);}
        else if(s.axis){if(key==Qt::Key_Backspace)s.digits.chop(1);
            else if(key>=Qt::Key_0&&key<=Qt::Key_9&&s.digits.size()<16)s.digits+=QChar('0'+key-Qt::Key_0);
            else if((key==Qt::Key_Period||key==Qt::Key_Comma)&&!s.digits.contains('.'))s.digits+='.';
            else if(!s.digits.isEmpty()&&s.digits.size()<18 && QStringLiteral("MCFTIN").contains(QChar(key)))s.digits+=QChar(key).toLower();
            else if(key==Qt::Key_Minus){if(s.digits.startsWith('-'))s.digits.remove(0,1);else s.digits.prepend('-');}
            previewTransform(s.last);}
        event->accept();return;
    }
    if((key==Qt::Key_X||key==Qt::Key_Y||key==Qt::Key_Z) && event->modifiers()==Qt::NoModifier &&
       state_->gesture==State::Gesture::None && canTransform()){
        beginTransform(key-Qt::Key_X+1,mapFromGlobal(QCursor::pos()),false);event->accept();return;
    }
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
void HullView::leaveEvent(QEvent* event){if(state_->hoverAxis){state_->hoverAxis=0;unsetCursor();update();}updateInteractionHover(0);QWidget::leaveEvent(event);}
